#include "teensy_link.h"
#include <Arduino.h>

namespace teensylink {
namespace {

State g_state;

uint16_t crc16(const uint8_t* data, int n) {  // CRC-16/CCITT-FALSE, same as the Teensy
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < n; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; ++b) crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}
uint32_t g_lastStateMs = 0;
bool g_everConnected = false;
uint8_t g_changedTracks = 0;
uint8_t g_changedFx = 0;
uint8_t g_changedWaves = 0;
uint8_t g_changedBeats = 0;
bool g_beatMaskChanged = false;
bool g_songChanged = false;
bool g_mixerChanged = false;
bool g_projectChanged = false;
bool g_listChanged = false;
bool g_filesChanged = false;
bool g_volumeChanged = false;
uint32_t g_maxGapMs = 0;
uint8_t g_changedPatterns = 0;
bool g_padRouteChanged = false;
uint32_t g_patClipsVersion = 0;
bool g_rhythmChanged = false;
uint32_t g_patHoldUntil[kNumPatterns] = {0};
uint32_t g_clipsHoldUntil = 0;
uint32_t g_bpmHoldUntil = 0;
uint32_t g_previewHoldUntil = 0;
uint32_t g_padModeHoldUntil = 0;
uint32_t g_synthHoldUntil = 0;
bool g_synthChanged = false;
uint32_t g_modHoldUntil[kMaxInstruments] = {};
uint32_t g_instHoldUntil = 0;
bool g_instChanged = false;
uint32_t g_clickHoldUntil = 0;
constexpr uint32_t kEditHoldMs = 700;
uint32_t g_stateLines = 0;
char g_line[300];
size_t g_lineLen = 0;

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

void handleWave(const char* line) {
  int track = 0, total = 0, offset = 0, count = 0, consumed = 0;
  if (sscanf(line, "W,%d,%d,%d,%d,%n", &track, &total, &offset, &count, &consumed) < 4) return;
  if (track < 0 || track >= kNumTracks || total <= 0 || total > kMaxWaveCols) return;
  if (offset < 0 || count < 0 || offset + count > total || consumed <= 0) return;
  Wave& w = g_state.wave[track];
  if (offset == 0) {
    w.cols = total;
    w.received = 0;
    w.runSum = 0;  // an existing complete waveform stays usable while a resend arrives
  }
  if (offset != w.received) return;  // out-of-order chunk; wait for a resend
  const char* hex = line + consumed;
  if ((int)strlen(hex) < count * 2) return;
  for (int i = 0; i < count; ++i) {
    uint8_t v = (uint8_t)((hexNibble(hex[i * 2]) << 4) | hexNibble(hex[i * 2 + 1]));
    w.peaks[offset + i] = v;
    w.runSum = w.runSum * 31 + v + 1;
  }
  w.received = offset + count;
  if (w.received >= w.cols) {
    bool changed = !w.complete || w.sum != w.runSum;
    w.sum = w.runSum;
    w.complete = true;
    if (changed) g_changedWaves |= (1 << track);
  }
}

void handleProject(const char* line) {
  int open = 0;
  int consumed = 0;
  if (sscanf(line, "J,%d,%n", &open, &consumed) < 1 || consumed <= 0) return;
  const char* name = line + consumed;
  ProjectInfo& p = g_state.project;
  bool changed = p.open != (open != 0) || strncmp(p.name, name, kNameLen - 1) != 0;
  if (!changed) return;
  p.open = open != 0;
  strncpy(p.name, name, kNameLen - 1);
  p.name[kNameLen - 1] = 0;
  // Different project (or closed): everything track-related is stale.
  for (int i = 0; i < kNumTracks; ++i) {
    g_state.wave[i].complete = false;
    g_state.wave[i].cols = 0;
    g_state.wave[i].received = 0;
    g_state.track[i].known = false;
    g_state.beats[i] = Beats();
  }
  g_changedBeats |= 0x0F;
  g_changedTracks |= 0x0F;
  g_changedWaves |= 0x0F;
  g_projectChanged = true;
}

void handleList(const char* line) {
  int idx = 0, count = 0, consumed = 0;
  if (sscanf(line, "L,%d,%d,%n", &idx, &count, &consumed) < 2 || consumed <= 0) return;
  ProjectInfo& p = g_state.project;
  if (count <= 0) {
    p.listCount = 0;
    p.listKnown = true;
    g_listChanged = true;
    return;
  }
  if (count > kMaxProjects) count = kMaxProjects;
  if (idx < 0 || idx >= count) return;
  strncpy(p.list[idx], line + consumed, kNameLen - 1);
  p.list[idx][kNameLen - 1] = 0;
  if (idx == count - 1) {
    p.listCount = count;
    p.listKnown = true;
    g_listChanged = true;
  }
}

void handleFiles(const char* line) {
  int idx = 0, count = 0, consumed = 0;
  if (sscanf(line, "E,%d,%d,%n", &idx, &count, &consumed) < 2 || consumed <= 0) return;
  FileList& f = g_state.files;
  if (count <= 0) {
    f.count = count < 0 ? -1 : 0;
    f.known = true;
    g_filesChanged = true;
    return;
  }
  if (count > kMaxFiles) count = kMaxFiles;
  if (idx < 0 || idx >= count) return;
  strncpy(f.names[idx], line + consumed, kFileNameLen - 1);
  f.names[idx][kFileNameLen - 1] = 0;
  if (idx == count - 1) {
    f.count = count;
    f.known = true;
    g_filesChanged = true;
  }
}

void handlePattern(const char* line) {
  int pat = 0;
  unsigned r[kDrumRows];
  if (sscanf(line, "p,%d,%x,%x,%x,%x,%x,%x,%x,%x", &pat, &r[0], &r[1], &r[2], &r[3], &r[4], &r[5],
             &r[6], &r[7]) != 9 ||
      pat < 0 || pat >= kNumPatterns) {
    return;
  }
  if (millis() < g_patHoldUntil[pat]) return;
  bool changed = false;
  for (int i = 0; i < kDrumRows; ++i) {
    uint16_t v = static_cast<uint16_t>(r[i]);
    if (g_state.drums.rows[pat][i] != v) {
      g_state.drums.rows[pat][i] = v;
      changed = true;
    }
  }
  if (changed) g_changedPatterns |= (1 << pat);
}

void handleNotes(const char* line) {
  int pat = 0, inst = 0, n = 0, consumed = 0;
  if (sscanf(line, "q,%d,%d,%d%n", &pat, &inst, &n, &consumed) != 3 || pat < 0 || pat >= kNumPatterns ||
      inst < 0 || inst >= kMaxInstruments || n < 0 || n > kMaxNotes) {
    return;
  }
  if (millis() < g_patHoldUntil[pat]) return;
  PatNote notes[kMaxNotes];
  const char* p = line + consumed;
  for (int i = 0; i < n; ++i) {
    int midi = 0, start = 0, len = 0, used = 0;
    if (sscanf(p, ",%d:%d:%d%n", &midi, &start, &len, &used) != 3) return;
    notes[i].midi = static_cast<uint8_t>(midi);
    notes[i].start = static_cast<uint8_t>(start);
    notes[i].len = static_cast<uint8_t>(len);
    p += used;
  }
  Drums& d = g_state.drums;
  bool changed = d.noteCount[pat][inst] != n;
  for (int i = 0; i < n && !changed; ++i) {
    changed = d.notes[pat][inst][i].midi != notes[i].midi || d.notes[pat][inst][i].start != notes[i].start ||
              d.notes[pat][inst][i].len != notes[i].len;
  }
  if (!changed) return;
  d.noteCount[pat][inst] = n;
  for (int i = 0; i < n; ++i) d.notes[pat][inst][i] = notes[i];
  g_changedPatterns |= (1 << pat);
}

void handlePads(const char* line) {
  int r[4], mode = -1, n[4] = {0, 0, 0, 0};
  int got = sscanf(line, "u,%d,%d,%d,%d,%d,%d,%d,%d,%d", &r[0], &r[1], &r[2], &r[3], &mode, &n[0], &n[1],
                   &n[2], &n[3]);
  if (got < 4) return;
  Drums& d = g_state.drums;
  bool changed = false;
  for (int i = 0; i < 4; ++i) {
    if (d.padRoute[i] != r[i]) {
      d.padRoute[i] = r[i];
      changed = true;
    }
  }
  if (got == 9) {
    for (int i = 0; i < 4; ++i) {
      if (d.padNote[i] != n[i]) {
        d.padNote[i] = n[i];
        changed = true;
      }
    }
    if (millis() >= g_padModeHoldUntil && d.padMode != mode) {
      d.padMode = mode;
      changed = true;
    }
  }
  if (changed) g_padRouteChanged = true;
}

void handleSynth(const char* line) {
  if (millis() < g_synthHoldUntil) return;  // our own edit is still on its way
  int inst = 0, consumed = 0;
  if (sscanf(line, "g,%d%n", &inst, &consumed) != 1 || inst < 0 || inst >= kMaxInstruments) return;
  int v[kSynthParams];
  const char* q = line + consumed;
  for (int i = 0; i < kSynthParams; ++i) {
    if (*q != ',') return;
    char* end = nullptr;
    long x = strtol(q + 1, &end, 10);
    if (end == q + 1) return;
    v[i] = static_cast<int>(x);
    q = end;
  }
  bool changed = false;
  for (int i = 0; i < kSynthParams; ++i) {
    if (g_state.synth[inst].v[i] != v[i]) {
      g_state.synth[inst].v[i] = v[i];
      changed = true;
    }
  }
  if (changed) g_synthChanged = true;
}

// The number of instruments, as reported by the Teensy.
void handleInstCount(const char* line) {
  int n = 0;
  if (sscanf(line, "w,%d", &n) != 1 || n < 0 || n > kMaxInstruments) return;
  if (millis() < g_instHoldUntil) return;
  if (g_state.drums.instCount != n) {
    g_state.drums.instCount = n;
    g_instChanged = true;
  }
}

void handlePatClips(const char* line) {
  int n = 0, consumed = 0;
  if (sscanf(line, "y,%d%n", &n, &consumed) != 1 || n < 0 || n > kMaxPatClips) return;
  if (millis() < g_clipsHoldUntil) return;
  PatClip clips[kMaxPatClips];
  const char* p = line + consumed;
  for (int i = 0; i < n; ++i) {
    int pat = 0, bar = 0, len = 0, used = 0;
    if (sscanf(p, ",%d,%d,%d%n", &pat, &bar, &len, &used) != 3) return;
    clips[i].pattern = static_cast<uint8_t>(pat);
    clips[i].startBar = static_cast<uint16_t>(bar);
    clips[i].lenBars = static_cast<uint8_t>(len);
    p += used;
  }
  Drums& d = g_state.drums;
  bool changed = d.clipCount != n;
  for (int i = 0; i < n && !changed; ++i) {
    changed = d.clips[i].pattern != clips[i].pattern || d.clips[i].startBar != clips[i].startBar ||
              d.clips[i].lenBars != clips[i].lenBars;
  }
  if (!changed) return;
  d.clipCount = n;
  for (int i = 0; i < n; ++i) d.clips[i] = clips[i];
  ++g_patClipsVersion;
}

void handleRhythm(const char* line) {
  int bpm = 0, on = 0, pat = 0, step = 0, rec = 0, click = 1;
  if (sscanf(line, "r,%d,%d,%d,%d,%d,%d", &bpm, &on, &pat, &step, &rec, &click) < 4) return;
  Drums& d = g_state.drums;
  uint32_t now = millis();
  bool changed = false;
  if (now >= g_bpmHoldUntil && d.bpm != bpm) {
    d.bpm = bpm;
    changed = true;
  }
  const bool recording = rec == 1;
  const bool countIn = rec == 2;
  if (now >= g_previewHoldUntil && (d.previewOn != (on != 0) || d.previewPattern != pat ||
                                    d.recording != recording || d.countIn != countIn)) {
    d.previewOn = on != 0;
    d.previewPattern = pat;
    d.recording = recording;
    d.countIn = countIn;
    changed = true;
  }
  if (now >= g_clickHoldUntil && d.clickOn != (click != 0)) {
    d.clickOn = click != 0;
    changed = true;
  }
  if (d.previewOn && d.previewStep != step) {
    d.previewStep = step;
    changed = true;
  }
  if (changed) g_rhythmChanged = true;
}

UploadReply g_upReply;

void handleUploadReply(const char* line) {
  if (strncmp(line, "Q,ok,", 5) == 0) {
    strncpy(g_upReply.name, line + 5, sizeof(g_upReply.name) - 1);
    g_upReply.ok = true;
  } else if (strncmp(line, "Q,ack,", 6) == 0) {
    g_upReply.acked = static_cast<uint32_t>(atol(line + 6));
  } else if (strncmp(line, "Q,done,", 7) == 0) {
    strncpy(g_upReply.name, line + 7, sizeof(g_upReply.name) - 1);
    g_upReply.done = true;
  } else if (strncmp(line, "Q,err,", 6) == 0) {
    strncpy(g_upReply.err, line + 6, sizeof(g_upReply.err) - 1);
  }
}

void handleLine(const char* line) {
  switch (line[0]) {
    case 'Q':
      handleUploadReply(line);
      break;
    case 'p':
      handlePattern(line);
      break;
    case 'y':
      handlePatClips(line);
      break;
    case 'q':
      handleNotes(line);
      break;
    case 'u':
      handlePads(line);
      break;
    case '@': {
      int inst=-1, consumed=0, type=0; modular::Patch patch;
      if (sscanf(line,"@,%d,%n",&inst,&consumed)==1 && consumed>0 && inst>=0 && inst<kMaxInstruments &&
          millis()>=g_modHoldUntil[inst] && millis()>=g_instHoldUntil && modular::parse(line+consumed,type,patch)) {
        if(g_state.instrumentModular[inst] != bool(type) || !modular::equal(g_state.modularPatch[inst],patch))
          Serial.printf("modular: slot %d type %d patch %08lx\n", inst, type, (unsigned long)modular::signature(patch));
        if(g_state.instrumentModular[inst] != bool(type)) g_instChanged=true;
        if(!modular::equal(g_state.modularPatch[inst],patch)) g_synthChanged=true;
        g_state.instrumentModular[inst]=type; g_state.modularPatch[inst]=patch;
      }
      break;
    }
    case 'g':
      handleSynth(line);
      break;
    case 'w':
      handleInstCount(line);
      break;
    case 'r':
      handleRhythm(line);
      break;
    case 'E':
      handleFiles(line);
      break;
    case 'B': {
      int track = 0, bpm100 = 0, bar10 = 0;
      unsigned long first = 0;
      if (sscanf(line, "B,%d,%d,%lu,%d", &track, &bpm100, &first, &bar10) == 4 && track >= 0 &&
          track < kNumTracks) {
        Beats& b = g_state.beats[track];
        bool valid = bpm100 > 0 && bar10 > 0;
        if (b.valid != valid || b.bpm100 != bpm100 || b.firstMs != first || b.barMs10 != bar10) {
          b.valid = valid;
          b.bpm100 = bpm100;
          b.firstMs = first;
          b.barMs10 = bar10;
          g_changedBeats |= (1 << track);
        }
      }
      break;
    }
    case 'U': {
      int vol = 0;
      if (sscanf(line, "U,%d", &vol) == 1 && vol != g_state.volumePermille) {
        g_state.volumePermille = vol;
        g_volumeChanged = true;
      }
      break;
    }
    case 'J':
      handleProject(line);
      break;
    case 'L':
      handleList(line);
      break;
    case 'S': {
      int playing = 0;
      unsigned long pos = 0, song = 0;
      if (sscanf(line, "S,%d,%lu,%lu", &playing, &pos, &song) == 3) {
        g_state.playing = playing != 0;
        g_state.posMs = pos;
        if (song != g_state.songMs) {
          g_state.songMs = song;
          g_songChanged = true;
        }
        uint32_t nowMs = millis();
        if (g_everConnected) {
          uint32_t gap = nowMs - g_lastStateMs;
          if (gap > g_maxGapMs) g_maxGapMs = gap;
        }
        ++g_stateLines;
        g_lastStateMs = nowMs;
        g_everConnected = true;
      }
      break;
    }
    case 'C': {
      // C,<track>,<fileLenMs>,<n>,<start0>,<end0>,<offset0>,...
      int track = 0, n = 0, consumed = 0;
      unsigned long len = 0;
      if (sscanf(line, "C,%d,%lu,%d%n", &track, &len, &n, &consumed) == 3 && track >= 0 &&
          track < kNumTracks && n >= 1) {
        if (n > kMaxClips) n = kMaxClips;
        uint32_t cs[kMaxClips] = {0}, ce[kMaxClips] = {0};
        int32_t co[kMaxClips] = {0};
        const char* p = line + consumed;
        bool ok = true;
        for (int k = 0; k < n && ok; ++k) {
          char* end = nullptr;
          if (*p != ',') {
            ok = false;
            break;
          }
          cs[k] = strtoul(p + 1, &end, 10);
          if (end == p + 1 || *end != ',') {
            ok = false;
            break;
          }
          p = end;
          ce[k] = strtoul(p + 1, &end, 10);
          if (end == p + 1) {
            ok = false;
            break;
          }
          p = end;
          if (*p != ',') {
            ok = false;
            break;
          }
          co[k] = static_cast<int32_t>(strtol(p + 1, &end, 10));
          if (end == p + 1) {
            ok = false;
            break;
          }
          p = end;
        }
        if (ok) {
          TrackInfo& t = g_state.track[track];
          if (t.known && t.fileLenMs != len) g_state.wave[track].complete = false;
          bool changed = !t.known || t.fileLenMs != len || t.clipCount != n;
          for (int k = 0; k < n && !changed; ++k) {
            if (t.clipStart[k] != cs[k] || t.clipEnd[k] != ce[k] || t.clipOff[k] != co[k]) changed = true;
          }
          if (changed) {
            t.fileLenMs = len;
            t.clipCount = n;
            for (int k = 0; k < n; ++k) {
              t.clipStart[k] = cs[k];
              t.clipEnd[k] = ce[k];
              t.clipOff[k] = co[k];
            }
            t.known = true;
            g_changedTracks |= (1 << track);
          }
        }
      }
      break;
    }
    case 'I': {
      int mask = 0;
      if (sscanf(line, "I,%d", &mask) == 1 && mask != g_state.beatMask) {
        g_state.beatMask = mask;
        g_beatMaskChanged = true;
      }
      break;
    }
    case '~': {
      int track, pan;
      if (sscanf(line, "~,%d,%d", &track, &pan) == 2 && track >= 0 && track < kNumTracks) {
        g_state.panPermille[track] = constrain(pan, -1000, 1000);
        g_mixerChanged = true;
      }
      break;
    }
    case 'M': {
      int f[4], p[4];
      if (sscanf(line, "M,%d,%d,%d,%d,%d,%d,%d,%d", &f[0], &f[1], &f[2], &f[3], &p[0], &p[1],
                 &p[2], &p[3]) == 8) {
        for (int i = 0; i < kNumTracks; ++i) {
          if (g_state.faderPermille[i] != f[i] || g_state.peakPermille[i] != p[i]) {
            g_mixerChanged = true;
          }
          g_state.faderPermille[i] = f[i];
          g_state.peakPermille[i] = p[i];
        }
      }
      break;
    }
    case 'F': {
      int track = 0, byp = 0, wet = 0, active = 0, canF = 0, canC = 0, t0 = -1, t1 = -1, t2 = -1;
      if (sscanf(line, "F,%d,%d,%d,%d,%d,%d,%d,%d,%d", &track, &byp, &wet, &active, &canF, &canC,
                 &t0, &t1, &t2) == 9 &&
          track >= 0 && track < kNumTracks) {
        FxInfo& fx = g_state.fx[track];
        bool changed = !fx.known || fx.bypassed != (byp != 0) || fx.wetPermille != wet ||
                       fx.activeSlot != active || fx.canAddFlange != (canF != 0) ||
                       fx.canAddChorus != (canC != 0) || fx.types[0] != t0 || fx.types[1] != t1 ||
                       fx.types[2] != t2;
        if (changed) {
          fx.known = true;
          fx.bypassed = byp != 0;
          fx.wetPermille = wet;
          fx.activeSlot = active;
          fx.canAddFlange = canF != 0;
          fx.canAddChorus = canC != 0;
          fx.types[0] = t0;
          fx.types[1] = t1;
          fx.types[2] = t2;
          g_changedFx |= (1 << track);
        }
      }
      break;
    }
    case 'W':
      handleWave(line);
      break;
    default:
      break;
  }
}

}  // namespace

