#pragma once
#include <stdint.h>
namespace theme {
enum class Id : uint8_t { Chalk, Graphite, Petal };
struct Palette { uint32_t background, surface, text, muted, accent, secondary, border; };
const Palette& palette();
const Palette& palette(Id id);
Id current();
void begin();
bool select(Id id);
const char* name(Id id);
}
