# NOXED MPC Overwitch Send 0.4.0 – MPC audio to the Elektron device (preview)

Effect plugin that goes with [MPC Overwitch](../MPC%20Overwitch/README.md). As an insert on any MPC track it sends that track's audio over Overbridge to an input of the Elektron device. On the Digitone II these are Main, Delay, Reverb or Chorus, so you can also feed the Digitone's effect sends. The conversion from the MPC rate to 48 kHz follows the clock drift, just like in the receive direction.

Send and MPC Overwitch share one USB connection: both plugins can run at the same time, up to 16 instances together. Several sends to the same DESTINATION are added together.

**Status:** not tested on the MPC yet. Please install **both plugins in the same version**; otherwise STATUS shows "version mismatch: update both plugins".

## Controls

- **DESTINATION** (double-tap): input on the device – Main, Delay, Reverb, Chorus (Digitone II). Other Overbridge devices show their own inputs.
- **SEND:** level to the device (default −1.9 dB).
- **DRY:** what stays on the MPC track (default 0 dB). Turn it fully down if the track should only be heard through the device.
- **STATUS / DEVICE / STREAM:** connection, device, buffer latency and dropouts of the send direction.
- **LEVELS:** `IN` = peak level arriving at the plugin (after SEND); `USB` = what the engine actually mixes to the device. If IN shows "silent", there is no signal on the track. If IN shows a level and USB shows "silent", the problem is in the engine. If both show a level and nothing arrives at the device, it is the device (routing/settings) or the encoding.

Q-Links: Destination, Send, Dry.

## How to test

1. Digitone II in Overbridge mode, connected to the MPC by USB.
2. Put MPC Overwitch Send as an insert on a track with material, DESTINATION e.g. **Reverb**.
3. Listen to the effect: through a plugin track with MPC Overwitch (SOURCE **Main** or **Reverb**) or through the Digitone's audio outputs.
4. Watch out for feedback: do not put MPC Overwitch Send on a track that runs MPC Overwitch. The returned audio would go back to the device and form a loop.

## Technical notes

- The audio thread only writes to a lock-free ring buffer. The shared USB engine (one thread per MPC process) reads it at 48 kHz, corrects drift, mixes all sends and encodes the Overbridge blocks (24-bit on Overbridge 2.1).
- The send direction is checked in simulation: no dropouts at −150…+120 ppm, MPC block sizes 32–512 and timing jitter up to 1.5 ms. Latency is about 6–18 ms depending on the block size.
- The code is shared with MPC Overwitch (`plugins/MPC Overwitch/src/ob_*.h`). Protocol after [Overwitch](https://github.com/dagargo/overwitch) (`engine.c`, MIT), device table from Overwitch (GPL-3).

Licence: GPL-3.0-or-later.

Requirements as for all NOXED plugins: MPC OS 3.10.1 or newer, with "Enable Enhancements" and "External Plugin Support" switched on under Preferences → Enhancements (Beta).
