#include "theme.h"
#include "ui_common.h"
#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "LCD.h"
#include "screen_mirror.h"
#include "teensy_link.h"

namespace ui {

Shared g;

namespace {
bool s_drawingGlyph = false;
bool s_showTabs = false;
bool s_showButton = false;
bool s_buttonIsBack = false;
int s_lastClockTenths = -1;

uint32_t kHeaderBg() { return theme::palette().surface; }
constexpr Rect kTabRects[4] = {{720, 0, 804, 99}, {810, 0, 934, 99}, {940, 0, 1064, 99}, {1070, 0, 1152, 99}};
constexpr const char* kTabLabels[4] = {"Mixer", "Playlist", "Pattern", "Input"};
constexpr Rect kMenuBtn = {1160, 0, 1279, 99};
constexpr int kTitleX = 215;
constexpr int kTitleMaxW = 200;
constexpr int kTagX = 430;
constexpr int kVolLabelX = 548;
constexpr Rect kVolSlider = {606, 0, 706, 99};  // touch and drawing area
int s_lastVolKnobX = -1;
uint32_t s_lastVolSendMs = 0;
constexpr Rect kSlider = {660, 108, 1240, 150};
constexpr Rect kPlayBtn = {40, kTransportTop, 330, 719};
constexpr Rect kRewindBtn = {950, kTransportTop, 1240, 719};
constexpr Rect kPatternPlayBtn = {40, kTransportTop, 240, 719};
constexpr Rect kRecBtn = {246, kTransportTop, 386, 719};
constexpr Rect kClickBtn = {392, kTransportTop, 512, 719};
constexpr Rect kPadModeBtn = {518, kTransportTop, 678, 719};
constexpr Rect kSongPlayBtn = {684, kTransportTop, 944, 719};
constexpr Rect kSplitRewindBtn = {950, kTransportTop, 1240, 719};
bool s_splitPlay = false;
constexpr int kClockX = 40;
constexpr int kClockY = 108;
// Scratch layer for flicker-free drawing: rows above the lanes are unused in kDim.
constexpr unsigned long kScratch = kDim;
uint32_t kTransportBg() { return theme::palette().surface; }
int s_lastKnobX = -1;
}  // namespace

uint16_t rgb565(uint32_t rgb, float scale) {
  uint32_t r = (uint32_t)(((rgb >> 16) & 0xFF) * scale);
  uint32_t gr = (uint32_t)(((rgb >> 8) & 0xFF) * scale);
  uint32_t b = (uint32_t)((rgb & 0xFF) * scale);
  return (uint16_t)(((r & 0xF8) << 8) | ((gr & 0xFC) << 3) | (b >> 3));
}

static bool s_staged = false;
static unsigned long s_canvas = kVisible;
static inline unsigned long redirect(unsigned long addr) { return (s_staged && addr == kVisible) ? kStaging : addr; }

void setCanvas(unsigned long addr) {
  s_canvas = addr;
  mirror::canvas(redirect(addr));
  ER5517.Canvas_Image_Start_address(redirect(addr));
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
  if (!s_drawingGlyph) mirror::rect(lx1, ly1, lx2, ly2, color);
  int px, py, pw, ph;
  toPanel(lx1, ly1, lx2, ly2, px, py, pw, ph);
  ER5517.DrawSquare_Fill(px, py, px + pw - 1, py + ph - 1, color);
}

void fillRoundRect(const Rect& r, int radius, uint16_t color) {
  if (r.x2 < r.x1 || r.y2 < r.y1) return;
  int maxRadius = min((r.x2-r.x1+1)/2, (r.y2-r.y1+1)/2);
  int rad = min(max(radius, 0), maxRadius);
  if (!rad) { fillRect(r.x1,r.y1,r.x2,r.y2,color); return; }
  mirror::roundRect(r.x1,r.y1,r.x2,r.y2,rad,color);
  bool previousSuppression = s_drawingGlyph;
  s_drawingGlyph = true;
  fillRect(r.x1,r.y1+rad,r.x2,r.y2-rad,color);
  // Merge equal-width scanlines to keep SPI and mirror traffic bounded.
  int start=0, lastInset=-1;
  for (int y=0;y<=rad;++y) {
    int dy=rad-y;
    int inset=y==rad ? -1 : rad-int(sqrtf(float(rad*rad-dy*dy)));
    if (inset!=lastInset && y>start) {
      fillRect(r.x1+lastInset,r.y1+start,r.x2-lastInset,r.y1+y-1,color);
      fillRect(r.x1+lastInset,r.y2-y+1,r.x2-lastInset,r.y2-start,color);
      start=y;
    }
    lastInset=inset;
  }
  s_drawingGlyph = previousSuppression;
}

void bteCopy(unsigned long src, unsigned long dst, int lx, int ly, int lw, int lh) {
  if (lw <= 0 || lh <= 0) return;
  src = redirect(src);
  dst = redirect(dst);
  mirror::copy(src, dst, lx, ly, lw, lh);
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

ScopedRedraw::ScopedRedraw(const Rect& region) : region_(region), owns_(false) {
  region_.x1=max(0,region_.x1); region_.y1=max(0,region_.y1);
  region_.x2=min(kW-1,region_.x2); region_.y2=min(kH-1,region_.y2);
  if (s_staged || s_canvas != kVisible || region_.x2<region_.x1 || region_.y2<region_.y1) return;
  bteCopy(kVisible,kStaging,region_.x1,region_.y1,region_.x2-region_.x1+1,region_.y2-region_.y1+1);
  owns_=true; s_staged=true; setCanvas(kVisible);
}
ScopedRedraw::~ScopedRedraw() {
  if (!owns_) return;
  s_staged=false;
  bteCopy(kStaging,kVisible,region_.x1,region_.y1,region_.x2-region_.x1+1,region_.y2-region_.y1+1);
  setCanvas(kVisible);
}
void beginStagedDraw() { s_staged = true; }

void endStagedDraw() { s_staged = false; setCanvas(kVisible); }

void presentStagedDraw() {
  s_staged = false;
  bteCopy(kStaging, kVisible, 0, 0, kW, kH);
  setCanvas(kVisible);
}

const GFXfont* fontSmall() { return &FreeSans12pt7b; }
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
  mirror::glyph(f == fontLarge() ? 1 : 0, static_cast<unsigned char>(c), x, baseline, fg);
  s_drawingGlyph = true;
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
  s_drawingGlyph = false;
}

// Evaluate the actual RGB565 pair, including scaled legacy track colors. Preserve
// semantic colors when readable; otherwise choose a contrasting neutral label.
static float luminance(uint16_t c) {
  float channels[] = {float((c >> 11) & 31) / 31, float((c >> 5) & 63) / 63, float(c & 31) / 31};
  for (float& v : channels) v = v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
  return channels[0] * 0.2126f + channels[1] * 0.7152f + channels[2] * 0.0722f;
}
static uint16_t readableText(uint16_t fg, uint16_t bg) {
  float a = luminance(fg), b = luminance(bg);
  float ratio = (fmaxf(a, b) + 0.05f) / (fminf(a, b) + 0.05f);
  if (ratio >= 4.5f) return fg;
  return (b + 0.05f) / 0.05f >= 1.05f / (b + 0.05f) ? 0x0000 : 0xffff;
}

void drawText(int x, int yTop, const char* s, const GFXfont* f, uint16_t fg, uint16_t bg,
              int minWidth) {
  fg = readableText(fg, bg);
  int w = textWidth(f, s);
  if (w < minWidth) w = minWidth;
  ScopedRedraw redraw({x,yTop,x+w,yTop+f->yAdvance});
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
  ScopedRedraw redraw(r);
  fg = readableText(fg, fill);
  fillRoundRect(r, 14, fill);
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
  drawText(kTagX, 36, g.linked ? "Connected" : "Offline", fontSmall(),
           rgb565(g.linked ? theme::palette().accent : theme::palette().muted), rgb565(kHeaderBg()), 106);
}

// A filled box with a border, like the PLAY button: bright enough to stand out from the dark header.
static void drawBox(const Rect& r, const char* label, uint32_t fillRgb, float fillScale, uint32_t borderRgb,
                    uint32_t textRgb) {
  drawButton(r, label, fontSmall(), rgb565(textRgb), rgb565(fillRgb, fillScale));

}

static void drawTab(int i, bool active) {
  const auto& p = theme::palette();
  drawBox(kTabRects[i], kTabLabels[i], active ? p.accent : p.surface,
          1.0f, active ? p.accent : p.border, p.text);
}

void drawHeader(const char* title, int activeTab, const char* buttonLabel) {
  s_showTabs = activeTab >= 0;
  s_showButton = s_showTabs || buttonLabel != nullptr;
  s_buttonIsBack = !s_showTabs && buttonLabel != nullptr && strcmp(buttonLabel, "BACK") == 0;
  setCanvas(kVisible);
  fillRect(0, 0, kW - 1, kHeaderH - 1, rgb565(kHeaderBg()));
  fillRect(24, kHeaderH - 1, kW - 25, kHeaderH - 1, rgb565(theme::palette().border));
  drawText(20, 32, "Studio", fontLarge(), rgb565(theme::palette().text), rgb565(kHeaderBg()));
  char clipped[32];
  strncpy(clipped, title != nullptr ? title : "", sizeof(clipped) - 1);
  clipped[sizeof(clipped) - 1] = '\0';
  while (clipped[0] != '\0' && textWidth(fontSmall(), clipped) > kTitleMaxW) {
    clipped[strlen(clipped) - 1] = '\0';
  }
  drawText(kTitleX, 36, clipped, fontSmall(), rgb565(theme::palette().accent), rgb565(kHeaderBg()));
  drawLinkTag();
  drawText(kVolLabelX, 36, "Vol", fontSmall(), rgb565(theme::palette().muted), rgb565(kHeaderBg()));
  drawVolume(true);
  setCanvas(kVisible);  // drawVolume() leaves the scratch layer selected
  if (s_showTabs) {
    for (int i = 0; i < 4; ++i) drawTab(i, i == activeTab);
  }
  if (s_showButton) {
    drawBox(kMenuBtn, s_showTabs ? "MENU" : buttonLabel, theme::palette().surface, 1.0f, theme::palette().border, theme::palette().text);
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
  fillRect(kVolSlider.x1 - 12, kVolSlider.y1, kVolSlider.x2 + 12, kVolSlider.y2, rgb565(kHeaderBg()));
  int barTop = (kVolSlider.y1 + kVolSlider.y2) / 2 - 4;
  fillRect(kVolSlider.x1, barTop, kVolSlider.x2, barTop + 8, rgb565(theme::palette().border));
  fillRect(kVolSlider.x1, barTop, x, barTop + 8, rgb565(theme::palette().accent));
  fillRoundRect({x - 10, 35, x + 10, 65}, 10, rgb565(theme::palette().text));
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
    if (inRect(kTabRects[3], x, y)) return HeaderHit::TabInput;
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
  drawText(kClockX, kClockY, buf, fontLarge(), rgb565(theme::palette().text), rgb565(kTransportBg()), 290);
  bteCopy(kScratch, kVisible, kClockX, kClockY, 291, 44);
}

void drawTransport(bool splitPlay) {
  s_splitPlay = splitPlay;
  setCanvas(kVisible);
  ScopedRedraw redraw({0,kTopBarTop,kW-1,kH-1});
  fillRect(0, kTopBarTop, kW - 1, kTopBarBottom, rgb565(kTransportBg()));
  fillRect(0, kTransportTop - 4, kW - 1, kH - 1, rgb565(kTransportBg()));
  if (splitPlay) {
    const teensylink::Drums& d = teensylink::state().drums;
    drawButton(kPatternPlayBtn, d.previewOn ? "Stop" : "Pattern", fontLarge(), rgb565(theme::palette().text),
               rgb565(d.previewOn ? theme::palette().secondary : theme::palette().accent));
    // REC: dark red when idle, orange while counting in, bright red with a white frame while recording.
    const uint32_t recFill = d.countIn ? 0xe08a00 : (d.recording ? 0xff2b2b : 0x8a1c1c);
    drawButton(kRecBtn, d.countIn ? "COUNT" : "REC", fontLarge(), rgb565(theme::palette().text), rgb565(recFill));
    if (d.recording || d.countIn) {
      const uint16_t w = rgb565(theme::palette().text);
      fillRect(kRecBtn.x1, kRecBtn.y1, kRecBtn.x2, kRecBtn.y1 + 3, w);
      fillRect(kRecBtn.x1, kRecBtn.y2 - 3, kRecBtn.x2, kRecBtn.y2, w);
      fillRect(kRecBtn.x1, kRecBtn.y1, kRecBtn.x1 + 3, kRecBtn.y2, w);
      fillRect(kRecBtn.x2 - 3, kRecBtn.y1, kRecBtn.x2, kRecBtn.y2, w);
    }
    drawButton(kClickBtn, d.clickOn ? "Click" : "Click off", fontSmall(), rgb565(d.clickOn ? theme::palette().text : theme::palette().muted),
               rgb565(d.clickOn ? theme::palette().accent : theme::palette().border));
    drawButton(kPadModeBtn, d.padMode == 1 ? "Note pads" : "Drum pads", fontSmall(), rgb565(theme::palette().text),
               rgb565(theme::palette().surface));
    drawButton(kSongPlayBtn, g.playing ? "Pause song" : "Play song", fontLarge(), rgb565(theme::palette().text),
               rgb565(g.playing ? theme::palette().secondary : theme::palette().accent));
    drawButton(kSplitRewindBtn, "Rewind", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  } else {
    drawButton(kPlayBtn, g.playing ? "Pause" : "Play", fontLarge(), rgb565(theme::palette().text),
               rgb565(g.playing ? theme::palette().secondary : theme::palette().accent));
    drawButton(kRewindBtn, "Rewind", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
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
  fillRect(kSlider.x1 - 16, kSlider.y1, kSlider.x2 + 16, kSlider.y2, rgb565(kTransportBg()));
  int barTop = (kSlider.y1 + kSlider.y2) / 2 - 5;
  fillRect(kSlider.x1, barTop, kSlider.x2, barTop + 10, rgb565(theme::palette().border));
  fillRect(kSlider.x1, barTop, x, barTop + 10, rgb565(theme::palette().accent));
  fillRoundRect({x - 11, kSlider.y1 + 3, x + 11, kSlider.y2 - 3}, 11, rgb565(theme::palette().text));
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
