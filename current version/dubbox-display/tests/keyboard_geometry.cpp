#include "keyboard.h"
#include "ui_common.h"
#include <assert.h>
#include <stdio.h>
static uint32_t now=100;
static int starts=0,stops=0;
uint32_t millis(){return now;}
namespace ui {
Shared g;
uint16_t rgb565(uint32_t rgb,float){return static_cast<uint16_t>(rgb);}
void setCanvas(unsigned long){}
void fillRect(int x1,int y1,int x2,int y2,uint16_t){
  assert(x1>=16 && x2<1264 && x1<=x2);
  assert(y1>=258 && y2<=610 && y1<=y2);
}
}
namespace teensylink {
void sendLiveNote(int,int,bool on){if(on)++starts;else++stops;}
void sendNoteRefresh(int,int){}
}
int main(){
  keyboard::Keyboard k;k.layout={16,258,610,1248,210};
  assert(k.keyCount==8);
  const int semis[]={0,2,4,5,7,9,11};
  for(int count=5;count<=14;++count)for(int base=36;base<=84;base+=12){
    keyboard::setKeyCount(k,count);k.octaveBase=base;keyboard::draw(k);
    int w=1248/count;
    assert(keyboard::keyAt(k,15,400)==-1);
    assert(keyboard::keyAt(k,16+count*w,400)==-1);
    assert(keyboard::keyAt(k,100,257)==-1);
    assert(keyboard::keyAt(k,100,611)==-1);
    for(int i=0;i<count;++i){
      int note=base+12*(i/7)+semis[i%7];
      assert(keyboard::keyAt(k,16+i*w+w/2,600)==note);
      if(i<count-1 && (i%7==0 || i%7==1 || i%7==3 || i%7==4 || i%7==5))
        assert(keyboard::keyAt(k,16+(i+1)*w,300)==note+1);
    }
  }
  k.keyCount=8;k.octaveBase=60;ui::g.linked=true;
  int xs[]={94,406,718},ys[]={600,600,600};
  keyboard::touchPoints(k,xs,ys,3,true);assert(k.heldCount==3 && starts==3);
  now+=20;keyboard::touchPoints(k,xs,ys,0,true);assert(stops==0);
  now+=80;keyboard::touchPoints(k,xs,ys,0,true);assert(stops==3 && k.heldCount==0);
  keyboard::touchPoints(k,xs,ys,3,true);
  keyboard::touchPoints(k,xs,ys,0,false);assert(starts==6 && stops==6);
  puts("keyboard geometry, drawing bounds, chords and releases passed");
}
