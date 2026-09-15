#pragma once
#include <Audio.h>
#include "src/studio/AudioQueue.h"
class StudioAudioStream:public AudioStream {
 studio::AudioQueue&queue_;
public:explicit StudioAudioStream(studio::AudioQueue&q):AudioStream(0,nullptr),queue_(q){}
 void update()override{
  auto frames=queue_.beginRead();
  audio_block_t* blocks[4]{};
  for(int c=0;c<4;++c){blocks[c]=allocate();if(blocks[c])for(unsigned i=0;i<AUDIO_BLOCK_SAMPLES;++i){float x=0;if(frames){const auto&f=(*frames)[i];x=c==0?f.main.l:c==1?f.main.r:c==2?f.headphones.l:f.headphones.r;}blocks[c]->data[i]=int16_t(studio::unit((x+1)*.5f)*65534-32767);}}
  if(frames)queue_.release();for(int c=0;c<4;++c)if(blocks[c]){transmit(blocks[c],c);release(blocks[c]);}
 }
};
static_assert(AUDIO_BLOCK_SAMPLES==studio::BlockSize,"Configure STUDIO block size to match Teensy Audio");
