#include "theme.h"
#include "screen_input.h"
#include <Arduino.h>
#include "teensy_link.h"
#include "ui_common.h"

namespace inputscreen {
namespace {

using namespace ui;

constexpr int kNum = teensylink::kNumInputs;
constexpr int kStripTop = 112;
constexpr int kStripBottom = 706;
constexpr int kStripW = 400;
constexpr int kStripStride = 420;
constexpr int kStripLeft = 20;

constexpr int kBarTop = 230;
constexpr int kBarBottom = 620;
constexpr int kBarH = kBarBottom - kBarTop;

constexpr int kMinTenths = -300;  // -30 dB, the Teensy's range
constexpr int kMaxTenths = 300;   // +30 dB
constexpr uint32_t kSendIntervalMs = 70;

uint32_t kPanelRgb() { return theme::palette().surface; }
uint32_t kBarBgRgb() { return theme::palette().border; }
constexpr uint32_t kAccentRgb[kNum] = {0xffd23d, 0x2fc7a0, 0x3d7fff};
const char* const kTitle[kNum] = {"MIC", "INPUT 1", "INPUT 2"};
const char* const kSubtitle[kNum] = {"BUILT-IN MIC", "LINE IN L", "LINE IN R"};

enum Flag { kNotch = 1, kLowCut = 2 };

int gain[kNum] = {0, 0, 0};              // tenths of a dB, as shown / last sent
int flags[kNum] = {0, kNotch, kNotch};   // until the Teensy reports its own
int shownMeterFill[kNum] = {-1, -1, -1};
uint16_t shownMeterColor[kNum] = {0, 0, 0};
int dragging = -1;
uint32_t lastSendMs = 0;

int stripX(int i) { return kStripLeft + i * kStripStride; }
Rect faderRect(int i) { return {stripX(i) + 30, kBarTop, stripX(i) + 100, kBarBottom}; }
Rect meterRect(int i) { return {stripX(i) + 130, kBarTop, stripX(i) + 170, kBarBottom}; }
Rect minusRect(int i) { return {stripX(i) + 205, 230, stripX(i) + 290, 332}; }
Rect plusRect(int i) { return {stripX(i) + 300, 230, stripX(i) + 385, 332}; }
Rect resetRect(int i) { return {stripX(i) + 205, 344, stripX(i) + 385, 446}; }
Rect notchRect(int i) { return {stripX(i) + 205, 458, stripX(i) + 385, 560}; }
Rect lowCutRect(int i) { return {stripX(i) + 205, 572, stripX(i) + 385, 674}; }

uint16_t meterColor(float peak) {
  if (peak >= 0.95f) return rgb565(0xe53935);  // at or near clipping
  if (peak >= 0.708f) return rgb565(0xff7043);
  if (peak >= 0.251f) return rgb565(0xffa726);
  return rgb565(0x43d05a);
}

void send(int i) {
  if (g.linked) teensylink::sendInput(i, gain[i], flags[i]);
}

void drawGain(int i) {
  setCanvas(kVisible);
  Rect r = faderRect(i);
  int fillH = (int)((long)kBarH * (gain[i] - kMinTenths) / (kMaxTenths - kMinTenths));
  fillRect(r.x1, r.y1, r.x2, r.y2 - fillH, rgb565(kBarBgRgb()));
  fillRect(r.x1, r.y2 - fillH + 1, r.x2, r.y2, rgb565(theme::palette().text));
  // The 0 dB mark, across the fader.
  int zeroY = kBarBottom - (int)((long)kBarH * (0 - kMinTenths) / (kMaxTenths - kMinTenths));
  fillRect(r.x1 - 10, zeroY - 1, r.x2 + 10, zeroY + 1, rgb565(kAccentRgb[i]));
  char buf[24];
  snprintf(buf, sizeof(buf), "%+.1f dB", gain[i] / 10.0f);
  drawText(stripX(i) + 14, 190, buf, fontLarge(), rgb565(theme::palette().text), rgb565(kPanelRgb()), 250);
}

void drawToggles(int i) {
  setCanvas(kVisible);
  const bool notch = (flags[i] & kNotch) != 0;
  const bool lowCut = (flags[i] & kLowCut) != 0;
  drawButton(notchRect(i), notch ? "Notch on" : "Notch off", fontSmall(), rgb565(theme::palette().text),
             rgb565(notch ? theme::palette().accent : theme::palette().border));
  drawButton(lowCutRect(i), lowCut ? "Low cut on" : "Low cut off", fontSmall(), rgb565(theme::palette().text),
             rgb565(lowCut ? theme::palette().accent : theme::palette().border));
}

void drawMeter(int i, bool force) {
  const int v = teensylink::state().input[i].levelPermille;
  const int fillH = (int)((long)kBarH * constrain(v, 0, 1000) / 1000);
  const uint16_t color = meterColor(v / 1000.0f);
  if (!force && shownMeterFill[i] >= 0 && abs(fillH - shownMeterFill[i]) < 5 && color == shownMeterColor[i]) {
    return;
  }
  setCanvas(kVisible);
  Rect r = meterRect(i);
  fillRect(r.x1, r.y1, r.x2, r.y2 - fillH, rgb565(kBarBgRgb()));
  fillRect(r.x1, r.y2 - fillH + 1, r.x2, r.y2, color);
  shownMeterFill[i] = fillH;
  shownMeterColor[i] = color;
}

void drawStrip(int i) {
  const int x = stripX(i);
  setCanvas(kVisible);
  fillRoundRect({x, kStripTop, x + kStripW - 1, kStripBottom}, 18, rgb565(kPanelRgb()));

  drawText(x + 14, 128, kTitle[i], fontLarge(), rgb565(kAccentRgb[i]), rgb565(kPanelRgb()));
  drawText(x + 14, 164, kSubtitle[i], fontSmall(), rgb565(theme::palette().muted), rgb565(kPanelRgb()));
  drawText(x + 30, 636, "GAIN", fontSmall(), rgb565(theme::palette().muted), rgb565(kPanelRgb()));
  drawText(x + 122, 636, "LEVEL", fontSmall(), rgb565(theme::palette().muted), rgb565(kPanelRgb()));
  drawButton(minusRect(i), "-1 dB", fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().accent));
  drawButton(plusRect(i), "+1 dB", fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().accent));
  drawButton(resetRect(i), "RESET 0 dB", fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().border));
  drawGain(i);
  drawToggles(i);
  shownMeterFill[i] = -1;
  drawMeter(i, true);
}

