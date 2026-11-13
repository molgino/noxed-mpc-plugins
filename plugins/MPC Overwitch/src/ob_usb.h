// ob_usb.h - Overbridge 2 / 2.1 USB engine for MPC Overwitch, talking to the device through the
// Linux usbfs interface (/dev/bus/usb), so nothing beyond the C library is needed on the MPC.
//
// Protocol: Overwitch engine.c (Copyright (C) 2019 Stefan Rehm, (C) 2021 David Garcia Goni, MIT
// licence) - interface 1 alt 3 / endpoint 0x83 carries device->host audio, interface 2 alt 3 /
// endpoint 0x03 host->device; both use interrupt transfers of 24 blocks x 7 frames at 48 kHz.
// Each block: 16-bit header, 16-bit frame counter (big endian), 28 private bytes, then 7 frames of
// big-endian samples whose sizes come from the device table (4 or 3 bytes on Overbridge 2.1).
// This file: original NOXED code, GPL-3.0-or-later.
//
// Threading: one engine thread does everything USB (find, open, stream, recover). The audio thread
// only reads the lock-free ring. Nothing here runs on the audio thread.
#pragma once

#include "ob_devices.h"
#include "ob_stream.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/usbdevice_fs.h>)
#include <linux/usbdevice_fs.h>
#define OB_HAVE_USBFS_H 1
#endif
#endif

#ifndef OB_HAVE_USBFS_H
// Minimal copy of the Linux usbfs ABI (include/uapi/linux/usbdevice_fs.h).
struct usbdevfs_ctrltransfer { uint8_t bRequestType, bRequest; uint16_t wValue, wIndex, wLength; uint32_t timeout; void* data; };
struct usbdevfs_setinterface { unsigned int interface, altsetting; };
struct usbdevfs_iso_packet_desc { unsigned int length, actual_length, status; };
struct usbdevfs_urb
{
    unsigned char type, endpoint;
    int status;
    unsigned int flags;
    void* buffer;
    int buffer_length, actual_length, start_frame;
    union { int number_of_packets; unsigned int stream_id; };
    int error_count;
    unsigned int signr;
    void* usercontext;
    struct usbdevfs_iso_packet_desc iso_frame_desc[0];
};
#define USBDEVFS_URB_TYPE_INTERRUPT 1
#define USBDEVFS_CONTROL _IOWR('U', 0, struct usbdevfs_ctrltransfer)
#define USBDEVFS_SETINTERFACE _IOR('U', 4, struct usbdevfs_setinterface)
#define USBDEVFS_SETCONFIGURATION _IOR('U', 5, unsigned int)
#define USBDEVFS_SUBMITURB _IOR('U', 10, struct usbdevfs_urb)
#define USBDEVFS_DISCARDURB _IO('U', 11)
#define USBDEVFS_REAPURBNDELAY _IOW('U', 13, void*)
#define USBDEVFS_CLAIMINTERFACE _IOR('U', 15, unsigned int)
#define USBDEVFS_RELEASEINTERFACE _IOR('U', 16, unsigned int)
#define USBDEVFS_CLEAR_HALT _IOR('U', 21, unsigned int)
#endif

