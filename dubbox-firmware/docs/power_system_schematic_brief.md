# Dub-Box Power System — Schematic Brief

Handoff doc for a Claude chat to help draft an actual schematic. Everything
below is either already confirmed on the built hardware, or a recommendation
worked out in a prior planning conversation — goal of the schematic session
is to turn this into a real, component-level schematic (part numbers,
values, connections) ready to breadboard or lay out.

## 1. What Dub-Box is

A Teensy 4.1 based hardware 4-track DAW: SD-card WAV playback across 4
tracks, per-track effects (delay/flange/freeverb/chorus, chainable), a
480×320 touchscreen (LVGL UI), 4 faders, 4 rotary encoders, per-track
buttons, drum pads. Currently bench-powered over USB. This brief covers
making it battery-powered and adding a mic input + speaker output.

## 2. Subsystems that need power

| Subsystem | Voltage | Notes |
|---|---|---|
| Teensy 4.1 | 5V in (VIN), regulates its own 3.3V core | Main MCU, runs the audio engine + UI |
| Teensy Audio Shield (SGTL5000 codec) | 3.3V, own onboard regulator from 5V | Already installed, stacked on the Teensy via tall headers |
| ESP32 co-processor | 3.3V | WiFi/networking only — see §3.3. Talks to the Teensy over UART, not yet wired |
| Speaker + amplifier | TBD, likely battery-direct (~3.0–4.2V) | Codec's headphone out can't drive a real speaker at volume — needs a class-D amp stage that doesn't exist yet |
| Mic input: 48V phantom + preamp | 48V (phantom feed) + a clean low-voltage rail (preamp) | XLR condenser mic input — doesn't exist yet, needs designing from scratch |

## 3. Already-confirmed hardware / constraints

### 3.1 GPIO is nearly exhausted

Direct-wired so far: 4 faders (analog), 4 encoders × 3 pins, 8 per-track
buttons (fx-select + drum pad), full SPI bus for the display/touch. That's
~44 of the Teensy 4.1's ~55 usable pins. **The only bus with headroom is
I2C (pins 18/19, SDA/SCL)** — shared with the audio codec's control
interface already, and reserved for a future MCP23017 GPIO expander
(transport + power buttons, not yet built). Any new interconnect (e.g. the
ESP32 link) needs to either use pins not yet listed in the pin map, or
share I2C/UART rather than assuming spare direct GPIO exists.

Full current pin map: `docs/pin_map.md` in this repo (faders, encoders,
buttons, display/touch SPI, all confirmed with pin numbers).

### 3.2 SD card and audio

- SD card uses the Teensy 4.1's **built-in SDIO slot** (`BUILTIN_SDCARD`),
  not the Audio Shield's own microSD slot — separate pins, already fixed,
  no decision needed here.
- Audio graph is built on the stock **Teensy Audio Library**
  (`AudioMixer4`, `AudioEffectDelay`, `AudioEffectFreeverb`, etc.) driving
  the SGTL5000 via I2S. A speaker amp fed via I2S directly from the Teensy
  (bypassing the codec's analog output) is architecturally the cleanest
  option — see §4.3.
- USB is currently used for: firmware upload, a serial control/logging
  protocol, and **MTP file transfer already implemented in firmware**
  (`USB_MTPDISK_SERIAL` build mode) — the SD card shows up as a drive on a
  PC over the same USB connection. This matters for §5 below: the physical
  USB port needs to keep carrying real USB data, not just power.

### 3.3 ESP32's purpose (why it's in the system at all)

Teensy 4.1 has no onboard WiFi/Bluetooth. The ESP32 is a **wireless
co-processor**, talking to the Teensy over a simple **UART link** (TX/RX +
a reset line — not yet wired, budgeted at ~3 pins in early planning).
Design rule: the Teensy owns real-time audio and must never block on the
network; the ESP32 handles all network I/O asynchronously and reports
back over UART. Intended use: cloud stem-separation (send audio up,
receive 4 stems back) and eventually project sync. **Not yet decided**:
exact ESP32 module (e.g. plain ESP32-WROOM vs. a dev board), and whether
it needs anything beyond UART + power (some modules need EN/boot-mode
pins pulled a certain way at startup — check the specific module chosen).

## 4. Recommended power architecture

One 3.7V nominal Li-ion/LiPo cell (3.0–4.2V range across its discharge
curve), feeding four independent rails rather than one shared rail —
each subsystem's failure mode is different enough that sharing causes
real problems (browning out the ESP32's WiFi burst, or noise in the mic
path).

### 4.1 Battery + charging

- Single-cell Li-ion/LiPo, **protected** (built-in protection circuit, or
  add a standalone protection IC — e.g. a DW01+FS8205 pair — if using a
  bare cell). Never wire a bare cell straight to the converters below.
- Charge IC: **power-path** design (charges the battery *and* powers the
  system simultaneously from USB, without forcing the battery through
  charge/discharge cycles while in use) — e.g. **MCP73871**, USB-C input.
  A simple non-power-path charger (e.g. TP4056) is cheaper but means the
  device can't be used normally while charging.
- Target capacity: not yet decided — see §7 current budget for what
  drives this choice.

### 4.2 Digital rails (Teensy + ESP32)

- **5V buck-boost** (e.g. **TPS63060**, up to 96% eff., ~2A) feeding the
  Teensy's **VIN** pin and the Audio Shield's onboard regulator. Needs
  buck-boost specifically because the cell's 3.0–4.2V range straddles 5V —
  a plain boost or buck alone can't cover the whole discharge curve.
- **ESP32 gets its own dedicated 3.3V regulator** (e.g. **TPS62203**,
  600mA–1A capable), fed from the 5V rail or the battery directly — do
  **not** share the Teensy's onboard 3.3V regulator. WiFi TX bursts pull
  up to ~500mA for milliseconds; sharing a regulator risks browning out
  both chips on the same spike.

### 4.3 Speaker amplifier

- **I2S class-D amp** (e.g. **MAX98357A**, 2.7–5.5V in, ~3W into 4Ω) fed
  directly from the Teensy's I2S bus, in parallel with (or instead of) the
  codec's own output — cleaner than tapping the SGTL5000's analog
  line-out into a separate analog class-D amp.
- Can run straight off the battery (no dedicated regulator needed) if the
  chosen IC tolerates 3.0–4.2V input directly — check the specific part.
- Not yet decided: speaker size/impedance/target loudness — affects which
  amp IC and what continuous/peak current to budget.

### 4.4 Mic input: 48V phantom power + preamp

This is the one genuinely new analog design in the system, and the one
most sensitive to noise.

- **Low-current boost converter** to 48V (phantom current is only a few
  mA per mic — efficiency doesn't matter here, but switching noise
  reaching the audio path does). Cap it around 10–20mA.
- Standard phantom feed: **two matched 6.8kΩ resistors** from the 48V
  rail to XLR pins 2 and 3 (relative to pin 1/ground), per the P48
  standard.
- **DC-blocking capacitors** between the XLR pins and the preamp input —
  the mic's audio signal rides on top of the 48V DC bias and must be
  decoupled before the preamp stage.
- **Balanced-to-single-ended preamp** with real gain (mic level is
  roughly 40–60dB below line level) — e.g. a discrete diff-amp with a
  low-noise op-amp (NE5532, OPA1612) or an instrumentation-amp-style mic
  preamp IC (e.g. INA217-class). Feed it from its own clean, filtered
  supply tap — not a rail shared with the digital boards.
- **Critical**: filter the 48V boost output hard (LC filter, ideally
  followed by a linear post-regulator) before it reaches the phantom
  resistors. This is the single biggest audio-quality risk in the whole
  system — unfiltered switching ripple here shows up as audible whine in
  the recorded signal.
- Output feeds the Audio Shield's **LINE IN** (not MIC IN — mic bias on
  that pin isn't relevant once you have a real external preamp).
- Not yet decided: single mic input or provision for more than one.

## 5. USB-C: one connector for power AND data

Teensy's own USB port already carries both power and data (that's what
the MTP file-transfer feature already uses). Bring a single external
USB-C jack to the enclosure and split it right at the connector:

