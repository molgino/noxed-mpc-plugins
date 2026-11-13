// MPC Overwitch Send - NOXED: sends the audio of an MPC track to an Elektron Overbridge device.
//
// Signal flow
//   track audio --SEND level--> ring --> engine thread (resampled host rate -> 48 kHz, drift-following,
//   mixed with other sends) --USB Overbridge--> device input (Main / Delay / Reverb / Chorus on Digitone II)
//   track audio --DRY level--> out (so the track keeps sounding unless DRY is turned down)
//
// It shares the USB engine of MPC Overwitch (ob_shared.h): both plugins can run at the same time and
// use one connection. Insert it on any track; its sound comes back through an MPC Overwitch instance
// (for example Source "Main" or "Delay") or the device's own outputs.
//
// Parameters (0.x: may still change before 1.0)
//   0 Dry          track signal passed through (param 0, nudged by the host on load: harmless)
//   1 Destination  device input: Main, Delay, Reverb, Chorus (Digitone II names)
//   2 Send         level sent to the device
//   3..6           read-only status rows: Status, Device, Stream, Levels (IN = signal arriving at
//                  the plugin after SEND, USB = what the engine actually mixes into the device input)
//
// Licence: GPL-3.0-or-later (shares code with MPC Overwitch, based on Overwitch).

#include "mpc_plugin.h"
#include "mpc_resources.h"

#include "../../MPC Overwitch/src/ob_shared.h"
#include "../../MPC Overwitch/src/ob_diag.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
enum ParamIndex { kDry = 0, kDest, kSend, kStatus, kDevice, kStream, kLevels, kNumParams };
std::vector<std::string> destNames() { return {"Main", "Delay", "Reverb", "Chorus"}; }
std::string dB(float g) { return g < 1e-5f ? "-inf dB" : diag::fmt("%.1f dB", 20.0f * std::log10(g)); }
} // namespace

class MPCOverwitchSend : public mpc::Plugin
{
public:
    MPCOverwitchSend() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::floatParam("dry", "Dry", 0.0f, 1.0f, 1.0f));
        addParam(mpc::choiceParam("dest", "Destination", destNames(), 0));
        addParam(mpc::floatParam("send", "Send", 0.0f, 1.0f, 0.8f));
        const char* ro[] = {"status", "device", "stream", "levels"};
        const char* names[] = {"Status", "Device", "Stream", "Levels"};
        for (int i = 0; i < 4; ++i)
        {
            mpc::Param p = mpc::floatParam(ro[i], names[i], 0.0f, 1.0f, 0.0f);
            p.automatable = false;
            addParam(p);
        }
        shared_ = ob::acquireShared(err_);
        if (shared_) slot_ = ob::claimSlot(shared_, ob::Slot::kSend, 0);
        rebuild();
        monitor_ = std::thread([this] { monitor(); });
    }

    ~MPCOverwitchSend() override
    {
        stop_.store(true);
        if (monitor_.joinable()) monitor_.join();
        ob::freeSlot(shared_, slot_);
        ob::releaseShared(shared_);
    }

    void prepare(double sampleRate, int maxBlockSize) override
    {
        if (slot_ >= 0)
        {
            ob::Slot& sl = shared_->slots[slot_];
            sl.hostRate.store(sampleRate);
            sl.hostBlock.store(maxBlockSize > 0 ? maxBlockSize : 512);
            sl.epoch.fetch_add(1, std::memory_order_release);   // engine re-primes its reader
        }
        dryG_ = 1.0f;
        sendG_ = 0.0f;
    }

    void reset() override
    {
        if (slot_ >= 0) shared_->slots[slot_].epoch.fetch_add(1, std::memory_order_release);
    }

    void process(mpc::ProcessContext& ctx) override
    {
        const int n = ctx.numFrames;
        const float* inL = ctx.inputs ? ctx.inputs[0] : nullptr;
        const float* inR = ctx.inputs && ctx.numInputs > 1 ? ctx.inputs[1] : inL;
        float* outL = ctx.outputs[0];
        float* outR = ctx.outputs[1];
        const float dryT = get(kDry);
        const float s = get(kSend);
        const float sendT = s * s * 1.25f;
        const bool live = slot_ >= 0 && shared_->engine.state() == ob::Engine::kStreaming;
        ob::Slot* sl = live ? &shared_->slots[slot_] : nullptr;
        if (sl)
        {
            const int d = (int)std::lround(get(kDest));
            if (sl->route.load(std::memory_order_relaxed) != d) sl->route.store(d, std::memory_order_relaxed);
        }
        float dg = dryG_, sg = sendG_, pk = 0.0f;
        const float k = 0.002f;
        for (int i = 0; i < n; ++i)
        {
            const float l = inL ? inL[i] : 0.0f;    // read the frame before writing it (in-place buffers)
            const float r = inR ? inR[i] : 0.0f;
            dg += (dryT - dg) * k;
            sg += (sendT - sg) * k;
            const float sL = l * sg, sR = r * sg;
            pk = std::max(pk, std::max(std::fabs(sL), std::fabs(sR)));
            if (sl) sl->ring.push(sL, sR);
            outL[i] = l * dg;
            outR[i] = r * dg;
        }
        if (pk > inPeak_.load(std::memory_order_relaxed)) inPeak_.store(pk, std::memory_order_relaxed);
        if (sl) sl->ring.publish();
        dryG_ = dg;
        sendG_ = sg;
    }

    bool formatValue(int index, float plain, std::string& out) override
    {
        if (index == kDry) { out = dB(plain); return true; }
        if (index == kSend) { out = dB(plain * plain * 1.25f); return true; }
        std::lock_guard<std::mutex> lock(mutex_);
        switch (index)
        {
            case kDest:
            {
                int i = (int)std::lround(plain);
                if (!dests_.empty()) out = i < (int)dests_.size() ? dests_[(size_t)i].name : "(not on this device)";
                else { auto n = destNames(); out = i >= 0 && i < (int)n.size() ? n[(size_t)i] : "?"; }
                return true;
            }
            case kStatus: out = status_; return true;
            case kDevice: out = device_; return true;
            case kStream: out = stream_; return true;
            case kLevels: out = levels_; return true;
            default: return false;
        }
    }

