#include "screen_playlist.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "screen_pattern.h"
#include "teensy_link.h"
#include "ui_common.h"

namespace playlist {
namespace {

using namespace ui;

constexpr int kMaxClips = teensylink::kMaxClips;
constexpr uint32_t kHandleRgb = 0xffaa00;
constexpr uint32_t kPlayheadRgb = 0x00e5ff;
constexpr int kHandleW = 14;
constexpr int kHandleGrab = 44;
constexpr int kMinClipPx = 12;
constexpr uint32_t kCropSendIntervalMs = 100;
constexpr uint32_t kHoldToMoveMs = 350;
constexpr int kMoveSlopPx = 12;
constexpr Rect kSnipBtn = {350, 64, 470, 100};
constexpr Rect kUndoBtn = {480, 64, 600, 100};

uint32_t laneBgRgb(int lane) { return (lane % 2 == 0) ? 0x232323 : 0x2c2c2c; }

// ---- Timeline <-> pixels ----

float pxPerMs() {
  return g.songMs ? (float)(kWaveRight - kWaveLeft) / (float)g.songMs : 0.0f;
}

// Unclamped: ms may be negative or past the end.
int msToXu(int32_t ms) { return kWaveLeft + (int)lroundf((float)ms * pxPerMs()); }

int msToX(uint32_t ms) {
  if (g.songMs == 0) return kWaveLeft;
  float f = (float)ms / (float)g.songMs;
  if (f > 1.0f) f = 1.0f;
  return kWaveLeft + (int)(f * (kWaveRight - kWaveLeft));
}

uint32_t xToMs(int x) {
  x = constrain(x, kWaveLeft, kWaveRight);
  return (uint32_t)((float)(x - kWaveLeft) / (float)(kWaveRight - kWaveLeft) * (float)g.songMs);
}

int32_t dxToMs(int dx) {
  float p = pxPerMs();
  return p > 0.0f ? (int32_t)lroundf((float)dx / p) : 0;
}

struct Track {
  int clipCount = 1;
  int cs[kMaxClips] = {kWaveLeft};   // clip edges, timeline pixels
  int ce[kMaxClips] = {kWaveRight};
  int32_t co[kMaxClips] = {0};       // each clip's timeline offset (ms)
  int lo[kMaxClips] = {kWaveLeft};   // pixels each clip's waveform art covers (room to crop)
  int hi[kMaxClips] = {kWaveRight};
  int x0[kMaxClips] = {kWaveLeft};   // where the clip's file starts / ends on the timeline
  int x1[kMaxClips] = {kWaveRight};
  bool empty = false;                // no audio on this track
  uint32_t fileLenMs = 0;
};

Track tracks[kNumTracks];
int playheadX = kWaveLeft;
bool visibleNow = false;

// File time at pixel x for clip k (never negative).
int32_t fileMsAt(const Track& t, int k, int x) {
  int32_t ms = dxToMs(x - kWaveLeft) - t.co[k];
  return ms < 0 ? 0 : ms;
}

// The clip whose pixel span holds x, or -1.
int clipAtX(const Track& t, int x) {
  for (int k = 0; k < t.clipCount; ++k) {
    if (x >= t.cs[k] && x < t.ce[k]) return k;
  }
  return -1;
}

bool touching = false;
int dragTrack = -1;
int dragClip = -1;
bool dragIsStart = false;
int dragOffset = 0;
bool dirtyLane[kNumTracks] = {false, false, false, false};
uint32_t lastDragDrawMs = 0;
uint32_t lastCropSendMs = 0;
uint8_t pendingTrackMask = 0;

// Pressing on a lane away from any handle: tap -> seek, drag -> scrub, hold -> move the track.
enum class Press { None, Pending, Scrubbing, Moving };
Press press = Press::None;
int pressLane = -1;
int pressX = 0;
uint32_t pressDownMs = 0;
int curX = 0;
uint32_t lastScrubSendMs = 0;
int pressClip = -1;
int32_t moveOffset0 = 0;
int32_t moveNewOffset = 0;
int32_t moveMin = 0;  // allowed range for the moved clip's offset
int32_t moveMax = 0;
uint32_t lastGhostMs = 0;

bool snipMode = false;
bool snipEligible = false;  // this touch may cut
int snipLane = -1;
int snipX = 0;

// One undoable edit. Snip: value = file position of the cut. Move: value = the clip's previous
// offset. Pattern add: clip = index of the new clip. Pattern remove: value/v2/v3 = the removed
// clip's pattern / start bar / length. Pattern move: value = previous start bar. Pattern
// resize: value = previous length in bars.
enum EditKind { kEditSnip, kEditMove, kEditPatAdd, kEditPatRemove, kEditPatMove, kEditPatResize };
struct EditRec {
  int kind;
  int lane;
  int clip;
  int32_t value;
  int32_t v2;
  int32_t v3;
};
EditRec history[16];
int historyCount = 0;
char lastProjectName[24] = {0};

bool beatsOn[kNumTracks] = {false, false, false, false};
uint32_t beatHoldUntil = 0;
bool beatMaskPending = false;

int laneY(int lane) { return kLaneTop + lane * kLaneStride; }
Rect plusRect(int lane) { return {16, laneY(lane) + 8, 82, laneY(lane) + 44}; }
Rect beatRect(int lane) { return {16, laneY(lane) + 52, 82, laneY(lane) + 88}; }

// ---- Waveform art: real peaks when they've arrived, placeholder shape before ----

float fakeWave(int lane, int x) {
  float t = (float)(x - kWaveLeft);
  float env = 0.35f + 0.65f * fabsf(sinf(t * 0.0075f * (lane + 1) + lane));
  float body = fabsf(sinf(t * 0.19f + lane * 1.7f)) * 0.6f + fabsf(sinf(t * 0.47f)) * 0.4f;
  return env * body;
}

// 0..1 height for the part of the file between fractions fa and fb.
float barHeight(int lane, int x, float fa, float fb, int maxPeak) {
  const teensylink::Wave& w = teensylink::state().wave[lane];
  if (!w.complete || w.cols <= 0) return fakeWave(lane, x);
  int c0 = constrain((int)(fa * w.cols), 0, w.cols - 1);
  int c1 = constrain((int)(fb * w.cols), c0 + 1, w.cols);
  int peak = 0;
  for (int c = c0; c < c1; ++c) {
    if (w.peaks[c] > peak) peak = w.peaks[c];
  }
  return sqrtf((float)peak / (float)maxPeak);
}

void drawLaneArt(unsigned long layer, int lane, float scale) {
  setCanvas(layer);
  const Track& t = tracks[lane];
  const int y = laneY(lane);
  fillRect(0, y, kW - 1, y + kLaneH - 1, rgb565(laneBgRgb(lane)));
  fillRect(0, y, 13, y + kLaneH - 1, rgb565(kTrackRgb[lane], scale < 1.0f ? 0.45f : 1.0f));
  const int mid = y + 66;
  const uint16_t col = rgb565(kTrackRgb[lane], scale);
  const teensylink::Wave& w = teensylink::state().wave[lane];
  int maxPeak = 8;
  if (w.complete) {
    for (int c = 0; c < w.cols; ++c) {
      if (w.peaks[c] > maxPeak) maxPeak = w.peaks[c];
    }
  }
  // Each clip draws the part of its file that fits around it, at the clip's own position.
  for (int k = 0; k < t.clipCount; ++k) {
    const int span = t.x1[k] - t.x0[k];
    if (span <= 0) continue;
    int startX = kWaveLeft + ((max(kWaveLeft, t.lo[k]) - kWaveLeft) / 5) * 5;
    int endX = min(kWaveRight, t.hi[k]);
    if (endX > startX) fillRect(startX, y, endX - 1, y + kLaneH - 1, rgb565(laneBgRgb(lane)));
    for (int x = startX; x < endX; x += 5) {
      float fa = (float)(x - t.x0[k]) / (float)span;
      float fb = (float)(x + 5 - t.x0[k]) / (float)span;
      int half = (int)(barHeight(lane, x, fa, fb, maxPeak) * 30.0f) + 2;
      fillRect(x, mid - half, x + 3, mid + half, col);
    }
  }
}

void drawBeatToggle(int lane, const teensylink::Beats& b) {
  uint32_t fill = 0x444444;
  if (beatsOn[lane]) fill = b.valid ? 0xc62828 : 0xb26a00;
  drawButton(beatRect(lane), "BEAT", fontSmall(), rgb565(0xffffff), rgb565(fill));
}

// Little red ticks on beat 1 of every bar (beat times are in file time; shift by each clip's offset).
void drawBeatTicks(int lane, const Track& t) {
  const teensylink::Beats& b = teensylink::state().beats[lane];
  if (!beatsOn[lane] || !b.valid) return;
  const int y = laneY(lane);
  const float barMs = b.barMs10 / 10.0f;
  for (int c = 0; c < t.clipCount; ++c) {
    for (int k = 0; k < 600; ++k) {
      float ms = b.firstMs + k * barMs + (float)t.co[c];
      if (ms > (float)g.songMs) break;
      if (ms < 0.0f) continue;
      int x = msToX((uint32_t)ms);
      if (x >= t.hi[c]) break;
      if (x < t.lo[c] || x < kWaveLeft) continue;
      fillRect(x - 1, y + kLaneH - 26, x + 1, y + kLaneH - 3, rgb565(0xff2222));
    }
  }
}

void composeLane(int lane) {
  const int y = laneY(lane);
  const Track& t = tracks[lane];
  const teensylink::Beats& b = teensylink::state().beats[lane];
  bteCopy(kDim, kComposed, 0, y, kW, kLaneH);
  char label[80];
  if (t.empty) {
    setCanvas(kComposed);
    snprintf(label, sizeof(label), "TRACK %d    (empty)", lane + 1);
    drawText(kWaveLeft + 4, y + 4, label, fontSmall(), rgb565(0x888888), rgb565(laneBgRgb(lane)));
    drawButton(plusRect(lane), "+", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3d));
    return;
  }
  for (int k = 0; k < t.clipCount; ++k) {
    bteCopy(kBright, kComposed, t.cs[k], y, t.ce[k] - t.cs[k], kLaneH);
  }

