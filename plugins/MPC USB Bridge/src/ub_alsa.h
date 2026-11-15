// ub_alsa.h - capture engine of MPC USB Bridge: records a class-compliant USB audio device through the
// MPC's own ALSA library (libasound, loaded at run time with dlopen; no headers or extra files needed)
// and hands the frames to the audio thread through a lock-free ring. Original NOXED code, GPL-3.0-or-later.
//
// Thread: one capture thread per engine (real-time priority when allowed). It picks a card (Auto: the
// first USB card that is not the MPC itself and offers capture, Roland preferred; or a fixed card),
// opens "hw:N,0" for capture, negotiates format / rate / channels, then reads periods of ~128 frames.
// Overruns are recovered with snd_pcm_recover; an unplugged device ends the stream and the thread goes
// back to searching. The audio thread only reads the ring (ob_stream.h DriftReader follows the clock
// drift between the USB device and the MPC).
#pragma once

#include "../../MPC Overwitch/src/ob_stream.h"
#include "ub_diag.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <type_traits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>

namespace ub {

// The few libasound entry points we need (ALSA ABI, stable for decades).
struct Alsa
{
    using pcm_t = void;
    using hw_t = void;
    int (*open)(pcm_t**, const char*, int, int) = nullptr;
    int (*close)(pcm_t*) = nullptr;
    int (*hwMalloc)(hw_t**) = nullptr;
    void (*hwFree)(hw_t*) = nullptr;
    int (*hwAny)(pcm_t*, hw_t*) = nullptr;
    int (*setAccess)(pcm_t*, hw_t*, int) = nullptr;
    int (*setFormat)(pcm_t*, hw_t*, int) = nullptr;
    int (*setChannelsNear)(pcm_t*, hw_t*, unsigned*) = nullptr;
    int (*setRateNear)(pcm_t*, hw_t*, unsigned*, int*) = nullptr;
    int (*setPeriodNear)(pcm_t*, hw_t*, unsigned long*, int*) = nullptr;
    int (*setBufferNear)(pcm_t*, hw_t*, unsigned long*) = nullptr;
    int (*hwParams)(pcm_t*, hw_t*) = nullptr;
    int (*prepare)(pcm_t*) = nullptr;
    int (*start)(pcm_t*) = nullptr;
    long (*readi)(pcm_t*, void*, unsigned long) = nullptr;
    int (*recover)(pcm_t*, int, int) = nullptr;
    const char* (*strerror)(int) = nullptr;
    bool ok = false;

    bool load(std::string& err)
    {
        void* h = dlopen("libasound.so.2", RTLD_NOW | RTLD_NOLOAD);
        if (!h) h = dlopen("libasound.so.2", RTLD_NOW);
        if (!h) { err = "no libasound"; return false; }
        bool all = true;
        auto get = [&](auto& fn, const char* name) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(dlsym(h, name));
            if (!fn) { all = false; err = std::string("libasound lacks ") + name; }
        };
        get(open, "snd_pcm_open"); get(close, "snd_pcm_close");
        get(hwMalloc, "snd_pcm_hw_params_malloc"); get(hwFree, "snd_pcm_hw_params_free");
        get(hwAny, "snd_pcm_hw_params_any"); get(setAccess, "snd_pcm_hw_params_set_access");
        get(setFormat, "snd_pcm_hw_params_set_format"); get(setChannelsNear, "snd_pcm_hw_params_set_channels_near");
        get(setRateNear, "snd_pcm_hw_params_set_rate_near"); get(setPeriodNear, "snd_pcm_hw_params_set_period_size_near");
        get(setBufferNear, "snd_pcm_hw_params_set_buffer_size_near"); get(hwParams, "snd_pcm_hw_params");
        get(prepare, "snd_pcm_prepare"); get(start, "snd_pcm_start"); get(readi, "snd_pcm_readi");
        get(recover, "snd_pcm_recover"); get(strerror, "snd_strerror");
        ok = all;
        return all;
    }
};

constexpr int kStreamCapture = 1;
constexpr int kAccessRwInterleaved = 3;
enum Fmt { kS16 = 2, kS24_4 = 6, kS32 = 10, kFloat = 14, kS24_3 = 32 };

struct Card { int index = -1; std::string id, usbid; };

