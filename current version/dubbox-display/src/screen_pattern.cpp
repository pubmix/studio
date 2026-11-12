#include "theme.h"
#include "screen_pattern.h"
#include <Arduino.h>
#include <stdio.h>
#include "keyboard.h"
#include "synth_panel.h"
#include "teensy_link.h"
#include "ui_common.h"

namespace pattern {
namespace {

using namespace ui;

constexpr int kRows = teensylink::kDrumRows;
constexpr int kSteps = teensylink::kPatSteps;
constexpr int kNumPatterns = teensylink::kNumPatterns;
constexpr int kMaxInst = teensylink::kMaxInstruments;

// Large primary controls. Pattern/instrument choices use a temporary chooser.
constexpr int kControlsTop = 154;
constexpr int kControlsBottom = 250;
constexpr Rect kPatternChoice = {16,154,196,250};
constexpr Rect kDrumsTab = {206,154,366,250};
constexpr Rect kInstChoice = {376,154,646,250};
constexpr Rect kAddInst = {656,154,836,250};
constexpr Rect kBpmMinus = {846,154,946,250};
constexpr Rect kBpmValue = {956,154,1136,250};
constexpr Rect kBpmPlus = {1146,154,1264,250};
constexpr int kSubTop=258, kSubBottom=350;
constexpr Rect kModeSound={16,258,166,350}, kModeRoll={174,258,324,350}, kModeKeys={332,258,482,350};
constexpr Rect kOctDown={490,258,590,350}, kOctLabel={598,258,678,350}, kOctUp={686,258,786,350};
constexpr Rect kClearBtn={794,258,974,350}, kDeleteBtn={982,258,1264,350};
constexpr uint32_t kDeleteConfirmMs=2500;
constexpr int kLabelX1=16, kLabelX2=202, kBadgeX1=206, kBadgeX2=232;
constexpr int kGridX=240, kCellW=126, kGridY=258, kInstTop=358;
constexpr int kBodyClearTop=254, kBodyBottom=613;
constexpr int kDrumRowH=128, kRollRows=12, kRollRowH=78;
constexpr int kVisibleSteps=8, kVisibleRows=2;
int stepPage=0, drumPage=0, rollPage=0;
constexpr Rect kPrevRows={16,524,310,610}, kNextRows={326,524,620,610};
constexpr Rect kPrevSteps={636,524,930,610}, kNextSteps={946,524,1264,610};
enum class Chooser {None, Pattern, Instrument, Kit};
int lastKitShown=-1;
constexpr Rect kClassicKit={48,300,620,430},kAcousticKit={660,300,1232,430};
Chooser chooser=Chooser::None;
constexpr Rect kChooserClose={1000,508,1248,610};
constexpr int kKeyCountMin=5, kKeyCountMax=14;
// Performance view: one toolbar, with the remaining content devoted to playable keys.
constexpr keyboard::Layout kKeysLayout={16,258,610,1248,210};
constexpr Rect kKeysSound={16,154,166,250}, kKeysRoll={174,154,324,250};
constexpr Rect kKeysOctDown={332,154,432,250}, kKeysOctLabel={440,154,520,250};
constexpr Rect kKeysOctUp={528,154,628,250};
constexpr Rect kKeyMinus={636,154,736,250}, kKeyCountBox={744,154,856,250};
constexpr Rect kKeyPlus={864,154,964,250}, kPedal={972,154,1264,250};
static_assert(kKeysLayout.top > kPedal.y2 && kKeysLayout.bottom < kTransportTop,
              "Keys must stay between toolbar and transport");
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
View view = View::Drums;
Mode mode = Mode::Sound;
int curInst = 0;       // the instrument being viewed
int octaveBase = 60;   // MIDI note of the roll's bottom row / the keyboards' first key (a C)
constexpr int kOctaveMin = 36;
constexpr int kOctaveMax = 84;

bool visibleNow = false;
bool waitForPresetRelease = false;
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

Rect chipRect(int i) { int x=32+(i%4)*308, y=272+(i/4)*114; return {x,y,x+292,y+102}; }
Rect instTabRect(int i) { int x=32+(i%4)*308; return {x,272,x+292,374}; }
Rect addRect(int) { return kAddInst; }

const teensylink::Drums& drums() { return teensylink::state().drums; }
int instCount() { return drums().instCount; }

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
  int x = kGridX + (c-stepPage*kVisibleSteps) * kCellW;
  int y = kGridY + (r-drumPage*kVisibleRows) * kDrumRowH;
  return {x, y, x + kCellW - 4, y + kDrumRowH - 4};
}

bool drumCellAt(int x, int y, int& r, int& c) {
  if (x < kGridX || x >= kGridX+kVisibleSteps*kCellW || y < kGridY || y >= kGridY+kVisibleRows*kDrumRowH) return false;
  c = (x - kGridX) / kCellW + stepPage*kVisibleSteps;
  r = (y - kGridY) / kDrumRowH + drumPage*kVisibleRows;
  return c >= 0 && c < kSteps && r >= 0 && r < kRows;
}

bool stepOn(int r, int c) { return (drums().rows[g.pattern][r] >> c) & 1; }

void drawDrumCell(int r, int c) {
  if (r/2!=drumPage || c/8!=stepPage) return;
  bool on = stepOn(r, c);
  bool playing = c == shownStep;
  uint32_t rgb;
  if (on) {
    rgb = playing ? theme::palette().text : kRowRgb[r];
  } else {
    rgb = playing ? theme::palette().border : (((c / 4) & 1) ? theme::palette().surface : theme::palette().background);
  }
  Rect rc = drumCellRect(r, c);
  fillRoundRect(rc, 10, rgb565(rgb));
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
    drawButton(badge, n, fontSmall(), rgb565(theme::palette().background), rgb565(rgb));
  } else {
    fillRect(badge.x1, badge.y1, badge.x2, badge.y2, rgb565(theme::palette().surface));
  }
}

