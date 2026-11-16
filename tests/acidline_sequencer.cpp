// Offline sequencer regression: transport, rests, block invariance, state.
#include "mpc_plugin.h"
#undef MPC_PLUGIN_ENTRY
#define MPC_PLUGIN_ENTRY(Class)
#include "../plugins/Acidline/src/Acidline.cpp"
#include <cassert>
#include <cstdio>
#include <vector>
static bool inAudio = false;
static size_t audioAllocations = 0;
void *operator new(std::size_t size) {
  if(inAudio) ++audioAllocations;
  if(void *p=std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc();
}
void *operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete[](void *p) noexcept {std::free(p);}
void operator delete(void *p,std::size_t) noexcept {std::free(p);}
void operator delete[](void *p,std::size_t) noexcept {std::free(p);}
static std::vector<float> render(Acidline &p, int block, bool playing=true, double start=0) {
  std::vector<float> result(44100); float l[512]{}, r[512]{};float *out[]={l,r};
  for(int offset=0;offset<44100;offset+=block){
    mpc::ProcessContext c;c.outputs=out;c.numOutputs=2;c.numFrames=std::min(block,44100-offset);c.sampleRate=44100;
    c.transport.valid=c.transport.ppqValid=c.transport.tempoValid=true;c.transport.playing=playing;c.transport.bpm=120;c.transport.ppqPos=start+offset/44100.0*2;
    inAudio=true;p.process(c);inAudio=false;assert(audioAllocations==0);for(int i=0;i<c.numFrames;++i){assert(std::isfinite(l[i]));result[offset+i]=l[i];}
  }return result;
}
static double energy(const std::vector<float>&v){double e=0;for(auto x:v)e+=x*x;return e;}
int main(){
  Acidline a,b;a.prepare(44100,512);b.prepare(44100,512);a.reset();b.reset();
  assert(energy(render(a,32))==0); // default: external MIDI only
  a.set(Acidline::SeqOn,1);b.set(Acidline::SeqOn,1);a.reset();b.reset();
  auto x=render(a,32),y=render(b,512);assert(energy(x)>.01);
  for(size_t i=0;i<x.size();++i)assert(std::abs(x[i]-y[i])<1e-5);
  assert(energy(render(a,128,false))<energy(x)*.2); // release on transport stop
  a.reset();assert(energy(render(a,128,false))==0);
  for(int step=0;step<16;++step)a.set(Acidline::StepFirst+step*4+1,0);
  a.reset();assert(energy(render(a,128))==0);
  a.set(Acidline::StepFirst+1,1);a.set(Acidline::SeqLength,1);a.reset();assert(energy(render(a,128,true,-2))>.01);
  a.set(Acidline::SeqOn,0);render(a,128);a.reset();assert(energy(render(a,128))==0);
  a.set(Acidline::Engine,1);a.set(Acidline::SeqOn,1);a.reset();
  auto model=render(a,32);assert(energy(model)>.01);
  a.reset();auto model512=render(a,512);
  for(size_t i=0;i<model.size();++i)assert(std::abs(model[i]-model512[i])<1e-5);
  a.reset();assert(energy(render(a,128,false))==0);
  a.set(Acidline::SeqOn,0);a.reset();assert(energy(render(a,128))==0);
  a.set(Acidline::StepFirst,43);a.set(Acidline::StepFirst+2,1);a.set(Acidline::StepFirst+3,1);
  const auto state=a.internalSaveState();
  a.set(Acidline::StepFirst,24);a.set(Acidline::Engine,0);
  assert(a.internalLoadState(state.data(),state.size()));
  assert(a.getInt(Acidline::StepFirst)==43 && a.getInt(Acidline::Engine)==1);
  assert(a.getBool(Acidline::StepFirst+2) && a.getBool(Acidline::StepFirst+3));
  // Record two pitches, then four independent timing cells, just as a two-pass 303 write.
  a.set(Acidline::SeqOn,0);a.reset();
  auto press=[&](int key){a.set(key,1-a.getInt(key));render(a,128,false);};
  press(AcidPatternEditor::Clear);
  a.set(AcidPatternEditor::Mode,1);render(a,128,false);
  press(AcidPatternEditor::KeyFirst);press(AcidPatternEditor::KeyFirst+7);
  assert(a.getInt(AcidPatternEditor::PitchCount)==2);
  assert(a.getInt(Acidline::StepFirst)==36 && a.getInt(Acidline::StepFirst+4)==43);
  press(AcidPatternEditor::Back);press(AcidPatternEditor::Accent);press(AcidPatternEditor::Slide);
  assert(a.getBool(Acidline::StepFirst+6) && a.getBool(Acidline::StepFirst+7));
  a.set(AcidPatternEditor::Mode,2);render(a,128,false);
  press(AcidPatternEditor::Note);press(AcidPatternEditor::Tie);press(AcidPatternEditor::Rest);press(AcidPatternEditor::Note);
  assert(a.getInt(Acidline::SeqLength)==4 && a.getInt(AcidPatternEditor::PitchCount)==2);
  for(int i=0;i<4;++i)assert(a.getInt(AcidPatternEditor::TimeFirst+i)==(i==1?1:i==2?2:0));
  const auto pattern=a.internalSaveState();
  a.set(Acidline::SeqOn,1);a.reset();auto classic32=render(a,32);
  a.reset();auto classic512=render(a,512);
  assert(energy(classic32)>.01);
  for(size_t i=0;i<classic32.size();++i)assert(std::abs(classic32[i]-classic512[i])<1e-5);
  assert(a.internalLoadState(pattern.data(),pattern.size()));a.reset();render(a,128,false);
  assert(a.getInt(Acidline::SeqLength)==4 && a.getInt(AcidPatternEditor::PitchCount)==2);
  std::puts("PASS 303 pitch/time entry, accent/slide, independent rhythm, block invariance and safe state restore");
  std::puts("PASS saved pattern/model state and zero audio-thread allocations");
  std::puts("PASS Open303 playback, block invariance, stop and disable");
  std::puts("PASS sequencer default off, playback, block invariance, stop, rests, negative PPQ, disable");
}
