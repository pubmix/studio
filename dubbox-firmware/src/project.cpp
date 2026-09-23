#include "project.h"

#include <SD.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dubbox {

namespace {
constexpr const char* kDir = "/PROJECTS";
constexpr uint32_t kAutosaveDelayMs = 1500;

bool endsWithPrj(const char* n) {
  size_t len = strlen(n);
  return len > 4 && strcasecmp(n + len - 4, ".PRJ") == 0;
}
}  // namespace

FLASHMEM void ProjectManager::begin(AudioEngine* engine) {
  engine_ = engine;
  SD.mkdir(kDir);
}

// Keeps A-Z 0-9 space - _ (upper-cased), trims trailing spaces. False if empty.
FLASHMEM bool ProjectManager::sanitize(const char* in, char* out) const {
  size_t n = 0;
  for (const char* p = in; *p && n < kProjectNameMax; ++p) {
    char c = *p;
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
              (c == ' ' && n > 0);
    if (ok) out[n++] = c;
  }
  while (n > 0 && out[n - 1] == ' ') --n;
  out[n] = '\0';
  return n > 0;
}

FLASHMEM void ProjectManager::pathFor(const char* name, char* out, size_t outLen) const {
  snprintf(out, outLen, "%s/%s.PRJ", kDir, name);
}

FLASHMEM int ProjectManager::listNames(char names[][kProjectNameMax + 1], int maxCount) {
  int count = 0;
  File dir = SD.open(kDir);
  if (!dir) return 0;
  while (count < maxCount) {
    File f = dir.openNextFile();
    if (!f) break;
    if (!f.isDirectory() && endsWithPrj(f.name())) {
      size_t len = strlen(f.name()) - 4;
      if (len > kProjectNameMax) len = kProjectNameMax;
      memcpy(names[count], f.name(), len);
      names[count][len] = '\0';
      ++count;
    }
    f.close();
  }
  dir.close();
  return count;
}

namespace {
constexpr int32_t kNoOffset = INT32_MIN;  // clip has no saved offset (older project file)
}

uint32_t ProjectManager::clipSignature(int t) const {
  uint32_t h = 2166136261u;
  int n = engine_->trackClipCount(t);
  h = (h ^ static_cast<uint32_t>(n)) * 16777619u;
  for (int k = 0; k < n; ++k) {
    h = (h ^ engine_->clipStartMs(t, k)) * 16777619u;
    h = (h ^ engine_->clipEndMs(t, k)) * 16777619u;
    h = (h ^ static_cast<uint32_t>(engine_->clipOffsetMs(t, k))) * 16777619u;
  }
  return h;
}

void ProjectManager::setBeatFlag(int track, bool on) {
  if (track < 0 || track >= kNumTracks) return;
  if (on) {
    beatMask_ |= static_cast<uint8_t>(1 << track);
  } else {
    beatMask_ &= static_cast<uint8_t>(~(1 << track));
  }
}

