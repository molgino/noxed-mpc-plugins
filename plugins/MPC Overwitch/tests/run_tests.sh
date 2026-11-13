#!/bin/sh
# Desktop tests for MPC Overwitch (no hardware needed). Run from the project folder.
set -e
mkdir -p /tmp/fakesys/1-1
printf '1935\n' > /tmp/fakesys/1-1/idVendor; printf '0b34\n' > /tmp/fakesys/1-1/idProduct
printf '1\n' > /tmp/fakesys/1-1/busnum; printf '4\n' > /tmp/fakesys/1-1/devnum
g++ -O2 -std=c++17 -Isrc tests/sim_stream.cpp -o /tmp/ow_sim && /tmp/ow_sim | tail -1
g++ -O2 -std=c++17 -pthread -Isrc tests/engine_smoke.cpp -o /tmp/ow_smoke -ldl && /tmp/ow_smoke | tail -1
g++ -O2 -std=c++17 -fPIC -shared -fvisibility=hidden -DLIB -Isrc tests/two_libs.cpp -o /tmp/ow_liba.so -pthread
g++ -O2 -std=c++17 -fPIC -shared -fvisibility=hidden -DLIB -Isrc tests/two_libs.cpp -o /tmp/ow_libb.so -pthread
g++ -O2 -std=c++17 tests/two_libs.cpp -o /tmp/ow_two -ldl -pthread && /tmp/ow_two /tmp/ow_liba.so /tmp/ow_libb.so
