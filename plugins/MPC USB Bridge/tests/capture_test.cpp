// Desktop test of the capture engine: records the ALSA device named in USB_BRIDGE_PCM (run_tests.sh
// sets up a "file" PCM that plays a 1 kHz test tone at 44.1 kHz, S32), pulls it through the DriftReader
// at 48 kHz in real time, and checks level, frequency and that the stream does not drop out.
#include "../src/ub_alsa.h"
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

int main()
{
    ub::Capture cap;
    cap.start();
    for (int i = 0; i < 100 && cap.state() != ub::Capture::kStreaming; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (cap.state() != ub::Capture::kStreaming) { std::printf("FAIL not streaming: %s\n", cap.message().c_str()); return 1; }
    std::printf("device %s, %s, %.0f Hz, period %d\n", cap.device().c_str(), cap.format().c_str(), cap.rate(), cap.period());
    ob::DriftReader rd;
    const double host = 48000.0;
    const int n = 256;
    rd.prepare(cap.rate(), host, cap.period());
    std::vector<float> L(n), R(n), all;
    auto t = std::chrono::steady_clock::now();
    for (int b = 0; b < (int)(3.0 * host / n); ++b)
    {
        t += std::chrono::microseconds((long)(1e6 * n / host));
        std::this_thread::sleep_until(t);
        rd.read(cap.ring, L.data(), R.data(), n);
        if (b * n > host) all.insert(all.end(), L.begin(), L.end());   // after 1 s settling
        else if (b * n > host / 2) { (void)R; }
    }
    double pk = 0, rpk = 0; int zc = 0;
    for (size_t i = 1; i < all.size(); ++i) { pk = std::max(pk, (double)std::fabs(all[i])); if ((all[i - 1] < 0) != (all[i] < 0)) ++zc; }
    for (float v : R) rpk = std::max(rpk, (double)std::fabs(v));
    const double hz = zc / 2.0 / (all.size() / host);
    std::printf("peak L %.3f R %.3f, freq %.1f Hz, dropouts %u, overruns %u, measured %.0f Hz, latency %.1f ms\n",
                pk, rpk, hz, rd.dropouts(), cap.xruns(), cap.measuredRate(), rd.latencyMs());
    cap.stop();
    bool ok = std::fabs(pk - 0.5) < 0.03 && std::fabs(rpk - 0.25) < 0.03 && std::fabs(hz - 1000) < 5 && rd.dropouts() <= 1;
    std::printf("%s\n", ok ? "PASS capture_test" : "FAIL capture_test");
    return ok ? 0 : 1;
}