FLASHMEM bool ProjectManager::save() {
  if (!open_) return false;
  char path[64];
  pathFor(name_, path, sizeof(path));
  SD.remove(path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  f.printf("NAME=%s\n", name_);
  f.printf("BEAT=%d%d%d%d\n", (beatMask_ >> 0) & 1, (beatMask_ >> 1) & 1, (beatMask_ >> 2) & 1,
           (beatMask_ >> 3) & 1);
  DrumMachine& drums = engine_->drums();
  f.printf("BPM=%d\n", drums.bpm());
  f.printf("PADS=%d,%d,%d,%d\n", drums.padRoute(0), drums.padRoute(1), drums.padRoute(2), drums.padRoute(3));
  f.printf("INST=%d\n", drums.instrumentCount());
  for (int inst = 0; inst < drums.instrumentCount(); ++inst) {
    char patchText[128];
    modular::format(patchText, sizeof(patchText), drums.isModular(inst), drums.modularPatch(inst));
    f.printf("MOD%d=%s\n", inst, patchText);
    f.printf("SYNTH%d=", inst);
    for (int i = 0; i < Synth::kParams; ++i) f.printf(i ? ",%d" : "%d", drums.synthParam(inst, i));
    f.printf("\n");
  }
  f.printf("PADNOTES=%d,%d,%d,%d\n", drums.padNote(0), drums.padNote(1), drums.padNote(2), drums.padNote(3));
  for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
    f.printf("P%d=", p);
    for (int r = 0; r < DrumMachine::kRows; ++r) {
      f.printf(r ? ",%04x" : "%04x", static_cast<unsigned>(drums.rowBits(p, r)));
    }
    f.printf("\n");
  }
  for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
    for (int inst = 0; inst < drums.instrumentCount(); ++inst) {
      if (drums.noteCount(p, inst) == 0) continue;
      f.printf("NT%d%d=", p, inst);
      for (int i = 0; i < drums.noteCount(p, inst); ++i) {
        DrumMachine::Note nt = drums.note(p, inst, i);
        f.printf(i ? ",%d:%d:%d" : "%d:%d:%d", nt.midi, nt.start, nt.len);
      }
      f.printf("\n");
    }
  }
  f.printf("PC=");
  for (int i = 0; i < drums.clipCount(); ++i) {
    DrumMachine::Clip cl = drums.clip(i);
    f.printf(i ? ",%d:%d:%d" : "%d:%d:%d", cl.pattern, cl.startBar, cl.lenBars);
  }
  f.printf("\n");
  for (int i = 0; i < kNumTracks; ++i) {
    f.printf("PAN%d=%d\n", i, engine_->trackPan(i));
    savedPan_[i] = engine_->trackPan(i);
    const char* file = engine_->trackFilename(i);
    if (file == nullptr) {
      f.printf("T%d=\n", i);
      savedFile_[i][0] = '\0';
      savedClipSig_[i] = 0;
    } else {
      f.printf("T%d=%s", i, file);
      for (int k = 0; k < engine_->trackClipCount(i); ++k) {
        f.printf(",%lu,%lu", static_cast<unsigned long>(engine_->clipStartMs(i, k)),
                 static_cast<unsigned long>(engine_->clipEndMs(i, k)));
      }
      f.printf("\n");
      f.printf("C%d=", i);
      for (int k = 0; k < engine_->trackClipCount(i); ++k) {
        f.printf(k ? ",%ld" : "%ld", static_cast<long>(engine_->clipOffsetMs(i, k)));
      }
      f.printf("\n");
      strncpy(savedFile_[i], file, sizeof(savedFile_[i]) - 1);
      savedFile_[i][sizeof(savedFile_[i]) - 1] = '\0';
      savedClipSig_[i] = clipSignature(i);
    }
  }
  f.close();
  savedBeatMask_ = beatMask_;
  savedDrumSig_ = drums.signature();
  dirty_ = false;
  Serial.printf("PROJECT: saved %s\n", name_);
  return true;
}