// Cards that offer capture, MPC itself (Akai 09e8) excluded; Roland first.
inline std::vector<Card> captureCards()
{
    std::vector<Card> out;
    for (int c = 0; c < 16; ++c)
    {
        char p[64];
        std::snprintf(p, sizeof(p), "card%d/id", c);
        std::string id = ubdiag::trim(ubdiag::readAll(ubdiag::asound(p)));
        if (id.empty()) continue;
        std::snprintf(p, sizeof(p), "card%d/usbid", c);
        std::string usb = ubdiag::trim(ubdiag::readAll(ubdiag::asound(p)));
        if (usb.empty() || usb.compare(0, 4, "09e8") == 0) continue;
        std::snprintf(p, sizeof(p), "card%d/stream0", c);
        if (ubdiag::readAll(ubdiag::asound(p)).find("Capture:") == std::string::npos) continue;
        Card k{c, id, usb};
        if (usb.compare(0, 4, "0582") == 0) out.insert(out.begin(), k); else out.push_back(k);
    }
    return out;
}

class Capture
{
public:
    enum State { kSearching, kOpening, kStreaming, kError, kStopped };

    ob::StereoRing ring;
    std::atomic<int> wantCard{-1};   // -1 = auto, else ALSA card index

    ~Capture() { stop(); }
    void start()
    {
        if (thread_.joinable()) return;
        stop_.store(false);
        thread_ = std::thread([this] { run(); });
    }
    void stop()
    {
        stop_.store(true);
        if (thread_.joinable()) thread_.join();
    }

    int state() const { return state_.load(); }
    double rate() const { return rate_.load(); }
    double measuredRate() const { return measured_.load(); }
    int period() const { return period_.load(); }
    uint32_t xruns() const { return xruns_.load(); }
    bool rt() const { return rt_.load(); }
    std::string device() { std::lock_guard<std::mutex> l(m_); return device_; }
    std::string message() { std::lock_guard<std::mutex> l(m_); return msg_; }
    std::string format() { std::lock_guard<std::mutex> l(m_); return fmtName_; }

private:
    void set(int st, const std::string& msg)
    {
        { std::lock_guard<std::mutex> l(m_); msg_ = msg; }
        state_.store(st);
    }
    bool sleepFor(int ms)
    {
        for (int t = 0; t < ms && !stop_.load(); t += 50) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return !stop_.load();
    }

