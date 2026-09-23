#pragma once

#include <Audio.h>

namespace dubbox {

// The settings of the synth. Every field is a small integer so the display can edit it with a
// slider and the project file can store it as text. Field order = parameter index (see Synth::kParams).
struct SynthPatch {
  uint8_t wave1 = 1;      // oscillator 1 shape: 0 sine, 1 triangle, 2 saw, 3 square
  uint8_t wave2 = 0;      // oscillator 2 shape
  int8_t oct2 = 1;        // oscillator 2 pitch, in octaves from oscillator 1 (-2..2)
  int8_t detune = 0;      // oscillator 2 fine tune in cents (-50..50)
  uint8_t mix = 30;       // 0 = oscillator 1 only, 50 = equal, 100 = oscillator 2 only
  uint8_t attack = 3;     // envelope times / levels are 0..100 slider positions
  uint8_t decay = 59;
  uint8_t sustain = 18;
  uint8_t release = 27;
  uint8_t cutoff = 100;   // low-pass filter, 0..100 = 60 Hz..12 kHz
  uint8_t resonance = 0;
  uint8_t chorus = 0;     // effect amounts 0..100
  uint8_t reverb = 0;
  uint8_t level = 80;     // overall volume
};

// A small polyphonic synthesizer: per voice two oscillators (sine / triangle / saw / square) with
// an octave, detune and mix control for the second, and an ADSR envelope; all voices share a
// low-pass filter. Its chorus and reverb amounts are send levels into an effect shared by all the
// instruments (see chorusSend() / reverbSend()). The caller (DrumMachine's sequencer, live keys) decides
// which voice plays which note and when it ends. The default patch is a piano-like sound.
class Synth {
 public:
  static constexpr int kVoices = 6;
  static constexpr int kParams = 14;

  // Mono output of the dry sound (after the filter and level).
  AudioStream& output() { return finalOut_; }
  // The same sound scaled by the CHORUS / REVERB amounts, to feed the shared effects.
  AudioStream& chorusSend() { return chorusSend_; }
  AudioStream& reverbSend() { return reverbSend_; }
  // Allocates the wiring and applies the current patch. Call once after AudioMemory().
  void begin();

  void noteOn(int voice, int midiNote);
  void noteOff(int voice);

  // A synth that is switched off makes no sound and costs (almost) no CPU: its oscillators are
  // silenced and its effects get no input. Instruments that do not exist are kept switched off.
  void setEnabled(bool on);
  bool enabled() const { return enabled_; }

  const SynthPatch& patch() const { return patch_; }
  void setPatch(const SynthPatch& p);
  // Sets one parameter by index (clamped to its range); false for a bad index.
  bool setParam(int index, int value);
  // Sets every parameter at once (kParams values, clamped) and applies them a single time.
  void setAll(const int* values);
  int param(int index) const;
  static void paramRange(int index, int& lo, int& hi);

 private:
  void assign(int index, int value);  // stores one clamped parameter (does not apply it)
  void apply();  // pushes the patch to the voices and effects

  SynthPatch patch_;
  bool enabled_ = true;
  float osc2Ratio_ = 2.0f;  // oscillator 2 frequency / oscillator 1 frequency

  AudioSynthWaveform osc1_[kVoices];
  AudioSynthWaveform osc2_[kVoices];
  AudioMixer4 voiceMix_[kVoices];
  AudioEffectEnvelope env_[kVoices];
  AudioMixer4 sumA_, sumB_, sum_;
  AudioFilterStateVariable filter_;
  AudioMixer4 finalOut_, chorusSend_, reverbSend_;

  AudioConnection* voicePatch_[kVoices * 4] = {nullptr};
  AudioConnection* fxPatch_[6] = {nullptr};
};

}  // namespace dubbox
