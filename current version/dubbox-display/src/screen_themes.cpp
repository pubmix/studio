#include "screen_themes.h"
#include "theme.h"
#include "ui_common.h"

namespace themescreen {
namespace {
using namespace ui;
constexpr Rect cards[] = {{48, 220, 424, 595}, {452, 220, 828, 595}, {856, 220, 1232, 595}};
const char* descriptions[] = {"Cream and teal", "Dark and amber", "Blush and forest"};
}
void enter() {
  using namespace ui;
  const auto& active = theme::palette();
  setCanvas(kVisible);
  drawText(48, 120, "Make it yours", fontLarge(), rgb565(active.text), rgb565(active.background));
  drawText(48, 165, "Choose a look. Your choice is saved for next time.", fontSmall(), rgb565(active.muted), rgb565(active.background));
  for (int i = 0; i < 3; ++i) {
    auto id = static_cast<theme::Id>(i);
    const auto& p = theme::palette(id);
    const Rect& r = cards[i];
    bool selected = theme::current() == id;
    fillRoundRect(r, 22, rgb565(selected ? active.accent : active.border));
    fillRoundRect({r.x1+4, r.y1+4, r.x2-4, r.y2-4}, 18, rgb565(p.background));
    drawText(r.x1+22, r.y1+28, theme::name(id), fontLarge(), rgb565(p.text), rgb565(p.background));
    drawText(r.x1+22, r.y1+79, descriptions[i], fontSmall(), rgb565(p.muted), rgb565(p.background));
    // A small instrument preview uses the candidate's actual palette.
    fillRect(r.x1+22, r.y1+135, r.x2-22, r.y1+245, rgb565(p.surface));
    for (int j=0; j<4; ++j) {
      int x=r.x1+48+j*76;
      fillRect(x, r.y1+150, x+6, r.y1+229, rgb565(p.border));
      int y=r.y1+169+(j%3)*18;
      fillRect(x-12, y, x+18, y+10, rgb565(j%2 ? p.secondary : p.accent));
    }
    uint32_t swatches[] = {p.background, p.text, p.accent, p.secondary};
    for (int j=0; j<4; ++j) fillRect(r.x1+22+j*83,r.y1+265,r.x1+92+j*83,r.y1+291,rgb565(swatches[j]));
    drawText(r.x1+22, r.y1+319, selected ? "SELECTED" : "Tap to apply", fontSmall(), rgb565(p.text), rgb565(p.background));
  }
}
bool touchDown(int x, int y) {
  for (int i=0;i<3;++i) {
    if (!ui::inRect(cards[i],x,y)) continue;
    auto id=static_cast<theme::Id>(i);
    if (theme::current()==id) return false;
    return theme::select(id);
  }
  return false;
}

void redraw() {
  enter();
}

}
