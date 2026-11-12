# Dub-Box handoff (state as of 2026-11-11)

Read this first if you are picking the project up on another machine or in a new AI session. It says what exists,
what has been verified on real hardware, how to build, flash and debug it, and what is still open. Detailed
references are linked, not copied, so they do not go stale here.

## What the project is

A portable four-track instrument ("Dub-Box", part of STUDIO). Two boards:

| Board | Folder | Role |
|---|---|---|
| Teensy 4.1 + Audio Shield Rev D2 (SGTL5000) | [`dubbox-firmware`](README.md) | Audio engine, SD projects, drum machine, synth, physical controls (faders, encoders, buttons, pads), mic/line recording |
| ESP32 (NodeMCU-32S) driving a 5" 720x1280 LT7683 touchscreen (GT911 touch) | [`../dubbox-display`](../dubbox-display/README.md) | The whole touchscreen UI, WiFi upload, Bluetooth audio out, USB-MIDI bridge |

They talk over a UART (Teensy `Serial1` pins 0/1 <-> ESP32 GPIO17/GPIO4, 2,000,000 baud, newline-terminated text
lines, first character selects the command). The protocol is documented at the end of
[`src/esp_link.h`](src/esp_link.h); the display side is `dubbox-display/src/teensy_link.cpp`.
Hardware drawings, briefs and the protoboard layout tool are in [`../hardware/dubbox`](../hardware/dubbox/README.md).

## State of the work

**Verified on hardware (by the maker):** four-track WAV playback with a shared transport, per-track effect chains,
projects on SD, waveform and beat display, clips (snip/move/join/undo), master volume, drum machine with
pattern clips, recording a pad pattern with a count-in and click, synth instruments (up to 4), WiFi upload,
recording from the internal mic / line inputs onto a track, the INPUT screen, adding a file to a track from the
picker (short file names), and the physical faders, encoders and pads as wired before 2026-11-11.

**Changed on 2026-11-11 and only partly tested:**
- New MAIN MENU > CONTROLS TEST screen (live faders, encoder turns/pushes, 8 buttons; streamed from the Teensy
  with the `#` command only while the screen is open).
- The MCP23017 I2C expander was removed from the build: FX buttons 1-4 are now read directly on Teensy pins
  28-31. Fader order and drum-pad order were reversed in `pins.h` to match the actual wiring.
- Reliability fixes: command framing on the UART, SD access ordering, atomic project saves, autosave deferral,
  crash report written to `CRASH.TXT` on the SD card, test-tone code compiled out by default.
- Screen changes are drawn into a hidden display-RAM layer and copied over in one step (to remove a black flash).
  Built and flashed, **not yet confirmed visually**.
- File-name buffers raised from 64 to 128 characters; `ProjectManager::assignTrack` also resolves a name cut at
  63 characters to the one file that starts with it (the display used to cut names). Tested by sending the
  command over USB, **not yet by tapping through the touchscreen**.
- FX buttons on pins 28-31 and the reversed fader/pad order: built and flashed, **not yet confirmed by the maker**
  on the CONTROLS TEST screen.

**Not verified / open:**
- Why the display occasionally resets: see "Known problems" (power).
- Audio dropouts while playing when a clip boundary seek or realign happens (SD open/seek with the audio
  interrupt disabled, `audio_play_sd_wav_seekable.cpp`).
- Long single-shot stalls in `loop()` (beat analysis, `delay(20)` loops in `project.cpp`).
- Audio block pool (280) is large but a 650 ms delay uses most of it; watch `mem: x/280 (peak y)` in the status line.
- Transport and power buttons are not built; input settings are not saved across restarts; effect state is not saved
  in projects; MP3 is not supported (WAV 16-bit PCM 44.1 kHz only).

## Wiring (source of truth: `src/pins.h`)

| Signal | Teensy 4.1 pin |
|---|---|
| UART to/from ESP32 | 0 (RX1), 1 (TX1), common ground required |
| Faders 1-4 wipers (outer legs 3V3/GND) | 27 (A13), 26 (A12), 25 (A11), 24 (A10), order reversed 2026-11-11 |
| Encoders 1-4 A / B / push | 33/34/35, 36/37/38, 6/9/10, 22/39/40 (pins 33-40 are bottom solder pads) |
| Drum pads 1-4 (other leg GND, pull-ups) | 17, 16, 41, 14 (reversed 2026-11-11) |
| FX buttons 1-4 | 28, 29, 30, 31 |
| Audio Shield I2S1 / codec | 7, 8, 20, 21, 23 and I2C 18/19 (fixed by the shield) |
| Internal mic I2S2 (ICS-43434) | 3 (LRCLK), 4 (BCLK), 5 (data in); Teensy drives the clocks |
| Free | 2, 11, 12, 13, 15, 32 |

