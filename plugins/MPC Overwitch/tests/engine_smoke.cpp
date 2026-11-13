// Engine smoke test without hardware: state machine against a fake sysfs tree, source mapping,
// block sizes and sample decoding.  g++ -O2 -std=c++17 -pthread -Isrc tests/engine_smoke.cpp
#include "ob_usb.h"
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
    float a = ob::decodeSample(s3, 3, true), b = ob::decodeSample(s4, 4, true), c = ob::decodeSample(n3, 3, true);
    std::printf("     decode: 3-byte %.6f, 4-byte %.6f, negative %.6f\n", a, b, c);
    check(std::fabs(a - 0.5f) < 1e-5f && std::fabs(b - 0.5f) < 1e-5f && std::fabs(c + 0.5f) < 1e-5f, "24-bit sample decoding");

    setenv("OVERWITCH_SYSFS", "/tmp/fakesys", 1);
    ob::Engine e;
    e.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    std::printf("     state %d, device '%s', message '%s'\n", e.state(), e.deviceName().c_str(), e.message().c_str());
    check(e.state() == ob::Engine::kError && e.message().find("open") == 0, "fake DN2 without device node -> open error");
    e.stop();
    unsetenv("OVERWITCH_SYSFS");
    std::printf("%s\n", fails ? "FAILED" : "ALL PASSED");
    return fails;
}
