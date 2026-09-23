#include "synth.h"

#include <math.h>

namespace dubbox {

namespace {

short waveType(int w) {
  switch (w) {
    case 1:
      return WAVEFORM_TRIANGLE;
    case 2:
      return WAVEFORM_SAWTOOTH;
    case 3:
      return WAVEFORM_SQUARE;
    default:
      return WAVEFORM_SINE;
  }
}

// Slider position 0..100 -> a time in ms: quadratic, so the short end is fine-grained.
float timeMs(int v, float maxMs) {
  float x = v / 100.0f;
  return 1.0f + x * x * maxMs;
}

}  // namespace

void Synth::paramRange(int index, int& lo, int& hi) {
  lo = 0;
  hi = 100;
  switch (index) {
    case 0:
    case 1:
      hi = 3;
      break;
    case 2:
      lo = -2;
      hi = 2;
      break;
    case 3:
      lo = -50;
      hi = 50;
      break;
    default:
      break;
  }
}

int Synth::param(int index) const {
  switch (index) {
    case 0: return patch_.wave1;
    case 1: return patch_.wave2;
    case 2: return patch_.oct2;
    case 3: return patch_.detune;
    case 4: return patch_.mix;
    case 5: return patch_.attack;
    case 6: return patch_.decay;
    case 7: return patch_.sustain;
    case 8: return patch_.release;
    case 9: return patch_.cutoff;
    case 10: return patch_.resonance;
    case 11: return patch_.chorus;
    case 12: return patch_.reverb;
    case 13: return patch_.level;
    default: return 0;
  }
}

void Synth::assign(int index, int value) {
  int lo, hi;
  paramRange(index, lo, hi);
  value = value < lo ? lo : (value > hi ? hi : value);
  switch (index) {
    case 0: patch_.wave1 = static_cast<uint8_t>(value); break;
    case 1: patch_.wave2 = static_cast<uint8_t>(value); break;
    case 2: patch_.oct2 = static_cast<int8_t>(value); break;
    case 3: patch_.detune = static_cast<int8_t>(value); break;
    case 4: patch_.mix = static_cast<uint8_t>(value); break;
    case 5: patch_.attack = static_cast<uint8_t>(value); break;
    case 6: patch_.decay = static_cast<uint8_t>(value); break;
    case 7: patch_.sustain = static_cast<uint8_t>(value); break;
    case 8: patch_.release = static_cast<uint8_t>(value); break;
    case 9: patch_.cutoff = static_cast<uint8_t>(value); break;
    case 10: patch_.resonance = static_cast<uint8_t>(value); break;
    case 11: patch_.chorus = static_cast<uint8_t>(value); break;
    case 12: patch_.reverb = static_cast<uint8_t>(value); break;
    default: patch_.level = static_cast<uint8_t>(value); break;
  }
}

bool Synth::setParam(int index, int value) {
  if (index < 0 || index >= kParams) return false;
  assign(index, value);
  apply();
  return true;
}

void Synth::setAll(const int* values) {
  for (int i = 0; i < kParams; ++i) assign(i, values[i]);
  apply();
}

void Synth::setPatch(const SynthPatch& p) {
  patch_ = p;
  for (int i = 0; i < kParams; ++i) assign(i, param(i));  // clamps every field
  apply();
}

void Synth::begin() {
  for (int v = 0; v < kVoices; ++v) {
    osc1_[v].begin(0.5f, 440.0f, WAVEFORM_TRIANGLE);
    osc2_[v].begin(0.3f, 880.0f, WAVEFORM_SINE);
    voiceMix_[v].gain(0, 1.0f);
    voiceMix_[v].gain(1, 1.0f);
    env_[v].hold(0.0f);
    voicePatch_[v * 4 + 0] = new AudioConnection(osc1_[v], 0, voiceMix_[v], 0);
    voicePatch_[v * 4 + 1] = new AudioConnection(osc2_[v], 0, voiceMix_[v], 1);
    voicePatch_[v * 4 + 2] = new AudioConnection(voiceMix_[v], 0, env_[v], 0);
    voicePatch_[v * 4 + 3] = new AudioConnection(env_[v], 0, v < 4 ? sumA_ : sumB_, v < 4 ? v : v - 4);
  }
  for (int i = 0; i < 4; ++i) {
    sumA_.gain(i, 0.3f);
    sumB_.gain(i, 0.3f);
  }
  sum_.gain(0, 1.0f);
  sum_.gain(1, 1.0f);
  filter_.octaveControl(0.0f);

  // voices -> filter -> dry output, and the same sound into the chorus / reverb send mixers
  fxPatch_[0] = new AudioConnection(sumA_, 0, sum_, 0);
  fxPatch_[1] = new AudioConnection(sumB_, 0, sum_, 1);
  fxPatch_[2] = new AudioConnection(sum_, 0, filter_, 0);  // (removed while the synth is switched off)
  fxPatch_[3] = new AudioConnection(filter_, 0, finalOut_, 0);  // filter output 0 = low-pass
  fxPatch_[4] = new AudioConnection(filter_, 0, chorusSend_, 0);
  fxPatch_[5] = new AudioConnection(filter_, 0, reverbSend_, 0);
  apply();
}

void Synth::apply() {
  const SynthPatch& p = patch_;
  // Oscillator balance: both at full level around the middle, fading one out towards the ends.
  const float g1 = enabled_ ? fminf(1.0f, 2.0f * (100 - p.mix) / 100.0f) : 0.0f;
  const float g2 = enabled_ ? fminf(1.0f, 2.0f * p.mix / 100.0f) : 0.0f;
  osc2Ratio_ = powf(2.0f, p.oct2 + p.detune / 1200.0f);
  for (int v = 0; v < kVoices; ++v) {
    osc1_[v].begin(waveType(p.wave1));
    osc2_[v].begin(waveType(p.wave2));
    osc1_[v].amplitude(0.5f * g1);
    osc2_[v].amplitude(0.5f * g2);
    env_[v].attack(timeMs(p.attack, 1500.0f));
    env_[v].decay(timeMs(p.decay, 2000.0f));
    env_[v].sustain(p.sustain / 100.0f);
    env_[v].release(timeMs(p.release, 3000.0f));
  }
  filter_.frequency(60.0f * powf(200.0f, p.cutoff / 100.0f));
  filter_.resonance(0.7f + p.resonance / 100.0f * 4.3f);
  const float level = p.level / 100.0f * 1.2f;
  finalOut_.gain(0, level);
  chorusSend_.gain(0, level * 0.9f * p.chorus / 100.0f);
  reverbSend_.gain(0, level * 0.9f * p.reverb / 100.0f);
}

void Synth::setEnabled(bool on) {
  if (on == enabled_) return;
  enabled_ = on;
  AudioNoInterrupts();
  if (on) {
    fxPatch_[2] = new AudioConnection(sum_, 0, filter_, 0);
  } else {
    delete fxPatch_[2];  // with no input the filter, chorus and reverb do no work
    fxPatch_[2] = nullptr;
  }
  AudioInterrupts();
  apply();
}

void Synth::noteOn(int voice, int midiNote) {
  if (voice < 0 || voice >= kVoices || !enabled_) return;
  const float freq = 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
  osc1_[voice].frequency(freq);
  osc2_[voice].frequency(freq * osc2Ratio_);
  env_[voice].noteOn();
}

void Synth::noteOff(int voice) {
  if (voice < 0 || voice >= kVoices) return;
  env_[voice].noteOff();
}

}  // namespace dubbox