  setCanvas(kComposed);
  drawBeatTicks(lane, t);
  for (int k = 0; k < t.clipCount; ++k) {
    fillRect(t.cs[k] + 1, y, t.cs[k] + kHandleW, y + kLaneH - 1, rgb565(kHandleRgb));
    fillRect(t.ce[k] - kHandleW, y, t.ce[k] - 1, y + kLaneH - 1, rgb565(kHandleRgb));
  }

  char bpmText[16] = "";
  if (b.valid) snprintf(bpmText, sizeof(bpmText), "    %d BPM", (b.bpm100 + 50) / 100);
  char clipText[20] = "";
  if (t.clipCount > 1) snprintf(clipText, sizeof(clipText), "    %d clips", t.clipCount);
  snprintf(label, sizeof(label), "TRACK %d    in %.2fs    out %.2fs%s%s", lane + 1,
           xToMs(t.cs[0]) / 1000.0f, xToMs(t.ce[t.clipCount - 1]) / 1000.0f, clipText, bpmText);
  drawText(kWaveLeft + 4, y + 4, label, fontSmall(), rgb565(0xffffff), rgb565(laneBgRgb(lane)));
  drawButton(plusRect(lane), "+", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3d));
  drawBeatToggle(lane, b);
}

void presentLane(int lane) { bteCopy(kComposed, kVisible, 0, laneY(lane), kW, kLaneH); }

