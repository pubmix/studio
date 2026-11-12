#pragma once
#include <Arduino.h>
#include <SD.h>

#include "audio_engine.h"
#include "project.h"

namespace dubbox {

// Records one input (the internal mic, or LINE IN L / R) to a mono 44.1 kHz 16-bit WAV on the SD
// card and, when stopped, puts it on the first empty track at the playhead position it started
// from. Only runs while the transport is stopped: extra SD writes while tracks stream have wedged
// the card before.
class MicRecorder {
 public:
  enum State { kIdle = 0, kRecording = 1, kError = 2 };

  void begin(AudioEngine* engine, ProjectManager* projects);
  // Requests are queued and handled in update(), so they are safe to make from the UART handler.
  // `source`: 0 = internal mic, 1 = input 1 (LINE IN L), 2 = input 2 (LINE IN R).
  void requestStart(int source = 0) {
    pendingSource_ = source >= 0 && source <= 2 ? source : 0;
    startRequested_ = true;
  }
  void requestStop() { stopRequested_ = true; }
  // Call every loop. `sdBusy`: something else (a scan, an upload) is using the SD card.
  void update(bool sdBusy);

  bool recording() const { return recording_; }
  // The source of the current (or last) take; what the level meter should show.
  int source() const { return source_; }
  State state() const { return recording_ ? kRecording : (err_[0] != '\0' ? kError : kIdle); }
  uint32_t elapsedMs() const { return static_cast<uint32_t>(samples_ * 1000ull / 44100ull); }
  // Short code for the last failed start ("playing", "noproject", "notrack", "busy", "sd"), or "".
  const char* lastError() const { return err_; }

 private:
  void startRecording(bool sdBusy);
  void drain();
  void finish();
  void fail(const char* code);
  void writeHeader(uint32_t dataBytes);

  AudioEngine* engine_ = nullptr;
  ProjectManager* projects_ = nullptr;
  File file_;
  char name_[16] = {0};
  char err_[12] = {0};
  uint32_t errMs_ = 0;
  bool recording_ = false;
  bool startRequested_ = false;
  bool stopRequested_ = false;
  int source_ = 0;
  int pendingSource_ = 0;
  int track_ = 0;
  uint32_t startMs_ = 0;
  uint64_t samples_ = 0;
  int16_t buf_[256];
};

}  // namespace dubbox
