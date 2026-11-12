#pragma once

// Lists saved projects for opening.
namespace loadscreen {

void enter();
// Redraws the list after fresh data arrives from the Teensy.
void refresh();
// Returns the tapped project's name, or nullptr.
const char* touchDown(int x, int y);

// Draw current state without navigation or device commands.
void redraw();

}  // namespace loadscreen
