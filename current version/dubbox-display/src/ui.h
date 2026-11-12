#pragma once
#include <stdint.h>

namespace ui {

// Builds the off-screen art and shows the menu. Call once after the display and touch are up.
void begin();
// Call every loop: link, touch, screens.
void update();
// Idle-only draw replay; preserves project, audio and editing state.
bool mirrorSnapshot(uint32_t requestId);

}  // namespace ui
