// Functional regression tests and musical demo renders for the NOXED
// instruments. Build: zig c++ -std=c++17 -O2 -Iframework tests/noxed_audio.cpp
// -o tests/noxed_audio.exe Run with MPC_PLUGIN_FOLDER pointing at plugins/Prism
// Wave and two output WAV paths.
#include "mpc_plugin.h"
#undef MPC_PLUGIN_ENTRY
#define MPC_PLUGIN_ENTRY(Class)
#include "../plugins/Acidline/src/Acidline.cpp"
#include "../plugins/Prism Wave/src/PrismWave.cpp"
#include <chrono>
#include <fstream>
#include <numeric>
#include <stdexcept>
struct Event {
  int frame;
  mpc::MidiEvent midi;
};
static std::vector<float> render(mpc::Plugin &p, float seconds,
                                 std::vector<Event> events, int block = 128) {
  const int total = int(seconds * 44100);
  std::vector<float> out(size_t(total) * 2);
  float l[512], r[512];
  float *channels[] = {l, r};
  std::stable_sort(
      events.begin(), events.end(),
      [](const Event &a, const Event &b) { return a.frame < b.frame; });
  size_t e = 0;
  for (int start = 0; start < total; start += block) {
    int n = std::min(block, total - start);
    std::fill(l, l + n, 0);
    std::fill(r, r + n, 0);
    mpc::MidiEvent midi[128];
    int count = 0;
    while (e < events.size() && events[e].frame < start + n) {
      midi[count] = events[e].midi;
      midi[count++].sampleOffset = events[e++].frame - start;
    }
    mpc::ProcessContext c;
    c.outputs = channels;
    c.numOutputs = 2;
    c.numFrames = n;
    c.sampleRate = 44100;
    c.midi = midi;
    c.numMidiEvents = count;
    c.transport.valid = c.transport.tempoValid = c.transport.ppqValid =
        c.transport.playing = true;
    c.transport.bpm = 130;
    c.transport.ppqPos = start / 44100.0 * 130 / 60;
    p.process(c);
    for (int i = 0; i < n; ++i) {
      if (!std::isfinite(l[i]) || !std::isfinite(r[i]))
        throw std::runtime_error("nonfinite audio");
      out[2 * (start + i)] = l[i];
      out[2 * (start + i) + 1] = r[i];
    }
  }
  return out;
}
static void require(bool condition, const char *msg) {
  if (!condition)
    throw std::runtime_error(msg);
  std::printf("PASS %s\n", msg);
}
static double rms(const std::vector<float> &x, int begin = 0, int end = -1) {
  if (end < 0)
    end = int(x.size());
  double e = 0;
  for (int i = begin; i < end; ++i)
    e += x[i] * x[i];
  return std::sqrt(e / std::max(1, end - begin));
}
static double difference(const std::vector<float> &a,
                         const std::vector<float> &b) {
  double sum = 0;
  for (size_t i = 0; i < a.size(); ++i)
    sum += std::fabs(a[i] - b[i]);
  return sum / a.size();
}
static void wav(const char *path, const std::vector<float> &x) {
  std::ofstream f(path, std::ios::binary);
  auto u16 = [&](unsigned v) {
    f.put(char(v));
    f.put(char(v >> 8));
  };
  auto u32 = [&](unsigned v) {
    u16(v);
    u16(v >> 16);
  };
  f.write("RIFF", 4);
  u32(36 + unsigned(x.size() * 2));
  f.write("WAVEfmt ", 8);
  u32(16);
  u16(1);
  u16(2);
  u32(44100);
  u32(176400);
  u16(4);
  u16(16);
  f.write("data", 4);
  u32(unsigned(x.size() * 2));
  for (float s : x)
    u16(unsigned(int16_t(std::lround(mpc::clamp(s, -1.0f, 1.0f) * 32767))));
  if (!f)
    throw std::runtime_error("cannot write WAV");
}
static std::vector<Event> single(int vel = 90) {
  return {{4410, mpc::MidiEvent::noteOn(0, 48, vel)},
          {22050, mpc::MidiEvent::noteOff(0, 48)}};
}
int main(int argc, char **argv) {
  try {
    Acidline acid;
    acid.prepare(44100, 512);
    acid.reset();
    auto low = render(acid, 1.5f, single(90), 32);
    require(rms(low) > 0.001, "Acidline produces audio at 32-frame blocks");
    require(rms(low, 0, 8820) == 0,
            "MIDI sample offset leaves pre-note region silent");
    require(rms(low, int(low.size()) - 8820) < .0001,
            "Acidline releases without a stuck note");
    acid.reset();
    auto high = render(acid, 1.5f, single(120), 512);
    require(rms(high) > rms(low) * 1.05,
            "Velocity accent changes amplitude/brightness");
    std::vector<Event> overlapping = {{0, mpc::MidiEvent::noteOn(0, 48, 110)},
                                      {5000, mpc::MidiEvent::noteOn(0, 60, 90)},
                                      {9000, mpc::MidiEvent::noteOff(0, 60)},
                                      {16000, mpc::MidiEvent::noteOff(0, 48)}};
    acid.reset();
    auto slide = render(acid, 1.2f, overlapping);
    acid.set(Acidline::Legato, 0);
    acid.reset();
    auto retrigger = render(acid, 1.2f, overlapping);
    require(difference(slide, retrigger) > .001,
            "Overlapping notes use legato slide rather than retrigger");
    require(rms(slide, 20000, 30000) > .0001,
            "Mono last-note priority returns to held note");
    require(rms(slide, int(slide.size()) - 8820) < .0001,
            "Overlapping mono notes eventually release");
    for (int p = 0; p < acid.numPrograms(); ++p) {
      acid.setProgram(p);
      acid.reset();
      auto x = render(acid, 1.5f, single());
      require(rms(x) > .00001, acid.programName(p));
    }
    noxed::Wavetables tables;
    require(tables.banks[4].imported, "User 01 demo WAV is actually loaded");
    require(!tables.banks[5].imported && tables.get(5).frames == 8,
            "Empty user slot has a defined Basic Morph fallback");
    const auto &bank = tables.banks[4];
    require(bank.frames == 16,
            "Imported multiframe WAV preserves all 16 frames");
    require(std::fabs(bank.sample(.137f, 0, 0) - bank.sample(.137f, 1, 0)) >
                .001,
            "Imported wavetable frame scanning changes the wave");
    {
      int level = noxed::Wavetables::mip(5000, 44100);
      std::vector<noxed::Complex> a(noxed::N);
      for (int i = 0; i < noxed::N; ++i)
        a[i] = bank.sample(float(i) / noxed::N, .5f, level);
      noxed::fft(a, false);
      double high = 0, all = 0;
      for (int k = 1; k < noxed::N / 2; ++k) {
        all += std::norm(a[k]);
        if (k > 3)
          high += std::norm(a[k]);
      }
      require(high / all < 1e-7,
              "High-pitch mip level suppresses out-of-band harmonics");
    }
    PrismWave prism;
    prism.prepare(44100, 512);
    prism.set(PrismWave::ATable, 4);
    prism.set(PrismWave::BLevel, 0);
    prism.set(PrismWave::M1, 0);
    prism.reset();
    auto imported = render(prism, 2, single(), 32);
    require(rms(imported) > .001,
            "Prism Wave plays the imported WAV at 32-frame blocks");
    prism.set(PrismWave::ATable, 0);
    prism.reset();
    auto basic = render(prism, 2, single(), 512);
    require(difference(imported, basic) > .001,
            "Selecting an imported slot changes rendered plugin audio");
    require(rms(basic, int(basic.size()) - 8820) < .0001,
            "Prism Wave releases without a stuck note");
    prism.set(PrismWave::ATable, 4);
    prism.set(PrismWave::APos, 0);
    prism.reset();
    auto first = render(prism, 1.5f, single());
    prism.set(PrismWave::APos, 100);
    prism.reset();
    auto last = render(prism, 1.5f, single());
    require(difference(first, last) > .001,
            "Position knob scans imported frames in the actual plugin");
    prism.set(PrismWave::APos, 40);
    prism.reset();
    auto middle = render(prism, 1.5f, single());
    prism.set(PrismWave::S1, 1);
    prism.set(PrismWave::D1, 0);
    prism.set(PrismWave::M1, 90);
    prism.reset();
    auto modulated = render(prism, 1.5f, single());
    std::printf("INFO modulation audio difference %.8f (amount %.1f)\n",
                difference(middle, modulated), prism.get(PrismWave::M1));
    require(difference(middle, modulated) > .0001,
            "Modulation matrix moves imported wavetable position");
    prism.set(PrismWave::Release, 50);
    prism.reset();
    auto pedal = render(prism, 1.5f,
                        {{0, mpc::MidiEvent::noteOn(0, 60, 100)},
                         {4000, mpc::MidiEvent::cc(0, 64, 127)},
                         {6000, mpc::MidiEvent::noteOff(0, 60)},
                         {22050, mpc::MidiEvent::cc(0, 64, 0)}});
    require(rms(pedal, 22000, 30000) > .0001 &&
                rms(pedal, int(pedal.size()) - 8820) < .0001,
            "Sustain pedal holds and then releases Prism voices");
    for (int p = 0; p < prism.numPrograms(); ++p) {
      prism.setProgram(p);
      prism.reset();
      auto x = render(prism, 2, single());
      require(rms(x) > .00001, prism.programName(p));
    }
    prism.set(PrismWave::AUnison, 3);
    prism.set(PrismWave::BUnison, 3);
    prism.reset();
    {
      std::vector<Event> ev;
      for (int n = 48; n < 56; ++n)
        ev.push_back({0, mpc::MidiEvent::noteOn(0, n, 127)});
      auto start = std::chrono::steady_clock::now();
      auto x = render(prism, 2, ev, 32);
      double sec = std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - start)
                       .count();
      require(rms(x) > .0001, "Eight-voice / 48-oscillator unison stress "
                              "render is finite and audible");
      std::printf("INFO desktop worst-case rendering %.3f seconds for 2 "
                  "seconds audio (not MPC CPU)\n",
                  sec);
    }
    if (argc == 3) {
      for (int p = 0; p < acid.numParams(); ++p)
        acid.set(p, acid.param(p).def);
      acid.reset();
      std::vector<Event> riff;
      int notes[] = {36, 36, 43, 48, 36, 46, 43, 39,
                     36, 48, 43, 51, 48, 46, 43, 39};
      int step = int(44100 * 60.0 / 130 / 4);
      for (int i = 0; i < 64; ++i) {
        int start = i * step, note = notes[i % 16],
            len = (i % 4 == 2) ? int(step * 1.3) : int(step * .65);
        riff.push_back(
            {start, mpc::MidiEvent::noteOn(0, note, i % 4 == 0 ? 120 : 86)});
        riff.push_back({start + len, mpc::MidiEvent::noteOff(0, note)});
      }
      wav(argv[1], render(acid, 9.0f, riff));
      for (int p = 0; p < prism.numParams(); ++p)
        prism.set(p, prism.param(p).def);
      prism.set(PrismWave::ATable, 4);
      prism.set(PrismWave::AUnison, 3);
      prism.set(PrismWave::BUnison, 3);
      prism.set(PrismWave::Attack, 120);
      prism.set(PrismWave::Release, 700);
      prism.set(PrismWave::M1, 40);
      prism.set(PrismWave::Chorus, 25);
      prism.set(PrismWave::DelayMix, 20);
      prism.reset();
      std::vector<Event> chords;
      int roots[] = {48, 44, 51, 46};
      for (int c = 0; c < 4; ++c)
        for (int interval : {0, 3, 7, 14}) {
          int n = roots[c] + interval;
          chords.push_back({c * 88200, mpc::MidiEvent::noteOn(0, n, 100)});
          chords.push_back({c * 88200 + 70000, mpc::MidiEvent::noteOff(0, n)});
        }
      wav(argv[2], render(prism, 10.0f, chords));
    }
    std::puts("ALL NOXED FUNCTIONAL TESTS PASSED");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL %s\n", e.what());
    return 1;
  }
}
