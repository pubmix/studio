#include "screen_mixer.h"
#include <Arduino.h>
#include <cmath>
#include "teensy_link.h"
#include "ui_common.h"

namespace mixer {
namespace {

using namespace ui;

constexpr int kStripTop = 106;
constexpr int kStripBottom = 642;
constexpr int kStripW = 300;
constexpr int kStripStride = 310;
constexpr int kStripLeft = 30;

constexpr int kBarTop = 164;
constexpr int kBarBottom = 480;
constexpr int kBarH = kBarBottom - kBarTop;

constexpr int kSlotH = 62;
constexpr int kSlotGap = 8;
constexpr int kNumSlots = 3;

const char* fxName(int type) {
  switch (type) {
    case 0: return "DELAY";
    case 1: return "FLANGE";
    case 2: return "REVERB";
    case 3: return "CHORUS";
    default: return "";
  }
}

constexpr uint32_t kPanelRgb = 0x2a2a2a;
constexpr uint32_t kBarBgRgb = 0x4a4a4a;
constexpr uint32_t kBoxRgb = 0x3d3d3d;
constexpr uint32_t kActiveRgb = 0xffaa00;

int stripX(int i) { return kStripLeft + i * kStripStride; }
Rect faderRect(int i) { return {stripX(i) + 16, kBarTop, stripX(i) + 66, kBarBottom}; }
Rect meterRect(int i) { return {stripX(i) + 80, kBarTop, stripX(i) + 120, kBarBottom}; }
int fxX(int i) { return stripX(i) + 140; }
Rect slotRect(int i, int slot) {
  int top = kBarTop + (kNumSlots - 1 - slot) * (kSlotH + kSlotGap);
  return {fxX(i), top, fxX(i) + 144, top + kSlotH};
}
Rect wetRect(int i) { return {fxX(i), 394, fxX(i) + 144, 434}; }
Rect plusRect(int i) { return {stripX(i) + 232, 114, stripX(i) + 288, 158}; }
Rect bypassRect(int i) { return {fxX(i), 484, fxX(i) + 144, 544}; }

// Local state shown on screen, so redraws only touch what changed.
int shownFader[kNumTracks] = {-1, -1, -1, -1};
int shownPeak[kNumTracks] = {-1, -1, -1, -1};
uint32_t shownDuration[kNumTracks] = {};

int modalTrack = -1;
int dragWetTrack = -1;
uint32_t lastWetSendMs = 0;
int localWet[kNumTracks] = {-1, -1, -1, -1};  // wet shown while dragging

int shownPan[kNumTracks] = {2000,2000,2000,2000};
int dragPan = -1, panStartX = 0, panStartValue = 0, localPan = 0;
uint32_t lastPanSend = 0;
Rect panRect(int i) { return {stripX(i)+10, 520, stripX(i)+128, 608}; }
Rect centerRect(int i) { return {stripX(i)+10, 610, stripX(i)+128, 637}; }
void drawPan(int i, bool force=false) {
  int value = dragPan == i ? localPan : teensylink::state().panPermille[i];
  if (!force && shownPan[i] == value) return;
  shownPan[i] = value;
  auto r = panRect(i);
  setCanvas(kVisible);
  fillRect(r.x1,r.y1,r.x2,r.y2,rgb565(kPanelRgb));
  int cx=stripX(i)+69, cy=575;
  for (int y=-27;y<=27;++y) {
    int w=(int)sqrtf(27*27-y*y);
    fillRect(cx-w,cy+y,cx+w,cy+y,rgb565(kBoxRgb));
  }
  float angle = value/1000.0f*2.35f;
  for(int d=7;d<=25;++d) {
    int x=cx+(int)(sinf(angle)*d), y=cy-(int)(cosf(angle)*d);
    fillRect(x-1,y-1,x+1,y+1,rgb565(kActiveRgb));
  }
  char label[20];
  if (!value) snprintf(label,sizeof(label),"PAN C");
  else snprintf(label,sizeof(label),"%c %d%%",value<0?'L':'R',abs(value)/10);
  drawText(cx-textWidth(fontSmall(),label)/2,522,label,fontSmall(),rgb565(0xdddddd),rgb565(kPanelRgb));
  drawButton(centerRect(i),"CENTER",fontSmall(),rgb565(0xffffff),rgb565(kBoxRgb));
}

uint16_t meterColor(float peak) {
  if (peak >= 0.708f) return rgb565(0xe53935);  // -3 dBFS and above
  if (peak >= 0.251f) return rgb565(0xffa726);  // -12..-3 dBFS
  return rawRgb565(theme::palette().accent);
}

int shownFillFader[kNumTracks] = {-1, -1, -1, -1};
int shownFillMeter[kNumTracks] = {-1, -1, -1, -1};
uint16_t shownMeterColor[kNumTracks] = {0, 0, 0, 0};

// Redraws only the slice of a vertical bar that changed, so bars don't flicker.
void drawBar(const Rect& r, int fillH, int& shownFill, uint16_t fillColor, uint16_t bgColor,
             bool fullRedraw, bool segmented = false) {
  setCanvas(kVisible);
  if (fullRedraw || shownFill < 0) {
    fillRect(r.x1, r.y1, r.x2, r.y2 - fillH, bgColor);
    fillRect(r.x1, r.y2 - fillH + 1, r.x2, r.y2, fillColor);
  } else if (fillH > shownFill) {
    fillRect(r.x1, r.y2 - fillH + 1, r.x2, r.y2 - shownFill, fillColor);
  } else if (fillH < shownFill) {
    fillRect(r.x1, r.y2 - shownFill + 1, r.x2, r.y2 - fillH, bgColor);
  }
  if (segmented) for (int y = r.y1; y <= r.y2; y += 12)
    fillRect(r.x1, y, r.x2, y + 2, bgColor);
  shownFill = fillH;
}

void drawFader(int i, bool force) {
  const teensylink::State& st = teensylink::state();
  int v = st.faderPermille[i];
  if (!force && abs(v - shownFader[i]) < 8) return;
  shownFader[i] = v;
  if (force) shownFillFader[i] = -1;
  int fillH = (int)((long)kBarH * v / 1000);
  drawBar(faderRect(i), fillH, shownFillFader[i], rgb565(0xffffff), rgb565(kBarBgRgb), false);
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
  drawBar(meterRect(i), fillH, shownFillMeter[i], color, rgb565(kBarBgRgb), colorChanged, true);
}

void drawWet(int i) {
  const teensylink::FxInfo& fx = teensylink::state().fx[i];
  int wet = (dragWetTrack == i && localWet[i] >= 0) ? localWet[i] : fx.wetPermille;
  Rect r = wetRect(i);
  setCanvas(kVisible);
  int w = r.x2 - r.x1;
  int fillW = (int)((long)w * wet / 1000);
  fillRect(r.x1, r.y1, r.x1 + fillW, r.y2, rgb565(kActiveRgb));
  fillRect(r.x1 + fillW + 1, r.y1, r.x2, r.y2, rgb565(kBarBgRgb));
  char buf[16];
  snprintf(buf, sizeof(buf), "WET %d%%", wet / 10);
  drawText(fxX(i), 440, buf, fontSmall(), rgb565(0xdddddd), rgb565(kPanelRgb), 144);
}

void drawFxColumn(int i) {
  const teensylink::FxInfo& fx = teensylink::state().fx[i];
  setCanvas(kVisible);
  bool addShown = false;
  for (int slot = 0; slot < kNumSlots; ++slot) {
    Rect r = slotRect(i, slot);
    int type = fx.known ? fx.types[slot] : -1;
    if (type >= 0) {
      bool active = fx.activeSlot == slot;
      if (active) {
        fillRect(r.x1, r.y1, r.x2, r.y2, rgb565(kActiveRgb));
        fillRect(r.x1 + 4, r.y1 + 4, r.x2 - 4, r.y2 - 4, rgb565(kBoxRgb));
      } else {
        fillRect(r.x1, r.y1, r.x2, r.y2, rgb565(kBoxRgb));
      }
      const char* name = fxName(type);
      int w = textWidth(fontSmall(), name);
      drawText((r.x1 + r.x2) / 2 - w / 2, (r.y1 + r.y2) / 2 - 14, name, fontSmall(),
               rgb565(fx.bypassed ? 0x777777 : 0xffffff), rgb565(kBoxRgb));
    } else if (!addShown && fx.known && (fx.canAddFlange || fx.canAddChorus)) {
      addShown = true;
      drawButton(r, "+", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3a));
    } else {
      fillRect(r.x1, r.y1, r.x2, r.y2, rgb565(kPanelRgb));
    }
  }
  drawWet(i);
  drawButton(bypassRect(i), fx.bypassed ? "MUTED" : "FX ON", fontSmall(), rgb565(0xffffff),
             rgb565(fx.bypassed ? 0x8a2b2b : 0x1f5a2e));
}

void drawTrackInfo(int i) {
  setCanvas(kVisible);
  // Metadata supplied by the existing Teensy track state, never fabricated.
  const auto& track = teensylink::state().track[i];
  shownDuration[i] = track.fileLenMs;
  char duration[20];
  if (track.fileLenMs) snprintf(duration, sizeof(duration), "%02lu:%02lu WAV",
      (unsigned long)(track.fileLenMs/60000), (unsigned long)((track.fileLenMs/1000)%60));
  else snprintf(duration, sizeof(duration), "NO AUDIO");
  drawText(fxX(i),560,duration,fontSmall(),rgb565(0x999999),rgb565(kPanelRgb),144);
}

void drawStrip(int i) {
  int x = stripX(i);
  setCanvas(kVisible);
  fillRect(x, kStripTop, x + kStripW - 1, kStripBottom, rgb565(kPanelRgb));
  fillRect(x, kStripTop, x + kStripW - 1, kStripTop + 7, rgb565(kTrackRgb[i]));
  drawBevel({x, kStripTop, x + kStripW - 1, kStripBottom});
  char name[16];
  snprintf(name, sizeof(name), "TRACK %d", i + 1);
  drawText(x + 14, 118, name, fontLarge(), rgb565(kTrackRgb[i]), rgb565(kPanelRgb));
  drawButton(plusRect(i), "+", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3d));
  drawText(x + 12, 490, "LVL", fontSmall(), rgb565(0x999999), rgb565(kPanelRgb));
  drawText(x + 80, 490, "PK", fontSmall(), rgb565(0x999999), rgb565(kPanelRgb));
  drawText(x + 140, 598, "EFFECTS", fontSmall(), rgb565(0x999999), rgb565(kPanelRgb));
  drawPan(i, true);
  drawTrackInfo(i);
  drawFader(i, true);
  drawMeter(i, true);
  drawFxColumn(i);
}

