#pragma once

#include <Audio.h>

#include "synth.h"

namespace dubbox {

// A step-sequencer with synthesized drums (no SD access, so it never competes with track
// streaming) and up to four synth instruments. Patterns are 16 steps (one 4/4 bar of 16th notes)
// x 8 drum rows; up to 8 patterns exist. Each instrument is a polyphonic synth with its own sound;
// a pattern holds a few notes (pitch, start step, length in steps) for each instrument. Pattern
// "clips" place a pattern on the song timeline: starting at a bar, repeating for a number of bars.
// The timeline is locked to the engine's transport clock (onTimeline()); a pattern can also be
// auditioned on its own as a loop (startPreview()).
class DrumMachine {
 public:
  static constexpr int kNumPatterns = 8;
  static constexpr int kRows = 8;  // KICK SNARE CLAP CL-HAT OP-HAT TOM RIM PERC
  static constexpr int kSteps = 16;
  static constexpr int kMaxClips = 16;
  static constexpr int kMaxInstruments = 4;
  static constexpr int kDefaultBpm = 120;
  static constexpr int kMinBpm = 40;
  static constexpr int kMaxBpm = 240;

  struct Clip {
    uint8_t pattern;
    uint16_t startBar;
    uint8_t lenBars;
  };

  static constexpr int kMaxNotes = 24;  // notes per pattern per instrument
  static constexpr int kMinNote = 24;   // C1
  static constexpr int kMaxNote = 108;  // C8
  struct Note {
    uint8_t midi;
    uint8_t start;  // step 0-15
    uint8_t len;    // steps, 1..(16 - start): notes never run past the end of the bar
  };

  // Mono output of the drums, to be wired into the final mix.
  AudioStream& output() { return outMix_; }
  // Mono output of all the instruments together, with the shared chorus and reverb.
  AudioStream& instrumentOutput() { return busOut_; }
  // Sets the voice and mixer parameters. Call once after AudioMemory().
  void begin();
  // Clears every pattern, clip and instrument and restores the default tempo (new / opened / closed project).
  void reset();

  // ---- Patterns ----
  uint16_t rowBits(int pattern, int row) const { return rows_[pattern][row]; }  // bit n = step n
  void setRowBits(int pattern, int row, uint16_t bits);
  void setStep(int pattern, int row, int step, bool on);
  void clearPattern(int pattern);
  bool patternEmpty(int pattern) const;

  // ---- Instruments (synths) ----
  int instrumentCount() const { return instCount_; }
  // Appends an instrument with the default sound; false if all four exist.
  bool addInstrument(bool modular = false);
  bool isModular(int i) const { return inst_[i].isModular(); }
  const modular::Patch& modularPatch(int i) const { return inst_[i].modularPatch(); }
  bool setModular(int i, bool on, const modular::Patch& p) { return i >= 0 && i < instCount_ && inst_[i].setModular(on, p); }
  // Deletes an instrument and its notes; the ones after it move down one place.
  bool removeInstrument(int inst);
  const SynthPatch& synthPatch(int inst) const { return inst_[inst].patch(); }
  bool setSynthParam(int inst, int index, int value);
  void setSynthAll(int inst, const int* values);
  int synthParam(int inst, int index) const { return inst_[inst].param(index); }

  // ---- Notes of an instrument in a pattern ----
  int noteCount(int pattern, int inst) const { return noteCount_[pattern][inst]; }
  Note note(int pattern, int inst, int i) const { return notes_[pattern][inst][i]; }
  // Adds a note, or changes the length of the note that starts at the same pitch and step.
  // A length that would run into the next note of the same pitch is shortened; false if the
  // start lies inside another note of that pitch, arguments are out of range, or the list is full.
  bool setNote(int pattern, int inst, int midi, int start, int len);
  bool removeNote(int pattern, int inst, int midi, int start);
  void clearNotes(int pattern, int inst);
  // Plays a note briefly (keyboard feedback while editing).
  void audition(int inst, int midi);

