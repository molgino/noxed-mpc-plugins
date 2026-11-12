// NOXED Acidline: PolyBLEP oscillator -> nonlinear three-pole resonant lowpass
// (2x processing) -> accent VCA -> drive/DC blocker -> smoothed master.
// Original acid-inspired design, not a circuit-exact Roland emulation.
// SPDX-License-Identifier: MIT
#include "dsp/adsr.h"
#include "dsp/onepole.h"
#include "dsp/oscillator.h"
#include "dsp/smoother.h"
#include "mpc_plugin.h"
#include <algorithm>
using namespace mpc::dsp;
class Acidline : public mpc::Plugin {
public:
  enum P {
    Volume,
    Cutoff,
    Resonance,
    EnvAmount,
    Decay,
    Accent,
    Slide,
    Drive,
    Waveform,
    Tune,
    PulseWidth,
    Threshold,
    Legato,
    Release,
    KeyTrack,
    BendRange,
    Count
  };
  static mpc::PluginInfo makeInfo() {
    mpc::PluginInfo i;
    i.name = "Acidline";
    i.vendor = "NOXED";
    i.uniqueId = MPC_FOURCC('N', 'c', 'e', 'p');
    i.version = 101;
    i.isInstrument = true;
    i.numInputs = 0;
    return i;
  }
  Acidline() : Plugin(makeInfo()) {
    using namespace mpc;
    addParam(floatParam("volume", "Master", -48, 0, -12, "dB", 1));
    addParam(logParam("cutoff", "Cutoff", 30, 12000, 420, "Hz", 0));
    addParam(floatParam("resonance", "Resonance", 0, 100, 65, "%", 0));
    addParam(floatParam("env", "Env Amount", 0, 100, 65, "%", 0));
    addParam(logParam("decay", "Decay", 40, 2500, 260, "ms", 0));
    addParam(floatParam("accent", "Accent", 0, 100, 60, "%", 0));
    addParam(floatParam("slide", "Slide Time", 0, 400, 65, "ms", 0));
    addParam(floatParam("drive", "Drive", 0, 100, 20, "%", 0));
    addParam(choiceParam("wave", "Waveform", {"Saw", "Square"}, 0));
    addParam(floatParam("tune", "Tune", -12, 12, 0, "st", 2));
    addParam(floatParam("pulse", "Pulse Width", 25, 75, 50, "%", 0));
    addParam(intParam("threshold", "Accent Vel", 1, 127, 100));
    addParam(boolParam("legato", "Legato Slide", true));
    addParam(logParam("release", "Release", 5, 1000, 65, "ms", 0));
    addParam(floatParam("keytrack", "Key Track", 0, 100, 20, "%", 0));
    addParam(intParam("bend", "Bend Range", 0, 12, 2, "st"));
    auto pr = [&](const char *n,
                  std::initializer_list<std::pair<int, float>> edits) {
      std::vector<float> v;
      for (int p = 0; p < Count; ++p)
        v.push_back(param(p).def);
      for (auto x : edits)
        v[x.first] = x.second;
      addProgram(n, v);
    };
    pr("NOXED Acid", {});
    pr("Rubber Square", {{Waveform, 1},
                         {Cutoff, 260},
                         {Resonance, 78},
                         {EnvAmount, 78},
                         {Slide, 95}});
    pr("Warehouse",
       {{Cutoff, 180}, {Resonance, 88}, {Drive, 60}, {Decay, 420}});
    pr("Clean Sub",
       {{Cutoff, 160}, {Resonance, 15}, {EnvAmount, 10}, {Drive, 0}});
    pr("Sharp Screamer", {{Cutoff, 1100},
                          {Resonance, 94},
                          {EnvAmount, 80},
                          {Decay, 130},
                          {Drive, 45}});
    pr("Liquid", {{Cutoff, 600},
                  {Resonance, 70},
                  {Decay, 700},
                  {Slide, 120},
                  {Accent, 85}});
  }
  void prepare(double sr, int) override {
    sr_ = float(sr);
    osc_.setSampleRate(sr * 2);
    amp_.setSampleRate(sr);
    env_.setSampleRate(sr);
    gain_.setTime(12, sr);
    cut_.setTime(12, sr);
    res_.setTime(12, sr);
    drv_.setTime(12, sr);
    dc_.setCutoff(20, sr);
  }
  void reset() override {
    amp_.reset();
    env_.reset();
    std::fill(z_, z_ + 3, 0);
    std::fill(held_, held_ + 128, 0u);
    serial_ = 0;
    note_ = -1;
    accent_ = 0;
    bend_ = 0;
    ctrl_ = 0;
    last_ = 0;
    osc_.resetPhase();
    dc_.reset();
    gain_.snap(dbToGain(get(Volume)));
    cut_.snap(get(Cutoff));
    res_.snap(get(Resonance) * .01f);
    drv_.snap(get(Drive) * .01f);
    pitch_.snap(48);
  }
  void process(mpc::ProcessContext &c) override {
    for (int p = 0; p < Count; ++p)
      s_[p] = get(p);
    gain_.setTarget(dbToGain(s_[Volume]));
    cut_.setTarget(s_[Cutoff]);
    res_.setTarget(s_[Resonance] * .01f);
    drv_.setTarget(s_[Drive] * .01f);
    pitch_.setTime(s_[Slide], sr_);
    amp_.set(2, 180, 0.72f, s_[Release]);
    env_.set(1, s_[Decay], 0, s_[Release]);
    osc_.setWave(s_[Waveform] > .5f ? Wave::Square : Wave::Saw);
    osc_.setPulseWidth(s_[PulseWidth] * .01f);
    int e = 0;
    for (int n = 0; n < c.numFrames; ++n) {
      while (e < c.numMidiEvents && c.midi[e].sampleOffset <= n)
        handle(c.midi[e++]);
      float pitch = pitch_.process(), a = amp_.process(), f = env_.process();
      float cutoff = cut_.process(), r = res_.process(), d = drv_.process();
      if ((ctrl_++ & 7) == 0) {
        osc_.setFrequency(
            mpc::noteToHz(pitch + s_[Tune] + bend_ * s_[BendRange]));
        float oct = f * (s_[EnvAmount] * .065f + accent_ * s_[Accent] * .018f) +
                    (pitch - 48) * s_[KeyTrack] / 1200;
        float hz = clamp(cutoff * std::exp2(oct), 25.0f, 14000.0f);
        float g = std::tan(kPi * hz / (sr_ * 2));
        g_ = g / (1 + g);
      }
      float y = 0;
      for (int os = 0; os < 2; ++os) {
        float x = fastTanh(osc_.process() * .55f - (r * 7.7f) * last_);
        for (int p = 0; p < 3; ++p) {
          float v = (x - z_[p]) * g_;
          x = v + z_[p];
          z_[p] = clamp(x + v, -4.0f, 4.0f);
        }
        last_ = fastTanh(x);
        y += last_;
      }
      y = dc_.process(fastTanh(y * .5f * (1 + d * 6))) * a *
          (.7f + accent_ * s_[Accent] * .009f);
      y = fastTanh(y) * gain_.process();
      c.outputs[0][n] = y;
      if (c.numOutputs > 1)
        c.outputs[1][n] = y;
    }
  }

private:
  void handle(const mpc::MidiEvent &e) {
    if (e.isNoteOn()) {
      bool slide = note_ >= 0 && held_[note_] != 0 && s_[Legato] > .5f;
      held_[e.note()] = ++serial_;
      note_ = e.note();
      if (slide)
        pitch_.setTarget(float(note_));
      else {
        pitch_.snap(float(note_));
        accent_ = e.velocity() >= s_[Threshold] ? 1.0f : 0.0f;
        amp_.noteOn();
        env_.noteOn();
      }
      ctrl_ = 0;
    } else if (e.isNoteOff()) {
      held_[e.note()] = 0;
      if (note_ == e.note()) {
        int newest = -1;
        unsigned order = 0;
        for (int k = 0; k < 128; ++k)
          if (held_[k] > order) {
            newest = k;
            order = held_[k];
          }
        note_ = newest;
        if (note_ < 0) {
          amp_.noteOff();
          env_.noteOff();
        } else
          pitch_.setTarget(float(note_));
      }
    } else if (e.isAllNotesOff()) {
      std::fill(held_, held_ + 128, 0u);
      note_ = -1;
      amp_.noteOff();
      env_.noteOff();
    } else if (e.isPitchBend())
      bend_ = e.pitchBend() / 8192.0f;
  }
  PolyBlepOsc osc_;
  ADSR amp_, env_;
  Smoother pitch_, gain_, cut_, res_, drv_;
  OnePoleHP dc_;
  float sr_ = 44100, s_[Count]{}, z_[3]{}, last_ = 0, g_ = .05f, accent_ = 0,
        bend_ = 0;
  unsigned held_[128]{}, serial_ = 0, ctrl_ = 0;
  int note_ = -1;
};
MPC_PLUGIN_ENTRY(Acidline)
