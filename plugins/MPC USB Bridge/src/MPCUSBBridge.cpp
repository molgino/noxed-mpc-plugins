// MPC USB Bridge - NOXED 0.2. Brings a class-compliant USB audio device that the MPC does not use as its
// interface (Roland AIRA Compact S-1, T-8, J-6, ...) into the MPC as a plugin track.
//
// Signal flow:
//   USB device --(ALSA capture through the MPC's own libasound, ub_alsa.h, capture thread)-->
//   lock-free ring --(ob_stream.h DriftReader: resampler device rate -> MPC rate that follows the clock
//   drift between the two devices)--> LEVEL --> plugin output (stereo; a mono device is copied to both).
//
// Parameters (order and ids are permanent from 0.2 on):
//   0 Level   output gain (param 0, nudged by the host on load: harmless)
//   1 Card    Auto = first USB audio card that is not the MPC (Roland preferred), or a fixed ALSA card
//   2 Report  scrolls the USB / ALSA report (also written to MPC-USB-Bridge-Report.txt)
//   3..7      read-only rows Status, Device, Stream, Levels, System
//   8 MIDI Ch Off or 1..16: notes / CC / pitch bend / aftertouch / program changes of this MPC track go
//             to the device's own MIDI port on this channel (ob_midi.h; the MPC host itself cannot)
// Licence: GPL-3.0-or-later.

#include "mpc_plugin.h"
#include "mpc_resources.h"

#include "ub_alsa.h"
#include "../../MPC Overwitch/src/ob_midi.h"
#include "ub_diag.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

enum ParamIndex { kLevel = 0, kCard, kLine, kStatus, kDevice, kStream, kLevels, kSystem, kMidiCh, kUiOpen, kNumParams };
constexpr int kMaxLines = 96;
constexpr int kMaxCard = 16;

} // namespace