void rebuildLane(int lane) {
  drawLaneArt(kBright, lane, 1.0f);
  drawLaneArt(kDim, lane, 0.42f);
  composeLane(lane);
  if (visibleNow) presentLane(lane);
}

void drawPlayhead() {
  setCanvas(kVisible);
  fillRect(playheadX - 1, kLaneTop - 2, playheadX + 1, kLanesBottom + 2, rgb565(kPlayheadRgb));
}

void erasePlayhead(int x) {
  bteCopy(kComposed, kVisible, x - 3, kLaneTop - 2, 7, kLanesBottom - kLaneTop + 4);
}

void drawSnipButton() {
  setCanvas(kVisible);
  drawButton(kSnipBtn, snipMode ? "SNIP ON" : "SNIP", fontSmall(), rgb565(0xffffff),
             rgb565(snipMode ? 0xe65100 : 0x555555));
}

void drawUndoButton() {
  setCanvas(kVisible);
  drawButton(kUndoBtn, "UNDO", fontSmall(), rgb565(historyCount > 0 ? 0xffffff : 0x888888),
             rgb565(historyCount > 0 ? 0x1c4f9c : 0x333333));
}

void clearSnipPreview() {
  if (snipLane < 0) return;
  bteCopy(kComposed, kVisible, snipX - 3, laneY(snipLane), 7, kLaneH);
  snipLane = -1;
  drawPlayhead();
}

// True if x can be cut in this lane: inside a clip, not right at its edges.
bool canCutAt(int lane, int x) {
  const Track& t = tracks[lane];
  if (t.empty) return false;
  for (int k = 0; k < t.clipCount; ++k) {
    if (x > t.cs[k] + kMinClipPx && x < t.ce[k] - kMinClipPx) return true;
  }
  return false;
}

void updateSnipPreview(int x, int y) {
  int lane = -1;
  for (int l = 0; l < kNumTracks; ++l) {
    if (y >= laneY(l) && y <= laneY(l) + kLaneH) lane = l;
  }
  if (lane >= 0 && !canCutAt(lane, x)) lane = -1;
  if (lane < 0) {
    clearSnipPreview();
    return;
  }
  if (lane == snipLane && x == snipX) return;
  clearSnipPreview();
  snipLane = lane;
  snipX = x;
  setCanvas(kVisible);
  fillRect(snipX - 1, laneY(lane), snipX + 1, laneY(lane) + kLaneH - 1, rgb565(0xffffff));
}

void pushHistory(const EditRec& rec) {
  if (historyCount < 16) {
    history[historyCount++] = rec;
  } else {
    memmove(history, history + 1, sizeof(history) - sizeof(history[0]));
    history[15] = rec;
  }
  drawUndoButton();
}

// Undoes the most recent edit (snip, clip move, or a pattern-lane change).
void undoEdit() {
  while (historyCount > 0) {
    EditRec rec = history[--historyCount];
    const teensylink::Drums& drums = teensylink::state().drums;
    if (rec.kind == kEditPatAdd) {
      if (rec.clip >= drums.clipCount) continue;
      if (g.linked) teensylink::removePatClip(rec.clip);
      break;
    }
    if (rec.kind == kEditPatRemove) {
      if (g.linked) teensylink::addPatClip(rec.value, rec.v2, rec.v3, rec.clip);
      break;
    }
    if (rec.kind == kEditPatMove) {
      if (rec.clip >= drums.clipCount) continue;
      if (g.linked) teensylink::movePatClip(rec.clip, rec.value);
      break;
    }
    if (rec.kind == kEditPatResize) {
      if (rec.clip >= drums.clipCount) continue;
      if (g.linked) teensylink::resizePatClip(rec.clip, rec.value);
      break;
    }
    const teensylink::TrackInfo& info = teensylink::state().track[rec.lane];
    if (rec.kind == kEditMove) {
      if (rec.clip >= info.clipCount) continue;
      if (g.linked) teensylink::sendClipOffset(rec.lane, rec.clip, rec.value);
      break;
    }
    if (info.clipCount < 2) continue;  // already joined some other way
    int best = 0;
    long bestDist = 1L << 30;
    for (int k = 0; k + 1 < info.clipCount; ++k) {
      long d = labs((long)info.clipEnd[k] - (long)rec.value);
      if (d < bestDist) {
        bestDist = d;
        best = k;
      }
    }
    if (g.linked) teensylink::sendMerge(rec.lane, best);
    break;
  }
  drawUndoButton();
}

