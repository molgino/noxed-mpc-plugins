// Desktop test of ob_midi.h: messages are re-channelled, held notes are released on channel change and on
// stop, Off sends nothing, and everything reaches the port (OB_MIDI_DEV points the writer at a file).
#include "../src/ob_midi.h"
#include <cstdio>
#include <vector>

static std::vector<uint8_t> readFile(const char* p)
{
    std::vector<uint8_t> v;
    if (FILE* f = std::fopen(p, "rb")) { int c; while ((c = std::fgetc(f)) != EOF) v.push_back((uint8_t)c); std::fclose(f); }
    return v;
}

int main()
{
    const char* path = std::getenv("OB_MIDI_DEV");
    if (!path) { std::printf("FAIL set OB_MIDI_DEV\n"); return 1; }
    std::remove(path);
    {
        obmidi::Forwarder fw([] { return -1; });
        fw.start();
        for (int i = 0; i < 300 && fw.status(0).find("->") == std::string::npos; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const uint8_t on[3] = {0x92, 60, 100}, cc[3] = {0xB5, 74, 64}, pb[3] = {0xE0, 0, 64}, pc[2] = {0xC3, 7}, off[3] = {0x80, 60, 0};
        fw.process(on, 3, 0);      // ch 3 in -> ch 1 out
        fw.process(cc, 3, 0);
        fw.process(pb, 3, 0);
        fw.process(pc, 2, 0);
        fw.process(on, 3, 0);
        fw.process(on, 3, 4);      // channel change: note 60 released on ch 1, new note on ch 5
        fw.process(off, 3, -1);    // Off: releases note on ch 5, sends nothing itself
        fw.process(on, 3, -1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::printf("status: %s, sent %u\n", fw.status(0).c_str(), fw.sent());
    }
    const std::vector<uint8_t> want = {0x90, 60, 100, 0xB0, 74, 64, 0xE0, 0, 64, 0xC0, 7, 0x90, 60, 100,
                                       0x80, 60, 0, 0x94, 60, 100, 0x84, 60, 0};
    const std::vector<uint8_t> got = readFile(path);
    std::printf("got %zu bytes:", got.size());
    for (uint8_t b : got) std::printf(" %02x", b);
    std::printf("\n%s\n", got == want ? "PASS midi_test" : "FAIL midi_test");
    return got == want ? 0 : 1;
}
