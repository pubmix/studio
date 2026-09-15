#include "BoardConfig.h"
#include "TeensyAdapter.h"
#include "SdAdapter.h"
#include "src/studio/App.h"
#include "src/studio/Ui.h"
using namespace studio;
SdAssets assets;
// Delay buffers live in Teensy RAM2, preserving RAM1 for code and stack.
DMAMEM AudioEngine engine(assets);
App app(engine);
SdRawStorage rawStorage;
JournalStorage storage(rawStorage);
AudioQueue queue;
StudioAudioStream audio(queue);
#if STUDIO_ENABLE_I2S
AudioOutputI2S output;
AudioConnection left(audio,0,output,0),right(audio,1,output,1);
#endif
// No headphone connection until separate headphone output hardware is specified.
void setup(){
 Serial.begin(115200);AudioMemory(20);
 app.status.audioReady=STUDIO_ENABLE_I2S;
#if STUDIO_ENABLE_SD
 app.status.storageReady=SD.begin(BUILTIN_SDCARD);
 if(app.status.storageReady){StemSet set;set.id=1;bool valid=true;for(unsigned i=0;i<4;++i){char path[96];snprintf(path,sizeof path,"STUDIO/Imports/Prepared/%s",StemFiles[i]);valid&=assets.add(100+i,path,&set.stems[i]);}if(valid){PreparedStemEngine prepared;prepared.submit(set,assets);app.openDub(prepared);}}
 if(app.status.storageReady){for(unsigned i=0;i<7;++i){char path[128];snprintf(path,sizeof path,"STUDIO/Samples/Factory/%s.wav",FactorySamples[i]);assets.add(i+1,path);}Project recovered;if(loadProject(storage,"System/recovery.studio",recovered))app.session.page=Page::Recovery;}
#endif
 Serial.println("STUDIO prototype: p play, s stop, r record, d dub, j jam, 1-4 lane, f FX, +/- intensity, k note, n note off");
}
void loop(){
 if(Serial.available()){char c=Serial.read();ControlEvent e{Control::Play,app.session.lane,0,millis()};bool event=true;switch(c){case 'p':break;case 's':e.control=Control::Stop;break;case 'r':e.control=Control::Record;break;case 'f':e.control=Control::Top;break;case '+':e.control=Control::EncoderTurn;e.value=1;break;case '-':e.control=Control::EncoderTurn;e.value=-1;break;default:event=false;break;}if(event)app.control(e);if(c=='d')app.mode(Mode::Dub);if(c=='j')app.mode(Mode::Jam);if(c>='1'&&c<='4')app.session.lane=c-'1';if(c=='k')app.note(60,.8f,true);if(c=='n')app.note(60,0,false);if(c=='y'&&app.session.page==Page::Recovery){app.recover(storage);app.session.page=Page::Performance;}}
 if(auto block=queue.beginWrite()){for(auto&f:*block)f=engine.process(app.project,app.session.musicMode,app.settings);queue.commit();}
#if !STUDIO_ENABLE_I2S
 // Diagnostic build drains at approximately real-time; it does not drive pins.
 static elapsedMicros elapsed; if(elapsed>=2903){elapsed-=2903;if(queue.beginRead())queue.release();}
#endif
 if(app.status.storageReady)app.autosave(storage,millis());
 app.status.underruns=queue.underruns.load()+engine.stems.underruns;
}