private:
    void monitor()
    {
        auto last = std::chrono::steady_clock::now();
        bool pending = false;
        while (!stop_.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            pending = rebuild() || pending;
            auto now = std::chrono::steady_clock::now();
            if (pending && now - last > std::chrono::seconds(1))
            {
                requestDisplayUpdate();
                last = now;
                pending = false;
            }
        }
    }

    bool rebuild()
    {
        std::string status, device = "-", stream = "-";
        std::vector<ob::Source> dests;
        if (!shared_) status = diag::clip("Error: " + err_);
        else if (slot_ < 0) status = "Busy: all 16 slots in use";
        else
        {
            ob::Engine& e = shared_->engine;
            const int st = e.state();
            device = diag::clip(e.deviceName().empty() ? "none" : e.deviceName());
            dests = e.destinations();
            switch (st)
            {
                case ob::Engine::kSearching:     status = "Searching for device..."; break;
                case ob::Engine::kNotOverbridge: status = "Set device USB: Overbridge"; break;
                case ob::Engine::kUnknownDevice: status = "Unsupported Elektron device"; break;
                case ob::Engine::kOpening:       status = "Connecting..."; break;
                case ob::Engine::kStreaming:     status = "SENDING"; break;
                case ob::Engine::kError:         status = diag::clip("Error: " + e.message()); break;
                default:                         status = "Stopped"; break;
            }
            if (st == ob::Engine::kStreaming)
            {
                const ob::Slot& sl = shared_->slots[slot_];
                stream = diag::clip(diag::fmt("%.1f ms, %u dropouts", sl.sendLatencyMs.load(), sl.sendDropouts.load()));
            }
        }
        // Peaks since the last rebuild (0.5 s). The row redraws when IN or USB starts or stops carrying signal.
        const float in = inPeak_.exchange(0.0f);
        const float usb = (shared_ && slot_ >= 0) ? shared_->slots[slot_].sendUsbPeak.exchange(0.0f) : 0.0f;
        auto lvl = [](float v) { return v < 1e-4f ? std::string("silent") : diag::fmt("%.0f dB", 20.0f * std::log10(v)); };
        std::string levels = "IN " + lvl(in) + " / USB " + lvl(usb);
        const bool rateKnown = stream != "-";
        std::string key = status + "|" + device + (rateKnown ? "|s" : "") + (in >= 1e-4f ? "|i" : "") + (usb >= 1e-4f ? "|u" : "");
        std::lock_guard<std::mutex> lock(mutex_);
        bool changed = key != key_;
        key_ = key;
        status_ = status; device_ = device; stream_ = stream; levels_ = levels; dests_ = dests;
        return changed;
    }

    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo info;
        info.name         = "MPC Overwitch Send";
        info.vendor       = "NOXED";   // must match plugin.json "manufacturer"
        info.uniqueId     = MPC_FOURCC(kUid[0], kUid[1], kUid[2], kUid[3]);   // must match plugin.json "uid"
        info.version      = 301;            // 0.3.1
        info.isInstrument = false;
        info.numInputs    = 2;
        info.numOutputs   = 2;
        info.tailSamples  = 0;
        return info;
    }

    static constexpr char kUid[5] = "NFpQ";

    ob::Shared* shared_ = nullptr;
    int slot_ = -1;
    std::string err_;
    float dryG_ = 1.0f, sendG_ = 0.0f;
    std::thread monitor_;
    std::atomic<bool> stop_{false};
    std::mutex mutex_;
    std::string status_, device_, stream_, levels_, key_;
    std::atomic<float> inPeak_{0.0f};
    std::vector<ob::Source> dests_;
};

// This macro creates the exported VSTPluginMain entry point. Keep it last.
MPC_PLUGIN_ENTRY(MPCOverwitchSend)
