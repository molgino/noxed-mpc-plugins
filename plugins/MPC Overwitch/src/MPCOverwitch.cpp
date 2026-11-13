// MPC Overwitch - NOXED (stage 0.1: USB probe): can an MPC plugin reach an Elektron box over USB?
//
// Purpose: first step towards an Overbridge client plugin for the MPC (Digitone II etc.).
// It makes NO sound. It answers, on the MPC screen and in a report file:
//   1. Does the plugin see the Elektron device on the USB bus (Linux sysfs)?
//   2. Is the device in Overbridge mode (vendor-specific interface) or class-compliant audio?
//   3. Which kernel driver, if any, has bound each interface (e.g. the MPC's snd-usb-audio)?
//   4. Can the plugin open the device node /dev/bus/usb/BBB/DDD read/write (permissions)?
//   5. Optional (Claim Test switch, default off): can it claim the vendor interface and
//      release it straight away? Nothing is sent to the device; no data transfer happens.
//
// How it works
//   A background thread (never the audio thread) rescans every 2 seconds, so plugging
//   the box in or switching its USB mode shows up while the plugin is open. Results are
//   shown as the value text of read-only parameters, and the full report is written to
//   "MPC-Overwitch-Report.txt" in the plugin folder on the MPC drive (only when it changes).
//   When a visible row changes, the plugin asks the host to re-read all texts (requestDisplayUpdate).
//   The REPORT knob scrolls through the report one line at a time.
//
// Nothing here modifies the system: only reads from /sys and /proc, opens the device node
// and (optionally) claims/releases one interface with the standard usbfs ioctls.
// Licence: GPL-3.0-or-later (planned Overbridge client builds on Overwitch, GPL-3).

#include "mpc_plugin.h"
#include "mpc_resources.h"

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace {

enum ParamIndex
{
    kLine = 0,     // report line selector (param 0: the host nudges it on load, harmless)
    kClaimTest,    // off: never claim; on: try claim + immediate release of the vendor interface
    kStatus,       // read-only text: verdict
    kDevice,       // read-only text: Elektron device found
    kMode,         // read-only text: Overbridge or class-compliant
    kDriver,       // read-only text: kernel driver on the vendor interface
    kAccess,       // read-only text: open() result
    kClaim,        // read-only text: claim result
    kSystem,       // read-only text: uid / kernel
    kNumParams
};

constexpr int kMaxLines = 64;
constexpr unsigned kElektronVid = 0x1935;

// usbfs ioctls (linux/usbdevice_fs.h), defined here so no kernel headers are needed.
#ifndef USBDEVFS_CLAIMINTERFACE
#define USBDEVFS_CLAIMINTERFACE _IOR('U', 15, unsigned int)
#endif
#ifndef USBDEVFS_RELEASEINTERFACE
#define USBDEVFS_RELEASEINTERFACE _IOR('U', 16, unsigned int)
#endif

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

std::string readLine(const std::string& path)
{
    std::string s;
    if (!mpc::readTextFile(path, s)) return std::string();
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

unsigned readHex(const std::string& path)
{
    std::string s = readLine(path);
    return s.empty() ? 0u : (unsigned)std::strtoul(s.c_str(), nullptr, 16);
}

int readInt(const std::string& path)
{
    std::string s = readLine(path);
    return s.empty() ? 0 : std::atoi(s.c_str());
}

std::string linkBase(const std::string& path)
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

std::string fmt(const char* f, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

// Screen text is limited to 31 characters by the host.
std::string clip(const std::string& s) { return s.size() <= 31 ? s : s.substr(0, 30) + "~"; }

std::string errShort(int e)
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

std::string errName(int e)
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
Snapshot scan(bool claimTest, int scanNo)
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

} // namespace

class MPCOverwitch : public mpc::Plugin
{
public:
    MPCOverwitch() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::intParam("line", "Report", 1, kMaxLines, 1));
        addParam(mpc::boolParam("claim_test", "Claim Test", false));
        const char* ro[] = {"status", "device", "mode", "driver", "access", "claim", "system"};
        const char* names[] = {"Status", "Device", "USB Mode", "Driver", "Access", "Claim", "System"};
        for (int i = 0; i < 7; ++i)
        {
            mpc::Param p = mpc::floatParam(ro[i], names[i], 0.0f, 1.0f, 0.0f);
            p.automatable = false;
            addParam(p);
        }
        // First scan synchronously so the screen has text immediately; then the worker takes over.
        snap_ = scan(false, ++scans_);
        writeReport();
        worker_ = std::thread([this] { run(); });
    }

    ~MPCOverwitch() override
    {
        stop_.store(true);
        if (worker_.joinable()) worker_.join();
    }

    void process(mpc::ProcessContext&) override {}   // silent: outputs are already zeroed

    bool formatValue(int index, float plain, std::string& out) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        switch (index)
        {
            case kLine:
            {
                int i = (int)std::lround(plain) - 1;
                out = (i >= 0 && i < (int)snap_.lines.size()) ? fmt("%d: ", i + 1) + snap_.lines[(size_t)i]
                                                               : fmt("%d: (end of report)", i + 1);
                out = clip(out);
                return true;
            }
            case kStatus: out = snap_.status; return true;
            case kDevice: out = snap_.device; return true;
            case kMode:   out = snap_.mode;   return true;
            case kDriver: out = snap_.driver; return true;
            case kAccess: out = snap_.access; return true;
            case kClaim:  out = snap_.claim;  return true;
            case kSystem: out = snap_.system; return true;
            default: return false;
        }
    }

private:
    void run()
    {
        while (!stop_.load())
        {
            for (int i = 0; i < 20 && !stop_.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (stop_.load()) break;
            Snapshot s = scan(getBool(kClaimTest), ++scans_);
            bool changed;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                changed = visibleKey(s) != visibleKey(snap_);
                snap_ = std::move(s);
            }
            writeReport();
            // The MPC redraws a value text only when the value moves; these rows never move, so ask
            // the host to re-read all texts, but only when something on screen actually changed
            // (plugging in, USB mode, claim result). Called from this worker thread, never from audio.
            if (changed) requestDisplayUpdate();
        }
    }

    static std::string visibleKey(const Snapshot& s)
    {
        return s.status + "|" + s.device + "|" + s.mode + "|" + s.driver + "|" + s.access + "|" + s.claim + "|" + s.system;
    }

    // Writes the report next to the plugin, only when its content (apart from the scan number) changed.
    void writeReport()
    {
        std::string body;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            body = snap_.full;
        }
        size_t nl = body.find('\n');
        std::string key = nl == std::string::npos ? body : body.substr(nl);
        if (key == lastWritten_) return;
        lastWritten_ = key;
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
        info.version      = 101;            // 0.1.1
        info.isInstrument = true;
        info.numInputs    = 0;
        info.numOutputs   = 2;
        info.tailSamples  = 0;
        return info;
    }

    static constexpr char kUid[5] = "NMmv";

    std::mutex mutex_;
    Snapshot snap_;
    std::atomic<bool> stop_{false};
    std::atomic<int> scans_{0};
    std::thread worker_;
    std::string lastWritten_;
};

// This macro creates the exported VSTPluginMain entry point. Keep it last.
MPC_PLUGIN_ENTRY(MPCOverwitch)