// A drum label (touch and hold it, then press a pad, to route that sound to the pad) and the
// number of the pad that plays it.
void drawDrumLabel(int r) {
  if (r/2!=drumPage) return;
  const int y = kGridY + (r-drumPage*kVisibleRows) * kDrumRowH;
  const bool held = routeRow == r;
  Rect lab = {kLabelX1, y, kLabelX2, y + kDrumRowH - 4};
  if (held) {
    drawButton(lab, "PRESS PAD", fontSmall(), rgb565(theme::palette().background), rgb565(theme::palette().text));
  } else {
    drawButton(lab, kRowNames[r], fontSmall(), rgb565(theme::palette().text), rgb565(kRowRgb[r], 0.5f));
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
  if (x < kLabelX1 || x > kBadgeX2 || y < kGridY || y >= kGridY+kVisibleRows*kDrumRowH) return false;
  r = (y - kGridY) / kDrumRowH + drumPage*kVisibleRows;
  return r >= 0 && r < kRows;
}

// ---- Piano roll (the current instrument's notes in this pattern) ----

int rollMidi(int r) { return octaveBase + (kRollRows - 1) - r; }
bool isBlackKey(int midi) {
  int n = midi % 12;
  return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

Rect rollCellRect(int r, int c) {
  int x = kGridX + (c-stepPage*kVisibleSteps) * kCellW;
  int y = kInstTop + (r-rollPage*kVisibleRows) * kRollRowH;
  return {x, y, x + kCellW - 4, y + kRollRowH - 4};
}

bool rollCellAt(int x, int y, int& r, int& c) {
  if (x < kGridX || x >= kGridX+kVisibleSteps*kCellW || y < kInstTop || y >= kInstTop+kVisibleRows*kRollRowH) return false;
  c = (x - kGridX) / kCellW + stepPage*kVisibleSteps;
  r = (y - kInstTop) / kRollRowH + rollPage*kVisibleRows;
  return c >= 0 && c < kSteps && r >= 0 && r < kRollRows;
}

bool rollLabelAt(int x, int y, int& r) {
  if (x < kLabelX1 || x > kBadgeX2 || y < kInstTop || y >= kInstTop+kVisibleRows*kRollRowH) return false;
  r = (y - kInstTop) / kRollRowH + rollPage*kVisibleRows;
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
  if (r/2!=rollPage || c/8!=stepPage) return;
  const int midi = rollMidi(r);
  const bool playing = c == shownStep;
  teensylink::PatNote note;
  const bool hasNote = noteCovering(midi, c, note);
  Rect rc = rollCellRect(r, c);
  fillRect(rc.x1, rc.y1, rc.x2 + 4, rc.y2, rgb565(theme::palette().background));  // clears the gap a note bar may have covered
  uint32_t rgb;
  if (hasNote) {
    rgb = playing ? theme::palette().text : kNoteRgb;
  } else if (isBlackKey(midi)) {
    rgb = playing ? theme::palette().border : (((c / 4) & 1) ? theme::palette().surface : theme::palette().background);
  } else {
    rgb = playing ? theme::palette().border : (((c / 4) & 1) ? theme::palette().surface : theme::palette().background);
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
  if (r/2!=rollPage) return;
  const int midi = rollMidi(r);
  const int y = kInstTop + (r-rollPage*kVisibleRows) * kRollRowH;
  const bool black = isBlackKey(midi);
  Rect lab = {kLabelX1, y, kLabelX2, y + kRollRowH - 4};
  if (routeNote == midi) {
    drawButton(lab, "PRESS PAD", fontSmall(), rgb565(theme::palette().background), rgb565(theme::palette().text));
  } else {
    char label[8];
    snprintf(label, sizeof(label), "%s%d", kNoteNames[midi % 12], midi / 12 - 1);
    drawButton(lab, label, fontSmall(), rgb565(black ? 0xfafafa : 0x101214), rgb565(black ? 0x202028 : 0xdcdcdc));
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
  const uint16_t col = rgb565(pedalDown ? 0x4dd0e1 : theme::palette().border);
  const int t = pedalDown ? 10 : 4;
  fillRect(kPedal.x1, kPedal.y1, kPedal.x2, kPedal.y1 + t - 1, col);
  fillRect(kPedal.x1, kPedal.y2 - t + 1, kPedal.x2, kPedal.y2, col);
  fillRect(kPedal.x1, kPedal.y1, kPedal.x1 + t - 1, kPedal.y2, col);
  fillRect(kPedal.x2 - t + 1, kPedal.y1, kPedal.x2, kPedal.y2, col);
  if (!pedalDown) {  // erase the thicker lit frame's inner edge
    fillRect(kPedal.x1 + t, kPedal.y1 + t, kPedal.x2 - t, kPedal.y1 + 9, rgb565(theme::palette().surface));
    fillRect(kPedal.x1 + t, kPedal.y2 - 9, kPedal.x2 - t, kPedal.y2 - t, rgb565(theme::palette().surface));
    fillRect(kPedal.x1 + t, kPedal.y1 + t, kPedal.x1 + 9, kPedal.y2 - t, rgb565(theme::palette().surface));
    fillRect(kPedal.x2 - 9, kPedal.y1 + t, kPedal.x2 - t, kPedal.y2 - t, rgb565(theme::palette().surface));
  }
}

void drawKeysSide() {
  char oct[8];
  snprintf(oct,sizeof(oct),"C%d",octaveBase/12-1);
  const uint16_t fg=rgb565(theme::palette().text), bg=rgb565(theme::palette().surface);
  drawButton(kKeysSound,"Sound",fontSmall(),fg,bg);
  drawButton(kKeysRoll,"Roll",fontSmall(),fg,bg);
  drawButton(kKeysOctDown,"Oct -",fontSmall(),fg,bg);
  drawButton(kKeysOctLabel,oct,fontSmall(),fg,bg);
  drawButton(kKeysOctUp,"Oct +",fontSmall(),fg,bg);
  setCanvas(kVisible);
  char lab[12];
  snprintf(lab, sizeof(lab), "%d KEYS", kb.keyCount);
  drawButton(kKeyCountBox, lab, fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawButton(kKeyMinus, "-", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawButton(kKeyPlus, "+", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawButton(kPedal, "SUSTAIN", fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawPedalFrame();
}

// ---- Whole-screen drawing ----

// Draws the pattern chips. Only chips whose look changed since last drawn are redrawn (unless
// `all`), because chip text is slow to draw and a visible flicker on every tap.
void drawChips(bool all = true) {
  if (view == View::Inst && mode == Mode::Keys) return;
  (void)all; setCanvas(kVisible);
  char label[24]; snprintf(label,sizeof(label),"Pattern %d",g.pattern+1);
  drawButton(kPatternChoice,label,fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
}

void refreshChips() { drawChips(false); }

void drawBpm() {
  if (view == View::Inst && mode == Mode::Keys) return;
  setCanvas(kVisible);
  lastBpmShown = drums().bpm;
  char label[16];
  snprintf(label, sizeof(label), "%d BPM", drums().bpm);
  drawButton(kBpmMinus, "-", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawButton(kBpmValue, label, fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawButton(kBpmPlus, "+", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
}

// DRUMS and instrument tabs and the "+" that adds an instrument.
void drawTools() {
  if (view == View::Inst && mode == Mode::Keys) return;
  setCanvas(kVisible);
  lastKitShown=drums().kit;
  drawButton(kDrumsTab,view==View::Drums?(drums().kit==1?"Acoustic kit":"Classic kit"):"Drums",fontSmall(),rgb565(theme::palette().text),rgb565(view==View::Drums?theme::palette().accent:theme::palette().surface));
  char label[24]; snprintf(label,sizeof(label),view==View::Inst?"Instrument %d":"Instruments",curInst+1);
  drawButton(kInstChoice,label,fontSmall(),rgb565(theme::palette().text),rgb565(view==View::Inst?theme::palette().accent:theme::palette().surface));
  drawButton(kAddInst,instCount()<kMaxInst?"Add synth":"All added",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
}

void drawPager() {
  int page=view==View::Drums?drumPage:rollPage;
  int rows=view==View::Drums?kRows:kRollRows;
  char prev[32],next[32],left[24],right[24];
  snprintf(prev,sizeof(prev),"Previous %s",view==View::Drums?"sounds":"notes");
  snprintf(next,sizeof(next),"Next %s (%d/%d)",view==View::Drums?"sounds":"notes",page+1,(rows+1)/2);
  snprintf(left,sizeof(left),"Steps 1-8%s",stepPage==0?" *":"");
  snprintf(right,sizeof(right),"Steps 9-16%s",stepPage==1?" *":"");
  drawButton(kPrevRows,prev,fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  drawButton(kNextRows,next,fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  drawButton(kPrevSteps,left,fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  drawButton(kNextSteps,right,fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
}
void drawChooser() {
  setCanvas(kVisible);
  ScopedRedraw redraw({16,258,1264,610});
  fillRoundRect({16,258,1264,610},20,rgb565(theme::palette().surface));
  if(chooser==Chooser::Kit) {
    drawButton(kClassicKit,"Classic",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().background));
    drawButton(kAcousticKit,"Acoustic",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().background));
    drawButton(kChooserClose,"Done",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().accent));
    return;
  }
  int n=chooser==Chooser::Pattern?kNumPatterns:instCount();
  for(int i=0;i<n;++i) {
    char name[24];snprintf(name,sizeof(name),chooser==Chooser::Pattern?"Pattern %d":"Instrument %d",i+1);
    drawButton(chooser==Chooser::Pattern?chipRect(i):instTabRect(i),name,fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().background));
  }
  drawButton(kChooserClose,"Done",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().accent));
}

// The bar under the controls row while an instrument is open: SOUND / ROLL / KEYS, the octave,
// CLEAR (roll only) and DELETE (which asks to be tapped twice).
void drawSubBar() {
  if (view == View::Inst && mode == Mode::Keys) return;
  setCanvas(kVisible);
  ScopedRedraw redraw({0,kSubTop-2,kW-1,kSubBottom+2});
  fillRect(0, kSubTop - 2, kW - 1, kSubBottom + 2, rgb565(theme::palette().background));
  struct {
    Rect r;
    const char* label;
    Mode m;
  } modes[3] = {{kModeSound, "SOUND", Mode::Sound}, {kModeRoll, "ROLL", Mode::Roll}, {kModeKeys, "KEYS", Mode::Keys}};
  for (const auto& b : modes) {
    const bool on = mode == b.m;
    drawButton(b.r, b.label, fontSmall(), rgb565(on ? theme::palette().background : theme::palette().text), rgb565(on ? kNoteRgb : 0x4a2a7a));
  }
  char oct[8];
  snprintf(oct, sizeof(oct), "C%d", octaveBase / 12 - 1);
  drawButton(kOctDown, "-", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawButton(kOctLabel, oct, fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawButton(kOctUp, "+", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  if (mode == Mode::Roll) drawButton(kClearBtn, "CLEAR", fontSmall(), rgb565(theme::palette().text), rgb565(0x7a2a2a));
  const bool armed = millis() < deleteArmedUntil;
  drawButton(kDeleteBtn, armed ? "SURE?" : "DELETE", fontSmall(), rgb565(theme::palette().text), rgb565(armed ? 0xff2b2b : 0x9c2a2a));
}

// `clear` blanks the body first (needed when the view changes; skipped for in-place refreshes
// so the screen does not flash).
void drawBody(bool clear = true, bool preserveTouch = false) {
  setCanvas(kVisible);
  const int bodyTop=(view==View::Inst && mode==Mode::Keys)?kControlsTop:kBodyClearTop;
  ScopedRedraw redraw({0,bodyTop,kW-1,kBodyBottom});
  if (clear) fillRect(0, bodyTop, kW - 1, kBodyBottom, rgb565(theme::palette().background));
  if (view == View::Drums) {
    drawDrumGrid();
    drawPager();
    return;
  }
  drawSubBar();
  switch (mode) {
    case Mode::Sound:
      if(preserveTouch)synthpanel::redraw(curInst);
      else synthpanel::draw(curInst, octaveBase);
      break;
    case Mode::Roll:
      drawRoll();
      drawPager();
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

void drawAll(bool preserveTouch = false) {
  setCanvas(kVisible);
  ScopedRedraw redraw({0,kHeaderH,kW-1,kBodyBottom});
  fillRect(0,kControlsTop,kW-1,kControlsBottom,rgb565(theme::palette().background));
  drawChips();
  drawTools();
  drawBpm();
  drawBody(true,preserveTouch);
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
  drawAll();
}

void setMode(Mode m) {
  if (mode == m) return;
  mode = m;
  shownStep = -1;
  releaseKeys();
  waitForPresetRelease = m == Mode::Keys;
  drawAll();
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
  if (mode==Mode::Keys) drawKeysSide(); else drawSubBar();
  switch (mode) {
    case Mode::Sound:
      synthpanel::setOctave(base);
      break;
    case Mode::Roll:
      drawRoll();
      drawPager();
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

void redraw() {
  drawAll(true);
  if(chooser!=Chooser::None)drawChooser();
}

void enter() {
  visibleNow = true;
  chooser=Chooser::None;
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
  if (chooser==Chooser::Kit) {
    int kit=inRect(kClassicKit,x,y)?0:(inRect(kAcousticKit,x,y)?1:-1);
    if(kit>=0 || inRect(kChooserClose,x,y)) {
      if(kit>=0 && g.linked) teensylink::setDrumKit(kit);
      chooser=Chooser::None;drawAll();
    }
    return;
  }
  if (chooser!=Chooser::None) {
    int count=chooser==Chooser::Pattern?kNumPatterns:instCount();
    for(int i=0;i<count;++i) {
      if(inRect(chooser==Chooser::Pattern?chipRect(i):instTabRect(i),x,y)) {
        Chooser was=chooser;chooser=Chooser::None;
        if(was==Chooser::Pattern) selectPattern(i); else setView(View::Inst,i);
        drawAll();return;
      }
    }
    if(inRect(kChooserClose,x,y)) {chooser=Chooser::None;drawAll();}
    return;
  }
  if(view==View::Inst && mode==Mode::Keys) {
    if(inRect(kKeysSound,x,y)) setMode(Mode::Sound);
    else if(inRect(kKeysRoll,x,y)) setMode(Mode::Roll);
    else if(inRect(kKeysOctDown,x,y)) changeOctave(-1);
    else if(inRect(kKeysOctUp,x,y)) changeOctave(1);
    else if(inRect(kKeyMinus,x,y)) changeKeyCount(-1);
    else if(inRect(kKeyPlus,x,y)) changeKeyCount(1);
    return; // Keys and the momentary sustain pedal use the multi-touch finger list.
  }
  if(inRect(kPatternChoice,x,y)) {chooser=Chooser::Pattern;drawChooser();return;}
  if(inRect(kInstChoice,x,y)) {chooser=Chooser::Instrument;drawChooser();return;}
  if(view==View::Drums || mode==Mode::Roll) {
    int& page=view==View::Drums?drumPage:rollPage;
    int count=view==View::Drums?(kRows+1)/2:(kRollRows+1)/2;
    if(inRect(kPrevRows,x,y)) {page=(page+count-1)%count;drawBody();return;}
    if(inRect(kNextRows,x,y)) {page=(page+1)%count;drawBody();return;}
    if(inRect(kPrevSteps,x,y)) {stepPage=0;drawBody();return;}
    if(inRect(kNextSteps,x,y)) {stepPage=1;drawBody();return;}
  }
  if (inRect(kDrumsTab, x, y)) {
    if(view==View::Drums){chooser=Chooser::Kit;drawChooser();return;}
    setView(View::Drums, 0);
    return;
  }
  const int n = instCount();
  if (n < kMaxInst && inRect(addRect(n), x, y)) {  // a new instrument, opened on its sound
    if (g.linked) teensylink::addInstrument();
    if (instCount() > n) {
      mode = Mode::Sound;
      view = View::Drums;  // (forces setView to switch)
      setView(View::Inst, instCount() - 1);
    }
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
      if (synthpanel::touchDown(curInst, x, y)) {
        if (synthpanel::takePresetApplied()) {
          setMode(Mode::Keys);
          waitForPresetRelease = true;
        } else touchMode = Touch::Panel;
      }
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
    // Drag to the right edge to continue a long note into the second half;
    // dragging back to the left edge returns to steps 1-8.
    if (stepPage==0 && x>=kGridX+kVisibleSteps*kCellW-12) {stepPage=1;drawBody();}
    else if (stepPage==1 && x<=kGridX+12) {stepPage=0;drawBody();}
    int col = constrain((x - kGridX) / kCellW + stepPage*kVisibleSteps, 0, kSteps - 1);
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
  if (waitForPresetRelease) {
    if (n > 0) return;
    waitForPresetRelease = false;
  }
  const bool inInst = visibleNow && chooser==Chooser::None && view == View::Inst;
  const bool keysActive = inInst && mode == Mode::Keys;
  const bool soundActive = inInst && mode == Mode::Sound;
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
  if (!visible || chooser!=Chooser::None) return;

  if (instsChanged) {  // instruments were added / removed (e.g. a project opened)
    fixView();
    drawAll();
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
    if (synthChanged && view == View::Inst && mode == Mode::Sound) synthpanel::refresh(curInst);
  }
  if (rhythm && drums().kit != lastKitShown) drawTools();
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
