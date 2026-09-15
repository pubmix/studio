#pragma once
#include "AudioEngine.h"
#include <atomic>
namespace studio {
// Single foreground producer, single audio interrupt consumer. One slot remains empty.
class AudioQueue {
 std::array<std::array<OutputFrame,BlockSize>,4> blocks_{};
 std::atomic<unsigned> write_{0},read_{0};
public:
 std::atomic<uint32_t> underruns{0};
 bool full()const{return (write_.load(std::memory_order_relaxed)+1)%4==read_.load(std::memory_order_acquire);}
 std::array<OutputFrame,BlockSize>*beginWrite(){return full()?nullptr:&blocks_[write_.load(std::memory_order_relaxed)];}
 void commit(){write_.store((write_.load(std::memory_order_relaxed)+1)%4,std::memory_order_release);}
 const std::array<OutputFrame,BlockSize>*beginRead(){unsigned r=read_.load(std::memory_order_relaxed);if(r==write_.load(std::memory_order_acquire)){underruns.fetch_add(1,std::memory_order_relaxed);return nullptr;}return &blocks_[r];}
 void release(){read_.store((read_.load(std::memory_order_relaxed)+1)%4,std::memory_order_release);}
};
}
