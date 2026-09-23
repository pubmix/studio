#include "input_manager.h"

#include <Arduino.h>

#include "pins.h"

namespace dubbox {

namespace {
// EMA coefficient for fader smoothing: higher = more responsive, lower =
// smoother but laggier. 0.2 removes pot jitter without feeling sluggish.
constexpr float kFaderSmoothing = 0.2f;
constexpr float kAdcMax = 1023.0f;  // Teensy 4.1 default 10-bit ADC.

// Standard for 5-pin mechanical rotary encoders (4 quadrature edges per
// detent). Some parts use 1 or 2 — if a turn feels like it takes way more
// or fewer physical clicks than expected on hardware, revisit this.
constexpr int32_t kEncoderCountsPerDetent = 4;

constexpr uint32_t kButtonDebounceMs = 10;
}  // namespace

void InputManager::begin() {
  for (int i = 0; i < kNumTracks; ++i) {
    pinMode(pins::kFader[i], INPUT);
    faderSmoothed_[i] = analogRead(pins::kFader[i]) / kAdcMax;
  }

  for (int i = 0; i < kNumEncoders; ++i) {
    encoders_[i] = new Encoder(pins::kEncoderA[i], pins::kEncoderB[i]);
    encoderButtons_[i].attach(pins::kEncoderButton[i], INPUT_PULLUP);
    encoderButtons_[i].interval(kButtonDebounceMs);
  }

  for (int i = 0; i < kNumFxSelectButtons; ++i) {
    fxSelectButtons_[i].attach(pins::kFxSelectButton[i], INPUT_PULLUP);
    fxSelectButtons_[i].interval(kButtonDebounceMs);
  }
  for (int i = 0; i < kNumDrumPadButtons; ++i) {
    drumPadButtons_[i].attach(pins::kDrumPadButton[i], INPUT_PULLUP);
    drumPadButtons_[i].interval(kButtonDebounceMs);
  }
}

void InputManager::update() {
  for (int i = 0; i < kNumTracks; ++i) {
    float raw = analogRead(pins::kFader[i]) / kAdcMax;
    faderSmoothed_[i] += kFaderSmoothing * (raw - faderSmoothed_[i]);
  }

  // INPUT_PULLUP on all buttons: the switch pulls the pin LOW when
  // pressed, so a falling edge is a new press.
  for (int i = 0; i < kNumEncoders; ++i) {
    encoderButtons_[i].update();
  }
  for (int i = 0; i < kNumFxSelectButtons; ++i) {
    fxSelectButtons_[i].update();
    fxSelectJustPressed_[i] = fxSelectButtons_[i].fell();
  }
  for (int i = 0; i < kNumDrumPadButtons; ++i) {
    drumPadButtons_[i].update();
    drumPadJustPressed_[i] = drumPadButtons_[i].fell();
    drumPadJustReleased_[i] = drumPadButtons_[i].rose();
  }
}

float InputManager::faderValue(int trackIndex) const {
  return faderSmoothed_[trackIndex];
}

int32_t InputManager::encoderRotationDelta(int encoderIndex) {
  int32_t raw = encoders_[encoderIndex]->read();
  int32_t rawDelta = raw - encoderLastRaw_[encoderIndex];
  int32_t detents = rawDelta / kEncoderCountsPerDetent;
  // Only consume the whole detents being reported — a leftover partial
  // count rolls into the next call rather than being silently dropped.
  encoderLastRaw_[encoderIndex] += detents * kEncoderCountsPerDetent;
  return detents;
}

bool InputManager::encoderHeld(int encoderIndex) const {
  // INPUT_PULLUP: LOW means the button is currently pressed.
  return encoderButtons_[encoderIndex].read() == LOW;
}

bool InputManager::fxSelectPressed(int index) const {
  return fxSelectJustPressed_[index];
}

bool InputManager::drumPadPressed(int index) const {
  return drumPadJustPressed_[index];
}

bool InputManager::drumPadReleased(int index) const {
  return drumPadJustReleased_[index];
}

}  // namespace dubbox
