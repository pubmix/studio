#include "synth_panel.h"
#include "theme.h"
#include <Arduino.h>
#include <stdio.h>
#include "teensy_link.h"
#include "ui_common.h"
namespace synthpanel {
namespace {
using namespace ui;
constexpr int kParams=teensylink::kSynthParams;
enum Param {kWave1,kWave2,kOct2,kDetune,kMix,kAttack,kDecay,kSustain,kRelease,kCutoff,kResonance,kChorus,kReverb,kLevel};
struct Preset {
  const char* name;
  int v[kParams];
};
// wave1, wave2, oct2, detune, mix, attack, decay, sustain, release, cutoff, reso, chorus, reverb, level
constexpr Preset kPresets[] = {
    {"PIANO", {4, 0, 0, 0, 0, 0, 0, 100, 27, 100, 0, 0, 0, 80}},
    {"LEAD", {2, 2, 0, 12, 50, 3, 30, 80, 20, 75, 25, 40, 15, 70}},
    {"PAD", {2, 1, 0, 8, 50, 60, 50, 70, 70, 55, 10, 60, 50, 70}},
    {"BASS", {3, 2, -1, 0, 50, 1, 40, 60, 15, 40, 30, 0, 0, 90}},
    {"PLUCK", {2, 3, 1, -7, 30, 0, 22, 0, 20, 60, 35, 20, 20, 75}},
    {"ORGAN", {0, 0, 1, 0, 50, 2, 10, 100, 12, 100, 0, 40, 10, 70}},
};
constexpr int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);


// Sound controls use pages so each editable tile is a normal finger target.
int page=0,dragParam=-1,dragInst=0;
bool presetApplied=false;
int shownValues[kParams];
int shownInst=-1,shownPage=-1;
uint32_t lastSendMs=0;
constexpr int kPageParams[6][3]={{kWave1,kWave2,kOct2},{kDetune,kMix,kLevel},{kAttack,kDecay,kSustain},{kRelease,-1,-1},{kCutoff,kResonance,kChorus},{kReverb,-1,-1}};
const char* const kLabels[kParams]={"Oscillator 1","Oscillator 2","Octave 2","Detune","Oscillator mix","Attack","Decay","Sustain","Release","Cutoff","Resonance","Chorus","Reverb","Level"};
const char* const kWaveNames[]={"Sine","Triangle","Saw","Square","Piano"};
const char* const kPages[]={"Presets 1/2","Presets 2/2","Oscillators 1/2","Oscillators 2/2","Envelope 1/2","Envelope 2/2","Tone & FX 1/2","Tone & FX 2/2","Arpeggiator 1/2","Arpeggiator 2/2"};
Rect tabRect(int i) {return {16+i*420,358,16+i*420+408,450};}
Rect tileRect(int i) {int x=16+i*420,y=462;return {x,y,x+408,610};}
int value(int inst,int p) {return teensylink::state().synth[inst].v[p];}
void range(int p,int& lo,int& hi) {lo=0;hi=100;if(p<=kWave2)hi=p==kWave1?4:3;if(p==kOct2){lo=-2;hi=2;}if(p==kDetune){lo=-50;hi=50;}}
int paramSlot(int p) {if(page<2 || page>=8)return -1;for(int i=0;i<3;++i)if(kPageParams[page-2][i]==p)return i;return -1;}
void tile(int inst,int slot) {
 setCanvas(kVisible);
 const Rect r=tileRect(slot);const auto& c=theme::palette();
 ScopedRedraw redraw(r);
 if(page<2){drawButton(r,kPresets[page*3+slot].name,fontSmall(),rgb565(c.text),rgb565(c.surface));return;}
 if(page>=8){
   const auto& a=teensylink::state().arp[inst];char label[64];const char* names[]={"Up","Down","UpDown","Random"};
   if(page==8){if(slot==0)snprintf(label,sizeof(label),"Arp: %s",a.enabled?"ON":"OFF");else if(slot==1)snprintf(label,sizeof(label),"Pattern: %s",names[a.pattern]);else snprintf(label,sizeof(label),"Octaves: %d",a.octaves);}
   else {if(slot==0)snprintf(label,sizeof(label),"Steps / beat: %d",a.rate);else if(slot==1)snprintf(label,sizeof(label),"Gate: %d%%",a.gate);else snprintf(label,sizeof(label),"Live keys only");}
   drawButton(r,label,fontSmall(),rgb565(c.text),rgb565(c.surface));return;
 }
 int p=kPageParams[page-2][slot];if(p<0)return;
 fillRoundRect(r,12,rgb565(c.surface));
 char label[64];int v=value(inst,p);shownValues[p]=v;
 if(p<=kWave2)snprintf(label,sizeof(label),"%s: %s",kLabels[p],kWaveNames[constrain(v,0,p==kWave1?4:3)]);
 else snprintf(label,sizeof(label),"%s  %d",kLabels[p],v);
 drawText(r.x1+16,r.y1+8,label,fontSmall(),rgb565(c.text),rgb565(c.surface));
 if(p>kWave2){int lo,hi;range(p,lo,hi);int x=r.x1+20+(r.x2-r.x1-40)*(constrain(v,lo,hi)-lo)/(hi-lo);
 fillRoundRect({r.x1+20,r.y2-21,r.x2-20,r.y2-15},3,rgb565(c.border));
 fillRoundRect({r.x1+20,r.y2-21,x,r.y2-15},3,rgb565(c.accent));
 fillRoundRect({x-8,r.y2-29,x+8,r.y2-7},8,rgb565(c.text));}
}
void drawPage(int inst){setCanvas(kVisible);ScopedRedraw redraw({0,354,kW-1,613});shownInst=inst;shownPage=page;fillRect(0,354,kW-1,613,rgb565(theme::palette().background));
 drawButton(tabRect(0),"Previous",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
 drawButton(tabRect(1),kPages[page],fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().background));
 drawButton(tabRect(2),"Next",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
 for(int i=0;i<3;++i)tile(inst,i);
}
void dragTo(int x) {if(dragParam<0)return;int slot=paramSlot(dragParam);if(slot<0)return;Rect r=tileRect(slot);int lo,hi;range(dragParam,lo,hi);int v=lo+(constrain(x,r.x1+20,r.x2-20)-r.x1-20)*(hi-lo)/(r.x2-r.x1-40);
 uint32_t now=millis();if(now-lastSendMs>=40){teensylink::setSynthParam(dragInst,dragParam,v);lastSendMs=now;}else teensylink::setSynthParamLocal(dragInst,dragParam,v);
 tile(dragInst,slot);
}
}
void draw(int inst,int octaveBase){(void)octaveBase;dragParam=-1;drawPage(inst);}
void redraw(int inst){drawPage(inst);}
void setOctave(int octaveBase){(void)octaveBase;}
bool takePresetApplied(){bool result=presetApplied;presetApplied=false;return result;}
bool touchDown(int inst,int x,int y){dragParam=-1;presetApplied=false;
 if(inRect(tabRect(0),x,y)){page=(page+9)%10;drawPage(inst);return true;}
 if(inRect(tabRect(2),x,y)){page=(page+1)%10;drawPage(inst);return true;}
 for(int i=0;i<3;++i){if(!inRect(tileRect(i),x,y))continue;
 if(page<2){teensylink::setSynthAll(inst,kPresets[page*3+i].v);presetApplied=true;return true;}
 if(page>=8){
   auto a=teensylink::state().arp[inst];
   if(page==8){if(i==0)a.enabled=!a.enabled;else if(i==1)a.pattern=(a.pattern+1)%4;else a.octaves=a.octaves%3+1;}
   else {if(i==0)a.rate=a.rate==1?2:(a.rate==2?4:(a.rate==4?8:1));else if(i==1)a.gate=a.gate>=100?25:a.gate+25;else return true;}
   teensylink::setArp(inst,a);drawPage(inst);return true;
 }
 int p=kPageParams[page-2][i];if(p<0)return false;
 if(p<=kWave2){teensylink::setSynthParam(inst,p,(value(inst,p)+1)%(p==kWave1?5:4));tile(inst,i);return true;}
 dragParam=p;dragInst=inst;lastSendMs=0;dragTo(x);return true;
 }return false;}
void touchMove(int x){dragTo(x);}
void touchUp(int inst){(void)inst;if(dragParam>=0)teensylink::setSynthParam(dragInst,dragParam,value(dragInst,dragParam));dragParam=-1;}
void touchPoints(int inst,const int* xs,const int* ys,int n,bool active){(void)inst;(void)xs;(void)ys;(void)n;(void)active;}
void refresh(int inst){
 if(dragParam>=0)return;
 if(shownInst!=inst || shownPage!=page){drawPage(inst);return;}
 if(page>=8){drawPage(inst);return;}
 if(page<2)return; // Preset labels do not change when parameter echoes arrive.
 setCanvas(kVisible);
 for(int i=0;i<3;++i){int p=kPageParams[page-2][i];if(p>=0 && shownValues[p]!=value(inst,p))tile(inst,i);}
}
void releaseKeys(){}
}