Rect modalPanel() { return {340, 150, 940, 560}; }
Rect flangeBtn() { return {390, 250, 890, 330}; }
Rect chorusBtn() { return {390, 350, 890, 430}; }
Rect cancelBtn() { return {390, 460, 890, 530}; }

void drawModal(int track) {
  const teensylink::FxInfo& fx = teensylink::state().fx[track];
  Rect p = modalPanel();
  setCanvas(kVisible);
  fillRect(p.x1, p.y1, p.x2, p.y2, rgb565(0xffffff));
  fillRect(p.x1 + 4, p.y1 + 4, p.x2 - 4, p.y2 - 4, rgb565(0x222222));
  char title[32];
  snprintf(title, sizeof(title), "ADD EFFECT - TRACK %d", track + 1);
  int w = textWidth(fontLarge(), title);
  drawText((p.x1 + p.x2) / 2 - w / 2, p.y1 + 24, title, fontLarge(), rgb565(0xffffff),
           rgb565(0x222222));
  drawButton(flangeBtn(), "FLANGE", fontLarge(), rgb565(0xffffff),
             rgb565(fx.canAddFlange ? 0x1f7a3a : 0x444444));
  drawButton(chorusBtn(), "CHORUS", fontLarge(), rgb565(0xffffff),
             rgb565(fx.canAddChorus ? 0x1f7a3a : 0x444444));
  drawButton(cancelBtn(), "CANCEL", fontLarge(), rgb565(0xffffff), rgb565(0x8a2b2b));
}

