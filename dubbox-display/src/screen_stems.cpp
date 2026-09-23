#include "screen_stems.h"
#include "stem_client.h"
#include "screen_keyboard.h"
#include "ui_common.h"
#include "wifi_upload.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
namespace stemscreen {
namespace {
using namespace ui;
using stemclient::Action;
enum class View { Library, Search, Choose, Job };
View view=View::Library;
stemclient::Result result;
stemclient::Item selected{};
int offset=0, mode=0;
bool compressed=false, rights=false, searching=false;
char job[25]={}, notice[120]={};
uint32_t lastPoll=0;
constexpr Rect kSearch={60,620,330,695},kLibrary={350,620,620,695},kPrevious={780,620,980,695},kNext={1000,620,1220,695};
constexpr Rect kFour={60,245,410,320},kExtended={450,245,800,320},kDynamic={840,245,1220,320};
constexpr Rect kTransfer={60,348,1220,423},kRights={60,451,1220,522},kStart={700,610,1220,690},kBack={60,610,450,690};
const char* modes[]={"four","extended","dynamic"};
void line(int y,const char* text,uint32_t color=0xdddddd){
 char clipped[100];snprintf(clipped,sizeof(clipped),"%.94s",text);
 while(strlen(clipped)&&textWidth(fontSmall(),clipped)>1150)clipped[strlen(clipped)-1]=0;
 drawText(60,y,clipped,fontSmall(),rgb565(color),rgb565(0),1160);
}
void button(const Rect& rect,const char* text,bool active=false){drawButton(rect,text,fontLarge(),rgb565(0xffffff),rawRgb565(active?theme::palette().accent:theme::palette().raised));}
void clear(){setCanvas(kVisible);fillRect(0,61,kW-1,kH-1,rgb565(0));}
bool terminal(){return !strcmp(result.state,"ready")||!strcmp(result.state,"failed")||!strcmp(result.state,"cancelled");}
void drawLibrary(){
 clear();line(80,"STEMS ON YOUR DUB-BOX",0xff9f3d);
 line(116,notice[0]?notice:searching?"YOUTUBE RESULTS - SELECT A SONG":"COMPUTER SOURCES AND RECENT SPLITS");
 for(int i=0;i<result.count;++i){
  const int y=158+i*69;Rect row={60,y,1220,y+60};
  fillRect(row.x1,row.y1,row.x2,row.y2,rgb565(0x1d2330));
  char title[68];snprintf(title,sizeof(title),"%.65s",result.items[i].title);
  while(strlen(title)&&textWidth(fontSmall(),title)>1120)title[strlen(title)-1]=0;
  drawText(76,y+4,title,fontSmall(),rgb565(0xffffff),rgb565(0x1d2330),1120);
  drawText(76,y+32,result.items[i].state,fontSmall(),rgb565(0x9aa3b5),rgb565(0x1d2330),1120);
 }
 if(!result.count&&!notice[0])line(170,"No sources yet. Search YouTube, or stage a track from the upload page.");
 button(kSearch,"YOUTUBE");button(kLibrary,"REFRESH");
 if(!searching&&offset>0)button(kPrevious,"PREVIOUS");
 if(!searching&&result.more)button(kNext,"NEXT");
}
void drawChoose(){
 clear();line(85,selected.title,0xff9f3d);
 line(133,"Choose depth. AI processing runs on your paired companion computer.");
 line(180,"I confirm that I have permission to download and separate this audio.");
 button(kFour,"SIMPLE / 4",mode==0);button(kExtended,"EXTENDED",mode==1);button(kDynamic,"DYNAMIC",mode==2);
 button(kTransfer,compressed?"TRANSFER: COMPRESSED / LOSSLESS":"TRANSFER: ORIGINAL WAV",compressed);
 button(kRights,rights?"PERMISSION CONFIRMED":"TAP TO CONFIRM PERMISSION",rights);
 line(548,notice[0]?notice:"Device playback: 4 stems. Deeper sources remain on the companion.");
 button(kBack,"BACK");button(kStart,"START SPLIT",rights);
}
void drawJob(){
 clear();line(80,result.title[0]?result.title:"YOUR STEM SPLIT",0xff9f3d);
 const bool delivering=!strcmp(result.delivery,"sending");
 line(130,notice[0]?notice:delivering?result.deliveryMessage:result.message);
 char text[110];
 if(!terminal()){
  if(result.percent>=0)snprintf(text,sizeof(text),"Processing: %d%% of this stage",result.percent);
  else snprintf(text,sizeof(text),"Processing on companion. You can leave this screen and return via the library.");
  line(175,text);
 }else{
  snprintf(text,sizeof(text),"%d source(s) ready. Four-track device mixdown: %.2f MiB",result.sourceCount,result.bytes/1048576.0);
  line(175,text);
 }
 if(result.fallback)line(214,"Selected backend failed; Demucs fallback was used.",0xffca28);
 if(delivering&&result.wavBytes){
  snprintf(text,sizeof(text),"Current WAV: %.2f MiB / transfer: %.2f MiB",result.wavBytes/1048576.0,result.wireBytes/1048576.0);line(245,text);
 }
 if(strcmp(result.delivery,"idle")&&result.delivery[0])line(283,result.deliveryMessage);
 for(int i=0;i<result.fileCount;++i){snprintf(text,sizeof(text),"%d  Saved: %s",i+1,result.files[i]);line(328+i*42,text,0x66bb6a);}
 if(!strcmp(result.delivery,"failed"))line(506,"Transfer stopped. Confirmed files remain; inspect SD before retrying.",0xff6b6b);
 else if(!strcmp(result.delivery,"done"))line(506,"Saved to SD. Assign the files to tracks using the existing file picker.",0x66bb6a);
 else if(!strcmp(result.state,"ready"))line(506,"Ready: choose transfer mode, then SAVE TO DUB-BOX.");
 if(!strcmp(result.state,"ready")&&!delivering){
  Rect transfer={60,546,1220,596};button(transfer,compressed?"COMPRESSED / LOSSLESS":"ORIGINAL WAV",compressed);
 }
 button(kBack,"LIBRARY");
 if(!terminal())button(kStart,"CANCEL SPLIT");
 else if(!strcmp(result.state,"ready")&&!delivering)button(kStart,!strcmp(result.delivery,"done")?"SAVE AGAIN":"SAVE TO DUB-BOX",true);
}
void ask(Action action,const char* value="",const char* kind=""){
 if(stemclient::request(action,value,kind,modes[mode],compressed,offset)){
  snprintf(notice,sizeof(notice),"%s",action==Action::Create?"Starting split...":action==Action::Send?"Starting transfer...":"Connecting to companion...");
 }else snprintf(notice,sizeof(notice),"Please wait for the current request.");
 if(view==View::Library)drawLibrary();else if(view==View::Choose)drawChoose();else if(view==View::Job)drawJob();
}
void library(){view=View::Library;searching=false;notice[0]=0;result.count=0;ask(Action::Library);}
}
void enter(){
 offset=0;view=View::Library;searching=false;result={};notice[0]=0;
 if(!stemclient::configured()){
  snprintf(notice,sizeof(notice),"Pair once on the Wi-Fi upload page using your computer's LAN address.");drawLibrary();
 }else library();
}
void touchDown(int x,int y){
 if(view==View::Search){
  auto action=keyboard::touchDown(x,y);
  if(action==keyboard::Action::Cancel){library();}
  else if(action==keyboard::Action::Create){
   if(!keyboard::name()[0])keyboard::showMessage("ENTER A SONG OR ARTIST");
   else {view=View::Library;searching=true;result.count=0;ask(Action::Search,keyboard::name());}
  }
  return;
 }
 if(stemclient::busy())return;
 if(view==View::Library){
  if(inRect(kSearch,x,y)){view=View::Search;clear();keyboard::enter("SEARCH YOUTUBE", "SEARCH",64);}
  else if(inRect(kLibrary,x,y)){offset=0;library();}
  else if(!searching&&offset>0&&inRect(kPrevious,x,y)){offset-=6;library();}
  else if(!searching&&result.more&&inRect(kNext,x,y)){offset+=6;library();}
  else for(int i=0;i<result.count;++i){const int row=158+i*69;
   if(x>=60&&x<=1220&&y>=row&&y<=row+60){
    selected=result.items[i];notice[0]=0;
    if(!strcmp(selected.kind,"job")){snprintf(job,sizeof(job),"%s",selected.id);view=View::Job;result={};ask(Action::Status,job);}
    else {view=View::Choose;rights=false;drawChoose();}
    break;
   }
  }
 }else if(view==View::Choose){
  if(inRect(kBack,x,y)){library();return;}
  if(inRect(kFour,x,y))mode=0;
  if(inRect(kExtended,x,y))mode=1;
  if(inRect(kDynamic,x,y))mode=2;
  if(inRect(kTransfer,x,y))compressed=!compressed;
  if(inRect(kRights,x,y))rights=!rights;
  if(inRect(kStart,x,y)&&rights){ask(Action::Create,selected.id,selected.kind);return;}
  drawChoose();
 }else if(view==View::Job){
  if(inRect(kBack,x,y)){library();return;}
  if(!strcmp(result.state,"ready")&&strcmp(result.delivery,"sending")&&x>=60&&x<=1220&&y>=546&&y<=596){compressed=!compressed;drawJob();}
  if(inRect(kStart,x,y)){
   if(!terminal())ask(Action::Cancel,job);
   else if(!strcmp(result.state,"ready")&&strcmp(result.delivery,"sending"))ask(Action::Send,job);
  }
 }
}
void update(){
 stemclient::Result incoming;
 if(stemclient::take(incoming)){
  lastPoll=millis();
  if(!incoming.ok){snprintf(notice,sizeof(notice),"%s",incoming.error);}
  else {
   notice[0]=0;
   result=incoming;
   if(incoming.action==Action::Create){snprintf(job,sizeof(job),"%s",result.id);view=View::Job;}
  }
  if(view==View::Library)drawLibrary();else if(view==View::Choose)drawChoose();else if(view==View::Job)drawJob();
 }
 if(view==View::Job&&job[0]&&!stemclient::busy()&&millis()-lastPoll>=2500){
  lastPoll=millis();stemclient::request(Action::Status,job);
 }
}
}
