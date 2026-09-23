#include "esp_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SD.h>

#include "storage.h"

namespace dubbox {

namespace {
constexpr uint32_t kBaud = 2000000;
constexpr uint32_t kStateIntervalMs = 50;
constexpr uint32_t kMixIntervalMs = 66;
constexpr uint32_t kRefreshMs = 2000;
constexpr int kColsPerChunk = 60;

// Extra UART transmit buffer, in RAM2 so it costs no RAM1 stack headroom.
DMAMEM uint8_t g_txExtra[2048];
DMAMEM uint8_t g_rxExtra[16384];  // the default receive buffer is tiny; the display can send many lines and upload chunks
DMAMEM uint8_t g_upBuf[1024];
constexpr uint32_t kUploadTimeoutMs = 6000;
constexpr int kUploadChunk = 1024;

struct Upload {
  bool active = false;
  uint32_t size = 0;
  uint32_t got = 0;
  uint32_t nextSeq = 0;
  uint32_t lastMs = 0;
  char path[72] = {0};
};
Upload g_up;
File g_upFile;
uint32_t g_upWriteUs = 0, g_upWriteMaxUs = 0;

uint16_t crc16(const uint8_t* data, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; ++b) crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}
DMAMEM char g_fileNames[kMaxWavFileEntries][kMaxWavFilenameLen];

int permille(float v) {
  int p = static_cast<int>(v * 1000.0f + 0.5f);
  return p < 0 ? 0 : (p > 1000 ? 1000 : p);
}
}  // namespace

FLASHMEM void EspLink::begin(ProjectManager* projects) {
  projects_ = projects;
  Serial1.addMemoryForWrite(g_txExtra, sizeof(g_txExtra));
  Serial1.addMemoryForRead(g_rxExtra, sizeof(g_rxExtra));
  Serial1.begin(kBaud);
}

// Drops the line (returns false) if the UART transmit buffer can't take it
// whole, so it never blocks.
bool EspLink::sendLine(const char* line) {
  size_t n = strlen(line);
  if (static_cast<size_t>(Serial1.availableForWrite()) < n + 1) return false;
  Serial1.write(line, n);
  Serial1.write('\n');
  return true;
}

void EspLink::setWaveform(int track, const uint8_t* peaks, int columns) {
  if (track < 0 || track >= kNumTracks) return;
  wave_[track] = peaks;
  waveCols_[track] = columns;
  waveOffset_[track] = 0;
  waveQueued_[track] = true;
}

void EspLink::setBeats(int track, const BeatInfo& beats) {
  if (track < 0 || track >= kNumTracks) return;
  beats_[track] = beats;
  beatsQueued_[track] = true;
}

FLASHMEM void EspLink::clearWaveform(int track) {
  if (track < 0 || track >= kNumTracks) return;
  wave_[track] = nullptr;
  waveCols_[track] = 0;
  waveOffset_[track] = 0;
  waveQueued_[track] = false;
}

void EspLink::sendProjectList() {
  if (!listPending_ || projects_ == nullptr) return;
  if (listSendIndex_ == 0) {
    listCount_ = projects_->listNames(listNames_, kMaxProjects);
  }
  char line[64];
  if (listCount_ == 0) {
    snprintf(line, sizeof(line), "L,0,0,");
  } else {
    snprintf(line, sizeof(line), "L,%d,%d,%s", listSendIndex_, listCount_, listNames_[listSendIndex_]);
  }
  if (!sendLine(line)) return;
  ++listSendIndex_;
  if (listSendIndex_ >= listCount_) {
    listPending_ = false;
    listSendIndex_ = 0;
  }
}

FLASHMEM void EspLink::sendFileList(AudioEngine& engine) {
  if (!filePending_) return;
  if (fileSendIndex_ == 0) {
    fileCount_ = engine.isPlaying() ? -1 : listWavFiles(g_fileNames);
  }
  char line[96];
  if (fileCount_ <= 0) {
    snprintf(line, sizeof(line), "E,0,%d,", fileCount_);
  } else {
    snprintf(line, sizeof(line), "E,%d,%d,%s", fileSendIndex_, fileCount_, g_fileNames[fileSendIndex_]);
  }
  if (!sendLine(line)) return;
  ++fileSendIndex_;
  if (fileCount_ <= 0 || fileSendIndex_ >= fileCount_) {
    filePending_ = false;
    fileSendIndex_ = 0;
  }
}

