#pragma once

namespace dubbox {

// Initializes the Teensy 4.1's built-in microSD slot. Must be called before
// any AudioPlaySdWav::play() call. Returns false if no card is present or
// it fails to mount.
bool initSdCard();

// Max entries listWavFiles() returns, and the size of each name buffer. Matches
// AudioEngine::kMaxFilenameLen (kept in sync by hand to avoid an audio_engine.h dependency).
constexpr int kMaxWavFileEntries = 16;
constexpr int kMaxWavFilenameLen = 64;

// Lists up to kMaxWavFileEntries file names ending in ".wav" (any case) from the SD card root
// into outNames[0..return value-1]. Returns how many were found. Call fresh each time (a file
// just dropped on the card over MTP shows up immediately); never while tracks are streaming.
int listWavFiles(char outNames[][kMaxWavFilenameLen]);

}  // namespace dubbox
