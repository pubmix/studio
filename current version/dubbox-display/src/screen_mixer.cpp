#include "theme.h"
#include "track_icons.h"
#include "screen_mixer.h"
#include <Arduino.h>
#include "teensy_link.h"
#include "ui_common.h"

namespace mixer {
namespace {

using namespace ui;

constexpr int kStripTop = 154;
constexpr int kStripBottom = 608;
constexpr int kStripW = 300;
constexpr int kStripStride = 310;
constexpr int kStripLeft = 30;

constexpr int kBarTop = 240;
constexpr int kBarBottom = 450;
constexpr int kBarH = kBarBottom - kBarTop;

constexpr int kNumSlots = 3;

const char* fxName(int type) {
  switch (type) {
    case 0: return "Delay";
    case 1: return "Flange";
    case 2: return "Reverb";
    case 3: return "Chorus";
    default: return "";
  }
}

uint32_t kPanelRgb() { return theme::palette().surface; }
uint32_t kBarBgRgb() { return theme::palette().background; }
uint32_t activeRgb() { return theme::palette().accent; }

int stripX(int i) { return kStripLeft + i * kStripStride; }
Rect faderRect(int i) { return {stripX(i) + 34, kBarTop, stripX(i) + 46, kBarBottom}; }
Rect meterRect(int i) { return {stripX(i) + 96, kBarTop, stripX(i) + 105, kBarBottom}; }
Rect slotRect(int, int slot) { return {90 + slot * 370, 240, 430 + slot * 370, 342}; }
Rect wetRect(int) { return {90, 370, 790, 472}; }
Rect plusRect(int i) { return {stripX(i) + 140, 240, stripX(i) + 284, 342}; }
Rect effectsRect(int i) { return {stripX(i) + 140, 364, stripX(i) + 284, 466}; }
Rect bypassRect(int) { return {850, 370, 1170, 472}; }
Rect closeRect() { return {850, 490, 1170, 592}; }
Rect flangeBtn() { return {90, 240, 600, 342}; }
Rect chorusBtn() { return {660, 240, 1170, 342}; }

// Local state shown on screen, so redraws only touch what changed.
int shownFader[kNumTracks] = {-1, -1, -1, -1};
int shownPeak[kNumTracks] = {-1, -1, -1, -1};

int modalTrack = -1;
bool choosingEffect = false;
int dragWetTrack = -1;
uint32_t lastWetSendMs = 0;
int localWet[kNumTracks] = {-1, -1, -1, -1};  // wet shown while dragging

uint16_t meterColor(float peak) {
  if (peak >= 0.708f) return rgb565(0xe53935);  // -3 dBFS and above
  if (peak >= 0.251f) return rgb565(0xffa726);  // -12..-3 dBFS
  return rgb565(theme::palette().accent);
}

int shownFillFader[kNumTracks] = {-1, -1, -1, -1};
int shownFillMeter[kNumTracks] = {-1, -1, -1, -1};
uint16_t shownMeterColor[kNumTracks] = {0, 0, 0, 0};

// Redraws only the slice of a vertical bar that changed, so bars don't flicker.
void drawBar(const Rect& r, int fillH, int& shownFill, uint16_t fillColor, uint16_t bgColor,
             bool fullRedraw) {
  setCanvas(kVisible);
  if (fullRedraw || shownFill < 0) {
    fillRect(r.x1, r.y1, r.x2, r.y2 - fillH, bgColor);
    fillRect(r.x1, r.y2 - fillH + 1, r.x2, r.y2, fillColor);
  } else if (fillH > shownFill) {
    fillRect(r.x1, r.y2 - fillH + 1, r.x2, r.y2 - shownFill, fillColor);
  } else if (fillH < shownFill) {
    fillRect(r.x1, r.y2 - shownFill + 1, r.x2, r.y2 - fillH, bgColor);
  }
  shownFill = fillH;
}

void drawFader(int i, bool force) {
  const teensylink::State& st = teensylink::state();
  int v = st.faderPermille[i];
  if (!force && abs(v - shownFader[i]) < 8) return;
  shownFader[i] = v;
  if (force) shownFillFader[i] = -1;
  int fillH = (int)((long)kBarH * v / 1000);
  drawBar(faderRect(i), fillH, shownFillFader[i], rgb565(theme::palette().accent), rgb565(kBarBgRgb()), false);
}

void drawMeter(int i, bool force) {
  const teensylink::State& st = teensylink::state();
  int v = st.peakPermille[i];
  if (!force && abs(v - shownPeak[i]) < 15) return;
  shownPeak[i] = v;
  if (force) shownFillMeter[i] = -1;
  int fillH = (int)((long)kBarH * v / 1000);
  uint16_t color = meterColor(v / 1000.0f);
  bool colorChanged = color != shownMeterColor[i];
  shownMeterColor[i] = color;
  drawBar(meterRect(i), fillH, shownFillMeter[i], color, rgb565(kBarBgRgb()), colorChanged);
}

void drawWet(int i) {
  const teensylink::FxInfo& fx = teensylink::state().fx[i];
  int wet = (dragWetTrack == i && localWet[i] >= 0) ? localWet[i] : fx.wetPermille;
  Rect r = wetRect(i);
  setCanvas(kVisible);
  int w = r.x2 - r.x1;
  int fillW = (int)((long)w * wet / 1000);
  fillRect(r.x1, r.y1, r.x2, r.y2, rgb565(kPanelRgb()));
  const int mid = r.y1 + 72;
  fillRoundRect({r.x1, mid - 4, r.x2, mid + 4}, 4, rgb565(kBarBgRgb()));
  fillRect(r.x1, mid - 3, r.x1 + fillW, mid + 3, rgb565(activeRgb()));
  int knob = constrain(r.x1 + fillW, r.x1 + 7, r.x2 - 7);
  fillRoundRect({knob - 7, mid - 13, knob + 7, mid + 13}, 6, rgb565(activeRgb()));
  char buf[16];
  snprintf(buf, sizeof(buf), "Wet %d%%", wet / 10);
  drawText(r.x1, r.y1 + 6, buf, fontLarge(), rgb565(theme::palette().text), rgb565(kPanelRgb()), 144);
}

void drawDetail(int i) {
  const auto& p = theme::palette();
  const auto& fx = teensylink::state().fx[i];
  setCanvas(kVisible);
  fillRect(0, kStripTop, kW - 1, kStripBottom, rgb565(p.background));
  fillRoundRect({60, 164, 1200, 606}, 18, rgb565(p.surface));
  char title[48];
  snprintf(title, sizeof(title), choosingEffect ? "Add effect to track %d" : "Track %d effects", i + 1);
  drawText(90, 184, title, fontLarge(), rgb565(p.text), rgb565(p.surface));
  if (choosingEffect) {
    drawButton(flangeBtn(), "Flange", fontLarge(), rgb565(p.text), rgb565(fx.canAddFlange ? p.accent : p.background));
    drawButton(chorusBtn(), "Chorus", fontLarge(), rgb565(p.text), rgb565(fx.canAddChorus ? p.accent : p.background));
    drawButton(closeRect(), "Back", fontLarge(), rgb565(p.text), rgb565(p.background));
    return;
  }
  bool addShown = false;
  for (int slot = 0; slot < kNumSlots; ++slot) {
    int type = fx.known ? fx.types[slot] : -1;
    bool canAdd = type < 0 && !addShown && fx.known && (fx.canAddFlange || fx.canAddChorus);
    const char* label = type >= 0 ? fxName(type) : (canAdd ? "Add effect" : "Empty");
    if (canAdd) addShown = true;
    drawButton(slotRect(i, slot), label, fontLarge(), rgb565(p.text),
               rgb565(type >= 0 && fx.activeSlot == slot ? p.accent : p.background));
  }
  drawWet(i);
  drawButton(bypassRect(i), fx.bypassed ? "Enable effects" : "Mute effects", fontLarge(), rgb565(p.text),
             rgb565(fx.bypassed ? p.background : p.accent));
  drawButton(closeRect(), "Done", fontLarge(), rgb565(p.text), rgb565(p.background));
}

void drawFxSummary(int i) {
  setCanvas(kVisible);
  const auto& p = theme::palette();
  const auto& fx = teensylink::state().fx[i];
  int x = stripX(i);
  drawText(x + 20, 516, fx.bypassed ? "Effects muted" : "Effects on", fontSmall(), rgb565(p.muted), rgb565(p.surface), 260);
}

void drawStrip(int i) {
  int x = stripX(i);
  setCanvas(kVisible);
  fillRoundRect({x, kStripTop, x + kStripW - 1, kStripBottom}, 18, rgb565(kPanelRgb()));
  trackicons::draw(i, x + 16, 174, rgb565(theme::palette().text), rgb565(kPanelRgb()));
  fillRect(x + 16, 212, x + 51, 214, rgb565(kTrackRgb[i]));
  static const char* const names[] = {"Vocals", "Melody", "Bass", "Rhythm"};
  drawText(x + 64, 176, names[i], fontLarge(), rgb565(theme::palette().text), rgb565(kPanelRgb()));
  drawButton(plusRect(i), "Audio", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().accent));
  drawText(x + 12, 476, "Level", fontSmall(), rgb565(theme::palette().muted), rgb565(kPanelRgb()));
  drawText(x + 80, 476, "Peak", fontSmall(), rgb565(theme::palette().muted), rgb565(kPanelRgb()));
  drawButton(effectsRect(i), "Effects", fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().background));
  drawFader(i, true);
  drawMeter(i, true);
  drawFxSummary(i);
}