  // ---- Drum pads ----
  // The four physical pads play drum sounds live. Each pad is routed to one drum row.
  static constexpr int kPads = 4;
  int padRoute(int pad) const { return padRoute_[pad]; }  // drum row, or -1 for none
  void setPadRoute(int pad, int row);
  // The display reports which drum row a finger is holding (-1 = none). While one is held, the
  // next pad press routes that sound to the pad instead of playing it. The hold expires after a
  // moment unless the display keeps reporting it.
  void setRouteHold(int row);
  // The pads control either the drum sounds or piano notes (padMode: 0 = drums, 1 = notes). In
  // note mode they play instrument `inst` (the one being viewed on the display; -1 = unchanged).
  int padMode() const { return padMode_; }
  int padInst() const { return padInst_; }
  void setPadMode(int mode, int inst = -1);
  // Note mode: each pad is routed to one MIDI note, routed the same way as drums (hold a key on
  // the display, press a pad).
  int padNote(int pad) const { return padNote_[pad]; }
  void setPadNote(int pad, int midi);
  void setNoteHold(int midi);  // -1 = none
  // A pad was pressed / released: routes the held sound or note to it, or plays what it is
  // routed to (notes sound for as long as the pad is held).
  void padPressed(int pad);
  void padReleased(int pad);
  // Live notes (the pads in note mode, and the on-screen keyboards). While recording, the note
  // is written into the looping pattern: snapped to the nearest step, as long as it was held.
  // `fromDisplay` marks notes played on the display's keyboard: the display keeps refreshing them
  // (refreshNote()), and one that stops being refreshed is released, so a lost message can never
  // leave a note stuck.
  void noteDown(int inst, int midi, bool fromDisplay = false);
  void noteUp(int inst, int midi);
  void refreshNote(int inst, int midi);
  // The sustain pedal: while it is down, notes that are let go keep sounding until it is released.
  // A pedal held from the display must keep being refreshed (setSustain(true) again).
  void setSustain(bool on, bool fromDisplay = false);
  bool sustain() const { return sustain_; }
  // Plays one drum row right now.
  void hit(int row);

  // ---- Tempo ----
  int bpm() const { return bpm_; }
  void setBpm(int bpm);
  uint32_t barMs() const { return static_cast<uint32_t>(240000.0f / bpm_ + 0.5f); }

  // ---- Pattern clips on the timeline ----
  int clipCount() const { return clipCount_; }
  Clip clip(int i) const { return clips_[i]; }
  // Inserts a clip at `index` (-1 = append). False if the list is full or arguments are invalid.
  bool addClip(int pattern, int startBar, int lenBars, int index = -1);
  bool removeClip(int index);
  bool moveClip(int index, int startBar);
  bool resizeClip(int index, int lenBars);
  // End of the last clip on the timeline in ms (0 if there are none).
  uint32_t endMs() const;

  // ---- Sequencing ----
  // Re-aims the timeline sequencer after a seek, a start, or a tempo change: the next step
  // played is the first one at or after `timelineMs`.
  void resync(uint32_t timelineMs);
  // Call while the transport clock runs: triggers every step the clock has reached.
  void onTimeline(uint32_t timelineMs);
  // Loops one pattern on its own clock (independent of the song transport).
  // With `record`, pad presses are written into the pattern (snapped to the nearest 16th step)
  // while it loops, with a metronome click; recording only adds hits, it never erases. Calling
  // this again for the pattern already looping just switches recording on or off, without
  // restarting the loop.
  void startPreview(int pattern, bool record = false);
  void setRecording(bool on);
  bool recording() const { return recording_; }
  void stopPreview();
  // True during the one-bar count-in before recording starts.
  bool countingIn() const { return countIn_; }
  // The metronome click while recording (the count-in always clicks).
  bool clickOn() const { return clickOn_; }
  void setClickOn(bool on) { clickOn_ = on; }
  // Releases every sounding sequenced note (transport paused, sought, or switched to preview).
  void releaseAll();
  bool previewing() const { return previewOn_; }
  int previewPattern() const { return previewPattern_; }
  int previewStep() const { return previewStep_; }
  // Call every loop: plays the preview loop.
  void update();
  // Number of drum hits and instrument notes triggered since boot (diagnostics).
  uint32_t hitCount() const { return hits_; }

  // Changes whenever anything saved with the project changes (for autosave).
  uint32_t signature() const;

 private:
  float stepMs() const { return 15000.0f / bpm_; }  // one 16th note
  uint8_t maskAt(uint32_t absStep) const;            // rows sounding on a timeline step
  uint8_t patternMask(int pattern, int step) const;
  void trigger(uint8_t rowMask);
  void recordHit(int row);           // writes a pad hit into the looping pattern
  void clickBeat(bool accent);       // metronome tick while recording
  bool liveHeld(int inst, int midi) const;  // a live note of this pitch is being held
  int pickVoice(int inst);           // a voice free of the sequencer, auditions and held notes
  // Note scheduling on a step counter (timeline steps or preview steps).
  void releaseDue(uint32_t absStep);
  void startNote(int inst, int midi, uint32_t offStep);
  void triggerNotes(int pattern, int step, uint32_t absStep);
  void triggerNotesAt(uint32_t absStep);  // every pattern clip covering this timeline step
  void releaseLive(int slot);  // actually stops a live note's voice and frees its slot

  uint16_t rows_[kNumPatterns][kRows] = {{0}};
  int bpm_ = kDefaultBpm;
  Clip clips_[kMaxClips];
  int clipCount_ = 0;

