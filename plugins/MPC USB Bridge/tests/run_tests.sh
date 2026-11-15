#!/bin/sh
# Desktop tests of MPC USB Bridge (Linux with libasound.so.2). The capture engine records an ALSA
# "file" device that plays a generated 1 kHz test tone (44.1 kHz, S32 stereo), paced in real time.
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
python3 -c "
import math,struct,sys
with open(sys.argv[1],'wb') as f:
    f.write(b''.join(struct.pack('<ii',int(0.5*math.sin(2*math.pi*1000*i/44100)*2**31),int(0.25*math.sin(2*math.pi*1000*i/44100)*2**31)) for i in range(44100*10)))
" "$T/in.raw"
cat > "$T/.asoundrc" <<CONF
pcm.ubtest { type file slave.pcm "null" file "/dev/null" infile "$T/in.raw" format "raw" }
CONF
${CXX:-g++} -std=c++17 -O2 -I../../../framework capture_test.cpp -o "$T/capture_test" -lpthread -ldl
HOME="$T" USB_BRIDGE_PCM=ubtest USB_BRIDGE_TEST_PACE=1 "$T/capture_test"
rm -rf "$T"
