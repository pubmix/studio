#pragma once
#include "Transport.h"
#include "Hal.h"
namespace studio {
inline int keyboardNote(const Project&p,unsigned keyIndex){return keyIndex<24?int(p.octave)*12+int(keyIndex):-1;}
int scaleNote(int note,uint8_t key,Scale scale,bool lock);
class JamEngine {
 struct Voice {float phase=0,env=0,velocity=0;uint8_t note=60;bool held=false;double samplePosition=0;float filtered=0;};
 std::array<std::array<Voice,8>,4> voices_{};std::array<uint64_t,4> loopStart_{};uint32_t noise_=1;uint64_t lastTick_=UINT64_MAX;
public:
 void note(Project&,unsigned lane,uint8_t note,float velocity,bool on,uint64_t tick,bool capture=true);
 void stop(); void sequence(Project&,uint64_t tick);Stereo process(const Project&,IAudioAssets* assets=nullptr);
 bool startLoop(Project&,unsigned,uint64_t tick=0);void finishLoop(Project&,unsigned,uint32_t ticks);
};
}
