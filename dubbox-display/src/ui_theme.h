#pragma once
#include <stdint.h>
namespace ui { namespace theme {
enum class Skin : uint8_t { Classic, Modern, Dark, Light };
struct Palette {
  uint32_t background, panel, raised, inset, text, muted, accent, edge, shadow;
  uint8_t bevel;
};
const Palette& palette();
Skin current();
const char* name(Skin skin);
bool select(Skin skin); // Changes only appearance; returns persistence success.
void begin();
uint32_t resolve(uint32_t legacyRgb);
} }