void begin() {
  Serial2.setRxBufferSize(8192);
  Serial2.setTxBufferSize(4096);  // a whole upload chunk goes out without blocking the UI
  Serial2.begin(2000000, SERIAL_8N1, kRxPin, kTxPin);
}

void poll() {
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\n' || c == '\r') {
      if (g_lineLen > 0) {
        g_line[g_lineLen] = '\0';
        handleLine(g_line);
        g_lineLen = 0;
      }
    } else if (g_lineLen < sizeof(g_line) - 1) {
      g_line[g_lineLen++] = c;
    } else {
      g_lineLen = 0;  // overlong line: drop it
    }
  }
}

bool connected() { return g_everConnected && millis() - g_lastStateMs < 1500; }

void takeLinkStats(uint32_t& stateLines, uint32_t& maxGapMs) {
  stateLines = g_stateLines;
  maxGapMs = g_maxGapMs;
  g_stateLines = 0;
  g_maxGapMs = 0;
}
const State& state() { return g_state; }

static uint8_t takeMask(uint8_t& m) {
  uint8_t v = m;
  m = 0;
  return v;
}
uint8_t takeChangedTracks() { return takeMask(g_changedTracks); }
uint8_t takeChangedFx() { return takeMask(g_changedFx); }
uint8_t takeChangedWaves() { return takeMask(g_changedWaves); }
uint8_t takeChangedBeats() { return takeMask(g_changedBeats); }

