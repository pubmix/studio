#pragma once
#include "Dsp.h"
#include "StemEngine.h"
#include "Jam.h"
namespace studio {
struct OutputFrame {Stereo main{},headphones{};};
class AudioEngine {
 IAudioAssets&assets_; std::array<StudioDsp,4> fx_{};float click_=0;uint64_t lastBeat_=UINT64_MAX;
public:
 bool pendingRecord=false;unsigned recordLane=0;
 void armRecord(unsigned lane,uint8_t bars){recordLane=lane<4?lane:0;pendingRecord=true;transport.record(bars);}
 StemPlayer stems;JamEngine jam;Transport transport;Master master;std::array<OneShot,4> slots{};OneShot preview;
 Retrigger retrigger=Retrigger::Unconfigured;
 explicit AudioEngine(IAudioAssets&a):assets_(a){}
 bool assetAvailable(uint32_t id)const{return assets_.length(id)>0;}
 bool loadStems(const StemSet&s){return stems.load(s,assets_);}
 bool trigger(Project&p,unsigned lane){return lane<4&&slots[lane].trigger(p.slots[lane],assets_,retrigger);}
 bool previewAsset(uint32_t id){return preview.trigger(id,assets_,Retrigger::Restart);}
 void stop(){pendingRecord=false;transport.stop();stems.rewind();jam.stop();lastBeat_=UINT64_MAX;}
 OutputFrame process(Project&,Mode musicMode,const Settings&);
};
}
