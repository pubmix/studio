"""Compile actual DrumMachine integration methods against bounded fake synth/clock."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'drum_machine.cpp').read_text()
methods=['emitArp','stopArpInputs','setArp','releaseLive','noteDown','noteUp','refreshNote','setSustain','pickVoice','releaseAll']
code=[]
for name in methods:
    start=source.rfind('\n',0,source.index('DrumMachine::'+name+'('))+1
    brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    code.append(source[start:end])
scaffold=r'''
#include "arpeggiator.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
uint32_t clockMs=0;
uint32_t millis(){return clockMs;}
uint32_t micros(){return clockMs*1000;}
struct SerialStub {template<class... T>void printf(const char*,T...){}void println(const char*){}} Serial;
struct Synth {static constexpr int kVoices=6;int notes[6]={-1,-1,-1,-1,-1,-1};
void noteOn(int v,int m){assert(v>=0&&v<6);notes[v]=m;}
void noteOff(int v){assert(v>=0&&v<6);notes[v]=-1;}};
class DrumMachine {public:
static constexpr int kMaxInstruments=4,kLiveSlots=8,kMinNote=24,kMaxNote=108,kSteps=16;
static constexpr uint32_t kNoStep=0xffffffffu,kHeldForever=0xffffffffu;
struct ArpSettings {bool enabled=false;uint8_t pattern=0,octaves=1,rate=4,gate=75;};
struct ArpContext {DrumMachine* owner=nullptr;int inst=0;};
struct LiveNote {bool used;int inst,midi,voice;uint32_t startMs;int pattern,step;bool fromDisplay;uint32_t refreshMs;bool sustained;};
Synth inst_[4];studio_arp::Arpeggiator arp_[4];ArpSettings arpSettings_[4];ArpContext arpContext_[4];LiveNote live_[8]={};
uint32_t voiceOffStep_[4][6],voiceAuditionUntilMs_[4][6]={};
int instCount_=2,bpm_=120,previewPattern_=0;uint32_t previewStartMs_=0,sustainRefreshMs_=0;
bool recording_=false,previewOn_=false,sustain_=false,sustainFromDisplay_=false;
DrumMachine(){for(int i=0;i<4;++i){arpContext_[i]={this,i};arp_[i].bind(emitArp,&arpContext_[i]);for(auto& x:voiceOffStep_[i])x=kNoStep;}}
static void emitArp(void*,studio_arp::Event);void stopArpInputs(int);bool setArp(int,bool,int,int,int,int);
void releaseLive(int);void noteDown(int,int,bool);void noteUp(int,int);void refreshNote(int,int);void setSustain(bool,bool=false);int pickVoice(int);void releaseAll();
float stepMs(){return 125.f;}bool setNote(int,int,int,int,int){return true;}
};
'''
tests=r'''
int main(){
DrumMachine d;
// Disabled mode retains direct polyphony and normal release behavior.
d.noteDown(0,60,false);assert(d.inst_[0].notes[0]==60);d.noteUp(0,60);assert(d.inst_[0].notes[0]==-1);
d.inst_[0].noteOn(5,80);d.setArp(0,false,0,2,4,75);assert(d.inst_[0].notes[5]==80);
assert(d.setArp(0,true,0,2,4,75));
for(int v=0;v<5;++v){d.voiceOffStep_[0][v]=100+v;d.inst_[0].noteOn(v,40+v);}
assert(d.pickVoice(0)==0); // the reserved voice is never a free/steal candidate
// Both MIDI/pad path and display path enter the same held-chord scheduler.
d.noteDown(0,60,false);d.noteDown(0,64,true);d.arp_[0].tick(micros());assert(d.inst_[0].notes[5]==60);
for(int v=0;v<5;++v)assert(d.inst_[0].notes[v]==40+v);
d.noteUp(0,60);assert(d.inst_[0].notes[5]==-1);clockMs=125;d.arp_[0].tick(micros());assert(d.inst_[0].notes[5]==64);
d.refreshNote(0,64);bool refreshed=false;for(auto& n:d.live_)if(n.used&&n.midi==64)refreshed=n.refreshMs==125;assert(refreshed);
d.noteUp(0,64);assert(d.inst_[0].notes[5]==-1);
// Pedal ownership and release are retained.
d.noteDown(0,67,true);d.arp_[0].tick(micros());d.setSustain(true,true);d.noteUp(0,67);assert(d.arp_[0].heldCount()==1);d.setSustain(false);assert(d.arp_[0].heldCount()==0 && d.inst_[0].notes[5]==-1);
// Stop/seek must clear held inputs too, so the next update cannot restart them.
d.noteDown(0,60,true);d.arp_[0].tick(micros());d.releaseAll();d.arp_[0].tick(micros()+125000);assert(d.arp_[0].heldCount()==0 && d.inst_[0].notes[5]==-1);
for(auto& n:d.live_)assert(!n.used);
// Mode change releases active output and the reserved slot safely returns to allocator.
d.noteDown(0,60,true);d.arp_[0].tick(micros());assert(d.setArp(0,false,3,3,8,50));assert(d.inst_[0].notes[5]==-1);
for(int v=0;v<5;++v)d.voiceOffStep_[0][v]=10;assert(d.pickVoice(0)==5);
d.noteDown(1,72,false);assert(d.inst_[1].notes[0]==72);d.setArp(0,true,1,1,2,25);assert(d.inst_[1].notes[0]==72);d.noteUp(1,72);
assert(!d.setArp(-1,true,0,1,4,75));assert(!d.setArp(0,true,4,1,4,75));
puts("Actual DrumMachine arp ownership/release integration passed");}
'''
with tempfile.TemporaryDirectory(prefix='studio-arp-') as tmp:
    cpp=Path(tmp)/'test.cpp';out=Path(tmp)/'test';cpp.write_text(scaffold+'\n'.join(code)+tests)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-fsanitize=address,undefined','-I'+str(root/'arp'),str(cpp),'-o',str(out)],check=True)
    subprocess.run([str(out)],check=True)
