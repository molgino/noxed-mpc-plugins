// MPC USB Bridge - NOXED (stage 0.1: probe). Goal: bring class-compliant USB audio devices that the MPC
// cannot use as its interface (Roland AIRA Compact S-1, T-8, J-6, Elektron boxes in USB-audio mode, ...
// usually because they run at 48 kHz) into the MPC as a plugin track, with sample-rate conversion.
//
// This version makes NO sound. On the MPC screen and in MPC-USB-Bridge-Report.txt it shows:
//   - every USB device with its interfaces and the kernel driver bound to each (Roland = vendor 0582);
//     a Roland device without an audio interface is probably in AIRA LINK mode
//   - every ALSA sound card, its USB id, and the formats / channels / sample rates each USB card offers
//     (from /proc/asound/cardN/stream0)
//   - whether the plugin can open the card's capture and playback devices (/dev/snd/pcmCxD0c/p),
//     whether they are in use, and whether libasound is available in the MPC process
// Nothing is configured, streamed or changed: files are only read, PCM devices only opened and closed.
//
// Parameters: 0 Report (scrolls the report; param 0 is nudged by the host on load: harmless),
//             1..5 read-only rows Status, Device, Audio, Access, System.
// Licence: GPL-3.0-or-later.

#include "mpc_plugin.h"
#include "mpc_resources.h"

#include "../../MPC Overwitch/src/ob_diag.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace {

enum ParamIndex { kLine = 0, kStatus, kDevice, kAudio, kAccess, kSystem, kNumParams };
constexpr int kMaxLines = 96;
constexpr unsigned kRolandVid = 0x0582;

struct Report
{
    std::vector<std::string> lines;   // short lines for the REPORT knob
    std::string full;                 // report file
    std::string status = "Scanning...", device = "-", audio = "-", access = "-", system = "-";
};

// USB_BRIDGE_ASOUND lets the desktop test point the scan at a fake /proc/asound; unset on the MPC.
std::string asound(const std::string& rest)
{
    const char* fake = std::getenv("USB_BRIDGE_ASOUND");
    return (fake ? std::string(fake) : std::string("/proc/asound")) + "/" + rest;
}

std::string readAll(const std::string& path)
{
    std::string s;
    if (!mpc::readTextFile(path, s)) return std::string();
    return s;
}

std::vector<std::string> splitLines(const std::string& s)
{
    std::vector<std::string> out;
    size_t a = 0;
    while (a < s.size())
    {
        size_t b = s.find('\n', a);
        if (b == std::string::npos) b = s.size();
        out.push_back(s.substr(a, b - a));
        a = b + 1;
    }
    return out;
}

std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// Summarises /proc/asound/cardN/stream0: per direction the formats, channels and rates.
std::vector<std::string> streamSummary(const std::string& text, bool& has44k, bool& has48k)
{
    std::vector<std::string> out;
    std::string dir, fmt, ch, rates;
    auto flush = [&]() {
        if (!dir.empty() && !fmt.empty()) out.push_back(dir + ": " + fmt + " " + ch + "ch " + rates);
        fmt.clear(); ch.clear(); rates.clear();
    };
    for (const std::string& raw : splitLines(text))
    {
        std::string l = trim(raw);
        if (l == "Playback:" || l == "Capture:") { flush(); dir = l.substr(0, l.size() - 1); }
        else if (l.compare(0, 7, "Format:") == 0) { flush(); fmt = trim(l.substr(7)); }
        else if (l.compare(0, 9, "Channels:") == 0) ch = trim(l.substr(9));
        else if (l.compare(0, 6, "Rates:") == 0)
        {
            rates = trim(l.substr(6));
            if (rates.find("44100") != std::string::npos) has44k = true;
            if (rates.find("48000") != std::string::npos) has48k = true;
        }
    }
    flush();
    return out;
}

std::string pcmAccess(int card, char dir)
{
    char node[64];
    std::snprintf(node, sizeof(node), "/dev/snd/pcmC%dD0%c", card, dir);
    int fd = ::open(node, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) { ::close(fd); return "open OK"; }
    return diag::errShort(errno);
}

std::string pcmState(int card, char dir)
{
    char p[96];
    std::snprintf(p, sizeof(p), "card%d/pcm0%c/sub0/status", card, dir);
    std::string s = trim(readAll(asound(p)));
    if (s.empty()) return "?";
    if (s == "closed") return "closed";
    size_t st = s.find("state:");
    size_t pid = s.find("owner_pid");
    std::string out = st != std::string::npos ? trim(s.substr(st + 6, s.find('\n', st) - st - 6)) : "open";
    if (pid != std::string::npos) out += " pid" + trim(s.substr(pid + 11, s.find('\n', pid) - pid - 11));
    return out;
}

