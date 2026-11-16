Open303 DSP sources, MIT, Copyright (c) 2009 Robin Schmidt.
Upstream: https://github.com/RobinSchmidt/Open303/tree/313bf0d9ade7c1dcb6b3a74f5ea1780a29d70074/Source/DSPCode
Only DSP code is included; no proprietary SDK, plugin wrapper or artwork.
Local portability changes: explicit std::list; memcpy exponent extraction avoids strict-aliasing UB;
headers include climits/cstring via acid303.h; source line endings normalized.
The adapter calls protected trigger/slide methods directly, bypassing the allocating MIDI list.
The internal upstream sequencer stays off; Acidline supplies host-synchronized steps.
Local memory fix: initPrototypeTable clears exactly tableLength entries (upstream wrote four beyond the array).
Portability: exp10 helper renamed to avoid libc collision; wavetable FFT scratch made instance-local; sampleRate member updated in setter.
MPC real-time adaptation: no pulse-width table regeneration in process; cutoff coefficient calculation at 1/8 rate; envelope states and 4x audio oversampling unchanged.