void redrawAll() {
  setCanvas(kVisible);
  fillRect(0, kStripTop, kW - 1, kStripBottom, rgb565(theme::palette().background));
  for (int i = 0; i < kNumTracks; ++i) drawStrip(i);
}

int wetFromX(int i, int x) {
  Rect r = wetRect(i);
  return constrain((int)((long)(x - r.x1) * 1000 / (r.x2 - r.x1)), 0, 1000);
}

}  // namespace

void redraw() {
  redrawAll();
  if(modalTrack>=0)drawDetail(modalTrack);
}

void enter() {
  modalTrack = -1;
  choosingEffect = false;
  dragWetTrack = -1;
  for (int i = 0; i < kNumTracks; ++i) {
    shownFader[i] = -1;
    shownPeak[i] = -1;
  }
  teensylink::takeChangedFx();
  teensylink::takeMixerChanged();
  redrawAll();
}

int touchDown(int x, int y) {
  const auto& st = teensylink::state();
  if (modalTrack >= 0) {
    int t = modalTrack;
    const auto& fx = st.fx[t];
    if (inRect(closeRect(), x, y)) {
      if (choosingEffect) { choosingEffect = false; drawDetail(t); }
      else { modalTrack = -1; redrawAll(); }
      return -1;
    }
    if (choosingEffect) {
      int type = inRect(flangeBtn(), x, y) && fx.canAddFlange ? 1 :
                 (inRect(chorusBtn(), x, y) && fx.canAddChorus ? 3 : -1);
      if (type >= 0) { teensylink::sendAddFx(t, type); choosingEffect = false; drawDetail(t); }
      return -1;
    }
    if (inRect(wetRect(t), x, y)) {
      dragWetTrack = t;
      localWet[t] = wetFromX(t, x);
      drawWet(t);
      teensylink::sendWet(t, localWet[t]);
      lastWetSendMs = millis();
    } else if (inRect(bypassRect(t), x, y)) {
      teensylink::sendBypass(t, !fx.bypassed);
    } else {
      for (int slot = 0; slot < kNumSlots; ++slot) {
        if (!inRect(slotRect(t, slot), x, y)) continue;
        if (fx.known && fx.types[slot] >= 0) teensylink::sendActiveSlot(t, slot);
        else if (fx.known && (fx.canAddFlange || fx.canAddChorus)) {
          bool firstEmpty = true;
          for (int j = 0; j < slot; ++j) if (fx.types[j] < 0) firstEmpty = false;
          if (firstEmpty) { choosingEffect = true; drawDetail(t); }
        }
        break;
      }
    }
    return -1;
  }
  for (int i = 0; i < kNumTracks; ++i) {
    if (inRect(plusRect(i), x, y)) return i;
    if (inRect(effectsRect(i), x, y)) { modalTrack = i; choosingEffect = false; drawDetail(i); return -1; }
  }
  return -1;
}

