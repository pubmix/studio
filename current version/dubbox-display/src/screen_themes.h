#pragma once
namespace themescreen {
void enter();
// True when a new palette was applied and cached screen art must be rebuilt.
bool touchDown(int x, int y);
// Draw current state without navigation or device commands.
void redraw();

}