void redrawAll() {
  for (int i = 0; i < kNumTracks; ++i) drawStrip(i);
}

int wetFromX(int i, int x) {
  Rect r = wetRect(i);
  return constrain((int)((long)(x - r.x1) * 1000 / (r.x2 - r.x1)), 0, 1000);
}

}  // namespace

void enter() {
  modalTrack = -1;
  dragPan = -1;
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
  const teensylink::State& st = teensylink::state();
  if (modalTrack >= 0) {
    int t = modalTrack;
    if (inRect(flangeBtn(), x, y) && st.fx[t].canAddFlange) {
      teensylink::sendAddFx(t, 1);
    } else if (inRect(chorusBtn(), x, y) && st.fx[t].canAddChorus) {
      teensylink::sendAddFx(t, 3);
    } else if (inRect(modalPanel(), x, y) && !inRect(cancelBtn(), x, y)) {
      return -1;  // tap on dead space inside the panel
    }
    modalTrack = -1;
    redrawAll();
    return -1;
  }

  for (int i = 0; i < kNumTracks; ++i) {
    if (inRect(centerRect(i),x,y)) {
      teensylink::sendPan(i,0);
      return -1;
    }
    if (inRect(panRect(i),x,y)) {
      dragPan=i; panStartX=x; panStartValue=st.panPermille[i]; localPan=panStartValue;
      lastPanSend=millis(); return -1;
    }
    const teensylink::FxInfo& fx = st.fx[i];
    if (inRect(wetRect(i), x, y)) {
      dragWetTrack = i;
      localWet[i] = wetFromX(i, x);
      drawWet(i);
      teensylink::sendWet(i, localWet[i]);
      lastWetSendMs = millis();
      return -1;
    }
    if (inRect(bypassRect(i), x, y)) {
      teensylink::sendBypass(i, !fx.bypassed);
      return -1;
    }
    for (int slot = 0; slot < kNumSlots; ++slot) {
      if (!inRect(slotRect(i, slot), x, y)) continue;
      if (fx.types[slot] >= 0) {
        teensylink::sendActiveSlot(i, slot);
      } else if (fx.known && (fx.canAddFlange || fx.canAddChorus)) {
        // The "+" sits in the first empty slot.
        bool firstEmpty = true;
        for (int s = 0; s < slot; ++s) {
          if (fx.types[s] < 0) firstEmpty = false;
        }
        if (firstEmpty) {
          modalTrack = i;
          drawModal(i);
        }
      }
      return -1;
    }
  }
  for (int i = 0; i < kNumTracks; ++i) {
    if (inRect(plusRect(i), x, y)) return i;
  }
  return -1;
}