bool takeSongChanged() {
  bool v = g_songChanged;
  g_songChanged = false;
  return v;
}
bool takeProjectChanged() {
  bool v = g_projectChanged;
  g_projectChanged = false;
  return v;
}
bool takeListChanged() {
  bool v = g_listChanged;
  g_listChanged = false;
  return v;
}
bool takeFilesChanged() {
  bool v = g_filesChanged;
  g_filesChanged = false;
  return v;
}
bool takeBeatMaskChanged() {
  bool v = g_beatMaskChanged;
  g_beatMaskChanged = false;
  return v;
}
uint8_t takePatternsChanged() { return takeMask(g_changedPatterns); }
bool takeInstrumentsChanged() {
  bool v = g_instChanged;
  g_instChanged = false;
  return v;
}
bool takeSynthChanged() {
  bool v = g_synthChanged;
  g_synthChanged = false;
  return v;
}
bool takePadRouteChanged() {
  bool v = g_padRouteChanged;
  g_padRouteChanged = false;
  return v;
}
uint32_t patClipsVersion() { return g_patClipsVersion; }
bool takeRhythmChanged() {
  bool v = g_rhythmChanged;
  g_rhythmChanged = false;
  return v;
}
bool takeVolumeChanged() {
  bool v = g_volumeChanged;
  g_volumeChanged = false;
  return v;
}
bool takeMixerChanged() {
  bool v = g_mixerChanged;
  g_mixerChanged = false;
  return v;
}