void applyTrackFromLink(int lane) {
  const teensylink::TrackInfo& info = teensylink::state().track[lane];
  if (!info.known) return;
  Track& t = tracks[lane];
  t.empty = info.fileLenMs == 0;
  t.fileLenMs = info.fileLenMs;
  t.clipCount = constrain(info.clipCount, 1, kMaxClips);
  const int32_t len = (int32_t)t.fileLenMs;
  int prevEnd = kWaveLeft;
  for (int k = 0; k < t.clipCount; ++k) {
    t.co[k] = info.clipOff[k];
    t.x0[k] = msToXu(t.co[k]);
    t.x1[k] = msToXu(t.co[k] + len);
    int32_t sMs = (int32_t)min(info.clipStart[k], (uint32_t)len);
    int32_t eMs = (int32_t)min(info.clipEnd[k], (uint32_t)len);
    int s = constrain(msToXu(t.co[k] + sMs), prevEnd, kWaveRight);
    int e = constrain(msToXu(t.co[k] + eMs), s, kWaveRight);
    t.cs[k] = s;
    t.ce[k] = e;
    prevEnd = e;
  }
  for (int k = 0; k < t.clipCount; ++k) {
    t.lo[k] = max(t.x0[k], k > 0 ? t.ce[k - 1] : kWaveLeft);
    t.hi[k] = min(t.x1[k], k < t.clipCount - 1 ? t.cs[k + 1] : kWaveRight);
  }
}

void sendDragCrop(int lane, int clip) {
  if (!g.linked) return;
  const Track& t = tracks[lane];
  teensylink::sendCrop(lane, clip, fileMsAt(t, clip, t.cs[clip]), fileMsAt(t, clip, t.ce[clip]));
}

void flushDirtyLanes() {
  for (int lane = 0; lane < kNumTracks; ++lane) {
    if (!dirtyLane[lane]) continue;
    dirtyLane[lane] = false;
    composeLane(lane);
    presentLane(lane);
    drawPlayhead();
  }
}

// ---- Moving a clip along the timeline ----

int32_t moveClipS = 0;  // the moved clip's file window
int32_t moveClipE = 0;

void drawMoveGhost() {
  const int y = laneY(pressLane);
  presentLane(pressLane);
  int gx0 = msToXu(moveNewOffset + moveClipS);
  int gx1 = msToXu(moveNewOffset + moveClipE);
  int a = max(gx0, kWaveLeft);
  int b = min(gx1, kWaveRight);
  const uint16_t c = rgb565(0xffaa00);
  if (b > a + 6) {
    fillRect(a, y, b, y + 3, c);
    fillRect(a, y + kLaneH - 4, b, y + kLaneH - 1, c);
    if (gx0 >= kWaveLeft) fillRect(gx0, y, gx0 + 3, y + kLaneH - 1, c);
    if (gx1 <= kWaveRight) fillRect(gx1 - 3, y, gx1, y + kLaneH - 1, c);
  }
  char label[24];
  snprintf(label, sizeof(label), "MOVE %+.2f s", (moveNewOffset - moveOffset0) / 1000.0f);
  drawText(max(a, kWaveLeft) + 10, y + 40, label, fontSmall(), rgb565(0xffffff), rgb565(0x8a4a00));
  drawPlayhead();
}

void updateMoveGhost(int x) {
  int32_t off = constrain(moveOffset0 + dxToMs(x - pressX), moveMin, moveMax);
  if (off == moveNewOffset) return;
  moveNewOffset = off;
  uint32_t now = millis();
  if (now - lastGhostMs < 40) return;
  lastGhostMs = now;
  drawMoveGhost();
}

// Picks up the clip under the finger (the whole track if it was never snipped).
void beginMove() {
  const Track& t = tracks[pressLane];
  const teensylink::TrackInfo& info = teensylink::state().track[pressLane];
  int k = clipAtX(t, pressX);
  if (t.empty || t.fileLenMs == 0 || k < 0 || k >= info.clipCount) {
    press = Press::Scrubbing;  // nothing to pick up here
    return;
  }
  const uint32_t len = t.fileLenMs;
  pressClip = k;
  moveClipS = (int32_t)min(info.clipStart[k], len);
  moveClipE = (int32_t)min(info.clipEnd[k], len);
  moveOffset0 = info.clipOff[k];
  moveNewOffset = moveOffset0;
  // Stay clear of the neighbouring clips and of the start of the timeline; the last clip
  // may go as far as the end of the visible timeline.
  int64_t lowerTl = (k > 0) ? (int64_t)min(info.clipEnd[k - 1], len) + info.clipOff[k - 1] : 0;
  moveMin = (int32_t)(lowerTl - moveClipS);
  if (k < info.clipCount - 1) {
    moveMax = (int32_t)((int64_t)info.clipStart[k + 1] + info.clipOff[k + 1] - moveClipE);
  } else {
    moveMax = (int32_t)((int64_t)g.songMs - 200 - moveClipS);
  }
  if (moveMax < moveMin) moveMax = moveMin;
  press = Press::Moving;
  lastGhostMs = millis();
  drawMoveGhost();
}


