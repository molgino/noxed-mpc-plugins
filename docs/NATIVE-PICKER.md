# Native MPC choice list limitation

The user reports a left-hand choice list with missing labels in Prism Wave on MPC.
Our desktop host simulator does not draw that host-owned list; the earlier transparent-wrapper change was not a verified fix.

Independent hardware evidence: [Open MPC SDK-PORTING.md](https://github.com/open-mpc/mpc-vst-plugins/blob/33d99c6a17f6b804cd5f710e6978243d6f3d827f/docs/SDK-PORTING.md), section "Choice pick-lists" and the 2026-11-15 entry in NOTES.md. A Key 37 on MPC OS 3.10.1.12 showed blank native MenuOverlay rows even with replacementStrings exactly as documented by the SDK. A drawn radio-button picker is reported working on a Live III in that project.

Prism Wave 0.1.4 replaces this native widget with original PNG captions, radio buttons bound to the actual sound parameter, an explicit close button, and an appended UI-only open-state parameter. Sound IDs and indices 0–55 are preserved. Close notifications occur on a worker thread, not in audio processing. Reset/state restore closes the picker.

Offline verification: generated skin validation, explicit-open rendering, routing and same-value selection tests, host simulator and audio regression tests. This is not a claim of a new MPC hardware test. Overwitch's native lists have not been converted in this change.