void touchMove(int x, int y) {
  (void)y;
  if (dragPan >= 0) {
    localPan=constrain(panStartValue+(x-panStartX)*10,-1000,1000);
    if(abs(localPan)<30) localPan=0;
    drawPan(dragPan);
    if(millis()-lastPanSend>=50) { teensylink::sendPan(dragPan,localPan); lastPanSend=millis(); }
    return;
  }
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
  if(dragPan>=0) { teensylink::sendPan(dragPan,localPan); dragPan=-1; }
  if (dragWetTrack >= 0) {
    teensylink::sendWet(dragWetTrack, localWet[dragWetTrack]);
    localWet[dragWetTrack] = -1;
    dragWetTrack = -1;
  }
}

void update(bool visible) {
  uint8_t fxMask = teensylink::takeChangedFx();
  bool mixChanged = teensylink::takeMixerChanged();
  if (!visible || modalTrack >= 0) return;
  for (int i = 0; i < kNumTracks; ++i) {
    if (shownDuration[i] != teensylink::state().track[i].fileLenMs) drawTrackInfo(i);
    if (fxMask & (1 << i)) {
      if (dragWetTrack == i) continue;
      drawFxColumn(i);
    }
  }
  if (mixChanged) {
    for (int i = 0; i < kNumTracks; ++i) {
      drawPan(i);
      drawFader(i, false);
      drawMeter(i, false);
    }
  }
}

}  // namespace mixer
