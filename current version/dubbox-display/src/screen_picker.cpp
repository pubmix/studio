#include "theme.h"
#include "screen_picker.h"
#include <Arduino.h>
#include <string.h>
#include "teensy_link.h"

namespace picker {
namespace {

using namespace ui;

constexpr int kCols = 3;
constexpr int kRows = 3;
constexpr int kColW = 388;
constexpr int kRowH = 102;
constexpr int kRowStep = 112;
constexpr int kTop = 150;
constexpr int kLeft = 40;
constexpr int kColStep = 410;
constexpr int kMaxShownChars = 15;

constexpr Rect kRemove = {40, 608, 1240, 710};

int page=0;
constexpr Rect kPrev={40,490,370,592}, kNext={910,490,1240,592};
int g_track = 0;
char g_chosen[teensylink::kFileNameLen] = {0};

Rect itemRect(int index) {
  int col = index / kRows;
  int row = index % kRows;
  int x = kLeft + col * kColStep;
  int y = kTop + row * kRowStep;
  return {x, y, x + kColW, y + kRowH};
}

void shortName(const char* full, char* out, size_t outLen) {
  size_t n = strlen(full);
  if (n <= kMaxShownChars) {
    strncpy(out, full, outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }
  memcpy(out, full, kMaxShownChars - 2);
  out[kMaxShownChars - 2] = '.';
  out[kMaxShownChars - 1] = '.';
  out[kMaxShownChars] = '\0';
}

void draw() {
  const teensylink::FileList& f = teensylink::state().files;
  setCanvas(kVisible);
  fillRect(0, kHeaderH, kW - 1, kH - 1, rgb565(theme::palette().background));
  char title[48];
  snprintf(title, sizeof(title), "CHOOSE A WAV FILE FOR TRACK %d", g_track + 1);
  drawText(40, 110, title, fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background));
  if (!g.linked) {
    drawText(40, kTop + 20, "TEENSY NOT CONNECTED", fontLarge(), rgb565(0xff6b6b), rgb565(theme::palette().background));
    return;
  }
  if (!f.known) {
    drawText(40, kTop + 20, "LOADING...", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().background));
    return;
  }
  if (f.count < 0) {
    drawText(40, kTop + 20, "PAUSE PLAYBACK FIRST", fontLarge(), rgb565(0xff6b6b), rgb565(theme::palette().background));
    drawText(40, kTop + 70, "Go back, press PAUSE, then try again.", fontSmall(), rgb565(theme::palette().muted),
             rgb565(theme::palette().background));
    return;
  }
  if (f.count == 0) {
    drawText(40, kTop + 20, "NO WAV FILES ON THE SD CARD", fontLarge(), rgb565(theme::palette().text),
             rgb565(theme::palette().background));
    drawText(40, kTop + 70, "Copy files over USB, then reopen this screen.", fontSmall(),
             rgb565(theme::palette().muted), rgb565(theme::palette().background));
  }
  if(page>0 && page*kCols*kRows>=f.count) page=0;
  for (int i = 0; i+page*kCols*kRows < f.count && i < kCols * kRows; ++i) {
    char shown[kMaxShownChars + 2];
    shortName(f.names[i+page*kCols*kRows], shown, sizeof(shown));
    drawButton(itemRect(i), shown, fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().accent));
  }
  if(page>0) drawButton(kPrev,"Previous",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  if((page+1)*kCols*kRows<f.count) drawButton(kNext,"Next",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  drawButton(kRemove, "REMOVE AUDIO FROM THIS TRACK", fontLarge(), rgb565(theme::palette().text),
             rgb565(0x8a2b2b));
}

}  // namespace

int track() { return g_track; }
const char* chosen() { return g_chosen; }

void setTrack(int trackIndex) { g_track = trackIndex; }

void enter(int trackIndex) {
  g_track = trackIndex;
  page=0;
  teensylink::sendListFiles();
  draw();
}

void refresh() { draw(); }

Action touchDown(int x, int y) {
  if (!g.linked) return Action::None;
  const teensylink::FileList& f = teensylink::state().files;
  if (!f.known || f.count < 0) return Action::None;
  if(page>0 && inRect(kPrev,x,y)) { --page; draw(); return Action::None; }
  if((page+1)*kCols*kRows<f.count && inRect(kNext,x,y)) { ++page; draw(); return Action::None; }
  for (int i = 0; i+page*kCols*kRows < f.count && i < kCols * kRows; ++i) {
    if (inRect(itemRect(i), x, y)) {
      strncpy(g_chosen, f.names[i+page*kCols*kRows], sizeof(g_chosen) - 1);
      g_chosen[sizeof(g_chosen) - 1] = '\0';
      return Action::Picked;
    }
  }
  if (inRect(kRemove, x, y)) return Action::Remove;
  return Action::None;
}


void redraw() {
  draw();
}

}  // namespace picker
