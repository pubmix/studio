from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[1]
work=Path(sys.argv[1]).resolve()
# Reuse the existing actual-font/software-canvas host setup and validate original screens.
subprocess.run([sys.executable,str(root/'tests/render_ui.py'),str(work)],check=True)
(work/'stems_main.cpp').write_text(r'''#include "ui_common.h"
#include "screen_stems.h"
#include "screen_menu.h"
#include "stem_client.h"
#include "Preferences.h"
#include <cassert>
#include <cstring>
#include <cstdio>
uint8_t Preferences::stored=255;bool Preferences::fail=false;
namespace ui{void exportPreview(const char*);}
namespace teensylink{void sendVolume(int){}void sendSeek(uint32_t){}}
namespace stemclient {
Result queued; bool available=false;int requests=0;Action last;char selectedMode[12];bool zipped=false;
bool configured(){return true;}bool busy(){return false;}
bool take(Result& r){if(!available)return false;r=queued;available=false;return true;}
bool request(Action a,const char* value,const char*,const char* mode,bool compressed,int){
 last=a;++requests;strcpy(selectedMode,mode);zipped=compressed;queued={};queued.ok=true;queued.action=a;
 if(a==Action::Library){queued.count=2;strcpy(queued.items[0].kind,"source");strcpy(queued.items[0].id,"123456789012345678901234");strcpy(queued.items[0].title,"STAGED SONG.WAV");strcpy(queued.items[0].state,"choose split");strcpy(queued.items[1].kind,"job");strcpy(queued.items[1].id,"abcdefghijklmnopqrstuvwx");strcpy(queued.items[1].title,"EARLIER SPLIT");strcpy(queued.items[1].state,"ready");}
 else{strcpy(queued.id,"abcdefghijklmnopqrstuvwx");strcpy(queued.title,"STAGED SONG.WAV");strcpy(queued.state,"ready");strcpy(queued.message,"Stems ready");strcpy(queued.delivery,"idle");queued.sourceCount=6;queued.bytes=1411376;}
 if(a==Action::Send){strcpy(queued.delivery,"sending");strcpy(queued.deliveryMessage,"Sending vocals");}
 available=true;return true;
}
}
void save(const char* folder,const char* name){char p[1024];snprintf(p,sizeof(p),"%s/%s.ppm",folder,name);ui::exportPreview(p);}
int main(int argc,char**argv){
 using namespace ui;theme::begin();g.linked=true;
 assert(menu::touchDown(500,390)==Screen::Stems);
 drawHeader("STEM SPLITTER",-1,"MENU");stemscreen::enter();stemscreen::update();save(argv[1],"stems-library");
 stemscreen::touchDown(100,175);int before=stemclient::requests;stemscreen::touchDown(900,645);assert(stemclient::requests==before);
 stemscreen::touchDown(600,280);stemscreen::touchDown(600,385);stemscreen::touchDown(600,480);save(argv[1],"stems-controls");
 stemscreen::touchDown(900,645);assert(stemclient::last==stemclient::Action::Create);assert(!strcmp(stemclient::selectedMode,"extended"));assert(stemclient::zipped);
 stemscreen::update();save(argv[1],"stems-ready");stemscreen::touchDown(900,645);assert(stemclient::last==stemclient::Action::Send);assert(stemclient::zipped);stemscreen::update();save(argv[1],"stems-transfer");
 stemclient::queued={};stemclient::queued.ok=true;stemclient::queued.action=stemclient::Action::Status;strcpy(stemclient::queued.state,"separating");stemclient::available=true;stemscreen::update();stemscreen::touchDown(900,645);assert(stemclient::last==stemclient::Action::Cancel);
 puts("PASS: real menu hit target, rights gate, depth/transfer choices, job state, delivery, cancellation; 4 actual-code stem previews.");
}
''')
cmd=['clang++','-std=c++17','-ffunction-sections','-fdata-sections','-I'+str(work),'-I'+str(root/'src'),'-I'+str(root/'.pio/libdeps/esp32dev/Adafruit GFX Library'),str(work/'common.cpp'),str(work/'stems_main.cpp')]+[str(root/'src'/n) for n in ['ui_theme.cpp','screen_menu.cpp','screen_stems.cpp','screen_keyboard.cpp']]+['-o',str(work/'stems-preview')]
if sys.platform=='darwin':cmd[1:1]=['-Wl,-dead_strip','-isystem','/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1']
else:cmd[1:1]=['-Wl,--gc-sections']
subprocess.run(cmd,check=True);subprocess.run([str(work/'stems-preview'),str(work)],check=True)
