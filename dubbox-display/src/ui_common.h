#pragma once
#include <stdint.h>
#include <gfxfont.h>
#include "ui_theme.h"

// Landscape UI on a 720x1280 portrait panel. Landscape (lx, ly) maps to panel
// (px, py) = (ly, 1279 - lx), verified on hardware with the ribbon on the right.
namespace ui {

constexpr int kW = 1280;
constexpr int kH = 720;
constexpr int kPanelW = 720;
constexpr int kPanelH = 1280;

constexpr int kNumTracks = 4;
constexpr int kHeaderH = 60;
constexpr int kTransportTop = 650;  // bottom row: PLAY / REWIND
constexpr int kTopBarTop = 61;      // strip under the header: clock and song slider
constexpr int kTopBarBottom = 102;

// Display RAM layers (panel-native layout, 720*1280*2 bytes each).
constexpr unsigned long kVisible = 0;
constexpr unsigned long kBright = 1843200;
constexpr unsigned long kDim = 3686400;
constexpr unsigned long kComposed = 5529600;

constexpr uint32_t kTrackRgb[kNumTracks] = {0x3d7fff, 0xff9f3d, 0x7fff3d, 0xff3d7f};

enum class Screen { Menu, NewProject, LoadProject, Mixer, Playlist, Pattern, FilePicker, Wifi, Settings, Stems };

struct Shared {
  bool linked = false;
  bool playing = false;
  uint32_t posMs = 0;
  uint32_t songMs = 8000;
  int volPermille = 1000;     // master headphone volume
  uint32_t volHoldUntil = 0;  // while millis() < this, the Teensy's value does not overwrite volPermille
  uint32_t posHoldUntil = 0;  // while millis() < this, the Teensy's position does not overwrite posMs
  int pattern = 0;            // the drum pattern being edited / placed (0-7)
};
extern Shared g;

struct Rect {
  int x1, y1, x2, y2;
};
inline bool inRect(const Rect& r, int x, int y) {
  return x >= r.x1 && x <= r.x2 && y >= r.y1 && y <= r.y2;
}

uint16_t rawRgb565(uint32_t rgb, float scale = 1.0f);
uint16_t rgb565(uint32_t rgb, float scale = 1.0f);
void setCanvas(unsigned long layerAddr);
void fillRect(int lx1, int ly1, int lx2, int ly2, uint16_t color);
void bteCopy(unsigned long src, unsigned long dst, int lx, int ly, int lw, int lh);

const GFXfont* fontSmall();  // 12 pt bold
const GFXfont* fontLarge();  // 18 pt bold
int textWidth(const GFXfont* f, const char* s);
void drawText(int x, int yTop, const char* s, const GFXfont* f, uint16_t fg, uint16_t bg,
              int minWidth = 0);
// Fills `r` with `fill` and centres `label` inside it.
void drawButton(const Rect& r, const char* label, const GFXfont* f, uint16_t fg, uint16_t fill);

// Header bar: DUB-BOX and a title on the left, link tag and volume slider in the middle, and on
// the right either the three coloured tabs (MIXER / PLAYLIST / PATTERN, `activeTab` lit) plus a
// MENU button, or, with `activeTab` < 0, a single button labelled `buttonLabel` ("BACK" reports
// HeaderHit::Back, anything else HeaderHit::Menu; nullptr = no button).
enum class HeaderHit { None, TabMixer, TabPlaylist, TabPattern, Menu, Back, Volume };
enum Tab { kTabMixer = 0, kTabPlaylist = 1, kTabPattern = 2 };
void drawHeader(const char* title, int activeTab, const char* buttonLabel = nullptr);
HeaderHit headerHit(int x, int y);
void drawLinkTag();
void drawBevel(const Rect& r, bool inset = false);
// Master headphone volume slider in the header, next to the link tag.
int volumeFromX(int x);
void drawVolume(bool force = false);
// Applies a volume from a touch x position: updates the slider and sends it to the Teensy.
void setVolumeFromX(int x, bool final);

// Transport (bottom PLAY/PAUSE + REWIND, top clock + slider) shared by mixer and playlist.
// With `splitPlay` (the Pattern screen) the bottom row is: PATTERN (loops the current pattern on
// its own), REC (a one-bar count-in, then records pad presses / notes into it while it loops),
// CLICK (metronome on/off), the pad-mode toggle (pads play drums or piano notes), PLAY SONG, REWIND.
enum class TransportHit { None, Play, PlayPattern, Record, ClickToggle, PadMode, Rewind, Slider };
void drawTransport(bool splitPlay = false);
TransportHit transportHit(int x, int y);
void drawClock(bool force = false);
// Song slider under the header (top right): position <-> x mapping and redraw.
uint32_t sliderMsForX(int x);
void drawSlider(bool force = false);
// Moves the playhead: sends a seek to the Teensy (or moves the demo clock when offline).
void seekToMs(uint32_t ms);
// True for the bottom button row and the top clock/slider strip.
bool inTransportArea(int x, int y);

}  // namespace ui
