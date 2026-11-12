// NOXED standalone Windows wavetable importer; no Python needed. SPDX-License-Identifier: MIT
// Build: clang++ -std=c++17 -O2 -static -Iframework "plugins/Prism Wave/import_wavetable.cpp" -o import_wavetable.exe
#include "dsp/wav.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs = std::filesystem;
static std::string unquote(std::string s) {
  if (s.size() > 1 && s.front() == '"' && s.back() == '"')
    return s.substr(1, s.size() - 2);
  return s;
}
static std::string ask(const char *label, const char *def = "") {
  std::cout << label;
  if (*def) std::cout << " [" << def << "]";
  std::cout << ": ";
  std::string value; std::getline(std::cin, value);
  return value.empty() ? def : unquote(value);
}
int main(int argc, char **argv) {
  const bool interactive = argc == 1;
  int result = 0;
  try {
    std::string input, destination; int slot = 0, frameSize = 2048; bool replace = false;
    if (interactive) {
      std::cout << "NOXED Prism Wave - WAV Wavetable Import\n\n";
      input = ask("Input WAV path (a cyclic wavetable, not a song)");
      slot = std::stoi(ask("User slot 1..8", "2"));
      frameSize = std::stoi(ask("Samples per input frame", "2048"));
      destination = ask("Plugin folder, e.g. E:/VST2 Plugins/NOXED - VST - Prism Wave");
    } else {
      input = argv[1];
      for (int i = 2; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--replace") { replace = true; continue; }
        if (i + 1 >= argc) throw std::runtime_error("Missing option value");
        std::string value = argv[++i];
        if (key == "--slot") slot = std::stoi(value);
        else if (key == "--frame-size") frameSize = std::stoi(value);
        else if (key == "--plugin-folder") destination = value;
        else throw std::runtime_error("Unknown option: " + key);
      }
    }
    if (slot < 1 || slot > 8 || frameSize < 32 || frameSize > 16384 || destination.empty())
      throw std::runtime_error("Use --slot 1..8, --frame-size 32..16384 and --plugin-folder PATH");
    const fs::path folder(destination);
    if (!fs::is_regular_file(folder / "plugin.json"))
      throw std::runtime_error("Destination must be the Prism Wave plugin folder containing plugin.json");
    if (fs::file_size(input) > 256 * 1024 * 1024)
      throw std::runtime_error("Input WAV exceeds 256 MB");
    dsp::WavData wav;
    if (!dsp::loadWav(input, wav) || wav.numChannels() > 2)
      throw std::runtime_error("Expected mono/stereo PCM or float32 WAV");
    const size_t samples = wav.numFrames();
    if (samples % frameSize || samples < size_t(frameSize) || samples / frameSize > 1024)
      throw std::runtime_error("Input must contain 1..1024 complete cycles of the specified frame size");
    for (const auto &channel : wav.channels)
      for (float x : channel)
        if (!std::isfinite(x)) throw std::runtime_error("Input contains NaN/Infinity");
    const int frames = int(samples / frameSize), count = std::min(frames, 64);
    std::vector<int16_t> audio; audio.reserve(size_t(count) * 2048);
    std::vector<float> cycle(frameSize);
    for (int f = 0; f < count; ++f) {
      int source = int(std::lround(double(f) * (frames - 1) / std::max(1, count - 1)));
      double mean = 0;
      for (int i = 0; i < frameSize; ++i) {
        float x = 0; for (const auto &channel : wav.channels) x += channel[size_t(source) * frameSize + i];
        cycle[i] = x / wav.numChannels(); mean += cycle[i];
      }
      mean /= frameSize; float peak = 0;
      for (float &x : cycle) { x -= float(mean); peak = std::max(peak, std::fabs(x)); }
      if (peak < 1e-8f) throw std::runtime_error("A frame is silent after DC removal");
      for (int i = 0; i < 2048; ++i) {
        float p = float(i) * frameSize / 2048; int j = int(p); float t = p - j;
        float x = (cycle[j] + t * (cycle[(j + 1) % frameSize] - cycle[j])) / peak * .85f;
        audio.push_back(int16_t(std::lround(std::clamp(x, -1.0f, 1.0f) * 32767)));
      }
    }
    char name[32]; std::snprintf(name, sizeof(name), "user%02d.wav", slot);
    const fs::path target = folder / "Content" / "Wavetables" / name;
    if (fs::exists(target) && !replace) {
      if (!interactive || ask("Slot exists. Type REPLACE to overwrite") != "REPLACE")
        throw std::runtime_error("Slot exists; use --replace to overwrite intentionally");
    }
    fs::create_directories(target.parent_path());
    std::FILE *file = std::fopen(target.string().c_str(), "wb");
    if (!file) throw std::runtime_error("Cannot write output slot");
    auto u16 = [&](unsigned x) { std::fputc(x & 255, file); std::fputc((x >> 8) & 255, file); };
    auto u32 = [&](unsigned x) { u16(x & 65535); u16(x >> 16); };
    std::fwrite("RIFF", 1, 4, file); u32(36 + unsigned(audio.size() * 2));
    std::fwrite("WAVEfmt ", 1, 8, file); u32(16); u16(1); u16(1); u32(44100); u32(88200); u16(2); u16(16);
    std::fwrite("data", 1, 4, file); u32(unsigned(audio.size() * 2));
    for (int16_t x : audio) u16(uint16_t(x));
    const bool bad = std::ferror(file) != 0; const int closed = std::fclose(file);
    if (bad || closed) throw std::runtime_error("Output write failed");
    std::cout << "Imported " << frames << " -> " << count << " frames into User " << slot << ": " << target.string()
              << "\nReload all Prism Wave instances on the MPC to load the new slot.\n";
  } catch (const std::exception &e) { std::cerr << "Import failed: " << e.what() << "\n"; result = 1; }
  if (interactive) { std::cout << "Press Enter to close."; std::string line; std::getline(std::cin, line); }
  return result;
}
