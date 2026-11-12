#include "screen_controls.h"

#include <Arduino.h>
#include <stdio.h>

#include "teensy_link.h"
#include "ui_common.h"

namespace controlsscreen {
namespace {

using namespace ui;

constexpr int kFaders = 4;
constexpr int kEncoders = 4;
constexpr int kButtons = 8;  // 4 fx-select, then 4 drum pads
constexpr int kTotalChecks = kFaders + kEncoders * 3 + kButtons;

constexpr uint32_t kGreenSeen = 0x1f8a3a;
constexpr uint32_t kGreenHeld = 0x7dff9a;
constexpr uint32_t kGrey = 0x3a3a3a;
constexpr uint32_t kBarBg = 0x4a4a4a;
constexpr uint32_t kBarFill = 0xffd23d;
constexpr uint32_t kTick = 0x40c4ff;
constexpr uint32_t kRed = 0xe53935;

constexpr int kBarTop = 150;
constexpr int kBarBottom = 560;
constexpr int kBarH = kBarBottom - kBarTop;
constexpr int kBarW = 60;
constexpr int kRangeNeededPermille = 800;  // a fader counts as working once it covered this much travel

constexpr Rect kReset = {1060, 640, 1260, 700};

// What has been seen since the screen opened (or since RESET).
int faderMin[kFaders];
int faderMax[kFaders];
int encBase[kEncoders];  // detent count when the screen opened, so counts start at 0
bool encBaseSet = false;
int encLast[kEncoders];
bool seenCw[kEncoders];
bool seenCcw[kEncoders];
bool seenPush[kEncoders];
bool seenButton[kButtons];

// What is currently drawn, so only changes are repainted.
int shownFill[kFaders];
int shownText[kFaders];
bool shownFaderOk[kFaders];
uint32_t lastTextMs[kFaders];
int shownCount[kEncoders];
int shownEncChip[kEncoders][3];  // CCW, CW, PUSH: 0 unseen, 1 seen, 2 held
int shownButton[kButtons];
int shownTested = -1;
int shownLinked = -1;

int barX(int i) { return 40 + i * 100; }
Rect barRect(int i) { return {barX(i), kBarTop, barX(i) + kBarW, kBarBottom}; }
Rect faderChip(int i) { return {barX(i) - 6, 612, barX(i) + kBarW + 6, 648}; }
int encY(int i) { return 150 + i * 110; }
Rect encChip(int i, int chip) {
  const int x = chip == 0 ? 600 : chip == 1 ? 690 : 780;
  return {x, encY(i) + 26, x + (chip == 2 ? 80 : 80), encY(i) + 70};
}
Rect buttonCell(int b) {
  const int row = b / 4;
  const int col = b % 4;
  const int top = row == 0 ? 190 : 340;
  return {890 + col * 92, top, 890 + col * 92 + 80, top + 80};
}

bool faderOk(int i) { return faderMax[i] - faderMin[i] >= kRangeNeededPermille; }

int testedCount() {
  int n = 0;
  for (int i = 0; i < kFaders; ++i) n += faderOk(i) ? 1 : 0;
  for (int i = 0; i < kEncoders; ++i) n += (seenCw[i] ? 1 : 0) + (seenCcw[i] ? 1 : 0) + (seenPush[i] ? 1 : 0);
  for (int b = 0; b < kButtons; ++b) n += seenButton[b] ? 1 : 0;
  return n;
}

void resetSeen() {
  const teensylink::Controls& c = teensylink::state().controls;
  for (int i = 0; i < kFaders; ++i) {
    faderMin[i] = c.known ? c.faderPermille[i] : 1000;
    faderMax[i] = c.known ? c.faderPermille[i] : 0;
    shownFill[i] = -1;
    shownText[i] = -1;
    shownFaderOk[i] = false;
    lastTextMs[i] = 0;
  }
  for (int i = 0; i < kEncoders; ++i) {
    encBase[i] = c.known ? c.encoderDetents[i] : 0;
    encLast[i] = encBase[i];
    seenCw[i] = seenCcw[i] = seenPush[i] = false;
    shownCount[i] = 1 << 20;
    for (int k = 0; k < 3; ++k) shownEncChip[i][k] = -1;
  }
  encBaseSet = c.known;
  for (int b = 0; b < kButtons; ++b) {
    seenButton[b] = false;
    shownButton[b] = -1;
  }
  shownTested = -1;
  shownLinked = -1;
}

void drawFader(int i, const teensylink::Controls& c) {
  const int p = constrain(c.faderPermille[i], 0, 1000);
  if (p < faderMin[i]) faderMin[i] = p;
  if (p > faderMax[i]) faderMax[i] = p;
  const bool ok = faderOk(i);
  const Rect r = barRect(i);
  const int fill = (int)((long)kBarH * p / 1000);
  if (abs(fill - shownFill[i]) >= 2 || ok != shownFaderOk[i] || shownFill[i] < 0) {
    setCanvas(kVisible);
    fillRect(r.x1, r.y1, r.x2, r.y2 - fill, rgb565(kBarBg));
    if (fill > 0) fillRect(r.x1, r.y2 - fill + 1, r.x2, r.y2, rgb565(ok ? kGreenHeld : kBarFill));
    // Ticks beside the bar at the lowest and highest positions seen so far.
    const int yMin = kBarBottom - (int)((long)kBarH * faderMin[i] / 1000);
    const int yMax = kBarBottom - (int)((long)kBarH * faderMax[i] / 1000);
    fillRect(r.x1 - 12, yMin - 1, r.x1 - 3, yMin + 1, rgb565(kTick));
    fillRect(r.x2 + 3, yMin - 1, r.x2 + 12, yMin + 1, rgb565(kTick));
    fillRect(r.x1 - 12, yMax - 1, r.x1 - 3, yMax + 1, rgb565(kTick));
    fillRect(r.x2 + 3, yMax - 1, r.x2 + 12, yMax + 1, rgb565(kTick));
    shownFill[i] = fill;
  }
  const uint32_t now = millis();
  if (p / 10 != shownText[i] && now - lastTextMs[i] >= 90) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", p / 10);
    setCanvas(kVisible);
    drawText(barX(i) - 4, 570, buf, fontSmall(), rgb565(0xffffff), rgb565(0x000000), 70);
    shownText[i] = p / 10;
    lastTextMs[i] = now;
  }
  if (ok != shownFaderOk[i]) {
    setCanvas(kVisible);
    drawButton(faderChip(i), ok ? "OK" : "MOVE", fontSmall(), rgb565(0xffffff), rgb565(ok ? kGreenSeen : kGrey));
    shownFaderOk[i] = ok;
  }
}

void drawEncoder(int i, const teensylink::Controls& c) {
  if (!encBaseSet) {
    for (int k = 0; k < kEncoders; ++k) {
      encBase[k] = c.encoderDetents[k];
      encLast[k] = c.encoderDetents[k];
    }
    encBaseSet = true;
  }
  const int count = c.encoderDetents[i];
  if (count > encLast[i]) seenCw[i] = true;
  if (count < encLast[i]) seenCcw[i] = true;
  encLast[i] = count;
  const bool held = (c.mask & (1 << i)) != 0;
  if (held) seenPush[i] = true;

  const int rel = count - encBase[i];
  setCanvas(kVisible);
  if (rel != shownCount[i]) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%+d", rel);
    drawText(560, encY(i) + 30, buf, fontLarge(), rgb565(0xffffff), rgb565(0x000000), 30);
    shownCount[i] = rel;
  }
  const int state[3] = {seenCcw[i] ? 1 : 0, seenCw[i] ? 1 : 0, held ? 2 : (seenPush[i] ? 1 : 0)};
  static const char* const names[3] = {"CCW", "CW", "PUSH"};
  for (int k = 0; k < 3; ++k) {
    if (state[k] == shownEncChip[i][k]) continue;
    const uint32_t fill = state[k] == 2 ? kGreenHeld : state[k] == 1 ? kGreenSeen : kGrey;
    drawButton(encChip(i, k), names[k], fontSmall(), rgb565(state[k] == 2 ? 0x000000 : 0xffffff), rgb565(fill));
    shownEncChip[i][k] = state[k];
  }
}

