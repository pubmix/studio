#include "Transport.h"
namespace studio {
void Transport::tick(){++ticks_;if(remaining_&&!--remaining_){recording=true;ticks_=0;frames=0;}}
bool Transport::advance(){if(!playing)return false;++frames;if(external)return false;phase_+=uint64_t(std::llround(bpm*1000))*96;constexpr uint64_t period=uint64_t(60)*SampleRate*1000;if(phase_>=period){phase_-=period;tick();return true;}return false;}
void Transport::midiClock(){if(external&&playing)for(int i=0;i<4;++i)tick();}
}
