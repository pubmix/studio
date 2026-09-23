# Dub-Box pin map

Status legend: ✅ confirmed and wired into code · ⬜ not yet decided.

## Faders (analog in)

| Signal | Pin | Status |
|---|---|---|
| Fader 1 | A10 (pin 24) | ✅ |
| Fader 2 | A11 (pin 25) | ✅ |
| Fader 3 | A12 (pin 26) | ✅ |
| Fader 4 | A13 (pin 27) | ✅ |

Fader 1 history, all 2026-07-20:
- **A0** — electrically clear of the Audio Shield, but physically sits
  under the shield's PCB footprint, hard to get a wire onto.
- **A14 (pin 38)** — turned out to be a bare solder pad on the
  *underside* of the Teensy 4.1 (near the SD slot), not a normal
  pluggable header pin. An unsoldered jumper against it reads as
  floating/noisy ("random") input — that's almost certainly what happened.
- **A10 (pin 24)** — confirmed. A normal top-header pin, still past the
  Audio Shield's shorter physical footprint, but a standard pluggable
  location (no soldering needed).

Faders 2-4 confirmed on A11-A13 (pins 25-27), same top-header row as A10,
same reasoning.

If a fader still reads noisy/jumpy after moving to a proper header pin,
check the pot's two **outer legs** are firmly connected to 3.3V and GND —
a pot with only the wiper connected (no power/ground reference) floats and
reads just as randomly as an unconnected pin does.

## ESP32 display link (current UI)

The UI is a separate ESP32 touchscreen (`../dubbox-display`). Wiring to the Teensy:

| Teensy pin | ESP32 pin | Signal |
|---|---|---|
| 1 (TX1) | IO4 (RX2) | Teensy -> ESP32, 921600 baud |
| 0 (RX1) | IO17 (TX2) | ESP32 -> Teensy |
| GND | GND | common ground |

Protocol: `src/esp_link.h`.

## RETIRED: TFT (ILI9488) + touch (XPT2046 resistive)

The old Teensy-side screen and its firmware (LVGL UI, TFT_eSPI) were removed on 2026-09-21.
The section below is kept only as wiring history; pins 11-13 and 28-32 are free again.

| Module pin | Teensy pin | Status |
|---|---|---|
| SCK / CLK (display) | 13 | ✅ |
| T_CLK (touch) | 13 | ✅ same physical pin as SCK — touch (XPT2046) and display share one SPI bus, that's what `TOUCH_CS` in TFT_eSPI's config is for |
| SDI / MOSI (display) | 11 | ✅ |
| T_DIN (touch) | 11 | ✅ same physical pin as MOSI, same reasoning |
| SDO / MISO (display) | 12 | ✅ reconnected — needed once touch reads were wired up |
| T_DO (touch) | 12 | ✅ same physical pin as display MISO, same shared-bus reasoning as SCK/MOSI above |
| CS (display) | 28 | ✅ |
| DC / RS (display) | 29 | ✅ |
| RST (display) | 30 | ✅ |
| T_CS (touch) | 31 | ✅ this one IS its own dedicated pin — separate chip-select is how two SPI devices share one bus without colliding |
| T_IRQ (touch) | 32 | ✅ |
| LED / backlight | 3.3V direct | ✅ no GPIO, no dimming for now |
| VCC | 3.3V | ✅ |
| GND | GND | ✅ |

Confirmed 2026-07-20. Audio Shield uses tall stacking headers, so pins
10-13 (under the shield's footprint) are still reachable via the shield's
pass-through sockets. CS/DC/RST/Touch-CS/Touch-IRQ landed on 28-32 —
same "past the shield's shorter physical footprint, standard top-header
pins" region as the faders (24-27), for the same reason.


**Note:** a later revision of `docs/build_brief.md` claimed the shield is
direct-soldered flush (no stacking headers) — re-checked physically on
2026-07-20 and confirmed false for this unit: there's a visible gap/socket
between the boards. Wiring above stays correct. If this ever comes up
again (different unit, reassembly, etc.), re-verify physically before
trusting either source blindly.

## Encoders 1-4 (fx core)

| Signal | Enc 1 | Enc 2 | Enc 3 | Enc 4 |
|---|---|---|---|---|
| A | 33 | 36 | 6 | 22 |
| B | 34 | 37 | 9 | 39 |
| Push (terminal 1) | 35 | 38 | 10 | 40 |
| Common (C) + Push terminal 2 | GND | GND | GND | GND |

Encoder 1 confirmed 2026-07-20, encoder 2 2026-09-07, encoders 3-4
2026-09-10.

- **Encoders 1-2 (pins 33-38)**: bottom solder-pad section near the SD
  slot — pins 34+ there need wires soldered on, not friction-fit. Same
  section as the pin-38/A14 mixup during fader 1's wiring; fine here
  because it's deliberate and soldered. Recheck enc 2's 36↔37 for a
  solder bridge — that was killing its rotation.
- **Encoder 3 (pins 6/9/10)**: normal header pins, no soldering.
- **Encoder 4**: A on header pin 22; B/Push on bottom solder pads 39/40.

This exhausts the easily-reachable GPIO. Transport + power buttons will
need an MCP23017 I2C expander on the existing 18/19 bus (per the build
brief's "expander for the buttons specifically" note).

**Encoder roles**: encoder 1 controls the shared delay's overall intensity
(rotate) and delay time (push), globally. Encoders 2-4 control tracks 2-4's
individual send levels into that shared delay (rotate) and mute/unmute
that send (push). Track 1 has no dedicated send encoder — its send stays
at `AudioEngine`'s built-in default. See `docs/build_brief.md`'s effects
mapping and `audio_engine.h` for the send/return architecture.

## Per-track buttons (fx-select + drum pad)

| Signal | Pin 1 | Pin 2 | Pin 3 | Pin 4 |
|---|---|---|---|---|
| Fx-select | 2 | 3 | 4 | 5 |
| Drum pad | 14 (A0) | **41 (A17)** | 16 (A2) | 17 (A3) |

Confirmed 2026-09-10. All normal pluggable header pins except drum pad 2,
moved to 41 (bottom solder pad, same section as encoder 4's 39/40) while
troubleshooting — needed to isolate the fault from pin 15 specifically.
(Pad 3 was briefly moved there by mistake and has been moved back to 16.)

Wiring for any of these: one leg of one internal pair to GND, one leg of
the *other* pair to the pin above, read with `INPUT_PULLUP` (LOW =
pressed). This batch of switches wires its pairs diagonally rather than
same-side — confirm with a meter; physical layout doesn't matter, only
which legs are internally joined.

Currently wired and debounced but not driving anything — presses just
print an acknowledgement to serial (`FX-select button N pressed` /
`Drum pad N pressed`).

## Audio Shield (SGTL5000)

Fixed by the shield's physical stacking layout — not user-configurable, no
decision needed. Handled automatically by `AudioControlSGTL5000` /
`AudioOutputI2S` in the Teensy Audio Library.

## SD card

Using the Teensy 4.1's built-in microSD slot (`BUILTIN_SDCARD`), not the
Audio Shield's onboard slot. Fixed pins, no decision needed.

## Not yet assigned

Ask before wiring these into code:

| Signal | Count | Notes |
|---|---|---|
| Transport buttons | 3 pins | **Needs MCP23017** — direct GPIO is exhausted after encoders 3-4 and the 8 per-track buttons |
| Power button | 1 pin | **Needs MCP23017** |
