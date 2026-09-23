#include "screen_menu.h"

namespace menu {
namespace {

using namespace ui;

constexpr Rect kNewProject = {320, 150, 960, 250};
constexpr Rect kLoadProject = {320, 290, 960, 390};
constexpr Rect kWifi = {320, 430, 960, 530};

}  // namespace

void enter() {
  setCanvas(kVisible);
  int w = textWidth(fontLarge(), "MAIN MENU");
  drawText(kW / 2 - w / 2, 84, "MAIN MENU", fontLarge(), rgb565(0xffffff), rgb565(0x000000));
  drawButton(kNewProject, "NEW PROJECT", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3d));
  drawButton(kLoadProject, "LOAD PROJECT", fontLarge(), rgb565(0xffffff), rgb565(0x1c4f9c));
  drawButton(kWifi, "WIFI UPLOAD", fontLarge(), rgb565(0xffffff), rgb565(0x6a3fb0));
}

Screen touchDown(int x, int y) {
  if (inRect(kNewProject, x, y)) return Screen::NewProject;
  if (inRect(kLoadProject, x, y)) return Screen::LoadProject;
  if (inRect(kWifi, x, y)) return Screen::Wifi;
  return Screen::Menu;
}

}  // namespace menu