void touchMove(int x, int y) {
  (void)y;
  if (dragWetTrack < 0) return;
  int i = dragWetTrack;
  localWet[i] = wetFromX(i, x);
  drawWet(i);
  uint32_t now = millis();
  if (now - lastWetSendMs >= 80) {
    lastWetSendMs = now;
    teensylink::sendWet(i, localWet[i]);
  }
}

void touchUp() {
  if (dragWetTrack >= 0) {
    teensylink::sendWet(dragWetTrack, localWet[dragWetTrack]);
    localWet[dragWetTrack] = -1;
    dragWetTrack = -1;
  }
}

void update(bool visible) {
  uint8_t fxMask = teensylink::takeChangedFx();
  bool mixChanged = teensylink::takeMixerChanged();
  if (!visible) return;
  if (modalTrack >= 0) {
    if ((fxMask & (1 << modalTrack)) && dragWetTrack < 0) drawDetail(modalTrack);
    return;
  }
  for (int i = 0; i < kNumTracks; ++i) {
    if (fxMask & (1 << i)) {
      if (dragWetTrack == i) continue;
      drawFxSummary(i);
    }
  }
  if (mixChanged) {
    for (int i = 0; i < kNumTracks; ++i) {
      drawFader(i, false);
      drawMeter(i, false);
    }
  }
}

}  // namespace mixer
