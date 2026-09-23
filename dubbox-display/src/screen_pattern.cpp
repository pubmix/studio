#include "screen_pattern.h"
#include <Arduino.h>
#include <stdio.h>
#include "keyboard.h"
#include "synth_panel.h"
#include "modular_panel.h"
#include "teensy_link.h"
#include "ui_common.h"

namespace pattern {
namespace {

using namespace ui;

constexpr int kRows = teensylink::kDrumRows;
constexpr int kSteps = teensylink::kPatSteps;
constexpr int kNumPatterns = teensylink::kNumPatterns;
constexpr int kMaxInst = teensylink::kMaxInstruments;

// Controls row: pattern chips, DRUMS and instrument tabs, "+", tempo.
constexpr int kControlsTop = 108;
constexpr int kControlsBottom = 150;
constexpr Rect kDrumsTab = {434, kControlsTop, 522, kControlsBottom};
constexpr int kInstTabX = 526;
constexpr int kInstTabStride = 92;
constexpr Rect kBpmMinus = {1014, kControlsTop, 1056, kControlsBottom};
constexpr Rect kBpmValue = {1060, kControlsTop, 1188, kControlsBottom};
constexpr Rect kBpmPlus = {1192, kControlsTop, 1234, kControlsBottom};

// Inside an instrument: a bar of buttons under the controls row picks SOUND / ROLL / KEYS.
constexpr int kSubTop = 156;
constexpr int kSubBottom = 196;
constexpr Rect kModeSound = {16, kSubTop, 116, kSubBottom};
constexpr Rect kModeRoll = {122, kSubTop, 222, kSubBottom};
constexpr Rect kModeKeys = {228, kSubTop, 328, kSubBottom};
constexpr Rect kOctDown = {340, kSubTop, 380, kSubBottom};
constexpr Rect kOctLabel = {384, kSubTop, 440, kSubBottom};
constexpr Rect kOctUp = {444, kSubTop, 484, kSubBottom};
constexpr Rect kClearBtn = {500, kSubTop, 590, kSubBottom};
constexpr Rect kDeleteBtn = {1130, kSubTop, 1250, kSubBottom};
constexpr uint32_t kDeleteConfirmMs = 2500;

// Body: drum grid (8 rows), or, inside an instrument, its sound panel, piano roll or keyboard.
constexpr int kLabelX1 = 16;
constexpr int kLabelX2 = 126;    // label text box; the pad badge sits between here and the grid
constexpr int kBadgeX1 = 130;
constexpr int kBadgeX2 = 152;
constexpr int kGridX = 160;
constexpr int kCellW = 68;
constexpr int kGridY = 160;       // top of the drum grid
constexpr int kInstTop = 204;     // top of an instrument's roll / keyboard / sound panel
constexpr int kBodyClearTop = 152;
constexpr int kBodyBottom = 640;
constexpr int kDrumRowH = 58;
constexpr int kRollRows = 12;
constexpr int kRollRowH = 36;

// Keyboard view: 5-14 white keys, with a key-count control and the sustain pedal on the left.
constexpr int kKeyCountMin = 5;
constexpr int kKeyCountMax = 14;
constexpr keyboard::Layout kKeysLayout = {160, 210, 630, 1088, 260};
constexpr Rect kKeyCountBox = {16, 210, 150, 262};
constexpr Rect kKeyMinus = {16, 268, 80, 318};
constexpr Rect kKeyPlus = {86, 268, 150, 318};
constexpr Rect kPedal = {16, 340, 150, 630};
constexpr uint32_t kReleaseDebounceMs = 70;  // the touch chip drops a finger for a frame or two now and then
constexpr uint32_t kKeepAliveMs = 150;

const char* const kRowNames[kRows] = {"KICK", "SNARE", "CLAP", "CL HAT", "OP HAT", "TOM", "RIM", "PERC"};
constexpr uint32_t kRowRgb[kRows] = {0xff5252, 0xffa726, 0xffee58, 0x66bb6a,
                                     0x26c6da, 0x42a5f5, 0xab47bc, 0xec407a};
constexpr uint32_t kPatternRgb[kNumPatterns] = {0xb45cff, 0xff6b6b, 0xffb74d, 0x81c784,
                                                0x4dd0e1, 0x64b5f6, 0xf06292, 0xdce775};
constexpr uint32_t kNoteRgb = 0xb45cff;
const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

constexpr uint32_t kBpmHoldMs = 400;
constexpr uint32_t kBpmRepeatMs = 80;

enum class View { Drums, Inst };
enum class Mode { Sound, Roll, Keys };
bool choosingInstrument = false;
constexpr Rect chooseSynth={270,290,570,370},chooseModular={620,290,970,370},chooseCancel={470,410,770,465};
View view = View::Drums;
Mode mode = Mode::Sound;
int curInst = 0;       // the instrument being viewed
int octaveBase = 60;   // MIDI note of the roll's bottom row / the keyboards' first key (a C)
constexpr int kOctaveMin = 36;
constexpr int kOctaveMax = 84;

bool visibleNow = false;
int lastBpmShown = -1;
uint16_t shownRows[kRows];   // drum rows as last drawn (so remote changes redraw only the cells that differ)
int chipShown[kNumPatterns];  // what each chip was last drawn as (0 = never drawn, else 1 + selected*2 + empty)
int routeRow = -1;            // drum row whose label is being held (pad routing), or -1
int routeNote = -1;           // piano key whose label is being held (pad routing), or -1
uint32_t lastRouteSendMs = 0;
int shownStep = -1;  // column currently highlighted as playing
uint32_t deleteArmedUntil = 0;  // DELETE was tapped once; a second tap before this deletes the instrument

keyboard::Keyboard kb;  // the full-width keyboard (KEYS mode)
bool pedalDown = false;
uint32_t pedalSeenMs = 0;
uint32_t pedalKeepAliveMs = 0;

enum class Touch { None, Paint, Bpm, Note, Route, Panel };
Touch touchMode = Touch::None;
bool paintValue = true;
int lastPaintRow = -1;
int lastPaintCol = -1;
int noteMidi = 0;   // the note being drawn by dragging
int noteStart = 0;
int noteLen = 1;
int bpmDir = 0;
uint32_t bpmPressMs = 0;
uint32_t bpmLastRepeatMs = 0;

Rect chipRect(int i) { return {16 + i * 52, kControlsTop, 16 + i * 52 + 48, kControlsBottom}; }
Rect instTabRect(int i) {
  const int x = kInstTabX + i * kInstTabStride;
  return {x, kControlsTop, x + 88, kControlsBottom};
}
Rect addRect(int count) {
  const int x = kInstTabX + count * kInstTabStride;
  return {x, kControlsTop, x + 40, kControlsBottom};
}

const teensylink::Drums& drums() { return teensylink::state().drums; }
int instCount() { return drums().instCount; }
bool modularInstrument() { return teensylink::state().instrumentModular[curInst]; }

bool patternEmpty(int p) {
  const teensylink::Drums& d = drums();
  for (int r = 0; r < kRows; ++r) {
    if (d.rows[p][r] != 0) return false;
  }
  for (int i = 0; i < d.instCount; ++i) {
    if (d.noteCount[p][i] != 0) return false;
  }
  return true;
}

// The column to show as playing: the loop preview's step, or, while the song plays, the step of
// this pattern's clip under the playhead.
int currentStep() {
  const teensylink::Drums& d = drums();
  if (d.previewOn) return (d.previewPattern == g.pattern && !d.countIn) ? d.previewStep : -1;
  if (!g.playing || d.bpm <= 0) return -1;
  uint32_t absStep = (uint32_t)(g.posMs / (15000.0f / d.bpm));
  uint32_t bar = absStep / kSteps;
  for (int i = 0; i < d.clipCount; ++i) {
    const teensylink::PatClip& c = d.clips[i];
    if (c.pattern == g.pattern && bar >= c.startBar && bar < (uint32_t)c.startBar + c.lenBars) {
      return (int)(absStep % kSteps);
    }
  }
  return -1;
}

// ---- Drum grid ----

Rect drumCellRect(int r, int c) {
  int x = kGridX + c * kCellW;
  int y = kGridY + r * kDrumRowH;
  return {x, y, x + kCellW - 4, y + kDrumRowH - 4};
}

bool drumCellAt(int x, int y, int& r, int& c) {
  if (x < kGridX || y < kGridY) return false;
  c = (x - kGridX) / kCellW;
  r = (y - kGridY) / kDrumRowH;
  return c >= 0 && c < kSteps && r >= 0 && r < kRows;
}

bool stepOn(int r, int c) { return (drums().rows[g.pattern][r] >> c) & 1; }

void drawDrumCell(int r, int c) {
  bool on = stepOn(r, c);
  bool playing = c == shownStep;
  uint32_t rgb;
  if (on) {
    rgb = playing ? 0xffffff : kRowRgb[r];
  } else {
    rgb = playing ? 0x60606c : (((c / 4) & 1) ? 0x33333c : 0x26262e);
  }
  Rect rc = drumCellRect(r, c);
  fillRect(rc.x1, rc.y1, rc.x2, rc.y2, rgb565(rgb));
}

// The lowest-numbered pad routed to a drum row (0-3), or -1.
int padOfRow(int r) {
  for (int p = 0; p < 4; ++p) {
    if (drums().padRoute[p] == r) return p;
  }
  return -1;
}

// The lowest-numbered pad that plays a note (0-3), or -1.
int padOfNote(int midi) {
  for (int p = 0; p < 4; ++p) {
    if (drums().padNote[p] == midi) return p;
  }
  return -1;
}

void drawBadge(const Rect& badge, int pad, uint32_t rgb) {
  if (pad >= 0) {
    char n[4];
    snprintf(n, sizeof(n), "%d", pad + 1);
    drawButton(badge, n, fontSmall(), rgb565(0x000000), rgb565(rgb));
  } else {
    fillRect(badge.x1, badge.y1, badge.x2, badge.y2, rgb565(0x18181c));
  }
}

// A drum label (touch and hold it, then press a pad, to route that sound to the pad) and the
// number of the pad that plays it.
void drawDrumLabel(int r) {
  const int y = kGridY + r * kDrumRowH;
  const bool held = routeRow == r;
  Rect lab = {kLabelX1, y, kLabelX2, y + kDrumRowH - 4};
  if (held) {
    drawButton(lab, "PRESS PAD", fontSmall(), rgb565(0x000000), rgb565(0xffffff));
  } else {
    drawButton(lab, kRowNames[r], fontSmall(), rgb565(0xffffff), rgb565(kRowRgb[r], 0.5f));
  }
  drawBadge({kBadgeX1, y, kBadgeX2, y + kDrumRowH - 4}, padOfRow(r), kRowRgb[r]);
}

void drawDrumGrid() {
  setCanvas(kVisible);
  for (int r = 0; r < kRows; ++r) {
    drawDrumLabel(r);
    for (int c = 0; c < kSteps; ++c) drawDrumCell(r, c);
    shownRows[r] = drums().rows[g.pattern][r];
  }
}

// Redraws only the drum cells that differ from what is on screen (hits arriving while recording).
void syncDrumCells() {
  setCanvas(kVisible);
  for (int r = 0; r < kRows; ++r) {
    uint16_t now = drums().rows[g.pattern][r];
    uint16_t diff = now ^ shownRows[r];
    if (diff == 0) continue;
    shownRows[r] = now;
    for (int c = 0; c < kSteps; ++c) {
      if ((diff >> c) & 1) drawDrumCell(r, c);
    }
  }
}

bool drumLabelAt(int x, int y, int& r) {
  if (x < kLabelX1 || x > kBadgeX2 || y < kGridY) return false;
  r = (y - kGridY) / kDrumRowH;
  return r >= 0 && r < kRows;
}

// ---- Piano roll (the current instrument's notes in this pattern) ----

int rollMidi(int r) { return octaveBase + (kRollRows - 1) - r; }
bool isBlackKey(int midi) {
  int n = midi % 12;
  return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

Rect rollCellRect(int r, int c) {
  int x = kGridX + c * kCellW;
  int y = kInstTop + r * kRollRowH;
  return {x, y, x + kCellW - 4, y + kRollRowH - 4};
}

bool rollCellAt(int x, int y, int& r, int& c) {
  if (x < kGridX || y < kInstTop) return false;
  c = (x - kGridX) / kCellW;
  r = (y - kInstTop) / kRollRowH;
  return c >= 0 && c < kSteps && r >= 0 && r < kRollRows;
}

bool rollLabelAt(int x, int y, int& r) {
  if (x < kLabelX1 || x > kBadgeX2 || y < kInstTop) return false;
  r = (y - kInstTop) / kRollRowH;
  return r >= 0 && r < kRollRows;
}

// The note of this instrument at `midi` that covers `step` in the current pattern, if any.
bool noteCovering(int midi, int step, teensylink::PatNote& out) {
  const teensylink::Drums& d = drums();
  for (int i = 0; i < d.noteCount[g.pattern][curInst]; ++i) {
    const teensylink::PatNote& n = d.notes[g.pattern][curInst][i];
    if (n.midi == midi && step >= n.start && step < n.start + n.len) {
      out = n;
      return true;
    }
  }
  return false;
}

void drawRollCell(int r, int c) {
  const int midi = rollMidi(r);
  const bool playing = c == shownStep;
  teensylink::PatNote note;
  const bool hasNote = noteCovering(midi, c, note);
  Rect rc = rollCellRect(r, c);
  fillRect(rc.x1, rc.y1, rc.x2 + 4, rc.y2, rgb565(0x000000));  // clears the gap a note bar may have covered
  uint32_t rgb;
  if (hasNote) {
    rgb = playing ? 0xffffff : kNoteRgb;
  } else if (isBlackKey(midi)) {
    rgb = playing ? 0x4a4a55 : (((c / 4) & 1) ? 0x26262e : 0x1c1c23);
  } else {
    rgb = playing ? 0x5a5a66 : (((c / 4) & 1) ? 0x34343e : 0x2a2a33);
  }
  int x2 = rc.x2;
  if (hasNote && c + 1 < note.start + note.len) x2 += 4;  // joins the next cell of the same note
  fillRect(rc.x1, rc.y1, x2, rc.y2, rgb565(rgb));
  if (hasNote && c == note.start) {  // a lighter edge marks where the note begins
    fillRect(rc.x1, rc.y1, rc.x1 + 3, rc.y2, rgb565(playing ? 0xb45cff : 0xe0c4ff));
  }
}

void drawRollRow(int r) {
  for (int c = 0; c < kSteps; ++c) drawRollCell(r, c);
}

// A piano-key label (touch and hold it, then press a pad, to route that note to the pad) and the
// number of the pad that plays it.
void drawRollLabel(int r) {
  const int midi = rollMidi(r);
  const int y = kInstTop + r * kRollRowH;
  const bool black = isBlackKey(midi);
  Rect lab = {kLabelX1, y, kLabelX2, y + kRollRowH - 4};
  if (routeNote == midi) {
    drawButton(lab, "PRESS PAD", fontSmall(), rgb565(0x000000), rgb565(0xffffff));
  } else {
    char label[8];
    snprintf(label, sizeof(label), "%s%d", kNoteNames[midi % 12], midi / 12 - 1);
    drawButton(lab, label, fontSmall(), rgb565(black ? 0xffffff : 0x000000), rgb565(black ? 0x202028 : 0xdcdcdc));
  }
  drawBadge({kBadgeX1, y, kBadgeX2, y + kRollRowH - 4}, padOfNote(midi), kNoteRgb);
}

void drawRoll() {
  setCanvas(kVisible);
  for (int r = 0; r < kRollRows; ++r) {
    drawRollLabel(r);
    drawRollRow(r);
  }
}

// ---- Keyboard view: key count and sustain pedal beside the keys ----

// The sustain pedal: a frame that lights up while it is held (the label is drawn once).
void drawPedalFrame() {
  const uint16_t col = rgb565(pedalDown ? 0x4dd0e1 : 0x3a4650);
  const int t = pedalDown ? 10 : 4;
  fillRect(kPedal.x1, kPedal.y1, kPedal.x2, kPedal.y1 + t - 1, col);
  fillRect(kPedal.x1, kPedal.y2 - t + 1, kPedal.x2, kPedal.y2, col);
  fillRect(kPedal.x1, kPedal.y1, kPedal.x1 + t - 1, kPedal.y2, col);
  fillRect(kPedal.x2 - t + 1, kPedal.y1, kPedal.x2, kPedal.y2, col);
  if (!pedalDown) {  // erase the thicker lit frame's inner edge
    fillRect(kPedal.x1 + t, kPedal.y1 + t, kPedal.x2 - t, kPedal.y1 + 9, rgb565(0x232b33));
    fillRect(kPedal.x1 + t, kPedal.y2 - 9, kPedal.x2 - t, kPedal.y2 - t, rgb565(0x232b33));
    fillRect(kPedal.x1 + t, kPedal.y1 + t, kPedal.x1 + 9, kPedal.y2 - t, rgb565(0x232b33));
    fillRect(kPedal.x2 - 9, kPedal.y1 + t, kPedal.x2 - t, kPedal.y2 - t, rgb565(0x232b33));
  }
}

void drawKeysSide() {
  setCanvas(kVisible);
  char lab[12];
  snprintf(lab, sizeof(lab), "%d KEYS", kb.keyCount);
  drawButton(kKeyCountBox, lab, fontSmall(), rgb565(0xffffff), rgb565(0x22222b));
  drawButton(kKeyMinus, "-", fontLarge(), rgb565(0xffffff), rgb565(0x3a3a48));
  drawButton(kKeyPlus, "+", fontLarge(), rgb565(0xffffff), rgb565(0x3a3a48));
  drawButton(kPedal, "SUSTAIN", fontSmall(), rgb565(0xffffff), rgb565(0x232b33));
  drawPedalFrame();
}

// ---- Whole-screen drawing ----

// Draws the pattern chips. Only chips whose look changed since last drawn are redrawn (unless
// `all`), because chip text is slow to draw and a visible flicker on every tap.
void drawChips(bool all = true) {
  setCanvas(kVisible);
  for (int i = 0; i < kNumPatterns; ++i) {
    const bool selected = i == g.pattern;
    const bool empty = patternEmpty(i);
    const int state = 1 + (selected ? 2 : 0) + (empty ? 1 : 0);
    if (!all && chipShown[i] == state) continue;
    chipShown[i] = state;
    char label[8];
    snprintf(label, sizeof(label), "P%d", i + 1);
    uint32_t fill = selected ? kPatternRgb[i] : (empty ? 0x2c2c33 : 0x3f3f4d);
    uint32_t fg = selected ? 0x000000 : (empty ? 0xaaaaaa : 0xffffff);
    Rect r = chipRect(i);
    drawButton(r, label, fontSmall(), rgb565(fg), rgb565(fill));
    if (!selected && !empty) {  // colour tab under a pattern that has content
      fillRect(r.x1, r.y2 - 4, r.x2, r.y2, rgb565(kPatternRgb[i]));
    }
  }
}

void refreshChips() { drawChips(false); }

void drawBpm() {
  setCanvas(kVisible);
  lastBpmShown = drums().bpm;
  char label[16];
  snprintf(label, sizeof(label), "%d BPM", drums().bpm);
  drawButton(kBpmMinus, "-", fontLarge(), rgb565(0xffffff), rgb565(0x3a3a48));
  drawButton(kBpmValue, label, fontSmall(), rgb565(0xffffff), rgb565(0x22222b));
  drawButton(kBpmPlus, "+", fontLarge(), rgb565(0xffffff), rgb565(0x3a3a48));
}

// DRUMS and instrument tabs and the "+" that adds an instrument.
void drawTools() {
  setCanvas(kVisible);
  fillRect(kDrumsTab.x1, kControlsTop, 1008, kControlsBottom, rgb565(0x000000));
  const bool drumsView = view == View::Drums;
  drawButton(kDrumsTab, "DRUMS", fontSmall(), rgb565(drumsView ? 0x000000 : 0xffffff),
             rgb565(drumsView ? 0xff9f3d : 0x5a3a14));
  const int n = instCount();
  for (int i = 0; i < n; ++i) {
    char lab[8];
    snprintf(lab, sizeof(lab), "%s %d", teensylink::state().instrumentModular[i] ? "MOD" : "SYN", i + 1);
    const bool on = view == View::Inst && i == curInst;
    drawButton(instTabRect(i), lab, fontSmall(), rgb565(on ? 0x000000 : 0xffffff), rgb565(on ? kNoteRgb : 0x4a2a7a));
  }
  if (n < kMaxInst) drawButton(addRect(n), "+", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3d));
}

// The bar under the controls row while an instrument is open: SOUND / ROLL / KEYS, the octave,
// CLEAR (roll only) and DELETE (which asks to be tapped twice).
void drawSubBar() {
  setCanvas(kVisible);
  fillRect(0, kSubTop - 2, kW - 1, kSubBottom + 2, rgb565(0x000000));
  struct {
    Rect r;
    const char* label;
    Mode m;
  } modes[3] = {{kModeSound, modularInstrument() ? "PATCH" : "SOUND", Mode::Sound}, {kModeRoll, "ROLL", Mode::Roll}, {kModeKeys, "KEYS", Mode::Keys}};
  for (const auto& b : modes) {
    const bool on = mode == b.m;
    drawButton(b.r, b.label, fontSmall(), rgb565(on ? 0x000000 : 0xffffff), rgb565(on ? kNoteRgb : 0x4a2a7a));
  }
  char oct[8];
  snprintf(oct, sizeof(oct), "C%d", octaveBase / 12 - 1);
  drawButton(kOctDown, "-", fontLarge(), rgb565(0xffffff), rgb565(0x3a3a48));
  drawButton(kOctLabel, oct, fontSmall(), rgb565(0xffffff), rgb565(0x22222b));
  drawButton(kOctUp, "+", fontLarge(), rgb565(0xffffff), rgb565(0x3a3a48));
  if (mode == Mode::Roll) drawButton(kClearBtn, "CLEAR", fontSmall(), rgb565(0xffffff), rgb565(0x7a2a2a));
  const bool armed = millis() < deleteArmedUntil;
  drawButton(kDeleteBtn, armed ? "SURE?" : "DELETE", fontSmall(), rgb565(0xffffff), rgb565(armed ? 0xff2b2b : 0x9c2a2a));
}

// `clear` blanks the body first (needed when the view changes; skipped for in-place refreshes
// so the screen does not flash).
void drawBody(bool clear = true) {
  setCanvas(kVisible);
  if (clear) fillRect(0, kBodyClearTop, kW - 1, kBodyBottom, rgb565(0x000000));
  if (choosingInstrument) {
    drawButton({260,210,990,260},"ADD AN INSTRUMENT",fontLarge(),rgb565(0xffffff),rgb565(0x000000));
    drawButton(chooseSynth,"SYNTH",fontLarge(),rgb565(0xffffff),rgb565(0x4a2a7a));
    drawButton(chooseModular,"MODULAR",fontLarge(),rgb565(0xffffff),rgb565(0x1f737c));
    drawButton(chooseCancel,"CANCEL",fontSmall(),rgb565(0xffffff),rgb565(0x333344));
    return;
  }
  if (view == View::Drums) {
    drawDrumGrid();
    return;
  }
  drawSubBar();
  switch (mode) {
    case Mode::Sound:
      if (modularInstrument()) modularpanel::draw(curInst);
      else synthpanel::draw(curInst, octaveBase);
      break;
    case Mode::Roll:
      drawRoll();
      break;
    case Mode::Keys:
      keyboard::draw(kb);
      drawKeysSide();
      break;
  }
}

void drawColumn(int c) {
  if (c < 0 || (view == View::Inst && mode != Mode::Roll)) return;
  setCanvas(kVisible);
  if (view == View::Drums) {
    for (int r = 0; r < kRows; ++r) drawDrumCell(r, c);
  } else {
    for (int r = 0; r < kRollRows; ++r) drawRollCell(r, c);
  }
}

void drawAll() {
  drawChips();
  drawTools();
  drawBpm();
  drawBody();
}

// An instrument view needs the instrument to exist.
void fixView() {
  if (view != View::Inst) return;
  const int n = instCount();
  if (n == 0) {
    view = View::Drums;
  } else if (curInst >= n) {
    curInst = n - 1;
  }
}

// Lets go of any keyboard notes that are down, and the sustain pedal.
void releaseKeys() {
  modularpanel::resetSelection();
  keyboard::releaseAll(kb);
  synthpanel::releaseKeys();
  if (pedalDown) {
    pedalDown = false;
    if (g.linked) teensylink::sendSustain(false);
    if (visibleNow && view == View::Inst && mode == Mode::Keys) {
      setCanvas(kVisible);
      drawPedalFrame();
    }
  }
}

// The pads follow what is on screen: drums in the DRUMS view, the open instrument's notes otherwise
// (the pad-mode button in the bottom row can still override that).
void syncPads() {
  if (g.linked) teensylink::setPadMode(view == View::Drums ? 0 : 1, view == View::Drums ? -1 : curInst);
}

void setView(View v, int inst) {
  if (view == v && (v == View::Drums || inst == curInst)) return;
  view = v;
  if (v == View::Inst) {
    curInst = inst;
    kb.inst = inst;
  }
  shownStep = -1;
  deleteArmedUntil = 0;
  releaseKeys();
  syncPads();
  drawTools();
  drawBody();
}

void setMode(Mode m) {
  if (mode == m) return;
  mode = m;
  shownStep = -1;
  releaseKeys();
  drawBody();
}

void selectPattern(int i) {
  if (i == g.pattern) return;
  g.pattern = i;
  shownStep = -1;
  if (drums().previewOn && g.linked) teensylink::setPreview(true, i);
  drawAll();
}

void setCell(int r, int c, bool on) {
  if (stepOn(r, c) == on) return;
  if (g.linked) teensylink::editStep(g.pattern, r, c, on);
  setCanvas(kVisible);
  drawDrumCell(r, c);
  shownRows[r] = drums().rows[g.pattern][r];
  refreshChips();  // a pattern may have just become empty / non-empty
}

void nudgeBpm(int delta) {
  int bpm = drums().bpm + delta;
  if (g.linked) teensylink::setBpm(bpm);
  drawBpm();
}

void changeOctave(int delta) {
  int base = octaveBase + delta * 12;
  if (base < kOctaveMin || base > kOctaveMax) return;
  octaveBase = base;
  kb.octaveBase = base;
  releaseKeys();
  drawSubBar();
  switch (mode) {
    case Mode::Sound:
      if (!modularInstrument()) synthpanel::setOctave(base);
      break;
    case Mode::Roll:
      drawRoll();
      break;
    case Mode::Keys:
      keyboard::draw(kb);
      break;
  }
}

// More keys = smaller keys; fewer = bigger and easier to hit.
void changeKeyCount(int delta) {
  int n = kb.keyCount + delta;
  if (n < kKeyCountMin || n > kKeyCountMax) return;
  releaseKeys();
  keyboard::setKeyCount(kb, n);
  drawBody();
}

// Length of the note now at (noteMidi, noteStart), as the link resolved it.
int currentNoteLen() {
  const teensylink::Drums& d = drums();
  for (int i = 0; i < d.noteCount[g.pattern][curInst]; ++i) {
    const teensylink::PatNote& n = d.notes[g.pattern][curInst][i];
    if (n.midi == noteMidi && n.start == noteStart) return n.len;
  }
  return 1;
}

void rollTouchDown(int r, int c) {
  const int midi = rollMidi(r);
  teensylink::PatNote existing;
  if (noteCovering(midi, c, existing)) {  // tapping a note deletes it
    if (g.linked) teensylink::removeNote(g.pattern, curInst, midi, existing.start);
    drawRollRow(r);
    refreshChips();
    return;
  }
  // Otherwise start a new note here; dragging sideways stretches it.
  noteMidi = midi;
  noteStart = c;
  noteLen = 1;
  touchMode = Touch::Note;
  if (g.linked) {
    teensylink::setNote(g.pattern, curInst, midi, c, 1);
    teensylink::auditionNote(curInst, midi);
  }
  drawRollRow(r);
  refreshChips();
}

// Deletes the open instrument after a second tap (the first one just asks).
void tapDelete() {
  if (millis() >= deleteArmedUntil) {
    deleteArmedUntil = millis() + kDeleteConfirmMs;
    drawSubBar();
    return;
  }
  deleteArmedUntil = 0;
  releaseKeys();
  if (g.linked) teensylink::removeInstrument(curInst);
  if (instCount() == 0) {
    view = View::Drums;
  } else if (curInst >= instCount()) {
    curInst = instCount() - 1;
  }
  kb.inst = curInst;
  shownStep = -1;
  syncPads();
  drawChips();
  drawTools();
  drawBody();
}

}  // namespace

unsigned patternRgb(int index) { return kPatternRgb[index & 7]; }

static void endRouteHold();

void enter() {
  visibleNow = true;
  choosingInstrument = false;
  modularpanel::resetSelection();
  for (int i = 0; i < kNumPatterns; ++i) chipShown[i] = 0;
  shownStep = -1;
  touchMode = Touch::None;
  kb.layout = kKeysLayout;
  kb.octaveBase = octaveBase;
  kb.heldCount = 0;
  fixView();
  kb.inst = curInst;
  syncPads();
  drawAll();
}

void leave() {
  endRouteHold();
  releaseKeys();
  visibleNow = false;
  if (drums().previewOn && g.linked) teensylink::setPreview(false, g.pattern);
}

void touchDown(int x, int y) {
  touchMode = Touch::None;
  bpmDir = 0;
  if (choosingInstrument) {
    if (inRect(chooseCancel,x,y)) {choosingInstrument=false;drawBody();return;}
    if (inRect(chooseSynth,x,y)||inRect(chooseModular,x,y)) {
      int before=instCount();
      if(g.linked) teensylink::addInstrument(inRect(chooseModular,x,y));
      choosingInstrument=false;
      if(instCount()>before){mode=Mode::Sound;view=View::Drums;setView(View::Inst,instCount()-1);}
      else drawBody();
    }
    return;
  }
  for (int i = 0; i < kNumPatterns; ++i) {
    if (inRect(chipRect(i), x, y)) {
      selectPattern(i);
      return;
    }
  }
  if (inRect(kDrumsTab, x, y)) {
    setView(View::Drums, 0);
    return;
  }
  const int n = instCount();
  for (int i = 0; i < n; ++i) {
    if (inRect(instTabRect(i), x, y)) {
      setView(View::Inst, i);
      return;
    }
  }
  if (n < kMaxInst && inRect(addRect(n), x, y)) {  // a new instrument, opened on its sound
    releaseKeys();
    choosingInstrument=true;
    drawBody();
    return;
  }
  if (inRect(kBpmMinus, x, y) || inRect(kBpmPlus, x, y)) {
    bpmDir = inRect(kBpmMinus, x, y) ? -1 : 1;
    touchMode = Touch::Bpm;
    bpmPressMs = bpmLastRepeatMs = millis();
    nudgeBpm(bpmDir);
    return;
  }
  int r, c;
  if (view == View::Drums) {
    if (drumLabelAt(x, y, r)) {
      routeRow = r;  // hold this sound, then press a pad to route it there
      touchMode = Touch::Route;
      lastRouteSendMs = millis();
      if (g.linked) teensylink::sendRouteHold(r);
      setCanvas(kVisible);
      drawDrumLabel(r);
      return;
    }
    if (drumCellAt(x, y, r, c)) {
      touchMode = Touch::Paint;
      paintValue = !stepOn(r, c);  // the first cell decides whether dragging paints or erases
      lastPaintRow = r;
      lastPaintCol = c;
      setCell(r, c, paintValue);
    }
    return;
  }

  // ---- Inside an instrument ----
  if (y >= kSubTop - 2 && y <= kSubBottom + 2) {
    if (inRect(kModeSound, x, y)) {
      setMode(Mode::Sound);
    } else if (inRect(kModeRoll, x, y)) {
      setMode(Mode::Roll);
    } else if (inRect(kModeKeys, x, y)) {
      setMode(Mode::Keys);
    } else if (inRect(kOctDown, x, y)) {
      changeOctave(-1);
    } else if (inRect(kOctUp, x, y)) {
      changeOctave(1);
    } else if (mode == Mode::Roll && inRect(kClearBtn, x, y)) {
      if (g.linked) teensylink::clearNotes(g.pattern, curInst);
      drawRoll();
      refreshChips();
    } else if (inRect(kDeleteBtn, x, y)) {
      tapDelete();
    }
    return;
  }
  switch (mode) {
    case Mode::Sound:
      if (modularInstrument()) {modularpanel::touchDown(curInst,x,y);break;}
      if (synthpanel::touchDown(curInst, x, y)) touchMode = Touch::Panel;
      break;
    case Mode::Roll:
      if (rollLabelAt(x, y, r)) {
        routeNote = rollMidi(r);  // hold this key, then press a pad to route the note there
        touchMode = Touch::Route;
        lastRouteSendMs = millis();
        if (g.linked) teensylink::sendNoteHold(routeNote);
        setCanvas(kVisible);
        drawRollLabel(r);
      } else if (rollCellAt(x, y, r, c)) {
        rollTouchDown(r, c);
      }
      break;
    case Mode::Keys:
      // (The keys themselves are handled by touchPoints(), so chords work.)
      if (inRect(kKeyMinus, x, y)) {
        changeKeyCount(-1);
      } else if (inRect(kKeyPlus, x, y)) {
        changeKeyCount(1);
      }
      break;
  }
}

void touchMove(int x, int y) {
  if (touchMode == Touch::Paint) {
    int r, c;
    if (!drumCellAt(x, y, r, c) || (r == lastPaintRow && c == lastPaintCol)) return;
    lastPaintRow = r;
    lastPaintCol = c;
    setCell(r, c, paintValue);
  } else if (touchMode == Touch::Note) {
    int col = constrain((x - kGridX) / kCellW, 0, kSteps - 1);
    int len = max(1, col - noteStart + 1);
    if (len == noteLen) return;
    noteLen = len;
    if (g.linked) teensylink::setNote(g.pattern, curInst, noteMidi, noteStart, len);
    noteLen = currentNoteLen();  // the link shortens it if it would run into the next note
    setCanvas(kVisible);
    drawRollRow(kRollRows - 1 - (noteMidi - octaveBase));
  } else if (touchMode == Touch::Panel) {
    synthpanel::touchMove(x);
  }
}

// Ends a pad-routing hold: tells the Teensy and redraws the label.
static void endRouteHold() {
  if (routeRow >= 0) {
    int r = routeRow;
    routeRow = -1;
    if (g.linked) teensylink::sendRouteHold(-1);
    if (visibleNow && view == View::Drums) {
      setCanvas(kVisible);
      drawDrumLabel(r);
    }
  }
  if (routeNote >= 0) {
    int midi = routeNote;
    routeNote = -1;
    if (g.linked) teensylink::sendNoteHold(-1);
    if (visibleNow && view == View::Inst && mode == Mode::Roll) {
      int r = kRollRows - 1 - (midi - octaveBase);
      if (r >= 0 && r < kRollRows) {
        setCanvas(kVisible);
        drawRollLabel(r);
      }
    }
  }
}

void touchUp() {
  endRouteHold();
  if (touchMode == Touch::Panel) synthpanel::touchUp(curInst);
  touchMode = Touch::None;
  bpmDir = 0;
}

// Every finger on the screen, once per loop: plays (and releases) keyboard notes and the pedal.
// The pedal follows a finger on it with a short release delay (the touch chip briefly loses
// fingers), and while it is down the Teensy is told every kKeepAliveMs so it can release a pedal
// that stops being refreshed.
void touchPoints(const int* xs, const int* ys, int n) {
  const bool inInst = visibleNow && !choosingInstrument && view == View::Inst;
  const bool keysActive = inInst && mode == Mode::Keys;
  const bool soundActive = inInst && mode == Mode::Sound && !modularInstrument();
  if (kb.inst != curInst) {
    keyboard::releaseAll(kb);
    kb.inst = curInst;
  }
  keyboard::touchPoints(kb, xs, ys, n, keysActive);
  synthpanel::touchPoints(curInst, xs, ys, n, soundActive);
  if (!keysActive) {
    if (pedalDown) releaseKeys();
    return;
  }
  const uint32_t now = millis();
  bool pedalNow = false;
  for (int i = 0; i < n && i < 5; ++i) {
    if (inRect(kPedal, xs[i], ys[i])) pedalNow = true;
  }
  if (pedalNow) {
    pedalSeenMs = now;
    if (!pedalDown) {
      pedalDown = true;
      if (g.linked) teensylink::sendSustain(true);
      setCanvas(kVisible);
      drawPedalFrame();
    }
  } else if (pedalDown && now - pedalSeenMs > kReleaseDebounceMs) {
    pedalDown = false;
    if (g.linked) teensylink::sendSustain(false);
    setCanvas(kVisible);
    drawPedalFrame();
  }
  if (pedalDown && now - pedalKeepAliveMs >= kKeepAliveMs) {
    pedalKeepAliveMs = now;
    if (g.linked) teensylink::sendSustain(true);
  }
}

void update(bool visible) {
  visibleNow = visible;
  uint8_t changed = teensylink::takePatternsChanged();  // changes reported by the Teensy, not our own edits
  bool rhythm = teensylink::takeRhythmChanged();
  bool padsChanged = teensylink::takePadRouteChanged();
  bool instsChanged = teensylink::takeInstrumentsChanged();
  bool synthChanged = teensylink::takeSynthChanged();
  if (!visible || choosingInstrument) return;

  if (instsChanged) {  // instruments were added / removed (e.g. a project opened)
    fixView();
    drawChips();
    drawTools();
    drawBody();
  } else {
    if (changed) {
      refreshChips();
      if (changed & (1 << g.pattern)) {
        if (view == View::Drums) {
          syncDrumCells();
        } else if (mode == Mode::Roll) {
          drawRoll();
        }
      }
    }
    if (synthChanged && view == View::Inst && mode == Mode::Sound) {
      if(modularInstrument()) modularpanel::draw(curInst); else synthpanel::refresh(curInst);
    }
  }
  if (rhythm && drums().bpm != lastBpmShown) drawBpm();  // (the preview step also arrives here, often)
  if (padsChanged) {
    setCanvas(kVisible);
    if (view == View::Drums) {
      for (int r = 0; r < kRows; ++r) drawDrumLabel(r);
    } else if (mode == Mode::Roll) {
      for (int r = 0; r < kRollRows; ++r) drawRollLabel(r);
    }
  }
  if (touchMode == Touch::Route && (routeRow >= 0 || routeNote >= 0) && g.linked) {
    uint32_t now = millis();
    if (now - lastRouteSendMs >= 300) {  // the Teensy drops the hold after ~1 s without a refresh
      lastRouteSendMs = now;
      if (routeRow >= 0) teensylink::sendRouteHold(routeRow);
      if (routeNote >= 0) teensylink::sendNoteHold(routeNote);
    }
  }
  if (deleteArmedUntil != 0 && millis() >= deleteArmedUntil) {  // nobody confirmed: back to DELETE
    deleteArmedUntil = 0;
    if (view == View::Inst) drawSubBar();
  }

  int step = currentStep();
  if (step != shownStep) {
    int old = shownStep;
    shownStep = step;
    drawColumn(old);
    drawColumn(step);
  }

  if (touchMode == Touch::Bpm && bpmDir != 0) {
    uint32_t now = millis();
    if (now - bpmPressMs >= kBpmHoldMs && now - bpmLastRepeatMs >= kBpmRepeatMs) {
      bpmLastRepeatMs = now;
      nudgeBpm(bpmDir);
    }
  }
}

}  // namespace pattern
