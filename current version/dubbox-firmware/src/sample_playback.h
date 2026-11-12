#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "samples/studio_samples.h"

namespace dubbox {
// Allocation-free PCM cursor, kept separate from AudioStream for host boundary tests.
// Samples are signed mono 16-bit PCM in memory-mapped flash. No SD reads or loops.
class SampleCursor {
 public:
  bool start(const studio_samples::Sample& sample, int midi, float outputRate) {
    stop();
    if (!sample.pcm || sample.frames < 2 || !sample.sampleRate || !isfinite(outputRate) || outputRate <= 0 || midi < 0 || midi > 127) return false;
    double ratio = double(sample.sampleRate) / outputRate * pow(2.0, double(midi-int(sample.rootMidi))/12.0);
    if (!isfinite(ratio) || ratio <= 0 || ratio > 128.0) return false;
    pcm_=sample.pcm; frames_=sample.frames; phase_=0;
    step_=uint32_t(ratio*65536.0+0.5); if (!step_) step_=1;
    active_=true; return true;
  }
  void stop() {active_=false;pcm_=nullptr;frames_=0;phase_=0;fade_=0;}
  void choke() {if(active_)fade_=176;} // ~4 ms at the Teensy audio rate
  bool active() const {return active_;}
  uint32_t step() const {return step_;}
  int16_t next() {
    if(!active_) return 0;
    uint32_t i=uint32_t(phase_>>16);
    if(i>=frames_) {stop();return 0;}
    int32_t a=pcm_[i],b=(i+1<frames_)?pcm_[i+1]:0;
    int32_t v=a+int32_t((int64_t(b-a)*int64_t(phase_&65535))>>16);
    if(fade_) {v=v*int32_t(fade_)/176; if(!--fade_)active_=false;}
    phase_+=step_;
    if((phase_>>16)>=frames_)active_=false;
    if(v>32767)v=32767;
    if(v< -32768)v=-32768;
    return int16_t(v);
  }
  void skip(size_t count) {while(count-- && active_) (void)next();}
 private:
  const int16_t* pcm_=nullptr;
  uint32_t frames_=0,step_=65536,fade_=0;
  uint64_t phase_=0;
  bool active_=false;
};
}
