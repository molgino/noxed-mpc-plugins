// MPC Overwitch - NOXED: Elektron Overbridge devices (Digitone II, Digitakt II, ...) as an MPC plugin.
//
// Signal flow
//   Elektron device --USB Overbridge (48 kHz, 24 blocks x 7 frames per transfer)--> engine thread
//   (ob_usb.h: usbfs, decodes the selected stereo source) --lock-free ring--> audio thread
//   (ob_stream.h: adaptive resampler 48 kHz -> host rate that follows the clock drift) --> LEVEL --> out
//
// Threads: the engine thread owns the USB device; a monitor thread builds the status text and asks
// the MPC to redraw when something visible changes; process() only reads the ring. Only one plugin
// instance can own the device; further instances show "Busy" and stay silent.
//
// Parameters (0.x: may still change before 1.0)
//   0 Level    output gain (param 0, nudged by the host on load: harmless)
//   1 Source   which device output to play: Main, Track 1..16, Delay, Reverb, Chorus, Input (Digitone II
//              names; other devices map the same indices onto their own outputs)
//   2 Report   scrolls the diagnostic report one line at a time
//   3..7       read-only status rows: Status, Device, Stream, Dropouts, System
//
// Protocol from Overwitch (https://github.com/dagargo/overwitch): engine.c MIT (Stefan Rehm, David
// Garcia Goni), device table GPL-3. This plugin: GPL-3.0-or-later.

#include "mpc_plugin.h"
#include "mpc_resources.h"

#include "ob_usb.h"
#include "ob_diag.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sys/utsname.h>

namespace {

enum ParamIndex { kLevel = 0, kSource, kLine, kStatus, kDevice, kStream, kDropouts, kSystem, kNumParams };
constexpr int kMaxLines = 64;
const char* const kVersion = "0.2.1";

std::atomic<bool> g_deviceOwned{false};   // one instance per process owns the USB device

std::vector<std::string> sourceNames()
{
    std::vector<std::string> v = {"Main"};
    for (int i = 1; i <= 16; ++i) v.push_back("Track " + std::to_string(i));
    for (const char* s : {"Delay", "Reverb", "Chorus", "Input"}) v.push_back(s);
    return v;
}

} // namespace