    void run()
    {
        sched_param sp{};
        sp.sched_priority = 60;
        rt_.store(pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) == 0);
        std::string err;
        if (!alsa_.load(err)) { set(kError, err); while (sleepFor(1000)) {} set(kStopped, "stopped"); return; }
        while (!stop_.load())
        {
            std::string pcmName, label;
            const char* forced = std::getenv("USB_BRIDGE_PCM");   // desktop tests: e.g. "null"
            if (forced) { pcmName = forced; label = forced; }
            else
            {
                std::vector<Card> cards = captureCards();
                const int want = wantCard.load();
                const Card* pick = nullptr;
                for (const Card& c : cards)
                    if (want < 0 || c.index == want) { pick = &c; break; }
                if (!pick) { set(kSearching, want < 0 ? "connect a USB audio device" : "chosen card not found"); sleepFor(1000); continue; }
                pcmName = "hw:" + std::to_string(pick->index) + ",0";
                label = pick->id + " (" + pick->usbid + ")";
            }
            { std::lock_guard<std::mutex> l(m_); device_ = label; }
            set(kOpening, "opening " + pcmName);
            void* pcm = nullptr;
            if (!openPcm(pcmName, pcm, err)) { set(kError, err); sleepFor(2000); continue; }
            set(kStreaming, "streaming");
            stream(pcm);
            alsa_.close(pcm);
            rate_.store(0); measured_.store(0);
            if (!stop_.load()) set(kSearching, "device gone, searching");
            sleepFor(500);
        }
        set(kStopped, "stopped");
    }

    bool openPcm(const std::string& name, void*& pcm, std::string& err)
    {
        int r = alsa_.open(&pcm, name.c_str(), kStreamCapture, 0);
        if (r < 0) { err = "open: " + std::string(alsa_.strerror(r)); return false; }
        void* hw = nullptr;
        alsa_.hwMalloc(&hw);
        bool ok = alsa_.hwAny(pcm, hw) >= 0 && alsa_.setAccess(pcm, hw, kAccessRwInterleaved) >= 0;
        fmt_ = -1;
        if (ok)
            for (int f : {kS32, kS24_3, kS24_4, kS16, kFloat})
                if (alsa_.setFormat(pcm, hw, f) >= 0) { fmt_ = f; break; }
        unsigned ch = 2, rate = 48000;
        unsigned long per = 128, buf = 128 * 8;
        int dir = 0;
        // Prefer the device's 44.1 kHz when it has it (S-1), else 48 kHz; the resampler handles either.
        unsigned want44 = 44100;
        if (ok && fmt_ >= 0)
        {
            alsa_.setChannelsNear(pcm, hw, &ch);
            rate = want44;
            alsa_.setRateNear(pcm, hw, &rate, &dir);
            alsa_.setPeriodNear(pcm, hw, &per, &dir);
            alsa_.setBufferNear(pcm, hw, &buf);
            r = alsa_.hwParams(pcm, hw);
            ok = r >= 0;
            if (!ok) err = "hw params: " + std::string(alsa_.strerror(r));
        }
        else if (fmt_ < 0) { ok = false; err = "no supported sample format"; }
        else err = "hw params refused";
        alsa_.hwFree(hw);
        if (!ok) { alsa_.close(pcm); pcm = nullptr; return false; }
        channels_ = (int)ch;
        rate_.store(rate);
        period_.store((int)per);
        const char* fn = fmt_ == kS32 ? "S32" : fmt_ == kS24_3 ? "S24_3" : fmt_ == kS24_4 ? "S24" : fmt_ == kS16 ? "S16" : "FLOAT";
        { std::lock_guard<std::mutex> l(m_); fmtName_ = std::string(fn) + " " + std::to_string(ch) + "ch"; }
        bytesPerSample_ = fmt_ == kS16 ? 2 : fmt_ == kS24_3 ? 3 : 4;
        buf_.assign((size_t)per * (size_t)ch * (size_t)bytesPerSample_, 0);
        alsa_.prepare(pcm);
        alsa_.start(pcm);
        return true;
    }

    float sample(const uint8_t* p) const
    {
        switch (fmt_)
        {
            case kS32:   { int32_t v; std::memcpy(&v, p, 4); return v * (1.0f / 2147483648.0f); }
            case kS24_4: { int32_t v; std::memcpy(&v, p, 4); v = (v << 8) >> 8; return v * (1.0f / 8388608.0f); }
            case kS24_3: { int32_t v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24); return v * (1.0f / 2147483648.0f); }
            case kS16:   { int16_t v; std::memcpy(&v, p, 2); return v * (1.0f / 32768.0f); }
            default:     { float v; std::memcpy(&v, p, 4); return v; }
        }
    }

    void stream(void* pcm)
    {
        const unsigned long per = (unsigned long)period_.load();
        const int frameBytes = channels_ * bytesPerSample_;
        auto t0 = std::chrono::steady_clock::now();
        uint64_t frames = 0;
        // Desktop tests only: ALSA's "null" device is not clocked, so pace it to its nominal rate.
        const bool pace = std::getenv("USB_BRIDGE_TEST_PACE") != nullptr;
        auto tick = t0;
        while (!stop_.load())
        {
            long n = alsa_.readi(pcm, buf_.data(), per);
            if (n < 0)
            {
                if (n == -ENODEV || n == -EBADFD || n == -EIO) break;           // unplugged
                ++xruns_;
                if (alsa_.recover(pcm, (int)n, 1) < 0) break;
                alsa_.start(pcm);
                continue;
            }
            for (long i = 0; i < n; ++i)
            {
                const uint8_t* fr = &buf_[(size_t)i * (size_t)frameBytes];
                const float l = sample(fr);
                const float r = channels_ > 1 ? sample(fr + bytesPerSample_) : l;
                ring.push(l, r);
            }
            ring.publish();
            if (pace)
            {
                tick += std::chrono::nanoseconds((long long)(1e9 * (double)n / rate_.load()));
                std::this_thread::sleep_until(tick);
            }
            frames += (uint64_t)n;
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - t0).count();
            if (dt >= 2.0) { measured_.store(frames / dt); frames = 0; t0 = now; }
        }
    }

    Alsa alsa_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<int> state_{kSearching};
    std::atomic<double> rate_{0}, measured_{0};
    std::atomic<int> period_{0};
    std::atomic<uint32_t> xruns_{0};
    std::atomic<bool> rt_{false};
    std::mutex m_;
    std::string device_, msg_, fmtName_;
    int fmt_ = -1, channels_ = 2, bytesPerSample_ = 4;
    std::vector<uint8_t> buf_;
};

} // namespace ub