namespace ob {

constexpr uint16_t kElektronVid = 0x1935;
constexpr int kFramesPerBlock = 7;
constexpr int kBlocksPerTransfer = 24;
constexpr int kBlockHeader = 32;          // header + frames + 28 private bytes
constexpr unsigned kIfIn = 1, kIfOut = 2, kAlt = 3;
constexpr unsigned char kEpIn = 0x83, kEpOut = 0x03;

// One big-endian sample from an Overbridge block. On Overbridge 2.1 the 4-byte slots carry 24 bits in
// their last three bytes (Overwitch: shift the raw word right by 8 before the byte swap).
inline float decodeSample(const uint8_t* s, int size, bool v21)
{
    int32_t hv = 0;
    std::memcpy(&hv, s, (size_t)size);
    if (v21 && size == 4) hv >>= 8;
    hv = (int32_t)__builtin_bswap32((uint32_t)hv);   // big endian on the wire; the MPC is little endian
    return hv * (1.0f / 2147483647.0f);
}

// A selectable stereo (or mono) route among a device's tracks: "Track 1 L"/"Track 1 R" or
// "Main L Input"/"Main R Input" become one stereo route; anything else stays mono.
struct Source { std::string name; int left = -1, right = -1; };

inline std::vector<Source> routesFor(const Track* t, int n)
{
    std::vector<Source> out;
    auto tidy = [](std::string s) {
        std::string r;
        for (char c : s) if (!(c == ' ' && !r.empty() && r.back() == ' ')) r += c;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        return r;
    };
    for (int i = 0; i < n; ++i)
    {
        std::string a = t[i].name;
        size_t p = std::string::npos;
        if (a.size() > 2 && a.compare(a.size() - 2, 2, " L") == 0) p = a.size() - 2;
        else if (a.find(" L ") != std::string::npos) p = a.find(" L ");
        if (p != std::string::npos && i + 1 < n)
        {
            std::string expect = a;
            expect[p + 1] = 'R';
            if (expect == t[i + 1].name)
            {
                out.push_back({tidy(a.substr(0, p) + a.substr(p + 2)), i, i + 1});
                ++i;
                continue;
            }
        }
        out.push_back({tidy(a), i, i});
    }
    return out;
}
inline std::vector<Source> sourcesFor(const Device* d) { return d ? routesFor(d->outputs, d->numOutputs) : std::vector<Source>(); }
inline std::vector<Source> destinationsFor(const Device* d) { return d ? routesFor(d->inputs, d->numInputs) : std::vector<Source>(); }

// One plugin instance's connection to the engine. Receive slots get the decoded source written into
// their ring by the engine; send slots get host audio written into their ring by the plugin, which
// the engine resamples to 48 kHz and mixes into the chosen device input.
constexpr int kMaxSlots = 16;
struct Slot
{
    enum Kind { kFree = 0, kReceive = 1, kSend = 2 };
    std::atomic<int> kind{kFree};
    std::atomic<int> route{0};               // receive: source index; send: destination index
    std::atomic<double> hostRate{44100.0};   // send slots: the host rate and block size of the writer
    std::atomic<int> hostBlock{512};
    std::atomic<uint32_t> epoch{0};          // bumped by the plugin on (re)start; the engine resets its reader
    std::atomic<uint32_t> sendDropouts{0};   // send slots: reported by the engine
    std::atomic<double> sendLatencyMs{0.0};
    StereoRing ring;
};

class Engine
{
public:
    enum State { kStopped, kSearching, kNotOverbridge, kUnknownDevice, kOpening, kStreaming, kError };

    explicit Engine(Slot* slots) : slots_(slots) {}
    ~Engine() { stop(); }

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

    // UI-side accessors.
    int state() const { return state_.load(); }
    uint32_t usbErrors() const { return usbErrors_.load(); }
    uint32_t overruns() const { return overruns_.load(); }
    double measuredRate() const { return rateHz_.load(); }
    bool rtPriority() const { return rt_.load(); }
    std::string deviceName() { std::lock_guard<std::mutex> l(m_); return devName_; }
    std::string message() { std::lock_guard<std::mutex> l(m_); return msg_; }
    std::vector<Source> sources() { std::lock_guard<std::mutex> l(m_); return sources_; }
    std::vector<Source> destinations() { std::lock_guard<std::mutex> l(m_); return dests_; }

private:
    struct Found { bool any = false; uint16_t pid = 0; int bus = 0, dev = 0, config = 0; bool vendorIf = false; std::string product; };

    static std::string readLine(const std::string& p)
    {
        FILE* f = std::fopen(p.c_str(), "r");
        if (!f) return std::string();
        char buf[256] = {};
        if (!std::fgets(buf, sizeof(buf), f)) buf[0] = 0;
        std::fclose(f);
        std::string s(buf);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        return s;
    }

    static Found findElektron()
    {
        Found f;
        const char* fake = std::getenv("OVERWITCH_SYSFS");
        const std::string root = fake ? std::string(fake) + "/" : "/sys/bus/usb/devices/";
        DIR* d = opendir(root.c_str());
        if (!d) return f;
        std::vector<std::string> names;
        while (dirent* e = readdir(d)) if (e->d_name[0] != '.') names.push_back(e->d_name);
        closedir(d);
        for (const std::string& n : names)
        {
            if (n.find(':') != std::string::npos) continue;
            if ((uint16_t)std::strtoul(readLine(root + n + "/idVendor").c_str(), nullptr, 16) != kElektronVid) continue;
            f.any = true;
            f.pid = (uint16_t)std::strtoul(readLine(root + n + "/idProduct").c_str(), nullptr, 16);
            f.bus = std::atoi(readLine(root + n + "/busnum").c_str());
            f.dev = std::atoi(readLine(root + n + "/devnum").c_str());
            f.config = std::atoi(readLine(root + n + "/bConfigurationValue").c_str());
            f.product = readLine(root + n + "/product");
            for (const std::string& m : names)
                if (m.compare(0, n.size() + 1, n + ":") == 0 &&
                    std::strtoul(readLine(root + m + "/bInterfaceClass").c_str(), nullptr, 16) == 0xff)
                    f.vendorIf = true;
            if (ob::findDevice(f.pid)) break;   // prefer a supported device if several Elektron boxes are present
        }
        return f;
    }