void sendHello() { Serial2.print("H\n"); }
void sendGetList() { Serial2.print("G\n"); }
void sendNewProject(const char* name) { Serial2.printf("N,%s\n", name); }
void sendOpenProject(const char* name) { Serial2.printf("O,%s\n", name); }
void sendCloseProject() { Serial2.print("Q\n"); }
void sendListFiles() { Serial2.print("D\n"); }

void uploadStart(uint32_t size, const char* name) {
  g_upReply = UploadReply();
  Serial2.printf("S,%lu,%s\n", static_cast<unsigned long>(size), name);
}

bool uploadSendChunk(uint32_t seq, const uint8_t* data, int len) {
  if (Serial2.availableForWrite() < len + 32) return false;
  Serial2.printf("W,%lu,%d,%04x\n", static_cast<unsigned long>(seq), len, crc16(data, len));
  Serial2.write(data, len);
  return true;
}

void uploadCancel() { Serial2.print("C\n"); }
const UploadReply& uploadReply() { return g_upReply; }
void sendAssignTrack(int track, const char* filename) { Serial2.printf("T,%d,%s\n", track, filename); }
void sendClearTrack(int track) { Serial2.printf("R,%d\n", track); }
void sendTogglePlay() { Serial2.print("P\n"); }
void sendRewind() { Serial2.print("Z\n"); }
void sendVolume(int permille) { Serial2.printf("U,%d\n", permille); }
void sendSeek(uint32_t positionMs) { Serial2.printf("B,%lu\n", (unsigned long)positionMs); }
void sendCrop(int track, int clip, uint32_t startMs, uint32_t endMs) {
  Serial2.printf("K,%d,%d,%lu,%lu\n", track, clip, (unsigned long)startMs, (unsigned long)endMs);
}
void sendSplit(int track, uint32_t positionMs) {
  Serial2.printf("E,%d,%lu\n", track, (unsigned long)positionMs);
}
void sendMerge(int track, int clip) { Serial2.printf("M,%d,%d\n", track, clip); }
void sendClipOffset(int track, int clip, int32_t offsetMs) {
  Serial2.printf("L,%d,%d,%ld\n", track, clip, (long)offsetMs);
}
void sendBeatFlag(int track, bool on) { Serial2.printf("I,%d,%d\n", track, on ? 1 : 0); }
void sendAddFx(int track, int fxType) { Serial2.printf("A,%d,%d\n", track, fxType); }
void sendActiveSlot(int track, int slot) { Serial2.printf("X,%d,%d\n", track, slot); }
void sendPan(int track, int pan) { Serial2.printf("~,%d,%d\n", track, constrain(pan, -1000, 1000)); }
void sendWet(int track, int wetPermille) { Serial2.printf("V,%d,%d\n", track, wetPermille); }
void sendBypass(int track, bool bypassed) { Serial2.printf("Y,%d,%d\n", track, bypassed ? 1 : 0); }

