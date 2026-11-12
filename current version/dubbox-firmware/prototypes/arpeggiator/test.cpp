#include "arpeggiator.h"
#include <assert.h>
#include <stdio.h>
using namespace studio_arp;
struct Capture {int current=-1;unsigned on=0,off=0,n=0;uint8_t notes[128]={};};
void emit(void* p,Event e){auto& c=*static_cast<Capture*>(p);if(e.on){assert(c.current==-1);c.current=e.midi;++c.on;assert(c.n<128);c.notes[c.n++]=e.midi;}else{assert(c.current==e.midi);c.current=-1;++c.off;}}
void pattern(Pattern pat,const uint8_t* expected,unsigned length){Capture c;Arpeggiator a(emit,&c);a.hold(67);a.hold(60);a.hold(64);Config cfg;cfg.pattern=pat;assert(a.configure(cfg,0));a.start(0);for(unsigned i=0;i<length;++i)a.tick(i*125000);a.stop();assert(c.on==c.off);for(unsigned i=0;i<length;++i)assert(c.notes[i]==expected[i]);}
int main(){
 const uint8_t up[]={60,64,67,60},down[]={67,64,60,67},bounce[]={60,64,67,64,60,64};
 pattern(Pattern::Up,up,4);pattern(Pattern::Down,down,4);pattern(Pattern::UpDown,bounce,6);
 Capture c;Arpeggiator a(emit,&c);Config cfg;cfg.octaves=3;a.configure(cfg,0);a.hold(60);a.start(0);a.tick(0);a.tick(93749);assert(c.off==0);a.tick(93750);assert(c.off==1);a.tick(125000);a.tick(250000);assert(c.notes[1]==72 && c.notes[2]==84);a.release(60);assert(c.on==c.off);a.tick(375000);assert(c.on==3);
 for(int i=0;i<6;++i)assert(a.hold(60+i));assert(a.hold(60));assert(!a.hold(80));assert(!a.hold(128));assert(a.heldCount()==6);a.start(500000);a.tick(500000);cfg.pattern=Pattern::Down;a.configure(cfg,500000);assert(c.on==c.off);a.tick(500000);a.clear();assert(c.on==c.off && a.heldCount()==0);
 cfg.bpm=0;assert(!a.configure(cfg,0));cfg.bpm=120;cfg.octaves=3;a.configure(cfg,0);a.hold(127);a.start(0);a.tick(0);assert(c.current==127);a.stop();
 Capture x,y;Arpeggiator ax(emit,&x),ay(emit,&y);cfg.pattern=Pattern::Random;ax.configure(cfg,0);ay.configure(cfg,0);ax.seed(123);ay.seed(123);for(int m=60;m<66;++m){ax.hold(m);ay.hold(m);}ax.start(0);ay.start(0);for(unsigned t=0;t<4000000;t+=125000){ax.tick(t);ay.tick(t);}ax.stop();ay.stop();for(unsigned i=0;i<x.n;++i)assert(x.notes[i]==y.notes[i]);assert(x.on==x.off && y.on==y.off);
 a.clear();a.hold(60);a.start(0xffff0000u);a.tick(0xffff0000u);a.tick(0xffff0000u+93750u);assert(!a.sounding());a.tick(0xffff0000u+125000u);assert(a.sounding());a.stop();assert(c.on==c.off);
 a.start(0);unsigned before=c.on;a.tick(10000000);assert(c.on==before+1);a.stop();assert(c.on==c.off);
 for(unsigned rate=1;rate<=8;++rate)for(unsigned gi=0;gi<3;++gi){
   Capture z;Arpeggiator az(emit,&z);Config cz;cz.stepsPerBeat=rate;cz.gatePercent=gi==0?1:(gi==1?50:100);cz.pattern=Pattern::UpDown;
   assert(az.configure(cz,0));az.hold(60);az.start(0);az.tick(0);
   unsigned period=60000000u/(120*rate),end=period*cz.gatePercent/100;
   az.tick(end-1);assert(z.off==0);az.tick(end);assert(z.off==1);
   az.stop();assert(z.on==z.off);
 }
 printf("Arpeggiator tests passed; instance %lu bytes\n",static_cast<unsigned long>(sizeof(a)));
}
