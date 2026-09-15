#include "App.h"
#include "Persistence.h"
#include <algorithm>
namespace studio {
void History::push(std::array<Project,8>&a,unsigned&n,const Project&p){if(n==a.size()){for(unsigned i=1;i<n;++i)a[i-1]=a[i];--n;}a[n++]=p;}
bool History::undo(Project&p){if(!u_)return false;push(redo_,r_,p);p=undo_[--u_];return true;}
bool History::redo(Project&p){if(!r_)return false;push(undo_,u_,p);p=redo_[--r_];return true;}
void App::mode(Mode m){session.mode=m;session.page=Page::Performance;if(m==Mode::Dub||m==Mode::Jam){if(session.musicMode!=m)audio_.jam.stop();session.musicMode=m;}}
void App::control(const ControlEvent&e){
 switch(e.control){case Control::Play:audio_.transport.play();return;case Control::Stop:for(unsigned i=0;i<4;++i)if(project.tracks[i].loop.recording)audio_.jam.finishLoop(project,i,uint32_t(audio_.transport.ticks()));audio_.stop();return;case Control::Record:if(session.musicMode==Mode::Jam){edit();auto&l=project.tracks[session.lane].loop;if(l.recording){audio_.jam.finishLoop(project,session.lane,uint32_t(audio_.transport.ticks()));audio_.transport.recording=false;}else audio_.armRecord(session.lane,project.countInBars);}else audio_.transport.record(project.countInBars);return;case Control::Tap:audio_.transport.tap(e.timeMs);edit();project.bpm=audio_.transport.bpm;return;default:break;}
 if(e.lane>=4||!std::isfinite(e.value))return;
 // System pages inherit the last music environment. DAW physical semantics await review.
 if(session.mode==Mode::Daw)return;
 auto musical=session.musicMode;if(musical==Mode::Jam&&!settings.jamPhysicalMapping)return;
 if(session.page==Page::Browser){if(e.lane!=session.lane)return;if(e.control==Control::EncoderTurn){int count=musical==Mode::Dub?int(Effect::Count):3;int n=int(session.browserIndex)+int(e.value);session.browserIndex=uint8_t((n%count+count)%count);return;}if(e.control==Control::EncoderClick){edit();if(musical==Mode::Dub)project.dub[e.lane].fx.effect=Effect(session.browserIndex);else project.tracks[e.lane].sound.kind=SoundKind(session.browserIndex);session.page=Page::Performance;return;}}
 if(e.control==Control::EncoderClick){session.lane=e.lane;session.browserIndex=0;session.page=Page::Browser;return;}
 if(e.control==Control::Bottom){if(musical==Mode::Dub)audio_.trigger(project,e.lane);else{edit();auto&loop=project.tracks[e.lane].loop;if(loop.recording)audio_.jam.finishLoop(project,e.lane,uint32_t(audio_.transport.ticks()));else audio_.jam.startLoop(project,e.lane,audio_.transport.ticks());}return;}
 edit();if(musical==Mode::Dub){auto&l=project.dub[e.lane];if(e.control==Control::Top)l.fx.enabled=!l.fx.enabled;if(e.control==Control::EncoderTurn)l.fx.intensity=unit(l.fx.intensity+e.value*.01f);if(e.control==Control::Fader)l.volume=unit(e.value);}else{auto&t=project.tracks[e.lane];if(e.control==Control::Top){session.lane=e.lane;t.armed=!t.armed;}if(e.control==Control::EncoderTurn)t.sound.macro=unit(t.sound.macro+e.value*.01f);if(e.control==Control::Fader)t.volume=unit(e.value);}
}
bool App::openDub(IStemEngine&e){StemSet s;if(!e.prepared(s)||!audio_.loadStems(s))return false;audio_.stop();edit();project.stemSet=s.id;mode(Mode::Dub);return true;}
void App::openFiles(FileContext::Action a,uint8_t lane){if(lane>=4)return;files={a,lane,session.mode};mode(Mode::Files);}
bool App::chooseAsset(uint32_t id,SoundKind kind){if(!id||files.target>=4)return false;if((files.action==FileContext::Action::AssignSlot||kind==SoundKind::Sample)&&!audio_.assetAvailable(id))return false;if(files.action==FileContext::Action::Preview)return audio_.previewAsset(id);edit();if(files.action==FileContext::Action::AssignSlot)project.slots[files.target]=id;else if(files.action==FileContext::Action::LoadTrack){project.tracks[files.target].sound.asset=id;project.tracks[files.target].sound.kind=kind;}else return false;mode(files.returnMode);return true;}
void App::midi(uint8_t s,uint8_t a,uint8_t b){if(s==0xF8){audio_.transport.midiClock();return;}if(s==0xFA&&settings.externalClock){audio_.stop();audio_.transport.play();return;}if(s==0xFC&&settings.externalClock){audio_.stop();return;}if((s&15)!=settings.midiChannel-1)return;if((s&0xF0)==0x90)note(a,b/127.f,b>0);if((s&0xF0)==0x80)note(a,0,false);}
void App::note(uint8_t n,float v,bool on){if(session.musicMode!=Mode::Jam)return;audio_.jam.note(project,session.lane,n,v,on,audio_.transport.ticks());if(project.tracks[session.lane].loop.recording)session.dirty=true;}
void App::selectEffect(Effect e){if(e>=Effect::Count)return;edit();project.dub[session.lane].fx.effect=e;session.page=Page::Performance;}
void App::designFx(const Fx&f){if(f.effect>=Effect::Count||!std::isfinite(f.timeSeconds)||f.timeSeconds<=0||f.timeSeconds>8)return;edit();project.dub[session.lane].fx=f;project.dub[session.lane].fx.intensity=unit(f.intensity);}
void App::designSound(const Sound&s){if(unsigned(s.kind)>3||!std::isfinite(s.attack)||!std::isfinite(s.release)||s.attack<.001f||s.release<.005f)return;edit();project.tracks[session.lane].sound=s;}
void App::setStep(unsigned i,const Step&s){if(i>=16)return;edit();project.tracks[session.lane].pattern.steps[i]=s;}
bool App::save(IStorage&s,const char*path){if(!saveProject(s,path,project))return false;session.dirty=false;return true;}
bool App::load(IStorage&s,const char*path){Project p;if(!loadProject(s,path,p))return false;audio_.stop();edit();project=p;session.dirty=false;return true;}
bool App::autosave(IStorage&s,uint64_t ms){if(!session.dirty||ms-lastAutosave_<settings.autosaveMs)return false;if(!saveProject(s,"System/recovery.studio",project))return false;lastAutosave_=ms;return true;}
bool App::recover(IStorage&s){return load(s,"System/recovery.studio");}
}