FLASHMEM bool ProjectManager::loadFile(const char* name, Loaded& out) {
  char path[64];
  pathFor(name, path, sizeof(path));
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  for (int i = 0; i < kNumTracks; ++i) {
    out.files[i][0] = '\0';
    out.clipN[i] = 0;
  }
  out.beatMask = 0;
  out.bpm = DrumMachine::kDefaultBpm;
  out.patClipN = 0;
  out.padRoute[0] = 0;
  out.padRoute[1] = 1;
  out.padRoute[2] = 3;
  out.padRoute[3] = 2;
  out.padNote[0] = 60;
  out.padNote[1] = 64;
  out.padNote[2] = 67;
  out.padNote[3] = 72;
  out.instCount = 0;
  out.oldFormat = false;
  for (int i = 0; i < DrumMachine::kMaxInstruments; ++i) out.hasSynth[i] = false;
  for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
    for (int i = 0; i < DrumMachine::kMaxInstruments; ++i) out.noteN[p][i] = 0;
  }
  for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
    for (int r = 0; r < DrumMachine::kRows; ++r) out.rows[p][r] = 0;
  }
  for (int i = 0; i < kNumTracks; ++i) {
    out.offsetMs[i] = 0;
    for (int k = 0; k < AudioEngine::kMaxClips; ++k) out.clipO[i][k] = kNoOffset;
  }
  char line[320];  // the longest lines are a full piano part (24 notes)
  while (f.available()) {
    size_t n = 0;
    while (f.available() && n < sizeof(line) - 1) {
      char c = static_cast<char>(f.read());
      if (c == '\n') break;
      if (c != '\r') line[n++] = c;
    }
    line[n] = '\0';
    int panTrack, panValue;
    if (sscanf(line, "PAN%d=%d", &panTrack, &panValue) == 2 && panTrack >= 0 && panTrack < kNumTracks) {
      out.pan[panTrack] = constrain(panValue, -1000, 1000);
      continue;
    }
    if (strncmp(line, "OFF=", 4) == 0) {
      const char* q = line + 4;
      for (int t = 0; t < kNumTracks && *q != '\0'; ++t) {
        char* end = nullptr;
        out.offsetMs[t] = static_cast<int32_t>(strtol(q, &end, 10));
        if (end == q) break;
        q = (*end == ',') ? end + 1 : end;
      }
    } else if (n >= 3 && line[0] == 'C' && line[1] >= '0' && line[1] < '0' + kNumTracks &&
               line[2] == '=') {
      int t = line[1] - '0';
      const char* q = line + 3;
      for (int k = 0; k < AudioEngine::kMaxClips && *q != '\0'; ++k) {
        char* end = nullptr;
        out.clipO[t][k] = static_cast<int32_t>(strtol(q, &end, 10));
        if (end == q) break;
        q = (*end == ',') ? end + 1 : end;
      }
    } else if (strncmp(line, "INST=", 5) == 0) {
      out.instCount = constrain(atoi(line + 5), 0, DrumMachine::kMaxInstruments);
    } else if (n >= 5 && strncmp(line, "MOD", 3) == 0 && line[3] >= '0' && line[3] < '0' + DrumMachine::kMaxInstruments && line[4] == '=') {
      int i = line[3] - '0';
      modular::parse(line + 5, out.modularType[i], out.modularPatch[i]);
    } else if (strncmp(line, "SYNTH", 5) == 0 &&
               (line[5] == '=' || (line[5] >= '0' && line[5] < '0' + DrumMachine::kMaxInstruments && line[6] == '='))) {
      int idx = 0;  // "SYNTH=" is an older file: its single synth is instrument 1
      const char* q = line + 6;
      if (line[5] != '=') {
        idx = line[5] - '0';
        q = line + 7;
      } else {
        out.oldFormat = true;
      }
      for (int i = 0; i < Synth::kParams && *q != '\0'; ++i) {
        char* end = nullptr;
        out.synth[idx][i] = static_cast<int>(strtol(q, &end, 10));
        if (end == q) break;
        out.hasSynth[idx] = true;
        q = (*end == ',') ? end + 1 : end;
      }
    } else if (strncmp(line, "PADNOTES=", 9) == 0) {
      const char* q = line + 9;
      for (int i = 0; i < DrumMachine::kPads && *q != '\0'; ++i) {
        char* end = nullptr;
        out.padNote[i] = static_cast<int>(strtol(q, &end, 10));
        if (end == q) break;
        q = (*end == ',') ? end + 1 : end;
      }
    } else if (strncmp(line, "PADS=", 5) == 0) {
      const char* q = line + 5;
      for (int i = 0; i < DrumMachine::kPads && *q != '\0'; ++i) {
        char* end = nullptr;
        out.padRoute[i] = static_cast<int>(strtol(q, &end, 10));
        if (end == q) break;
        q = (*end == ',') ? end + 1 : end;
      }
    } else if (strncmp(line, "BPM=", 4) == 0) {
      out.bpm = constrain(atoi(line + 4), DrumMachine::kMinBpm, DrumMachine::kMaxBpm);
    } else if ((n >= 5 && line[0] == 'N' && line[1] == 'T' && line[2] >= '0' &&
                line[2] < '0' + DrumMachine::kNumPatterns && line[3] >= '0' &&
                line[3] < '0' + DrumMachine::kMaxInstruments && line[4] == '=') ||
               (n >= 4 && line[0] == 'P' && line[1] == 'N' && line[2] >= '0' &&
                line[2] < '0' + DrumMachine::kNumPatterns && line[3] == '=')) {
      const bool old = line[0] == 'P';  // "PN<p>=" is an older file's single piano part: instrument 1
      const int p = line[old ? 2 : 2] - '0';
      const int inst = old ? 0 : line[3] - '0';
      if (old) out.oldFormat = true;
      const char* q = line + (old ? 4 : 5);
      while (*q != '\0' && out.noteN[p][inst] < DrumMachine::kMaxNotes) {
        char* end = nullptr;
        long midi = strtol(q, &end, 10);
        if (end == q || *end != ':') break;
        q = end + 1;
        long start = strtol(q, &end, 10);
        if (end == q || *end != ':') break;
        q = end + 1;
        long len = strtol(q, &end, 10);
        if (end == q) break;
        out.notes[p][inst][out.noteN[p][inst]++] = {static_cast<uint8_t>(midi), static_cast<uint8_t>(start),
                                                    static_cast<uint8_t>(len)};
        q = (*end == ',') ? end + 1 : end;
      }
    } else if (n >= 3 && line[0] == 'P' && line[1] == 'C' && line[2] == '=') {
      const char* q = line + 3;
      while (*q != '\0' && out.patClipN < DrumMachine::kMaxClips) {
        char* end = nullptr;
        long pat = strtol(q, &end, 10);
        if (end == q || *end != ':') break;
        q = end + 1;
        long bar = strtol(q, &end, 10);
        if (end == q || *end != ':') break;
        q = end + 1;
        long len = strtol(q, &end, 10);
        if (end == q) break;
        if (pat >= 0 && pat < DrumMachine::kNumPatterns && bar >= 0 && len >= 1) {
          out.patClips[out.patClipN++] = {static_cast<uint8_t>(pat), static_cast<uint16_t>(bar),
                                          static_cast<uint8_t>(constrain(len, 1, 99))};
        }
        q = (*end == ',') ? end + 1 : end;
      }
    } else if (n >= 3 && line[0] == 'P' && line[1] >= '0' &&
               line[1] < '0' + DrumMachine::kNumPatterns && line[2] == '=') {
      int p = line[1] - '0';
      const char* q = line + 3;
      for (int r = 0; r < DrumMachine::kRows && *q != '\0'; ++r) {
        char* end = nullptr;
        out.rows[p][r] = static_cast<uint16_t>(strtoul(q, &end, 16));
        if (end == q) break;
        q = (*end == ',') ? end + 1 : end;
      }
    } else if (strncmp(line, "BEAT=", 5) == 0) {
      for (int t = 0; t < kNumTracks && line[5 + t] != '\0'; ++t) {
        if (line[5 + t] == '1') out.beatMask |= static_cast<uint8_t>(1 << t);
      }
    } else if (n >= 3 && line[0] == 'T' && line[1] >= '0' && line[1] < '0' + kNumTracks &&
               line[2] == '=') {
      int t = line[1] - '0';
      char* value = line + 3;
      if (*value == '\0') continue;
      char* comma = strchr(value, ',');
      if (comma) *comma = '\0';
      strncpy(out.files[t], value, AudioEngine::kMaxFilenameLen - 1);
      out.files[t][AudioEngine::kMaxFilenameLen - 1] = '\0';
      char* rest = comma ? comma + 1 : nullptr;
      while (rest != nullptr && *rest != '\0' && out.clipN[t] < AudioEngine::kMaxClips) {
        char* end = nullptr;
        uint32_t cs = strtoul(rest, &end, 10);
        if (end == rest || *end != ',') break;
        rest = end + 1;
        uint32_t ce = strtoul(rest, &end, 10);
        if (end == rest) break;
        out.clipS[t][out.clipN[t]] = cs;
        out.clipE[t][out.clipN[t]] = ce;
        ++out.clipN[t];
        rest = (*end == ',') ? end + 1 : end;
      }
    }
  }
  f.close();
  return true;
}