ESP32 side: display bus pins and SSD2828 bit-bang pins are in `dubbox-display/src/main.cpp` and `LCD.h`
(remapped off the boot-strap pins 0/2/12); Bluetooth listens to the Teensy's I2S1 bus on GPIO34 (BCLK),
GPIO35 (LRCLK), GPIO36 (data). Full per-module notes: [`docs/pin_map.md`](docs/pin_map.md) (parts mentioning the
MCP23017 are out of date).

## Build and flash

Requires PlatformIO. On Windows set `PIP_USER=0` before `pio run` (pip user installs break PlatformIO tools).

```
# Teensy (this folder)
pio run                 # build
pio run -t upload       # flash; a first "error writing to Teensy" followed by a good retry is normal
pio device monitor      # 115200 baud; prints a status line every second

# ESP32 display (../dubbox-display)
pio run
pio run -t upload       # upload_port is pinned to COM6 in platformio.ini: change it for your machine
```

**ESP32 flashing on this particular NodeMCU-32S is unreliable from the PC** (esptool reports "Wrong boot mode
detected (0x17)": the auto-reset does not pull GPIO0 low). What worked: connect the board's **P0 (GPIO0) pin
to GND with a jumper**, repeatedly reset the chip (pulse RTS) while reading the boot banner until it prints
`boot:0x7 (DOWNLOAD_BOOT...)`, then run `pio run -t upload`, then remove the jumper. A 10 uF capacitor between EN
and GND is the usual permanent fix for dev boards like this one (not yet done). A classic ESP32 has no
software "reboot into download mode" register.

**Do not hold the ESP32's serial port open from a PC script while it runs** (opening it with DTR/RTS handling
reset the board repeatedly in testing). Log only the Teensy's USB serial, or use the debug hook below.

## Debugging aids built in

- Status line on the Teensy USB serial every second: faders, which tracks play, CPU, audio memory, loop time,
  drum info, peaks, mic level, and `fx held: abcd` (the four FX buttons).
- A USB-serial line starting with `>` runs a link command as if the display had sent it, for example
  `>O,PROJECTNAME` (open a project) or `>T,0,FILE.WAV` (put a file on track 1). This lets you exercise the
  Teensy from a script without touching the screen.
- After a hard fault the Teensy appends its crash report to `CRASH.TXT` on the SD card root (also printed on
  USB serial about 6 s after boot). Read it over the MTP drive.
- Build flag `-DDUBBOX_TEST_TONE` (commented out by default in `platformio.ini`) enables a 1 kHz test tone,
  the pan test (`t`, `p`, `x` over USB serial) and a spectrum print.
- The display prints its reset reason at boot on its USB serial (`RESET REASON: ...`; look for BROWNOUT).

## Known problems and lessons

- **Display resets / "pulsing":** the display's 5 V (about 320 mA) comes from the ESP32 VIN and both boards run
  from PC USB. The ESP32 reported brownouts. Treat the supply as marginal: power the display from a real 5 V rail
  (a bench supply or powered hub with a short thick cable) and add 470-1000 uF at the ESP32 VIN. A build that
  lowered the CPU clock and the brownout threshold boot-looped, so those tweaks were removed; do not re-add
  them without a measurement.
- Keep the display's redraws small: draw only what changed, avoid text in interactive paths (see
  `dubbox-display/src/ui_common.*` and the screens).
- SD card access from `loop()` must not overlap playback: the audio interrupt streams from the card. Pause first.
- MTP (USB drive) only runs while paused; background scans only while paused and with MTP skipped.
- Names of WAV files are limited to 127 characters.
- Teensy RAM: mark rare or boot-only functions `FLASHMEM`.
- Never put quoted or backslashed code in shell heredocs or `python -c`; write scripts to files.

## Suggested next steps

1. Confirm on hardware: flicker-free menus, adding long-named files, a full play session without display resets.
2. Fix the display power (see above), then reflash the display from a clean tree.
3. Smooth the dropouts: do SD open/seek outside the audio-interrupt-disabled window, stagger the four
   `play()` calls, throttle `L`/`B` while playing.
4. Split the beat analysis across loop iterations; replace the `delay(20)` loops with polling.
5. Save input settings and effect state; build transport/power buttons.
6. Update `docs/pin_map.md` and `docs/bringup.md` for the current wiring.
