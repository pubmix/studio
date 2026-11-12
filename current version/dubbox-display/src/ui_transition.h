#pragma once
#include <stdint.h>
namespace uitransition {
// Sixth RGB565 frame in the LT7683's 16 MiB RAM. Reserve exclusively as the old
// screen snapshot, copied BEFORE drawing the new screen into staging.
constexpr unsigned long kOldFrame = 9216000;
enum class State { Idle, Running, Complete, Failed };
using BlendObserver = void (*)(unsigned long oldFrame, unsigned long newFrame,
                              unsigned long destination, uint8_t alpha32);
// Call after the old-frame copy and new-frame drawing have finished. No RAM
// copies are made here. Inputs/destination must be three distinct full frames.
// Requires an observer so the computer mirror cannot silently miss blends.
// While Running, caller must suspend all other LCD drawing/BTE operations and
// keep both sources unchanged; continue servicing UART, audio-link and mirror.
// Does NOT set ui's staging flag: integration must finish that state separately.
bool begin(unsigned long newFrame, unsigned long destination, BlendObserver observer);
// Nonblocking polling: launches at most one BTE operation per call, never waits
// in a loop. Call until Complete/Failed; last state stays available until begin.
State update();
State state();
bool hardwareBusy();
// Failed means the BTE remained busy >200ms; do not issue a competing copy while
// busy. Caller can recover/present final screen after controller becomes idle.
}
