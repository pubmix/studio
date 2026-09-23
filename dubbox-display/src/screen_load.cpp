#include "screen_load.h"
#include <Arduino.h>
#include "teensy_link.h"
#include "ui_common.h"

namespace loadscreen {
namespace {

using namespace ui;

constexpr int kColW = 560;
constexpr int kRowH = 76;
constexpr int kRowStep = 92;
constexpr int kTop = 100;
constexpr int kLeftX[2] = {60, 660};
constexpr int kRows = 6;

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
  fillRect(0, kHeaderH, kW - 1, kH - 1, rgb565(0x000000));
  drawText(60, 70, "CHOOSE A PROJECT", fontSmall(), rgb565(0x999999), rgb565(0x000000));
  if (!g.linked) {
    drawText(60, kTop + 20, "TEENSY NOT CONNECTED", fontLarge(), rgb565(0xff6b6b), rgb565(0x000000));
    return;
  }
  if (!p.listKnown) {
    drawText(60, kTop + 20, "LOADING...", fontLarge(), rgb565(0xffffff), rgb565(0x000000));
    return;
  }
  if (p.listCount == 0) {
    drawText(60, kTop + 20, "NO SAVED PROJECTS YET", fontLarge(), rgb565(0xffffff),
             rgb565(0x000000));
    drawText(60, kTop + 70, "Go back and choose NEW PROJECT.", fontSmall(), rgb565(0x999999),
             rgb565(0x000000));
    return;
  }
  for (int i = 0; i < p.listCount && i < kRows * 2; ++i) {
    drawButton(itemRect(i), p.list[i], fontLarge(), rgb565(0xffffff), rgb565(0x1c4f9c));
  }
}

}  // namespace

void enter() {
  teensylink::sendGetList();
  draw();
}

void refresh() { draw(); }

const char* touchDown(int x, int y) {
  if (!g.linked) return nullptr;
  const teensylink::ProjectInfo& p = teensylink::state().project;
  for (int i = 0; i < p.listCount && i < kRows * 2; ++i) {
    if (inRect(itemRect(i), x, y)) return p.list[i];
  }
  return nullptr;
}

}  // namespace loadscreen
