// ob_midi.h - MIDI forwarding for MPC Overwitch and MPC USB Bridge: notes, controllers, pitch bend,
// aftertouch and program changes that the MPC track sends to the plugin go straight to the USB device's
// own MIDI port, re-channelled to the channel chosen in the plugin. Original NOXED code, GPL-3.0-or-later.
//
// The MPC host discards MIDI that a plugin sends back to it, so this does not use the host: the plugin
// writes to the device's ALSA MIDI port itself.
//   1. /dev/snd/midiC<card>D0, opened write-only with O_APPEND (lets several writers share the port)
//   2. if that is busy (the MPC holds the port), the ALSA sequencer through the MPC's own libasound:
//      a client port of ours connected to the device's kernel sequencer port.
// Threads: the audio thread only pushes 3-byte messages into a lock-free queue (no system calls);
// a writer thread opens the port, drains the queue every millisecond and reconnects after unplugging.
// Timing: messages leave at most one audio block (+1 ms) after their position in the block.
#pragma once

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

namespace obmidi {

// Lock-free single-producer / single-consumer queue of short MIDI messages.
class Queue
{
public:
    static constexpr uint32_t kSize = 1024;   // power of two
    bool push(uint8_t a, uint8_t b, uint8_t c, uint8_t n)
    {
        const uint32_t w = w_.load(std::memory_order_relaxed);
        if (w - r_.load(std::memory_order_acquire) >= kSize) return false;
        uint8_t* m = buf_[w & (kSize - 1)];
        m[0] = a; m[1] = b; m[2] = c; m[3] = n;
        w_.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(uint8_t out[4])
    {
        const uint32_t r = r_.load(std::memory_order_relaxed);
        if (r == w_.load(std::memory_order_acquire)) return false;
        std::memcpy(out, buf_[r & (kSize - 1)], 4);
        r_.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    uint8_t buf_[kSize][4] = {};
    alignas(64) std::atomic<uint32_t> w_{0};
    alignas(64) std::atomic<uint32_t> r_{0};
};

// ALSA sequencer fallback (only the few entry points needed; libasound is already loaded in the MPC).
struct Seq
{
    void* seq = nullptr;
    void* enc = nullptr;
    int port = -1;
    int (*open)(void**, const char*, int, int) = nullptr;
    int (*close)(void*) = nullptr;
    int (*setName)(void*, const char*) = nullptr;
    int (*createPort)(void*, const char*, unsigned, unsigned) = nullptr;
    int (*connectTo)(void*, int, int, int) = nullptr;
    int (*outputDirect)(void*, void*) = nullptr;
    int (*encNew)(size_t, void**) = nullptr;
    void (*encFree)(void*) = nullptr;
    long (*encode)(void*, const unsigned char*, long, void*) = nullptr;
    void (*encReset)(void*) = nullptr;

    bool load()
    {
        if (open) return true;
        void* h = dlopen("libasound.so.2", RTLD_NOW | RTLD_NOLOAD);
        if (!h) h = dlopen("libasound.so.2", RTLD_NOW);
        if (!h) return false;
        bool all = true;
        auto get = [&](auto& fn, const char* name) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(dlsym(h, name));
            if (!fn) all = false;
        };
        get(open, "snd_seq_open"); get(close, "snd_seq_close"); get(setName, "snd_seq_set_client_name");
        get(createPort, "snd_seq_create_simple_port"); get(connectTo, "snd_seq_connect_to");
        get(outputDirect, "snd_seq_event_output_direct"); get(encNew, "snd_midi_event_new");
        get(encFree, "snd_midi_event_free"); get(encode, "snd_midi_event_encode"); get(encReset, "snd_midi_event_reset_encode");
        if (!all) open = nullptr;
        return all;
    }

    // Kernel sequencer client of an ALSA card, from /proc/asound/seq/clients ("Client  24 : "S-1" [Kernel]"
    // followed by "  Port   0 : "S-1 MIDI 1" ..."); matched by the card's name in /proc/asound/cards.
    static int clientForCard(int card, const std::string& cardName)
    {
        FILE* f = std::fopen("/proc/asound/seq/clients", "r");
        if (!f) return -1;
        char line[256];
        int found = -1;
        while (std::fgets(line, sizeof(line), f))
        {
            int c = -1;
            char name[128] = {};
            if (std::sscanf(line, "Client %d : \"%127[^\"]\" [Kernel", &c, name) == 2 && std::strstr(line, "Kernel"))
            {
                if (!cardName.empty() && cardName == name) { found = c; break; }
                if (c == 16 + 4 * card && found < 0) found = c;   // usual numbering: 16 + 4 x card
            }
        }
        std::fclose(f);
        return found;
    }

    bool connect(int client, std::string& err)
    {
        if (!load()) { err = "no ALSA sequencer"; return false; }
        int r = open(&seq, "default", 1 /* output */, 0);
        if (r < 0 || !seq) { err = "seq open " + std::to_string(r); seq = nullptr; return false; }
        setName(seq, "NOXED MIDI");
        port = createPort(seq, "out", (1u << 0) | (1u << 5) /* READ | SUBS_READ */, (1u << 1) | (1u << 20));
        if (port < 0 || connectTo(seq, port, client, 0) < 0 || encNew(256, &enc) < 0)
        {
            err = "seq connect to client " + std::to_string(client) + " failed";
            disconnect();
            return false;
        }
        return true;
    }

    bool send(const uint8_t* b, int n)
    {
        alignas(8) unsigned char ev[64] = {};
        encReset(enc);
        if (encode(enc, b, n, ev) != n || ev[0] == 0) return true;   // incomplete: drop
        ev[3] = 253;              // queue = SND_SEQ_QUEUE_DIRECT
        ev[13] = (unsigned char)port;
        ev[14] = 254;             // dest = SND_SEQ_ADDRESS_SUBSCRIBERS
        ev[15] = 253;             // dest port = SND_SEQ_ADDRESS_UNKNOWN
        return outputDirect(seq, ev) >= 0;
    }

    void disconnect()
    {
        if (enc) { encFree(enc); enc = nullptr; }
        if (seq) { close(seq); seq = nullptr; }
        port = -1;
    }
};

// One per plugin instance.
class Forwarder
{
public:
    // findCard runs on the writer thread: returns the ALSA card of the device, or -1 when none.
    explicit Forwarder(std::function<int()> findCard) : findCard_(std::move(findCard)) {}
    ~Forwarder() { stop(); }

