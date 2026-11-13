// Two separately loaded plugin libraries (RTLD_LOCAL, like the MPC loads plugins) must find and share
// one engine. Build: see tests/run_tests.sh.
#ifdef LIB
#include "ob_shared.h"
extern "C" __attribute__((visibility("default"))) void* acq() { std::string e; return ob::acquireShared(e); }
extern "C" __attribute__((visibility("default"))) void rel(void* p) { ob::releaseShared((ob::Shared*)p); }
extern "C" __attribute__((visibility("default"))) int claim(void* p, int k) { return ob::claimSlot((ob::Shared*)p, k, 0); }
#else
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <thread>
#include <chrono>
int main(int, char** argv)
{
    setenv("OVERWITCH_SYSFS", "/nonexistent", 1);
    void* a = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    void* b = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!a || !b) { std::printf("FAIL dlopen: %s\n", dlerror()); return 1; }
    auto acqA = (void* (*)())dlsym(a, "acq"), acqB = (void* (*)())dlsym(b, "acq");
    auto relA = (void (*)(void*))dlsym(a, "rel"), relB = (void (*)(void*))dlsym(b, "rel");
    auto clA = (int (*)(void*, int))dlsym(a, "claim"), clB = (int (*)(void*, int))dlsym(b, "claim");
    void* pa = acqA();
    void* pb = acqB();
    int sa = clA(pa, 1), sb = clB(pb, 2);
    bool ok = pa && pa == pb && sa == 0 && sb == 1;
    std::printf("%s two libraries share one engine (%p / %p, slots %d %d)\n", ok ? "PASS" : "FAIL", pa, pb, sa, sb);
    relA(pa);                       // the creating library leaves first ...
    dlclose(a);                     // ... and the host unloads it: the engine thread must survive (RTLD_NODELETE)
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    relB(pb);                       // last user stops the engine created by library a
    std::printf("PASS engine survived unloading its creator and stopped cleanly\n");
    return ok ? 0 : 1;
}
#endif
