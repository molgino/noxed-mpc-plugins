# NOXED MPC USB Bridge 0.2.1 – a USB audio device as a plugin track (Roland S-1 & co., preview)

Brings a class-compliant USB audio device that the MPC does not use as its interface (first the **Roland AIRA Compact S-1**, also T-8, J-6 and similar) into the MPC as an instrument plugin track. The plugin records the device through the MPC's own ALSA library (`libasound`, loaded at run time) and converts the audio to the MPC's rate with the same drift-correcting resampler as MPC Overwitch.

**Setup (S-1):** switch **AIRA LINK off** on the S-1, connect the S-1 to the MPC by USB, load the plugin "MPC USB Bridge" on a track. After about a second STATUS shows **STREAMING**.

| Control | Meaning |
|---|---|
| LEVEL (Q-Link 13) | Output level |
| CARD (Q-Link 14) | **Auto** = first USB audio device that is not the MPC (Roland preferred), or a fixed ALSA card |
| REPORT (Q-Link 15) | Scrolls through the USB/ALSA report (as in 0.1) |
| MIDI CHANNEL (Q-Link 16) | Off or channel 1–16, default 1 (the S-1 receives on channel 1 out of the box) |

| Row | Meaning |
|---|---|
| STATUS | STREAMING / Streaming (buffering) / Waiting / ERROR with the reason |
| DEVICE | ALSA name and USB ID of the device, plus the MIDI status after the "\|" |
| STREAM | Format, device rate (set and measured) → MPC rate, period size |
| LEVELS | Peak level, latency, drift in ppm, dropouts, overruns |
| SYSTEM | Processor, user ID, kernel |

**MIDI to the device:** notes, CCs, pitch bend, aftertouch and program changes that the MPC track sends to this plugin go straight to the device's USB MIDI port, on the channel set under **MIDI CHANNEL** (Off = send nothing). The MPC itself cannot pass MIDI on from plugins, so the plugin opens the device's MIDI port itself: directly first, and through the ALSA sequencer if the MPC holds the port. The DEVICE row shows after the "|" whether it works ("MIDI ch 1 -> hw:2,0", "MIDI: port busy …", "MIDI: device has no MIDI port"). Held notes are released when you change the channel, stop the transport or remove the plugin. Timing: at most one audio block plus 1 ms delay (a few ms). **Tip:** if the device is also set as a track's MIDI output in the MPC's own MIDI settings, notes arrive twice; switch one of them off. Tested on the computer only.

**What works and what doesn't (0.2):**
- Audio only from the device into the MPC (stereo; a mono device is copied to both sides), not back yet. MIDI goes to the device (since 0.2.1).
- A device can only be opened by **one** plugin instance at a time; a second instance shows "ERROR: open: Device or resource busy".
- Unplugging and replugging works: the plugin keeps searching on its own.
- Latency about 5–15 ms. Low CPU: one capture thread plus the resampler.
- **Parameter change from 0.1:** LEVEL and CARD are new and the order changed. Reload a track that used the 0.1 test version. From 0.2 on, IDs and order stay fixed.
- Audio streaming and MIDI have not been tested on the MPC yet, only on the computer (see below).

## Report from 0.1 (still included)

REPORT scrolls through all USB devices with their interfaces and drivers, all ALSA cards with formats, channels and rates, which audio devices are in use, and whether the ALSA library is available in the MPC process. The full report is written to `MPC-USB-Bridge-Report.txt` in the plugin folder (updated every 10 s).

## How to test (S-1)

1. Set **AIRA LINK to OFF** on the S-1: [SHIFT] + pad [15] (MENU), choose "A.Lnk", OFF. That is what the Roland manual says for connecting to computers and other devices.
2. Connect the S-1 to a USB host port of the MPC with a USB-C cable.
3. Load MPC USB Bridge as a plugin program and look at the **USB** page.
4. Bonus: set AIRA LINK to ON, replug and look again. The report then shows how the S-1 presents itself in that mode.

## Report verdicts

| Verdict | Meaning |
|---|---|
| No USB audio device found | No USB audio device recognised as a sound card (cable, AIRA LINK, power) |
| Roland without audio: AIRA LINK on? | Roland device present, but without an audio interface |
| Seen, but cannot open audio | Card present, but the plugin is not allowed to open it (permissions) |
| Busy: card in use | The card is already in use (e.g. by the MPC itself) |
| PROMISING: audio can be opened | Best case: the bridge can very likely work |

## AIRA LINK

AIRA LINK is an undocumented Roland protocol for audio and tempo over USB between Roland devices (hosts include the MX-1 mixer and the DJ-808). It is not used: audio and MIDI run in normal USB mode (AIRA LINK OFF), and tempo comes from the MPC as MIDI clock.

## Result on the MPC Live III (15 Nov 2026)

Roland S-1 with AIRA LINK off: its own sound card "S1", capture and playback with 2 channels S32_LE each at **44.1 kHz** (not 48 kHz), both can be opened by the plugin; libasound is already loaded in the MPC process. Verdict: "PROMISING: audio can be opened". So the bridge with audio is feasible. Because the S-1 offers 44.1 kHz, the MPC may also be able to use it directly as its audio interface; the plugin stays useful because the MPC only uses one interface at a time.

## Tests on the computer

`sh tests/run_tests.sh` (Linux with libasound): the capture engine records an ALSA "file" device that plays a 1 kHz test tone at 44.1 kHz / S32 and reads it out in real time at 48 kHz. It checks left/right level, frequency and dropouts (result: 1000.2 Hz, 0 dropouts, ~10 ms latency). Also runs in GitHub Actions.

The report was checked with a recreated `/proc/asound` (variable `USB_BRIDGE_ASOUND`) and a sample S-1 description; SDK host test passed (the warning about silence is expected without a device).

Licence: GPL-3.0-or-later.

Requirements as for all NOXED plugins: MPC OS 3.10.1 or newer, with "Enable Enhancements" and "External Plugin Support" switched on under Preferences → Enhancements (Beta).