// ---- Drum-pattern lane ----

constexpr int kPatLane = kNumTracks;  // lane index used by the press state machine
constexpr int kPatBandTop = kPatLaneTop + 30;
constexpr int kPatBandBottom = kPatLaneTop + kPatLaneH - 4;
constexpr int kPatResizeGrab = 26;
constexpr uint32_t kPatLaneBg = 0x24202e;

int selClip = -1;              // selected pattern clip (index into the link's clip list)
int patMoveBar0 = 0;           // move / resize gesture state
int patMoveBar = 0;
int patLen0 = 0;
int patLen = 0;
int patResizeIdx = -1;
uint32_t lastClipsVersion = 0;
int lastBpmShown = -1;

Rect patCycleRect() { return {16, kPatLaneTop + 4, 82, kPatLaneTop + 32}; }
Rect patAddRect() { return {16, kPatLaneTop + 38, 82, kPatLaneTop + 66}; }
Rect patDelRect() { return {16, kPatLaneTop + 72, 82, kPatLaneTop + 100}; }

float barMsF() { return 240000.0f / (float)max(1, teensylink::state().drums.bpm); }
float barPxF() { return pxPerMs() * barMsF(); }
int xOfBar(int bar) { return kWaveLeft + (int)lroundf((float)bar * barPxF()); }
int barOfX(int x) {
  float px = barPxF();
  return px > 0.0f ? (int)floorf((float)(x - kWaveLeft) / px) : 0;
}

// Pixel span of pattern clip i, clamped to the lane's drawing area.
void patClipSpan(int i, int& x1, int& x2) {
  const teensylink::PatClip& c = teensylink::state().drums.clips[i];
  x1 = min(max(xOfBar(c.startBar), kWaveLeft), kWaveRight);
  x2 = min(max(xOfBar(c.startBar + c.lenBars), x1 + 6), kWaveRight);
}

int patClipAtX(int x) {
  const teensylink::Drums& d = teensylink::state().drums;
  for (int i = d.clipCount - 1; i >= 0; --i) {  // later clips draw on top
    int x1, x2;
    patClipSpan(i, x1, x2);
    if (x >= x1 && x <= x2) return i;
  }
  return -1;
}

void composePatLane() {
  const teensylink::Drums& d = teensylink::state().drums;
  const int y = kPatLaneTop;
  setCanvas(kComposed);
  fillRect(0, y, kW - 1, y + kPatLaneH - 1, rgb565(kPatLaneBg));
  fillRect(0, y, 13, y + kPatLaneH - 1, rgb565(pattern::patternRgb(g.pattern)));

  // Bar lines, spaced at least ~10 px apart (every bar, or every 4th / 16th when zoomed out).
  float px = barPxF();
  if (px > 0.0f) {
    int every = 1;
    while (px * every < 10.0f && every < 64) every *= 4;
    for (int bar = 0;; bar += every) {
      int x = xOfBar(bar);
      if (x > kWaveRight) break;
      fillRect(x, kPatBandTop, x, kPatBandBottom, rgb565((bar % 16 == 0) ? 0x4a4460 : 0x322e40));
    }
  }

  char label[96];
  if (d.clipCount == 0) {
    snprintf(label, sizeof(label), "PATTERNS    %d BPM    press + to place P%d at the playhead", d.bpm,
             g.pattern + 1);
  } else {
    snprintf(label, sizeof(label), "PATTERNS    %d BPM", d.bpm);
  }
  drawText(kWaveLeft + 4, y + 4, label, fontSmall(), rgb565(0xffffff), rgb565(kPatLaneBg));

  for (int i = 0; i < d.clipCount; ++i) {
    int x1, x2;
    patClipSpan(i, x1, x2);
    const uint32_t rgb = pattern::patternRgb(d.clips[i].pattern);
    fillRect(x1, kPatBandTop + 2, x2, kPatBandBottom - 2, rgb565(rgb, 0.85f));
    fillRect(x1, kPatBandTop + 2, x1 + 1, kPatBandBottom - 2, rgb565(0x000000));
    fillRect(x2 - 1, kPatBandTop + 2, x2, kPatBandBottom - 2, rgb565(0x000000));
    if (x2 - x1 >= 40) {
      char nm[8];
      snprintf(nm, sizeof(nm), "P%d", d.clips[i].pattern + 1);
      drawButton({x1 + 2, kPatBandTop + 4, x1 + 38, kPatBandTop + 34}, nm, fontSmall(), rgb565(0x000000),
                 rgb565(rgb));
    }
    if (i == selClip) {
      const uint16_t w = rgb565(0xffffff);
      fillRect(x1, kPatBandTop, x2, kPatBandTop + 2, w);
      fillRect(x1, kPatBandBottom - 2, x2, kPatBandBottom, w);
      fillRect(x1, kPatBandTop, x1 + 2, kPatBandBottom, w);
      fillRect(x2 - 2, kPatBandTop, x2, kPatBandBottom, w);
      fillRect(max(x1 + 3, x2 - 12), kPatBandTop + 3, x2 - 3, kPatBandBottom - 3, rgb565(0x000000));
    }
  }

  char cyc[8];
  snprintf(cyc, sizeof(cyc), "P%d", g.pattern + 1);
  drawButton(patCycleRect(), cyc, fontSmall(), rgb565(0x000000), rgb565(pattern::patternRgb(g.pattern)));
  drawButton(patAddRect(), "+", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3d));
  drawButton(patDelRect(), "DEL", fontSmall(), rgb565(selClip >= 0 ? 0xffffff : 0x888888),
             rgb565(selClip >= 0 ? 0x9c2a2a : 0x333333));
}

