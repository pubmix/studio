#pragma once
#include "../../packages/modular/patch.h"
#include <stdint.h>

// UART link to the Teensy (see dubbox-firmware/src/esp_link.h for the protocol).
// ESP32 UART2: RX = GPIO4 (from Teensy pin 1 / TX1), TX = GPIO17 (to Teensy pin 0 / RX1).
namespace teensylink {

constexpr int kNumTracks = 4;
constexpr int kRxPin = 4;
constexpr int kTxPin = 17;
constexpr int kMaxWaveCols = 256;

constexpr int kMaxClips = 6;

struct TrackInfo {
  uint32_t fileLenMs = 0;
  int clipCount = 1;
  uint32_t clipStart[kMaxClips] = {0};
  uint32_t clipEnd[kMaxClips] = {0};
  int32_t clipOff[kMaxClips] = {0};  // where each clip's file time 0 sits on the timeline
  bool known = false;
};

struct FxInfo {
  bool known = false;
  bool bypassed = false;
  int wetPermille = 0;
  int activeSlot = 0;
  bool canAddFlange = false;
  bool canAddChorus = false;
  int types[3] = {-1, -1, -1};  // -1 = empty slot
};

struct Wave {
  uint8_t peaks[kMaxWaveCols] = {0};
  int cols = 0;
  int received = 0;
  bool complete = false;
  uint32_t sum = 0;     // checksum of the last completed transfer
  uint32_t runSum = 0;  // checksum of the transfer in progress
};

constexpr int kMaxProjects = 12;
constexpr int kNameLen = 24;
constexpr int kMaxFiles = 16;
constexpr int kFileNameLen = 64;

struct ProjectInfo {
  bool open = false;
  char name[kNameLen] = {0};
  int listCount = 0;
  char list[kMaxProjects][kNameLen] = {{0}};
  bool listKnown = false;
};

struct FileList {
  int count = 0;          // -1 = the Teensy is playing (list refused)
  bool known = false;
  char names[kMaxFiles][kFileNameLen] = {{0}};
};

struct Beats {
  bool valid = false;
  int bpm100 = 0;        // tempo x100
  uint32_t firstMs = 0;  // first bar start (beat 1)
  int barMs10 = 0;       // bar length in ms x10
};

// Drum machine (patterns are 16 steps x 8 rows; a pattern clip repeats one on the timeline).
constexpr int kNumPatterns = 8;
constexpr int kDrumRows = 8;
constexpr int kPatSteps = 16;
constexpr int kMaxPatClips = 16;
constexpr int kMaxNotes = 24;  // notes per pattern per instrument
constexpr int kMaxInstruments = 4;

struct PatClip {
  uint8_t pattern = 0;
  uint16_t startBar = 0;
  uint8_t lenBars = 1;
};

// A piano note inside a pattern: pitch (MIDI number), first step (0-15), length in steps.
struct PatNote {
  uint8_t midi = 60;
  uint8_t start = 0;
  uint8_t len = 1;
};

struct Drums {
  uint16_t rows[kNumPatterns][kDrumRows] = {{0}};  // bit n of a row = step n
  int bpm = 120;
  int clipCount = 0;
  PatClip clips[kMaxPatClips];
  int instCount = 0;  // synth instruments
  int noteCount[kNumPatterns][kMaxInstruments] = {{0}};
  PatNote notes[kNumPatterns][kMaxInstruments][kMaxNotes];
  int padRoute[4] = {0, 1, 3, 2};  // drum row each physical pad plays (-1 = none)
  bool previewOn = false;
  bool recording = false;  // pad presses are being written into the looping pattern
  bool countIn = false;    // the one-bar count-in before recording
  bool clickOn = true;     // metronome click while recording
  int padMode = 0;         // what the pads play: 0 = drum sounds, 1 = piano notes
  int padNote[4] = {60, 64, 67, 72};  // MIDI note each pad plays in piano mode
  int previewPattern = 0;
  int previewStep = 0;
};

// The synth patch: wave1, wave2, oct2, detune, mix, attack, decay, sustain, release, cutoff,
// resonance, chorus, reverb, level (ranges: waves 0-3, oct2 -2..2, detune -50..50, rest 0-100).
constexpr int kSynthParams = 14;
struct SynthState {
  int v[kSynthParams] = {1, 0, 1, 0, 30, 3, 59, 18, 27, 100, 0, 0, 0, 80};  // the piano-like default
};

struct State {
  bool playing = false;
  uint32_t posMs = 0;
  uint32_t songMs = 0;
  int panPermille[kNumTracks] = {};
  int faderPermille[kNumTracks] = {0, 0, 0, 0};
  int peakPermille[kNumTracks] = {0, 0, 0, 0};
  TrackInfo track[kNumTracks];
  FxInfo fx[kNumTracks];
  Wave wave[kNumTracks];
  ProjectInfo project;
  FileList files;
  int volumePermille = 1000;
  int beatMask = 0;  // per-track beat marker toggles saved with the project
  Beats beats[kNumTracks];
  Drums drums;
  bool instrumentModular[kMaxInstruments] = {};
  modular::Patch modularPatch[kMaxInstruments];
  SynthState synth[kMaxInstruments];  // the patch of each instrument
};

void begin();
void poll();
// True while state lines have arrived recently.
bool connected();
const State& state();
// Link quality since the last call: state lines received and the longest gap between them.
void takeLinkStats(uint32_t& stateLines, uint32_t& maxGapMs);

// Change flags, each cleared when read.
uint8_t takeChangedTracks();  // crop/length changed (bitmask)
uint8_t takeChangedFx();      // fx chain changed (bitmask)
uint8_t takeChangedWaves();   // a waveform finished arriving (bitmask)
bool takeSongChanged();
bool takeMixerChanged();      // fader/peak values changed
bool takeProjectChanged();    // project opened/closed/renamed
bool takeListChanged();       // project list (re)received
bool takeFilesChanged();      // SD file list (re)received
bool takeVolumeChanged();     // master volume reported by the Teensy
uint8_t takeChangedBeats();   // beat grid changed (bitmask)
bool takeBeatMaskChanged();   // beat toggles reported by the Teensy
bool takePadRouteChanged();      // the pad routing was reported by the Teensy
bool takeSynthChanged();          // a synth patch was reported by the Teensy
bool takeInstrumentsChanged();    // the number of instruments changed (reported by the Teensy)
uint8_t takePatternsChanged();  // drum pattern contents changed (bitmask over patterns)
uint32_t patClipsVersion();     // increments whenever the pattern clips change (poll and compare)
bool takeRhythmChanged();       // tempo or loop-preview state changed

void sendHello();
void sendGetList();
void sendNewProject(const char* name);
void sendOpenProject(const char* name);
void sendCloseProject();
void sendListFiles();

// WiFi upload of a WAV to the Teensy's SD card (see esp_link.h on the Teensy for the protocol).
struct UploadReply {
  bool ok = false;       // Teensy accepted the upload
  bool done = false;     // the whole file is on the card
  uint32_t acked = 0;    // chunks the Teensy has written
  char name[48] = {0};   // file name it chose
  char err[12] = {0};    // non-empty: refused / aborted
};
constexpr int kUploadChunk = 1024;
void uploadStart(uint32_t size, const char* name);   // also clears the reply
bool uploadSendChunk(uint32_t seq, const uint8_t* data, int len);  // false if the UART cannot take it yet
void uploadCancel();
const UploadReply& uploadReply();
void sendAssignTrack(int track, const char* filename);
void sendClearTrack(int track);
void sendTogglePlay();
void sendRewind();
void sendSeek(uint32_t positionMs);
void sendVolume(int permille);
void sendCrop(int track, int clip, uint32_t startMs, uint32_t endMs);
void sendSplit(int track, uint32_t positionMs);
void sendMerge(int track, int clip);
void sendClipOffset(int track, int clip, int32_t offsetMs);
void sendBeatFlag(int track, bool on);
void sendAddFx(int track, int fxType);
void sendActiveSlot(int track, int slot);
void sendPan(int track, int pan);
void sendWet(int track, int wetPermille);
void sendBypass(int track, bool bypassed);

// Drum machine edits. Each updates the local copy at once (so the screen reacts immediately)
// and sends the change to the Teensy; the Teensy's own reports are ignored briefly afterwards.
void editStep(int pattern, int row, int step, bool on);
void clearPattern(int pattern);
void setBpm(int bpm);
// Loops a pattern on its own; with `record`, pad presses are recorded into it. For a pattern that is
// already looping, `record` only turns recording on or off.
void setPreview(bool on, int pattern, bool record = false);
void addPatClip(int pattern, int bar, int lenBars, int index = -1);
void removePatClip(int index);
void movePatClip(int index, int bar);
void resizePatClip(int index, int lenBars);

// Instruments (synths). Each has its own sound and its own notes in every pattern. Notes follow the
// Teensy's rules: they stay inside the bar and do not overlap another note of the same pitch.
void setModularPatch(int inst, const modular::Patch& patch);
void addInstrument(bool modular = false);            // appends one with the default sound (up to kMaxInstruments)
void removeInstrument(int inst); // deletes it and its notes; later ones move down
void setNote(int pattern, int inst, int midi, int start, int len);  // add, or change the length of the note there
void removeNote(int pattern, int inst, int midi, int start);
void clearNotes(int pattern, int inst);
void auditionNote(int inst, int midi);
// Tells the Teensy which drum row a finger is holding (-1 = none): the next pad press routes it.
void sendRouteHold(int row);
// Piano mode: a finger is holding this piano key (-1 = none); the next pad press routes it.
void sendNoteHold(int midi);
// On-screen keyboard: a note goes down / up (recorded while recording).
void sendLiveNote(int inst, int midi, bool on);
// Says a keyboard note is still down (resent while held; the Teensy releases notes it stops hearing about).
void sendNoteRefresh(int inst, int midi);
// The on-screen sustain pedal (resent while down, same rule).
void sendSustain(bool down);
// Changes one synth parameter of an instrument (local copy at once, then its whole patch is sent).
void setSynthParam(int inst, int index, int value);
// Only updates the local copy (a slider being dragged between sends).
void setSynthParamLocal(int inst, int index, int value);
void setSynthAll(int inst, const int* values);
void setClickOn(bool on);
// What the pads play: drums (0) or the notes of instrument `inst` (1; inst -1 = unchanged).
void setPadMode(int mode, int inst = -1);

}  // namespace teensylink
