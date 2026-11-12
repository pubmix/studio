# Dub-Box display (ESP32 touchscreen UI)

## Purpose and scope

The touchscreen user interface of the Dub-Box. It runs on an ESP32 (NodeMCU-32S) that drives a 5" 720x1280 panel
(LT7683 display controller with 16 MB display RAM, SSD2828 MIPI bridge, GT911 capacitive touch) and talks to the
Teensy 4.1 audio firmware in [`../dubbox-firmware`](../dubbox-firmware/README.md) over a 2 Mbaud UART. The Teensy
owns all audio and storage; this board only draws, reads touch, and relays WiFi/Bluetooth/MIDI. Start with
[`../dubbox-firmware/HANDOFF.md`](../dubbox-firmware/HANDOFF.md) for the whole-project picture.

## Status

Implemented and used on hardware: main menu, new/load project, mixer, playlist (clips, snip, undo, zoom),
pattern/drum machine, instruments (synth panel, piano roll, keyboard), input screen, file picker, WiFi upload,
Bluetooth A2DP audio out, USB-MIDI bridge. New on 2026-11-11 (built and flashed, not fully confirmed): CONTROLS TEST
screen and flicker-free screen changes (see below).

## Important files

| File | What it does |
|---|---|
| `src/main.cpp` | Boot: display and touch init, then `ui::begin()`; loop calls `ui::update()` |
| `src/ui.cpp`, `ui_common.*` | Screen routing, header, transport, drawing helpers, staged (flicker-free) screen changes |
| `src/teensy_link.*` | The UART protocol (parser for Teensy lines, senders for display commands) |
| `src/screen_*.cpp` | One file per screen (menu, mixer, playlist, pattern, input, picker, wifi, bluetooth, midi, controls) |
| `src/LCD.*`, `SSD2828.h`, `touch_gt911.*` | Vendor display driver (pruned) and touch driver |
| `src/wifi_upload.*`, `upload_page.h` | WiFi web upload page and relay to the Teensy |
| `src/bt_audio.*` | Bluetooth A2DP source fed from the Teensy's I2S bus |

## Build and flash

Needs PlatformIO (`PIP_USER=0` on Windows). `pio run`, `pio run -t upload`. `upload_port` is pinned to `COM6` in
`platformio.ini`; change it for your machine. Flashing this board from a PC is unreliable: see "Build and flash" in
the firmware HANDOFF (GPIO0 jumper procedure).

## Design notes

- Landscape 1280x720 on a portrait panel: landscape (lx, ly) maps to panel (px, py) = (ly, 1279 - lx).
- The UI is drawn with display-chip draw commands and BTE copies between display-RAM layers (visible, bright, dim,
  composed, staging); LVGL pixel pushing is too slow on this link. SPI starts at 8 MHz, then 20 MHz (40 MHz hangs).
- `ui::beginStagedDraw()` / `presentStagedDraw()`: a screen change is drawn into the hidden `kStaging` layer
  (display RAM address 7,372,800) and copied to the visible layer in one step.
- Redraw only what changed; avoid text in interactive paths.
- Bluetooth and WiFi do not both fit in heap: they are mutually exclusive per boot (toggling Bluetooth restarts
  the ESP32). Partition table is `huge_app.csv`.
- Do not hold this board's USB serial port open from a PC script while it runs; it resets the board.

## Limitations and next steps

Marginal 5 V supply (brownout resets) and the flaky auto-reset are hardware issues, see the firmware HANDOFF.
Next: confirm the staged drawing visually, then fix the display power and reflash from this tree.
