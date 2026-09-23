#pragma once

#include <Arduino.h>

// Pin assignments. Keep every GPIO number for this project here, in one
// place — see docs/pin_map.md for wiring notes and status per signal.

namespace dubbox {
namespace pins {

// Faders (linear pots, analog input). All 4 confirmed wired.
// Fader 1: A0 -> A14 -> A10, all on 2026-07-20.
//   A0-A3 are electrically free but physically sit under the Audio
//   Shield's PCB footprint, hard to wire to.
//   A14 (pin 38) turned out to be a bare solder pad on the *underside* of
//   the Teensy 4.1 (near the SD slot), not a normal pluggable header pin —
//   an unsoldered jumper against it reads as floating/noisy input.
//   A10 (pin 24) is a normal top-header pin, still past the Audio
//   Shield's shorter footprint, but a standard pluggable location.
// Faders 2-4 on A11-A13 (pins 25-27), same top-header row, same reasoning.
constexpr uint8_t kFader[4] = {A10, A11, A12, A13};

// The UI is a separate ESP32 touchscreen on UART (Serial1: pin 0 = RX, pin 1 = TX), see
// esp_link.h. Pins 11-13 and 28-32 (once the old ILI9488 SPI screen) are free.

// Encoders 1-4 (fx core phase). Encoder 1 confirmed 2026-07-20, encoder 2
// 2026-09-07, encoders 3-4 2026-09-10.
//   Encoders 1-2 sit on the bottom solder-pad section near the SD slot
//   (33-38) — pins 34+ there are NOT friction-fit header holes, they need
//   wires soldered on (that's what caused the earlier fader-1/pin-38
//   mixup; fine here since it's deliberate).
//   Encoder 3 is on normal header pins 6/9/10.
//   Encoder 4: A on header pin 22, B/Push on bottom solder pads 39/40.
// This exhausts the easily-reachable GPIO — transport + power buttons
// will need an MCP23017 I2C expander on the existing 18/19 bus.
constexpr uint8_t kEncoderA[4] = {33, 36, 6, 22};
constexpr uint8_t kEncoderB[4] = {34, 37, 9, 39};
constexpr uint8_t kEncoderButton[4] = {35, 38, 10, 40};

// Per-track buttons (fx core / UI phase), confirmed 2026-09-10. Normal
// pluggable header pins, no soldering. Each button: one leg of one
// internal pair to GND, one leg of the *other* pair to the pin below,
// read with INPUT_PULLUP (LOW = pressed). This batch of switches wires
// its pairs diagonally rather than same-side — confirm with a meter; the
// physical layout doesn't matter, only which legs are internally joined.
// Drum pad 2 moved from 15 (A1) to 41 (A17) on 2026-09-10 while
// troubleshooting — 41 is a bottom solder pad (same section as encoder
// 4's 39/40), not a plain jumper-in pin like the rest of this set. (Pad 3
// was briefly moved there by mistake and has been moved back to 16.)
constexpr uint8_t kFxSelectButton[4] = {2, 3, 4, 5};
constexpr uint8_t kDrumPadButton[4] = {14, 41, 16, 17};  // A0,A17,A2,A3

// Not yet assigned — direct GPIO is fully spent after encoders 3-4, so
// these need an MCP23017 I2C expander on the existing 18/19 bus:
//   kTransportButton[3], kPowerButton
// Ask before wiring these in.

}  // namespace pins
}  // namespace dubbox