void presentPatLane() { bteCopy(kComposed, kVisible, 0, kPatLaneTop, kW, kPatLaneH); }

void refreshPatLane() {
  composePatLane();
  if (visibleNow) {
    presentPatLane();
    drawPlayhead();
  }
}

// Orange outline of the pattern clip being moved or stretched.
void drawPatGhost(int bar, int len) {
  presentPatLane();
  int x1 = min(max(xOfBar(bar), kWaveLeft), kWaveRight);
  int x2 = min(max(xOfBar(bar + len), x1 + 6), kWaveRight);
  const uint16_t c = rgb565(0xffaa00);
  fillRect(x1, kPatBandTop, x2, kPatBandTop + 3, c);
  fillRect(x1, kPatBandBottom - 3, x2, kPatBandBottom, c);
  fillRect(x1, kPatBandTop, x1 + 3, kPatBandBottom, c);
  fillRect(x2 - 3, kPatBandTop, x2, kPatBandBottom, c);
  char label[24];
  snprintf(label, sizeof(label), "BAR %d  x%d", bar + 1, len);
  drawText(x1 + 8, kPatBandTop + 8, label, fontSmall(), rgb565(0xffffff), rgb565(0x8a4a00));
  drawPlayhead();
}

void beginPatMove() {
  const teensylink::Drums& d = teensylink::state().drums;
  if (pressClip < 0 || pressClip >= d.clipCount) {
    press = Press::Scrubbing;
    return;
  }
  patMoveBar0 = d.clips[pressClip].startBar;
  patMoveBar = patMoveBar0;
  patLen = d.clips[pressClip].lenBars;
  press = Press::Moving;
  drawPatGhost(patMoveBar, patLen);
}

void updatePatMove(int x) {
  float px = barPxF();
  int delta = px > 0.0f ? (int)lroundf((float)(x - pressX) / px) : 0;
  int bar = max(0, patMoveBar0 + delta);
  if (bar == patMoveBar) return;
  patMoveBar = bar;
  drawPatGhost(patMoveBar, patLen);
}

void updatePatResize(int x) {
  const teensylink::Drums& d = teensylink::state().drums;
  if (patResizeIdx < 0 || patResizeIdx >= d.clipCount) return;
  float px = barPxF();
  int startBar = d.clips[patResizeIdx].startBar;
  int len = px > 0.0f ? (int)lroundf((float)(x - xOfBar(startBar)) / px) : 1;
  len = constrain(len, 1, 99);
  if (len == patLen) return;
  patLen = len;
  drawPatGhost(startBar, patLen);
}

// Press inside the pattern lane. Returns -1 (the "+" picker index is only for track lanes).
int patTouchDown(int x, int y) {
  press = Press::None;
  patResizeIdx = -1;
  const teensylink::Drums& d = teensylink::state().drums;
  if (inRect(patCycleRect(), x, y)) {
    g.pattern = (g.pattern + 1) % teensylink::kNumPatterns;
    refreshPatLane();
    return -1;
  }
  if (inRect(patAddRect(), x, y)) {
    if (d.clipCount < teensylink::kMaxPatClips && g.linked) {
      int bar = (int)((float)g.posMs / barMsF());
      teensylink::addPatClip(g.pattern, bar, 4);
      selClip = teensylink::state().drums.clipCount - 1;
      pushHistory({kEditPatAdd, kPatLane, selClip, 0, 0, 0});
    }
    refreshPatLane();
    return -1;
  }
  if (inRect(patDelRect(), x, y)) {
    if (selClip >= 0 && selClip < d.clipCount && g.linked) {
      const teensylink::PatClip c = d.clips[selClip];
      pushHistory({kEditPatRemove, kPatLane, selClip, c.pattern, c.startBar, c.lenBars});
      teensylink::removePatClip(selClip);
      selClip = -1;
    }
    refreshPatLane();
    return -1;
  }
  if (snipMode) return -1;
  int i = patClipAtX(x);
  if (i >= 0 && y >= kPatBandTop) {
    int x1, x2;
    patClipSpan(i, x1, x2);
    selClip = i;
    if (x >= x2 - kPatResizeGrab && x2 - x1 >= 2 * kPatResizeGrab) {
      patResizeIdx = i;
      patLen0 = d.clips[i].lenBars;
      patLen = patLen0;
      refreshPatLane();
      return -1;
    }
    refreshPatLane();
  } else if (selClip >= 0) {
    selClip = -1;
    refreshPatLane();
  }
  // Pressing a clip's body: tap seeks, dragging scrubs, holding picks it up. Empty space: seek/scrub.
  if (x >= kWaveLeft - 8 && x <= kWaveRight + 8) {
    press = Press::Pending;
    pressLane = kPatLane;
    pressClip = i;
    pressX = x;
    pressDownMs = millis();
  }
  return -1;
}

}  // namespace

