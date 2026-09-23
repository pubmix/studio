#include "ui_common.h"
#include <Arduino.h>
#include <string.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "LCD.h"
#include "teensy_link.h"

namespace ui {

Shared g;

namespace {
bool s_showTabs = false;
bool s_showButton = false;
bool s_buttonIsBack = false;
int s_lastClockTenths = -1;

constexpr uint32_t kHeaderBg = 0x18202e;
constexpr Rect kTabRects[3] = {{722, 6, 836, 54}, {844, 6, 994, 54}, {1002, 6, 1152, 54}};
constexpr const char* kTabLabels[3] = {"MIXER", "PLAYLIST", "PATTERN"};
constexpr uint32_t kTabRgb[3] = {0x3d7fff, 0xff9f3d, 0xb45cff};
constexpr Rect kMenuBtn = {1160, 6, 1268, 54};
constexpr int kTitleX = 215;
constexpr int kTitleMaxW = 200;
constexpr int kTagX = 430;
constexpr int kVolLabelX = 548;
constexpr Rect kVolSlider = {606, 10, 706, 50};  // touch and drawing area
int s_lastVolKnobX = -1;
uint32_t s_lastVolSendMs = 0;
constexpr Rect kSlider = {660, 66, 1240, 98};
constexpr Rect kPlayBtn = {40, kTransportTop + 6, 620, kTransportTop + 64};
constexpr Rect kRewindBtn = {660, kTransportTop + 6, 1240, kTransportTop + 64};
constexpr Rect kPatternPlayBtn = {40, kTransportTop + 6, 240, kTransportTop + 64};
constexpr Rect kRecBtn = {246, kTransportTop + 6, 386, kTransportTop + 64};
constexpr Rect kClickBtn = {392, kTransportTop + 6, 512, kTransportTop + 64};
constexpr Rect kPadModeBtn = {518, kTransportTop + 6, 678, kTransportTop + 64};
constexpr Rect kSongPlayBtn = {684, kTransportTop + 6, 944, kTransportTop + 64};
constexpr Rect kSplitRewindBtn = {950, kTransportTop + 6, 1240, kTransportTop + 64};
bool s_splitPlay = false;
constexpr int kClockX = 40;
constexpr int kClockY = 61;
// Scratch layer for flicker-free drawing: rows above the lanes are unused in kDim.
constexpr unsigned long kScratch = kDim;
constexpr uint32_t kTransportBg = 0x1c1c1c;
int s_lastKnobX = -1;
}  // namespace

uint16_t rgb565(uint32_t rgb, float scale) {
  uint32_t r = (uint32_t)(((rgb >> 16) & 0xFF) * scale);
  uint32_t gr = (uint32_t)(((rgb >> 8) & 0xFF) * scale);
  uint32_t b = (uint32_t)((rgb & 0xFF) * scale);
  return (uint16_t)(((r & 0xF8) << 8) | ((gr & 0xFC) << 3) | (b >> 3));
}

void setCanvas(unsigned long addr) {
  ER5517.Canvas_Image_Start_address(addr);
  ER5517.Canvas_image_width(kPanelW);
  ER5517.Active_Window_XY(0, 0);
  ER5517.Active_Window_WH(kPanelW, kPanelH);
}

static void toPanel(int lx1, int ly1, int lx2, int ly2, int& px, int& py, int& pw, int& ph) {
  px = ly1;
  py = (kPanelH - 1) - lx2;
  pw = ly2 - ly1 + 1;
  ph = lx2 - lx1 + 1;
}

void fillRect(int lx1, int ly1, int lx2, int ly2, uint16_t color) {
  if (lx2 < lx1 || ly2 < ly1) return;
  int px, py, pw, ph;
  toPanel(lx1, ly1, lx2, ly2, px, py, pw, ph);
  ER5517.DrawSquare_Fill(px, py, px + pw - 1, py + ph - 1, color);
}

void bteCopy(unsigned long src, unsigned long dst, int lx, int ly, int lw, int lh) {
  if (lw <= 0 || lh <= 0) return;
  int px, py, pw, ph;
  toPanel(lx, ly, lx + lw - 1, ly + lh - 1, px, py, pw, ph);
  ER5517.BTE_S0_Color_16bpp();
  ER5517.BTE_S0_Memory_Start_Address(src);
  ER5517.BTE_S0_Image_Width(kPanelW);
  ER5517.BTE_S0_Window_Start_XY(px, py);
  ER5517.BTE_S1_Color_16bpp();
  ER5517.BTE_S1_Memory_Start_Address(src);
  ER5517.BTE_S1_Image_Width(kPanelW);
  ER5517.BTE_S1_Window_Start_XY(px, py);
  ER5517.BTE_Destination_Color_16bpp();
  ER5517.BTE_Destination_Memory_Start_Address(dst);
  ER5517.BTE_Destination_Image_Width(kPanelW);
  ER5517.BTE_Destination_Window_Start_XY(px, py);
  ER5517.BTE_ROP_Code(0x0C);
  ER5517.BTE_Operation_Code(0x02);
  ER5517.BTE_Window_Size(pw, ph);
  ER5517.BTE_Enable();
  ER5517.Check_BTE_Busy();
}

const GFXfont* fontSmall() { return &FreeSansBold12pt7b; }
const GFXfont* fontLarge() { return &FreeSansBold18pt7b; }

int textWidth(const GFXfont* f, const char* s) {
  int w = 0;
  for (; *s; ++s) {
    if (*s < f->first || *s > f->last) continue;
    w += f->glyph[*s - f->first].xAdvance;
  }
  return w;
}

static int fontAscent(const GFXfont* f) {
  const GFXglyph& h = f->glyph['H' - f->first];
  return -h.yOffset;
}

static void drawGlyph(const GFXfont* f, char c, int x, int baseline, uint16_t fg) {
  const GFXglyph& gl = f->glyph[c - f->first];
  const uint8_t* bitmap = f->bitmap + gl.bitmapOffset;
  uint8_t bits = 0;
  uint8_t bit = 0;
  int idx = 0;
  for (int yy = 0; yy < gl.height; ++yy) {
    int runStart = -1;
    for (int xx = 0; xx <= gl.width; ++xx) {
      bool on = false;
      if (xx < gl.width) {
        if (!(bit++ & 7)) bits = bitmap[idx++];
        on = (bits & 0x80) != 0;
        bits <<= 1;
      }
      if (on && runStart < 0) runStart = xx;
      if (!on && runStart >= 0) {
        int gx = x + gl.xOffset;
        int gy = baseline + gl.yOffset + yy;
        fillRect(gx + runStart, gy, gx + xx - 1, gy, fg);
        runStart = -1;
      }
    }
  }
}

void drawText(int x, int yTop, const char* s, const GFXfont* f, uint16_t fg, uint16_t bg,
              int minWidth) {
  int w = textWidth(f, s);
  if (w < minWidth) w = minWidth;
  int ascent = fontAscent(f);
  fillRect(x, yTop, x + w, yTop + f->yAdvance, bg);
  int cx = x;
  int baseline = yTop + ascent;
  for (const char* p = s; *p; ++p) {
    if (*p < f->first || *p > f->last) continue;
    drawGlyph(f, *p, cx, baseline, fg);
    cx += f->glyph[*p - f->first].xAdvance;
  }
}

void drawButton(const Rect& r, const char* label, const GFXfont* f, uint16_t fg, uint16_t fill) {
  fillRect(r.x1, r.y1, r.x2, r.y2, fill);
  int w = textWidth(f, label);
  int h = fontAscent(f);
  int tx = (r.x1 + r.x2) / 2 - w / 2;
  int ty = (r.y1 + r.y2) / 2 - h / 2 - (f->yAdvance - h) / 2 + 2;
  // Draw glyphs directly (no bg fill) so the button colour stays intact.
  int cx = tx;
  int baseline = ty + fontAscent(f);
  for (const char* p = label; *p; ++p) {
    if (*p < f->first || *p > f->last) continue;
    drawGlyph(f, *p, cx, baseline, fg);
    cx += f->glyph[*p - f->first].xAdvance;
  }
}

void drawLinkTag() {
  setCanvas(kVisible);
  drawText(kTagX, 16, g.linked ? "LINKED" : "OFFLINE", fontSmall(),
           rgb565(g.linked ? 0x35d07f : 0x999999), rgb565(kHeaderBg), 106);
}

// A filled box with a border, like the PLAY button: bright enough to stand out from the dark header.
static void drawBox(const Rect& r, const char* label, uint32_t fillRgb, float fillScale, uint32_t borderRgb,
                    uint32_t textRgb) {
  drawButton(r, label, fontSmall(), rgb565(textRgb), rgb565(fillRgb, fillScale));
  const uint16_t b = rgb565(borderRgb);
  fillRect(r.x1, r.y1, r.x2, r.y1 + 3, b);
  fillRect(r.x1, r.y2 - 3, r.x2, r.y2, b);
  fillRect(r.x1, r.y1, r.x1 + 3, r.y2, b);
  fillRect(r.x2 - 3, r.y1, r.x2, r.y2, b);
}

static void drawTab(int i, bool active) {
  if (active) {  // full colour, white frame, dark text
    drawBox(kTabRects[i], kTabLabels[i], kTabRgb[i], 1.0f, 0xffffff, 0x000000);
  } else {  // mid-bright fill with a border in the tab's own colour
    drawBox(kTabRects[i], kTabLabels[i], kTabRgb[i], 0.7f, kTabRgb[i], 0xffffff);
  }
}

void drawHeader(const char* title, int activeTab, const char* buttonLabel) {
  s_showTabs = activeTab >= 0;
  s_showButton = s_showTabs || buttonLabel != nullptr;
  s_buttonIsBack = !s_showTabs && buttonLabel != nullptr && strcmp(buttonLabel, "BACK") == 0;
  setCanvas(kVisible);
  fillRect(0, 0, kW - 1, kHeaderH - 1, rgb565(kHeaderBg));
  fillRect(0, kHeaderH - 4, kW - 1, kHeaderH - 1, rgb565(0x1f9d55));
  drawText(20, 10, "DUB-BOX", fontLarge(), rgb565(0xffffff), rgb565(kHeaderBg));
  char clipped[32];
  strncpy(clipped, title != nullptr ? title : "", sizeof(clipped) - 1);
  clipped[sizeof(clipped) - 1] = '\0';
  while (clipped[0] != '\0' && textWidth(fontSmall(), clipped) > kTitleMaxW) {
    clipped[strlen(clipped) - 1] = '\0';
  }
  drawText(kTitleX, 16, clipped, fontSmall(), rgb565(0x9df0c0), rgb565(kHeaderBg));
  drawLinkTag();
  drawText(kVolLabelX, 16, "VOL", fontSmall(), rgb565(0xaaaaaa), rgb565(kHeaderBg));
  drawVolume(true);
  setCanvas(kVisible);  // drawVolume() leaves the scratch layer selected
  if (s_showTabs) {
    for (int i = 0; i < 3; ++i) drawTab(i, i == activeTab);
  }
  if (s_showButton) {
    drawBox(kMenuBtn, s_showTabs ? "MENU" : buttonLabel, 0xd03a3a, 1.0f, 0xff8a80, 0xffffff);
  }
}

static int volKnobX() {
  return kVolSlider.x1 + (int)((long)(kVolSlider.x2 - kVolSlider.x1) * g.volPermille / 1000);
}

int volumeFromX(int x) {
  if (x < kVolSlider.x1) x = kVolSlider.x1;
  if (x > kVolSlider.x2) x = kVolSlider.x2;
  return (int)((long)(x - kVolSlider.x1) * 1000 / (kVolSlider.x2 - kVolSlider.x1));
}

void drawVolume(bool force) {
  int x = volKnobX();
  if (!force && x == s_lastVolKnobX) return;
  s_lastVolKnobX = x;
  setCanvas(kScratch);  // header rows are unused in this layer: compose here, copy in one step
  fillRect(kVolSlider.x1 - 12, kVolSlider.y1, kVolSlider.x2 + 12, kVolSlider.y2, rgb565(kHeaderBg));
  int barTop = (kVolSlider.y1 + kVolSlider.y2) / 2 - 4;
  fillRect(kVolSlider.x1, barTop, kVolSlider.x2, barTop + 8, rgb565(0x555555));
  fillRect(kVolSlider.x1, barTop, x, barTop + 8, rgb565(0x35d07f));
  fillRect(x - 8, kVolSlider.y1, x + 8, kVolSlider.y2, rgb565(0xffffff));
  bteCopy(kScratch, kVisible, kVolSlider.x1 - 12, kVolSlider.y1, kVolSlider.x2 - kVolSlider.x1 + 25,
          kVolSlider.y2 - kVolSlider.y1 + 1);
}

void setVolumeFromX(int x, bool final) {
  uint32_t now = millis();
  g.volPermille = volumeFromX(x);
  g.volHoldUntil = now + 600;
  if (g.linked && (final || now - s_lastVolSendMs >= 60)) {
    s_lastVolSendMs = now;
    teensylink::sendVolume(g.volPermille);
  }
  drawVolume();
}

HeaderHit headerHit(int x, int y) {
  if (y >= kHeaderH) return HeaderHit::None;
  if (x >= kVolSlider.x1 - 12 && x <= kVolSlider.x2 + 12) return HeaderHit::Volume;
  if (s_showTabs) {
    if (inRect(kTabRects[0], x, y)) return HeaderHit::TabMixer;
    if (inRect(kTabRects[1], x, y)) return HeaderHit::TabPlaylist;
    if (inRect(kTabRects[2], x, y)) return HeaderHit::TabPattern;
  }
  if (s_showButton && inRect(kMenuBtn, x, y)) return s_buttonIsBack ? HeaderHit::Back : HeaderHit::Menu;
  return HeaderHit::None;
}

void drawClock(bool force) {
  int tenths = (int)(g.posMs / 100);
  if (!force && tenths == s_lastClockTenths) return;
  s_lastClockTenths = tenths;
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f / %.1f s", g.posMs / 1000.0f, g.songMs / 1000.0f);
  setCanvas(kScratch);
  drawText(kClockX, kClockY, buf, fontLarge(), rgb565(0xffffff), rgb565(kTransportBg), 290);
  bteCopy(kScratch, kVisible, kClockX, kClockY, 291, 44);
}

void drawTransport(bool splitPlay) {
  s_splitPlay = splitPlay;
  setCanvas(kVisible);
  fillRect(0, kTopBarTop, kW - 1, kTopBarBottom, rgb565(kTransportBg));
  fillRect(0, kTransportTop - 4, kW - 1, kH - 1, rgb565(kTransportBg));
  if (splitPlay) {
    const teensylink::Drums& d = teensylink::state().drums;
    drawButton(kPatternPlayBtn, d.previewOn ? "STOP" : "PATTERN", fontLarge(), rgb565(0xffffff),
               rgb565(d.previewOn ? 0xc06a00 : 0x7a3ec0));
    // REC: dark red when idle, orange while counting in, bright red with a white frame while recording.
    const uint32_t recFill = d.countIn ? 0xe08a00 : (d.recording ? 0xff2b2b : 0x8a1c1c);
    drawButton(kRecBtn, d.countIn ? "COUNT" : "REC", fontLarge(), rgb565(0xffffff), rgb565(recFill));
    if (d.recording || d.countIn) {
      const uint16_t w = rgb565(0xffffff);
      fillRect(kRecBtn.x1, kRecBtn.y1, kRecBtn.x2, kRecBtn.y1 + 3, w);
      fillRect(kRecBtn.x1, kRecBtn.y2 - 3, kRecBtn.x2, kRecBtn.y2, w);
      fillRect(kRecBtn.x1, kRecBtn.y1, kRecBtn.x1 + 3, kRecBtn.y2, w);
      fillRect(kRecBtn.x2 - 3, kRecBtn.y1, kRecBtn.x2, kRecBtn.y2, w);
    }
    drawButton(kClickBtn, d.clickOn ? "CLICK" : "NO CLICK", fontSmall(), rgb565(d.clickOn ? 0xffffff : 0xaaaaaa),
               rgb565(d.clickOn ? 0x1f7a3d : 0x444444));
    drawButton(kPadModeBtn, d.padMode == 1 ? "NOTE PADS" : "DRUM PADS", fontSmall(), rgb565(0xffffff),
               rgb565(d.padMode == 1 ? 0x5a3aa0 : 0x9a5a14));
    drawButton(kSongPlayBtn, g.playing ? "PAUSE SONG" : "PLAY SONG", fontLarge(), rgb565(0xffffff),
               rgb565(g.playing ? 0x8a5a00 : 0x1f7a3a));
    drawButton(kSplitRewindBtn, "REWIND", fontLarge(), rgb565(0xffffff), rgb565(0x333333));
  } else {
    drawButton(kPlayBtn, g.playing ? "PAUSE" : "PLAY", fontLarge(), rgb565(0xffffff),
               rgb565(g.playing ? 0x8a5a00 : 0x1f7a3a));
    drawButton(kRewindBtn, "REWIND", fontLarge(), rgb565(0xffffff), rgb565(0x333333));
  }
  drawClock(true);
  drawSlider(true);
}

static int knobX() {
  if (g.songMs == 0) return kSlider.x1;
  uint32_t pos = g.posMs > g.songMs ? g.songMs : g.posMs;
  return kSlider.x1 + (int)((uint64_t)(kSlider.x2 - kSlider.x1) * pos / g.songMs);
}

uint32_t sliderMsForX(int x) {
  if (x < kSlider.x1) x = kSlider.x1;
  if (x > kSlider.x2) x = kSlider.x2;
  return (uint32_t)((uint64_t)g.songMs * (x - kSlider.x1) / (kSlider.x2 - kSlider.x1));
}

void drawSlider(bool force) {
  int x = knobX();
  if (!force && x == s_lastKnobX) return;
  s_lastKnobX = x;
  setCanvas(kScratch);
  fillRect(kSlider.x1 - 16, kSlider.y1, kSlider.x2 + 16, kSlider.y2, rgb565(kTransportBg));
  int barTop = (kSlider.y1 + kSlider.y2) / 2 - 5;
  fillRect(kSlider.x1, barTop, kSlider.x2, barTop + 10, rgb565(0x555555));
  fillRect(kSlider.x1, barTop, x, barTop + 10, rgb565(0x00e5ff));
  fillRect(x - 11, kSlider.y1, x + 11, kSlider.y2, rgb565(0xffffff));
  bteCopy(kScratch, kVisible, kSlider.x1 - 16, kSlider.y1, kSlider.x2 - kSlider.x1 + 33,
          kSlider.y2 - kSlider.y1 + 1);
}

void seekToMs(uint32_t ms) {
  if (ms > g.songMs) ms = g.songMs;
  g.posMs = ms;
  g.posHoldUntil = millis() + 450;
  if (g.linked) teensylink::sendSeek(ms);
}

bool inTransportArea(int x, int y) {
  (void)x;
  return y >= kTransportTop - 4 || (y >= kTopBarTop && y <= kTopBarBottom);
}

TransportHit transportHit(int x, int y) {
  if (x >= kSlider.x1 - 24 && x <= kSlider.x2 + 24 && y >= kSlider.y1 - 4 && y <= kSlider.y2 + 4) {
    return TransportHit::Slider;
  }
  if (s_splitPlay) {
    if (inRect(kPatternPlayBtn, x, y)) return TransportHit::PlayPattern;
    if (inRect(kRecBtn, x, y)) return TransportHit::Record;
    if (inRect(kClickBtn, x, y)) return TransportHit::ClickToggle;
    if (inRect(kPadModeBtn, x, y)) return TransportHit::PadMode;
    if (inRect(kSongPlayBtn, x, y)) return TransportHit::Play;
    if (inRect(kSplitRewindBtn, x, y)) return TransportHit::Rewind;
  } else {
    if (inRect(kPlayBtn, x, y)) return TransportHit::Play;
    if (inRect(kRewindBtn, x, y)) return TransportHit::Rewind;
  }
  return TransportHit::None;
}

}  // namespace ui
