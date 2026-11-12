// Original NOXED wavetable bank. FFT-derived mip levels built at load time.
// 2048 samples/frame, <=64 frames, 8 fixed user slots. No I/O in audio
// callback. SPDX-License-Identifier: MIT
#pragma once
#include "dsp/math.h"
#include "dsp/wav.h"
#include "mpc_plugin.h"
#include <algorithm>
#include <array>
#include <complex>
namespace noxed {
constexpr int N = 2048, Levels = 10;
using Complex = std::complex<float>;
inline void fft(std::vector<Complex> &a, bool inverse) {
  for (int i = 1, j = 0; i < N; ++i) {
    int bit = N >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (int len = 2; len <= N; len <<= 1) {
    float angle = (inverse ? 2 : -2) * mpc::dsp::kPi / len;
    Complex step(std::cos(angle), std::sin(angle));
    for (int i = 0; i < N; i += len) {
      Complex w(1, 0);
      for (int j = 0; j < len / 2; ++j) {
        Complex u = a[i + j], v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
        w *= step;
      }
    }
  }
  if (inverse)
    for (auto &x : a)
      x /= float(N);
}
struct Bank {
  int frames = 0;
  bool imported = false;
  std::vector<float> data;
  void build(const std::vector<float> &input) {
    frames = int(input.size() / N);
    data.resize(size_t(frames) * Levels * N);
    std::vector<Complex> spectrum(N), work(N);
    for (int f = 0; f < frames; ++f) {
      for (int i = 0; i < N; ++i)
        spectrum[i] = input[size_t(f) * N + i];
      fft(spectrum, false);
      spectrum[0] = 0;
      spectrum[N / 2] = 0;
      float peak = 0;
      for (int i = 0; i < N; ++i)
        peak = std::max(peak, std::fabs(input[size_t(f) * N + i]));
      const float scale = peak > 1e-6f ? .85f / peak : 1;
      for (int l = 0; l < Levels; ++l) {
        int harmonics = std::max(1, (N / 2 - 1) >> l);
        std::copy(spectrum.begin(), spectrum.end(), work.begin());
        for (int k = harmonics + 1; k < N - harmonics; ++k)
          work[k] = 0;
        fft(work, true);
        for (int i = 0; i < N; ++i)
          data[(size_t(f) * Levels + l) * N + i] = work[i].real() * scale;
      }
    }
  }
  float sample(float phase, float position, int level) const {
    float fp = mpc::clamp(position, 0.0f, 1.0f) * (frames - 1);
    int f = int(fp), f2 = std::min(f + 1, frames - 1);
    float p = phase * N;
    int i = int(p) & (N - 1), j = (i + 1) & (N - 1);
    float t = p - int(p);
    const float *a = &data[(size_t(f) * Levels + level) * N],
                *b = &data[(size_t(f2) * Levels + level) * N];
    float x = a[i] + t * (a[j] - a[i]), y = b[i] + t * (b[j] - b[i]);
    return x + (fp - f) * (y - x);
  }
};
struct Wavetables {
  std::array<Bank, 12> banks;
  Wavetables() {
    for (int b = 0; b < 4; ++b) {
      std::vector<float> x(8 * N);
      for (int f = 0; f < 8; ++f)
        for (int i = 0; i < N; ++i) {
          float p = float(i) / N, t = float(f) / 7, y = 0;
          if (b == 0) { // morph sine -> triangle -> saw -> square
            float shapes[] = {std::sin(2 * mpc::dsp::kPi * p),
                              1 - 4 * std::fabs(p - .5f), 2 * p - 1,
                              p < .5f ? 1.0f : -1.0f};
            float q = t * 3;
            int a = std::min(2, int(q));
            y = shapes[a] + (q - a) * (shapes[a + 1] - shapes[a]);
          } else if (b == 1)
            y = std::sin(2 * mpc::dsp::kPi * p +
                         (t * 5) * std::sin(2 * mpc::dsp::kPi * p * 2));
          else if (b == 2) {
            for (int h = 1; h <= 24; ++h) {
              float center = 2 + t * 17;
              float d = (h - center) / 3;
              y += std::sin(2 * mpc::dsp::kPi * p * h) * std::exp(-d * d) /
                   std::sqrt(float(h));
            }
          } else {
            for (int h = 1; h <= 20; ++h)
              y += std::sin(2 * mpc::dsp::kPi * p * h) *
                   (std::sin(h * 1.71f + t * 5) * .5f + .5f) / h;
          }
          x[f * N + i] = y;
        }
      banks[b].build(x);
    }
    for (int u = 0; u < 8; ++u) {
      char path[64];
      std::snprintf(path, sizeof(path), "Wavetables/user%02d.wav", u + 1);
      // Bound memory before handing the bytes to the SDK WAV decoder.
      const std::string full = mpc::contentPath(path);
      std::FILE *file = std::fopen(full.c_str(), "rb");
      if (!file)
        continue;
      if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        continue;
      }
      long bytes = std::ftell(file);
      std::fclose(file);
      if (bytes < 44 || bytes > 2 * 1024 * 1024) {
        mpc::log("Prism Wave: rejected oversized/empty %s", path);
        continue;
      }
      ::dsp::WavData w;
      if (!::dsp::loadWav(full, w))
        continue;
      size_t size = w.numFrames();
      bool valid = size >= N && size <= 64 * N && size % N == 0;
      if (valid)
        for (float x : w.channels[0])
          if (!std::isfinite(x) || std::fabs(x) > 100) {
            valid = false;
            break;
          }
      if (!valid) {
        mpc::log("Prism Wave: rejected %s; use the importer (2048 "
                 "samples/frame, max 64 frames)",
                 path);
        continue;
      }
      banks[u + 4].build(w.channels[0]);
      banks[u + 4].imported = true;
    }
  }
  const Bank &get(int i) const {
    i = mpc::clamp(i, 0, 11);
    return banks[i].frames ? banks[i] : banks[0];
  }
  static std::vector<std::string> names() {
    return {"Basic Morph", "Spectral FM", "Formants", "Digital",
            "User 01",     "User 02",     "User 03",  "User 04",
            "User 05",     "User 06",     "User 07",  "User 08"};
  }
  static int mip(float hz, float sr) {
    float allowed = sr * .45f / std::max(hz, 1.0f);
    int l = 0;
    while (l < Levels - 1 && ((N / 2 - 1) >> l) > allowed)
      ++l;
    return l;
  }
};
} // namespace noxed