void EspLink::sendWaveChunks() {
  static const char kHex[] = "0123456789abcdef";
  for (int t = 0; t < kNumTracks; ++t) {
    if (!waveQueued_[t] || wave_[t] == nullptr) continue;
    int count = waveCols_[t] - waveOffset_[t];
    if (count > kColsPerChunk) count = kColsPerChunk;
    char line[200];
    int n = snprintf(line, sizeof(line), "W,%d,%d,%d,%d,", t, waveCols_[t], waveOffset_[t], count);
    for (int i = 0; i < count; ++i) {
      uint8_t v = wave_[t][waveOffset_[t] + i];
      line[n++] = kHex[v >> 4];
      line[n++] = kHex[v & 0x0F];
    }
    line[n] = '\0';
    if (!sendLine(line)) return;  // buffer full: retry next update
    waveOffset_[t] += count;
    if (waveOffset_[t] >= waveCols_[t]) waveQueued_[t] = false;
    return;  // one chunk per update keeps the loop short
  }
}

FLASHMEM void EspLink::handleLine(AudioEngine& engine, const char* line) {
  switch (line[0]) {
    case 'H':
      for (int t = 0; t < kNumTracks; ++t) {
        lastClipSig_[t] = 0;
        lastFxSig_[t] = 0;
        if (wave_[t] != nullptr) {
          waveOffset_[t] = 0;
          waveQueued_[t] = true;
        }
      }
      for (int p = 0; p < DrumMachine::kNumPatterns; ++p) lastPatSig_[p] = 0;
      lastPatClipSig_ = 0;
      lastPadSig_ = 0;
      lastInstSig_ = 0;
      for (int i = 0; i < DrumMachine::kMaxInstruments; ++i) { lastSynthSig_[i] = 0; lastModSig_[i] = 0; }
      lastRhythmSig_ = 0;
      for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
        for (int i = 0; i < DrumMachine::kMaxInstruments; ++i) lastNoteSig_[p][i] = 0;
      }
      listPending_ = true;
      listSendIndex_ = 0;
      lastOpen_ = -1;
      lastVolume_ = -1;
      lastBeatMask_ = -1;
      for (int t = 0; t < kNumTracks; ++t) beatsQueued_[t] = true;
      Serial.println("ESP: hello, resending state");
      break;
    case 'G':
      listPending_ = true;
      listSendIndex_ = 0;
      break;
    case 'D':
      filePending_ = true;
      fileSendIndex_ = 0;
      break;
    case 'T': {
      int track = 0, consumed = 0;
      if (projects_ != nullptr && sscanf(line, "T,%d,%n", &track, &consumed) >= 1 && consumed > 0 &&
          !engine.isPlaying()) {
        projects_->assignTrack(track, line + consumed);
      }
      break;
    }
    case 'R': {
      int track = 0;
      if (projects_ != nullptr && sscanf(line, "R,%d", &track) == 1 && !engine.isPlaying()) {
        projects_->clearTrack(track);
      }
      break;
    }
    case 'N':
      if (projects_ != nullptr && line[1] == ',') {
        bool ok = projects_->createNew(line + 2);
        Serial.printf("ESP: new project '%s' -> %s\n", line + 2, ok ? "ok" : "rejected");
        listPending_ = true;
        listSendIndex_ = 0;
      }
      break;
    case 'O':
      if (projects_ != nullptr && line[1] == ',') {
        bool ok = projects_->openProject(line + 2);
        Serial.printf("ESP: open project '%s' -> %s\n", line + 2, ok ? "ok" : "failed");
      }
      break;
    case 'Q':
      if (projects_ != nullptr && projects_->isOpen()) {
        projects_->closeProject();
        Serial.println("ESP: project closed");
      }
      break;
    case 'P':
      if (g_up.active) break;  // the SD card is busy with an upload
      engine.togglePlayPauseAll();
      Serial.printf("ESP: play/pause -> %s\n", engine.isPlaying() ? "play" : "pause");
      break;
    case 'U': {
      int vol = 0;
      if (sscanf(line, "U,%d", &vol) == 1) {
        engine.setMasterVolume(constrain(vol, 0, 1000) / 1000.0f);
        Serial.printf("ESP: master volume -> %d/1000\n", vol);
      }
      break;
    }
    case 'B': {
      long ms = 0;
      if (sscanf(line, "B,%ld", &ms) == 1 && ms >= 0) {
        engine.seekAllToMillis(static_cast<uint32_t>(ms));
        Serial.printf("ESP: seek %ld -> pos %lu\n", ms,
                      static_cast<unsigned long>(engine.transportPositionMillis()));
      }
      break;
    }
    case 'Z':
      engine.seekAllToMillis(0);
      Serial.println("ESP: rewind");
      break;
    case 'K': {
      int track = 0, clip = 0;
      long startMs = 0, endMs = 0;
      if (sscanf(line, "K,%d,%d,%ld,%ld", &track, &clip, &startMs, &endMs) == 4 && track >= 0 &&
          track < kNumTracks && startMs >= 0 && endMs >= 0) {
        engine.setClipWindow(track, clip, static_cast<uint32_t>(startMs),
                             static_cast<uint32_t>(endMs));
      }
      break;
    }
    case 'E': {
      int track = 0;
      long ms = 0;
      if (sscanf(line, "E,%d,%ld", &track, &ms) == 2 && track >= 0 && track < kNumTracks && ms < 0) {
        // A negative position means "undo every snip": one clip over the whole file again.
        engine.setTrackClips(track, nullptr, nullptr, nullptr, 0);
        Serial.printf("ESP: track %d reset to one clip\n", track + 1);
      } else if (sscanf(line, "E,%d,%ld", &track, &ms) == 2 && track >= 0 && track < kNumTracks &&
                 ms >= 0) {
        bool ok = engine.splitClipAt(track, static_cast<uint32_t>(ms));
        Serial.printf("ESP: snip track %d at %ld ms -> %s\n", track + 1, ms, ok ? "ok" : "no room");
      }
      break;
    }
    case 'L': {
      int track = 0, clip = 0;
      long off = 0;
      if (sscanf(line, "L,%d,%d,%ld", &track, &clip, &off) == 3 && track >= 0 &&
          track < kNumTracks && clip >= 0 && clip < engine.trackClipCount(track)) {
        engine.setClipOffsetMs(track, clip, static_cast<int32_t>(off));
        Serial.printf("ESP: track %d clip %d offset -> %ld ms\n", track + 1, clip,
                      static_cast<long>(engine.clipOffsetMs(track, clip)));
      }
      break;
    }
    case 'M': {
      int track = 0, clip = 0;
      if (sscanf(line, "M,%d,%d", &track, &clip) == 2 && track >= 0 && track < kNumTracks) {
        bool ok = engine.mergeClips(track, clip);
        Serial.printf("ESP: undo snip on track %d clip %d -> %s\n", track + 1, clip, ok ? "ok" : "nothing to join");
      }
      break;
    }
    case 's': {
      int pat = 0, row = 0, step = 0, on = 0;
      if (sscanf(line, "s,%d,%d,%d,%d", &pat, &row, &step, &on) == 4) {
        engine.drums().setStep(pat, row, step, on != 0);
      }
      break;
    }
    case 'k': {  // a pad press (or release) sent over the link: same as the physical pad, handy for testing
      int pad = 0, down = 1;
      int got = sscanf(line, "k,%d,%d", &pad, &down);
      if (got >= 1) {
        if (got == 2 && down == 0) {
          engine.drums().padReleased(pad);
        } else {
          engine.drums().padPressed(pad);
        }
      }
      break;
    }
    case '@': {
      int inst=-1, consumed=0, type=0; modular::Patch patch;
      if (sscanf(line, "@,%d,%n", &inst, &consumed)==1 && consumed>0 && modular::parse(line+consumed,type,patch)) {
        bool accepted = engine.drums().setModular(inst,type==1,patch);
        Serial.printf("MODULAR: slot %d type %d patch %08lx %s\n", inst, type, (unsigned long)modular::signature(patch), accepted ? "accepted" : "rejected");
      }
      break;
    }
    case 'g': {  // an instrument's whole patch
      int inst = 0, consumed = 0;
      if (sscanf(line, "g,%d%n", &inst, &consumed) != 1 || inst < 0 || inst >= engine.drums().instrumentCount()) break;
      int v[Synth::kParams];
      for (int i = 0; i < Synth::kParams; ++i) v[i] = engine.drums().synthParam(inst, i);  // keep any missing field
      const char* q = line + consumed;
      for (int i = 0; i < Synth::kParams; ++i) {
        if (*q != ',') break;
        char* end = nullptr;
        long x = strtol(q + 1, &end, 10);
        if (end == q + 1) break;
        v[i] = static_cast<int>(x);
        q = end;
      }
      engine.drums().setSynthAll(inst, v);
      break;
    }
    case 'e': {
      int midi = -1;
      if (sscanf(line, "e,%d", &midi) == 1) engine.drums().setNoteHold(midi);
      break;
    }
    case 'o': {
      int inst = 0, midi = 0, on = 0;
      if (sscanf(line, "o,%d,%d,%d", &inst, &midi, &on) == 3) {
        if (on == 1) {
          engine.drums().noteDown(inst, midi, true);
        } else if (on == 2) {
          engine.drums().refreshNote(inst, midi);
        } else {
          engine.drums().noteUp(inst, midi);
        }
      }
      break;
    }
    case 'z': {
      int on = 0;
      if (sscanf(line, "z,%d", &on) == 1) engine.drums().setSustain(on != 0, true);
      break;
    }
    case 'b': {
      int on = 1;
      if (sscanf(line, "b,%d", &on) == 1) engine.drums().setClickOn(on != 0);
      break;
    }
    case 'v': {
      int mode = 0, inst = -1;
      int got = sscanf(line, "v,%d,%d", &mode, &inst);
      if (got >= 1) engine.drums().setPadMode(mode, got == 2 ? inst : -1);
      break;
    }
    case 'h': {
      int row = -1;
      if (sscanf(line, "h,%d", &row) == 1) engine.drums().setRouteHold(row);
      break;
    }
    case 'i': {
      int inst = 0, on = 0, type = 0;
      if (sscanf(line, "i,%d,%d,%d", &inst, &on, &type) >= 2) {
        if (on != 0) {
          if (inst == engine.drums().instrumentCount() && (type == 0 || type == 1)) engine.drums().addInstrument(type == 1);
        } else {
          engine.drums().removeInstrument(inst);
        }
      }
      break;
    }
    case 'n': {
      int pat = 0, inst = 0, midi = 0, start = 0, len = 0;
      if (sscanf(line, "n,%d,%d,%d,%d,%d", &pat, &inst, &midi, &start, &len) == 5) {
        engine.drums().setNote(pat, inst, midi, start, len);
      }
      break;
    }
    case 'x': {
      int pat = 0, inst = 0, midi = 0, start = 0;
      if (sscanf(line, "x,%d,%d,%d,%d", &pat, &inst, &midi, &start) == 4) engine.drums().removeNote(pat, inst, midi, start);
      break;
    }
    case 'c': {
      int pat = 0, inst = 0;
      if (sscanf(line, "c,%d,%d", &pat, &inst) == 2) engine.drums().clearNotes(pat, inst);
      break;
    }
    case 't': {
      int inst = 0, midi = 0;
      if (sscanf(line, "t,%d,%d", &inst, &midi) == 2) engine.drums().audition(inst, midi);
      break;
    }
    case 'w': {
      int pat = 0;
      if (sscanf(line, "w,%d", &pat) == 1) engine.drums().clearPattern(pat);
      break;
    }
    case 'j': {
      int bpm = 0;
      if (sscanf(line, "j,%d", &bpm) == 1) {
        engine.drums().setBpm(bpm);
        engine.drums().resync(engine.transportPositionMillis());
      }
      break;
    }
    case 'f': {
      int on = 0, pat = 0, rec = 0;
      int got = sscanf(line, "f,%d,%d,%d", &on, &pat, &rec);
      if (got >= 2) {
        if (on != 0 && !engine.isPlaying()) {
          engine.drums().startPreview(pat, rec != 0);
        } else {
          engine.drums().stopPreview();
        }
      }
      break;
    }
    case 'a': {
      int pat = 0, bar = 0, len = 0, index = -1;
      int got = sscanf(line, "a,%d,%d,%d,%d", &pat, &bar, &len, &index);
      if (got >= 3) engine.drums().addClip(pat, bar, len, got == 4 ? index : -1);
      break;
    }
    case 'd': {
      int index = 0;
      if (sscanf(line, "d,%d", &index) == 1) engine.drums().removeClip(index);
      break;
    }
    case 'm': {
      int index = 0, bar = 0;
      if (sscanf(line, "m,%d,%d", &index, &bar) == 2) engine.drums().moveClip(index, bar);
      break;
    }
    case 'l': {
      int index = 0, len = 0;
      if (sscanf(line, "l,%d,%d", &index, &len) == 2) engine.drums().resizeClip(index, len);
      break;
    }
    case 'I': {
      int track = 0, on = 0;
      if (projects_ != nullptr && sscanf(line, "I,%d,%d", &track, &on) == 2) {
        projects_->setBeatFlag(track, on != 0);
      }
      break;
    }
    case 'A': {
      int track = 0, type = 0;
      if (sscanf(line, "A,%d,%d", &track, &type) == 2 && track >= 0 && track < kNumTracks) {
        engine.addTrackFxSlot(track, type);
        Serial.printf("ESP: add fx %d on track %d\n", type, track + 1);
      }
      break;
    }
    case 'X': {
      int track = 0, slot = 0;
      if (sscanf(line, "X,%d,%d", &track, &slot) == 2 && track >= 0 && track < kNumTracks &&
          slot >= 0 && slot < engine.trackFxSlotCount(track)) {
        engine.setTrackActiveFxSlot(track, slot);
      }
      break;
    }
    case '~': {
      int track, pan;
      if (sscanf(line, "~,%d,%d", &track, &pan) == 2 && track >= 0 && track < kNumTracks)
        engine.setTrackPan(track, pan);
      break;
    }
    case 'V': {
      int track = 0, wet = 0;
      if (sscanf(line, "V,%d,%d", &track, &wet) == 2 && track >= 0 && track < kNumTracks) {
        engine.setTrackFxWetLevel(track, constrain(wet, 0, 1000) / 1000.0f);
      }
      break;
    }
    case 'Y': {
      int track = 0, byp = 0;
      if (sscanf(line, "Y,%d,%d", &track, &byp) == 2 && track >= 0 && track < kNumTracks) {
        engine.setTrackFxBypassed(track, byp != 0);
      }
      break;
    }
    case 'S':
      startUpload(engine, line + 1);
      break;
    case 'W': {
      unsigned long seq = 0;
      int len = 0;
      unsigned crc = 0;
      if (sscanf(line, "W,%lu,%d,%x", &seq, &len, &crc) != 3 || !g_up.active || seq != g_up.nextSeq ||
          len <= 0 || len > kUploadChunk || g_up.got + static_cast<uint32_t>(len) > g_up.size) {
        if (g_up.active) abortUpload("seq");
        break;
      }
      binLeft_ = static_cast<size_t>(len);
      binPos_ = 0;
      binSeq_ = static_cast<uint32_t>(seq);
      binCrc_ = static_cast<uint16_t>(crc);
      break;
    }
    case 'C':
      if (g_up.active) {
        abortUpload(nullptr);
        Serial.println("UPLOAD: cancelled by the display");
      }
      break;
    default:
      break;
  }
}

