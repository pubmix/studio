#pragma once
#include "Hal.h"
namespace studio {
class DebouncedButton {
 bool stable_=false,candidate_=false;uint32_t changed_=0;
public:bool press(bool down,uint32_t ms){if(down!=candidate_){candidate_=down;changed_=ms;}if(candidate_!=stable_&&uint32_t(ms-changed_)>=12){stable_=candidate_;return stable_;}return false;}
};
class QuadratureEncoder {
 uint8_t previous_=0;int accumulated_=0;
public:int update(bool a,bool b){uint8_t next=(unsigned(a)<<1)|unsigned(b);constexpr int8_t movement[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};if((next^previous_)==3)accumulated_=0;else accumulated_+=movement[(previous_<<2)|next];previous_=next;if(accumulated_>=4){accumulated_=0;return 1;}if(accumulated_<=-4){accumulated_=0;return -1;}return 0;}
};
struct FaderCalibration {int minimum=0,maximum=1023;bool reversed=false;float normalized(int raw)const{if(maximum<=minimum)return 0;float x=unit(float(raw-minimum)/float(maximum-minimum));return reversed?1-x:x;}};
}
