#include "screen_menu.h"

namespace menu {
namespace {

using namespace ui;

constexpr Rect kNewProject = {320, 130, 960, 220};
constexpr Rect kLoadProject = {320, 240, 960, 330};
constexpr Rect kStems = {320, 350, 960, 440};
constexpr Rect kWifi = {320, 460, 960, 550};
constexpr Rect kSettings = {320, 570, 960, 660};

}  // namespace

void enter() {
  setCanvas(kVisible);
  int w = textWidth(fontLarge(), "MAIN MENU");
  drawText(kW / 2 - w / 2, 84, "MAIN MENU", fontLarge(), rgb565(0xffffff), rgb565(0x000000));
  drawButton(kNewProject, "NEW PROJECT", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3d));
  drawButton(kLoadProject, "LOAD PROJECT", fontLarge(), rgb565(0xffffff), rgb565(0x1c4f9c));
  drawButton(kStems, "STEM SPLITTER", fontLarge(), rgb565(0xffffff), rgb565(0x805322));
  drawButton(kWifi, "WIFI UPLOAD", fontLarge(), rgb565(0xffffff), rgb565(0x6a3fb0));
  drawButton(kSettings, "SETTINGS", fontLarge(), rgb565(0xffffff), rgb565(0x3c4654));
}

Screen touchDown(int x, int y) {
  if (inRect(kNewProject, x, y)) return Screen::NewProject;
  if (inRect(kLoadProject, x, y)) return Screen::LoadProject;
  if (inRect(kStems, x, y)) return Screen::Stems;
  if (inRect(kWifi, x, y)) return Screen::Wifi;
  if (inRect(kSettings, x, y)) return Screen::Settings;
  return Screen::Menu;
}

}  // namespace menu
