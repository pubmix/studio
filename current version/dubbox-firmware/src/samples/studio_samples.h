#pragma once
#include <stdint.h>
namespace studio_samples {
struct Sample { const int16_t* pcm; uint32_t frames; uint16_t sampleRate; uint8_t rootMidi; };
constexpr uint8_t kPianoCount = 5;
constexpr uint8_t kDrumCount = 8;
extern const Sample piano[kPianoCount];
// Kick, snare, clap, closed hat, open hat, tom, rim, tambourine.
extern const Sample drums[kDrumCount];
}