std::string libasound()
{
    if (void* h = dlopen("libasound.so.2", RTLD_LAZY | RTLD_NOLOAD)) { dlclose(h); return "libasound loaded in MPC"; }
    if (void* h = dlopen("libasound.so.2", RTLD_LAZY)) { dlclose(h); return "libasound available"; }
    return "no libasound";
}

Report scan(int scanNo)
{
    Report r;
    std::vector<std::string>& L = r.lines;
    std::string& F = r.full;
    auto both = [&](const std::string& s, const std::string& l) { L.push_back(diag::clip(s)); F += l + "\n"; };

    both(diag::fmt("USB Bridge scan #%d", scanNo), diag::fmt("NOXED MPC USB Bridge report, scan #%d", scanNo));
    utsname u{};
    uname(&u);
    r.system = diag::clip(diag::fmt("%s uid%u %s", u.machine, (unsigned)geteuid(), u.release));
    const std::string asl = libasound();
    both(asl, "ALSA library: " + asl);

    // USB devices.
    const char* fake = std::getenv("USB_BRIDGE_SYSFS");
    const std::string root = fake ? std::string(fake) + "/" : "/sys/bus/usb/devices/";
    std::vector<std::string> names = diag::listDir(root);
    struct Dev { std::string sys, product, maker; unsigned vid, pid; bool audio = false, vendorIf = false; std::string ifs; };
    std::vector<Dev> devs;
    for (const std::string& n : names)
    {
        if (n.find(':') != std::string::npos || n.compare(0, 3, "usb") == 0) continue;
        Dev d;
        d.sys = n;
        d.vid = diag::readHex(root + n + "/idVendor");
        d.pid = diag::readHex(root + n + "/idProduct");
        d.product = diag::readLine(root + n + "/product");
        d.maker = diag::readLine(root + n + "/manufacturer");
        for (const std::string& m : names)
        {
            if (m.compare(0, n.size() + 1, n + ":") != 0) continue;
            unsigned cls = diag::readHex(root + m + "/bInterfaceClass"), sub = diag::readHex(root + m + "/bInterfaceSubClass");
            std::string drv = diag::linkBase(root + m + "/driver");
            if (cls == 0x01 && sub == 0x02) d.audio = true;   // audio streaming interface
            if (cls == 0xff) d.vendorIf = true;
            d.ifs += diag::fmt(" %02x/%02x:%s", cls, sub, drv.empty() ? "-" : drv.c_str());
        }
        devs.push_back(d);
    }
    both(diag::fmt("USB devices: %d", (int)devs.size()), diag::fmt("\nUSB devices: %d", (int)devs.size()));
    const Dev* roland = nullptr;
    for (const Dev& d : devs)
    {
        both(diag::fmt("%04x:%04x %s", d.vid, d.pid, d.product.c_str()),
             diag::fmt("  %s %04x:%04x %s / %s  interfaces (class/sub:driver):%s", d.sys.c_str(), d.vid, d.pid,
                       d.maker.c_str(), d.product.c_str(), d.ifs.c_str()));
        if (d.vid == kRolandVid && !roland) roland = &d;
    }

    // ALSA cards.
    F += "\nALSA cards (/proc/asound/cards):\n" + readAll(asound("cards"));
    int target = -1;
    bool t44 = false, t48 = false;
    std::string targetName, targetAccess, shortAccess;
    for (int c = 0; c < 16; ++c)
    {
        char p[64];
        std::snprintf(p, sizeof(p), "card%d/id", c);
        std::string id = trim(readAll(asound(p)));
        if (id.empty()) continue;
        std::snprintf(p, sizeof(p), "card%d/usbid", c);
        std::string usbid = trim(readAll(asound(p)));
        both(diag::fmt("card%d %s %s", c, id.c_str(), usbid.empty() ? "(not USB)" : usbid.c_str()),
             diag::fmt("\ncard%d id=%s usbid=%s", c, id.c_str(), usbid.empty() ? "-" : usbid.c_str()));
        if (usbid.empty()) continue;
        std::snprintf(p, sizeof(p), "card%d/stream0", c);
        bool h44 = false, h48 = false;
        std::string stream = readAll(asound(p));
        F += stream;
        for (const std::string& s : streamSummary(stream, h44, h48)) L.push_back(diag::clip(" " + s));
        const std::string ca = pcmAccess(c, 'c'), pa = pcmAccess(c, 'p');
        std::string acc = "capture " + ca + " (" + pcmState(c, 'c') + "), playback " + pa + " (" + pcmState(c, 'p') + ")";
        both(" in " + ca + ", out " + pa, "  access: " + acc);
        bool isRoland = usbid.compare(0, 4, "0582") == 0;
        if (target < 0 || isRoland)
        {
            target = c; t44 = h44; t48 = h48; targetName = id; targetAccess = acc;
            shortAccess = "in " + ca + " / out " + pa;
        }
    }

    // Verdict.
    if (target < 0)
    {
        if (roland && !roland->audio)
        {
            r.status = "Roland without audio: AIRA LINK on?";
            r.device = diag::clip(roland->product.empty() ? "Roland device" : roland->product);
        }
        else
        {
            r.status = "No USB audio device found";
            r.device = roland ? diag::clip(roland->product) : "none";
        }
    }
    else
    {
        r.device = diag::clip(targetName);
        r.audio = diag::clip(std::string(t48 ? "48k" : "") + (t44 ? (t48 ? " + 44.1k" : "44.1k") : "") +
                             (!t44 && !t48 ? "no 44.1/48k" : "") + " offered");
        r.access = diag::clip(shortAccess);
        bool capOk = targetAccess.find("capture open OK") != std::string::npos;
        bool busy = targetAccess.find("EBUSY") != std::string::npos;
        if (capOk) r.status = "PROMISING: audio can be opened";
        else if (busy) r.status = "Busy: card in use";
        else r.status = "Seen, but cannot open audio";
    }
    if (roland && roland->vendorIf && !roland->audio)
        both("Roland: vendor if only (AIRA LINK?)", "Roland device shows only vendor-specific interfaces: AIRA LINK mode is probably ON.");
    both("=> " + r.status, "\nVerdict: " + r.status);
    return r;
}

} // namespace