FLASHMEM void ProjectManager::unloadAll() {
  engine_->drums().reset();
  for (int i = 0; i < kNumTracks; ++i) {
    engine_->setTrackPan(i, 0);
    if (engine_->trackLoaded(i)) engine_->unloadTrack(i);
    savedFile_[i][0] = '\0';
    savedClipSig_[i] = 0;
  }
  loadedMask_ = 0x0F;
}

FLASHMEM bool ProjectManager::seedFromFiles(const char* name, const char* const files[kNumTracks]) {
  char clean[kProjectNameMax + 1];
  if (!sanitize(name, clean)) return false;
  char path[64];
  pathFor(clean, path, sizeof(path));
  if (SD.exists(path)) return false;
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  f.printf("NAME=%s\n", clean);
  for (int i = 0; i < kNumTracks; ++i) {
    if (files[i] != nullptr && SD.exists(files[i])) {
      f.printf("T%d=%s,0,0\n", i, files[i]);
    } else {
      f.printf("T%d=\n", i);
    }
  }
  f.close();
  listDirty_ = true;
  Serial.printf("PROJECT: seeded %s\n", clean);
  return true;
}

FLASHMEM bool ProjectManager::createNew(const char* rawName) {
  char clean[kProjectNameMax + 1];
  if (!sanitize(rawName, clean)) return false;
  char path[64];
  pathFor(clean, path, sizeof(path));
  if (SD.exists(path)) return false;
  if (open_) closeProject();
  unloadAll();
  strncpy(name_, clean, sizeof(name_));
  beatMask_ = 0;
  open_ = true;
  save();
  listDirty_ = true;
  dirty_ = false;
  Serial.printf("PROJECT: created %s\n", name_);
  return true;
}

