#include "screen_menu.h"
#include "theme.h"
namespace menu {
namespace {
using namespace ui;
constexpr Rect buttons[] = {
  {48,244,596,376},{48,400,596,532},
  {656,244,936,346},{952,244,1232,346},
  {656,364,936,466},{952,364,1232,466},
  {656,484,1232,586}
};
const char* labels[] = {"New project", "Open project", "Wi-Fi upload", "USB MIDI", "Bluetooth", "Controls", "Themes"};
constexpr Screen destinations[] = {Screen::NewProject,Screen::LoadProject,Screen::Wifi,Screen::Midi,Screen::Bluetooth,Screen::Controls,Screen::Themes};
}
void enter() {
  const auto& p=theme::palette();
  setCanvas(kVisible);
  drawText(48,138,"Make something great.",fontLarge(),rgb565(p.text),rgb565(p.background));
  drawText(48,202,"Projects",fontSmall(),rgb565(p.muted),rgb565(p.background));
  drawText(656,202,"Your device",fontSmall(),rgb565(p.muted),rgb565(p.background));
  for(int i=0;i<7;++i) {
    const Rect& r=buttons[i];
    drawButton(r,labels[i],i<2 ? fontLarge():fontSmall(),rgb565(p.text),rgb565(i==0?p.accent:p.surface));
  }
  drawText(48,620,"Four tracks. Endless possibilities.",fontSmall(),rgb565(p.muted),rgb565(p.background));
}
Screen touchDown(int x,int y) {
  for(int i=0;i<7;++i) if(inRect(buttons[i],x,y)) return destinations[i];
  return Screen::Menu;
}

void redraw() {
  enter();
}

}
