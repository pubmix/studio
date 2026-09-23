#include "../dsp.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
int main(){
 using namespace modular;
 Patch p,original=p; int order[Nodes];assert(valid(p,order));
 int rank[Nodes];for(int i=0;i<Nodes;++i)rank[order[i]]=i;
 for(int i=0;i<Ports;++i)if(p.cable[i]>=0)assert(rank[p.cable[i]]<rank[owner[i]]);
 assert(!route(p,FilterIn,Vca));assert(equal(p,original)); // VCA->filter->VCA cycle
 assert(!route(p,Gain,Osc1));assert(!route(p,Output,Lfo));assert(!route(p,Output,99));assert(!route(p,-1,Osc1));
 assert(route(p,Pitch1,Lfo));assert(route(p,Pitch2,Lfo)); // fan-out CV
 char text[128];assert(format(text,sizeof(text),1,p)>0);Patch decoded;int type=0;
 assert(parse(text,type,decoded)&&type==1&&equal(p,decoded));
 std::string wire=text;
 assert(!parse((wire+",42").c_str(),type,decoded));
 assert(!parse((wire+"\nBAD").c_str(),type,decoded));
 for(size_t i=0;i<wire.size();++i){Patch untouched=decoded;int old=type;
   if(i==wire.size()-1)continue; // truncating a multi-digit final field could be valid
   bool ok=parse(wire.substr(0,i).c_str(),type,decoded);
   if(!ok)assert(type==old&&equal(untouched,decoded));
 }
 assert(format(text,2,1,p)==-1);
 Engine engine;assert(engine.configure(original));engine.on(0,60);
 double energy=0;for(int i=0;i<44100;++i){float x=engine.sample();assert(std::isfinite(x)&&fabs(x)<=.98f);energy+=x*x;}assert(energy>1);
 engine.off(0);for(int i=0;i<150000;++i)engine.sample();assert(!engine.sounding()&&engine.sample()==0);
 Patch silent=original;assert(route(silent,Output,-1));assert(engine.configure(silent));engine.on(0,60);
 for(int i=0;i<10000;++i)assert(engine.sample()==0);
 // An explicit oscillator->output route follows MIDI pitch (sine, envelope sustained).
 Patch tone;for(int i=0;i<Ports;++i)tone.cable[i]=-1;tone.cable[Output]=Osc1;tone.value[Wave1]=0;tone.value[Sustain]=100;
 engine.silence();engine.configure(tone);engine.on(0,69);int crossings=0;float last=0;
 for(int i=0;i<44100;++i){float x=engine.sample();if(last<0&&x>=0)++crossings;last=x;}assert(crossings>=439&&crossings<=441);
 engine.silence();engine.on(0,81);crossings=0;last=0;
 for(int i=0;i<44100;++i){float x=engine.sample();if(last<0&&x>=0)++crossings;last=x;}assert(crossings>=879&&crossings<=881);
 for(int n=0;n<6;++n)engine.on(n,120+n);
 for(int i=0;i<44100;++i){float x=engine.sample();assert(std::isfinite(x)&&fabs(x)<=.98f);}
 engine.silence();assert(!engine.sounding());
 // Block renderer must match the scalar reference for polyphonic routed patches,
 // parameter changes, each waveform, release and silence.
 for(int wave=0;wave<4;++wave){
  Engine scalar,block;Patch q;q.value[Wave1]=wave;q.value[Wave2]=wave;q.cable[Pitch1]=Lfo;
  scalar.configure(q);block.configure(q);
  for(int v=0;v<6;++v){scalar.on(v,48+v*4);block.on(v,48+v*4);}
  for(int b=0;b<200;++b){
   if(b==70)for(int v=0;v<6;++v){scalar.off(v);block.off(v);}
   float out[128];block.render(out,128);
   for(int i=0;i<128;++i)assert(fabs(scalar.sample()-out[i])<0.00001f);
  }
 }
 puts("PASS: typed routing, cycles, fan-out, serialization, atomic invalid edits, silence, release, pitch, six-voice bounds");
}
