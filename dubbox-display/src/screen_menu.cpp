#include "screen_menu.h"

namespace menu {
namespace {

using namespace ui;

constexpr Rect kNewProject = {320, 112, 960, 192};
constexpr Rect kLoadProject = {320, 204, 960, 284};
constexpr Rect kWifi = {320, 296, 960, 376};
constexpr Rect kMidi = {320, 388, 960, 468};
constexpr Rect kBluetooth = {320, 480, 960, 560};
constexpr Rect kControls = {320, 572, 960, 652};

}  // namespace

void enter() {
  setCanvas(kVisible);
  int w = textWidth(fontLarge(), "MAIN MENU");
  drawText(kW / 2 - w / 2, 84, "MAIN MENU", fontLarge(), rgb565(0xffffff), rgb565(0x000000));
  drawButton(kNewProject, "NEW PROJECT", fontLarge(), rgb565(0xffffff), rgb565(0x1f7a3d));
  drawButton(kLoadProject, "LOAD PROJECT", fontLarge(), rgb565(0xffffff), rgb565(0x1c4f9c));
  drawButton(kWifi, "WIFI UPLOAD", fontLarge(), rgb565(0xffffff), rgb565(0x6a3fb0));
  drawButton(kMidi, "USB MIDI", fontLarge(), rgb565(0xffffff), rgb565(0x2b8a8a));
  drawButton(kBluetooth, "BLUETOOTH", fontLarge(), rgb565(0xffffff), rgb565(0x1565c0));
  drawButton(kControls, "CONTROLS TEST", fontLarge(), rgb565(0xffffff), rgb565(0xb3541e));
}

Screen touchDown(int x, int y) {
  if (inRect(kNewProject, x, y)) return Screen::NewProject;
  if (inRect(kLoadProject, x, y)) return Screen::LoadProject;
  if (inRect(kWifi, x, y)) return Screen::Wifi;
  if (inRect(kMidi, x, y)) return Screen::Midi;
  if (inRect(kBluetooth, x, y)) return Screen::Bluetooth;
  if (inRect(kControls, x, y)) return Screen::Controls;
  return Screen::Menu;
}

}  // namespace menu