    void setState(int s, const std::string& msg = std::string())
    {
        {
            std::lock_guard<std::mutex> l(m_);
            msg_ = msg;
        }
        state_.store(s);
    }

    bool sleepFor(int ms)
    {
        for (int t = 0; t < ms && !stop_.load(); t += 50) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return !stop_.load();
    }

    void run()
    {
        // Try real-time scheduling for the USB thread (works when the host runs as root, as on the MPC).
        sched_param sp{};
        sp.sched_priority = 60;
        rt_.store(pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) == 0);

        while (!stop_.load())
        {
            Found f = findElektron();
            const Device* dev = f.any ? ob::findDevice(f.pid) : nullptr;
            if (!f.any) { setState(kSearching, "connect an Overbridge device"); sleepFor(1000); continue; }
            {
                std::lock_guard<std::mutex> l(m_);
                devName_ = dev ? dev->name : (f.product.empty() ? "Elektron device" : f.product);
            }
            if (!dev)
            {
                setState(f.vendorIf ? kUnknownDevice : kNotOverbridge,
                         f.vendorIf ? "device not in Overbridge 2 table" : "set USB config to Overbridge");
                sleepFor(1000);
                continue;
            }
            {
                std::lock_guard<std::mutex> l(m_);
                sources_ = sourcesFor(dev);
                dests_ = destinationsFor(dev);
            }
            setState(kOpening, "opening");
            std::string err;
            if (open(f, dev, err))
            {
                stream(dev);
                close();
            }
            else
            {
                close();
                setState(kError, err);
                sleepFor(2000);
                continue;
            }
            if (!stop_.load()) setState(kSearching, "device gone, searching");
            sleepFor(500);
        }
        setState(kStopped, "stopped");
    }

    static size_t frameBytes(const Track* t, int n)
    {
        size_t s = 0;
        for (int i = 0; i < n; ++i) s += t[i].size;
        return s;
    }

    // Returns wMaxPacketSize * transactions for an endpoint of a given interface/alt from raw descriptors.
    static int packetSize(const std::vector<uint8_t>& d, unsigned ifNum, unsigned alt, unsigned char ep)
    {
        int curIf = -1, curAlt = -1;
        for (size_t i = 0; i + 2 <= d.size();)
        {
            uint8_t len = d[i], type = d[i + 1];
            if (len < 2 || i + len > d.size()) break;
            if (type == 4 && len >= 4) { curIf = d[i + 2]; curAlt = d[i + 3]; }
            if (type == 5 && len >= 6 && curIf == (int)ifNum && curAlt == (int)alt && d[i + 2] == ep)
            {
                int w = d[i + 4] | (d[i + 5] << 8);
                return (w & 0x7ff) * (1 + ((w >> 11) & 3));
            }
            i += len;
        }
        return 0;
    }

