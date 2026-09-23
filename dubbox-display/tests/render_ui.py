from pathlib import Path
import re,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
w=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(tempfile.mkdtemp(prefix="studio-ui-"))
w.mkdir(parents=True,exist_ok=True)
(w/'Arduino.h').write_text('''#pragma once
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#define PROGMEM
using std::min; using std::max;
template<class T> T constrain(T a,T b,T c){return std::max(b,std::min(a,c));}
inline uint32_t millis(){return 1000;}
''')
(w/'Preferences.h').write_text('''#pragma once
#include <stdint.h>
struct Preferences {
static uint8_t stored; static bool fail;
bool begin(const char*,bool){return !fail;}
uint8_t getUChar(const char*,uint8_t){return stored;}
unsigned putUChar(const char*,uint8_t n){stored=n;return 1;}
void end(){}
};
''')
(w/'Fonts').mkdir(exist_ok=True)
for font in ['FreeSansBold12pt7b.h','FreeSansBold18pt7b.h']:
    (w/'Fonts'/font).write_text((root/'.pio/libdeps/esp32dev/Adafruit GFX Library/Fonts'/font).read_text().replace('#include <Adafruit_GFX.h>',''))
s=(root/'src/ui_common.cpp').read_text().replace('#include "LCD.h"','').replace('#include "screen_mirror.h"','')
a=s.index('void setCanvas(');b=s.index('const GFXfont* fontSmall()',a)
s=s[:a]+'''static uint16_t pixels[4][720][1280];
static unsigned layer=0;
void setCanvas(unsigned long a){layer=a/1843200;}
void fillRect(int x,int y,int r,int b,uint16_t c){
for(int yy=max(0,y);yy<=min(719,b);++yy) for(int xx=max(0,x);xx<=min(1279,r);++xx) pixels[layer][yy][xx]=c;
}
void bteCopy(unsigned long s,unsigned long d,int x,int y,int w,int h){
for(int yy=y;yy<y+h && yy<720;++yy) for(int xx=x;xx<x+w && xx<1280;++xx) if(xx>=0&&yy>=0) pixels[d/1843200][yy][xx]=pixels[s/1843200][yy][xx];
}
void exportPreview(const char* path){
FILE* f=fopen(path,"wb"); fprintf(f,"P6\\n1280 720\\n255\\n");
for(int y=0;y<720;++y) for(int x=0;x<1280;++x){auto c=pixels[0][y][x]; unsigned char rgb[3]={(unsigned char)((c>>11)*255/31),(unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);
}
'''+s[b:]
s=re.sub(r'^.*mirror::.*\n','',s,flags=re.M)
(w/'common.cpp').write_text(s)
(w/'main.cpp').write_text('''#include "ui_common.h"
#include "screen_mixer.h"
#include "screen_menu.h"
#include "screen_settings.h"
#include "screen_load.h"
#include "teensy_link.h"
#include "Preferences.h"
#include <cassert>
#include <cstring>
#include <cstdio>
uint8_t Preferences::stored=255; bool Preferences::fail=false;
namespace ui {void exportPreview(const char*);}
namespace teensylink {
State st;
const State& state(){return st;}
uint8_t takeChangedFx(){return 0;} bool takeMixerChanged(){return false;}
int lastPanTrack=-1,lastPanValue=0;
void sendPan(int t,int v){lastPanTrack=t;lastPanValue=v;st.panPermille[t]=v;}
void sendWet(int,int){} void sendBypass(int,bool){} void sendActiveSlot(int,int){} void sendAddFx(int,int){}
void sendVolume(int){} void sendSeek(uint32_t){} void sendListProjects(){}
}
int main(int argc,char**argv){
using namespace ui;
theme::begin(); assert(theme::current()==theme::Skin::Modern);
assert(!theme::select(static_cast<theme::Skin>(9)));
for(int i=0;i<4;++i){assert(theme::select(static_cast<theme::Skin>(i)));theme::begin();assert(static_cast<int>(theme::current())==i);}
Preferences::fail=true;assert(!theme::select(theme::Skin::Modern));Preferences::fail=false;
g.linked=true;g.posMs=42700;g.songMs=189000;
for(int i=0;i<4;++i){teensylink::st.faderPermille[i]=850-i*100;teensylink::st.peakPermille[i]=680-i*145;auto&fx=teensylink::st.fx[i];fx.known=true;fx.types[0]=0;fx.types[1]=2;fx.wetPermille=300;fx.activeSlot=0;teensylink::st.track[i].fileLenMs=189000;}
teensylink::st.project.listCount=3;teensylink::st.project.listKnown=true;
strcpy(teensylink::st.project.list[0],"EVENING SESSION");strcpy(teensylink::st.project.list[1],"DUB STUDY 02");strcpy(teensylink::st.project.list[2],"LIVE SET");
for(int skin=0;skin<4;++skin){theme::select(static_cast<theme::Skin>(skin));
for(int screen=0;screen<4;++screen){setCanvas(kVisible);fillRect(0,0,1279,719,rgb565(0));
if(screen==0){drawHeader("DUB STUDY 02",kTabMixer);mixer::enter();drawTransport();}
if(screen==1){drawHeader("SETTINGS",-1,"MENU");settings::enter();}
if(screen==2){drawHeader("MENU",-1);menu::enter();}
if(screen==3){drawHeader("LOAD PROJECT",-1,"MENU");loadscreen::refresh();}
char path[512];snprintf(path,sizeof(path),"%s/%s-%d.ppm",argv[1],theme::name(theme::current()),screen);exportPreview(path);}}
mixer::enter();mixer::touchDown(99,575);mixer::touchMove(199,575);mixer::touchUp();
assert(teensylink::lastPanTrack==0 && teensylink::lastPanValue==1000);
mixer::touchDown(99,575);mixer::touchMove(-201,575);mixer::touchUp();
assert(teensylink::lastPanValue==-1000);
mixer::touchDown(99,624);assert(teensylink::lastPanValue==0);
mixer::touchDown(409,575);mixer::touchMove(459,575);mixer::touchUp();
assert(teensylink::lastPanTrack==1 && teensylink::lastPanValue==500);
assert(!settings::touchDown(10,600)); assert(settings::touchDown(100,250)); assert(theme::current()==theme::Skin::Classic);
puts("PASS: theme default, persistence, invalid values, save failure and selector hit targets; 16 source-rendered previews.");
}
''')
cmd=['clang++','-std=c++17','-ffunction-sections','-fdata-sections','-I'+str(w),'-I'+str(root/'src'),'-I'+str(root/'.pio/libdeps/esp32dev/Adafruit GFX Library'),str(w/'common.cpp'),str(w/'main.cpp')]+[str(root/'src'/f) for f in ['ui_theme.cpp','screen_mixer.cpp','screen_menu.cpp','screen_settings.cpp','screen_load.cpp']]+['-o',str(w/'preview')]
if sys.platform == 'darwin':
    cmd[1:1]=['-Wl,-dead_strip']
    sdk=Path('/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1')
    if sdk.exists(): cmd[1:1]=['-isystem',str(sdk)]
else: cmd[1:1]=['-Wl,--gc-sections']
subprocess.run(cmd,check=True);subprocess.run([str(w/'preview'),str(w)],check=True)
