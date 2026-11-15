// ub_diag.h - USB / ALSA diagnostics of MPC USB Bridge (the 0.1 probe): what the plugin sees.
// Used for the REPORT lines and MPC-USB-Bridge-Report.txt. Only reads /sys and /proc/asound and
// opens/closes PCM devices while no stream is running. Original NOXED code, GPL-3.0-or-later.
#pragma once

#include "mpc_resources.h"
#include "../../MPC Overwitch/src/ob_diag.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace ubdiag {

constexpr unsigned kRolandVid = 0x0582;

struct Report
{
    std::vector<std::string> lines;   // short lines for the REPORT knob
    std::string full;                 // report file
    std::string status = "Scanning...", device = "-", audio = "-", access = "-", system = "-";
};

// USB_BRIDGE_ASOUND lets the desktop test point the scan at a fake /proc/asound; unset on the MPC.
inline std::string asound(const std::string& rest)
{
    const char* fake = std::getenv("USB_BRIDGE_ASOUND");
    return (fake ? std::string(fake) : std::string("/proc/asound")) + "/" + rest;
}

inline std::string readAll(const std::string& path)
{
    std::string s;
    if (!mpc::readTextFile(path, s)) return std::string();
    return s;
}

inline std::vector<std::string> splitLines(const std::string& s)
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

inline std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// Summarises /proc/asound/cardN/stream0: per direction the formats, channels and rates.
inline std::vector<std::string> streamSummary(const std::string& text, bool& has44k, bool& has48k)
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

inline std::string pcmAccess(int card, char dir)
{
    char node[64];
    std::snprintf(node, sizeof(node), "/dev/snd/pcmC%dD0%c", card, dir);
    int fd = ::open(node, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) { ::close(fd); return "open OK"; }
    return diag::errShort(errno);
}

inline std::string pcmState(int card, char dir)
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

inline std::string libasound()
{
    if (void* h = dlopen("libasound.so.2", RTLD_LAZY | RTLD_NOLOAD)) { dlclose(h); return "libasound loaded in MPC"; }
    if (void* h = dlopen("libasound.so.2", RTLD_LAZY)) { dlclose(h); return "libasound available"; }
    return "no libasound";
}

inline Report scan(int scanNo)
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


} // namespace ubdiag
