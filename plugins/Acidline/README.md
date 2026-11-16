# NOXED Acidline 0.2.0

Monophonic acid synth for Akai MPC standalone. Original TB-303-inspired sound engine; not a circuit-accurate emulation, no Roland software and no original samples.

## Playing

- Notes come from the MPC sequencer, pads or a keyboard. Optional built-in 16-step sequencer, synchronized to the MPC transport. Switch SEQUENCER on on any SEQ page; press MPC Play. With it off, MIDI plays normally.
- Velocity at or above **Accent Vel** (default 100) turns on accent for a newly struck note. Accent lifts the VCA and the filter envelope.
- For **slide**, let MIDI notes overlap and switch on **Legato Slide**. The envelopes are not retriggered; accent is kept from the previous note.
- When you release the newest note, the instrument returns to the last note still held. Pitch bend range is adjustable. No sustain pedal.

## Local test version (not published)

Four SEQ banks edit steps 1–4, 5–8, 9–12 and 13–16. Each step has Note (C1–C5), Play/Rest, Accent and Slide; a slide ties into the next active step without retriggering. Pattern Length is 1–16; Step Gate is 10–95%. Timing is straight sixteenths. The 16-key strip identifies the bank being edited, not a live playhead. Step parameters are saved and automatable. Stop, bypassing the sequencer and switching models release the voice. The sequencer needs host playing, valid tempo and PPQ; no standalone free-run.

SOUND MODEL selects **Original** (the existing engine, default for compatibility) or **303 Model** (Open303 by Robin Schmidt, MIT). The latter includes its 303 saw/square tables, TB-303 filter mode with feedback high-pass, measured envelope mapping, accent shaping and slide. Select it explicitly before comparing. Drive remains an optional NOXED output stage; Pulse Width and Key Track belong to the Original engine; the 303 Model keeps its original waveform geometry. Filter coefficients update every eight samples; envelope and audio states run at full rate. Start with Drive = 0 for model comparisons.

This is a circuit-informed DSP model, **not a verified component-by-component circuit simulation**. No claim of exact TB-303 equivalence: hardware/reference comparisons of oscillator, cutoff/resonance, decay, accent and slide, plus CPU measurement on MPC, remain pending. The silver panel evokes the 303; editing is adapted to the MPC rather than copying its original button-entry workflow.

## Controls

**BASSLINE:** silver hardware-style panel with the six classic tone controls: Tuning, Cut Off Freq, Resonance, Env Mod, Decay, Accent. Below them Volume, Drive, Slide Time, Release, waveform buttons (saw/square) and a legato switch with status LED. Knobs and value fields respond to touch and Q-Links; double-tap opens the large value editor.

**SETUP:** Pulse Width, Accent Velocity, Key Track, Bend Range. Release is directly on the main page.

Since 0.1.1: own silver panel graphics, knurled black film-strip knobs, ivory buttons and orange accents. All graphics are drawn from scratch; no manufacturer images. On the main page the first Q-Link group controls Tuning/Cutoff/Resonance/Env Mod, the second Decay/Accent/Waveform/Legato and the third Volume/Drive/Slide/Release. Sound, presets, parameter order and plugin ID are unchanged from 0.1.0, so existing projects keep working.

Normal builds copy **skins/**; ui.json is kept only as a simple alternative layout. To regenerate the graphics: `python tools/make_acidline_skin.py --params plugins/Acidline/build/params.json` (needs Pillow). The preview is rendered from the generated TUI component tree and the same PNGs; it is not a screenshot from the device.

Signal flow: PolyBLEP oscillator → non-linear resonant three-pole low-pass (2× oversampled) → VCA/accent → drive/DC filter → master. Six built-in presets: NOXED Acid, Rubber Square, Warehouse, Clean Sub, Sharp Screamer, Liquid.

## Installation

MPC OS **3.10.1 or newer**. Copy the folder **NOXED - VST - Acidline** unchanged into **VST2 Plugins** at the root of the drive. Switch on **Enable Enhancements** and **External Plugin Support** under Preferences > Enhancements (Beta). Load the instrument under External Plugins. ARM32 and ARM64 are included; the MPC Live III uses ARM64.

## Status and limits

Version 0.1.0 was tried on an MPC by the author. The 0.1.1 skin is validated on the computer; touch and Q-Link operation still need checking on the MPC. Host simulator and function tests are listed in the test report. Faithfulness to the TB-303 and CPU load on hardware have not been measured. High resonance is deliberately aggressive. Start at a low level on the MPC; a plugin error can close the MPC application. Save and reload a project before using the instrument for real work.

## Updating from 0.1.0

Replace the folder **NOXED - VST - Acidline** with the new version, then restart the MPC so the changed skins are loaded. Same plugin name and ID; old projects keep using Acidline. MPC OS 3.10.1+ with Enable Enhancements / External Plugin Support must stay switched on.

## Source and licence

NOXED wrapper and sequencer, MIT. The optional 303 Model uses Open303 DSP, MIT, Copyright (c) 2009 Robin Schmidt; see src/open303/UPSTREAM.md for the pinned revision and local fixes. Its licence is included in Content/Open303-LICENSE.txt in the test package. Uses the MIT-licensed inMusic MPC Plugin Builder SDK (separate LICENSE in the source package). Roland and TB-303 are trademarks of their respective owners; no affiliation or endorsement.
