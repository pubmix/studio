from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'dubbox-firmware/src/audio_engine.cpp').read_text()
methods=[]
for name in ['setTrackPan','recomputeTrackGain','applyTrackFxMixGain']:
 a=s.index('void AudioEngine::'+name+'(');i=s.index('{',a);depth=1;j=i+1
 while depth:
  depth+=(s[j]=='{')-(s[j]=='}');j+=1
 methods.append(s[a:j])
code='''#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
template<class T>T constrain(T v,T a,T b){return std::max(a,std::min(v,b));}
void AudioNoInterrupts(){} void AudioInterrupts(){}
constexpr int kNumTracks=4;
struct Mixer{float v[4]={};void gain(int i,float g){v[i]=g;}};
struct AudioEngine{
Mixer mixerL_,mixerR_,wetSubMixL_,wetSubMixR_;
int trackPan_[4]={};float trackFaderGain_[4]={1,1,1,1},trackCropGain_[4]={1,1,1,1},trackFxWetLevel_[4]={.4,.4,.4,.4},trackFxMuteHold_[4]={};bool trackFxBypassed_[4]={};
void setTrackPan(int,int);void recomputeTrackGain(int);void applyTrackFxMixGain(int);
};
'''+ '\n'.join(methods)+'''
int main(){AudioEngine a;
a.setTrackPan(0,0);assert(a.mixerL_.v[0]==1 && a.mixerR_.v[0]==1);
a.setTrackPan(0,-1000);assert(a.mixerL_.v[0]==1 && a.mixerR_.v[0]==0);assert(a.wetSubMixR_.v[0]==0);
a.setTrackPan(0,1000);assert(a.mixerL_.v[0]==0 && a.mixerR_.v[0]==1);assert(a.wetSubMixL_.v[0]==0);
a.setTrackPan(1,500);assert(a.mixerL_.v[1]==.5 && a.mixerR_.v[1]==1);assert(a.trackPan_[0]==1000);
a.setTrackPan(1,9000);assert(a.trackPan_[1]==1000);
a.trackFaderGain_[2]=.5;a.trackCropGain_[2]=.5;a.setTrackPan(2,0);assert(a.mixerL_.v[2]==.25);
a.trackFxBypassed_[2]=true;a.trackFxMuteHold_[2]=.75;a.setTrackPan(2,-1000);assert(fabs(a.wetSubMixL_.v[2]-.3)<1e-6);assert(a.wetSubMixR_.v[2]==0);
a.setTrackPan(-1,500);a.setTrackPan(4,500);
puts("PASS: actual audio gain methods; center, extremes, bounds, independent tracks, fader/crop, FX tails");}
'''
temp=tempfile.TemporaryDirectory(prefix='studio-pan-')
p=Path(temp.name)/'pan_test.cpp';p.write_text(code)
binary=str(Path(temp.name)/'pan_test')
subprocess.run(['clang++','-std=c++17','-isystem','/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1','-fsanitize=address,undefined',str(p),'-o',binary],check=True)
subprocess.run([binary],check=True)
