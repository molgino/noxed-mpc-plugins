# NOXED MPC Overwitch 0.5.1 – groups and MIDI (preview)

MPC Overwitch brings the audio of an Elektron device into the MPC over **Overbridge**. The Digitone II is tested first; the device table also has Digitakt II, Digitakt, Digitone (Keys), Syntakt, Analog Four/Rytm MKII and Analog Heat. The device runs at 48 kHz; the plugin converts to the MPC's sample rate (44.1 kHz) and continuously corrects the small clock difference between the two devices. That removes the sample-rate problem of plain USB audio.

It is based on the open-source project [Overwitch](https://github.com/dagargo/overwitch) by David García Goñi. Its USB protocol (`engine.c`, MIT, Stefan Rehm and David García Goñi) was reimplemented for the MPC, and the device table is generated from Overwitch's `devices.json` (GPL-3) by `tools/gen_overwitch_devices.py`. No Elektron code, graphics or software is used. Licence of this plugin: GPL-3.0-or-later.

**Status:** streaming from the device works on an MPC Live III with a Digitone II (13 Nov 2026): STREAMING, measured 48.00 kHz, about 8 ms buffer, drift correction within a few ppm, 0 dropouts, 0 USB errors, USB thread at real-time priority; the audio arrives cleanly on the plugin track. Longer sessions, CPU load, the send direction, MIDI and the other Elektron devices have not been checked on hardware yet.

## MIDI to the device (0.5.0)

Notes, CCs, pitch bend, aftertouch and program changes that the MPC track sends to this plugin go straight to the device's USB MIDI port, on the channel set under **MIDI CHANNEL** (Off = send nothing). The MPC itself cannot pass MIDI on from plugins, so the plugin opens the device's MIDI port itself: directly first, and through the ALSA sequencer if the MPC holds the port. The DEVICE row shows after the "|" whether it works ("MIDI ch 1 -> hw:2,0", "MIDI: port busy …", "MIDI: device has no MIDI port"). Held notes are released when you change the channel, stop the transport or remove the plugin. Timing: at most one audio block plus 1 ms delay (a few ms). **Tip:** if the device is also set as a track's MIDI output in the MPC's own MIDI settings, notes arrive twice; switch one of them off. Tested on the computer only.

The default is **Off**, so existing projects and multiple instances do not send anything twice. With several instances each track can send on a different channel (e.g. Digitone II tracks 1–16). MPC Overwitch Send is an effect and gets no MIDI from the MPC.

## Controls

- **SOURCE** (double-tap opens the list): which device output to play – Main, Track 1–16, Delay, Reverb, Chorus, Input (Digitone II names; other devices show their own outputs, entries that do not exist stay silent).
- **LEVEL:** output level (default −1.9 dB, maximum +1 dB).
- **MIDI CHANNEL:** Off or 1–16, see above.
- **STATUS:** Searching… → Connecting… → Streaming (buffering) → **STREAMING**. Other messages: "Set device USB: Overbridge", "Unsupported Elektron device", "Error: …", "Busy: other instance has it".
The main screen shows only routing, level, MIDI channel, connection status and device.
Diagnostic parameters remain available internally for project compatibility but are hidden from the skin and Q-Links.

**Groups (since 0.4):** on the second page, **GROUP**, you switch Track 1–16, Delay, Reverb, Chorus and Input on or off. With SOURCE set to **Group**, all switched-on outputs are summed to one stereo signal – for example tracks 1–4 as drums on plugin track 1 and tracks 5–8 as synths on track 2 (one instance per group). The SOURCE field shows the group, e.g. "Group: T1+T2+T3". On the GROUP page tracks are arranged in two rows of eight, with Delay/Reverb/Chorus/Input underneath; the 16 Q-Links follow tracks 1–16. Per-track levels inside a group are not available yet.

Several tracks: create one MPC plugin track with MPC Overwitch for each device track you want, and choose its SOURCE there (up to 16 together with Send instances).

Q-Links: first row Source, Level and MIDI Channel, no diagnostic Q-Link. In Program mode Q-Link 1 is Level and Q-Link 2 is Source.

## How to test

1. Set the Digitone II's USB configuration to **Overbridge** and connect it to the MPC by USB.
2. Load MPC Overwitch as a plugin program. After a second or two STATUS should show **STREAMING**.
3. Play the Digitone; the audio arrives on the plugin track. Change SOURCE to hear single tracks.
4. Note down STATUS, STREAM and DROPOUTS (or take a photo), especially if it crackles or stays silent.

## Technical notes

- Its own engine talks directly to the MPC's Linux USB interface (`/dev/bus/usb`, usbfs); no extra library is needed. Interfaces 1 and 2, alt setting 3, interrupt transfers of 24 blocks × 7 frames.
- The USB work runs in its own thread (at real-time priority when allowed), never in the audio thread. The audio thread only reads a lock-free ring buffer.
- Resampler: 4-point Hermite interpolation with an adaptive ratio. It keeps the buffer at about 6–17 ms (depending on the MPC block size) and corrects by at most ±0.2 % (inaudible). If the buffer runs dry, the output is muted briefly, refilled and the safety margin is increased.
- **Audio to the device (since 0.3):** with the effect plugin **MPC Overwitch Send** (separate plugin folder). Without a Send instance, silence goes to the device.
- Unplugging the cable or changing the USB mode: the plugin goes silent and reconnects automatically.
- **Multiple instances (since 0.3):** up to 16 instances of MPC Overwitch and MPC Overwitch Send share one USB connection. Each instance has its own SOURCE, e.g. track 1 on plugin track 1, track 2 on plugin track 2. The libraries find each other through a small anchor file in `/dev/shm`, are protected against unloading, and different versions of the two plugins refuse to share with "version mismatch".
- CPU: conversion for two channels plus decoding; not yet measured on the MPC, expected well under one percent of a core.

## Tests on the computer

- `tests/sim_stream.cpp`: simulated USB stream with clock drift −150…+200 ppm, jitter up to 2.5 ms, MPC block sizes 32–512, host rates 44.1 and 48 kHz: no dropouts, clean sine playback.
- `tests/sim_stream.cpp` also checks the send direction (MPC blocks → USB transfers at 48 kHz with drift).
- `tests/engine_smoke.cpp`: device table, block sizes (Digitone II: 1012/256 bytes), sources and destinations, 24-bit decoding, shared engine with 16 slots, error path without a device.
- `tests/two_libs.cpp`: two separately loaded plugin libraries share one engine, even after the library that created it is unloaded.
- `tests/midi_test.cpp`: MIDI re-channelling, release of held notes, Off sends nothing.
- Run everything with `sh tests/run_tests.sh`; it also runs in GitHub Actions.
- SDK host test: passed. Without a device the output is silent; the warning about that is expected.

## History

- **0.5.0:** MIDI CHANNEL (parameter 28, Q-Link 15): notes and CCs of the track go to the Elektron device's USB MIDI port.
- **0.4.1:** GROUP pads switch off again and have captions (found on hardware).
- **0.4.0:** groups: source "Group" sums the outputs chosen on the GROUP page. Both Overwitch plugins must be 0.4.0.
- **0.3.1:** level meter in the Send plugin; both plugins must be 0.3.1.
- **0.3.0:** up to 16 instances at once, each with its own source; shared USB connection with the new effect MPC Overwitch Send (audio to the device). Not tested on hardware yet.
- **0.2.1:** the STREAM row updates as soon as the first rate measurement is available (before, "0.00 kHz" stayed on screen).
- **0.2.0:** Overbridge streaming from the device to the MPC, source selection, level, drift correction. Tested successfully on an MPC Live III with a Digitone II.
- **0.1.1:** status rows update by themselves.
- **0.1.0:** USB test. Result on an MPC Live III with a Digitone II in Overbridge mode (13 Nov 2026): device found (1935:0b34), Overbridge interface free, access as root, claim successful.

Parameters may still change before 1.0; new ones are only ever added at the end.

Requirements as for all NOXED plugins: MPC OS 3.10.1 or newer, with "Enable Enhancements" and "External Plugin Support" switched on under Preferences → Enhancements (Beta).
