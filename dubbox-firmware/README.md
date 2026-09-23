# Dub-Box firmware

Teensy 4.1 firmware for the Dub-Box hardware 4-track DAW. The Teensy runs the audio engine
and the physical controls; the touchscreen UI runs on a separate ESP32 board
(`../dubbox-display`) connected over UART.

## What it does

- Plays up to four WAV tracks from SD through the Audio Shield (SGTL5000), mixed to stereo,
  with a shared transport clock. Each track can be snipped into clips that move freely on the
  timeline (`src/audio_engine.*`).
- Per-track effect chains (Delay, Flange, Freeverb, Chorus) with dub-style mutes that let the
  tails ring out.
- A drum machine with synthesized voices (`src/drum_machine.*`): 8 patterns of 16 steps x 8 drums,
  looped on their own or placed on the song timeline as pattern clips.
- A polyphonic synth (`src/synth.*`): two oscillators, ADSR, filter, chorus and reverb, with presets;
  played by pattern piano parts, the on-screen keyboards and the pads in note mode.
- Projects saved as text files in `/PROJECTS` on the SD card (`src/project.*`).
- Background waveform and beat-grid scans for the display, run only while paused
  (`src/waveform.*`).
- The SD card also shows up as a USB drive (MTP) for dragging WAVs on, while paused.
- WiFi upload: the display's ESP32 joins your home WiFi (MENU > WIFI UPLOAD; the password is typed on the
  screen and remembered) and serves a page at its address (or http://dubbox.local). The page converts any
  audio file to a 44.1 kHz 16-bit stereo WAV in the browser and uploads it; the ESP32 relays it over the UART link
  (2 Mbaud, ~190 KB/s, CRC-checked 1 KB chunks) and the Teensy writes it to the SD card root, where it appears
  in the track picker. Refused while a song is playing. Code: `dubbox-display/src/wifi_upload.*`,
  `upload_page.h`, `screen_wifi.*`; protocol at the end of `src/esp_link.h`.
- Physical controls (`src/input_manager.*`, wired in `src/main.cpp`): four faders, four
  encoders (wet level / effect type / effect parameter), four fx buttons (bypass) and four
  drum pads that play drum sounds. Route a sound to a pad by holding its name on the display's
  Pattern screen and pressing the pad.
- The display protocol is documented in `src/esp_link.h`.

## Build

```
pio run                # build
pio run -t upload      # flash to Teensy 4.1
pio device monitor     # serial console (115200 baud); prints a status line every second
```

Requires [PlatformIO](https://platformio.org/). If an upload hangs waiting for the bootloader,
press the PROGRAM button on the Teensy once.

## Setup notes

- Put WAV files on the SD card (16-bit PCM, 44.1 kHz, mono or stereo). On a first boot with no
  projects and `TRACK1.WAV`..`TRACK4.WAV` at the card root, a "DEMO" project is created from them.
- Wiring: `docs/pin_map.md`. Display link and protocol: `src/esp_link.h`,
  `docs/display_architecture_brief.md`.
- Do not read the SD card (MTP, saves, scans) while tracks are streaming; it locks the Teensy up.