void drawButtons(const teensylink::Controls& c) {
  setCanvas(kVisible);
  for (int b = 0; b < kButtons; ++b) {
    // mask bits 4-7 = fx-select 1-4, bits 8-11 = drum pads 1-4
    const bool held = (c.mask & (1 << (4 + b))) != 0;
    if (held) seenButton[b] = true;
    const int state = held ? 2 : (seenButton[b] ? 1 : 0);
    if (state == shownButton[b]) continue;
    char label[4];
    snprintf(label, sizeof(label), "%d", (b % 4) + 1);
    const uint32_t fill = state == 2 ? kGreenHeld : state == 1 ? kGreenSeen : kGrey;
    drawButton(buttonCell(b), label, fontLarge(), rgb565(state == 2 ? 0x000000 : 0xffffff), rgb565(fill));
    shownButton[b] = state;
  }
}

void drawSummary(const teensylink::Controls& c) {
  const int tested = testedCount();
  if (tested != shownTested) {
    char buf[40];
    snprintf(buf, sizeof(buf), tested == kTotalChecks ? "ALL %d WORKING" : "TESTED %d / %d", tested, kTotalChecks);
    setCanvas(kVisible);
    drawText(20, 668, buf, fontLarge(), rgb565(tested == kTotalChecks ? kGreenHeld : 0xffffff), rgb565(0x000000), 420);
    shownTested = tested;
  }
  (void)c;
}

