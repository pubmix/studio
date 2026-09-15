#include "AudioEngine.h"
namespace studio {
OutputFrame AudioEngine::process(Project&p,Mode mode,const Settings&s){transport.setBpm(p.bpm);transport.external=s.externalClock;Stereo sum{};std::array<Stereo,4> input{};
 if(pendingRecord&&transport.recording&&mode==Mode::Jam){jam.startLoop(p,recordLane,transport.ticks());pendingRecord=false;}
 if(transport.playing&&!transport.counting()){if(mode==Mode::Dub)stems.read(assets_,input);if(mode==Mode::Jam)jam.sequence(p,transport.ticks());}
 for(unsigned i=0;i<4;++i){sum=sum+input[i]*p.dub[i].volume+fx_[i].process(input[i],p.dub[i].fx);sum=sum+slots[i].process(assets_);}
 if(mode==Mode::Jam)sum=sum+jam.process(p,&assets_);
 if(transport.playing&&(p.metronome||transport.counting())){auto beat=transport.ticks()/96;if(beat!=lastBeat_){lastBeat_=beat;click_=.15f;}sum=sum+Stereo{click_,click_};click_*=-.985f;}
 Stereo cue=preview.process(assets_);if(s.previewToMain)sum=sum+cue;
 auto main=master.process(sum,p.master);auto hp=main+cue*(s.previewToMain?0:1);float peak=std::max(std::fabs(hp.l),std::fabs(hp.r));if(peak>.98f)hp=hp*(.98f/peak);
 transport.advance();return {main,hp};}
}
