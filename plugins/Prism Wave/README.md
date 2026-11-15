# NOXED Prism Wave 0.1.2

Original wavetable synth for Akai MPC standalone, inspired by the workflow of modern wavetable synthesizers. Not a full Serum 2 clone. No Xfer code and no bundled Serum wavetables, samples or presets.

## Sound engine

- Eight polyphonic voices, each with two wavetable oscillators of 1–3 unison voices, detune, octave, level and frame morphing.
- Four built-in banks: Basic Morph, Spectral FM, Formants, Digital; eight WAV import slots.
- Frame and phase interpolation; ten FFT-filtered mip levels limit high harmonics. Fully alias-free output under every modulation is not guaranteed.
- Sine sub, white noise, stereo SVF with LP/BP/HP/notch and key tracking.
- Amp ADSR and Env2, two global LFOs with tempo sync, four modulation routes.
- Modulation sources: LFO1, LFO2, Env2, velocity, mod wheel, channel aftertouch. Destinations: A/B position, cutoff, pitch, pan, level. Amount is bipolar. Env2 also modulates the filter via Env>Filter.
- Drive, stereo chorus and tempo-synced ping-pong delay. No reverb, no sample/granular player, no graphical wavetable editor, no MPE and no Serum preset import.
- Sustain pedal and pitch bend. Eight built-in programs including **User 01 Scan**.

## Interface (since 0.1.1)

Own dark skin in the style of modern software wavetable synths: panels with header bars, arc knobs with a cyan/violet gradient, selector fields and displays. All graphics are drawn from scratch; no logos, graphics or fonts from Xfer, Vital or other manufacturers. Five pages:

- **OSC:** OSC A and OSC B side by side. Header with wavetable selector; below it a 3D wavetable display that follows **Position** (swipe to change the position). Knobs Position, Octave, Level; below them Unison, Detune, Sub, Noise (A) or Semi, Unison, Detune, Width (B).
- **FILTER:** filter curve that follows **Cutoff** and mode (LP/BP/HP/NOTCH buttons), Cutoff, Resonance, Env 2 Amt; VOICE with Key Track, Bend Range, Master; Distortion (Drive), Chorus (Mix), Delay (Mix, Division, Feedback).
- **ENV:** ENV 1 (amplitude) and ENV 2 (modulation) with a curve display (follows the sustain level; times are shown on the knobs) and four A/D/S/R knobs each.
- **LFO:** per LFO a shape display, shape selector, Rate (free) or Division (with SYNC on) in the same place, and a SYNC switch.
- **MATRIX:** four rows of source → bipolar amount bar → destination.

The Master knob is also at the top right of every page. Selector fields open their list on double-tap or encoder press; tapping an element activates the matching Q-Link row. Q-Links work in rows of four (top hardware row = upper knob group). In Program mode A/B Position, Cutoff, Resonance, Env 2 Amt, Drive, Chorus, Delay, A/B Level, Sub, Noise and the amp ADSR sit on the 16 Q-Links.

The displays for User 02–08 show a neutral layered picture because imported tables are only loaded on the MPC. User 01 shows the bundled demo table; if you replace it, regenerate the display with `tools/make_prismwave_skin.py`.

The displays are film strips with 21 steps; the sound position itself is continuous.

The skin files live in `skins/` and are copied unchanged by the build. To regenerate them (needs Pillow and numpy):

```text
python tools/make_prismwave_skin.py --params "plugins/Prism Wave/build/params.json"
```

`ui.json` is kept as a simple fallback but is not used as long as `skins/` exists.

## Importing your own wavetables

The import is **prepared on your computer**; there is no file picker on the MPC. The plugin reads the slots when an instance is created. After changing files, remove and re-add existing instances, and restart the MPC if needed.

1. Prepare a WAV wavetable whose consecutive frames each contain one wave cycle, typically 2048 samples per frame. Ordinary songs or one-shot samples are not split into cycles automatically.
2. On **64-bit Windows**, double-click the included **import_wavetable.exe**. It asks for the WAV path, slot, frame length and the unzipped plugin folder on the drive. No Python installation needed. Or from a terminal (scriptable):

```text
import_wavetable.exe "MyTable.wav" --slot 2 --frame-size 2048 --plugin-folder "E:/VST2 Plugins/NOXED - VST - Prism Wave"
```

On macOS/Linux, or with Python 3.9+, use the cross-platform tool:

```text
python import_wavetable.py "MyTable.wav" --slot 2 --frame-size 2048 --plugin-folder "E:/VST2 Plugins/NOXED - VST - Prism Wave"
```

The drive letter is an example. The target path must point at the unzipped plugin folder, not at VST/ or Content/. To overwrite a slot that is already in use, add **--replace**.

3. Load or reload the plugin on the MPC. Choose **User 02** in **A Table** or **B Table** and move **Position**.

Accepted input: mono/stereo PCM WAV (8/16/24/32 bit) or float32 WAV. Stereo is mixed to mono. Give the frame length with --frame-size (32–16384); there is no automatic detection. The tool removes DC, normalises each frame and resamples it to 2048 samples. Up to 64 frames are kept; with 65–1024 input frames, 64 evenly spaced frames are chosen. Single-cycle WAVs work with their matching frame length. WAV wavetables from other synths only work if they use this cyclic format and you are allowed to use them. No .SerumPreset/.fxp/.vstpreset files.

Internally this produces **Content/Wavetables/user01.wav** to **user08.wav**. User 01 holds an original NOXED demo table; User 02–08 start empty. Missing or invalid slots play Basic Morph, and the parameter display shows **Basic fallback**. Imported files are converted to band-limited tables on load. That costs load time and, with all eight slots filled, about 40 MB of table RAM per instance plus working data.

**Projects store the slot number and position, not the WAV file.** Back up the slot files together with your projects. Replacing a slot later changes the sound of existing projects. Keep your own content files when updating. Without the computer tool, only ready-made mono WAVs with 2048 samples per frame and at most 64 frames may be copied into the slots directly.

## Installation and test status

MPC OS **3.10.1 or newer**, with **Enable Enhancements** and **External Plugin Support** switched on under Preferences > Enhancements (Beta). Copy **NOXED - VST - Prism Wave** unchanged into **VST2 Plugins** at the root of the drive. Load it as an instrument under External Plugins. ARM32/ARM64 are included; the MPC Live III uses ARM64.

Versions 0.1.1 and 0.1.2 only change the interface; sound, plugin ID, parameter order and presets are unchanged (host test audio byte-identical to 0.1.0). Restart the MPC after replacing the plugin folder. The skin was checked on an MPC Live III: the OSC, ENV, LFO and MATRIX pages show all values and the SYNC switches toggle. 0.1.2 fixes the filter mode buttons, which had no captions and did not respond; that fix has not been checked on hardware yet. CPU load and load time on the MPC have not been measured. Reduce unison when running many instances. Plugins run without crash protection; back up your projects before the first test.

## Licence

Original NOXED code and demo wavetable: MIT. Uses the MIT-licensed inMusic MPC Plugin Builder SDK; its LICENSE is in the source package. Serum/Xfer belong to their respective owners; no affiliation or endorsement.
