#pragma once

#include <Bounce2.h>
#include <Encoder.h>
#include <cstdint>

#include "project_state.h"

namespace dubbox {

// Reads raw hardware input and exposes it in normalized form: the 4 volume
// faders, 4 rotary encoders, and 8 per-track buttons (4 fx-select + 4
// drum-pad). Each encoder i is that track's own fx control (rotate = wet
// level, hold+rotate = cycle effect type, tap = cycle the effect's
// primary parameter — see main.cpp's handleFxControls()); each fx-select
// button toggles that track's effect on/off; the drum pads play the
// drum sounds routed to them. Transport + power buttons will come via an MCP23017 expander.
class InputManager {
 public:
  static constexpr int kNumEncoders = 4;
  static constexpr int kNumFxSelectButtons = 4;
  static constexpr int kNumDrumPadButtons = 4;

  void begin();

  // Call every loop() iteration.
  void update();

  // Smoothed fader position, 0.0 (down) .. 1.0 (up).
  float faderValue(int trackIndex) const;

  // Detents turned since the last call for encoder `encoderIndex`
  // (positive = clockwise). Most 5-pin mechanical encoders report 4 raw
  // quadrature counts per detent, divided out here so callers get
  // "clicks." Only whole detents are consumed — a partial count left over
  // mid-turn carries into the next call rather than being dropped.
  int32_t encoderRotationDelta(int encoderIndex);

  // True for as long as the given encoder's push button is physically
  // held down (level, not edge) — for hold+turn gestures (e.g. holding an
  // encoder while rotating it to pick an effect type), same pattern as
  // fxSelectHeld() below.
  bool encoderHeld(int encoderIndex) const;

  // True for exactly one update() cycle when the given button transitions
  // to pressed. `index` is 0..kNumFxSelectButtons-1 / 0..kNumDrumPadButtons-1.
  bool fxSelectPressed(int index) const;
  bool drumPadPressed(int index) const;
  // True for exactly one update() cycle when the given drum pad is let go.
  bool drumPadReleased(int index) const;

 private:
  float faderSmoothed_[kNumTracks] = {0};

  // Encoder objects have no default constructor (they need their pins at
  // construction), so they're allocated in begin() with `new` rather than
  // declared as an array of values — PJRC's documented pattern for
  // building an encoder set in a loop.
  Encoder* encoders_[kNumEncoders] = {nullptr};
  int32_t encoderLastRaw_[kNumEncoders] = {0};
  Bounce encoderButtons_[kNumEncoders];

  Bounce fxSelectButtons_[kNumFxSelectButtons];
  bool fxSelectJustPressed_[kNumFxSelectButtons] = {false};
  Bounce drumPadButtons_[kNumDrumPadButtons];
  bool drumPadJustPressed_[kNumDrumPadButtons] = {false};
  bool drumPadJustReleased_[kNumDrumPadButtons] = {false};
};

}  // namespace dubbox
