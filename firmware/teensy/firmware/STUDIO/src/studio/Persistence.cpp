#include "Persistence.h"
#include <cstring>
namespace studio {
namespace {
uint32_t hash(const uint8_t*p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
struct Wire {
 uint8_t*out;const uint8_t*in;size_t cap,pos=0;bool ok=true;
 void u(uint32_t&v){if(pos+4>cap){ok=false;return;}if(out)for(int i=0;i<4;++i)out[pos+i]=uint8_t(v>>(8*i));else{v=0;for(int i=0;i<4;++i)v|=uint32_t(in[pos+i])<<(8*i);}pos+=4;}
 void f(float&v){uint32_t x;std::memcpy(&x,&v,4);u(x);if(in){std::memcpy(&v,&x,4);if(!std::isfinite(v))ok=false;}}
 void b(bool&v){uint32_t x=v;u(x);if(x>1)ok=false;v=x!=0;}
 void byte(uint8_t&v){uint32_t x=v;u(x);if(x>255)ok=false;v=uint8_t(x);}
 template<class E>void e(E&v,unsigned max){uint32_t x=unsigned(v);u(x);if(x>=max)ok=false;v=E(x);}
 void str(char*p,size_t n){if(pos+n>cap){ok=false;return;}if(out)std::memcpy(out+pos,p,n);else std::memcpy(p,in+pos,n);if(!std::memchr(p,0,n))ok=false;pos+=n;}
 void mapping(Mapping&m){f(m.low);f(m.high);e(m.curve,4);}
 void fx(Fx&x){e(x.effect,unsigned(Effect::Count));b(x.enabled);f(x.intensity);f(x.timeSeconds);mapping(x.wet);mapping(x.feedback);mapping(x.drive);mapping(x.tone);mapping(x.width);}
 void project(Project&p){u(p.version);u(p.id);u(p.stemSet);str(p.name,sizeof p.name);f(p.bpm);f(p.master);byte(p.key);byte(p.octave);byte(p.countInBars);e(p.scale,7);b(p.scaleLock);b(p.metronome);e(p.theme,8);for(auto&d:p.dub){fx(d.fx);f(d.volume);}for(auto&t:p.tracks){auto&s=t.sound;u(s.asset);e(s.kind,4);f(s.macro);f(s.attack);f(s.release);f(s.cutoff);f(s.drive);f(t.volume);b(t.armed);for(auto&st:t.pattern.steps){b(st.on);byte(st.note);f(st.velocity);f(st.gate);}auto&l=t.loop;byte(l.count);u(l.lengthTicks);b(l.playing);for(auto&ev:l.events){u(ev.tick);byte(ev.note);f(ev.velocity);b(ev.on);}l.recording=false;}for(auto&slot:p.slots)u(slot);}
};
bool range(float x){return std::isfinite(x)&&x>=0&&x<=1;}
bool valid(const Project&p){if(p.version!=1||p.bpm<20||p.bpm>300||!range(p.master)||p.key>11||p.octave>8||p.countInBars>2)return false;for(auto&d:p.dub){if(!range(d.volume)||!range(d.fx.intensity)||d.fx.timeSeconds<=0||d.fx.timeSeconds>8)return false;}for(auto&t:p.tracks){if(!range(t.volume)||!range(t.sound.macro)||t.sound.attack<.001f||t.sound.release<.005f||t.loop.count>64||!t.loop.lengthTicks)return false;for(auto&s:t.pattern.steps)if(s.note>127||!range(s.velocity)||!range(s.gate))return false;for(unsigned i=0;i<t.loop.count;++i)if(t.loop.events[i].note>127||t.loop.events[i].tick>=t.loop.lengthTicks||!range(t.loop.events[i].velocity))return false;}return true;}
}
size_t encodeProject(const Project&p,uint8_t*out,size_t cap){if(cap<12||!valid(p))return 0;Project copy=p;Wire w{out,nullptr,cap};uint32_t magic=0x31555453;w.u(magic);w.project(copy);if(!w.ok||w.pos+4>cap)return 0;uint32_t h=hash(out,w.pos);w.u(h);return w.ok?w.pos:0;}
bool decodeProject(const uint8_t*in,size_t n,Project&p){if(n<12)return false;Project tmp;Wire w{nullptr,in,n};uint32_t magic=0;w.u(magic);if(magic!=0x31555453)return false;w.project(tmp);if(!w.ok||w.pos+4!=n)return false;uint32_t expected=hash(in,w.pos),actual=0;w.u(actual);if(!w.ok||actual!=expected||!valid(tmp))return false;p=tmp;return true;}
bool saveProject(IStorage&s,const char*path,const Project&p){std::array<uint8_t,ProjectBytes>b{};auto n=encodeProject(p,b.data(),b.size());return n&&s.atomicWrite(path,b.data(),n);}
bool loadProject(IStorage&s,const char*path,Project&p){std::array<uint8_t,ProjectBytes>b{};size_t n=0;return s.read(path,b.data(),b.size(),n)&&decodeProject(b.data(),n,p);}
}