- **D+ / D− / GND** go straight through to Teensy's native micro-USB port
  (a short USB-C→micro-USB pass-through pigtail/breakout — Teensy doesn't
  expose D+/D− on any header pin, only at that connector, so this must
  stay a real short USB link, not wires soldered to arbitrary pins).
- **VBUS / GND** also branch off, in parallel, into the charge IC's input
  from §4.1.
- Feed the charge IC's regulated system output into Teensy's **VIN** pin
  — never into VUSB. **Teensy 4.1 already has an onboard Schottky diode
  between VUSB and VIN that auto-arbitrates** (plugged into USB → runs
  off USB, diode blocks backfeed toward the host; unplugged → runs off
  VIN/battery), with <1mA reverse leakage. Wiring a separate 5V source
  into VUSB directly would put two live 5V sources on the same net
  whenever a PC is also connected — the one wiring mistake to avoid.

## 6. Current budget (rough estimates — confirm against real parts once chosen)

| Subsystem | Typical | Peak |
|---|---|---|
| Teensy 4.1 (display, SD, audio DSP) | 150–250mA | 350mA |
| ESP32 (WiFi active) | 80–160mA | 500mA |
| SGTL5000 codec | 15–20mA | — |
| Speaker amp (moderate volume) | 100–300mA | 1–2A |
| 48V phantom + preamp | ~200mA equivalent* | — |
| **Total (battery-equivalent)** | **~600–900mA** | **2–2.5A** |

\* 48V @ 15mA ≈ 0.7W ≈ 200mA at 3.7V after boost losses — small in
isolation, but the rail where clean current matters more than the amount.

Battery sizing: pick a cell/pack with a discharge (C) rating above the
2–2.5A peak; a 2500–3000mAh single cell gives roughly 3–4 hours at
typical draw with headroom for peaks. Runtime target not yet decided.

## 7. Design constraints to hold onto while drafting

1. **Noise isolation for the mic path** — every switching regulator here
   (5V, 3.3V, 48V) radiates/conducts switching ripple. The 48V rail and
   preamp are the most sensitive things in the system; filter and
   decouple them more aggressively than anything else.
2. **Star grounding** — one single return path to the battery negative
   terminal, with the mic/phantom analog ground kept separate until that
   one point. Ground loops between the digital boards and the analog
   front end will show up as audible hum/whine.
3. **Real battery protection** — see §4.1, don't skip it.
4. **Power-path charging** — see §4.1, needed if the device should be
   usable while plugged in and charging.
5. **GPIO scarcity** — see §3.1. Any new signal (ESP32 UART, amp
   enable/mute lines, etc.) needs to be checked against the existing pin
   map before assuming a pin is free.

## 8. Open questions for this schematic session

- Target runtime / battery capacity.
- Exact ESP32 module and its pin needs beyond UART TX/RX (EN, boot-mode
  strapping, etc. depend on the specific module).
- Speaker: size, impedance, target loudness (drives amp IC choice and
  current budget).
- Mic input: one XLR jack, or provision for more than one mic?
- Enclosure constraints (size, connector panel layout, thermal) — not
  captured here at all yet, and will affect connector/component choices.
- Whether the guitar/instrument input (discussed separately, needs a DI
  or high-Z buffer stage, not phantom power) should be designed
  alongside this same power system or treated as a fully separate front
  end.
