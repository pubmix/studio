#pragma once
#include <stdint.h>
namespace trackicons {
// 36x36 native pictograms. Draws on the caller's canvas so cached playlist art
// stays off-screen; caller explicitly selects its visible or composition layer.
void draw(int track, int x, int y, uint16_t fg, uint16_t bg);
}
