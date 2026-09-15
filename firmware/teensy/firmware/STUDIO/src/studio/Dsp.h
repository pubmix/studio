#pragma once
#include "Hal.h"
namespace studio {
struct IDsp {virtual ~IDsp()=default;virtual Stereo process(Stereo,const Fx&)=0;virtual void reset()=0;};
// Swappable implementation: Faust/DaisySP/custom adapters implement IDsp.
// Fixed memory; no allocation or blocking work in process().
class StudioDsp:public IDsp {
 static constexpr unsigned Capacity=8192;
 std::array<Stereo,Capacity> delay_{};unsigned cursor_=0;Stereo filtered_{};
public:Stereo process(Stereo,const Fx&)override;void reset()override{delay_={};cursor_=0;filtered_={};}
};
struct Meters {float left=0,right=0;bool clipping=false;};
class Master {public:Meters meters{};Stereo process(Stereo,float);void clearClip(){meters.clipping=false;}};
enum class Retrigger:uint8_t {Unconfigured,Restart,Ignore};
class OneShot {uint32_t asset_=0;uint64_t cursor_=0;bool active_=false;public:bool trigger(uint32_t,const IAudioAssets&,Retrigger);Stereo process(IAudioAssets&);bool active()const{return active_;}void stop(){active_=false;}};
}
