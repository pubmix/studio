#pragma once
#include <cassert>
#include <cstdint>
#define AUDIO_BLOCK_SAMPLES 128
#define AUDIO_SAMPLE_RATE_EXACT 44100.0
struct audio_block_t { int16_t data[128]; bool used = false; };
class AudioStream {
public:
 inline static audio_block_t pool[8]{};
 inline static int budget = 8;
 AudioStream(int, void*) {}
 virtual void update() = 0;
 static audio_block_t* allocate() {
  if (budget == 0) return nullptr;
  for(auto &b:pool) if(!b.used) {b.used=true; --budget; return &b;}
  return nullptr;
 }
 static void release(audio_block_t* b) {assert(b && b->used); b->used=false;}
 static void transmit(audio_block_t*, int) {}
};
