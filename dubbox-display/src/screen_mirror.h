#pragma once
#include <stdint.h>

// Nonblocking diagnostic stream of completed UI drawing commands, not LCD readback.
namespace mirror {
void begin();
void flush();
void canvas(unsigned long address);
void rect(int x1, int y1, int x2, int y2, uint16_t color);
void glyph(int font, unsigned char ch, int x, int baseline, uint16_t color);
void copy(unsigned long src, unsigned long dst, int x, int y, int w, int h);
void frame(int screen);
void touch(const char* phase, int x, int y);
}
