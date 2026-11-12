#pragma once
#include "ui_common.h"

// Starting menu: projects, device utilities, and persisted theme selection.
namespace menu {

void enter();
// Returns the screen to switch to, or Screen::Menu to stay.
ui::Screen touchDown(int x, int y);

// Draw current state without navigation or device commands.
void redraw();

}  // namespace menu