    bool open(const Found& f, const Device* dev, std::string& err)
    {
        char node[64];
        std::snprintf(node, sizeof(node), "/dev/bus/usb/%03d/%03d", f.bus, f.dev);
        fd_ = ::open(node, O_RDWR | O_CLOEXEC);
        if (fd_ < 0) { err = std::string("open: ") + std::strerror(errno); return false; }

        std::vector<uint8_t> desc(8192);
        ssize_t n = ::read(fd_, desc.data(), desc.size());
        desc.resize(n > 0 ? (size_t)n : 0);

        inFrame_ = frameBytes(dev->outputs, dev->numOutputs);
        outFrame_ = frameBytes(dev->inputs, dev->numInputs);
        inBlk_ = kBlockHeader + kFramesPerBlock * inFrame_;
        outBlk_ = kBlockHeader + kFramesPerBlock * outFrame_;
        int pin = packetSize(desc, kIfIn, kAlt, kEpIn), pout = packetSize(desc, kIfOut, kAlt, kEpOut);
        if ((pin && pin != (int)inBlk_) || (pout && pout != (int)outBlk_))
        {
            err = "unexpected block size " + std::to_string(pin) + "/" + std::to_string(pout);
            return false;
        }

        if (f.config != 1)
        {
            unsigned cfg = 1;
            if (ioctl(fd_, USBDEVFS_SETCONFIGURATION, &cfg) < 0) { err = std::string("set config: ") + std::strerror(errno); return false; }
        }
        for (unsigned i : {kIfIn, kIfOut})
        {
            if (ioctl(fd_, USBDEVFS_CLAIMINTERFACE, &i) < 0) { err = "claim if" + std::to_string(i) + ": " + std::strerror(errno); return false; }
            claimed_[i] = true;
            usbdevfs_setinterface si{i, kAlt};
            if (ioctl(fd_, USBDEVFS_SETINTERFACE, &si) < 0) { err = "alt setting: " + std::string(std::strerror(errno)); return false; }
        }
        for (unsigned ep : {(unsigned)kEpIn, (unsigned)kEpOut})
            if (ioctl(fd_, USBDEVFS_CLEAR_HALT, &ep) < 0) { err = "clear halt: " + std::string(std::strerror(errno)); return false; }

        // Read the Overbridge name (vendor requests 1 and 2, as the official client does), then pause:
        // sending the next packet immediately can upset some devices.
        uint8_t name[33] = {};
        for (uint8_t req : {1, 2})
        {
            usbdevfs_ctrltransfer c{};
            c.bRequestType = 0xC0;   // device-to-host | vendor | device
            c.bRequest = req;
            c.wLength = 32;
            c.timeout = 1000;
            c.data = name;
            int r = ioctl(fd_, USBDEVFS_CONTROL, &c);
            if (req == 1 && r > 0)
            {
                std::lock_guard<std::mutex> l(m_);
                devName_ = std::string(dev->name) + " (" + std::string((const char*)name, strnlen((const char*)name, 32)) + ")";
            }
        }
        usleep(100000);

        inBuf_.assign(inBlk_ * kBlocksPerTransfer, 0);
        outBuf_.assign(outBlk_ * kBlocksPerTransfer, 0);
        for (int b = 0; b < kBlocksPerTransfer; ++b) { outBuf_[b * outBlk_] = 0x07; outBuf_[b * outBlk_ + 1] = 0xff; }
        return true;
    }

    void close()
    {
        if (fd_ < 0) return;
        for (unsigned i : {kIfIn, kIfOut})
            if (claimed_[i]) { ioctl(fd_, USBDEVFS_RELEASEINTERFACE, &i); claimed_[i] = false; }
        ::close(fd_);
        fd_ = -1;
    }

    static inline void encodeSample(uint8_t* s, float f, int size, bool v21)
    {
        if (f > 1.0f) f = 1.0f;
        if (f < -1.0f) f = -1.0f;
        int32_t ov = (int32_t)(f * 2147483647.0f);
        if (v21 && size == 4) ov >>= 8;
        ov = (int32_t)__builtin_bswap32((uint32_t)ov);
        std::memcpy(s, &ov, (size_t)size);
    }

