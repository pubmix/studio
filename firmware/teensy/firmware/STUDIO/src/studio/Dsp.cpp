#include "Dsp.h"
#include <algorithm>
namespace studio {
Stereo StudioDsp::process(Stereo in,const Fx&fx){
 const float wet=unit(fx.wet.at(fx.intensity)),fb=std::min(.95f,unit(fx.feedback.at(fx.intensity)));
 const Stereo send=fx.enabled?in:Stereo{};
 float tone=std::max(.001f,unit(fx.tone.at(fx.intensity)));
 filtered_.l+=tone*(send.l-filtered_.l);filtered_.r+=tone*(send.r-filtered_.r);
 switch(fx.effect){
 case Effect::LowPass:return fx.enabled?filtered_*wet:Stereo{};
 case Effect::HighPass:return fx.enabled?Stereo{send.l-filtered_.l,send.r-filtered_.r}*wet:Stereo{};
 case Effect::Drive:{float d=1+20*unit(fx.drive.at(fx.intensity));return {std::tanh(send.l*d)*wet,std::tanh(send.r*d)*wet};}
 default:break;
 }
 // Test implementation supports up to 185 ms; longer echo requires external RAM or another IDsp.
 unsigned n=unsigned(std::max(1.f,std::min(float(Capacity-1),(std::isfinite(fx.timeSeconds)?fx.timeSeconds:.18f)*SampleRate)));
 Stereo tail=delay_[(cursor_+Capacity-n)%Capacity];
 if(fx.effect==Effect::RoomReverb){Stereo b=delay_[(cursor_+Capacity-n/2-1)%Capacity];tail=tail*.65f+b*.35f;}
 float drive=1+4*unit(fx.drive.at(fx.intensity));
 Stereo input=fx.effect==Effect::DubEcho?filtered_:send;
 delay_[cursor_]={std::tanh(input.l*drive+tail.l*fb),std::tanh(input.r*drive+tail.r*fb)};
 cursor_=(cursor_+1)%Capacity;
 float width=unit(fx.width.at(fx.intensity)),mid=(tail.l+tail.r)*.5f,side=(tail.l-tail.r)*.5f*width;
 return Stereo{mid+side,mid-side}*wet;
}
Stereo Master::process(Stereo x,float volume){x=x*unit(volume);if(!std::isfinite(x.l)||!std::isfinite(x.r)){meters.clipping=true;return {};}
 meters.clipping|=std::fabs(x.l)>1||std::fabs(x.r)>1;
 meters.left=std::max(std::fabs(x.l),meters.left*.9995f);meters.right=std::max(std::fabs(x.r),meters.right*.9995f);
 // Linked instantaneous safety gain, no lookahead. Transparent below ceiling.
 float peak=std::max(std::fabs(x.l),std::fabs(x.r));if(peak>.98f)x=x*(.98f/peak);return x;
}
bool OneShot::trigger(uint32_t id,const IAudioAssets&a,Retrigger p){if(!a.length(id))return false;if(active_&&p!=Retrigger::Restart)return false;asset_=id;cursor_=0;active_=true;return true;}
Stereo OneShot::process(IAudioAssets&a){Stereo x{};if(active_){a.frame(asset_,cursor_++,x);if(cursor_>=a.length(asset_))active_=false;}return x;}
}
