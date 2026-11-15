// ob_diag.h - USB diagnostics for MPC Overwitch (the 0.1 probe): what the plugin sees on the USB bus.
// Used for the REPORT lines and MPC-Overwitch-Report.txt while no stream is running.
// Reads /sys, opens the device node and, only on request, claims and releases the vendor interface.
// Original NOXED code, GPL-3.0-or-later.
#pragma once

#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "mpc_resources.h"

#ifndef USBDEVFS_CLAIMINTERFACE
#define USBDEVFS_CLAIMINTERFACE _IOR('U', 15, unsigned int)
#endif
#ifndef USBDEVFS_RELEASEINTERFACE
#define USBDEVFS_RELEASEINTERFACE _IOR('U', 16, unsigned int)
#endif

namespace diag {

constexpr unsigned kElektronVid = 0x1935;

struct Iface
{
    int number = -1;
    int alt = 0;
    unsigned cls = 0, sub = 0;
    int endpoints = 0;
    std::string driver;   // "" = none
};

struct UsbDev
{
    std::string sysName;    // e.g. "1-1.2"
    unsigned vid = 0, pid = 0;
    std::string product, manufacturer, speed;
    int bus = 0, devnum = 0;
    std::vector<Iface> ifaces;
};

inline std::string readLine(const std::string& path)
{
    std::string s;
    if (!mpc::readTextFile(path, s)) return std::string();
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

inline unsigned readHex(const std::string& path)
{
    std::string s = readLine(path);
    return s.empty() ? 0u : (unsigned)std::strtoul(s.c_str(), nullptr, 16);
}

inline int readInt(const std::string& path)
{
    std::string s = readLine(path);
    return s.empty() ? 0 : std::atoi(s.c_str());
}

inline std::string linkBase(const std::string& path)
{
    char buf[512];
    ssize_t n = readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (n <= 0) return std::string();
    buf[n] = 0;
    const char* slash = std::strrchr(buf, '/');
    return slash ? std::string(slash + 1) : std::string(buf);
}

std::vector<std::string> listDir(const std::string& path)
{
    std::vector<std::string> out;
    DIR* d = opendir(path.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d))
        if (e->d_name[0] != '.') out.push_back(e->d_name);
    closedir(d);
    return out;
}

inline std::string fmt(const char* f, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

// Screen text is limited to 31 characters by the host.
inline std::string clip(const std::string& s) { return s.size() <= 31 ? s : s.substr(0, 30) + "~"; }

inline std::string errShort(int e)
{
    switch (e)
    {
        case EACCES: return "EACCES";
        case EPERM:  return "EPERM";
        case ENOENT: return "ENOENT";
        case EBUSY:  return "EBUSY";
        case ENODEV: return "ENODEV";
        default:     return fmt("errno %d", e);
    }
}

inline std::string errName(int e)
{
    switch (e)
    {
        case EACCES: return "EACCES (no permission)";
        case EPERM:  return "EPERM (not permitted)";
        case ENOENT: return "ENOENT (no device node)";
        case EBUSY:  return "EBUSY (in use)";
        case ENODEV: return "ENODEV (unplugged)";
        default:     return fmt("errno %d", e);
    }
}

struct Snapshot
{
    std::vector<std::string> lines;   // short lines for the REPORT knob
    std::string full;                 // report file contents
    std::string status = "Scanning...", device = "-", mode = "-", driver = "-",
                access = "-", claim = "-", system = "-";
};

// ---------------------------------------------------------------------------
// The scan (runs on the worker thread only).
// ---------------------------------------------------------------------------
inline Snapshot scan(bool claimTest, int scanNo)
{
    Snapshot s;
    std::vector<std::string> L;
    std::string F;
    auto both = [&](const std::string& shortLine, const std::string& longLine) {
        L.push_back(clip(shortLine));
        F += longLine + "\n";
    };

    both(fmt("Overwitch scan #%d", scanNo), fmt("NOXED MPC Overwitch report, scan #%d", scanNo));

    // System facts.
    utsname u{};
    uname(&u);
    unsigned uid = (unsigned)getuid(), euid = (unsigned)geteuid();
    std::string comm = readLine("/proc/self/comm");
    both(fmt("uid %u euid %u", uid, euid), fmt("Process: %s, uid %u, euid %u", comm.c_str(), uid, euid));
    both(fmt("kernel %s", u.release), fmt("Kernel: %s %s %s", u.sysname, u.release, u.machine));
    s.system = clip(fmt("%s uid%u %s", u.machine, euid, u.release));

    struct stat st{};
    bool usbfs = stat("/dev/bus/usb", &st) == 0;
    bool sysfs = stat("/sys/bus/usb/devices", &st) == 0;
    both(fmt("usbfs %s sysfs %s", usbfs ? "yes" : "NO", sysfs ? "yes" : "NO"),
         fmt("/dev/bus/usb present: %s, /sys/bus/usb/devices present: %s", usbfs ? "yes" : "no", sysfs ? "yes" : "no"));

    // Enumerate devices and their interfaces.
    // OVERWITCH_SYSFS lets the desktop test point the scan at a fake tree; unset on the MPC.
    const char* fake = std::getenv("OVERWITCH_SYSFS");
    const std::string root = fake ? std::string(fake) + "/" : "/sys/bus/usb/devices/";
    std::vector<UsbDev> devs;
    std::vector<std::string> names = listDir(root);
    for (const std::string& n : names)
    {
        if (n.find(':') != std::string::npos || n.compare(0, 3, "usb") == 0) continue;
        UsbDev d;
        d.sysName = n;
        d.vid = readHex(root + n + "/idVendor");
        d.pid = readHex(root + n + "/idProduct");
        d.product = readLine(root + n + "/product");
        d.manufacturer = readLine(root + n + "/manufacturer");
        d.speed = readLine(root + n + "/speed");
        d.bus = readInt(root + n + "/busnum");
        d.devnum = readInt(root + n + "/devnum");
        for (const std::string& m : names)
        {
            if (m.compare(0, n.size() + 1, n + ":") != 0) continue;
            Iface f;
            std::string p = root + m + "/";
            f.number = (int)readHex(p + "bInterfaceNumber");
            f.alt = (int)readHex(p + "bAlternateSetting");
            f.cls = readHex(p + "bInterfaceClass");
            f.sub = readHex(p + "bInterfaceSubClass");
            f.endpoints = (int)readHex(p + "bNumEndpoints");
            f.driver = linkBase(p + "driver");
            d.ifaces.push_back(f);
        }
        devs.push_back(d);
    }
    both(fmt("USB devices: %d", (int)devs.size()), fmt("USB devices on the bus: %d", (int)devs.size()));
    F += "\nAll devices:\n";
    for (const UsbDev& d : devs)
        F += fmt("  %s  %04x:%04x  bus %03d dev %03d  %s Mb/s  %s / %s  (%d interfaces)\n", d.sysName.c_str(), d.vid, d.pid,
                 d.bus, d.devnum, d.speed.c_str(), d.manufacturer.c_str(), d.product.c_str(), (int)d.ifaces.size());

    const UsbDev* el = nullptr;
    for (const UsbDev& d : devs)
        if (d.vid == kElektronVid) { el = &d; break; }

    if (!el)
    {
        s.status = "No Elektron device found";
        s.device = "none (check cable/power)";
        both(s.status, "\nNo Elektron device (vendor 1935) found.");
        for (const UsbDev& d : devs)
            L.push_back(clip(fmt("%04x:%04x %s", d.vid, d.pid, d.product.c_str())));
        s.lines = L;
        s.full = F;
        return s;
    }

    s.device = clip(fmt("%s %04x", el->product.empty() ? "Elektron" : el->product.c_str(), el->pid));
    both(s.device, fmt("\nElektron device: %s (%04x:%04x), bus %03d dev %03d, %s Mb/s, sysfs %s",
                       el->product.c_str(), el->vid, el->pid, el->bus, el->devnum, el->speed.c_str(), el->sysName.c_str()));

    // Interfaces: vendor-specific (0xff) means Overbridge mode; class 1 = USB audio.
    const Iface* vendor = nullptr;
    bool audio = false;
    for (const Iface& f : el->ifaces)
    {
        both(fmt("if%d cls %02x ep%d %s", f.number, f.cls, f.endpoints, f.driver.empty() ? "free" : f.driver.c_str()),
             fmt("  interface %d alt %d class %02x/%02x endpoints %d driver %s", f.number, f.alt, f.cls, f.sub,
                 f.endpoints, f.driver.empty() ? "(none)" : f.driver.c_str()));
        if (f.cls == 0xff && f.endpoints > 0 && !vendor) vendor = &f;
        if (f.cls == 0x01) audio = true;
    }
    s.mode = vendor ? "Overbridge (vendor if)" : (audio ? "USB audio, not Overbridge" : "unknown");
    s.driver = vendor ? (vendor->driver.empty() ? "none - interface free" : clip("taken by " + vendor->driver)) : "-";

    // Device node access.
    std::string node = fmt("/dev/bus/usb/%03d/%03d", el->bus, el->devnum);
    int fd = open(node.c_str(), O_RDWR | O_CLOEXEC);
    int openErr = fd < 0 ? errno : 0;
    if (fd < 0)
    {
        int fdr = open(node.c_str(), O_RDONLY | O_CLOEXEC);
        bool ro = fdr >= 0;
        if (fdr >= 0) close(fdr);
        s.access = clip("no: " + errName(openErr) + (ro ? ", ro ok" : ""));
        both("open: " + errShort(openErr) + (ro ? " (ro ok)" : ""), "Open " + node + " read/write: " + errName(openErr) + (ro ? " (read-only works)" : ""));
    }
    else
    {
        unsigned char desc[4096];
        ssize_t n = read(fd, desc, sizeof(desc));
        s.access = clip(fmt("open rw OK, %d desc bytes", (int)n));
        both("open: rw OK", fmt("Open %s read/write: OK, %d descriptor bytes", node.c_str(), (int)n));
    }

    // Optional claim test.
    if (!claimTest)
        s.claim = "off (switch CLAIM TEST on)";
    else if (fd < 0)
        s.claim = "skipped, no access";
    else if (!vendor)
        s.claim = "skipped, no vendor if";
    else if (!vendor->driver.empty())
        s.claim = clip("skipped, " + vendor->driver + " owns it");
    else
    {
        unsigned ifn = (unsigned)vendor->number;
        if (ioctl(fd, USBDEVFS_CLAIMINTERFACE, &ifn) == 0)
        {
            ioctl(fd, USBDEVFS_RELEASEINTERFACE, &ifn);
            s.claim = fmt("if%u claim+release OK", ifn);
        }
        else
            s.claim = clip(fmt("if%u ", ifn) + errShort(errno));
    }
    both(claimTest ? "claim: " + s.claim : "claim: off", "Claim test: " + s.claim);
    if (fd >= 0) close(fd);

    // Verdict.
    if (!vendor)
        s.status = audio ? "Set Digitone USB: Overbridge" : "No Overbridge interface";
    else if (fd < 0)
        s.status = "Seen, but no USB access";
    else if (!vendor->driver.empty())
        s.status = "Seen, interface busy";
    else if (!claimTest)
        s.status = "Open OK - try CLAIM TEST";
    else if (s.claim.find("OK") != std::string::npos)
        s.status = "PROMISING: claim works";
    else
        s.status = "Open OK, claim failed";
    both("=> " + s.status, "\nVerdict: " + s.status);

    s.lines = L;
    s.full = F;
    return s;
}


} // namespace diag
