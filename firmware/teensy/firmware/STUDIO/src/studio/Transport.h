#pragma once
#include "Model.h"
namespace studio {
// 96 ticks per quarter note. All music modes consume this single frame clock.
class Transport {
 uint64_t phase_=0; uint64_t ticks_=0; uint32_t remaining_=0; uint64_t lastTap_=0; bool haveTap_=false;
public:
 bool playing=false,recording=false,external=false; float bpm=92; uint64_t frames=0;
 void play(){playing=true;} void stop(){playing=recording=false;remaining_=0;phase_=0;ticks_=frames=0;}
 void record(uint8_t bars){playing=true;recording=bars==0;remaining_=unsigned(bars)*384;phase_=0;ticks_=frames=0;}
 void setBpm(float b){if(std::isfinite(b)&&b>=20&&b<=300)bpm=b;}
 void tap(uint64_t ms){if(haveTap_&&ms>lastTap_&&ms-lastTap_>=200&&ms-lastTap_<=3000)setBpm(60000.f/float(ms-lastTap_));lastTap_=ms;haveTap_=true;}
 bool advance(); void midiClock(); uint64_t ticks()const{return ticks_;} bool counting()const{return remaining_>0;}
private: void tick();
};
}