class MPCUSBBridge : public mpc::Plugin
{
public:
    MPCUSBBridge() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::intParam("line", "Report", 1, kMaxLines, 1));
        const char* ids[] = {"status", "device", "audio", "access", "system"};
        const char* names[] = {"Status", "Device", "Audio", "Access", "System"};
        for (int i = 0; i < 5; ++i)
        {
            mpc::Param p = mpc::floatParam(ids[i], names[i], 0.0f, 1.0f, 0.0f);
            p.automatable = false;
            addParam(p);
        }
        rep_ = scan(++scans_);
        write();
        worker_ = std::thread([this] { run(); });
    }

    ~MPCUSBBridge() override
    {
        stop_.store(true);
        if (worker_.joinable()) worker_.join();
    }

    void process(mpc::ProcessContext&) override {}   // silent in this probe version

    bool formatValue(int index, float plain, std::string& out) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        switch (index)
        {
            case kLine:
            {
                int i = (int)std::lround(plain) - 1;
                out = diag::clip(i >= 0 && i < (int)rep_.lines.size() ? diag::fmt("%d: ", i + 1) + rep_.lines[(size_t)i]
                                                                      : diag::fmt("%d: (end of report)", i + 1));
                return true;
            }
            case kStatus: out = rep_.status; return true;
            case kDevice: out = rep_.device; return true;
            case kAudio:  out = rep_.audio;  return true;
            case kAccess: out = rep_.access; return true;
            case kSystem: out = rep_.system; return true;
            default: return false;
        }
    }

private:
    // Rescans every 2 s; redraws the page when a visible row changed (plugging in, AIRA LINK on/off).
    void run()
    {
        while (!stop_.load())
        {
            for (int i = 0; i < 20 && !stop_.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (stop_.load()) break;
            Report r = scan(++scans_);
            bool changed;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                changed = key(r) != key(rep_);
                rep_ = std::move(r);
            }
            write();
            if (changed) requestDisplayUpdate();
        }
    }

    static std::string key(const Report& r) { return r.status + "|" + r.device + "|" + r.audio + "|" + r.access; }

    void write()
    {
        std::string body;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            body = rep_.full;
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
        info.version      = 100;            // 0.1.0
        info.isInstrument = true;
        info.numInputs    = 0;
        info.numOutputs   = 2;
        info.tailSamples  = 0;
        return info;
    }

    static constexpr char kUid[5] = "NXbD";

    std::mutex mutex_;
    Report rep_;
    std::atomic<bool> stop_{false};
    std::thread worker_;
    std::string lastWritten_;
    int scans_ = 0;
};

// This macro creates the exported VSTPluginMain entry point. Keep it last.
MPC_PLUGIN_ENTRY(MPCUSBBridge)
