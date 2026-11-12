#include "track_icons.h"
#include "ui_common.h"
namespace trackicons {
void draw(int track, int x, int y, uint16_t fg, uint16_t bg) {
  using namespace ui;
  if (track < 0 || track > 3) return;
  auto box = [&](int a,int b,int c,int d,uint16_t color) { fillRect(x+a,y+b,x+c,y+d,color); };
  auto round = [&](int a,int b,int c,int d,int radius,uint16_t color) { fillRoundRect({x+a,y+b,x+c,y+d},radius,color); };
  box(0,0,35,35,bg);
  switch (track) {
    case 0: // Capsule microphone, cradle and stand.
      round(13,2,23,21,5,fg);
      box(15,7,21,8,bg); box(15,11,21,12,bg);
      round(7,13,29,27,7,fg);
      box(10,13,26,21,bg);
      round(13,13,23,21,4,fg);
      box(17,27,19,31,fg); round(10,31,26,34,2,fg);
      break;
    case 1: // Beamed eighth notes.
      box(12,6,15,27,fg); box(27,3,30,24,fg);
      box(13,4,29,8,fg);
      round(4,23,15,31,4,fg); round(19,20,30,28,4,fg);
      break;
    case 2: // Upright bass-guitar silhouette: headstock, neck, waist, body.
      round(16,1,23,9,2,fg); box(18,8,21,22,fg);
      box(13,3,15,4,fg); box(24,6,26,7,fg);
      round(10,18,28,34,7,fg);
      round(8,17,14,23,3,bg); round(24,17,30,23,3,bg);
      box(18,20,19,30,bg); box(14,28,24,30,bg);
      break;
    case 3: // Drum kit: cymbals, toms and bass drum.
      box(4,7,5,29,fg); box(30,7,31,29,fg);
      round(0,4,10,7,2,fg); round(25,4,35,7,2,fg);
      round(10,9,17,16,2,fg); round(20,9,27,16,2,fg);
      round(10,17,27,34,8,fg); round(13,20,24,31,5,bg);
      box(1,29,8,30,fg); box(28,29,34,30,fg);
      break;
  }
}
}