    // Builds the next host->device transfer: frame counters, plus the mix of all send slots.
    void fillOut(const Device* dev)
    {
        const int nFrames = kFramesPerBlock * kBlocksPerTransfer;   // 168
        const int nIn = dev->numInputs;
        std::fill(mix_.begin(), mix_.end(), 0.0f);
        bool any = false;
        for (int k = 0; k < kMaxSlots; ++k)
        {
            Slot& sl = slots_[k];
            if (sl.kind.load(std::memory_order_acquire) != Slot::kSend) { sendActive_[k] = false; continue; }
            uint32_t ep = sl.epoch.load(std::memory_order_acquire);
            if (!sendActive_[k] || ep != sendEpoch_[k])
            {
                sendRd_[k].prepare(sl.hostRate.load(), 48000.0, sl.hostBlock.load());
                sendEpoch_[k] = ep;
                sendActive_[k] = true;
            }
            const int d = sl.route.load(std::memory_order_relaxed);
            if (d < 0 || d >= (int)destsLocal_.size()) continue;
            if (!sendRd_[k].read(sl.ring, tmpL_.data(), tmpR_.data(), nFrames)) { sl.sendDropouts.store(sendRd_[k].dropouts()); continue; }
            sl.sendDropouts.store(sendRd_[k].dropouts(), std::memory_order_relaxed);
            sl.sendLatencyMs.store(sendRd_[k].latencyMs(), std::memory_order_relaxed);
            const int tl = destsLocal_[(size_t)d].left, tr = destsLocal_[(size_t)d].right;
            for (int f = 0; f < nFrames; ++f)
            {
                if (tl == tr) mix_[(size_t)(f * nIn + tl)] += 0.5f * (tmpL_[(size_t)f] + tmpR_[(size_t)f]);
                else
                {
                    mix_[(size_t)(f * nIn + tl)] += tmpL_[(size_t)f];
                    mix_[(size_t)(f * nIn + tr)] += tmpR_[(size_t)f];
                }
            }
            any = true;
        }
        for (int b = 0; b < kBlocksPerTransfer; ++b)
        {
            uint8_t* blk = &outBuf_[b * outBlk_];
            blk[2] = (uint8_t)(outCounter_ >> 8);
            blk[3] = (uint8_t)outCounter_;
            outCounter_ = (uint16_t)(outCounter_ + kFramesPerBlock);
            if (!any && !outDirty_) continue;    // all zero already
            uint8_t* p = blk + kBlockHeader;
            for (int f = 0; f < kFramesPerBlock; ++f)
                for (int t = 0; t < nIn; ++t)
                {
                    encodeSample(p, mix_[(size_t)((b * kFramesPerBlock + f) * nIn + t)], dev->inputs[t].size, dev->v21);
                    p += dev->inputs[t].size;
                }
        }
        outDirty_ = any;
    }

    // Decodes one device->host transfer into the ring of every receive slot (each with its own source).
    void decodeIn(const Device* dev, int actual)
    {
        const int blocks = actual / (int)inBlk_;
        for (int k = 0; k < kMaxSlots; ++k)
        {
            Slot& sl = slots_[k];
            if (sl.kind.load(std::memory_order_acquire) != Slot::kReceive) continue;
            const int si = sl.route.load(std::memory_order_relaxed);
            int offL = -1, offR = -1, szL = 0, szR = 0;
            if (si >= 0 && si < (int)sourcesLocal_.size())
            {
                const int tl = sourcesLocal_[(size_t)si].left, tr = sourcesLocal_[(size_t)si].right;
                offL = trackOffset_[(size_t)tl]; szL = dev->outputs[tl].size;
                offR = trackOffset_[(size_t)tr]; szR = dev->outputs[tr].size;
            }
            for (int b = 0; b < blocks; ++b)
            {
                const uint8_t* data = &inBuf_[b * inBlk_ + kBlockHeader];
                for (int f = 0; f < kFramesPerBlock; ++f)
                {
                    const uint8_t* fr = data + f * inFrame_;
                    sl.ring.push(offL >= 0 ? decodeSample(fr + offL, szL, dev->v21) : 0.0f,
                                 offR >= 0 ? decodeSample(fr + offR, szR, dev->v21) : 0.0f);
                }
            }
            sl.ring.publish();
        }
        framesIn_ += (uint64_t)blocks * kFramesPerBlock;
    }

