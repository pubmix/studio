#pragma once
#include <stdint.h>

// Bounded diagnostic stream of completed UI drawing commands, not LCD readback.
namespace mirror {
void begin();
void flush();
// Fresh complete drawing baseline, never a board or project reset.
bool snapshotBegin(uint32_t requestId);
void snapshotEnd(uint32_t requestId, int screen);
void canvas(unsigned long address);
void rect(int x1, int y1, int x2, int y2, uint16_t color);
void roundRect(int x1, int y1, int x2, int y2, int radius, uint16_t color);
void glyph(int font, unsigned char ch, int x, int baseline, uint16_t color);
void copy(unsigned long src, unsigned long dst, int x, int y, int w, int h);
void blend(unsigned long oldFrame, unsigned long newFrame, unsigned long destination, uint8_t alpha32);
void frame(int screen);
void touch(const char* phase, int x, int y);
}