void editStep(int pattern, int row, int step, bool on) {
  if (pattern < 0 || pattern >= kNumPatterns || row < 0 || row >= kDrumRows || step < 0 ||
      step >= kPatSteps) {
    return;
  }
  uint16_t& bits = g_state.drums.rows[pattern][row];
  if (on) {
    bits |= static_cast<uint16_t>(1u << step);
  } else {
    bits &= static_cast<uint16_t>(~(1u << step));
  }
  g_patHoldUntil[pattern] = millis() + kEditHoldMs;
  Serial2.printf("s,%d,%d,%d,%d\n", pattern, row, step, on ? 1 : 0);
}

void clearPattern(int pattern) {
  if (pattern < 0 || pattern >= kNumPatterns) return;
  for (int r = 0; r < kDrumRows; ++r) g_state.drums.rows[pattern][r] = 0;
  g_patHoldUntil[pattern] = millis() + kEditHoldMs;
  Serial2.printf("w,%d\n", pattern);
}

void setBpm(int bpm) {
  bpm = bpm < 40 ? 40 : (bpm > 240 ? 240 : bpm);
  g_state.drums.bpm = bpm;
  g_bpmHoldUntil = millis() + kEditHoldMs;
  g_rhythmChanged = true;
  Serial2.printf("j,%d\n", bpm);
}

