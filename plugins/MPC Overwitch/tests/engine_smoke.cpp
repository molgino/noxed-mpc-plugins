// Engine smoke test without hardware: state machine against a fake sysfs tree, source mapping,
// block sizes and sample decoding.  g++ -O2 -std=c++17 -pthread -Isrc tests/engine_smoke.cpp
#include "ob_shared.h"
#include <cassert>
#include <cstdio>

#include <cmath>

int main()
{
    int fails = 0;
    auto check = [&](bool c, const char* what) { std::printf("%s %s\n", c ? "PASS" : "FAIL", what); fails += !c; };

    const ob::Device* dn2 = ob::findDevice(0x0b34);
    check(dn2 && dn2->v21 && dn2->numOutputs == 42 && dn2->numInputs == 8, "Digitone II in table");
    size_t in = 0, out = 0;
    for (int i = 0; i < dn2->numOutputs; ++i) in += dn2->outputs[i].size;
    for (int i = 0; i < dn2->numInputs; ++i) out += dn2->inputs[i].size;
    check(32 + 7 * in == 1012 && 32 + 7 * out == 256, "DN2 block sizes 1012 / 256 bytes");
    auto src = ob::sourcesFor(dn2);
    check(src.size() == 21 && src[0].name == "Main" && src[1].name == "Track 1" && src[17].name == "Delay" &&
          src[20].name == "Input" && src[1].left == 2 && src[1].right == 3, "DN2 sources match the Source list");
    auto dt = ob::sourcesFor(ob::findDevice(0x0b2c));
    std::printf("     Digitakt sources:");
    for (auto& s : dt) std::printf(" [%s %d/%d]", s.name.c_str(), s.left, s.right);
    std::printf("\n");

    // 24-bit (v2.1, 3-byte) and padded 4-byte samples: 0x400000 = +0.5 full scale.
    uint8_t s3[3] = {0x40, 0x00, 0x00}, s4[4] = {0x00, 0x40, 0x00, 0x00}, n3[3] = {0xC0, 0x00, 0x00};
    float da = ob::decodeSample(s3, 3, true), db = ob::decodeSample(s4, 4, true), dc = ob::decodeSample(n3, 3, true);
    std::printf("     decode: 3-byte %.6f, 4-byte %.6f, negative %.6f\n", da, db, dc);
    check(std::fabs(da - 0.5f) < 1e-5f && std::fabs(db - 0.5f) < 1e-5f && std::fabs(dc + 0.5f) < 1e-5f, "24-bit sample decoding");

    auto dests = ob::destinationsFor(dn2);
    std::printf("     DN2 destinations:");
    for (auto& d : dests) std::printf(" [%s %d/%d]", d.name.c_str(), d.left, d.right);
    std::printf("\n");
    check(dests.size() == 4 && dests[0].name == "Main Input" && dests[3].name == "Chorus Input" && dests[3].left == 6,
          "DN2 send destinations Main/Delay/Reverb/Chorus Input");

    setenv("OVERWITCH_SYSFS", "/tmp/fakesys", 1);
    std::string err;
    ob::Shared* a = ob::acquireShared(err);
    ob::Shared* b = ob::acquireShared(err);
    check(a && a == b && a->refs.load() == 2, "second instance shares the same engine");
    int s1 = ob::claimSlot(a, ob::Slot::kReceive, 0), s2 = ob::claimSlot(b, ob::Slot::kSend, 1);
    check(s1 == 0 && s2 == 1 && ob::slotsInUse(a, ob::Slot::kReceive) == 1 && ob::slotsInUse(a, ob::Slot::kSend) == 1, "slots claimed");
    int extra = 0;
    while (ob::claimSlot(a, ob::Slot::kReceive, 0) >= 0) ++extra;
    check(extra == 14, "16 slots in total, 17th refused");
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    ob::Engine& e = a->engine;
    std::printf("     state %d, device '%s', message '%s'\n", e.state(), e.deviceName().c_str(), e.message().c_str());
    check(e.state() == ob::Engine::kError && e.message().find("open") == 0, "fake DN2 without device node -> open error");
    // Group decoding: one fake transfer where Track 1 L/R = 0.25 and Track 2 L/R = 0.125.
    {
        const ob::Device* d = dn2;
        size_t frame = 0;
        for (int i = 0; i < d->numOutputs; ++i) frame += d->outputs[i].size;
        size_t blk = 32 + 7 * frame;
        std::vector<uint8_t> xfer(24 * blk, 0);
        auto put = [&](uint8_t* p, int size, float v) {   // inverse of decodeSample for 2.1 4-byte slots
            int32_t raw = (int32_t)(v * 2147483647.0f);
            uint8_t be[4] = {(uint8_t)(raw >> 24), (uint8_t)(raw >> 16), (uint8_t)(raw >> 8), (uint8_t)raw};
            if (size == 4) { p[0] = 0; p[1] = be[0]; p[2] = be[1]; p[3] = be[2]; } else std::memcpy(p, be, 3);
        };
        for (size_t b = 0; b < 24; ++b)
            for (size_t f = 0; f < 7; ++f)
            {
                uint8_t* fr = &xfer[b * blk + 32 + f * frame];
                size_t off = 0;
                for (int t = 0; t < d->numOutputs; ++t)
                {
                    float v = (t == 2 || t == 3) ? 0.25f : (t == 4 || t == 5) ? 0.125f : 0.0f;
                    put(fr + off, d->outputs[t].size, v);
                    off += d->outputs[t].size;
                }
            }
        ob::Slot* g = &a->slots[0];
        g->route.store(ob::kRouteGroup);
        g->groupMask.store(0x3);                       // Track 1 + Track 2
        ob::Slot* one = &a->slots[2];
        one->route.store(2);                           // Track 2 only
        uint64_t w0 = g->ring.written(), w1 = one->ring.written();
        a->engine.testDecode(d, xfer);
        bool okG = g->ring.written() - w0 == 168 && std::fabs(g->ring.l(w0 + 5) - 0.375f) < 1e-4f && std::fabs(g->ring.r(w0 + 100) - 0.375f) < 1e-4f;
        bool okS = std::fabs(one->ring.l(w1 + 5) - 0.125f) < 1e-4f;
        std::printf("     group sum %.4f (expect 0.375), single %.4f (expect 0.125)\n", g->ring.l(w0 + 5), one->ring.l(w1 + 5));
        check(okG && okS, "group = sum of selected tracks, single source unaffected");
    }
    ob::releaseShared(b);
    ob::releaseShared(a);
    check(access(ob::anchorPath().c_str(), F_OK) != 0, "last release removes the anchor");
    ob::Shared* c = ob::acquireShared(err);
    check(c && c->refs.load() == 1 && ob::slotsInUse(c, ob::Slot::kReceive) == 0, "fresh engine after full release");
    ob::releaseShared(c);
    unsetenv("OVERWITCH_SYSFS");
    std::printf("%s\n", fails ? "FAILED" : "ALL PASSED");
    return fails;
}
