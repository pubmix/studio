#include "theme.h"
#include "screen_load.h"
#include <Arduino.h>
#include "teensy_link.h"
#include "ui_common.h"

namespace loadscreen {
namespace {

using namespace ui;

constexpr int kColW = 560;
constexpr int kRowH = 102;
constexpr int kRowStep = 112;
constexpr int kTop = 150;
constexpr int kLeftX[2] = {60, 660};
constexpr int kRows = 4;

int page = 0;
constexpr Rect kPrev={60,610,360,712}, kNext={920,610,1220,712};

Rect itemRect(int index) {
  int col = index / kRows;
  int row = index % kRows;
  int x = kLeftX[col];
  int y = kTop + row * kRowStep;
  return {x, y, x + kColW, y + kRowH};
}

void draw() {
  const teensylink::ProjectInfo& p = teensylink::state().project;
  setCanvas(kVisible);
  fillRect(0, kHeaderH, kW - 1, kH - 1, rgb565(theme::palette().background));
  drawText(60, 110, "Choose a project", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background));
  if (!g.linked) {
    drawText(60, kTop + 20, "TEENSY NOT CONNECTED", fontLarge(), rgb565(0xff6b6b), rgb565(theme::palette().background));
    return;
  }
  if (!p.listKnown) {
    drawText(60, kTop + 20, "LOADING...", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().background));
    return;
  }
  if (p.listCount == 0) {
    drawText(60, kTop + 20, "NO SAVED PROJECTS YET", fontLarge(), rgb565(theme::palette().text),
             rgb565(theme::palette().background));
    drawText(60, kTop + 70, "Go back and choose NEW PROJECT.", fontSmall(), rgb565(theme::palette().muted),
             rgb565(theme::palette().background));
    return;
  }
  int pages = (p.listCount + kRows*2-1)/(kRows*2);
  if (page >= pages) page = pages-1;
  for (int i = 0; i + page*kRows*2 < p.listCount && i < kRows * 2; ++i) {
    drawButton(itemRect(i), p.list[i + page*kRows*2], fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().accent));
  }
  if(page>0) drawButton(kPrev,"Previous",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  if(page+1<pages) drawButton(kNext,"Next",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
}

}  // namespace

void enter() {
  page=0;
  teensylink::sendGetList();
  draw();
}

void refresh() { draw(); }

const char* touchDown(int x, int y) {
  if (!g.linked) return nullptr;
  const teensylink::ProjectInfo& p = teensylink::state().project;
  if(page>0 && inRect(kPrev,x,y)) { --page; draw(); return nullptr; }
  if((page+1)*kRows*2<p.listCount && inRect(kNext,x,y)) { ++page; draw(); return nullptr; }
  for (int i = 0; i+page*kRows*2 < p.listCount && i < kRows * 2; ++i) {
    if (inRect(itemRect(i), x, y)) return p.list[i+page*kRows*2];
  }
  return nullptr;
}


void redraw() {
  draw();
}

}  // namespace loadscreen