void drawStatic() {
  setCanvas(kVisible);
  fillRect(0, 61, kW - 1, kH - 1, rgb565(0x000000));
  drawText(20, 68, "TOUCH EVERY CONTROL: MOVE FADERS END TO END, TURN ENCODERS BOTH WAYS AND PUSH THEM, PRESS BUTTONS",
           fontSmall(), rgb565(0x9aa3b5), rgb565(0x000000));
  drawText(40, 104, "FADERS", fontLarge(), rgb565(0xffd23d), rgb565(0x000000));
  drawText(460, 104, "ENCODERS (turn + push)", fontLarge(), rgb565(0x40c4ff), rgb565(0x000000));
  drawText(890, 104, "BUTTONS", fontLarge(), rgb565(0x66bb6a), rgb565(0x000000));
  drawText(890, 156, "FX SELECT", fontSmall(), rgb565(0x9aa3b5), rgb565(0x000000));
  drawText(890, 306, "DRUM PADS", fontSmall(), rgb565(0x9aa3b5), rgb565(0x000000));
  for (int i = 0; i < kFaders; ++i) {
    char buf[8];
    snprintf(buf, sizeof(buf), "F%d", i + 1);
    drawText(barX(i) + 14, 124, buf, fontSmall(), rgb565(0xffffff), rgb565(0x000000));
  }
  for (int i = 0; i < kEncoders; ++i) {
    char buf[16];
    snprintf(buf, sizeof(buf), "ENC %d", i + 1);
    drawText(460, encY(i) + 30, buf, fontLarge(), rgb565(0xffffff), rgb565(0x000000));
  }
  drawButton(kReset, "RESET", fontSmall(), rgb565(0xffffff), rgb565(0x555555));
}

void paintAll() {
  drawStatic();
  const teensylink::Controls& c = teensylink::state().controls;
  for (int i = 0; i < kFaders; ++i) {
    shownFill[i] = -1;
    shownText[i] = -1;
    shownFaderOk[i] = !faderOk(i);  // force the chip to draw
    drawFader(i, c);
  }
  for (int i = 0; i < kEncoders; ++i) drawEncoder(i, c);
  drawButtons(c);
  drawSummary(c);
}

}  // namespace

void enter() {
  teensylink::takeControlsChanged();
  resetSeen();
  paintAll();
  if (g.linked) teensylink::sendControlsStream(true);
}

void leave() {
  if (g.linked) teensylink::sendControlsStream(false);
}

void touchDown(int x, int y) {
  if (inRect(kReset, x, y)) {
    resetSeen();
    paintAll();
  }
}

void update(bool visible) {
  const bool changed = teensylink::takeControlsChanged();
  if (!visible) return;
  if (shownLinked != (g.linked ? 1 : 0)) {
    shownLinked = g.linked ? 1 : 0;
    if (g.linked) {
      teensylink::sendControlsStream(true);  // the Teensy (re)connected while this screen was open
    } else {
      setCanvas(kVisible);
      drawText(1000, 68, "TEENSY NOT CONNECTED", fontSmall(), rgb565(kRed), rgb565(0x000000), 260);
    }
  }
  if (!changed) return;
  const teensylink::Controls& c = teensylink::state().controls;
  for (int i = 0; i < kFaders; ++i) drawFader(i, c);
  for (int i = 0; i < kEncoders; ++i) drawEncoder(i, c);
  drawButtons(c);
  drawSummary(c);
}

}  // namespace controlsscreen
