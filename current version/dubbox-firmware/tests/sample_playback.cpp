#include "sample_playback.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
using dubbox::SampleCursor;
static size_t allocations=0;
void* operator new(size_t n){++allocations;if(void* p=malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {free(p);}
static const int16_t ramp[]={0,1000,2000,3000,0};
static int16_t longSample[70002];
int main(){
 const studio_samples::Sample a={ramp,5,44100,60};SampleCursor c;
 assert(!c.start({nullptr,5,44100,60},60,44100));
 assert(!c.start({ramp,1,44100,60},60,44100));
 assert(!c.start({ramp,5,0,60},60,44100));
 assert(!c.start(a,60,0));assert(!c.start(a,60,NAN));assert(!c.start(a,-1,44100));
 assert(c.start(a,60,44100));assert(c.step()==65536);
 for(int i=0;i<5;++i)assert(c.next()==ramp[i]);assert(!c.active());assert(c.next()==0);
 assert(c.start(a,48,44100));assert(c.step()==32768);assert(c.next()==0);assert(c.next()==500);assert(c.next()==1000);assert(c.next()==1500);
 assert(c.start(a,72,44100));assert(c.next()==0);assert(c.next()==2000);assert(c.next()==0);assert(!c.active());
 assert(c.start({ramp,5,22050,60},60,44100));assert(c.step()==32768);
 const int16_t extreme[]={-32768,32767,-32768,32767};
 assert(c.start({extreme,4,44100,60},48,44100));assert(c.next()==-32768);assert(c.next()==-1);assert(c.next()==32767);
 // More than 65,535 frames must not wrap a 16.16 phase counter.
 for(int i=0;i<70002;++i)longSample[i]=int16_t(i%30000);
 assert(c.start({longSample,70002,44100,60},60,44100));c.skip(65536);assert(c.next()==longSample[65536]);c.skip(100000);assert(!c.active());
 // Choke ramps to zero instead of abruptly truncating an open hat.
 for(auto& v:longSample)v=17600;
 assert(c.start({longSample,70002,44100,60},60,44100));c.choke();for(int n=176;n>0;--n)assert(c.next()==n*100);assert(!c.active());assert(c.next()==0);
 size_t before=allocations;
 for(int midi=24;midi<=108;++midi){assert(c.start({longSample,70002,44100,60},midi,44100));for(int i=0;i<128;++i)(void)c.next();c.stop();}
 assert(allocations==before);
 static_assert(sizeof(SampleCursor)<=48,"Unexpected per-voice RAM growth");
 std::printf("PASS: root pitch, octave resampling, interpolation extremes, invalid metadata, long-tail bounds, choke, skip, zero heap; cursor=%zu bytes\n",sizeof(SampleCursor));
}
