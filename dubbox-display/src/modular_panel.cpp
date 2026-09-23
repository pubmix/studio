#include "modular_panel.h"
#include "teensy_link.h"
#include "ui_common.h"
#include <stdio.h>
#include <stdlib.h>
namespace modularpanel {
namespace {
using namespace ui;
const char* names[modular::Nodes]={"OSC 1","OSC 2","NOISE","LFO","ENVELOPE","MIXER","FILTER","VCA","OUTPUT"};
const char* ports[modular::Ports]={"PITCH","PITCH","IN A","IN B","AUDIO","CUTOFF","AUDIO","GAIN","AUDIO"};
const char* params[modular::Params]={"OSC 1 WAVE","OSC 2 WAVE","DETUNE","LFO RATE","CUTOFF","ATTACK","DECAY","SUSTAIN","RELEASE","LEVEL"};
const char* waves[]={"SINE","TRIANGLE","SAW","SQUARE"};
constexpr uint32_t colors[]={0xffb74d,0xff6b6b,0xdce775,0x4dd0e1,0x81c784,0xb45cff,0x64b5f6,0xf06292,0xffffff};
constexpr Rect paramBox={16,582,390,628},minus={398,582,458,628},plus={464,582,524,628};
constexpr Rect unplug={540,582,700,628},audition={712,582,850,628},preset={862,582,1040,628},cancel={1052,582,1250,628};
int source=-1,input=-1,param=0,shownInst=-1;
bool resetArmed=false;
const char* hint="Tap OUT, then an input. Cyan inputs accept LFO or ENVELOPE.";
Rect card(int n){int x=28+(n%3)*414,y=238+(n/3)*112;return {x,y,x+364,y+87};}
Rect output(int n){Rect c=card(n);return {c.x2-68,c.y1+48,c.x2-4,c.y1+82};}
Rect inlet(int p){Rect c=card(modular::owner[p]);int slot=0;for(int i=0;i<p;++i)if(modular::owner[i]==modular::owner[p])++slot;return {c.x1+4+slot*108,c.y1+48,c.x1+104+slot*108,c.y1+82};}
void segment(int x,int y,int xx,int yy,uint16_t color){
 // Every cable leg is horizontal or vertical: one rectangle per leg keeps
 // redraws quick and the live mirror within its bounded telemetry queue.
 fillRect(x<xx?x:xx,y<yy?y:yy,(x>xx?x:xx)+2,(y>yy?y:yy)+2,color);
}
void cable(int s,int p){
 Rect a=output(s),b=inlet(p);int x=(a.x1+a.x2)/2,y=a.y2,xx=(b.x1+b.x2)/2,yy=b.y2;
 // Route through the gaps between rack rows, keeping labels and controls clear.
 int gutter=card(s).y2+8+(p%3)*4; int side=card(s).x2+9+(p%3)*4;
 uint16_t col=rgb565(colors[s]);
 segment(x,y,x,gutter,col);segment(x,gutter,side,gutter,col);
 int targetGap=card(modular::owner[p]).y2+8+(p%3)*4;
 segment(side,gutter,side,targetGap,col);segment(side,targetGap,xx,targetGap,col);segment(xx,targetGap,xx,yy,col);
}
}
void resetSelection(){source=input=-1;resetArmed=false;hint="Tap OUT, then an input. Cyan inputs accept LFO or ENVELOPE.";}
void draw(int inst){
 if(inst!=shownInst){shownInst=inst;resetSelection();}
 setCanvas(kVisible);fillRect(0,204,1279,640,rgb565(0x000000));
 drawText(20,208,hint,fontSmall(),rgb565(0xcccccc),rgb565(0x000000));
 const auto& p=teensylink::state().modularPatch[inst];
 for(int n=0;n<modular::Nodes;++n){
   Rect c=card(n);fillRect(c.x1,c.y1,c.x2,c.y2,rgb565(0x22222b));
   drawText(c.x1+10,c.y1+8,names[n],fontSmall(),rgb565(colors[n]),rgb565(0x22222b));
   if(n<modular::Out)drawButton(output(n),"OUT",fontSmall(),rgb565(0x000000),rgb565(source==n?0xffffff:colors[n]));
 }
 for(int i=0;i<modular::Ports;++i)if(p.cable[i]>=0)cable(p.cable[i],i);
 for(int i=0;i<modular::Ports;++i){
   uint32_t col=input==i?0xffffff:(modular::cv[i]?0x1f737c:0x4a2a7a);
   drawButton(inlet(i),ports[i],fontSmall(),rgb565(input==i?0x000000:0xffffff),rgb565(col));
 }
 char text[48];if(param<2)snprintf(text,sizeof(text),"%s: %s",params[param],waves[p.value[param]]);
 else if(param==modular::Detune)snprintf(text,sizeof(text),"DETUNE: %+d CENTS",int(p.value[param])-50);
 else snprintf(text,sizeof(text),"%s: %d",params[param],p.value[param]);
 drawButton(paramBox,text,fontSmall(),rgb565(0xffffff),rgb565(0x333344));
 drawButton(minus,"-",fontLarge(),rgb565(0xffffff),rgb565(0x3a3a48));
 drawButton(plus,"+",fontLarge(),rgb565(0xffffff),rgb565(0x3a3a48));
 drawButton(unplug,"UNPLUG",fontSmall(),rgb565(0xffffff),rgb565(input>=0?0x9c2a2a:0x333344));
 drawButton(audition,"TEST C4",fontSmall(),rgb565(0xffffff),rgb565(0x1f7a3d));
 drawButton(preset,resetArmed?"CONFIRM?":"START PATCH",fontSmall(),rgb565(0xffffff),rgb565(0x4a2a7a));
 drawButton(cancel,"CANCEL CABLE",fontSmall(),rgb565(0xffffff),rgb565(0x333344));
}
void touchDown(int inst,int x,int y){
 if(!ui::g.linked)return;
 auto p=teensylink::state().modularPatch[inst];
 if(!inRect(preset,x,y))resetArmed=false;
 for(int n=0;n<modular::Out;++n)if(inRect(output(n),x,y)){
   source=n;input=-1;hint="Choose a matching input. An output can feed several inputs.";draw(inst);return;
 }
 for(int i=0;i<modular::Ports;++i)if(inRect(inlet(i),x,y)){
   input=i;
   if(source>=0){
     if(modular::route(p,i,source)){teensylink::setModularPatch(inst,p);source=-1;hint="Cable connected. Use KEYS or ROLL to play your patch.";}
     else hint="Cannot connect: use matching audio/CV sockets without feedback.";
   }else hint="Input selected. UNPLUG removes its cable; choose OUT to repatch.";
   draw(inst);return;
 }
 if(inRect(paramBox,x,y)){param=(param+1)%modular::Params;hint="Tap the parameter name to cycle; use - and + to adjust.";}
 else if(inRect(minus,x,y)||inRect(plus,x,y)){
   int v=int(p.value[param])+(inRect(plus,x,y)?1:-1);int hi=param<2?3:100;
   if(v>=0&&v<=hi){p.value[param]=v;teensylink::setModularPatch(inst,p);}
 }else if(inRect(unplug,x,y)&&input>=0){modular::route(p,input,-1);teensylink::setModularPatch(inst,p);hint="Cable removed.";}
 else if(inRect(audition,x,y)){teensylink::auditionNote(inst,60);return;}
 else if(inRect(preset,x,y)){
   if(resetArmed){teensylink::setModularPatch(inst,modular::Patch());resetSelection();hint="Starter patch restored.";}
   else {resetArmed=true;hint="Tap CONFIRM to replace this patch with the starter patch.";}
 }else if(inRect(cancel,x,y))resetSelection();
 else { // Tap a module title to jump to its principal control.
   for(int n=0;n<modular::Nodes;++n)if(inRect(card(n),x,y)){
     const int mainParam[]={0,1,9,3,5,2,4,7,9};param=mainParam[n];
     hint="Tap the parameter name to cycle; use - and + to adjust.";break;
   }
 }
 draw(inst);
}
}
