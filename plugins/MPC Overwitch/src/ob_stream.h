// ob_stream.h - lock-free stereo ring (USB thread -> audio thread) and an adaptive resampler that
// converts the Elektron 48 kHz stream to the host rate while following the clock drift between
// the two devices. Original NOXED code, GPL-3.0-or-later.
//
// How the drift follower works
//   The USB thread writes 48 kHz frames in bursts (168 frames every 3.5 ms). The audio thread reads
//   them with a fractional read position and a ratio close to 48000 / hostRate. Every block it
//   measures how far the read position trails the write position ("fill"), smooths that over
//   about half a second, and nudges the ratio in proportion to the distance from the target fill
//   (at most +-0.2 %, inaudible). Because the two clocks differ only by parts per million the fill
//   settles a few frames from the target and stays there. If the buffer ever runs dry or overflows
//   (USB stalls, plugin not processed for a while) the reader goes quiet, re-primes and continues;
//   each such event is counted as a dropout, and a dropout after priming raises the safety margin.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ob {

class StereoRing
{
public:
    static constexpr uint32_t kFrames = 16384;   // power of two, ~340 ms at 48 kHz
    static constexpr uint32_t kMask = kFrames - 1;

    // Producer (USB thread).
    void push(float l, float r)
    {
        uint64_t w = wLocal_;
        buf_[(w & kMask) * 2] = l;
        buf_[(w & kMask) * 2 + 1] = r;
        wLocal_ = w + 1;
    }
    void publish() { w_.store(wLocal_, std::memory_order_release); }

    // Consumer (audio thread).
    uint64_t written() const { return w_.load(std::memory_order_acquire); }
    float l(uint64_t i) const { return buf_[(i & kMask) * 2]; }
    float r(uint64_t i) const { return buf_[(i & kMask) * 2 + 1]; }

private:
    float buf_[kFrames * 2] = {};
    uint64_t wLocal_ = 0;
    alignas(64) std::atomic<uint64_t> w_{0};
};

// Reads a ring written in bursts at srcRate and produces audio at dstRate. Used in both directions:
//   device -> MPC: src 48000, dst host rate, bursts of 168 frames (one USB transfer)
//   MPC -> device: src host rate, dst 48000, bursts of one host block
class DriftReader
{
public:
    static constexpr int kUsbBurst = 168;         // frames per USB transfer (24 blocks x 7)

    void prepare(double srcRate, double dstRate, int burst)
    {
        srcRate_ = srcRate > 0 ? srcRate : 48000.0;
        hostRate_ = dstRate > 0 ? dstRate : 44100.0;
        burst_ = burst > 0 ? burst : kUsbBurst;
        nominal_ = srcRate_ / hostRate_;
        ratio_ = nominal_;
        primed_ = false;
        primedA_.store(false, std::memory_order_relaxed);
        safety_ = 96.0;
        maxBlock_ = 0;
    }

    // Fills out[0..n) with resampled audio. Returns false (and writes silence) while priming.
    bool read(const StereoRing& ring, float* outL, float* outR, int n)
    {
        if (n > maxBlock_) maxBlock_ = n;
        const double target = maxBlock_ * nominal_ + burst_ + safety_;
        const uint64_t w = ring.written();

        if (!primed_)
        {
            if ((double)w >= target + 4)
            {
                rpos_ = (double)w - target;
                fillAvg_ = target;
                ratio_ = nominal_;
                primed_ = true;
                primedA_.store(true, std::memory_order_relaxed);
            }
            if (!primed_) { silence(outL, outR, n); return false; }
        }

        double fill = (double)w - rpos_;
        // Overflow (reader stalled or producer restarted): resynchronise.
        if (fill > StereoRing::kFrames - 2048 || fill < 0)
        {
            ++dropouts_;
            rpos_ = (double)w - target;
            fill = target;
            fillAvg_ = target;
        }

        // Smooth the fill level over ~0.5 s and steer the ratio proportionally.
        const double a = std::min(1.0, n / (0.5 * hostRate_));
        fillAvg_ += (fill - fillAvg_) * a;
        double corr = (fillAvg_ - target) * 1.0e-5;
        if (corr > 2.0e-3) corr = 2.0e-3;
        if (corr < -2.0e-3) corr = -2.0e-3;
        ratio_ = nominal_ * (1.0 + corr);

        // Enough data for this block (4-point interpolation needs 2 frames ahead)?
        if (rpos_ + n * ratio_ + 3.0 > (double)w)
        {
            ++dropouts_;
            safety_ = std::min(safety_ + 48.0, 960.0);
            primed_ = false;
            primedA_.store(false, std::memory_order_relaxed);
            silence(outL, outR, n);
            return false;
        }

        for (int i = 0; i < n; ++i)
        {
            const double fl = std::floor(rpos_);
            const uint64_t k = (uint64_t)fl;
            const float t = (float)(rpos_ - fl);
            outL[i] = hermite(ring.l(k - 1), ring.l(k), ring.l(k + 1), ring.l(k + 2), t);
            outR[i] = hermite(ring.r(k - 1), ring.r(k), ring.r(k + 1), ring.r(k + 2), t);
            rpos_ += ratio_;
        }
        latMs_.store(fillAvg_ / srcRate_ * 1000.0, std::memory_order_relaxed);
        drift_.store((ratio_ / nominal_ - 1.0) * 1.0e6, std::memory_order_relaxed);
        return true;
    }

    void reset() { primed_ = false; primedA_.store(false, std::memory_order_relaxed); }

    // Diagnostics, safe to read from other threads.
    double latencyMs() const { return latMs_.load(std::memory_order_relaxed); }
    double driftPpm() const { return drift_.load(std::memory_order_relaxed); }
    uint32_t dropouts() const { return dropouts_.load(std::memory_order_relaxed); }
    bool primed() const { return primedA_.load(std::memory_order_relaxed); }

private:
    static float hermite(float xm1, float x0, float x1, float x2, float t)
    {
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((((a * t) - b) * t + c) * t + x0);
    }
    static void silence(float* l, float* r, int n)
    {
        std::memset(l, 0, sizeof(float) * (size_t)n);
        std::memset(r, 0, sizeof(float) * (size_t)n);
    }

    double srcRate_ = 48000.0;
    int burst_ = kUsbBurst;
    double hostRate_ = 44100.0, nominal_ = 48000.0 / 44100.0, ratio_ = 48000.0 / 44100.0;
    double rpos_ = 0.0, fillAvg_ = 0.0, safety_ = 96.0;
    int maxBlock_ = 0;
    bool primed_ = false;
    std::atomic<uint32_t> dropouts_{0};
    std::atomic<double> latMs_{0.0}, drift_{0.0};
    std::atomic<bool> primedA_{false};
};

} // namespace ob
