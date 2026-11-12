// NOXED Prism Wave: 2 mipmapped wavetable oscillators (1..3 unison each),
// sub/noise
// -> stereo SVF -> amp ADSR -> saturation/chorus/ping-pong delay -> master.
// Two ADSRs, two global tempo-sync LFOs, four assignable modulation routes.
// Original implementation, not Serum code, assets, presets or file formats.
// SPDX-License-Identifier: MIT
#include "Wavetables.h"
#include "dsp/adsr.h"
#include "dsp/delay_line.h"
#include "dsp/lfo.h"
#include "dsp/noise.h"
#include "dsp/oscillator.h"
#include "dsp/smoother.h"
#include "dsp/svf.h"
#include "dsp/voice.h"
using namespace mpc::dsp;
class PrismWave : public mpc::Plugin {
public:
  enum P {
    Master,
    ATable,
    APos,
    ALevel,
    AOct,
    AUnison,
    ADetune,
    BTable,
    BPos,
    BLevel,
    BOct,
    BSemi,
    BUnison,
    BDetune,
    Sub,
    Noise,
    Width,
    Cutoff,
    Resonance,
    Mode,
    EnvAmt,
    KeyTrack,
    Attack,
    Decay,
    Sustain,
    Release,
    EAttack,
    EDecay,
    ESustain,
    ERelease,
    L1Shape,
    L1Rate,
    L1Sync,
    L1Div,
    L2Shape,
    L2Rate,
    L2Sync,
    L2Div,
    S1,
    D1,
    M1,
    S2,
    D2,
    M2,
    S3,
    D3,
    M3,
    S4,
    D4,
    M4,
    Drive,
    Chorus,
    DelayMix,
    DelayDiv,
    Feedback,
    Bend,
    Count
  };
  static mpc::PluginInfo makeInfo() {
    mpc::PluginInfo i;
    i.name = "Prism Wave";
    i.vendor = "NOXED";
    i.uniqueId = MPC_FOURCC('N', 'H', 's', 'C');
    i.version = 100;
    i.isInstrument = true;
    i.numInputs = 0;
    return i;
  }
  PrismWave() : Plugin(makeInfo()) {
    using namespace mpc;
    addParam(floatParam("master", "Master", -48, 0, -12, "dB", 1));
    addParam(choiceParam("a_table", "A Table", noxed::Wavetables::names()));
    addParam(floatParam("a_pos", "A Position", 0, 100, 50, "%", 1));
    addParam(floatParam("a_level", "A Level", 0, 100, 80, "%", 0));
    addParam(intParam("a_oct", "A Octave", -2, 2, 0));
    addParam(intParam("a_uni", "A Unison", 1, 3, 1));
    addParam(floatParam("a_det", "A Detune", 0, 40, 9, "ct", 1));
    addParam(choiceParam("b_table", "B Table", noxed::Wavetables::names(), 1));
    addParam(floatParam("b_pos", "B Position", 0, 100, 25, "%", 1));
    addParam(floatParam("b_level", "B Level", 0, 100, 45, "%", 0));
    addParam(intParam("b_oct", "B Octave", -2, 2, 0));
    addParam(intParam("b_semi", "B Semi", -12, 12, 0, "st"));
    addParam(intParam("b_uni", "B Unison", 1, 3, 1));
    addParam(floatParam("b_det", "B Detune", 0, 40, 12, "ct", 1));
    addParam(floatParam("sub", "Sub Level", 0, 100, 15, "%", 0));
    addParam(floatParam("noise", "Noise Level", 0, 100, 0, "%", 0));
    addParam(floatParam("width", "Width", 0, 100, 75, "%", 0));
    addParam(logParam("cutoff", "Cutoff", 30, 18000, 6000, "Hz", 0));
    addParam(floatParam("res", "Resonance", 0, 100, 15, "%", 0));
    addParam(choiceParam("mode", "Filter Mode", {"LP", "BP", "HP", "Notch"}));
    addParam(floatParam("envamt", "Env>Filter", -100, 100, 20, "%", 0));
    addParam(floatParam("keytrack", "Key Track", 0, 100, 50, "%", 0));
    addParam(logParam("attack", "Amp Attack", 1, 5000, 5, "ms", 0));
    addParam(logParam("decay", "Amp Decay", 5, 5000, 400, "ms", 0));
    addParam(floatParam("sustain", "Amp Sustain", 0, 100, 75, "%", 0));
    addParam(logParam("release", "Amp Release", 5, 5000, 180, "ms", 0));
    addParam(logParam("eattack", "Env2 Attack", 1, 5000, 5, "ms", 0));
    addParam(logParam("edecay", "Env2 Decay", 5, 5000, 600, "ms", 0));
    addParam(floatParam("esustain", "Env2 Sustain", 0, 100, 0, "%", 0));
    addParam(logParam("erelease", "Env2 Release", 5, 5000, 200, "ms", 0));
    for (int l = 0; l < 2; ++l) {
      std::string id = l ? "l2" : "l1", name = l ? "LFO2 " : "LFO1 ";
      addParam(choiceParam((id + "shape").c_str(), (name + "Shape").c_str(),
                           LFO::shapeNames()));
      addParam(logParam((id + "rate").c_str(), (name + "Rate").c_str(), .03f,
                        30, l ? .27f : 1, "Hz", 2));
      addParam(
          boolParam((id + "sync").c_str(), (name + "Sync").c_str(), false));
      addParam(choiceParam((id + "div").c_str(), (name + "Div").c_str(),
                           divisionNames(), kDivisionQuarter));
    }
    for (int r = 0; r < 4; ++r) {
      std::string id = "mod" + std::to_string(r + 1),
                  n = "M" + std::to_string(r + 1) + " ";
      addParam(choiceParam(
          (id + "src").c_str(), (n + "Source").c_str(),
          {"Off", "LFO1", "LFO2", "Env2", "Velocity", "Wheel", "Aftertouch"},
          r == 0 ? 1 : 0));
      addParam(
          choiceParam((id + "dst").c_str(), (n + "Target").c_str(),
                      {"A Pos", "B Pos", "Cutoff", "Pitch", "Pan", "Level"},
                      r == 0 ? 0 : 2));
      addParam(floatParam((id + "amt").c_str(), (n + "Amount").c_str(), -100,
                          100, r == 0 ? 15 : 0, "%", 0));
    }
    addParam(floatParam("drive", "Drive", 0, 100, 0, "%", 0));
    addParam(floatParam("chorus", "Chorus Mix", 0, 100, 0, "%", 0));
    addParam(floatParam("delay", "Delay Mix", 0, 80, 0, "%", 0));
    addParam(choiceParam("delaydiv", "Delay Div", divisionNames(),
                         kDivisionQuarter));
    addParam(floatParam("feedback", "Feedback", 0, 85, 35, "%", 0));
    addParam(intParam("bend", "Bend Range", 0, 24, 2, "st"));
    auto pr = [&](const char *n,
                  std::initializer_list<std::pair<int, float>> edits) {
      std::vector<float> v;
      for (int p = 0; p < Count; ++p)
        v.push_back(param(p).def);
      for (auto x : edits)
        v[x.first] = x.second;
      addProgram(n, v);
    };
    pr("NOXED Init", {});
    pr("Wide Horizon", {{AUnison, 3},
                        {BUnison, 3},
                        {ATable, 0},
                        {APos, 80},
                        {Attack, 650},
                        {Release, 1600},
                        {Cutoff, 2200},
                        {Chorus, 25},
                        {DelayMix, 20},
                        {M1, 30}});
    pr("Spectral Pluck", {{ATable, 1},
                          {APos, 40},
                          {BLevel, 15},
                          {Sustain, 0},
                          {Decay, 360},
                          {EnvAmt, 65},
                          {Cutoff, 500},
                          {DelayMix, 25}});
    pr("Talking Bass", {{ATable, 2},
                        {APos, 35},
                        {BOct, -1},
                        {Cutoff, 1400},
                        {Resonance, 40},
                        {S1, 3},
                        {D1, 0},
                        {M1, 70},
                        {Drive, 25}});
    pr("Digital Glass", {{ATable, 3},
                         {BTable, 1},
                         {BOct, 1},
                         {BLevel, 30},
                         {Sustain, 20},
                         {Release, 850},
                         {DelayMix, 30},
                         {Sub, 0}});
    pr("User 01 Scan", {{ATable, 4},
                        {BLevel, 0},
                        {M1, 45},
                        {L1Rate, .3f},
                        {Cutoff, 14000},
                        {EnvAmt, 0}});
    pr("Slow Motion", {{ATable, 2},
                       {BTable, 3},
                       {AUnison, 3},
                       {Attack, 1200},
                       {Release, 2200},
                       {S1, 1},
                       {M1, 40},
                       {S2, 2},
                       {D2, 2},
                       {M2, 30},
                       {Chorus, 35}});
    pr("Neon Lead", {{ATable, 1},
                     {APos, 30},
                     {AUnison, 3},
                     {ADetune, 7},
                     {BLevel, 20},
                     {Drive, 20},
                     {DelayMix, 20},
                     {Cutoff, 4200}});
  }
  bool formatValue(int p, float value, std::string &out) override {
    if (p != ATable && p != BTable)
      return false;
    int b = mpc::clamp(int(std::lround(value)), 0, 11);
    out = param(p).choices[b];
    if (b >= 4 && !tables_.banks[b].imported)
      out += " (Basic fallback)";
    return true;
  }
  void prepare(double sr, int) override {
    sr_ = float(sr);
    for (auto &v : voices_) {
      v.amp.setSampleRate(sr);
      v.env.setSampleRate(sr);
      for (auto &f : v.filter)
        f.setSampleRate(sr);
    }
    for (auto &l : lfo_)
      l.setSampleRate(sr);
    for (auto &d : delay_)
      d.prepare(int(sr * 4) + 2);
    for (auto &d : chorus_)
      d.prepare(int(sr * .06) + 2);
    gain_.setTime(12, sr);
    delayTime_.setTime(50, sr);
    for (int p = 0; p < Count; ++p)
      smooth_[p].setTime(12, sr);
  }
  void reset() override {
    for (auto &v : voices_) {
      v.amp.reset();
      v.env.reset();
      for (auto &f : v.filter)
        f.reset();
      v.ctrl = 0;
    }
    alloc_.reset();
    alloc_.setPitchBend(0);
    wheel_ = after_ = 0;
    for (auto &l : lfo_)
      l.reset();
    for (auto &d : delay_)
      d.reset();
    for (auto &d : chorus_)
      d.reset();
    for (int p = 0; p < Count; ++p)
      smooth_[p].snap(get(p));
    gain_.snap(dbToGain(get(Master)));
    delayTime_.snap(sr_ * .5f);
    chorusPhase_ = 0;
  }
  void process(mpc::ProcessContext &c) override {
    for (int p = 0; p < Count; ++p) {
      s_[p] = get(p);
      smooth_[p].setTarget(s_[p]);
    }
    for (auto &v : voices_) {
      v.amp.set(s_[Attack], s_[Decay], s_[Sustain] * .01f, s_[Release]);
      v.env.set(s_[EAttack], s_[EDecay], s_[ESustain] * .01f, s_[ERelease]);
    }
    alloc_.setBendRange(s_[Bend]);
    gain_.setTarget(dbToGain(s_[Master]));
    for (int l = 0; l < 2; ++l) {
      int p = l ? L2Shape : L1Shape;
      lfo_[l].setShape(int(s_[p]));
      if (s_[p + 2] > .5f) {
        lfo_[l].setTempoSync(c.transport.bpm, divisionBeats(int(s_[p + 3])));
        if (c.transport.playing && c.transport.ppqValid)
          lfo_[l].syncToPpq(c.transport.ppqPos);
      } else
        lfo_[l].setFrequency(s_[p + 1]);
    }
    delayTime_.setTarget(
        clamp(float(beatsToSeconds(divisionBeats(int(s_[DelayDiv])),
                                   c.transport.bpm)) *
                  sr_,
              1.0f, sr_ * 3.95f));
    int e = 0;
    for (int n = 0; n < c.numFrames; ++n) {
      while (e < c.numMidiEvents && c.midi[e].sampleOffset <= n)
        handle(c.midi[e++]);
      for (int p = 0; p < Count; ++p)
        v_[p] = smooth_[p].process();
      float lf1 = lfo_[0].process(), lf2 = lfo_[1].process();
      float left = 0, right = 0;
      for (int vi = 0; vi < 8; ++vi) {
        auto &v = voices_[vi];
        if (!v.amp.isActive())
          continue;
        float amp = v.amp.process(), env = v.env.process();
        if ((v.ctrl++ & 15) == 0) {
          float mod[6]{};
          float source[] = {0, lf1, lf2, env, v.velocity, wheel_, after_};
          for (int r = 0; r < 4; ++r) {
            int p = S1 + r * 3;
            mod[int(s_[p + 1])] += source[int(s_[p])] * v_[p + 2] * .01f;
          }
          v.pos[0] = clamp(v_[APos] * .01f + mod[0], 0.0f, 1.0f);
          v.pos[1] = clamp(v_[BPos] * .01f + mod[1], 0.0f, 1.0f);
          v.pan = clamp(mod[4], -1.0f, 1.0f);
          v.level = clamp(1 + mod[5], 0.0f, 2.0f);
          float hz = alloc_.noteToHz(float(v.note) + mod[3] * 12);
          v.subInc = clamp(hz * .5f / sr_, 0.0f, .45f);
          for (int o = 0; o < 2; ++o) {
            int up = o ? BUnison : AUnison;
            int count = int(s_[up]);
            float oct = s_[o ? BOct : AOct] * 12 + (o ? s_[BSemi] : 0);
            for (int u = 0; u < count; ++u) {
              float spread = count == 1 ? 0 : 2.0f * u / (count - 1) - 1;
              float f = clamp(
                  hz * semitonesToRatio(
                           oct + spread * v_[o ? BDetune : ADetune] / 100),
                  1.0f, sr_ * .45f);
              v.inc[o][u] = f / sr_;
              v.mip[o][u] = noxed::Wavetables::mip(f, sr_);
            }
          }
          float oct = env * v_[EnvAmt] * .06f + mod[2] * 6 +
                      (v.note - 60) * v_[KeyTrack] / 1200;
          float fc = clamp(v_[Cutoff] * std::exp2(clamp(oct, -10.0f, 10.0f)),
                           20.0f, 18000.0f);
          for (auto &f : v.filter)
            f.set(fc, clamp(v_[Resonance] * .009f, 0.0f, .9f));
        }
        float vl = 0, vr = 0;
        for (int o = 0; o < 2; ++o) {
          const auto &bank = tables_.get(int(s_[o ? BTable : ATable]));
          int count = int(s_[o ? BUnison : AUnison]);
          float lev = v_[o ? BLevel : ALevel] * .01f / count;
          for (int u = 0; u < count; ++u) {
            float x = bank.sample(v.phase[o][u], v.pos[o], v.mip[o][u]) * lev;
            v.phase[o][u] += v.inc[o][u];
            v.phase[o][u] -= int(v.phase[o][u]);
            float pan = (count == 1 ? 0 : 2.0f * u / (count - 1) - 1) *
                        v_[Width] * .01f;
            vl += x * (1 - pan) * .5f;
            vr += x * (1 + pan) * .5f;
          }
        }
        float sub = WavetableSine::lookup(v.subPhase) * v_[Sub] * .01f;
        v.subPhase += v.subInc;
        v.subPhase -= int(v.subPhase);
        float noise = noise_.process() * v_[Noise] * .004f;
        vl += sub * .5f + noise;
        vr += sub * .5f + noise;
        float g = amp * (.35f + .65f * v.velocity) * v.level * .32f;
        left += v.filter[0].process(vl, int(s_[Mode])) * g * (1 - v.pan);
        right += v.filter[1].process(vr, int(s_[Mode])) * g * (1 + v.pan);
        if (!v.amp.isActive())
          alloc_.voiceFinished(vi);
      }
      float d = 1 + v_[Drive] * .08f;
      left = fastTanh(left * d);
      right = fastTanh(right * d);
      float cp = WavetableSine::lookup(chorusPhase_);
      chorusPhase_ += .23f / sr_;
      if (chorusPhase_ >= 1)
        chorusPhase_ -= 1;
      float cl = chorus_[0].readLinear(sr_ * (.018f + .004f * cp)),
            cr = chorus_[1].readLinear(sr_ * (.021f - .004f * cp));
      chorus_[0].write(left);
      chorus_[1].write(right);
      left = lerp(left, cl, v_[Chorus] * .005f);
      right = lerp(right, cr, v_[Chorus] * .005f);
      float dt = delayTime_.process(), dl = delay_[0].readLinear(dt),
            dr = delay_[1].readLinear(dt);
      delay_[0].write(fastTanh(left + dr * v_[Feedback] * .01f));
      delay_[1].write(fastTanh(right + dl * v_[Feedback] * .01f));
      float g = gain_.process();
      c.outputs[0][n] = fastTanh(lerp(left, dl, v_[DelayMix] * .01f)) * g;
      if (c.numOutputs > 1)
        c.outputs[1][n] = fastTanh(lerp(right, dr, v_[DelayMix] * .01f)) * g;
    }
  }

private:
  struct Voice {
    ADSR amp, env;
    SVF filter[2];
    int note = 60, ctrl = 0;
    float velocity = 1, phase[2][3]{}, inc[2][3]{}, pos[2]{}, subPhase = 0,
          subInc = 0, pan = 0, level = 1;
    int mip[2][3]{};
  };
  void release(int v) {
    voices_[v].amp.noteOff();
    voices_[v].env.noteOff();
  }
  void handle(const mpc::MidiEvent &e) {
    if (e.isNoteOn()) {
      int i = alloc_.noteOn(e.note(), e.velocity());
      auto &v = voices_[i];
      v.note = e.note();
      v.velocity = e.velocity() / 127.0f;
      v.ctrl = 0;
      if (!v.amp.isActive()) {
        for (int o = 0; o < 2; ++o)
          for (int u = 0; u < 3; ++u)
            v.phase[o][u] = float((i * 173 + o * 317 + u * 631) % 2048) / 2048;
        v.subPhase = 0;
        for (auto &f : v.filter)
          f.reset();
      }
      v.amp.noteOn();
      v.env.noteOn();
    } else if (e.isNoteOff()) {
      int v = alloc_.noteOff(e.note());
      if (v >= 0)
        release(v);
    } else if (e.isAllNotesOff())
      alloc_.allNotesOff([&](int v) { release(v); });
    else if (e.isPitchBend())
      alloc_.setPitchBend(e.pitchBend());
    else if (e.isController() && e.controller() == 64)
      alloc_.setSustain(e.controllerValue() >= 64, [&](int v) { release(v); });
    else if (e.isController() && e.controller() == 1)
      wheel_ = e.controllerValue() / 127.0f;
    else if (e.isAftertouch())
      after_ = e.data[1] / 127.0f;
  }
  noxed::Wavetables tables_;
  Voice voices_[8];
  VoiceAllocator<8> alloc_;
  LFO lfo_[2];
  WavetableSine sineInit_;
  WhiteNoise noise_;
  DelayLine delay_[2], chorus_[2];
  Smoother gain_, delayTime_, smooth_[Count];
  float sr_ = 44100, s_[Count]{}, v_[Count]{}, wheel_ = 0, after_ = 0,
        chorusPhase_ = 0;
};
MPC_PLUGIN_ENTRY(PrismWave)