FLASHMEM bool ProjectManager::openProject(const char* rawName) {
  char clean[kProjectNameMax + 1];
  if (!sanitize(rawName, clean)) return false;
  Loaded data;
  if (!loadFile(clean, data)) return false;

  if (open_) closeProject();
  unloadAll();

  for (int i = 0; i < kNumTracks; ++i) {
    engine_->setTrackPan(i, data.pan[i]);
    savedPan_[i] = data.pan[i];
  }
  bool loaded[kNumTracks] = {false, false, false, false};
  for (int i = 0; i < kNumTracks; ++i) {
    if (data.files[i][0] == '\0') continue;
    if (!SD.exists(data.files[i])) {
      Serial.printf("PROJECT: track %d file %s is missing\n", i + 1, data.files[i]);
      continue;
    }
    loaded[i] = engine_->loadTrack(i, data.files[i]);
    Serial.printf("PROJECT: track %d %s %s\n", i + 1, data.files[i], loaded[i] ? "loaded" : "FAILED");
  }

  // Each player parses its WAV header a few audio blocks after play() starts.
  for (int attempt = 0; attempt < 30; ++attempt) {
    bool ready = true;
    for (int i = 0; i < kNumTracks; ++i) {
      if (loaded[i] && engine_->trackLengthMillis(i) == 0) ready = false;
    }
    if (ready) break;
    delay(20);
  }

  engine_->pauseAll();
  engine_->seekAllToMillis(0);
  for (int i = 0; i < kNumTracks; ++i) {
    if (!loaded[i]) continue;
    int32_t offs[AudioEngine::kMaxClips];
    for (int k = 0; k < AudioEngine::kMaxClips; ++k) {
      offs[k] = (data.clipO[i][k] != kNoOffset) ? data.clipO[i][k] : data.offsetMs[i];
    }
    engine_->setTrackClips(i, data.clipS[i], data.clipE[i], offs, data.clipN[i]);
    Serial.printf("PROJECT: track %d has %d clip(s), first %lu-%lu ms\n", i + 1,
                  engine_->trackClipCount(i), static_cast<unsigned long>(engine_->clipStartMs(i, 0)),
                  static_cast<unsigned long>(engine_->clipEndMs(i, 0)));
    strncpy(savedFile_[i], data.files[i], sizeof(savedFile_[i]) - 1);
    savedClipSig_[i] = clipSignature(i);
  }

  strncpy(name_, clean, sizeof(name_));
  name_[kProjectNameMax] = '\0';
  beatMask_ = data.beatMask;
  savedBeatMask_ = beatMask_;
  {
    DrumMachine& drums = engine_->drums();
    drums.setBpm(data.bpm);
    int instCount = data.instCount;
    if (instCount == 0 && data.oldFormat) instCount = 1;  // an older file's single synth
    for (int i = 0; i < instCount; ++i) {
      drums.addInstrument(data.modularType[i] == 1);
      drums.setModular(i, data.modularType[i] == 1, data.modularPatch[i]);
      if(data.modularType[i]) Serial.printf("PROJECT: modular %d patch %08lx\n", i, (unsigned long)modular::signature(data.modularPatch[i]));
      if (data.hasSynth[i]) drums.setSynthAll(i, data.synth[i]);
    }
    Serial.printf("PROJECT: %d instrument(s)\n", drums.instrumentCount());
    for (int i = 0; i < drums.instrumentCount(); ++i) {
      Serial.printf("PROJECT: synth %d: wave %d,%d mix %d attack %d cutoff %d\n", i + 1, drums.synthParam(i, 0),
                    drums.synthParam(i, 1), drums.synthParam(i, 4), drums.synthParam(i, 5), drums.synthParam(i, 9));
    }
    for (int i = 0; i < DrumMachine::kPads; ++i) {
      drums.setPadRoute(i, data.padRoute[i]);
      drums.setPadNote(i, data.padNote[i]);
    }
    for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
      for (int r = 0; r < DrumMachine::kRows; ++r) drums.setRowBits(p, r, data.rows[p][r]);
    }
    for (int p = 0; p < DrumMachine::kNumPatterns; ++p) {
      for (int inst = 0; inst < drums.instrumentCount(); ++inst) {
        for (int i = 0; i < data.noteN[p][inst]; ++i) {
          const DrumMachine::Note& nt = data.notes[p][inst][i];
          drums.setNote(p, inst, nt.midi, nt.start, nt.len);
        }
      }
    }
    for (int i = 0; i < data.patClipN; ++i) {
      drums.addClip(data.patClips[i].pattern, data.patClips[i].startBar, data.patClips[i].lenBars);
    }
    savedDrumSig_ = drums.signature();
    Serial.printf("PROJECT: pads %d,%d,%d,%d notes %d,%d,%d,%d\n", drums.padRoute(0), drums.padRoute(1),
                  drums.padRoute(2), drums.padRoute(3), drums.padNote(0), drums.padNote(1),
                  drums.padNote(2), drums.padNote(3));
  }
  open_ = true;
  dirty_ = false;
  loadedMask_ = 0x0F;
  Serial.printf("PROJECT: opened %s\n", name_);
  return true;
}