class MPCUSBBridge : public mpc::Plugin
{
public:
    MPCUSBBridge() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::floatParam("level", "Level", 0.0f, 1.0f, 0.8f));
        addParam(mpc::intParam("card", "Card", 0, kMaxCard, 0));
        addParam(mpc::intParam("line", "Report", 1, kMaxLines, 1));
        const char* ids[] = {"status", "device", "stream", "levels", "system"};
        const char* names[] = {"Status", "Device", "Stream", "Levels", "System"};
        for (int i = 0; i < 5; ++i)
        {
            mpc::Param p = mpc::floatParam(ids[i], names[i], 0.0f, 1.0f, 0.0f);
            p.automatable = false;
            addParam(p);
        }
        std::vector<std::string> chans = {"Off"};
        for (int c = 1; c <= 16; ++c) chans.push_back(std::to_string(c));
        addParam(mpc::choiceParam("midich", "MIDI Ch", chans, 1));
        // UI only: open drawn pick-list (0 none, k+1 = parameter k); MPC's native list overlay is empty for VST2.
        mpc::Param pick = mpc::intParam("ui_open", "Choice Picker", 0, kUiOpen, 0);
        pick.automatable = false;
        addParam(pick);
        rep_ = ubdiag::scan(++scans_);
        rows();
        write();
        capture_.start();
        midi_.start();
        monitor_ = std::thread([this] { run(); });
    }

    ~MPCUSBBridge() override
    {
        stop_.store(true);
        if (monitor_.joinable()) monitor_.join();
        midi_.stop();
        capture_.stop();
    }

    void prepare(double sampleRate, int) override
    {
        hostRate_ = sampleRate > 0 ? sampleRate : 44100.0;
        srcRate_ = 0.0;   // re-primed on the next block
        gain_ = 0.0f;
    }
    void reset() override { reader_.reset(); midi_.releaseHeld(); set(kUiOpen, 0); }

    void parameterChanged(int index, float) override
    {
        if (index != kUiOpen && (int)std::lround(get(kUiOpen)) == index + 1) set(kUiOpen, 0);
        if (index == kUiOpen) pickerDirty_.store(true);
    }

    void process(mpc::ProcessContext& ctx) override
    {
        // MIDI from the track to the device (before the audio, independent of it).
        const int ch = (int)std::lround(get(kMidiCh)) - 1;
        if (wasPlaying_ && !ctx.transport.playing) midi_.releaseHeld();
        wasPlaying_ = ctx.transport.playing;
        for (int i = 0; i < ctx.numMidiEvents; ++i) midi_.process(ctx.midi[i].data, ctx.midi[i].size, ch);
        midiCh_.store(ch, std::memory_order_relaxed);

        float* l = ctx.outputs[0];
        float* r = ctx.outputs[1];
        const int n = ctx.numFrames;
        const int card = (int)std::lround(get(kCard)) - 1;
        if (capture_.wantCard.load(std::memory_order_relaxed) != card) capture_.wantCard.store(card, std::memory_order_relaxed);
        const double rate = capture_.rate();
        if (capture_.state() != ub::Capture::kStreaming || rate <= 0)
        {
            reader_.reset();
            for (int i = 0; i < n; ++i) l[i] = r[i] = 0.0f;
            peak_.store(0.0f, std::memory_order_relaxed);
            return;
        }
        if (rate != srcRate_)
        {
            srcRate_ = rate;
            const int per = capture_.period();
            reader_.prepare(rate, hostRate_, per > 0 ? per : 128);
        }
        reader_.read(capture_.ring, l, r, n);
        // Level: perceptual (squared) curve, smoothed per sample to avoid zipper noise.
        const float lv = get(kLevel);
        const float target = lv * lv * 1.25f;
        float g = gain_, pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            g += (target - g) * 0.002f;
            l[i] *= g;
            r[i] *= g;
            pk = std::max(pk, std::max(std::fabs(l[i]), std::fabs(r[i])));
        }
        gain_ = g;
        if (pk > peak_.load(std::memory_order_relaxed)) peak_.store(pk, std::memory_order_relaxed);
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
            case kCard:
            {
                int c = (int)std::lround(plain) - 1;
                if (c < 0) { out = "Auto (first USB audio device)"; return true; }
                std::string id = ubdiag::trim(ubdiag::readAll(ubdiag::asound("card" + std::to_string(c) + "/id")));
                out = diag::fmt("Card %d: ", c) + (id.empty() ? std::string("(none)") : id);
                return true;
            }
            case kLine:
            {
                int i = (int)std::lround(plain) - 1;
                out = diag::clip(i >= 0 && i < (int)rep_.lines.size() ? diag::fmt("%d: ", i + 1) + rep_.lines[(size_t)i]
                                                                      : diag::fmt("%d: (end of report)", i + 1));
                return true;
            }
            case kStatus: out = status_; return true;
            case kDevice: out = device_; return true;
            case kStream: out = stream_; return true;
            case kLevels: out = levels_; return true;
            case kSystem: out = rep_.system; return true;
            case kMidiCh:
            {
                int c = (int)std::lround(plain);
                out = c <= 0 ? "Off" : "Ch " + std::to_string(c);
                return true;
            }
            default: return false;
        }
    }