  Note notes_[kNumPatterns][kMaxInstruments][kMaxNotes];
  int noteCount_[kNumPatterns][kMaxInstruments] = {{0}};
  static constexpr uint32_t kNoStep = 0xFFFFFFFFu;
  static constexpr uint32_t kHeldForever = 0xFFFFFFFFu;  // voiceAuditionUntilMs_ value while a live note is held
  // Per instrument and voice: the step at which a sequenced note ends (kNoStep = free), and when
  // an auditioned note ends (0 = none, kHeldForever = held by a live note).
  uint32_t voiceOffStep_[kMaxInstruments][Synth::kVoices];
  uint32_t voiceAuditionUntilMs_[kMaxInstruments][Synth::kVoices] = {{0}};
  Synth inst_[kMaxInstruments];
  int instCount_ = 0;
  // The instruments' dry sounds are summed in instMix_; their CHORUS / REVERB sliders are send levels
  // into one chorus and one reverb shared by all of them (chorusBus_ / reverbBus_ sum the sends).
  AudioMixer4 instMix_, chorusBus_, reverbBus_, busOut_;
  AudioEffectChorus chorus_;
  AudioEffectFreeverb reverb_;
  static DMAMEM short chorusLine_[2048];
  AudioConnection* instPatch_[kMaxInstruments * 3] = {nullptr};
  AudioConnection* busPatch_[5] = {nullptr};

  int padRoute_[kPads] = {0, 1, 3, 2};  // kick, snare, closed hat, clap
  int routeHoldRow_ = -1;
  uint32_t routeHoldUntilMs_ = 0;
  uint32_t rowHitMs_[kRows] = {0};  // when each drum row was last played live (to avoid double hits while recording)
  int padMode_ = 0;
  int padInst_ = 0;
  int padNote_[kPads] = {60, 64, 67, 72};  // C4 E4 G4 C5
  int padDownNote_[kPads] = {-1, -1, -1, -1};
  int padDownInst_[kPads] = {0, 0, 0, 0};
  int noteHoldMidi_ = -1;
  uint32_t noteHoldUntilMs_ = 0;

  struct LiveNote {  // a note being held (pad or on-screen key)
    bool used;
    int inst;
    int midi;
    int voice;
    uint32_t startMs;
    int pattern;  // the pattern it is being recorded into (-1 = not recording)
    int step;
    bool fromDisplay;
    uint32_t refreshMs;  // last time the display said the key was still down
    bool sustained;      // let go, but still sounding because the sustain pedal is down
  };
  static constexpr int kLiveSlots = 8;
  LiveNote live_[kLiveSlots] = {};
  bool sustain_ = false;
  bool sustainFromDisplay_ = false;
  uint32_t sustainRefreshMs_ = 0;

  uint32_t hits_ = 0;
  uint32_t nextStep_ = 0;  // next timeline step to play (16 per bar)
  bool previewOn_ = false;
  bool recording_ = false;
  bool countIn_ = false;
  bool clickOn_ = true;
  int previewPattern_ = 0;
  uint32_t previewStartMs_ = 0;
  uint32_t previewNext_ = 0;
  int previewStep_ = 0;

  // Drum voices. Kick/snare/tom/rim/perc are pitched drums; the rest is shaped noise.
  AudioSynthSimpleDrum kick_, snareTone_, tom_, rim_, perc_, click_;
  AudioSynthNoiseWhite noise_;
  AudioFilterStateVariable snareFilter_, clapFilter_, hatFilter_;  // outputs: 0 low, 1 band, 2 high
  AudioEffectEnvelope snareEnv_, clapEnv_, hatClosedEnv_, hatOpenEnv_;
  AudioMixer4 mixA_, mixB_, mixC_, outMix_;

  AudioConnection cKickA_{kick_, 0, mixA_, 0};
  AudioConnection cSnareToneA_{snareTone_, 0, mixA_, 1};
  AudioConnection cNoiseSnare_{noise_, 0, snareFilter_, 0};
  AudioConnection cSnareFiltEnv_{snareFilter_, 2, snareEnv_, 0};
  AudioConnection cSnareEnvA_{snareEnv_, 0, mixA_, 2};
  AudioConnection cNoiseClap_{noise_, 0, clapFilter_, 0};
  AudioConnection cClapFiltEnv_{clapFilter_, 1, clapEnv_, 0};
  AudioConnection cClapEnvA_{clapEnv_, 0, mixA_, 3};
  AudioConnection cNoiseHat_{noise_, 0, hatFilter_, 0};
  AudioConnection cHatFiltClosed_{hatFilter_, 2, hatClosedEnv_, 0};
  AudioConnection cHatFiltOpen_{hatFilter_, 2, hatOpenEnv_, 0};
  AudioConnection cHatClosedB_{hatClosedEnv_, 0, mixB_, 0};
  AudioConnection cHatOpenB_{hatOpenEnv_, 0, mixB_, 1};
  AudioConnection cTomB_{tom_, 0, mixB_, 2};
  AudioConnection cRimB_{rim_, 0, mixB_, 3};
  AudioConnection cPercC_{perc_, 0, mixC_, 0};
  AudioConnection cClickC_{click_, 0, mixC_, 1};
  AudioConnection cMixAOut_{mixA_, 0, outMix_, 0};
  AudioConnection cMixBOut_{mixB_, 0, outMix_, 1};
  AudioConnection cMixCOut_{mixC_, 0, outMix_, 2};
};

}  // namespace dubbox
