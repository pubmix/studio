#pragma once
#include <Arduino.h>

#include "audio_engine.h"

namespace dubbox {

constexpr int kMaxProjects = 12;
constexpr int kProjectNameMax = 20;  // characters, excluding the terminator

// Saved projects live on the SD card as /PROJECTS/<NAME>.PRJ (plain text).
// A project stores which WAV file sits on each track with its clips, the beat-marker
// toggles, and the drum patterns with their tempo and playlist clips; fx state is not saved yet.
class ProjectManager {
 public:
  void begin(AudioEngine* engine);

  // Fills `names` with saved project names (sorted as found); returns count.
  int listNames(char names[][kProjectNameMax + 1], int maxCount);

  // Writes a project file naming `files` (full-length, uncropped) without
  // opening it. Used once to preserve the original four tracks as "DEMO".
  bool seedFromFiles(const char* name, const char* const files[kNumTracks]);

  // Creates an empty project and opens it. False if the name is invalid or taken.
  bool createNew(const char* name);
  // Loads a saved project: tracks are loaded and left paused at the start.
  bool openProject(const char* name);
  // Puts a WAV from the SD card onto a track (replacing whatever was there).
  // Pauses everything and rewinds to the start. Needs an open project.
  bool assignTrack(int track, const char* filename);
  // Empties a track.
  void clearTrack(int track);
  // Saves and closes the open project, unloading every track (playback stops).
  void closeProject();

  // Per-track "show beat markers" toggle, saved with the project.
  uint8_t beatMask() const { return beatMask_; }
  void setBeatFlag(int track, bool on);

  bool isOpen() const { return open_; }
  const char* name() const { return name_; }

  // Call every loop: debounced autosave of crop changes.
  void update();

  // Bitmask of tracks whose loaded file changed since the last call.
  uint8_t takeLoadedMask();
  // True once after the set of projects changed.
  bool takeListDirty();

 private:
  bool save();
  struct Loaded {
    char files[kNumTracks][AudioEngine::kMaxFilenameLen];
    uint32_t clipS[kNumTracks][AudioEngine::kMaxClips];
    uint32_t clipE[kNumTracks][AudioEngine::kMaxClips];
    int clipN[kNumTracks];
    int pan[kNumTracks] = {};
    uint8_t beatMask;
    int32_t offsetMs[kNumTracks];  // legacy track-wide offset (OFF= line), default for clips
    int32_t clipO[kNumTracks][AudioEngine::kMaxClips];
    int bpm;
    uint16_t rows[DrumMachine::kNumPatterns][DrumMachine::kRows];
    DrumMachine::Clip patClips[DrumMachine::kMaxClips];
    int patClipN;
    int padRoute[DrumMachine::kPads];
    int padNote[DrumMachine::kPads];
    int instCount;  // synth instruments
    int modularType[DrumMachine::kMaxInstruments] = {};
    modular::Patch modularPatch[DrumMachine::kMaxInstruments];
    bool hasSynth[DrumMachine::kMaxInstruments];
    int synth[DrumMachine::kMaxInstruments][Synth::kParams];
    DrumMachine::Note notes[DrumMachine::kNumPatterns][DrumMachine::kMaxInstruments][DrumMachine::kMaxNotes];
    int noteN[DrumMachine::kNumPatterns][DrumMachine::kMaxInstruments];
    bool oldFormat;  // an older file with a single piano part: it becomes instrument 1
  };
  bool loadFile(const char* name, Loaded& out);
  uint32_t clipSignature(int track) const;
  void pathFor(const char* name, char* out, size_t outLen) const;
  void unloadAll();
  bool sanitize(const char* in, char* out) const;

  AudioEngine* engine_ = nullptr;
  bool open_ = false;
  char name_[kProjectNameMax + 1] = {0};
  uint8_t loadedMask_ = 0;
  bool listDirty_ = true;

  // Autosave bookkeeping.
  uint32_t savedClipSig_[kNumTracks] = {0};
  uint8_t beatMask_ = 0;
  int savedPan_[kNumTracks] = {};
  uint8_t savedBeatMask_ = 0;
  uint32_t savedDrumSig_ = 0;
  char savedFile_[kNumTracks][AudioEngine::kMaxFilenameLen] = {{0}};
  uint32_t changedSinceMs_ = 0;
  bool dirty_ = false;
};

}  // namespace dubbox