    void start()
    {
        if (thread_.joinable()) return;
        stop_.store(false);
        thread_ = std::thread([this] { run(); });
    }
    // Sends note-offs for held notes, waits until they are written (up to 100 ms), stops the thread.
    void stop()
    {
        if (!thread_.joinable()) return;
        releaseHeld();
        for (int i = 0; i < 100 && pending_.load() != 0 && state_.load() == kOpen; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        stop_.store(true);
        thread_.join();
    }

    // Audio thread: outCh -1 = off, 0..15. Forwards every channel message re-channelled to outCh.
    void process(const uint8_t* d, int size, int outCh)
    {
        if (outCh != ch_) { releaseHeld(); ch_ = outCh; }
        if (ch_ < 0 || size < 1) return;
        const uint8_t st = d[0] & 0xF0;
        if (st < 0x80 || st == 0xF0) return;
        const int n = (st == 0xC0 || st == 0xD0) ? 2 : 3;
        const uint8_t a = (uint8_t)(st | ch_), b = size > 1 ? d[1] & 0x7F : 0, c = size > 2 ? d[2] & 0x7F : 0;
        if (st == 0x90 && c) held_[b] = 1;
        else if (st == 0x80 || st == 0x90) held_[b] = 0;
        else if (st == 0xB0 && (b == 123 || b == 120)) std::memset(held_, 0, sizeof(held_));
        send(a, b, c, (uint8_t)n);
    }
    // Audio thread: on transport stop / reset.
    void releaseHeld()
    {
        if (ch_ < 0) return;
        for (int k = 0; k < 128; ++k)
            if (held_[k]) { held_[k] = 0; send((uint8_t)(0x80 | ch_), (uint8_t)k, 0, 3); }
    }

    enum State { kOff, kSearching, kOpen, kError };
    // For the status rows (any thread).
    std::string status(int outCh)
    {
        if (outCh < 0) return "MIDI off";
        std::lock_guard<std::mutex> l(m_);
        switch (state_.load())
        {
            case kOpen:  return "MIDI ch " + std::to_string(outCh + 1) + " -> " + path_ + (dropped_.load() ? " (" + std::to_string(dropped_.load()) + " lost)" : "");
            case kError: return "MIDI: " + err_;
            default:     return "MIDI: no MIDI port found";
        }
    }
    uint32_t sent() const { return sent_.load(); }

private:
    void send(uint8_t a, uint8_t b, uint8_t c, uint8_t n)
    {
        if (queue_.push(a, b, c, n)) pending_.fetch_add(1, std::memory_order_relaxed);
        else dropped_.fetch_add(1, std::memory_order_relaxed);
    }
    void setState(int s, const std::string& path, const std::string& err)
    {
        std::lock_guard<std::mutex> l(m_);
        path_ = path; err_ = err;
        state_.store(s);
    }

    void run()
    {
        int fd = -1, card = -2;
        bool useSeq = false;
        Seq seq;
        auto closeAll = [&] { if (fd >= 0) { ::close(fd); fd = -1; } if (useSeq) { seq.disconnect(); useSeq = false; } };
        auto lastTry = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        uint8_t m[4];
        while (!stop_.load())
        {
            const bool connected = fd >= 0 || useSeq;
            const auto now = std::chrono::steady_clock::now();
            if (!connected && now - lastTry > std::chrono::seconds(2))
            {
                lastTry = now;
                const char* forced = std::getenv("OB_MIDI_DEV");   // desktop tests: a file or FIFO
                card = forced ? 0 : findCard_();
                if (forced)
                {
                    fd = ::open(forced, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
                    if (fd >= 0) setState(kOpen, forced, ""); else setState(kError, forced, std::strerror(errno));
                }
                else if (card < 0) setState(kSearching, "", "");
                else
                {
                    char path[64];
                    std::snprintf(path, sizeof(path), "/dev/snd/midiC%dD0", card);
                    fd = ::open(path, O_WRONLY | O_NONBLOCK | O_APPEND | O_CLOEXEC);
                    if (fd >= 0) setState(kOpen, std::string("hw:") + std::to_string(card) + ",0", "");
                    else
                    {
                        const int e = errno;
                        std::string err;
                        if (e == EBUSY)
                        {
                            char p[64];
                            std::snprintf(p, sizeof(p), "/proc/asound/card%d/id", card);
                            std::string id;
                            if (FILE* f = std::fopen(p, "r")) { char b[64] = {}; if (std::fgets(b, sizeof(b), f)) id = b; std::fclose(f); }
                            while (!id.empty() && (id.back() == '\n' || id.back() == ' ')) id.pop_back();
                            const int client = Seq::clientForCard(card, cardName(card));
                            if (client >= 0 && seq.connect(client, err))
                            {
                                useSeq = true;
                                setState(kOpen, "seq " + std::to_string(client) + " (" + id + ")", "");
                            }
                            else setState(kError, "", "port busy (" + (err.empty() ? std::string("no seq client") : err) + ")");
                        }
                        else if (e == ENOENT) setState(kError, "", "device has no MIDI port");
                        else setState(kError, "", std::string(path) + ": " + std::strerror(e));
                    }
                }
            }
            bool wrote = false;
            while ((fd >= 0 || useSeq) && queue_.pop(m))
            {
                pending_.fetch_sub(1, std::memory_order_relaxed);
                bool ok;
                if (useSeq) ok = seq.send(m, m[3]);
                else
                {
                    ssize_t w = ::write(fd, m, m[3]);
                    for (int t = 0; w < 0 && errno == EAGAIN && t < 20; ++t)
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(500));
                        w = ::write(fd, m, m[3]);
                    }
                    ok = w == (ssize_t)m[3];
                }
                if (!ok) { dropped_.fetch_add(1); closeAll(); setState(kSearching, "", ""); break; }   // unplugged
                sent_.fetch_add(1, std::memory_order_relaxed);
                wrote = true;
            }
            if (fd < 0 && !useSeq)   // nowhere to send: forget old messages instead of a burst later
                while (queue_.pop(m)) pending_.fetch_sub(1, std::memory_order_relaxed);
            if (!wrote) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        closeAll();
    }

    static std::string cardName(int card)
    {
        // /proc/asound/cards: " 2 [S1             ]: USB-Audio - S-1"
        FILE* f = std::fopen("/proc/asound/cards", "r");
        if (!f) return "";
        char line[256];
        std::string out;
        while (std::fgets(line, sizeof(line), f))
        {
            int c = -1;
            const char* br = std::strstr(line, "]: ");
            const char* dash = br ? std::strstr(br, " - ") : nullptr;
            if (std::sscanf(line, " %d", &c) == 1 && c == card && dash)
            {
                out = dash + 3;
                while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
                break;
            }
        }
        std::fclose(f);
        return out;
    }

    std::function<int()> findCard_;
    Queue queue_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<int> state_{kSearching};
    std::atomic<int> pending_{0};
    std::atomic<uint32_t> dropped_{0}, sent_{0};
    std::mutex m_;
    std::string path_, err_;
    int ch_ = -1;               // audio thread only
    uint8_t held_[128] = {};    // audio thread only
};

// ALSA card of the first USB device whose usbid starts with vidPrefix ("1935" = Elektron) and that has
// a MIDI port; -1 if none.
inline int findUsbMidiCard(const char* vidPrefix)
{
    for (int c = 0; c < 16; ++c)
    {
        char p[64];
        std::snprintf(p, sizeof(p), "/proc/asound/card%d/usbid", c);
        FILE* f = std::fopen(p, "r");
        if (!f) continue;
        char id[32] = {};
        bool ok = std::fgets(id, sizeof(id), f) != nullptr;
        std::fclose(f);
        if (!ok || std::strncmp(id, vidPrefix, std::strlen(vidPrefix)) != 0) continue;
        std::snprintf(p, sizeof(p), "/dev/snd/midiC%dD0", c);
        if (::access(p, F_OK) == 0) return c;
    }
    return -1;
}

} // namespace obmidi
