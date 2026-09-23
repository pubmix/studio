#include "screen_settings.h"
#include "ui_common.h"
namespace settings {
namespace {
using namespace ui;
constexpr Rect choices[] = {{80,210,330,340},{370,210,620,340},{660,210,910,340},{950,210,1200,340}};
bool saved = true;
}
void enter() {
  using namespace ui;
  setCanvas(kVisible);
  drawText(80,110,"STUDIO / APPEARANCE",fontLarge(),rgb565(0xffffff),rgb565(0x000000));
  drawText(80,165,"Choose a skin for every screen",fontSmall(),rgb565(0x999999),rgb565(0x000000));
  for(int i=0;i<4;++i) {
    bool selected = static_cast<int>(theme::current()) == i;
    drawButton(choices[i],theme::name(static_cast<theme::Skin>(i)),fontLarge(),rgb565(0xffffff),
               rawRgb565(selected ? theme::palette().accent : theme::palette().raised));
    if(selected) drawText(choices[i].x1+42,360,"SELECTED",fontSmall(),rawRgb565(theme::palette().accent),rgb565(0x000000));
  }
  drawText(80,440,saved ? "Skin saved on this device" : "Applied for now - could not save skin",fontSmall(),rgb565(0x999999),rgb565(0x000000));
  drawText(80,485,"Audio, projects and stem assignments stay the same.",fontSmall(),rgb565(0x999999),rgb565(0x000000));
}
bool touchDown(int x,int y) {
  for(int i=0;i<4;++i) if(ui::inRect(choices[i],x,y)) {
    if(static_cast<int>(ui::theme::current())==i) return false;
    saved=ui::theme::select(static_cast<ui::theme::Skin>(i));
    return true;
  }
  return false;
}
}
