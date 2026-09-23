#pragma once
#include "ui_common.h"

// Main menu: New Project (opens the mixer), Playlist, and placeholders.
namespace menu {

void enter();
// Returns the screen to switch to, or Screen::Menu to stay.
ui::Screen touchDown(int x, int y);

}  // namespace menu