private:
    // Updates the status rows twice a second and the full report every 10 s; redraws the page when a
    // visible row changed.
    void run()
    {
        int tick = 0;
        while (!stop_.load())
        {
            for (int i = 0; i < 20 && !stop_.load(); ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                if (pickerDirty_.exchange(false)) { notifyHostParameterChanged(kUiOpen); requestDisplayUpdate(); }
            }
            if (stop_.load()) break;
            if (++tick % 20 == 0)
            {
                ubdiag::Report r = ubdiag::scan(++scans_);
                std::lock_guard<std::mutex> lock(mutex_);
                rep_ = std::move(r);
            }
            std::string before, after;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                before = status_ + device_ + stream_ + levels_;
            }
            rows();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                after = status_ + device_ + stream_ + levels_;
            }
            if (tick % 20 == 0) write();
            if (after != before) requestDisplayUpdate();
        }
    }

    void rows()
    {
        const int st = capture_.state();
        std::string status, device = capture_.device(), stream, levels;
        const std::string msg = capture_.message();
        switch (st)
        {
            case ub::Capture::kStreaming: status = reader_.primed() ? "STREAMING" : "Streaming (buffering)"; break;
            case ub::Capture::kOpening:   status = "Opening..."; break;
            case ub::Capture::kSearching: status = "Waiting: " + msg; break;
            case ub::Capture::kError:     status = "ERROR: " + msg; break;
            default:                      status = "Stopped"; break;
        }
        if (device.empty()) device = "-";
        device += "  |  " + midi_.status(midiCh_.load(std::memory_order_relaxed));
        if (st == ub::Capture::kStreaming)
        {
            stream = diag::fmt("%s %.1f kHz (measured %.2f) -> MPC %.1f kHz, period %d", capture_.format().c_str(),
                               capture_.rate() / 1000.0, capture_.measuredRate() / 1000.0, hostRate_ / 1000.0, capture_.period());
            driftAvg_ += (reader_.driftPpm() - driftAvg_) * 0.2;
            const float pk = peak_.exchange(0.0f, std::memory_order_relaxed);
            levels = diag::fmt("peak %s, latency %.1f ms, drift %+.0f ppm, %u dropouts, %u overruns",
                               pk < 1e-5f ? "-inf" : diag::fmt("%.1f dB", 20.0f * std::log10(pk)).c_str(),
                               reader_.latencyMs(), driftAvg_, reader_.dropouts(), capture_.xruns());
            if (!capture_.rt()) levels += ", no RT priority";
        }
        else
        {
            stream = "-";
            levels = "-";
        }
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = status; device_ = device; stream_ = stream; levels_ = levels;
    }

    void write()
    {
        std::string body;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            body = rep_.full + "\nSTREAM\n  status: " + status_ + "\n  device: " + device_ + "\n  stream: " + stream_ + "\n";
        }
        size_t nl = body.find('\n');
        std::string k = nl == std::string::npos ? body : body.substr(nl);
        if (k == lastWritten_) return;
        lastWritten_ = k;
        if (FILE* f = std::fopen((mpc::pluginFolder() + "/MPC-USB-Bridge-Report.txt").c_str(), "w"))
        {
            std::fwrite(body.data(), 1, body.size(), f);
            std::fclose(f);
        }
    }

    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo info;
        info.name         = "MPC USB Bridge";
        info.vendor       = "NOXED";   // must match plugin.json "manufacturer"
        info.uniqueId     = MPC_FOURCC(kUid[0], kUid[1], kUid[2], kUid[3]);   // must match plugin.json "uid"
        info.version      = 230;            // 0.2.3
        info.isInstrument = true;
        info.numInputs    = 0;
        info.numOutputs   = 2;
        info.tailSamples  = 0;
        return info;
    }

    static constexpr char kUid[5] = "NXbD";

    ub::Capture capture_;
    obmidi::Forwarder midi_{[this] { return capture_.card(); }};
    std::atomic<int> midiCh_{0};
    bool wasPlaying_ = false;
    ob::DriftReader reader_;
    double hostRate_ = 44100.0, srcRate_ = 0.0, driftAvg_ = 0.0;
    float gain_ = 0.0f;
    std::atomic<float> peak_{0.0f};

    std::mutex mutex_;
    ubdiag::Report rep_;
    std::string status_ = "Starting...", device_ = "-", stream_ = "-", levels_ = "-";
    std::atomic<bool> stop_{false}, pickerDirty_{false};
    std::thread monitor_;
    std::string lastWritten_;
    int scans_ = 0;
};

// This macro creates the exported VSTPluginMain entry point. Keep it last.
MPC_PLUGIN_ENTRY(MPCUSBBridge)
