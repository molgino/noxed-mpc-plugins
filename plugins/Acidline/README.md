# NOXED Acidline 0.3.0

Monophonic acid synth for Akai MPC standalone. Original TB-303-inspired sound engine; not a circuit-accurate emulation, no Roland software and no original samples.

## Playing

- Notes come from the MPC sequencer, pads or a keyboard. Optional built-in 16-step sequencer, synchronized to the MPC transport. Switch SEQUENCER on on any SEQ page; press MPC Play. With it off, MIDI plays normally.
- Velocity at or above **Accent Vel** (default 100) turns on accent for a newly struck note. Accent lifts the VCA and the filter envelope.
- For **slide**, let MIDI notes overlap and switch on **Legato Slide**. The envelopes are not retriggered; accent is kept from the previous note.
- When you release the newest note, the instrument returns to the last note still held. Pitch bend range is adjustable. No sustain pedal.

## Preview version: hardware testing pending

The SEQUENCE page uses a 303-style **two-pass entry workflow**, rather than a step table:

1. Select **PITCH MODE**. Enter up to 16 pitches on the 13-key keyboard; input advances automatically. DOWN/NORMAL/UP shifts the input octave. Returning to PITCH MODE starts checking at the first pitch; TAP auditions that pitch, subsequent taps advance. BACK moves back. ACCENT and SLIDE edit the pitch currently indicated by the write cursor.
2. Select **TIME MODE**. Enter NOTE, TIE or REST for each sixteenth. NOTE consumes the next recorded pitch; TIE extends the previous note and REST inserts silence. Ties and rests do not consume pitches. The number of entered timing cells defines pattern length (1–16).
3. Return to **NORMAL**, enable **SEQ ON / OFF**, and press MPC Play. The keyboard auditions short notes while the internal sequencer is disabled. CLEAR clears the current pattern. This version has one pattern, no song/track chaining or original 303 real-time tap recording.

The small numbered lamps indicate the **write cursor**, not the playback position. Touching one jumps to that entry. These touch shortcuts and host clock synchronization adapt the workflow to MPC; this is not a claim of every original TB-303 key combination being reproduced. Q-Links expose timing, pitch count and sound model; SETUP also exposes Sound Model, Pattern Length and Gate.

Patterns from 0.2.0 keep the original playback mapping until PITCH MODE or TIME MODE is entered. That action converts active steps into a separate pitch sequence and note/rest timing. Existing parameter IDs/order remain unchanged; editor and rhythm parameters are appended. Save a copy of an old project before editing its pattern.

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