FLASHMEM bool ProjectManager::assignTrack(int track, const char* filename) {
  if (!open_ || track < 0 || track >= kNumTracks || filename == nullptr) return false;
  if (!SD.exists(filename)) return false;
  engine_->pauseAll();
  bool ok = engine_->loadTrack(track, filename);
  // The player parses the WAV header a few audio blocks after play() starts.
  for (int attempt = 0; attempt < 30 && ok && engine_->trackLengthMillis(track) == 0; ++attempt) {
    delay(20);
  }
  engine_->pauseAll();
  engine_->seekAllToMillis(0);
  uint32_t len = engine_->trackLengthMillis(track);
  engine_->setTrackClips(track, nullptr, nullptr, nullptr, 0);  // one clip covering the whole file
  loadedMask_ |= static_cast<uint8_t>(1 << track);
  Serial.printf("PROJECT: track %d <- %s (%s, %lu ms)\n", track + 1, filename, ok ? "ok" : "FAILED",
                static_cast<unsigned long>(len));
  return ok;
}

FLASHMEM void ProjectManager::clearTrack(int track) {
  if (!open_ || track < 0 || track >= kNumTracks) return;
  engine_->unloadTrack(track);
  loadedMask_ |= static_cast<uint8_t>(1 << track);
  Serial.printf("PROJECT: track %d cleared\n", track + 1);
}

FLASHMEM void ProjectManager::closeProject() {
  if (!open_) return;
  engine_->pauseAll();  // never write to the SD card while tracks are streaming from it
  save();
  unloadAll();
  open_ = false;
  name_[0] = '\0';
  Serial.println("PROJECT: closed");
}

void ProjectManager::update() {
  if (!open_) return;
  bool differs = beatMask_ != savedBeatMask_ || engine_->drums().signature() != savedDrumSig_;
  for (int i = 0; i < kNumTracks && !differs; ++i) {
    const char* file = engine_->trackFilename(i);
    if (engine_->trackPan(i) != savedPan_[i]) differs = true;
    const char* saved = savedFile_[i];
    if ((file == nullptr) != (saved[0] == '\0')) {
      differs = true;
    } else if (file != nullptr) {
      if (strcmp(file, saved) != 0 || clipSignature(i) != savedClipSig_[i]) differs = true;
    }
  }
  uint32_t now = millis();
  if (!differs) {
    dirty_ = false;
    return;
  }
  if (!dirty_) {
    dirty_ = true;
    changedSinceMs_ = now;
  } else if (now - changedSinceMs_ >= kAutosaveDelayMs && !engine_->isPlaying()) {
    // Writing the project file while tracks stream from the same SD card locks the
    // Teensy up, so the save waits until playback is paused.
    save();
  }
}

uint8_t ProjectManager::takeLoadedMask() {
  uint8_t m = loadedMask_;
  loadedMask_ = 0;
  return m;
}

bool ProjectManager::takeListDirty() {
  bool v = listDirty_;
  listDirty_ = false;
  return v;
}

}  // namespace dubbox