void setPreview(bool on, int pattern, bool record) {
  Drums& d = g_state.drums;
  const bool wasLooping = d.previewOn && d.previewPattern == pattern;
  if (wasLooping && d.countIn && !record) on = false;  // cancelling the count-in stops the loop
  d.previewOn = on;
  // Starting a loop with `record` begins with a one-bar count-in; for a loop already running,
  // recording starts at once.
  d.countIn = on && record && !wasLooping;
  d.recording = on && record && wasLooping;
  d.previewPattern = pattern;
  d.previewStep = d.countIn ? -1 : 0;
  g_previewHoldUntil = millis() + kEditHoldMs;
  g_rhythmChanged = true;
  Serial2.printf("f,%d,%d,%d\n", on ? 1 : 0, pattern, record ? 1 : 0);
}

void addPatClip(int pattern, int bar, int lenBars, int index) {
  Drums& d = g_state.drums;
  if (d.clipCount >= kMaxPatClips) return;
  if (index < 0 || index > d.clipCount) index = d.clipCount;
  for (int i = d.clipCount; i > index; --i) d.clips[i] = d.clips[i - 1];
  d.clips[index].pattern = static_cast<uint8_t>(pattern);
  d.clips[index].startBar = static_cast<uint16_t>(bar);
  d.clips[index].lenBars = static_cast<uint8_t>(lenBars);
  ++d.clipCount;
  g_clipsHoldUntil = millis() + kEditHoldMs;
  ++g_patClipsVersion;
  Serial2.printf("a,%d,%d,%d,%d\n", pattern, bar, lenBars, index);
}

