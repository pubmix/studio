# Dub-Box Display Architecture — Screen-on-ESP32 Brief

> **Status (2026-09-21):** built and working. The UI uses chip draw commands and BTE copies (not
> LVGL), the link protocol in section 5 has grown (see `src/esp_link.h` for the current one), and
> the old ILI9488 fallback has been removed from the Teensy firmware.

Companion to `power_system_schematic_brief.md`. Covers the decision to drive
the new touchscreen from the ESP32 instead of the Teensy, and the UART
protocol that keeps it in sync with the audio engine. Revised after checking
against the ER-TFT050-10-6304 module datasheet and the LT768x chip datasheet.

## 1. The hardware being driven

ER-TFT050-10-6304: 5" IPS TFT, 720x1280 (portrait), LT7683 graphics
controller on a controller board (PCBA ER-PCBA6304-1) with 128Mb (16MB)
onboard display RAM. Plain SKU (no Arduino shield): the host connector is a
20-pin FFC (CON1), broken out to a header via an FFC breakout board.

Datasheet facts that shape the design:

- Current: up to 420mA @3.3V or 320mA @5V. Add to the power budget.
- Interface: 3-wire/4-wire SPI or I2C, selected by jumpers J1-J4.
  Factory default is 4-wire SPI (J2/J3 short, J1/J4 open), 5V supply
  (J7 open), external backlight control (J5 short, J6 open).
- Logic I/O (VDDIO) is 3.3V regardless of J7, so the ESP32 connects directly
  (confirm on a meter before first power-up; derived from a garbled table).
- ~4.8x the pixels of the old 480x320 ILI9488.

## 2. Architecture: ESP32 drives the display, not the Teensy

Whichever chip is wired to the display SPI bus does the UI work. Wiring the
panel to the ESP32 frees Teensy compute/RAM. What crosses the Teensy-ESP32
link is state, never pixels or draw commands.

- Teensy: real-time audio, SD card, all audio-engine state.
- ESP32: display, touch, UI rendering, working from state the Teensy sends.

Module: ESP32-S3 with PSRAM preferred over classic WROVER (better graphics
performance, and see the GPIO16/17 note in section 4).

Side benefit: moving the UI off the Teensy should relieve the RAM1 stack
headroom problem (~11KB free after enabling MTP) that is a suspect in the
open track-reload crash.

## 3. LT7683 findings (from the chip datasheet)

