#pragma once
#include <stdint.h>

namespace studio_arp {
enum class Pattern : uint8_t { Up, Down, UpDown, Random };
struct Event { bool on; uint8_t midi; };
using Emit = void (*)(void*, Event);
struct Config {
  Pattern pattern = Pattern::Up;
  uint8_t octaves = 1;
  uint8_t stepsPerBeat = 4; // 1/4, 1/8, 1/16, etc.; quarter-note beat
  uint8_t gatePercent = 75;
  uint16_t bpm = 120;
};

// Call from one control-loop owner; never concurrently from an audio interrupt.
class Arpeggiator {
 public:
  static constexpr uint8_t capacity = 6;
  explicit Arpeggiator(Emit emit=nullptr, void* context=nullptr):emit_(emit),context_(context){}
  void bind(Emit emit,void* context){stop();emit_=emit;context_=context;}
  bool hold(uint8_t midi) {
    if(midi>127) return false;
    for(uint8_t i=0;i<count_;++i) if(held_[i]==midi) return true;
    if(count_==capacity) return false; // reject newest, leave existing notes intact
    uint8_t i=count_++;
    while(i>0 && held_[i-1]>midi){held_[i]=held_[i-1];--i;}
    held_[i]=midi; phase_=0; return true;
  }
  void release(uint8_t midi) {
    for(uint8_t i=0;i<count_;++i) if(held_[i]==midi){
      if(active_ && source_==midi) off();
      for(uint8_t j=i+1;j<count_;++j) held_[j-1]=held_[j];
      --count_;phase_=0;return;
    }
  }
  void clear(){off();count_=0;phase_=0;}
  void stop(){off();running_=false;phase_=0;}
  void start(uint32_t now){off();running_=true;phase_=0;next_=now;}
  bool configure(Config c,uint32_t now){
    if(static_cast<unsigned>(c.pattern)>3 || c.octaves<1 || c.octaves>3 ||
       c.stepsPerBeat<1 || c.stepsPerBeat>8 || c.gatePercent<1 || c.gatePercent>100 ||
       c.bpm<30 || c.bpm>300) return false;
    off();config_=c;phase_=0;next_=now;return true;
  }
  void seed(uint32_t value){random_=value?value:1;}
  void tick(uint32_t now){
    if(active_ && due(now,offAt_)) off();
    if(!running_ || !due(now,next_)) return;
    const uint32_t period=60000000UL/(config_.bpm*config_.stepsPerBeat);
    const uint32_t missed=(now-next_)/period;
    next_+=(missed+1)*period; // bounded work even after a delayed control loop
    phase_+=missed;
    off();
    uint8_t notes[18],sources[18],n=0;
    for(uint8_t o=0;o<config_.octaves;++o)for(uint8_t i=0;i<count_;++i){
      unsigned pitch=held_[i]+12*o;
      if(pitch>127) continue;
      uint8_t at=0;while(at<n && notes[at]<pitch)++at;
      if(at<n && notes[at]==pitch) continue;
      for(uint8_t j=n;j>at;--j){notes[j]=notes[j-1];sources[j]=sources[j-1];}
      notes[at]=static_cast<uint8_t>(pitch);sources[at]=held_[i];++n;
    }
    if(n==0 || !emit_) return;
    uint8_t index=0;
    switch(config_.pattern){
      case Pattern::Up:index=phase_%n;break;
      case Pattern::Down:index=n-1-phase_%n;break;
      case Pattern::UpDown:{unsigned cycle=n>1?2*(n-1):1;unsigned p=phase_%cycle;index=p<n?p:cycle-p;break;}
      case Pattern::Random:random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;index=random_%n;break;
    }
    ++phase_;midi_=notes[index];source_=sources[index];active_=true;
    offAt_=now+(period*config_.gatePercent)/100;
    emit_(context_,{true,midi_});
  }
  bool sounding()const{return active_;}
  uint8_t heldCount()const{return count_;}
 private:
  static bool due(uint32_t now,uint32_t deadline){return static_cast<int32_t>(now-deadline)>=0;}
  void off(){if(active_){active_=false;if(emit_)emit_(context_,{false,midi_});}}
  Emit emit_;void* context_;Config config_;
  uint8_t held_[capacity]={},count_=0,midi_=0,source_=0;
  bool running_=false,active_=false;
  uint32_t next_=0,offAt_=0,phase_=0,random_=1;
};
}