void init() {
  for (int lane = 0; lane < kNumTracks; ++lane) {
    drawLaneArt(kBright, lane, 1.0f);
    drawLaneArt(kDim, lane, 0.42f);
  }
  setCanvas(kComposed);
  fillRect(0, 0, kW - 1, kH - 1, rgb565(0x000000));
  for (int lane = 0; lane < kNumTracks; ++lane) composeLane(lane);
  composePatLane();
}

void enter() {
  visibleNow = true;
  snipMode = false;
  snipEligible = false;
  snipLane = -1;
  press = Press::None;
  setCanvas(kVisible);
  composePatLane();  // the current pattern may have changed on the Pattern screen
  for (int lane = 0; lane < kNumTracks; ++lane) presentLane(lane);
  presentPatLane();
  playheadX = msToX(g.posMs);
  drawPlayhead();
  drawSnipButton();
  drawUndoButton();
}

bool stripButtonDown(int x, int y) {
  if (inRect(kSnipBtn, x, y)) {
    snipMode = !snipMode;
    snipEligible = snipMode;  // dragging down from the button into a lane cuts
    if (!snipMode) clearSnipPreview();
    drawSnipButton();
    return true;
  }
  if (inRect(kUndoBtn, x, y)) {
    undoEdit();
    return true;
  }
  return false;
}

int touchDown(int x, int y) {
  touching = true;
  dragTrack = -1;
  dragClip = -1;
  press = Press::None;
  curX = x;
  if (y >= kPatLaneTop && y <= kPatLaneTop + kPatLaneH) return patTouchDown(x, y);
  for (int lane = 0; lane < kNumTracks; ++lane) {
    int ly = laneY(lane);
    if (y < ly || y > ly + kLaneH) continue;
    if (inRect(plusRect(lane), x, y)) return lane;  // "+" opens the audio picker
    if (!tracks[lane].empty && inRect(beatRect(lane), x, y)) {
      beatsOn[lane] = !beatsOn[lane];
      beatHoldUntil = millis() + 900;
      if (g.linked) teensylink::sendBeatFlag(lane, beatsOn[lane]);
      composeLane(lane);
      presentLane(lane);
      drawPlayhead();
      return -1;
    }
    if (snipMode) {
      snipEligible = true;
      updateSnipPreview(x, y);
      return -1;
    }
    const Track& t = tracks[lane];
    if (!t.empty) {
      // Nearest crop handle of any clip in this lane.
      int bestDist = kHandleGrab + 1;
      for (int k = 0; k < t.clipCount; ++k) {
        int dStart = abs(x - (t.cs[k] + kHandleW / 2));
        int dEnd = abs(x - (t.ce[k] - kHandleW / 2));
        if (dStart < bestDist) {
          bestDist = dStart;
          dragTrack = lane;
          dragClip = k;
          dragIsStart = true;
          dragOffset = t.cs[k] - x;
        }
        if (dEnd < bestDist) {
          bestDist = dEnd;
          dragTrack = lane;
          dragClip = k;
          dragIsStart = false;
          dragOffset = t.ce[k] - x;
        }
      }
      if (dragTrack >= 0) return -1;
    }
    if (x >= kWaveLeft - 8 && x <= kWaveRight + 8) {
      // Undecided yet: a tap seeks, dragging scrubs, holding still picks the track up.
      press = Press::Pending;
      pressLane = lane;
      pressX = x;
      pressDownMs = millis();
    }
    return -1;
  }
  return -1;
}

void touchMove(int x, int y) {
  curX = x;
  if (snipMode && snipEligible) {
    updateSnipPreview(x, y);
    return;
  }
  if (press == Press::Pending) {
    if (abs(x - pressX) <= kMoveSlopPx) return;
    press = Press::Scrubbing;
    lastScrubSendMs = millis();
    seekToMs(xToMs(x));
    return;
  }
  if (patResizeIdx >= 0) {
    updatePatResize(x);
    return;
  }
  if (press == Press::Moving) {
    if (pressLane == kPatLane) {
      updatePatMove(x);
    } else {
      updateMoveGhost(x);
    }
    return;
  }
  if (press == Press::Scrubbing) {
    uint32_t now = millis();
    if (now - lastScrubSendMs >= 60) {
      lastScrubSendMs = now;
      seekToMs(xToMs(x));
    } else {
      g.posMs = xToMs(x);
      g.posHoldUntil = now + 450;
    }
    return;
  }
  if (dragTrack < 0) return;
  Track& t = tracks[dragTrack];
  int nb = x + dragOffset;
  int k = dragClip;
  if (dragIsStart) {
    int lo = t.lo[k];
    int hi = t.ce[k] - kMinClipPx;
    if (hi < lo) hi = lo;
    t.cs[k] = constrain(nb, lo, hi);
  } else {
    int lo = t.cs[k] + kMinClipPx;
    int hi = t.hi[k];
    if (hi < lo) lo = hi;
    t.ce[k] = constrain(nb, lo, hi);
  }
  dirtyLane[dragTrack] = true;
}

