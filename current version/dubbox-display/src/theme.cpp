#include "theme.h"
#include <Preferences.h>
namespace theme {
namespace {
constexpr Palette colors[] = {
  {0xf3f0e9, 0xe5e1d8, 0x23272b, 0x65665f, 0x247a73, 0xc27860, 0xb5b4a9},
  {0x202426, 0x303638, 0xf2ebdd, 0xb2b7ae, 0xdda657, 0x899a78, 0x596164},
  {0xf5efe8, 0xebdcd9, 0x24473f, 0x6c6865, 0x78516b, 0xd9a7ac, 0xb4a29e},
};
Id selected = Id::Chalk;
bool valid(Id id) { return static_cast<uint8_t>(id) < 3; }
}
const Palette& palette(Id id) { return colors[valid(id) ? static_cast<uint8_t>(id) : 0]; }
const Palette& palette() { return palette(selected); }
Id current() { return selected; }
const char* name(Id id) {
  switch (id) { case Id::Graphite: return "Graphite"; case Id::Petal: return "Petal"; default: return "Chalk"; }
}
void begin() {
  selected = Id::Chalk;
  Preferences prefs;
  if (!prefs.begin("studio-ui", true)) return;
  Id saved = static_cast<Id>(prefs.getUChar("theme", 0));
  prefs.end();
  if (valid(saved)) selected = saved;
}
bool select(Id id) {
  if (!valid(id)) return false;
  if (id == selected) return true;
  Preferences prefs;
  if (!prefs.begin("studio-ui", false)) return false;
  bool saved = prefs.putUChar("theme", static_cast<uint8_t>(id)) == 1;
  prefs.end();
  if (saved) selected = id;
  return saved;
}
}