void redrawAll() {
  for (int i = 0; i < kNum; ++i) drawStrip(i);
}

void adoptStateFromTeensy() {
  const teensylink::State& st = teensylink::state();
  if (!st.inputKnown) return;
  for (int i = 0; i < kNum; ++i) {
    gain[i] = st.input[i].gainTenthsDb;
    flags[i] = st.input[i].flags;
  }
}

void setGain(int i, int tenths) {
  gain[i] = constrain(tenths, kMinTenths, kMaxTenths);
  drawGain(i);
  send(i);
  lastSendMs = millis();
}

int tenthsFromY(int y) {
  const int c = constrain(y, kBarTop, kBarBottom);
  return kMinTenths + (int)((long)(kBarBottom - c) * (kMaxTenths - kMinTenths) / kBarH);
}

}  // namespace

void enter() {
  dragging = -1;
  teensylink::takeInputChanged();
  teensylink::takeInputLevelChanged();
  adoptStateFromTeensy();
  redrawAll();
  if (g.linked) teensylink::sendInputMeters(true);
}

void leave() {
  dragging = -1;
  if (g.linked) teensylink::sendInputMeters(false);
}

void touchDown(int x, int y) {
  for (int i = 0; i < kNum; ++i) {
    Rect f = faderRect(i);
    if (x >= f.x1 - 16 && x <= f.x2 + 16 && y >= f.y1 - 16 && y <= f.y2 + 16) {
      dragging = i;
      setGain(i, tenthsFromY(y));
      return;
    }
    if (inRect(minusRect(i), x, y)) {
      setGain(i, gain[i] - 10);
      return;
    }
    if (inRect(plusRect(i), x, y)) {
      setGain(i, gain[i] + 10);
      return;
    }
    if (inRect(resetRect(i), x, y)) {
      setGain(i, 0);
      return;
    }
    if (inRect(notchRect(i), x, y)) {
      flags[i] ^= kNotch;
      drawToggles(i);
      send(i);
      return;
    }
    if (inRect(lowCutRect(i), x, y)) {
      flags[i] ^= kLowCut;
      drawToggles(i);
      send(i);
      return;
    }
  }
}

void touchMove(int x, int y) {
  (void)x;
  if (dragging < 0) return;
  gain[dragging] = tenthsFromY(y);
  drawGain(dragging);
  const uint32_t now = millis();
  if (now - lastSendMs >= kSendIntervalMs) {
    lastSendMs = now;
    send(dragging);
  }
}

void touchUp() {
  if (dragging >= 0) {
    send(dragging);  // the final position, in case the throttle skipped it
    dragging = -1;
  }
}

void update(bool visible) {
  const bool changed = teensylink::takeInputChanged();
  const bool levels = teensylink::takeInputLevelChanged();
  if (!visible) return;
  if (changed && dragging < 0) {
    int oldGain[kNum], oldFlags[kNum];
    for (int i = 0; i < kNum; ++i) {
      oldGain[i] = gain[i];
      oldFlags[i] = flags[i];
    }
    adoptStateFromTeensy();
    for (int i = 0; i < kNum; ++i) {
      if (gain[i] != oldGain[i]) drawGain(i);
      if (flags[i] != oldFlags[i]) drawToggles(i);
    }
  }
  if (levels) {
    for (int i = 0; i < kNum; ++i) drawMeter(i, false);
  }
}


void redraw() {
  redrawAll();
}

}  // namespace inputscreen
