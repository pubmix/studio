#include "ui_theme.h"
#include <Preferences.h>
namespace ui { namespace theme {
namespace {
constexpr Palette skins[] = {
  {0x18191d,0x34353d,0x4b4d57,0x101114,0xf4efdc,0xb5b3a8,0xffbc48,0x777a86,0x08090b,3},
  {0x101112,0x222426,0x35383a,0x090b0c,0xece8df,0xa8aaa8,0xffad36,0x55595a,0x060708,2},
  {0x050607,0x121517,0x24292c,0x020303,0xf1f2ef,0xa3aaa9,0xff9638,0x3a4144,0x000000,1},
  {0xe7e3da,0xf7f3ea,0xd7d2c7,0xc9c3b7,0x232527,0x5d605f,0x995000,0xffffff,0x9b968c,2}
};
Skin active = Skin::Modern;
}
const Palette& palette() { return skins[static_cast<uint8_t>(active)]; }
Skin current() { return active; }
const char* name(Skin s) {
  static const char* names[] = {"Classic", "Modern", "Dark", "Light"};
  auto i = static_cast<uint8_t>(s);
  return i < 4 ? names[i] : "Modern";
}
void begin() {
  Preferences p;
  if (!p.begin("studio-ui", true)) return;
  uint8_t s = p.getUChar("skin", 1);
  p.end();
  active = s < 4 ? static_cast<Skin>(s) : Skin::Modern;
}
bool select(Skin s) {
  if (static_cast<uint8_t>(s) >= 4) return false;
  if (s == active) return true;
  active = s;
  Preferences p;
  if (!p.begin("studio-ui", false)) return false;
  bool saved = p.putUChar("skin", static_cast<uint8_t>(s)) == 1;
  p.end();
  return saved;
}
// Compatibility palette for the existing immediate-mode screens. Resolve before
// intensity scaling so waveform bright/dim layers retain their relative levels.
// Track/pattern identity colors and warning reds remain semantic, not decoration.
uint32_t resolve(uint32_t c) {
  const auto& p = palette();
  if (active == Skin::Light) {
    switch(c) {
      case 0x3d7fff: return 0x2458a8;
      case 0xff9f3d: return 0x995000;
      case 0x7fff3d: return 0x386c19;
      case 0xff3d7f: return 0xa4214e;
      case 0xffa726: return 0x995000;
      default: break;
    }
  }
  switch(c) {
    case 0x000000: return p.background;
    case 0x18202e: case 0x1c1c1c: case 0x222222: case 0x232323:
    case 0x24202e: case 0x2a2a2a: return p.panel;
    case 0x2c2c2c: case 0x333333: case 0x3c4654: case 0x3d3d3d:
    case 0x444444: case 0x4a4a4a: case 0x555555: return p.raised;
    case 0xffffff: case 0xdddddd: case 0xeeeeee: return p.text;
    case 0x777777: case 0x888888: case 0x999999: case 0xaaaaaa: return p.muted;
    case 0xffaa00: case 0x00e5ff: case 0x9df0c0: case 0x1f9d55:
    case 0x35d07f: return p.accent;
    case 0x1f7a3a: case 0x1f7a3d: case 0x1c4f9c: case 0x6a3fb0:
    case 0x1f5a2e: return p.raised;
    default: break;
  }
  // Other neutral screen shades participate in Light as well.
  unsigned r = c >> 16, g = (c >> 8) & 255, b = c & 255;
  if (r == g && g == b) return r < 80 ? p.panel : (r < 190 ? p.muted : p.text);
  unsigned hi = r > g ? r : g; if (b > hi) hi = b;
  unsigned lo = r < g ? r : g; if (b < lo) lo = b;
  if (hi < 90 && hi - lo < 35) return p.panel;
  return c;
}
} }
