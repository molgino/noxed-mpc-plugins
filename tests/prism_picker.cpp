// Explicit picker state: field routing, selection, close, state restore and stable DSP IDs.
#include "mpc_plugin.h"
#undef MPC_PLUGIN_ENTRY
#define MPC_PLUGIN_ENTRY(Class)
#include "../plugins/Prism Wave/src/PrismWave.cpp"
#include <cassert>
#include <cstdio>
int main(){
  PrismWave p;p.prepare(44100,512);p.reset();
  assert(PrismWave::UiOpen==56 && p.numParams()==57);
  for(int choice:{PrismWave::ATable,PrismWave::BTable,PrismWave::L1Shape,PrismWave::L1Div,PrismWave::S1,PrismWave::D1,PrismWave::DelayDiv}){
    p.set(PrismWave::UiOpen,choice+1);
    const float before=p.get(PrismWave::Master);
    p.set(choice,p.param(choice).max);
    assert(p.getInt(PrismWave::UiOpen)==0 && p.get(PrismWave::Master)==before);
    p.set(PrismWave::UiOpen,choice+1);p.set(choice,p.get(choice));
    assert(p.getInt(PrismWave::UiOpen)==0); // Selecting the current value also closes.
  }
  p.set(PrismWave::UiOpen,2);auto state=p.internalSaveState();
  p.set(PrismWave::UiOpen,0);assert(p.internalLoadState(state.data(),state.size()));p.reset();
  assert(p.getInt(PrismWave::UiOpen)==0);
  std::puts("PASS explicit picker field routing, same-value selection, independent sound parameters and closed-on-restore");
}
