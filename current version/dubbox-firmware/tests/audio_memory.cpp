#include <SD.h>
#include <AudioStream.h>
#include <Arduino.h>
#define private public
#include "audio_play_sd_wav_seekable.h"
#undef private
#include <cstdio>
void wav() {
 fixture.assign(44+4096,0);
 auto put=[](int at,uint32_t n,int bytes){for(int i=0;i<bytes;++i)fixture[at+i]=(n>>(8*i))&255;};
 memcpy(fixture.data(),"RIFF",4);put(4,4132,4);memcpy(fixture.data()+8,"WAVEfmt ",8);
 put(16,16,4);put(20,1,2);put(22,2,2);put(24,44100,4);put(28,176400,4);put(32,4,2);put(34,16,2);
 memcpy(fixture.data()+36,"data",4);put(40,4096,4);
}
int main(){
 wav();
 { AudioPlaySdWavSeekable p; assert(p.play("test.wav"));
   AudioStream::budget=1; p.update(); // allocation fails while parsing stereo header
   assert(p.block_left==nullptr && p.block_right==nullptr);
   assert(p.total_length==4096 && p.data_start_file_pos==44);
   assert(p.lengthMillis()>0);
   AudioStream::budget=8;p.update();assert(p.isPlaying());p.stop(); }
 { AudioPlaySdWavSeekable p;assert(p.play("test.wav"));AudioStream::budget=8;p.update();
   AudioStream::budget=1;p.update(); // failure during normal stereo playback
   assert(p.block_left==nullptr && p.block_right==nullptr);p.stop(); }
 for(auto &b:AudioStream::pool)assert(!b.used);
 puts("PASS: stereo allocation exhaustion, recovery, stop and metadata");
}
