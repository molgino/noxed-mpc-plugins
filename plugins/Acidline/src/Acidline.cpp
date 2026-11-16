// NOXED Acidline: PolyBLEP oscillator -> nonlinear three-pole resonant lowpass
// (2x processing) -> accent VCA -> drive/DC blocker -> smoothed master.
// Optional Open303 model: 303 waveforms -> TB filter / accent envelope -> VCA.
// Internal host-synced 16-step sequencer or external MIDI. No claim of circuit-exact equivalence.
// SPDX-License-Identifier: MIT
#include "dsp/adsr.h"
#include "dsp/onepole.h"
#include "dsp/oscillator.h"
#include "dsp/smoother.h"
#include "mpc_plugin.h"
#include <algorithm>
#include "acid303.h"
#include "acid_editor.h"
#include <thread>
#include <chrono>
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
    SeqOn, SeqLength, SeqGate, StepFirst,
    Engine = StepFirst + 16 * 4,
    Count = AcidPatternEditor::Count
  };
  static mpc::PluginInfo makeInfo() {
    mpc::PluginInfo i;
    i.name = "Acidline";
    i.vendor = "NOXED";
    i.uniqueId = MPC_FOURCC('N', 'c', 'e', 'p');
    i.version = 300;
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
    addParam(boolParam("seq_on", "Sequencer", false));
    addParam(intParam("seq_length", "Pattern Length", 1, 16, 16));
    addParam(floatParam("seq_gate", "Step Gate", 10, 95, 55, "%", 0));
    const int pattern[16] = {36,36,43,36,48,36,39,43,36,48,43,39,36,43,46,48};
    for (int step = 0; step < 16; ++step) {
      const std::string id = "step_" + std::to_string(step + 1);
      const std::string name = "Step " + std::to_string(step + 1);
      addParam(intParam((id + "_note").c_str(), (name + " Note").c_str(), 24, 72, pattern[step]));
      addParam(boolParam((id + "_on").c_str(), (name + " Play").c_str(), true));
      addParam(boolParam((id + "_accent").c_str(), (name + " Accent").c_str(), step % 4 == 0));
      addParam(boolParam((id + "_slide").c_str(), (name + " Slide").c_str(), false));
    }
    addParam(choiceParam("engine", "Sound Model", {"Original", "303 Model"}, 0));
    addParam(choiceParam("edit_mode", "Write Mode", {"Normal", "Pitch Mode", "Time Mode"}, 0));
    addParam(intParam("edit_cursor", "Write Position", 1, 16, 1));
    addParam(intParam("edit_octave", "Octave Shift", -1, 1, 0));
    addParam(boolParam("pattern_303", "303 Pattern", false));
    addParam(intParam("pitch_count", "Pitch Notes", 1, 16, 16));
    for(int i=0;i<16;++i){
      std::string id="time_"+std::to_string(i+1),name="Time "+std::to_string(i+1);
      addParam(choiceParam(id.c_str(),name.c_str(),{"Note","Tie","Rest"},0));
    }
    const char *keys[]={"C","Cs","D","Ds","E","F","Fs","G","Gs","A","As","B","C_hi"};
    for(auto key:keys){
      std::string id="key_"+std::string(key),name="Key "+std::string(key);
      auto command=boolParam(id.c_str(),name.c_str(),false);command.automatable=false;addParam(command);
    }
    for(auto key:{"back","tap","time_note","time_tie","time_rest","clear","accent","slide"}){
      std::string id="edit_"+std::string(key);auto command=boolParam(id.c_str(),key,false);
      command.automatable=false;addParam(command);
    }
    auto readout=intParam("edit_display","Pattern Readout",0,1,0);readout.automatable=false;addParam(readout);
    refreshThread_=std::thread([this]{while(!stopRefresh_.load()){
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
      if(editorDirty_.exchange(false)){
        notifyHostParameterChanged(AcidPatternEditor::Display);
        notifyHostParameterChanged(AcidPatternEditor::Cursor);
        requestDisplayUpdate();
      }
    }});
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
  ~Acidline() override {
    stopRefresh_.store(true);if(refreshThread_.joinable())refreshThread_.join();
  }
  void parameterChanged(int index,float) override {editor_.changed(index);}
  void prepare(double sr, int) override {
    sr_ = float(sr);
    model_.setSampleRate(sr);
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
    editor_.reset();auditionSamples_=0;auditionKey_=-1;
    model_.resetVoice();
    amp_.reset();
    env_.reset();
    std::fill(z_, z_ + 3, 0);
    std::fill(held_, held_ + 128, 0u);
    seqStep_ = INT64_MIN;
    seqActive_ = seqHeld_ = seqSlide_ = false;
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
    if(editor_.consume(*this))editorDirty_.store(true);
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
    const bool seq = s_[SeqOn] > .5f;
    const bool running = seq && c.transport.playing && c.transport.ppqValid &&
                         c.transport.tempoValid && std::isfinite(c.transport.ppqPos) && std::abs(c.transport.ppqPos) < 1e12 &&
                         std::isfinite(c.transport.bpm) && c.transport.bpm > 0;
    if (seq != seqActive_ || (!running && seqStep_ != INT64_MIN)) {
      std::fill(held_, held_ + 128, 0u);
      note_ = -1;
      amp_.noteOff(); env_.noteOff(); model_.release();
      seqStep_ = INT64_MIN; seqHeld_ = seqSlide_ = false;
    }
    seqActive_ = seq;
    const double stepsPerSample = running ? c.transport.bpm / (60.0 * sr_) * 4 : 0;
    const bool model = s_[Engine] > .5f;
    if (model != modelActive_) {std::fill(held_, held_ + 128, 0u); note_ = -1; model_.resetVoice(); amp_.noteOff(); env_.noteOff(); model_.release(); seqStep_ = INT64_MIN; seqHeld_ = seqSlide_ = false;}
    modelActive_ = model;
    if(model) {
      model_.setCutoff(s_[Cutoff]); model_.setResonance(s_[Resonance]);
      model_.setEnvMod(s_[EnvAmount]); model_.setDecay(s_[Decay]);
      model_.setAccent(s_[Accent]); model_.setSlideTime(s_[Slide]);
      model_.setAmpRelease(s_[Release]); model_.setWaveform(s_[Waveform]);
      model_.setTuning(440.0 * std::exp2(s_[Tune] / 12.0));
      model_.setVolume(0); // Original 303 waveform geometry stays fixed; no table rebuild in audio.
      model_.setPitchBend(bend_ * s_[BendRange]);
    }
    if(editor_.audition>=0){
      if(!seq){
        if(auditionKey_>=0)handle(mpc::MidiEvent::noteOff(0,auditionKey_));
        auditionKey_=editor_.audition;auditionSamples_=int(sr_*.15f);
        handle(mpc::MidiEvent::noteOn(0,auditionKey_,100));
      }
      editor_.audition=-1;
    }
    int e = 0;
    for (int n = 0; n < c.numFrames; ++n) {
      while (e < c.numMidiEvents && c.midi[e].sampleOffset <= n) {
        const auto &event = c.midi[e++];
        if (!seq || event.isAllNotesOff() || event.isPitchBend()) handle(event);
      }
      if(auditionSamples_>0 && --auditionSamples_==0){
        handle(mpc::MidiEvent::noteOff(0,auditionKey_));auditionKey_=-1;
      }
      if (running) {
        const double position = c.transport.ppqPos * 4 + n * stepsPerSample;
        const auto absolute = static_cast<int64_t>(std::floor(position));
        const int length = int(s_[SeqLength]);
        const int step = int((absolute % length + length) % length);
        if (absolute != seqStep_) {
          const bool classic=s_[AcidPatternEditor::Style]>.5f;
          int pitchSlot=step;
          if(classic){pitchSlot=0;for(int k=0;k<step;++k)if(s_[AcidPatternEditor::TimeFirst+k]<.5f)++pitchSlot;
            pitchSlot%=int(s_[AcidPatternEditor::PitchCount]);}
          const int base = StepFirst + pitchSlot * 4;
          const int timing=classic?int(s_[AcidPatternEditor::TimeFirst+step]):0;
          const bool tie=classic && timing==1;
          const bool play=classic?timing==0:s_[base+1]>.5f;
          if(tie) { /* Hold the preceding pitch; do not consume or retrigger a pitch note. */ }
          else if (play) {
            if (seqHeld_ && seqSlide_) pitch_.setTarget(s_[base]);
            else {
              pitch_.snap(s_[base]);
              accent_ = s_[base + 2] > .5f ? 1.0f : 0.0f;
              amp_.noteOn(); env_.noteOn();
            }
            if(model) model_.trigger(int(s_[base]), s_[base + 2] > .5f, seqHeld_ && seqSlide_);
            ctrl_ = 0;
          } else { amp_.noteOff(); env_.noteOff(); model_.release(); }
          if(!tie){
            seqHeld_ = play;
            seqSlide_ = play && s_[base + 3] > .5f &&
                (classic?s_[AcidPatternEditor::TimeFirst+(step+1)%length]<1.5f:
                 s_[StepFirst+((step+1)%length)*4+1]>.5f);
          }
          seqStep_ = absolute;
        }
        const bool extends=s_[AcidPatternEditor::Style]>.5f && int(s_[AcidPatternEditor::TimeFirst+(step+1)%length])==1;
        if (seqHeld_ && !seqSlide_ && !extends && position - std::floor(position) >= s_[SeqGate] * .01) {
          amp_.noteOff(); env_.noteOff(); model_.release(); seqHeld_ = false;
        }
      }
      if(model) {
        float sample = float(model_.getSample());
        const float d = drv_.process();
        sample = dc_.process(fastTanh(sample * (1 + d * 6))) * gain_.process();
        c.outputs[0][n] = sample;
        if(c.numOutputs > 1) c.outputs[1][n] = sample;
        continue;
      }
      float pitch = pitch_.process(), a = amp_.process(), f = env_.process();
      float cutoff = cut_.process(), r = res_.process(), d = drv_.process();
      if ((ctrl_++ & 7) == 0) {
        osc_.setFrequency(
            mpc::noteToHz(pitch + s_[Tune] + bend_ * s_[BendRange]));
        float oct = f * (s_[EnvAmount] * .065f + accent_ * s_[Accent] * .018f) +
                    (pitch - 48) * s_[KeyTrack] / 1200;
        float hz = mpc::dsp::clamp(cutoff * std::exp2(oct), 25.0f, 14000.0f);
        float g = std::tan(kPi * hz / (sr_ * 2));
        g_ = g / (1 + g);
      }
      float y = 0;
      for (int os = 0; os < 2; ++os) {
        float x = fastTanh(osc_.process() * .55f - (r * 7.7f) * last_);
        for (int p = 0; p < 3; ++p) {
          float v = (x - z_[p]) * g_;
          x = v + z_[p];
          z_[p] = mpc::dsp::clamp(x + v, -4.0f, 4.0f);
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

  bool formatValue(int index, float value, std::string &out) override {
    if(index==AcidPatternEditor::Display){
      int cursor=getInt(AcidPatternEditor::Cursor)-1,mode=getInt(AcidPatternEditor::Mode);
      if(mode==2){
        static const char *time[]={"NOTE","TIE","REST"};
        out="TIME "+std::to_string(cursor+1)+"   "+time[getInt(AcidPatternEditor::TimeFirst+cursor)];
      }else{
        int key=getInt(StepFirst+cursor*4);static const char *notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        out="PITCH "+std::to_string(cursor+1)+"   "+notes[key%12]+std::to_string(key/12-1);
        if(getBool(StepFirst+cursor*4+2))out+="   ACCENT";
        if(getBool(StepFirst+cursor*4+3))out+="   SLIDE";
      }return true;
    }
    if(index >= StepFirst && index < Engine && (index - StepFirst) % 4 == 0) {
      static const char *names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
      const int key = std::max(0, std::min(127, int(std::lround(value))));
      out = std::string(names[key % 12]) + std::to_string(key / 12 - 1);
      return true;
    }
    return false;
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
      if(modelActive_) model_.trigger(note_, e.velocity() >= s_[Threshold], slide);
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
          model_.release();
        } else {
          pitch_.setTarget(float(note_));
          if(modelActive_) model_.trigger(note_, accent_ > .5f, true);
        }
      }
    } else if (e.isAllNotesOff()) {
      std::fill(held_, held_ + 128, 0u);
      note_ = -1;
      amp_.noteOff();
      env_.noteOff();
      model_.release();
    } else if (e.isPitchBend()) {
      bend_ = e.pitchBend() / 8192.0f;
      if(modelActive_) model_.setPitchBend(bend_ * s_[BendRange]);
    }
  }
  AcidPatternEditor editor_;
  std::atomic<bool> stopRefresh_{false},editorDirty_{false};
  std::thread refreshThread_;
  int auditionSamples_=0,auditionKey_=-1;
  Acid303 model_;
  bool modelActive_ = false;
  PolyBlepOsc osc_;
  ADSR amp_, env_;
  Smoother pitch_, gain_, cut_, res_, drv_;
  OnePoleHP dc_;
  float sr_ = 44100, s_[Count]{}, z_[3]{}, last_ = 0, g_ = .05f, accent_ = 0,
        bend_ = 0;
  unsigned held_[128]{}, serial_ = 0, ctrl_ = 0;
  int64_t seqStep_ = INT64_MIN;
  bool seqActive_ = false, seqHeld_ = false, seqSlide_ = false;
  int note_ = -1;
};
MPC_PLUGIN_ENTRY(Acidline)
