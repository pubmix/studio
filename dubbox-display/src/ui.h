#pragma once

namespace ui {

// Builds the off-screen art and shows the menu. Call once after the display and touch are up.
void begin();
// Call every loop: link, touch, screens.
void update();

}  // namespace ui
