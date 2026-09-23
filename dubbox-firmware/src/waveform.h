#pragma once

#include <Arduino.h>
#include <SD.h>

namespace dubbox {

// Fine-grained peak envelope of a whole WAV file, one byte per column
// (0-255, absolute peak >> 7), for the ESP32 display's waveform lanes.
// Scanned incrementally by sparsely probing a few short bursts inside each
// column (not reading every sample), a small time budget per step() call,
// so it can run in the background from loop() without starving playback.
class PeakScanner {
 public:
  static constexpr int kProbesPerColumn = 4;
  static constexpr size_t kProbeBytes = 256;

  bool start(const char* filename, uint8_t* out, int columns);
  // Does up to `budgetUs` of work. Returns true once the scan has finished
  // (successfully or not — check ok()).
  bool step(uint32_t budgetUs);
  void cancel() {
    if (active_) file_.close();
    active_ = false;
  }
  bool active() const { return active_; }
  bool ok() const { return ok_; }

 private:
  File file_;
  bool active_ = false;
  bool ok_ = false;
  uint8_t* out_ = nullptr;
  int columns_ = 0;
  int column_ = 0;
  uint32_t frameSize_ = 4;
  uint32_t totalFrames_ = 0;
  uint32_t dataStart_ = 0;
};

// Beat grid of one track: tempo plus where beat 1 of each 4-beat bar falls, for
// the ESP32 display's red bar markers. Found from the loudness-onset envelope
// (best on percussive material; `valid` stays false when no steady pulse is found).
struct BeatInfo {
  bool valid = false;
  float bpm = 0.0f;
  uint32_t firstDownbeatMs = 0;  // first bar start inside the file
  float barMs = 0.0f;            // length of one 4-beat bar
  float confidence = 0.0f;       // grid score relative to average onset energy (>1.25 = clear pulse)
  float candidateBpm = 0.0f;     // best guess even when not valid
};

// Reads a whole WAV once (sequentially, a little per step() call, like
// PeakScanner — run only while nothing is playing) and derives a BeatInfo.
// Analyses at most the first ~230 seconds; the display extends the grid past that.
class BeatScanner {
 public:
  bool start(const char* filename);
  // Does up to `budgetUs` of work; returns true when finished (see result()).
  bool step(uint32_t budgetUs);
  void cancel() {
    if (active_) file_.close();
    active_ = false;
  }
  bool active() const { return active_; }
  const BeatInfo& result() const { return result_; }

 private:
  void analyze();

  File file_;
  bool active_ = false;
  BeatInfo result_;
  uint32_t sampleRate_ = 44100;
  uint32_t frameSize_ = 4;
  uint32_t bytesRemaining_ = 0;
  uint32_t dataStart_ = 0;
  uint32_t dataSize_ = 0;
  uint32_t hopFill_ = 0;
  double hopSum_ = 0.0;
  float prevLog_ = 0.0f;
  int frames_ = 0;
};

}  // namespace dubbox