    void stream(const Device* dev)
    {
        sourcesLocal_ = sourcesFor(dev);
        destsLocal_ = destinationsFor(dev);
        trackOffset_.assign((size_t)dev->numOutputs, 0);
        for (int t = 1; t < dev->numOutputs; ++t) trackOffset_[(size_t)t] = trackOffset_[(size_t)t - 1] + dev->outputs[t - 1].size;
        mix_.assign((size_t)(kFramesPerBlock * kBlocksPerTransfer * dev->numInputs), 0.0f);
        tmpL_.assign(kFramesPerBlock * kBlocksPerTransfer, 0.0f);
        tmpR_.assign(kFramesPerBlock * kBlocksPerTransfer, 0.0f);
        for (bool& a : sendActive_) a = false;
        outDirty_ = true;
        usbdevfs_urb in{}, out{};
        in.type = USBDEVFS_URB_TYPE_INTERRUPT; in.endpoint = kEpIn;
        in.buffer = inBuf_.data(); in.buffer_length = (int)inBuf_.size(); in.usercontext = &in;
        out.type = USBDEVFS_URB_TYPE_INTERRUPT; out.endpoint = kEpOut;
        out.buffer = outBuf_.data(); out.buffer_length = (int)outBuf_.size(); out.usercontext = &out;
        fillOut(dev);
        bool inBusy = ioctl(fd_, USBDEVFS_SUBMITURB, &in) == 0;
        bool outBusy = ioctl(fd_, USBDEVFS_SUBMITURB, &out) == 0;
        if (!inBusy || !outBusy)
        {
            setState(kError, std::string("submit: ") + std::strerror(errno));
            discardAndReap(in, out, inBusy, outBusy);
            return;
        }
        setState(kStreaming, "streaming");
        auto t0 = std::chrono::steady_clock::now();
        framesIn_ = 0;
        bool gone = false;

        while (!stop_.load() && !gone)
        {
            pollfd p{fd_, POLLOUT, 0};
            int pr = poll(&p, 1, 200);
            if (pr < 0 && errno != EINTR) { gone = true; break; }
            if (p.revents & (POLLERR | POLLHUP)) { gone = true; break; }
            for (;;)
            {
                void* done = nullptr;
                if (ioctl(fd_, USBDEVFS_REAPURBNDELAY, &done) < 0)
                {
                    if (errno == EAGAIN) break;
                    gone = true;
                    break;
                }
                usbdevfs_urb* u = (usbdevfs_urb*)done;
                if (u->status == -ENODEV || u->status == -ESHUTDOWN) { gone = true; }
                else if (u->status != 0) ++usbErrors_;
                if (u == &in)
                {
                    inBusy = false;
                    if (u->status == 0) decodeIn(dev, u->actual_length);
                    if (!gone && !stop_.load()) inBusy = ioctl(fd_, USBDEVFS_SUBMITURB, &in) == 0;
                    if (!inBusy && !stop_.load() && !gone) gone = true;
                }
                else if (u == &out)
                {
                    outBusy = false;
                    fillOut(dev);
                    if (!gone && !stop_.load()) outBusy = ioctl(fd_, USBDEVFS_SUBMITURB, &out) == 0;
                    if (!outBusy && !stop_.load() && !gone) gone = true;
                }
            }
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - t0).count();
            if (dt >= 2.0)
            {
                rateHz_.store(framesIn_ / dt);
                framesIn_ = 0;
                t0 = now;
            }
        }
        discardAndReap(in, out, inBusy, outBusy);
        rateHz_.store(0.0);
    }

    void discardAndReap(usbdevfs_urb& in, usbdevfs_urb& out, bool inBusy, bool outBusy)
    {
        if (inBusy) ioctl(fd_, USBDEVFS_DISCARDURB, &in);
        if (outBusy) ioctl(fd_, USBDEVFS_DISCARDURB, &out);
        for (int i = 0; i < 20 && (inBusy || outBusy); ++i)
        {
            void* done = nullptr;
            if (ioctl(fd_, USBDEVFS_REAPURBNDELAY, &done) == 0)
            {
                if (done == &in) inBusy = false;
                if (done == &out) outBusy = false;
                continue;
            }
            if (errno != EAGAIN) break;   // device gone: closing the fd cancels whatever is left
            pollfd p{fd_, POLLOUT, 0};
            poll(&p, 1, 25);
        }
    }

    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<int> state_{kSearching};
    std::atomic<uint32_t> usbErrors_{0}, overruns_{0};
    std::atomic<double> rateHz_{0.0};
    std::atomic<bool> rt_{false};
    std::mutex m_;
    std::string devName_, msg_;
    Slot* slots_;
    std::vector<Source> sources_, dests_, sourcesLocal_, destsLocal_;
    std::vector<int> trackOffset_;
    std::vector<float> mix_, tmpL_, tmpR_;
    DriftReader sendRd_[kMaxSlots];
    uint32_t sendEpoch_[kMaxSlots] = {};
    bool sendActive_[kMaxSlots] = {};
    bool outDirty_ = true;
    int fd_ = -1;
    bool claimed_[3] = {false, false, false};
    size_t inFrame_ = 0, outFrame_ = 0, inBlk_ = 0, outBlk_ = 0;
    std::vector<uint8_t> inBuf_, outBuf_;
    uint16_t outCounter_ = 0;
    uint64_t framesIn_ = 0;
};

} // namespace ob
