// ob_shared.h - one Overbridge engine per MPC process, shared by every MPC Overwitch instance and by
// the MPC Overwitch Send effect (a separate plugin library). Original NOXED code, GPL-3.0-or-later.
//
// How the libraries find each other
//   The first instance creates a Shared object (engine + 16 slots) and publishes its address in a
//   small anchor file in /dev/shm (named after the process id), protected by a spin lock that lives in
//   that mapped page. Later instances, from either library, map the same page and take a reference.
//   The last one to leave stops the engine, frees it and removes the anchor.
//   Each library pins itself in memory (dlopen with RTLD_NODELETE), because the engine thread runs
//   code from the library that created it and must not be unloaded while another library uses it.
//   kAbi changes whenever the shared layout changes: mixed versions refuse to share and report it.
#pragma once

#include "ob_usb.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace ob {

constexpr uint32_t kAbi = 0x4f420004;   // "OB" v4 (0.3.1: send peak meter in Slot)

struct Shared
{
    uint32_t abi = kAbi;
    uint32_t size = sizeof(Shared);
    std::atomic<int> refs{0};
    Slot slots[kMaxSlots];
    Engine engine{slots};
};

struct Anchor
{
    std::atomic<uint32_t> lock;
    uint32_t abi;
    uint32_t size;
    uint32_t pad;
    std::atomic<uint64_t> ptr;
    uint64_t startTime;   // process start time: a stale anchor from an earlier process with the same pid is ignored
};

inline uint64_t processStartTime()
{
    FILE* f = std::fopen("/proc/self/stat", "r");
    if (!f) return 0;
    char buf[1024] = {};
    size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = 0;
    const char* p = std::strrchr(buf, ')');   // field 2 (comm) may contain spaces
    if (!p) return 0;
    int field = 2;
    for (; *p && field < 22; ++p) if (*p == ' ') ++field;
    return std::strtoull(p, nullptr, 10);
}

inline std::string anchorPath()
{
    char p[96];
    const char* dir = access("/dev/shm", W_OK) == 0 ? "/dev/shm" : "/tmp";
    std::snprintf(p, sizeof(p), "%s/noxed-overwitch-%d", dir, (int)getpid());
    return p;
}

class AnchorLock
{
public:
    AnchorLock()
    {
        path_ = anchorPath();
        fd_ = ::open(path_.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (fd_ < 0) return;
        if (ftruncate(fd_, 4096) != 0) { ::close(fd_); fd_ = -1; return; }
        void* m = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (m == MAP_FAILED) { ::close(fd_); fd_ = -1; return; }
        a_ = (Anchor*)m;
        uint32_t expect = 0;
        int spins = 0;
        while (!a_->lock.compare_exchange_weak(expect, 1, std::memory_order_acquire))
        {
            expect = 0;
            if (++spins > 100) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    ~AnchorLock()
    {
        if (a_)
        {
            a_->lock.store(0, std::memory_order_release);
            munmap(a_, 4096);
        }
        if (fd_ >= 0) ::close(fd_);
    }
    Anchor* get() { return a_; }
    const std::string& path() const { return path_; }

private:
    std::string path_;
    int fd_ = -1;
    Anchor* a_ = nullptr;
};

inline void pinThisLibrary()
{
    Dl_info info{};
    if (dladdr((void*)&pinThisLibrary, &info) && info.dli_fname)
        dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE);
}

// Returns the process-wide engine (creating and starting it if needed), or nullptr with a reason.
inline Shared* acquireShared(std::string& err)
{
    pinThisLibrary();
    AnchorLock lk;
    Anchor* a = lk.get();
    if (!a) { err = "no anchor (" + lk.path() + ")"; return nullptr; }
    const uint64_t st = processStartTime();
    if (a->startTime != st) { a->ptr.store(0); a->startTime = st; }
    Shared* s = (Shared*)(uintptr_t)a->ptr.load();
    if (!s)
    {
        s = new Shared();
        s->refs.store(1);
        a->abi = kAbi;
        a->size = sizeof(Shared);
        a->ptr.store((uint64_t)(uintptr_t)s);
        s->engine.start();
        return s;
    }
    if (a->abi != kAbi || a->size != sizeof(Shared) || s->abi != kAbi)
    {
        err = "version mismatch: update both plugins";
        return nullptr;
    }
    s->refs.fetch_add(1);
    return s;
}

inline void releaseShared(Shared* s)
{
    if (!s) return;
    AnchorLock lk;
    Anchor* a = lk.get();
    if (s->refs.fetch_sub(1) == 1)
    {
        s->engine.stop();
        delete s;
        if (a) a->ptr.store(0);
        unlink(lk.path().c_str());
    }
}

// Claims a free slot of the given kind; returns its index or -1 when all 16 are taken.
inline int claimSlot(Shared* s, int kind, int route)
{
    for (int i = 0; i < kMaxSlots; ++i)
    {
        int expect = Slot::kFree;
        // Mark busy first with a temporary value so the engine never sees a half-initialised slot.
        if (s->slots[i].kind.compare_exchange_strong(expect, -1))
        {
            s->slots[i].route.store(route);
            s->slots[i].sendDropouts.store(0);
            s->slots[i].epoch.fetch_add(1);
            s->slots[i].kind.store(kind, std::memory_order_release);
            return i;
        }
    }
    return -1;
}

inline void freeSlot(Shared* s, int i)
{
    if (s && i >= 0 && i < kMaxSlots) s->slots[i].kind.store(Slot::kFree, std::memory_order_release);
}

inline int slotsInUse(Shared* s, int kind)
{
    int n = 0;
    for (int i = 0; i < kMaxSlots; ++i) n += s->slots[i].kind.load() == kind;
    return n;
}

} // namespace ob
