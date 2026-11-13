// Desktop simulation of the USB->audio path: bursty 48 kHz producer with clock drift and jitter,
// block-based consumer at the host rate. Checks: no dropouts after start-up, clean sine output,
// drift estimate close to the simulated one.  g++ -O2 -std=c++17 -Isrc tests/sim_stream.cpp
#include "ob_stream.h"
#include <cstdio>
#include <random>
#include <vector>

static ob::StereoRing ring;

int run(double hostRate, int block, double driftPpm, double jitterMs, double seconds, bool verbose)
{
    ob::DriftReader rd;
    rd.prepare(hostRate);
    std::mt19937 rng(1);
    std::uniform_real_distribution<double> jit(-jitterMs / 1000.0, jitterMs / 1000.0);
    const double srcRate = 48000.0 * (1.0 + driftPpm * 1e-6);
    const double burstDt = 168.0 / srcRate, blockDt = block / hostRate;
    double tNextBurst = 0.002, tNextBlock = 0.0, phase = 0.0;
    std::vector<float> L(block), R(block), out;
    uint32_t dropAfterStart = 0, startDrops = 0;
    bool started = false;
    double tBurstNominal = 0.002;
    while (tNextBlock < seconds)
    {
        if (tNextBurst <= tNextBlock)
        {
            for (int i = 0; i < 168; ++i)
            {
                float v = (float)std::sin(phase);
                phase += 2 * M_PI * 997.0 / 48000.0;   // the device's own clock defines its samples
                ring.push(v, -v);
            }
            ring.publish();
            tBurstNominal += burstDt;
            tNextBurst = tBurstNominal + jit(rng);
        }
        else
        {
            bool ok = rd.read(ring, L.data(), R.data(), block);
            if (ok && !started) { started = true; startDrops = rd.dropouts(); }
            if (started) out.insert(out.end(), L.begin(), L.end());
            tNextBlock += blockDt;
        }
    }
    dropAfterStart = rd.dropouts() - startDrops;
    // Discontinuity check on the settled part (skip first 2 s).
    double worst = 0;
    size_t s0 = (size_t)(2 * hostRate);
    for (size_t i = s0 + 2; i < out.size(); ++i)
        worst = std::max(worst, (double)std::fabs(out[i] - 2 * out[i - 1] + out[i - 2]));
    double expect = std::pow(2 * M_PI * 997.0 / hostRate, 2);
    bool pass = dropAfterStart == 0 && worst < expect * 1.6;
    std::printf("%-5s rate %.0f block %3d drift %+6.0f ppm jitter %.1f ms: dropouts %u, latency %.1f ms, "
                "est. drift %+7.1f ppm, 2nd-diff %.4f (sine %.4f)\n",
                pass ? "PASS" : "FAIL", hostRate, block, driftPpm, jitterMs, dropAfterStart, rd.latencyMs(),
                rd.driftPpm(), worst, expect);
    (void)verbose;
    return pass ? 0 : 1;
}

int main()
{
    int fails = 0;
    for (double rate : {44100.0, 48000.0})
        for (int block : {32, 128, 256, 512})
            for (double drift : {-150.0, 0.0, 80.0, 200.0})
                fails += run(rate, block, drift, 1.0, 60.0, false);
    fails += run(44100.0, 128, 100.0, 2.5, 120.0, false);
    std::printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails ? 1 : 0;
}
