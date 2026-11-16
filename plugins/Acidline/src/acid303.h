// Open303 (MIT, Robin Schmidt) adapter. No MIDI list allocation in audio callbacks.
#pragma once
#include <climits>
#include <cstring>
#include <climits>
#include <cstring>
#include "open303/GlobalFunctions.cpp"
#include "open303/rosic_Complex.cpp"
#include "open303/rosic_NumberManipulations.cpp"
#include "open303/rosic_RealFunctions.cpp"
#include "open303/rosic_FunctionTemplates.cpp"
#include "open303/rosic_FourierTransformerRadix2.cpp"
#include "open303/rosic_MipMappedWaveTable.cpp"
#include "open303/rosic_BlendOscillator.cpp"
#include "open303/rosic_OnePoleFilter.cpp"
#include "open303/rosic_BiquadFilter.cpp"
#include "open303/rosic_TeeBeeFilter.cpp"
#include "open303/rosic_AnalogEnvelope.cpp"
#include "open303/rosic_DecayEnvelope.cpp"
#include "open303/rosic_LeakyIntegrator.cpp"
#include "open303/rosic_EllipticQuarterBandFilter.cpp"
#include "open303/rosic_MidiNoteEvent.cpp"
#include "open303/rosic_AcidPattern.cpp"
#include "open303/rosic_AcidSequencer.cpp"
#include "open303/rosic_Open303.cpp"
class Acid303 : public rosic::Open303 {
public:
  void trigger(int note, bool accented, bool slide) {
    controlCounter = 0;
    if(slide) slideToNote(note,accented); else triggerNote(note,accented);
  }
  void release(){ampEnv.noteOff();}
  void resetVoice(){
    ampEnv.reset();filter.reset();highpass1.reset();highpass2.reset();
    allpass.reset();notch.reset();antiAliasFilter.reset();ampDeClicker.reset();
    rc1.reset();rc2.reset();oscillator.resetPhase();controlCounter=0;idle=true;
  }
};