bool EspLink::uploading() const { return g_up.active; }

void EspLink::queueReply(const char* line) {
  strncpy(pendingReply_, line, sizeof(pendingReply_) - 1);
  pendingReply_[sizeof(pendingReply_) - 1] = '\0';
}

// Ends the upload; a non-null reason is reported to the ESP32 and the partial file is deleted.
void EspLink::abortUpload(const char* reason) {
  if (g_upFile) g_upFile.close();
  if (g_up.active) SD.remove(g_up.path);
  g_up.active = false;
  binLeft_ = 0;
  if (reason != nullptr) {
    char line[32];
    snprintf(line, sizeof(line), "Q,err,%s", reason);
    queueReply(line);
    Serial.printf("UPLOAD: aborted (%s)\n", reason);
  }
}

FLASHMEM void EspLink::startUpload(AudioEngine& engine, const char* args) {
  unsigned long size = 0;
  int consumed = 0;
  if (sscanf(args, ",%lu,%n", &size, &consumed) < 1 || consumed <= 0) return;
  if (g_up.active) abortUpload(nullptr);
  const char* want = args + consumed;
  if (engine.isPlaying() || engine.drums().previewing()) {
    queueReply("Q,err,busy");
    return;
  }
  if (size < 64 || size > 0x7FFF0000UL) {
    queueReply("Q,err,size");
    return;
  }
  char base[40];
  int n = 0;
  for (const char* p = want; *p != '\0' && n < 32; ++p) {
    char ch = *p;
    if (isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_') base[n++] = ch;
  }
  if (n == 0) {
    strcpy(base, "UPLOAD");
    n = 6;
  }
  base[n] = '\0';
  char name[48];
  snprintf(name, sizeof(name), "%s.wav", base);
  for (int i = 2; i < 100 && SD.exists(name); ++i) snprintf(name, sizeof(name), "%s-%d.wav", base, i);
  if (SD.exists(name)) {
    queueReply("Q,err,sd");
    return;
  }
  snprintf(g_up.path, sizeof(g_up.path), "%s", name);
  g_upFile = SD.open(g_up.path, FILE_WRITE);
  if (!g_upFile) {
    queueReply("Q,err,sd");
    return;
  }
  g_up.active = true;
  g_up.size = static_cast<uint32_t>(size);
  g_up.got = 0;
  g_up.nextSeq = 0;
  g_up.lastMs = millis();
  g_upWriteUs = g_upWriteMaxUs = 0;
  binLeft_ = 0;
  char line[72];
  snprintf(line, sizeof(line), "Q,ok,%s", name);
  queueReply(line);
  Serial.printf("UPLOAD: %s, %lu bytes\n", name, size);
}

// A whole chunk has arrived in g_upBuf: check it, write it, acknowledge.
void EspLink::finishChunk() {
  const size_t len = binPos_;
  binLeft_ = 0;
  if (!g_up.active) return;
  if (crc16(g_upBuf, len) != binCrc_) {
    abortUpload("crc");
    return;
  }
  const uint32_t w0 = micros();
  const bool wrote = g_upFile.write(g_upBuf, len) == len;
  const uint32_t wUs = micros() - w0;
  g_upWriteUs += wUs;
  if (wUs > g_upWriteMaxUs) g_upWriteMaxUs = wUs;
  if (!wrote) {
    abortUpload("sd");
    return;
  }
  g_up.got += static_cast<uint32_t>(len);
  g_up.nextSeq = binSeq_ + 1;
  g_up.lastMs = millis();
  char line[40];
  if (g_up.got >= g_up.size) {
    g_upFile.close();
    g_up.active = false;
    snprintf(line, sizeof(line), "Q,done,%s", g_up.path);
    Serial.printf("UPLOAD: %s complete (%lu bytes, SD writes %lu ms total, slowest %lu us)\n", g_up.path,
                  static_cast<unsigned long>(g_up.size), static_cast<unsigned long>(g_upWriteUs / 1000),
                  static_cast<unsigned long>(g_upWriteMaxUs));
  } else {
    snprintf(line, sizeof(line), "Q,ack,%lu", static_cast<unsigned long>(g_up.nextSeq));
  }
  queueReply(line);
}

void EspLink::serviceUpload() {
  if (pendingReply_[0] != '\0' && sendLine(pendingReply_)) pendingReply_[0] = '\0';
  if (g_up.active && millis() - g_up.lastMs > kUploadTimeoutMs) abortUpload("timeout");
}

void EspLink::update(AudioEngine& engine, const float faders[kNumTracks]) {
  while (Serial1.available()) {
    if (binLeft_ > 0) {
      g_upBuf[binPos_++] = static_cast<uint8_t>(Serial1.read());
      if (--binLeft_ == 0) finishChunk();
      continue;
    }
    char c = static_cast<char>(Serial1.read());
    if (c == '\n' || c == '\r') {
      if (rxLen_ > 0) {
        rx_[rxLen_] = '\0';
        handleLine(engine, rx_);
        rxLen_ = 0;
      }
    } else if (rxLen_ < sizeof(rx_) - 1) {
      rx_[rxLen_++] = c;
    }
  }

  serviceUpload();

  uint32_t now = millis();
  char buf[96];

  if (now - lastStateMs_ >= kStateIntervalMs) {
    lastStateMs_ = now;
    uint32_t songMs = engine.songEndMs();
    snprintf(buf, sizeof(buf), "S,%d,%lu,%lu", engine.isPlaying() ? 1 : 0,
             static_cast<unsigned long>(engine.transportPositionMillis()),
             static_cast<unsigned long>(songMs));
    sendLine(buf);
  }

  if (now - lastMixMs_ >= kMixIntervalMs) {
    lastMixMs_ = now;
    snprintf(buf, sizeof(buf), "M,%d,%d,%d,%d,%d,%d,%d,%d", permille(faders[0]),
             permille(faders[1]), permille(faders[2]), permille(faders[3]),
             permille(engine.trackPeakLevel(0)), permille(engine.trackPeakLevel(1)),
             permille(engine.trackPeakLevel(2)), permille(engine.trackPeakLevel(3)));
    sendLine(buf);
  }

  bool refresh = now - lastRefreshMs_ >= kRefreshMs;
  if (refresh) lastRefreshMs_ = now;
  for (int i = 0; i < kNumTracks; ++i) {
    uint32_t clipSig = 2166136261u;
    for (int k = 0; k < engine.trackClipCount(i); ++k) {
      clipSig = (clipSig ^ engine.clipStartMs(i, k)) * 16777619u;
      clipSig = (clipSig ^ engine.clipEndMs(i, k)) * 16777619u;
      clipSig = (clipSig ^ static_cast<uint32_t>(engine.clipOffsetMs(i, k))) * 16777619u;
    }
    clipSig = (clipSig ^ engine.trackLengthMillis(i)) * 16777619u;
    if (refresh || clipSig != lastClipSig_[i]) {
      char clipLine[200];
      int n = snprintf(clipLine, sizeof(clipLine), "C,%d,%lu,%d", i,
                       static_cast<unsigned long>(engine.trackLengthMillis(i)),
                       engine.trackClipCount(i));
      for (int k = 0; k < engine.trackClipCount(i) && n < static_cast<int>(sizeof(clipLine)) - 24;
           ++k) {
        n += snprintf(clipLine + n, sizeof(clipLine) - n, ",%lu,%lu,%ld",
                      static_cast<unsigned long>(engine.clipStartMs(i, k)),
                      static_cast<unsigned long>(engine.clipEndMs(i, k)),
                      static_cast<long>(engine.clipOffsetMs(i, k)));
      }
      if (sendLine(clipLine)) lastClipSig_[i] = clipSig;
    }

    int pan = engine.trackPan(i);
    if (refresh || pan != lastPan_[i]) {
      snprintf(buf, sizeof(buf), "~,%d,%d", i, pan);
      if (sendLine(buf)) lastPan_[i] = pan;
    }
    int types[3] = {-1, -1, -1};
    int count = engine.trackFxSlotCount(i);
    for (int slot = 0; slot < count && slot < 3; ++slot) types[slot] = engine.trackFxSlotType(i, slot);
    int byp = engine.trackFxBypassed(i) ? 1 : 0;
    int wet = permille(engine.trackFxWetLevel(i));
    int active = engine.trackActiveFxSlot(i);
    int canFlange = engine.trackFxSlotTypeAvailable(i, AudioEngine::kFxFlange) ? 1 : 0;
    int canChorus = engine.trackFxSlotTypeAvailable(i, AudioEngine::kFxChorus) ? 1 : 0;
    uint32_t sig = 1;
    sig = sig * 31 + byp;
    sig = sig * 31 + wet;
    sig = sig * 31 + (active + 1);
    sig = sig * 31 + canFlange;
    sig = sig * 31 + canChorus;
    for (int slot = 0; slot < 3; ++slot) sig = sig * 31 + (types[slot] + 2);
    if (refresh || sig != lastFxSig_[i]) {
      snprintf(buf, sizeof(buf), "F,%d,%d,%d,%d,%d,%d,%d,%d,%d", i, byp, wet, active, canFlange,
               canChorus, types[0], types[1], types[2]);
      if (sendLine(buf)) lastFxSig_[i] = sig;
    }
  }

  {
    DrumMachine& drums = engine.drums();
    for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
      uint32_t sig = 2166136261u;
      for (int r = 0; r < DrumMachine::kRows; ++r) sig = (sig ^ drums.rowBits(p, r)) * 16777619u;
      if (refresh || sig != lastPatSig_[p]) {
        int n = snprintf(buf, sizeof(buf), "p,%d", p);
        for (int r = 0; r < DrumMachine::kRows; ++r) {
          n += snprintf(buf + n, sizeof(buf) - n, ",%04x", static_cast<unsigned>(drums.rowBits(p, r)));
        }
        if (sendLine(buf)) lastPatSig_[p] = sig;
      }
    }

    {
      const uint32_t instSig = static_cast<uint32_t>(drums.instrumentCount()) + 1;
      if (refresh || instSig != lastInstSig_) {
        snprintf(buf, sizeof(buf), "w,%d", drums.instrumentCount());
        if (sendLine(buf)) lastInstSig_ = instSig;
      }
    }
    for (int inst = 0; inst < drums.instrumentCount(); ++inst) {
      uint32_t modSig = modular::signature(drums.modularPatch(inst)) ^ uint32_t(drums.isModular(inst));
      if (refresh || modSig != lastModSig_[inst]) {
        char text[128], report[144];
        modular::format(text,sizeof(text),drums.isModular(inst),drums.modularPatch(inst));
        snprintf(report,sizeof(report),"@,%d,%s",inst,text);
        if (sendLine(report)) lastModSig_[inst]=modSig;
      }
      uint32_t synthSig = 1;
      for (int i = 0; i < Synth::kParams; ++i) synthSig = synthSig * 31 + (drums.synthParam(inst, i) + 100);
      if (refresh || synthSig != lastSynthSig_[inst]) {
        char sl[120];
        int n = snprintf(sl, sizeof(sl), "g,%d", inst);
        for (int i = 0; i < Synth::kParams; ++i) n += snprintf(sl + n, sizeof(sl) - n, ",%d", drums.synthParam(inst, i));
        if (sendLine(sl)) lastSynthSig_[inst] = synthSig;
      }
    }
    for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
      for (int inst = 0; inst < drums.instrumentCount(); ++inst) {
        uint32_t sig = 2166136261u;
        for (int i = 0; i < drums.noteCount(p, inst); ++i) {
          DrumMachine::Note nt = drums.note(p, inst, i);
          sig = (sig ^ nt.midi) * 16777619u;
          sig = (sig ^ nt.start) * 16777619u;
          sig = (sig ^ nt.len) * 16777619u;
        }
        sig = (sig ^ static_cast<uint32_t>(drums.noteCount(p, inst))) * 16777619u;
        if (refresh || sig != lastNoteSig_[p][inst]) {
          char noteLine[300];
          int n = snprintf(noteLine, sizeof(noteLine), "q,%d,%d,%d", p, inst, drums.noteCount(p, inst));
          for (int i = 0; i < drums.noteCount(p, inst); ++i) {
            DrumMachine::Note nt = drums.note(p, inst, i);
            n += snprintf(noteLine + n, sizeof(noteLine) - n, ",%d:%d:%d", nt.midi, nt.start, nt.len);
          }
          if (sendLine(noteLine)) lastNoteSig_[p][inst] = sig;
        }
      }
    }

    {
      uint32_t padSig = 1;
      for (int i = 0; i < DrumMachine::kPads; ++i) {
        padSig = padSig * 31 + (drums.padRoute(i) + 2);
        padSig = padSig * 31 + drums.padNote(i);
      }
      padSig = padSig * 31 + drums.padMode();
      if (refresh || padSig != lastPadSig_) {
        snprintf(buf, sizeof(buf), "u,%d,%d,%d,%d,%d,%d,%d,%d,%d", drums.padRoute(0), drums.padRoute(1),
                 drums.padRoute(2), drums.padRoute(3), drums.padMode(), drums.padNote(0),
                 drums.padNote(1), drums.padNote(2), drums.padNote(3));
        if (sendLine(buf)) lastPadSig_ = padSig;
      }
    }

    uint32_t clipSig = 2166136261u;
    for (int i = 0; i < drums.clipCount(); ++i) {
      DrumMachine::Clip cl = drums.clip(i);
      clipSig = (clipSig ^ cl.pattern) * 16777619u;
      clipSig = (clipSig ^ cl.startBar) * 16777619u;
      clipSig = (clipSig ^ cl.lenBars) * 16777619u;
    }
    clipSig = (clipSig ^ static_cast<uint32_t>(drums.clipCount())) * 16777619u;
    if (refresh || clipSig != lastPatClipSig_) {
      char clipsLine[200];
      int n = snprintf(clipsLine, sizeof(clipsLine), "y,%d", drums.clipCount());
      for (int i = 0; i < drums.clipCount(); ++i) {
        DrumMachine::Clip cl = drums.clip(i);
        n += snprintf(clipsLine + n, sizeof(clipsLine) - n, ",%d,%d,%d", cl.pattern, cl.startBar,
                      cl.lenBars);
      }
      if (sendLine(clipsLine)) lastPatClipSig_ = clipSig;
    }

    uint32_t rSig = static_cast<uint32_t>(drums.bpm()) | (drums.previewing() ? 0x10000u : 0u) |
                    (static_cast<uint32_t>(drums.previewPattern()) << 20) |
                    (static_cast<uint32_t>(drums.previewStep()) << 24) | (drums.recording() ? 0x40000000u : 0u) | (drums.countingIn() ? 0x20000000u : 0u) |
                    (drums.clickOn() ? 0x10000000u : 0u) | 0x80000000u;
    if (refresh || rSig != lastRhythmSig_) {
      snprintf(buf, sizeof(buf), "r,%d,%d,%d,%d,%d,%d", drums.bpm(), drums.previewing() ? 1 : 0,
               drums.previewPattern(), drums.previewStep(),
               drums.countingIn() ? 2 : (drums.recording() ? 1 : 0), drums.clickOn() ? 1 : 0);
      if (sendLine(buf)) lastRhythmSig_ = rSig;
    }
  }

  {
    int vol = permille(engine.masterVolume());
    if (refresh || vol != lastVolume_) {
      snprintf(buf, sizeof(buf), "U,%d", vol);
      if (sendLine(buf)) lastVolume_ = vol;
    }
  }

  if (projects_ != nullptr) {
    int mask = projects_->beatMask();
    if (refresh || mask != lastBeatMask_) {
      snprintf(buf, sizeof(buf), "I,%d", mask);
      if (sendLine(buf)) lastBeatMask_ = mask;
    }
  }

  if (projects_ != nullptr) {
    int open = projects_->isOpen() ? 1 : 0;
    const char* nm = projects_->name();
    if (refresh || open != lastOpen_ || strcmp(nm, lastName_) != 0) {
      snprintf(buf, sizeof(buf), "J,%d,%s", open, nm);
      if (sendLine(buf)) {
        lastOpen_ = open;
        strncpy(lastName_, nm, sizeof(lastName_) - 1);
        lastName_[sizeof(lastName_) - 1] = '\0';
      }
    }
    if (projects_->takeListDirty()) {
      listPending_ = true;
      listSendIndex_ = 0;
    }
  }

  for (int t = 0; t < kNumTracks; ++t) {
    if (!beatsQueued_[t]) continue;
    const BeatInfo& b = beats_[t];
    if (b.valid) {
      snprintf(buf, sizeof(buf), "B,%d,%ld,%lu,%ld", t, lroundf(b.bpm * 100.0f),
               static_cast<unsigned long>(b.firstDownbeatMs), lroundf(b.barMs * 10.0f));
    } else {
      snprintf(buf, sizeof(buf), "B,%d,0,0,0", t);
    }
    if (sendLine(buf)) beatsQueued_[t] = false;
  }

  sendProjectList();
  sendFileList(engine);
  sendWaveChunks();
}

}  // namespace dubbox