void touchUp() {
  touching = false;
  if (snipEligible) {
    if (snipMode && snipLane >= 0 && g.linked) {
      const Track& st = tracks[snipLane];
      int sk = clipAtX(st, snipX);
      if (sk >= 0) {
        int32_t fm = fileMsAt(st, sk, snipX);
        teensylink::sendSplit(snipLane, (uint32_t)fm);
        pushHistory({kEditSnip, snipLane, -1, fm, 0, 0});
      }
    }
    clearSnipPreview();
    snipEligible = false;
  }
  if (patResizeIdx >= 0) {
    if (g.linked && patLen != patLen0) {
      teensylink::resizePatClip(patResizeIdx, patLen);
      pushHistory({kEditPatResize, kPatLane, patResizeIdx, patLen0, 0, 0});
    }
    patResizeIdx = -1;
    refreshPatLane();
  }
  switch (press) {
    case Press::Pending:  // a plain tap: move the playhead there
      seekToMs(xToMs(pressX));
      break;
    case Press::Scrubbing:
      seekToMs(g.posMs);
      break;
    case Press::Moving:
      if (pressLane == kPatLane) {
        if (g.linked && patMoveBar != patMoveBar0) {
          teensylink::movePatClip(pressClip, patMoveBar);
          pushHistory({kEditPatMove, kPatLane, pressClip, patMoveBar0, 0, 0});
        }
        refreshPatLane();
        break;
      }
      if (g.linked && moveNewOffset != moveOffset0) {
        teensylink::sendClipOffset(pressLane, pressClip, moveNewOffset);
        pushHistory({kEditMove, pressLane, pressClip, moveOffset0, 0, 0});
      }
      presentLane(pressLane);
      drawPlayhead();
      break;
    case Press::None:
      break;
  }
  press = Press::None;
  if (dragTrack >= 0) sendDragCrop(dragTrack, dragClip);
  dragTrack = -1;
  dragClip = -1;
  flushDirtyLanes();
}

void update(bool visible) {
  visibleNow = visible;
  uint32_t now = millis();

  if (strcmp(lastProjectName, teensylink::state().project.name) != 0) {
    strncpy(lastProjectName, teensylink::state().project.name, sizeof(lastProjectName) - 1);
    historyCount = 0;  // edits from another project can't be undone here
    selClip = -1;
    if (visible) drawUndoButton();
  }

  // Holding still on a lane picks the track up.
  if (visible && press == Press::Pending && now - pressDownMs >= kHoldToMoveMs &&
      abs(curX - pressX) <= kMoveSlopPx && !snipMode) {
    if (pressLane == kPatLane) {
      beginPatMove();
    } else {
      beginMove();
    }
  }

  bool songChanged = teensylink::takeSongChanged();
  uint8_t mask = teensylink::takeChangedTracks() | teensylink::takeChangedWaves() | pendingTrackMask;
  pendingTrackMask = 0;
  if (songChanged) mask = 0x0F;
  bool redrew = false;
  for (int lane = 0; lane < kNumTracks; ++lane) {
    if (!(mask & (1 << lane))) continue;
    if (dragTrack == lane || (press == Press::Moving && pressLane == lane)) {
      pendingTrackMask |= (1 << lane);
      continue;
    }
    applyTrackFromLink(lane);
    rebuildLane(lane);
    redrew = true;
  }

  {
    const teensylink::Drums& d = teensylink::state().drums;
    uint32_t ver = teensylink::patClipsVersion();
    if (ver != lastClipsVersion || d.bpm != lastBpmShown || songChanged) {
      lastClipsVersion = ver;
      lastBpmShown = d.bpm;
      if (selClip >= d.clipCount) selClip = -1;
      if (press != Press::Moving && patResizeIdx < 0) {
        composePatLane();
        if (visible) {
          presentPatLane();
          redrew = true;
        }
      }
    }
  }

  uint8_t beatMask = teensylink::takeChangedBeats();
  if (teensylink::takeBeatMaskChanged()) beatMaskPending = true;
  if (beatMaskPending && now >= beatHoldUntil) {
    beatMaskPending = false;
    int saved = teensylink::state().beatMask;
    for (int lane = 0; lane < kNumTracks; ++lane) {
      bool on = ((saved >> lane) & 1) != 0;
      if (on != beatsOn[lane]) {
        beatsOn[lane] = on;
        beatMask |= (1 << lane);
      }
    }
  }
  for (int lane = 0; lane < kNumTracks; ++lane) {
    if (!(beatMask & (1 << lane)) || dragTrack == lane) continue;
    composeLane(lane);
    if (visible) {
      presentLane(lane);
      redrew = true;
    }
  }
  if (redrew && visible) drawPlayhead();

  if (visible) {
    if (dragTrack >= 0 && now - lastDragDrawMs >= 30) {
      lastDragDrawMs = now;
      flushDirtyLanes();
    }
    if (dragTrack >= 0 && now - lastCropSendMs >= kCropSendIntervalMs) {
      lastCropSendMs = now;
      sendDragCrop(dragTrack, dragClip);
    }
    int x = msToX(g.posMs);
    if (x != playheadX) {
      erasePlayhead(playheadX);
      playheadX = x;
      drawPlayhead();
    }
  }
}

}  // namespace playlist
