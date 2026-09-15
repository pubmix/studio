#pragma once
#include "Model.h"
#include <cstddef>
namespace studio {
struct Stereo {float l=0,r=0;};
inline Stereo operator+(Stereo a,Stereo b){return {a.l+b.l,a.r+b.r};}
inline Stereo operator*(Stereo a,float b){return {a.l*b,a.r*b};}
enum class Control:uint8_t {Top,EncoderTurn,EncoderClick,Fader,Bottom,Play,Stop,Record,Tap};
struct ControlEvent {Control control;uint8_t lane=0;float value=0;uint64_t timeMs=0;};
struct IControls {virtual ~IControls()=default;virtual bool poll(ControlEvent&)=0;};
struct IMidi {virtual ~IMidi()=default;virtual bool read(uint8_t& status,uint8_t& data1,uint8_t& data2)=0;virtual void send(uint8_t,uint8_t,uint8_t)=0;};
struct IAudioAssets {virtual ~IAudioAssets()=default;virtual uint64_t length(uint32_t asset)const=0;virtual bool frame(uint32_t asset,uint64_t position,Stereo& out)=0;};
// Storage operations execute on the foreground task, never in an audio interrupt.
struct IStorage {virtual ~IStorage()=default;virtual bool read(const char*,uint8_t*,size_t,size_t&)=0;virtual bool atomicWrite(const char*,const uint8_t*,size_t)=0;};
struct IDisplay {virtual ~IDisplay()=default;virtual void label(int x,int y,const char*,uint32_t)=0;virtual void clear(uint32_t)=0;};
struct BoardConfig {int display=-1,codec=-1,headphones=-1; std::array<int,4> top{{-1,-1,-1,-1}},encoderA{{-1,-1,-1,-1}},encoderB{{-1,-1,-1,-1}},encoderPush{{-1,-1,-1,-1}},fader{{-1,-1,-1,-1}},bottom{{-1,-1,-1,-1}};};
}
