#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
namespace modular {
enum Node { Osc1, Osc2, Noise, Lfo, Env, Mix, Filter, Vca, Out, Nodes };
enum Port { Pitch1, Pitch2, MixA, MixB, FilterIn, Cutoff, VcaIn, Gain, Output, Ports };
enum Param { Wave1, Wave2, Detune, Rate, Frequency, Attack, Decay, Sustain, Release, Level, Params };
static constexpr int owner[Ports] = {Osc1,Osc2,Mix,Mix,Filter,Filter,Vca,Vca,Out};
static constexpr bool cv[Ports] = {true,true,false,false,false,true,false,true,false};
struct Patch {
  int8_t cable[Ports] = {-1,-1,Osc1,Osc2,Mix,Env,Filter,Env,Vca};
  uint8_t value[Params] = {2,1,55,20,55,2,25,70,20,60};
};
inline bool valid(const Patch& p, int* order = nullptr) {
  bool edges[Nodes][Nodes] = {}; int degree[Nodes] = {};
  for(int i=0;i<Params;++i) if(p.value[i] > (i<2?3:100)) return false;
  for(int i=0;i<Ports;++i) {
    int s=p.cable[i], d=owner[i]; if(s == -1) continue;
    if(s<0 || s>=Out || s==d || (cv[i] != (s==Lfo || s==Env))) return false;
    if(!edges[s][d]) { edges[s][d]=true; ++degree[d]; }
  }
  bool done[Nodes]={};
  for(int k=0;k<Nodes;++k) {
    int n=-1; for(int i=0;i<Nodes;++i) if(!done[i]&&!degree[i]) {n=i;break;}
    if(n<0) return false;
    done[n]=true;
    if(order) order[k]=n;
    for(int i=0;i<Nodes;++i) if(edges[n][i]) --degree[i];
  }
  return true;
}
inline bool route(Patch& p,int port,int source) {
  if(port<0||port>=Ports||source < -1||source>=Out) return false;
  Patch next=p; next.cable[port]=source; if(!valid(next)) return false; p=next; return true;
}
inline bool equal(const Patch& a,const Patch& b) {
  for(int i=0;i<Ports;++i) if(a.cable[i]!=b.cable[i]) return false;
  for(int i=0;i<Params;++i) if(a.value[i]!=b.value[i]) return false;
  return true;
}
inline uint32_t signature(const Patch& p) {
  uint32_t h=2166136261u;
  for(int i=0;i<Ports;++i) h=(h^uint32_t(p.cable[i]+1))*16777619u;
  for(int i=0;i<Params;++i) h=(h^p.value[i])*16777619u;
  return h;
}
// Strict whole-patch transaction: type then ten parameters and nine sources.
inline bool parse(const char* s,int& type,Patch& p) {
  int v[1+Params+Ports];
  for(int i=0;i<1+Params+Ports;++i) {
    char* end=nullptr; long x=strtol(s,&end,10);
    if(end==s || x < -1 || x > 100) return false;
    v[i]=int(x);
    if(i<Params+Ports) {if(*end!=',') return false; s=end+1;} else { while(*end=='\r' || *end=='\n') ++end; if(*end) return false; }
  }
  if(v[0]<0||v[0]>1) return false;
  Patch next;
  for(int i=0;i<Params;++i) {if(v[i+1]<0) return false; next.value[i]=v[i+1];}
  for(int i=0;i<Ports;++i) next.cable[i]=v[1+Params+i];
  if(!valid(next)) return false;
  type=v[0]; p=next; return true;
}
inline int format(char* out,size_t size,int type,const Patch& p) {
  int n=snprintf(out,size,"%d",type);
  for(int i=0;i<Params+Ports;++i) {
    if(n<0||size_t(n)>=size) return -1;
    n+=snprintf(out+n,size-n,",%d",i<Params?p.value[i]:p.cable[i-Params]);
  }
  return size_t(n)<size?n:-1;
}
}