class MPCOverwitch : public mpc::Plugin
{
public:
    MPCOverwitch() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::floatParam("level", "Level", 0.0f, 1.0f, 0.8f));
        addParam(mpc::choiceParam("source", "Source", sourceNames(), 0));
        addParam(mpc::intParam("line", "Report", 1, kMaxLines, 1));
        const char* ro[] = {"status", "device", "stream", "dropouts", "system"};
        const char* names[] = {"Status", "Device", "Stream", "Dropouts", "System"};
        for (int i = 0; i < 5; ++i)
        {
            mpc::Param p = mpc::floatParam(ro[i], names[i], 0.0f, 1.0f, 0.0f);
            p.automatable = false;
            addParam(p);
        }

        utsname u{};
        uname(&u);
        system_ = diag::clip(diag::fmt("%s uid%u %s", u.machine, (unsigned)geteuid(), u.release));

        owner_ = !g_deviceOwned.exchange(true);
        if (owner_)
        {
            engine_.reset(new ob::Engine());
            engine_->onStateChange = [this] { dirty_.store(true); };
            engine_->start();
        }
        rebuild(true);
        monitor_ = std::thread([this] { monitor(); });
    }

    ~MPCOverwitch() override
    {
        stop_.store(true);
        if (monitor_.joinable()) monitor_.join();
        if (engine_) engine_->stop();
        if (owner_) g_deviceOwned.store(false);
    }

    void prepare(double sampleRate, int) override { reader_.prepare(sampleRate); gain_ = 0.0f; }
    void reset() override { reader_.reset(); }

    void process(mpc::ProcessContext& ctx) override
    {
        float* l = ctx.outputs[0];
        float* r = ctx.outputs[1];
        const int n = ctx.numFrames;
        if (!engine_ || engine_->state() != ob::Engine::kStreaming)
        {
            reader_.reset();
            for (int i = 0; i < n; ++i) l[i] = r[i] = 0.0f;
            return;
        }
        engine_->source.store((int)std::lround(get(kSource)), std::memory_order_relaxed);
        reader_.read(engine_->ring, l, r, n);
        // Level: perceptual (squared) curve, smoothed per sample to avoid zipper noise.
        const float lv = get(kLevel);
        const float target = lv * lv * 1.25f;
        float g = gain_;
        const float k = 0.002f;
        for (int i = 0; i < n; ++i)
        {
            g += (target - g) * k;
            l[i] *= g;
            r[i] *= g;
        }
        gain_ = g;
    }

    bool formatValue(int index, float plain, std::string& out) override
    {
        if (index == kLevel)
        {
            float g = plain * plain * 1.25f;
            out = g < 1e-5f ? "-inf dB" : diag::fmt("%.1f dB", 20.0f * std::log10(g));
            return true;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        switch (index)
        {
            case kSource:
            {
                int i = (int)std::lround(plain);
                if (!devSources_.empty())
                    out = i < (int)devSources_.size() ? devSources_[(size_t)i].name : "(not on this device)";
                else
                {
                    std::vector<std::string> n = sourceNames();
                    out = i >= 0 && i < (int)n.size() ? n[(size_t)i] : "?";
                }
                return true;
            }
            case kLine:
            {
                int i = (int)std::lround(plain) - 1;
                out = diag::clip(i >= 0 && i < (int)lines_.size() ? diag::fmt("%d: ", i + 1) + lines_[(size_t)i]
                                                                  : diag::fmt("%d: (end of report)", i + 1));
                return true;
            }
            case kStatus:   out = status_;   return true;
            case kDevice:   out = device_;   return true;
            case kStream:   out = stream_;   return true;
            case kDropouts: out = dropouts_; return true;
            case kSystem:   out = system_;   return true;
            default: return false;
        }
    }

private:
    // Monitor thread: refresh the texts every 0.5 s, redraw the page when something visible changed
    // (at most once per second), write the report file when its content changed.
    void monitor()
    {
        auto lastRefresh = std::chrono::steady_clock::now();
        int tick = 0;
        while (!stop_.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (++tick % 5 && !dirty_.load()) continue;
            bool full = tick % 20 == 0;     // diagnostics scan every 2 s while not streaming
            bool changed = rebuild(full) || dirty_.exchange(false);
            auto now = std::chrono::steady_clock::now();
            if (changed && now - lastRefresh > std::chrono::seconds(1))
            {
                requestDisplayUpdate();
                lastRefresh = now;
            }
            else if (changed)
                dirty_.store(true);   // try again next tick
        }
    }

    // Rebuilds all texts. Returns true when a row that is visible without interaction changed.
    bool rebuild(bool runDiag)
    {
        std::string status, device, stream, dropouts;
        std::vector<std::string> lines;
        std::vector<ob::Source> sources;
        lines.push_back(diag::fmt("MPC Overwitch %s", kVersion));

        if (!owner_)
        {
            status = "Busy: other instance has it";
            device = "-";
            stream = "-";
            dropouts = "-";
            lines.push_back("only one instance can stream");
        }
        else
        {
            const int st = engine_->state();
            const std::string msg = engine_->message();
            device = diag::clip(engine_->deviceName().empty() ? "none" : engine_->deviceName());
            sources = engine_->sources();
            switch (st)
            {
                case ob::Engine::kSearching:      status = "Searching for device..."; break;
                case ob::Engine::kNotOverbridge:  status = "Set device USB: Overbridge"; break;
                case ob::Engine::kUnknownDevice:  status = "Unsupported Elektron device"; break;
                case ob::Engine::kOpening:        status = "Connecting..."; break;
                case ob::Engine::kStreaming:      status = reader_.primed() ? "STREAMING" : "Streaming (buffering)"; break;
                case ob::Engine::kError:          status = diag::clip("Error: " + msg); break;
                default:                          status = "Stopped"; break;
            }
            if (st == ob::Engine::kStreaming)
                stream = diag::clip(diag::fmt("%.2f kHz, %.1f ms, %+.0f ppm", engine_->measuredRate() / 1000.0,
                                              reader_.latencyMs(), driftAvg()));
            else
                stream = "-";
            dropouts = diag::fmt("%u dropouts, %u USB errors", reader_.dropouts(), engine_->usbErrors());
            lines.push_back(status);
            lines.push_back(device);
            if (!msg.empty()) lines.push_back(diag::clip("engine: " + msg));
            lines.push_back(diag::clip("rate " + stream));
            lines.push_back(diag::clip(dropouts));
            lines.push_back(engine_->rtPriority() ? "usb thread: realtime" : "usb thread: normal prio");
            int si = (int)std::lround(get(kSource));
            if (si >= 0 && si < (int)sources.size())
                lines.push_back(diag::clip(diag::fmt("source %s = out %d/%d", sources[(size_t)si].name.c_str(),
                                                     sources[(size_t)si].left + 1, sources[(size_t)si].right + 1)));
            if (st != ob::Engine::kStreaming && runDiag)
                diagLines_ = diag::scan(false, ++scans_).lines;
            if (st != ob::Engine::kStreaming)
                for (size_t i = 1; i < diagLines_.size(); ++i) lines.push_back(diagLines_[i]);
        }
        lines.push_back(system_);

        // Also redraw once when the first rate measurement arrives (the STREAM row showed 0.00 kHz until reopened).
        const bool rateKnown = owner_ && engine_->measuredRate() > 1000.0;
        std::string key = status + "|" + device + "|" + dropouts + (rateKnown ? "|rate" : "");
        bool changed;
        std::string report;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            changed = key != visibleKey_;
            visibleKey_ = key;
            status_ = status; device_ = device; stream_ = stream; dropouts_ = dropouts;
            lines_ = lines;
            devSources_ = sources;
            for (const std::string& s : lines) report += s + "\n";
        }
        if (changed) writeReport(report);
        return changed;
    }

    // Averages the resampler's drift estimate (it jitters with block timing) for display.
    double driftAvg()
    {
        driftAvg_ += (reader_.driftPpm() - driftAvg_) * 0.2;
        return driftAvg_;
    }

    void writeReport(const std::string& body)
    {
        std::string path = mpc::pluginFolder() + "/MPC-Overwitch-Report.txt";
        if (FILE* f = std::fopen(path.c_str(), "w"))
        {
            std::fwrite(body.data(), 1, body.size(), f);
            std::fclose(f);
        }
    }

    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo info;
        info.name         = "MPC Overwitch";
        info.vendor       = "NOXED";   // must match plugin.json "manufacturer"
        info.uniqueId     = MPC_FOURCC(kUid[0], kUid[1], kUid[2], kUid[3]);   // must match plugin.json "uid"
        info.version      = 201;            // 0.2.1
        info.isInstrument = true;
        info.numInputs    = 0;
        info.numOutputs   = 2;
        info.tailSamples  = 0;
        return info;
    }

    static constexpr char kUid[5] = "NMmv";

    bool owner_ = false;
    std::unique_ptr<ob::Engine> engine_;
    ob::DriftReader reader_;
    float gain_ = 0.0f;

    std::thread monitor_;
    std::atomic<bool> stop_{false}, dirty_{false};
    std::mutex mutex_;
    std::string status_, device_, stream_, dropouts_, system_, visibleKey_;
    std::vector<std::string> lines_, diagLines_;
    std::vector<ob::Source> devSources_;
    double driftAvg_ = 0.0;
    int scans_ = 0;
};

// This macro creates the exported VSTPluginMain entry point. Keep it last.
MPC_PLUGIN_ENTRY(MPCOverwitch)
