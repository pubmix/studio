#include "drum_machine.h"

#include <math.h>

namespace dubbox {

DMAMEM short DrumMachine::chorusLine_[2048];

void DrumMachine::begin() {
  for (int i = 0; i < kMaxInstruments; ++i) {
    inst_[i].begin();
    inst_[i].setEnabled(false);  // switched on when the instrument is added
    instMix_.gain(i, 0.7f);
    chorusBus_.gain(i, 1.0f);
    reverbBus_.gain(i, 1.0f);
    instPatch_[i * 3 + 0] = new AudioConnection(inst_[i].output(), 0, instMix_, i);
    instPatch_[i * 3 + 1] = new AudioConnection(inst_[i].chorusSend(), 0, chorusBus_, i);
    instPatch_[i * 3 + 2] = new AudioConnection(inst_[i].reverbSend(), 0, reverbBus_, i);
    for (int v = 0; v < Synth::kVoices; ++v) voiceOffStep_[i][v] = kNoStep;
  }
  chorus_.begin(chorusLine_, 2048, 2);
  reverb_.roomsize(0.7f);
  reverb_.damping(0.4f);
  busOut_.gain(0, 1.0f);
  busOut_.gain(1, 0.7f);
  busOut_.gain(2, 0.9f);
  busPatch_[0] = new AudioConnection(instMix_, 0, busOut_, 0);
  busPatch_[1] = new AudioConnection(chorusBus_, 0, chorus_, 0);
  busPatch_[2] = new AudioConnection(chorus_, 0, busOut_, 1);
  busPatch_[3] = new AudioConnection(reverbBus_, 0, reverb_, 0);
  busPatch_[4] = new AudioConnection(reverb_, 0, busOut_, 2);
  noise_.amplitude(0.7f);

  kick_.frequency(52.0f);
  kick_.length(280);
  kick_.secondMix(0.0f);
  kick_.pitchMod(0.9f);

  snareTone_.frequency(190.0f);
  snareTone_.length(110);
  snareTone_.secondMix(0.3f);
  snareTone_.pitchMod(0.25f);
  snareFilter_.frequency(1400.0f);
  snareFilter_.resonance(0.9f);
  snareEnv_.attack(1.0f);
  snareEnv_.hold(0.0f);
  snareEnv_.decay(140.0f);
  snareEnv_.sustain(0.0f);
  snareEnv_.release(20.0f);

  clapFilter_.frequency(1150.0f);
  clapFilter_.resonance(2.5f);
  clapEnv_.attack(1.0f);
  clapEnv_.hold(12.0f);
  clapEnv_.decay(110.0f);
  clapEnv_.sustain(0.0f);
  clapEnv_.release(20.0f);

  hatFilter_.frequency(7500.0f);
  hatFilter_.resonance(0.9f);
  hatClosedEnv_.attack(0.5f);
  hatClosedEnv_.hold(0.0f);
  hatClosedEnv_.decay(45.0f);
  hatClosedEnv_.sustain(0.0f);
  hatClosedEnv_.release(10.0f);
  hatOpenEnv_.attack(0.5f);
  hatOpenEnv_.hold(30.0f);
  hatOpenEnv_.decay(280.0f);
  hatOpenEnv_.sustain(0.0f);
  hatOpenEnv_.release(40.0f);

  tom_.frequency(115.0f);
  tom_.length(230);
  tom_.secondMix(0.0f);
  tom_.pitchMod(0.5f);

  rim_.frequency(880.0f);
  rim_.length(28);
  rim_.secondMix(0.6f);
  rim_.pitchMod(0.0f);

  click_.frequency(1500.0f);
  click_.length(18);
  click_.secondMix(0.0f);
  click_.pitchMod(0.0f);
  mixC_.gain(1, 0.5f);

  perc_.frequency(560.0f);
  perc_.length(150);
  perc_.secondMix(0.9f);
  perc_.pitchMod(0.05f);

  mixA_.gain(0, 1.0f);   // kick
  mixA_.gain(1, 0.5f);   // snare tone
  mixA_.gain(2, 0.6f);   // snare noise
  mixA_.gain(3, 0.9f);   // clap
  mixB_.gain(0, 0.45f);  // closed hat
  mixB_.gain(1, 0.45f);  // open hat
  mixB_.gain(2, 0.8f);   // tom
  mixB_.gain(3, 0.6f);   // rim
  mixC_.gain(0, 0.6f);   // perc
  outMix_.gain(0, 0.6f);
  outMix_.gain(1, 0.6f);
  outMix_.gain(2, 0.6f);
}

void DrumMachine::reset() {
  for (int p = 0; p < kNumPatterns; ++p) {
    for (int r = 0; r < kRows; ++r) rows_[p][r] = 0;
    for (int i = 0; i < kMaxInstruments; ++i) noteCount_[p][i] = 0;
  }
  clipCount_ = 0;
  bpm_ = kDefaultBpm;
  previewOn_ = false;
  recording_ = false;
  padRoute_[0] = 0;
  padRoute_[1] = 1;
  padRoute_[2] = 3;
  padRoute_[3] = 2;
  routeHoldRow_ = -1;
  countIn_ = false;
  padMode_ = 0;
  padInst_ = 0;
  padNote_[0] = 60;
  padNote_[1] = 64;
  padNote_[2] = 67;
  padNote_[3] = 72;
  for (int i = 0; i < kMaxInstruments; ++i) {
    inst_[i].setPatch(SynthPatch());  // back to the default sound
    inst_[i].setEnabled(false);
  }
  instCount_ = 0;
  for (int s = 0; s < kLiveSlots; ++s) releaseLive(s);
  releaseAll();
}

void DrumMachine::setRowBits(int pattern, int row, uint16_t bits) {
  if (pattern < 0 || pattern >= kNumPatterns || row < 0 || row >= kRows) return;
  rows_[pattern][row] = bits;
}

void DrumMachine::setStep(int pattern, int row, int step, bool on) {
  if (pattern < 0 || pattern >= kNumPatterns || row < 0 || row >= kRows || step < 0 ||
      step >= kSteps) {
    return;
  }
  if (on) {
    rows_[pattern][row] |= static_cast<uint16_t>(1u << step);
  } else {
    rows_[pattern][row] &= static_cast<uint16_t>(~(1u << step));
  }
}

void DrumMachine::clearPattern(int pattern) {
  if (pattern < 0 || pattern >= kNumPatterns) return;
  for (int r = 0; r < kRows; ++r) rows_[pattern][r] = 0;
}

bool DrumMachine::patternEmpty(int pattern) const {
  for (int r = 0; r < kRows; ++r) {
    if (rows_[pattern][r] != 0) return false;
  }
  return true;
}

// ---- Instruments ----

bool DrumMachine::addInstrument() {
  if (instCount_ >= kMaxInstruments) return false;
  inst_[instCount_].setPatch(SynthPatch());
  inst_[instCount_].setEnabled(true);
  for (int p = 0; p < kNumPatterns; ++p) noteCount_[p][instCount_] = 0;
  ++instCount_;
  return true;
}

bool DrumMachine::removeInstrument(int inst) {
  if (inst < 0 || inst >= instCount_) return false;
  // Stop everything that is sounding, then shift the later instruments down over this one.
  for (int s = 0; s < kLiveSlots; ++s) releaseLive(s);
  for (int i = 0; i < instCount_; ++i) {
    for (int v = 0; v < Synth::kVoices; ++v) {
      inst_[i].noteOff(v);
      voiceOffStep_[i][v] = kNoStep;
      voiceAuditionUntilMs_[i][v] = 0;
    }
  }
  for (int i = inst; i + 1 < instCount_; ++i) {
    inst_[i].setPatch(inst_[i + 1].patch());
    for (int p = 0; p < kNumPatterns; ++p) {
      noteCount_[p][i] = noteCount_[p][i + 1];
      for (int k = 0; k < noteCount_[p][i]; ++k) notes_[p][i][k] = notes_[p][i + 1][k];
    }
  }
  const int last = instCount_ - 1;
  inst_[last].setPatch(SynthPatch());
  inst_[last].setEnabled(false);
  for (int p = 0; p < kNumPatterns; ++p) noteCount_[p][last] = 0;
  --instCount_;
  if (padInst_ >= instCount_) padInst_ = instCount_ > 0 ? instCount_ - 1 : 0;
  return true;
}

bool DrumMachine::setSynthParam(int inst, int index, int value) {
  if (inst < 0 || inst >= instCount_) return false;
  return inst_[inst].setParam(index, value);
}

void DrumMachine::setSynthAll(int inst, const int* values) {
  if (inst < 0 || inst >= instCount_) return;
  inst_[inst].setAll(values);
}

// ---- Drum pads ----

void DrumMachine::setPadRoute(int pad, int row) {
  if (pad < 0 || pad >= kPads || row < -1 || row >= kRows) return;
  padRoute_[pad] = row;
}

void DrumMachine::setRouteHold(int row) {
  routeHoldRow_ = (row >= 0 && row < kRows) ? row : -1;
  routeHoldUntilMs_ = millis() + 1200;
}

// Snaps a pad hit to the nearest 16th step of the looping pattern and adds it.
void DrumMachine::recordHit(int row) {
  if (!recording_ || !previewOn_ || row < 0 || row >= kRows) return;
  float steps = (millis() - previewStartMs_) / stepMs();
  int step = static_cast<int>(steps + 0.5f) % kSteps;
  setStep(previewPattern_, row, step, true);
  Serial.printf("REC: pattern %d row %d step %d\n", previewPattern_ + 1, row, step);
}

void DrumMachine::clickBeat(bool accent) {
  click_.frequency(accent ? 2200.0f : 1500.0f);
  click_.noteOn();
}

void DrumMachine::hit(int row) {
  if (row < 0 || row >= kRows) return;
  rowHitMs_[row] = millis();
  trigger(static_cast<uint8_t>(1 << row));
}

bool DrumMachine::liveHeld(int inst, int midi) const {
  for (int i = 0; i < kLiveSlots; ++i) {
    if (live_[i].used && live_[i].inst == inst && live_[i].midi == midi) return true;
  }
  return false;
}

void DrumMachine::setPadMode(int mode, int inst) {
  int m = mode != 0 ? 1 : 0;
  if (inst >= 0 && inst < kMaxInstruments) {
    if (inst != padInst_) {
      for (int i = 0; i < kPads; ++i) padReleased(i);  // let go of notes on the instrument the pads leave
    }
    padInst_ = inst;
  }
  if (m == padMode_) return;
  // Let go of anything a pad is holding before the pads change meaning.
  for (int i = 0; i < kPads; ++i) padReleased(i);
  padMode_ = m;
}

void DrumMachine::setPadNote(int pad, int midi) {
  if (pad < 0 || pad >= kPads || midi < kMinNote || midi > kMaxNote) return;
  padNote_[pad] = midi;
}

void DrumMachine::setNoteHold(int midi) {
  noteHoldMidi_ = (midi >= kMinNote && midi <= kMaxNote) ? midi : -1;
  noteHoldUntilMs_ = millis() + 1200;
}

void DrumMachine::padPressed(int pad) {
  if (pad < 0 || pad >= kPads) return;
  const uint32_t now = millis();
  if (padMode_ == 1) {
    if (noteHoldMidi_ >= 0 && now < noteHoldUntilMs_) {
      setPadNote(pad, noteHoldMidi_);
      Serial.printf("DRUMS: pad %d -> note %d\n", pad + 1, noteHoldMidi_);
      audition(padInst_, noteHoldMidi_);  // let the user hear what they just routed
      return;
    }
    if (padInst_ >= instCount_) return;  // no instrument to play
    padDownNote_[pad] = padNote_[pad];
    padDownInst_[pad] = padInst_;
    noteDown(padInst_, padNote_[pad]);
    return;
  }
  if (routeHoldRow_ >= 0 && now < routeHoldUntilMs_) {
    setPadRoute(pad, routeHoldRow_);
    Serial.printf("DRUMS: pad %d -> row %d\n", pad + 1, routeHoldRow_);
    hit(routeHoldRow_);  // let the user hear what they just routed
    return;
  }
  const int row = padRoute_[pad];
  if (row >= 0) recordHit(row);
  hit(row);
}

void DrumMachine::padReleased(int pad) {
  if (pad < 0 || pad >= kPads || padDownNote_[pad] < 0) return;
  noteUp(padDownInst_[pad], padDownNote_[pad]);
  padDownNote_[pad] = -1;
}

// ---- Live notes ----

void DrumMachine::releaseLive(int slot) {
  LiveNote& n = live_[slot];
  if (!n.used) return;
  inst_[n.inst].noteOff(n.voice);
  voiceAuditionUntilMs_[n.inst][n.voice] = 0;
  n.used = false;
  n.sustained = false;
}

void DrumMachine::noteDown(int inst, int midi, bool fromDisplay) {
  if (inst < 0 || inst >= instCount_ || midi < kMinNote || midi > kMaxNote) return;
  // A note of this pitch still ringing on the pedal, or already held, is replaced by the new one.
  for (int i = 0; i < kLiveSlots; ++i) {
    if (live_[i].used && live_[i].inst == inst && live_[i].midi == midi) releaseLive(i);
  }
  int slot = -1;
  for (int i = 0; i < kLiveSlots; ++i) {
    if (!live_[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {  // all slots busy: let go of the sustained note that has been ringing longest
    uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < kLiveSlots; ++i) {
      if (live_[i].sustained && live_[i].startMs < oldest) {
        oldest = live_[i].startMs;
        slot = i;
      }
    }
    if (slot < 0) return;
    releaseLive(slot);
  }
  const int v = pickVoice(inst);
  inst_[inst].noteOn(v, midi);
  voiceOffStep_[inst][v] = kNoStep;
  voiceAuditionUntilMs_[inst][v] = kHeldForever;
  LiveNote& n = live_[slot];
  n = {true, inst, midi, v, millis(), -1, 0, fromDisplay, millis(), false};
  if (recording_ && previewOn_) {  // written into the pattern as it is played
    const int step = static_cast<int>((n.startMs - previewStartMs_) / stepMs() + 0.5f) % kSteps;
    if (setNote(previewPattern_, inst, midi, step, 1)) {
      n.pattern = previewPattern_;
      n.step = step;
      Serial.printf("REC: pattern %d instrument %d note %d step %d\n", previewPattern_ + 1, inst + 1, midi, step);
    }
  }
}

void DrumMachine::noteUp(int inst, int midi) {
  for (int i = 0; i < kLiveSlots; ++i) {
    LiveNote& n = live_[i];
    if (!n.used || n.sustained || n.inst != inst || n.midi != midi) continue;
    if (n.pattern >= 0) {  // stretch the recorded note to as long as it was held
      int len = static_cast<int>((millis() - n.startMs) / stepMs() + 0.5f);
      setNote(n.pattern, inst, midi, n.step, len < 1 ? 1 : len);
      Serial.printf("REC: note %d length %d steps\n", midi, len < 1 ? 1 : len);
    }
    if (sustain_) {
      n.sustained = true;  // keeps ringing until the pedal comes up
    } else {
      releaseLive(i);
    }
    return;
  }
}

void DrumMachine::refreshNote(int inst, int midi) {
  for (int i = 0; i < kLiveSlots; ++i) {
    if (live_[i].used && !live_[i].sustained && live_[i].inst == inst && live_[i].midi == midi) {
      live_[i].refreshMs = millis();
    }
  }
}

void DrumMachine::setSustain(bool on, bool fromDisplay) {
  if (on) {
    if (!sustain_) Serial.println("KEYS: sustain down");
    sustain_ = true;
    sustainFromDisplay_ = fromDisplay;
    sustainRefreshMs_ = millis();
    return;
  }
  if (!sustain_) return;
  Serial.println("KEYS: sustain up");
  sustain_ = false;
  for (int i = 0; i < kLiveSlots; ++i) {
    if (live_[i].used && live_[i].sustained) releaseLive(i);
  }
}

// ---- Notes of an instrument in a pattern ----

bool DrumMachine::setNote(int pattern, int inst, int midi, int start, int len) {
  if (pattern < 0 || pattern >= kNumPatterns || inst < 0 || inst >= instCount_ || midi < kMinNote ||
      midi > kMaxNote || start < 0 || start >= kSteps || len < 1) {
    return false;
  }
  if (start + len > kSteps) len = kSteps - start;
  Note* notes = notes_[pattern][inst];
  int& count = noteCount_[pattern][inst];
  int existing = -1;
  for (int i = 0; i < count; ++i) {
    const Note& o = notes[i];
    if (o.midi != midi) continue;
    if (o.start == start) {
      existing = i;
    } else if (start > o.start && start < o.start + o.len) {
      return false;  // starts inside another note of this pitch
    } else if (o.start > start && start + len > o.start) {
      len = o.start - start;  // stop where the next note of this pitch begins
    }
  }
  if (existing >= 0) {
    notes[existing].len = static_cast<uint8_t>(len);
    return true;
  }
  if (count >= kMaxNotes) return false;
  notes[count++] = {static_cast<uint8_t>(midi), static_cast<uint8_t>(start), static_cast<uint8_t>(len)};
  return true;
}

bool DrumMachine::removeNote(int pattern, int inst, int midi, int start) {
  if (pattern < 0 || pattern >= kNumPatterns || inst < 0 || inst >= kMaxInstruments) return false;
  Note* notes = notes_[pattern][inst];
  int& count = noteCount_[pattern][inst];
  for (int i = 0; i < count; ++i) {
    if (notes[i].midi == midi && notes[i].start == start) {
      for (int j = i; j + 1 < count; ++j) notes[j] = notes[j + 1];
      --count;
      return true;
    }
  }
  return false;
}

void DrumMachine::clearNotes(int pattern, int inst) {
  if (pattern < 0 || pattern >= kNumPatterns || inst < 0 || inst >= kMaxInstruments) return;
  noteCount_[pattern][inst] = 0;
}

void DrumMachine::audition(int inst, int midi) {
  if (inst < 0 || inst >= instCount_ || midi < kMinNote || midi > kMaxNote) return;
  const int v = pickVoice(inst);
  inst_[inst].noteOn(v, midi);
  voiceOffStep_[inst][v] = kNoStep;
  voiceAuditionUntilMs_[inst][v] = millis() + 350;
}

// ---- Note scheduling ----

void DrumMachine::releaseDue(uint32_t absStep) {
  for (int i = 0; i < instCount_; ++i) {
    for (int v = 0; v < Synth::kVoices; ++v) {
      if (voiceOffStep_[i][v] != kNoStep && voiceOffStep_[i][v] <= absStep) {
        inst_[i].noteOff(v);
        voiceOffStep_[i][v] = kNoStep;
      }
    }
  }
}

// A voice of the instrument that is free (not held by the sequencer, an audition or a live note),
// else the one whose note ends soonest.
int DrumMachine::pickVoice(int inst) {
  for (int i = 0; i < Synth::kVoices; ++i) {
    if (voiceOffStep_[inst][i] == kNoStep && voiceAuditionUntilMs_[inst][i] == 0) return i;
  }
  int v = 0;
  for (int i = 1; i < Synth::kVoices; ++i) {
    if (voiceOffStep_[inst][i] < voiceOffStep_[inst][v]) v = i;
  }
  return v;
}

void DrumMachine::startNote(int inst, int midi, uint32_t offStep) {
  const int v = pickVoice(inst);
  inst_[inst].noteOn(v, midi);
  voiceOffStep_[inst][v] = offStep;
  voiceAuditionUntilMs_[inst][v] = 0;
  ++hits_;
}

void DrumMachine::triggerNotes(int pattern, int step, uint32_t absStep) {
  for (int inst = 0; inst < instCount_; ++inst) {
    for (int i = 0; i < noteCount_[pattern][inst]; ++i) {
      const Note& n = notes_[pattern][inst][i];
      if (n.start == step && !liveHeld(inst, n.midi)) startNote(inst, n.midi, absStep + n.len);
    }
  }
}

void DrumMachine::triggerNotesAt(uint32_t absStep) {
  uint32_t bar = absStep / kSteps;
  int step = static_cast<int>(absStep % kSteps);
  for (int i = 0; i < clipCount_; ++i) {
    const Clip& c = clips_[i];
    if (bar >= c.startBar && bar < static_cast<uint32_t>(c.startBar) + c.lenBars) {
      triggerNotes(c.pattern, step, absStep);
    }
  }
}

void DrumMachine::releaseAll() {
  for (int i = 0; i < kMaxInstruments; ++i) {
    for (int v = 0; v < Synth::kVoices; ++v) {
      if (voiceAuditionUntilMs_[i][v] == kHeldForever) continue;  // a live note keeps sounding until let go
      if (voiceOffStep_[i][v] != kNoStep || voiceAuditionUntilMs_[i][v] != 0) inst_[i].noteOff(v);
      voiceOffStep_[i][v] = kNoStep;
      voiceAuditionUntilMs_[i][v] = 0;
    }
  }
}

void DrumMachine::setBpm(int bpm) {
  bpm = constrain(bpm, kMinBpm, kMaxBpm);
  if (bpm == bpm_) return;
  // Keep the preview loop on the same step when the tempo changes underneath it.
  uint32_t now = millis();
  bool wasPreviewing = previewOn_;
  bpm_ = bpm;
  if (wasPreviewing) previewStartMs_ = now - static_cast<uint32_t>(previewNext_ * stepMs());
}

bool DrumMachine::addClip(int pattern, int startBar, int lenBars, int index) {
  if (clipCount_ >= kMaxClips || pattern < 0 || pattern >= kNumPatterns || startBar < 0 ||
      startBar > 999 || lenBars < 1 || lenBars > 99) {
    return false;
  }
  if (index < 0 || index > clipCount_) index = clipCount_;
  for (int i = clipCount_; i > index; --i) clips_[i] = clips_[i - 1];
  clips_[index] = {static_cast<uint8_t>(pattern), static_cast<uint16_t>(startBar),
                   static_cast<uint8_t>(lenBars)};
  ++clipCount_;
  return true;
}

bool DrumMachine::removeClip(int index) {
  if (index < 0 || index >= clipCount_) return false;
  for (int i = index; i + 1 < clipCount_; ++i) clips_[i] = clips_[i + 1];
  --clipCount_;
  return true;
}

bool DrumMachine::moveClip(int index, int startBar) {
  if (index < 0 || index >= clipCount_) return false;
  clips_[index].startBar = static_cast<uint16_t>(constrain(startBar, 0, 999));
  return true;
}

bool DrumMachine::resizeClip(int index, int lenBars) {
  if (index < 0 || index >= clipCount_) return false;
  clips_[index].lenBars = static_cast<uint8_t>(constrain(lenBars, 1, 99));
  return true;
}

uint32_t DrumMachine::endMs() const {
  uint32_t endBar = 0;
  for (int i = 0; i < clipCount_; ++i) {
    uint32_t e = static_cast<uint32_t>(clips_[i].startBar) + clips_[i].lenBars;
    if (e > endBar) endBar = e;
  }
  return endBar * barMs();
}

uint8_t DrumMachine::patternMask(int pattern, int step) const {
  uint8_t mask = 0;
  for (int r = 0; r < kRows; ++r) {
    if ((rows_[pattern][r] >> step) & 1) mask |= static_cast<uint8_t>(1 << r);
  }
  return mask;
}

uint8_t DrumMachine::maskAt(uint32_t absStep) const {
  uint32_t bar = absStep / kSteps;
  int step = static_cast<int>(absStep % kSteps);
  uint8_t mask = 0;
  for (int i = 0; i < clipCount_; ++i) {
    const Clip& c = clips_[i];
    if (bar >= c.startBar && bar < static_cast<uint32_t>(c.startBar) + c.lenBars) {
      mask |= patternMask(c.pattern, step);
    }
  }
  return mask;
}

void DrumMachine::trigger(uint8_t m) {
  for (uint8_t b = m; b != 0; b &= static_cast<uint8_t>(b - 1)) ++hits_;
  if (m & 0x01) kick_.noteOn();
  if (m & 0x02) {
    snareTone_.noteOn();
    snareEnv_.noteOn();
  }
  if (m & 0x04) clapEnv_.noteOn();
  if (m & 0x08) hatClosedEnv_.noteOn();
  if (m & 0x10) hatOpenEnv_.noteOn();
  if (m & 0x20) tom_.noteOn();
  if (m & 0x40) rim_.noteOn();
  if (m & 0x80) perc_.noteOn();
}

void DrumMachine::resync(uint32_t timelineMs) {
  releaseAll();
  nextStep_ = static_cast<uint32_t>(ceilf(timelineMs / stepMs()));
}

void DrumMachine::onTimeline(uint32_t timelineMs) {
  if (clipCount_ == 0) {
    nextStep_ = static_cast<uint32_t>(timelineMs / stepMs()) + 1;
    releaseDue(nextStep_);
    return;
  }
  const float sm = stepMs();
  while (nextStep_ * sm <= timelineMs) {
    // A step that is more than a step and a half late (the loop was stalled) is skipped rather
    // than played as a burst.
    releaseDue(nextStep_);
    if (timelineMs - nextStep_ * sm < sm * 1.5f) {
      trigger(maskAt(nextStep_));
      triggerNotesAt(nextStep_);
    }
    ++nextStep_;
  }
}

void DrumMachine::stopPreview() {
  if (previewOn_) releaseAll();
  previewOn_ = false;
  recording_ = false;
  countIn_ = false;
}

void DrumMachine::setRecording(bool on) { recording_ = on && previewOn_ && !countIn_; }

void DrumMachine::startPreview(int pattern, bool record) {
  if (pattern < 0 || pattern >= kNumPatterns) return;
  if (previewOn_ && pattern == previewPattern_) {  // already looping this pattern: just toggle recording
    if (countIn_) {
      if (!record) stopPreview();  // cancel the count-in
      return;
    }
    recording_ = record;
    return;
  }
  releaseAll();
  recording_ = false;
  countIn_ = record;  // recording starts after a one-bar count-in
  if (record) Serial.println("REC: count-in");
  previewPattern_ = pattern;
  previewOn_ = true;
  previewStartMs_ = millis();
  previewNext_ = 0;
  previewStep_ = record ? -1 : 0;
}

void DrumMachine::update() {
  // Auditioned notes end on a timer, whatever the transport is doing.
  uint32_t now = millis();
  for (int i = 0; i < instCount_; ++i) {
    for (int v = 0; v < Synth::kVoices; ++v) {
      const uint32_t until = voiceAuditionUntilMs_[i][v];
      if (until != 0 && until != kHeldForever && now >= until) {
        inst_[i].noteOff(v);
        voiceAuditionUntilMs_[i][v] = 0;
      }
    }
  }
  // Display-played notes and the display's sustain pedal are kept alive by the display; if it stops
  // saying so (a lost message, a crash) they are let go rather than left ringing.
  for (int i = 0; i < kLiveSlots; ++i) {
    if (live_[i].used && !live_[i].sustained && live_[i].fromDisplay && now - live_[i].refreshMs > 600) {
      Serial.printf("KEYS: note %d released (display stopped refreshing it)\n", live_[i].midi);
      noteUp(live_[i].inst, live_[i].midi);
    }
  }
  if (sustain_ && sustainFromDisplay_ && now - sustainRefreshMs_ > 600) {
    Serial.println("KEYS: sustain released (display stopped refreshing it)");
    setSustain(false);
  }
  if (!previewOn_) return;
  const float sm = stepMs();
  if (countIn_) {  // one bar of clicks (accent on beat 1), then the loop starts and recording begins
    uint32_t counted = now - previewStartMs_;
    while (countIn_ && previewNext_ * sm <= counted) {
      if (previewNext_ >= static_cast<uint32_t>(kSteps)) {  // a full bar has been counted in
        countIn_ = false;
        recording_ = true;
        Serial.println("REC: recording");
        previewStartMs_ += static_cast<uint32_t>(kSteps * sm);  // the loop's first step lands on the bar line
        previewNext_ = 0;
        previewStep_ = 0;
        break;
      }
      if (previewNext_ % 4 == 0) clickBeat(previewNext_ == 0);
      ++previewNext_;
    }
    if (countIn_) return;
  }
  uint32_t elapsed = millis() - previewStartMs_;
  while (previewNext_ * sm <= elapsed) {
    int step = static_cast<int>(previewNext_ % kSteps);
    releaseDue(previewNext_);
    uint8_t mask = patternMask(previewPattern_, step);
    if (recording_) {  // a hit just played live and recorded on this step must not sound twice
      const uint32_t t = millis();
      for (int r = 0; r < kRows; ++r) {
        if ((mask & (1 << r)) && t - rowHitMs_[r] < 100) mask &= static_cast<uint8_t>(~(1 << r));
      }
    }
    trigger(mask);
    triggerNotes(previewPattern_, step, previewNext_);
    if (recording_ && clickOn_ && step % 4 == 0) clickBeat(step == 0);
    previewStep_ = step;
    ++previewNext_;
  }
}

uint32_t DrumMachine::signature() const {
  uint32_t h = 2166136261u;
  h = (h ^ static_cast<uint32_t>(bpm_)) * 16777619u;
  h = (h ^ static_cast<uint32_t>(instCount_)) * 16777619u;
  for (int inst = 0; inst < instCount_; ++inst) {
    for (int i = 0; i < Synth::kParams; ++i) h = (h ^ static_cast<uint32_t>(inst_[inst].param(i) + 100)) * 16777619u;
  }
  for (int i = 0; i < kPads; ++i) {
    h = (h ^ static_cast<uint32_t>(padRoute_[i] + 1)) * 16777619u;
    h = (h ^ static_cast<uint32_t>(padNote_[i])) * 16777619u;
  }
  for (int p = 0; p < kNumPatterns; ++p) {
    for (int r = 0; r < kRows; ++r) h = (h ^ rows_[p][r]) * 16777619u;
    for (int inst = 0; inst < instCount_; ++inst) {
      h = (h ^ static_cast<uint32_t>(noteCount_[p][inst])) * 16777619u;
      for (int i = 0; i < noteCount_[p][inst]; ++i) {
        h = (h ^ notes_[p][inst][i].midi) * 16777619u;
        h = (h ^ notes_[p][inst][i].start) * 16777619u;
        h = (h ^ notes_[p][inst][i].len) * 16777619u;
      }
    }
  }
  h = (h ^ static_cast<uint32_t>(clipCount_)) * 16777619u;
  for (int i = 0; i < clipCount_; ++i) {
    h = (h ^ clips_[i].pattern) * 16777619u;
    h = (h ^ clips_[i].startBar) * 16777619u;
    h = (h ^ clips_[i].lenBars) * 16777619u;
  }
  return h;
}

}  // namespace dubbox
