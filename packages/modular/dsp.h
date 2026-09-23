#pragma once
#include "patch.h"
#include <math.h>
namespace modular {
class Engine {
 public:
  static constexpr int Voices=6;
  Patch patch;
  Engine() { configure(patch); }
  bool configure(const Patch& p) {
    int next[Nodes]; if(!valid(p,next)) return false;
    patch=p; for(int i=0;i<Nodes;++i) order[i]=next[i];
    attack=1.f/((.001f+1.5f*squareValue(p.value[Attack]/100.f))*44100.f);
    decay=1.f/((.001f+2.f*squareValue(p.value[Decay]/100.f))*44100.f);
    release=1.f/((.001f+3.f*squareValue(p.value[Release]/100.f))*44100.f);
    alpha=1.f-expf(-6.2831853f*60.f*powf(200.f,p.value[Frequency]/100.f)/44100.f);
    lfoInc=(.05f+12.f*squareValue(p.value[Rate]/100.f))/44100.f;
    ratio=powf(2.f,(int(p.value[Detune])-50)/1200.f);
    return true;
  }
  void on(int voice,int midi) {
    if(voice<0||voice>=Voices||midi<0||midi>127) return;
    Voice& v=voices[voice]; v=Voice(); v.active=true; v.gate=true;
    v.inc=440.f*powf(2.f,(midi-69)/12.f)/44100.f;
  }
  void off(int voice) { if(voice>=0&&voice<Voices) voices[voice].gate=false; }
  void silence() { for(auto& v:voices) v=Voice(); }
  bool sounding() const {for(const auto& v:voices) if(v.active) return true; return false;}
  float sample() {
    float total=0;
    for(auto& v:voices) {
      if(!v.active) continue;
      if(!v.gate) {v.env=fmaxf(0,v.env-release); if(v.env==0) {v.active=false;continue;}}
      else if(!v.decaying) {v.env=fminf(1,v.env+attack); if(v.env>=1) v.decaying=true;}
      else v.env=fmaxf(patch.value[Sustain]/100.f,v.env-decay);
      float values[Nodes]={};
      for(int k=0;k<Nodes;++k) {
        int n=order[k];
        switch(n) {
          case Osc1: case Osc2: {
            int o=n==Osc1?0:1; float d=v.inc*(o?ratio:1.f)*(1.f+input(values,o?Pitch2:Pitch1)*.08f);
            d=fminf(.45f,fmaxf(.00001f,d)); v.phase[o]+=d; if(v.phase[o]>=1) v.phase[o]-=1;
            values[n]=osc(v.phase[o],d,patch.value[o?Wave2:Wave1]); break;
          }
          case Noise: random=random*1664525u+1013904223u; values[n]=(int32_t(random)/2147483648.f)*.5f; break;
          case Lfo: v.lfo+=lfoInc; if(v.lfo>=1)v.lfo-=1; values[n]=1.f-4.f*fabsf(v.lfo-.5f); break;
          case Env: values[n]=v.env; break;
          case Mix: values[n]=.5f*(input(values,MixA)+input(values,MixB));break;
          case Filter: {
            float a=fminf(.9f,fmaxf(.001f,alpha*(1.f+input(values,Cutoff)*3.f)));
            v.filter+=a*(input(values,FilterIn)-v.filter); values[n]=v.filter;break;
          }
          case Vca: values[n]=input(values,VcaIn)*fmaxf(0,fminf(1,input(values,Gain)));break;
          case Out: values[n]=input(values,Output);break;
        }
      }
      // Note-gated safety envelope prevents a disconnected gate cable from leaving a drone.
      total+=values[Out]*v.env;
    }
    return fmaxf(-.98f,fminf(.98f,total*.18f*patch.value[Level]/100.f));
  }
  // Process one module across the entire block before moving to the next.
  // This keeps graph dispatch outside the audio sample loop.
  void render(float* out, int count) {
    if(count<1 || count>128) return;
    for(int i=0;i<count;++i) out[i]=0;
    float signal[Nodes][128];
    const float sustain=patch.value[Sustain]*.01f;
    for(auto& v:voices) {
      if(!v.active) continue;
      for(int i=0;i<count;++i) {
        if(!v.gate) {v.env=fmaxf(0,v.env-release);if(v.env==0)v.active=false;}
        else if(!v.decaying) {v.env=fminf(1,v.env+attack);if(v.env>=1)v.decaying=true;}
        else v.env=fmaxf(sustain,v.env-decay);
        signal[Env][i]=v.env;
      }
      for(int k=0;k<Nodes;++k) {
        const int n=order[k];
        switch(n) {
          case Osc1: case Osc2: {
            int o=n==Osc1?0:1, port=o?Pitch2:Pitch1, s=patch.cable[port];
            float inc=v.inc*(o?ratio:1.f);int wave=patch.value[o?Wave2:Wave1];
            for(int i=0;i<count;++i) {
              float d=inc*(1.f+(s<0?0:signal[s][i])*.08f);
              d=fminf(.45f,fmaxf(.00001f,d));v.phase[o]+=d;if(v.phase[o]>=1)v.phase[o]-=1;
              signal[n][i]=osc(v.phase[o],d,wave);
            }
            break;
          }
          case Noise:
            for(int i=0;i<count;++i){random=random*1664525u+1013904223u;signal[n][i]=(int32_t(random)/2147483648.f)*.5f;}
            break;
          case Lfo:
            for(int i=0;i<count;++i){v.lfo+=lfoInc;if(v.lfo>=1)v.lfo-=1;signal[n][i]=1.f-4.f*fabsf(v.lfo-.5f);}
            break;
          case Env: break;
          case Mix: {
            int a=patch.cable[MixA],b=patch.cable[MixB];
            for(int i=0;i<count;++i)signal[n][i]=.5f*((a<0?0:signal[a][i])+(b<0?0:signal[b][i]));
            break;
          }
          case Filter: {
            int a=patch.cable[FilterIn],cv=patch.cable[Cutoff];
            for(int i=0;i<count;++i){
              float c=fminf(.9f,fmaxf(.001f,alpha*(1.f+(cv<0?0:signal[cv][i])*3.f)));
              v.filter+=c*((a<0?0:signal[a][i])-v.filter);signal[n][i]=v.filter;
            }
            break;
          }
          case Vca: {
            int a=patch.cable[VcaIn],cv=patch.cable[Gain];
            for(int i=0;i<count;++i)signal[n][i]=(a<0?0:signal[a][i])*fmaxf(0,fminf(1,cv<0?0:signal[cv][i]));
            break;
          }
          case Out: {
            int a=patch.cable[Output];
            for(int i=0;i<count;++i)out[i]+=(a<0?0:signal[a][i])*signal[Env][i];
            break;
          }
        }
      }
    }
    const float gain=.18f*patch.value[Level]/100.f;
    for(int i=0;i<count;++i)out[i]=fmaxf(-.98f,fminf(.98f,out[i]*gain));
  }
 private:
  struct Voice {bool active=false,gate=false,decaying=false; float phase[2]={},lfo=0,env=0,filter=0,inc=0;};
  Voice voices[Voices]; int order[Nodes]; uint32_t random=0x13572468;
  float attack=0,decay=0,release=0,alpha=0,lfoInc=0,ratio=1;
  static float squareValue(float x){return x*x;}
  float input(const float* v,int port) const {int s=patch.cable[port];return s<0?0:v[s];}
  static float blep(float t,float d) {if(t<d){t/=d;return t+t-t*t-1;} if(t>1-d){t=(t-1)/d;return t*t+t+t+1;}return 0;}
  static float osc(float t,float d,int wave) {
    if(wave==0) return sinf(t*6.2831853f);
    if(wave==1) return 1.f-4.f*fabsf(t-.5f);
    if(wave==2) return 2*t-1-blep(t,d);
    float s=t<.5f?1.f:-1.f; s+=blep(t,d); t+=.5f;if(t>=1)t-=1;return s-blep(t,d);
  }
};
}