void removePatClip(int index) {
  Drums& d = g_state.drums;
  if (index < 0 || index >= d.clipCount) return;
  for (int i = index; i + 1 < d.clipCount; ++i) d.clips[i] = d.clips[i + 1];
  --d.clipCount;
  g_clipsHoldUntil = millis() + kEditHoldMs;
  ++g_patClipsVersion;
  Serial2.printf("d,%d\n", index);
}

void movePatClip(int index, int bar) {
  Drums& d = g_state.drums;
  if (index < 0 || index >= d.clipCount) return;
  if (bar < 0) bar = 0;
  d.clips[index].startBar = static_cast<uint16_t>(bar);
  g_clipsHoldUntil = millis() + kEditHoldMs;
  ++g_patClipsVersion;
  Serial2.printf("m,%d,%d\n", index, bar);
}

void resizePatClip(int index, int lenBars) {
  Drums& d = g_state.drums;
  if (index < 0 || index >= d.clipCount) return;
  if (lenBars < 1) lenBars = 1;
  d.clips[index].lenBars = static_cast<uint8_t>(lenBars);
  g_clipsHoldUntil = millis() + kEditHoldMs;
  ++g_patClipsVersion;
  Serial2.printf("l,%d,%d\n", index, lenBars);
}

void setModularPatch(int inst, const modular::Patch& patch) {
  if(inst<0 || inst>=g_state.drums.instCount || !g_state.instrumentModular[inst] || !modular::valid(patch)) return;
  g_state.modularPatch[inst]=patch;
  g_modHoldUntil[inst]=millis()+kEditHoldMs;
  char text[128]; modular::format(text,sizeof(text),1,patch);
  Serial2.printf("@,%d,%s\n",inst,text);
}

void addInstrument(bool modular) {
  Drums& d = g_state.drums;
  if (d.instCount >= kMaxInstruments) return;
  const int i = d.instCount++;
  g_state.instrumentModular[i] = modular;
  g_state.modularPatch[i] = modular::Patch();
  g_state.synth[i] = SynthState();
  for (int p = 0; p < kNumPatterns; ++p) d.noteCount[p][i] = 0;
  g_instHoldUntil = millis() + kEditHoldMs;
  Serial2.printf("i,%d,1,%d\n", i, modular ? 1 : 0);
}

void removeInstrument(int inst) {
  Drums& d = g_state.drums;
  if (inst < 0 || inst >= d.instCount) return;
  for (int i = inst; i + 1 < d.instCount; ++i) {
    g_state.instrumentModular[i] = g_state.instrumentModular[i+1];
    g_state.modularPatch[i] = g_state.modularPatch[i+1];
    g_state.synth[i] = g_state.synth[i + 1];
    for (int p = 0; p < kNumPatterns; ++p) {
      d.noteCount[p][i] = d.noteCount[p][i + 1];
      for (int k = 0; k < d.noteCount[p][i]; ++k) d.notes[p][i][k] = d.notes[p][i + 1][k];
    }
  }
  const int last = d.instCount - 1;
  g_state.instrumentModular[last] = false;
  g_state.modularPatch[last] = modular::Patch();
  g_state.synth[last] = SynthState();
  for (int p = 0; p < kNumPatterns; ++p) d.noteCount[p][last] = 0;
  --d.instCount;
  const uint32_t until = millis() + kEditHoldMs;  // ignore the Teensy's reports until it has caught up
  g_instHoldUntil = until;
  g_synthHoldUntil = until;
  for (int p = 0; p < kNumPatterns; ++p) g_patHoldUntil[p] = until;
  Serial2.printf("i,%d,0\n", inst);
}

