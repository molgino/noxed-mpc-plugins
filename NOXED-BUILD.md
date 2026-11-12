# NOXED 0.1.0 source projects

Original SDK revision: febd2ca (2026-11-07). NOXED changes are in plugins/Acidline,
plugins/Prism Wave, tests/noxed_audio.cpp and tools/build_noxed_windows.py.
Build with Python 3.9+ using the standard SDK commands:

    python tools/build.py plugins/Acidline
    python tools/build.py "plugins/Prism Wave"

Run the SDK host_sim tests as documented in docs/install-and-test.md.
The WAV importer is available in C++ and Python source next to the Prism project.
The C++ importer and functional test harness can be compiled with a normal
C++17 compiler and -Iframework (on Windows, -static with LLVM-MinGW).
Set MPC_PLUGIN_FOLDER to the absolute plugins/Prism Wave path when running
the test harness and pass two output WAV paths for the optional musical demos.

Windows release builds used prebuilt Arm GNU 15.2 cross compilers and LLVM-MinGW
20261104. The optional tools/build_noxed_windows.py accepts --llvm-dir,
--arm32-dir and --arm64-dir pointing at their extracted root folders. Toolchains
are not bundled. ARM binaries require glibc >=2.38; the SDK documents 2.39 on MPC OS 3.

Do not change plugin uid strings, parameter identifiers or parameter order.
Test reports are included under tests/reports. No physical MPC was tested.