- **SPI clock max: 50MHz.** ESP32 realistic setting: 40MHz.
- **Bandwidth is the bottleneck.** Every byte in the SPI protocol carries an
  8-bit command prefix (A0/RW# bits, data on clocks 9-16): one data byte costs
  16 clocks. At 40MHz that is ~2.5MB/s at best. A full 720x1280 RGB565 frame
  is 1.84MB, ~0.74s per full-screen push, slower in practice (chip select is
  framed per access; unknown whether CS can stay low across many bytes).
- Consequence: a naive LVGL framebuffer-flush design is a poor fit. Partial
  updates are fine (a 600x100 waveform strip is ~50ms). Better: use the chip's
  hardware drawing (lines, rectangles, text, BTE block copies). A waveform
  drawn as vertical lines is a handful of commands instead of a pixel push.
  Decide LVGL-vs-draw-command UI after a bring-up measurement.
- **Resolution discrepancy:** datasheet lists LT7683+ max as 1024x768, but the
  panel is 720x1280 (921,600 px vs 786,432). The module works as sold, so
  BuyDisplay's timing setup handles it. Use the vendor demo code for display
  init/timing rather than deriving it from the LT7683 datasheet.
- 128Mb display RAM holds ~8 full-screen layers at 720x1280 RGB565.
- No existing LVGL driver for LT7683; a bare Arduino graphics library exists
  (github.com/ToSStudio/LT7683).

### 3.1 Measured on the bench (classic ESP32 DevKitC, 2026-09-20)

Display and GT911 touch both work (touch reports the full 0-720 x 0-1280).

| Operation | 8MHz SPI | 20MHz SPI |
|---|---|---|
| Full-screen hardware fill | 22 ms | 22 ms |
| 600x100 hardware rect | 1.5 ms | 1.4 ms |
| Raw pixel push (100x100) | 93 ms, 209 KB/s | 63 ms, 309 KB/s |

- 40MHz hangs (status reads never complete) once the SPI clock is raised after
  init. 20MHz is stable. Init must run at 8MHz (the chip's PLL isn't up yet),
  then switch speed.
- Raising the clock only gives 1.5x: pixel push is bounded by per-byte
  overhead (chip-select framing per byte, 16 clocks per data byte), not the
  clock. A full frame push would take seconds. **Conclusion: build the ESP32
  UI on the chip's draw commands (fills, lines, text, BTE copies), not on
  LVGL pixel flushing.** Static graphics should come from the on-module SPI
  flash via the chip's DMA path (vendor DMA demo), not pixel pushes.
- Vendor GT911 sketch defines reset/INT as pins 6/7 (flash pins on an ESP32);
  we use 26/27 instead.
- Gotcha: `SPI.setFrequency()` deadlocks with the vendor's open transaction;
  `lcmSetSpiHz()` in `LCD.cpp` restarts the transaction instead.

## 4. Wiring: CON1 (20-pin FFC) to ESP32

Zero impact on the Teensy pin map. First bring-up is standalone (ESP32 on PC
USB, not connected to the Teensy). Pins below are for a classic ESP32 DevKit
(VSPI); re-map if using an S3.

| CON1 pin | Symbol | ESP32 GPIO | Notes |
|---|---|---|---|
| 1-2 | VSS | GND | both pins |
| 3-4 | VDD | 5V (VIN) | J7 open (default). ~320mA; not from the ESP32 3.3V regulator |
| 5 | LCM_SCS | 5 | SPI chip select |
| 6 | LCM_SDO | 19 (MISO) | chip output to host |
| 7 | LCM_SDI | 23 (MOSI) | host output to chip |
| 8 | LCM_SCLK | 18 (SCK) | vendor demo runs 8MHz; 40MHz to be tested |
| 9 | BL_Control | 3.3V | default is external control: dark if floating. HIGH = on |
| 10 | LCM_INT | not connected | vendor demo doesn't use it |
| 11 | LCM_RESET | 16 | matches vendor demo |
| 12 | CTP_INT | 27 | GT911 touch, I2C addr 0x5D |
| 13 | CTP_RST | 26 | |
| 14 | CTP_SDA | 21 | |
| 15 | CTP_SCL | 22 | |
| 16 | 2828_CS | 25 | **required**, bit-banged SSD2828 init |
| 17 | 2828_RST | 33 | **required** |
| 18 | 2828_DIN | 32 | **required** |
| 19 | 2828_SCLK | 13 | **required** |
| 20 | 2828_DOUT | not connected | not used by demo |

The vendor demo bit-bangs the SSD2828 MIPI bridge from the ESP32 at boot, so
pins 16-19 are NOT optional (an earlier revision of this doc said to leave them
open: wrong). The vendor used GPIO 0/2/4/12 for these; we remapped to
25/33/32/13 because GPIO 0, 2 and 12 are ESP32 boot-strapping pins, and a
display input pulling one the wrong way at reset can block booting/flashing.
Project: `dubbox-display/` (PlatformIO, vendor driver copied into `src/`).

Notes:
- On WROVER/PSRAM ESP32 modules GPIO16/17 belong to the PSRAM (the plain
  DevKitC/WROOM in use now is unaffected, but keep it in mind for a later
  swap); do not put the Teensy UART on them. UART pins are remappable; pick free GPIOs.
- Common ground everywhere. Verify FFC breakout pin order with a continuity
  meter (pin 1 orientation can flip depending on contact side).
- JP1 (7-pin) is only the LT7683 flash-download port. Not used.

## 5. Teensy-ESP32 link (later, after the display works)

UART: Teensy Serial1 (RX=0, TX=1) to an ESP32 UART on remapped pins, plus a
reset line to ESP32 EN.

**Correction:** the reset line cannot be Teensy pin 23. Pin 23 is the Audio
Shield's MCLK. Use pin 15 (shield VOL pin, unused) or, once the ILI9488 is
retired, one of the freed pins 28-32.

Retiring the ILI9488 also frees Teensy pins 11-13 and 28-32, which helps the
MCP23017/button pin shortage.

### 5.1 Touch: resolved gestures (ESP32 to Teensy)
ESP32 decodes tap/swipe/drag/long-press; sends only resolved events.

### 5.2 Waveform: precomputed peak cache (Teensy to ESP32)
Min/max peak pairs per pixel column, multi-resolution (mipmap-style), sent
once per track on record-finish/import/edit. Incremental appends (~20-50ms)
during recording. ~100-150KB for a 5-minute multi-track project, 1-1.5s
one-time transfer at realistic UART rates.

### 5.3 Playhead + meters (Teensy to ESP32)
~10-20Hz, a handful of bytes each.

### 5.4 Editing: use non-destructive crop, not file surgery
Existing firmware already has non-destructive crop (playback window gating in
`audio_engine`, never touches the WAV file). Send:

    CMD_CROP { track_id, start_ms, end_ms }

Two-phase: live drag preview is local on the ESP32 (no UART traffic), one
commit message on explicit confirm. Because the WAV is never modified, a power
loss cannot corrupt anything and no undo machinery is required. Destructive
trim/split (temp-file-then-rename, one-level undo) is a later, separate
feature if ever needed.

## 6. Protocol notes

- UART needs framing: message type + track ID + zoom level + length +
  payload + checksum.
- Tune baud deliberately: both chips support several Mbaud; ~92KB/s
  (921600) is just a conservative sanity figure.

## 7. Migration plan

1. Bench-prove the display on the ESP32 alone (this doc, section 4). Run the
   vendor demo; measure full-screen fill time and partial-update time.
2. Choose UI approach (LVGL with dirty regions vs chip draw commands) from
   those measurements.
3. Only then build the UART link and port the UI. Keep the working ILI9488
   Teensy UI as the fallback until the new display is proven.
4. Porting scope: playlist screen, crop handles, FX picker, file picker,
   per-track color ribbons all call `audioEngine` directly today; those
   become UART messages.

## 8. Still open

- Vendor demo/init code for this exact module and its ESP32 compatibility.
- ESP32 module (classic vs S3) and final GPIO assignments.
- Whether Audio Shield Rev D2 exposes headphone jack-detect over I2C.
- Number of mipmap zoom levels.
- Real bench test of ESP32 headroom (UI + WiFi bursts).
- Whether simple crop is v1 (recommended) with destructive edit deferred.