void setNote(int pattern, int inst, int midi, int start, int len) {
  Drums& d = g_state.drums;
  if (pattern < 0 || pattern >= kNumPatterns || inst < 0 || inst >= d.instCount || start < 0 ||
      start >= kPatSteps || len < 1) {
    return;
  }
  if (start + len > kPatSteps) len = kPatSteps - start;
  int existing = -1;
  for (int i = 0; i < d.noteCount[pattern][inst]; ++i) {
    const PatNote& o = d.notes[pattern][inst][i];
    if (o.midi != midi) continue;
    if (o.start == start) {
      existing = i;
    } else if (start > o.start && start < o.start + o.len) {
      return;
    } else if (o.start > start && start + len > o.start) {
      len = o.start - start;
    }
  }
  if (existing >= 0) {
    d.notes[pattern][inst][existing].len = static_cast<uint8_t>(len);
  } else {
    if (d.noteCount[pattern][inst] >= kMaxNotes) return;
    PatNote& n = d.notes[pattern][inst][d.noteCount[pattern][inst]++];
    n.midi = static_cast<uint8_t>(midi);
    n.start = static_cast<uint8_t>(start);
    n.len = static_cast<uint8_t>(len);
  }
  g_patHoldUntil[pattern] = millis() + kEditHoldMs;
  Serial2.printf("n,%d,%d,%d,%d,%d\n", pattern, inst, midi, start, len);
}

void removeNote(int pattern, int inst, int midi, int start) {
  Drums& d = g_state.drums;
  if (pattern < 0 || pattern >= kNumPatterns || inst < 0 || inst >= kMaxInstruments) return;
  for (int i = 0; i < d.noteCount[pattern][inst]; ++i) {
    if (d.notes[pattern][inst][i].midi == midi && d.notes[pattern][inst][i].start == start) {
      for (int j = i; j + 1 < d.noteCount[pattern][inst]; ++j) d.notes[pattern][inst][j] = d.notes[pattern][inst][j + 1];
      --d.noteCount[pattern][inst];
      break;
    }
  }
  g_patHoldUntil[pattern] = millis() + kEditHoldMs;
  Serial2.printf("x,%d,%d,%d,%d\n", pattern, inst, midi, start);
}

void clearNotes(int pattern, int inst) {
  if (pattern < 0 || pattern >= kNumPatterns || inst < 0 || inst >= kMaxInstruments) return;
  g_state.drums.noteCount[pattern][inst] = 0;
  g_patHoldUntil[pattern] = millis() + kEditHoldMs;
  Serial2.printf("c,%d,%d\n", pattern, inst);
}

void auditionNote(int inst, int midi) { Serial2.printf("t,%d,%d\n", inst, midi); }

void sendRouteHold(int row) { Serial2.printf("h,%d\n", row); }

void sendNoteHold(int midi) { Serial2.printf("e,%d\n", midi); }

void sendLiveNote(int inst, int midi, bool on) { Serial2.printf("o,%d,%d,%d\n", inst, midi, on ? 1 : 0); }

void sendNoteRefresh(int inst, int midi) { Serial2.printf("o,%d,%d,2\n", inst, midi); }

void sendSustain(bool down) { Serial2.printf("z,%d\n", down ? 1 : 0); }

static void sendSynthPatch(int inst) {
  char line[120];
  int n = snprintf(line, sizeof(line), "g,%d", inst);
  for (int i = 0; i < kSynthParams; ++i) n += snprintf(line + n, sizeof(line) - n, ",%d", g_state.synth[inst].v[i]);
  Serial2.print(line);
  Serial2.print('\n');
}

void setSynthParam(int inst, int index, int value) {
  if (inst < 0 || inst >= kMaxInstruments || index < 0 || index >= kSynthParams) return;
  g_state.synth[inst].v[index] = value;
  g_synthHoldUntil = millis() + kEditHoldMs;
  sendSynthPatch(inst);
}

void setSynthParamLocal(int inst, int index, int value) {
  if (inst < 0 || inst >= kMaxInstruments || index < 0 || index >= kSynthParams) return;
  g_state.synth[inst].v[index] = value;
  g_synthHoldUntil = millis() + kEditHoldMs;
}

void setSynthAll(int inst, const int* values) {
  if (inst < 0 || inst >= kMaxInstruments) return;
  for (int i = 0; i < kSynthParams; ++i) g_state.synth[inst].v[i] = values[i];
  g_synthHoldUntil = millis() + kEditHoldMs;
  sendSynthPatch(inst);
}

void setClickOn(bool on) {
  g_state.drums.clickOn = on;
  g_clickHoldUntil = millis() + kEditHoldMs;
  g_rhythmChanged = true;
  Serial2.printf("b,%d\n", on ? 1 : 0);
}

void setPadMode(int mode, int inst) {
  g_state.drums.padMode = mode != 0 ? 1 : 0;
  g_padModeHoldUntil = millis() + kEditHoldMs;
  g_padRouteChanged = true;
  if (inst >= 0) {
    Serial2.printf("v,%d,%d\n", mode != 0 ? 1 : 0, inst);
  } else {
    Serial2.printf("v,%d\n", mode != 0 ? 1 : 0);
  }
}

}  // namespace teensylink
