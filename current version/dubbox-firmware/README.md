# Dub-Box firmware

> **Picking this project up on another machine or AI session? Start with [HANDOFF.md](HANDOFF.md)** (current state, wiring, build/flash steps, debugging aids, known problems).

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

## Master output volume

`AudioEngine::setMasterVolume()` now applies stereo digital gain after the final
mix and before I2S output, including the optional bench tone. The existing display
header slider sends `U,0` through `U,1000` (mute to unity); no protocol change is
needed. Previously it only changed the codec headphone register, which cannot
control speaker amplifiers connected directly to I2S. The hardware brief proposes
an I2S1 speaker tap. This gain also controls codec line out, headphones, and any
Bluetooth capture of that same bus; it is not an independent speaker-only level.
Recording gains and per-track faders remain independent. The codec headphone
register stays at its previous startup setting.

Validation: normal Teensy 4.1 PlatformIO build succeeds. Actual installed speaker
wiring, audible attenuation/mute, and playback quality need hardware validation;
this change has not been flashed as part of this task. The standard AudioAmplifier
gain setter is immediate, so large slider jumps may click; smoothing is a future
refinement. Bench-tone routing is updated but its optional build is not yet tested.

## Audio allocation regression fix

The stereo WAV player now clears released blocks after allocation failure and retains
valid duration/seek metadata when the right-channel block is unavailable during header
parsing. It exits that update safely instead of consuming PCM through a null pointer.
This addresses memory-pressure crashes, not SD seek latency or display brownouts.

Run `sh tests/run_audio_memory.sh` with a C++17 compiler and Address/UndefinedBehavior
Sanitizer support. The test compiles the actual player against small host substitutes
for SD and audio allocation. It exercises stereo header allocation failure, recovery,
normal-playback allocation failure, stop, and balanced block release. The original
handoff version crashes in this test; the patched version passes. Hardware playback
validation is still required.

## Recorded piano and selectable drum kits

The flash-resident CC0 bank in `src/samples/` provides five piano anchors and
an eight-row acoustic drum kit. New instruments default to recorded Piano;
new projects use Acoustic drums. Existing projects retain their old patches and
Classic drums when no DRUMKIT field is present. The display Piano preset selects
the recorded voice; the drum chooser switches Classic/Acoustic. See the sample
bank README for sources, license, conversion and quality limitations.

Validation: Teensy build, audio-allocation regression and sample cursor sanitizer
checks pass. The sound firmware was flashed and booted successfully on the connected
Teensy 4.1; master volume was restored and confirmed at 25 percent. Full-arrangement
CPU, clipping and listening tests remain outstanding. Build usage: 5,509,116 bytes
of flash including headers; RAM1 has 133,920 bytes available for local variables,
and RAM2 has 349,888 bytes available for heap allocations. These are link-time
figures, not a worst-case runtime guarantee.

The bundled Teensy Loader CLI 2.2 rejected the HEX at the one-megabyte boundary.
Official PaulStoffregen teensy_loader_cli revision
`03fca4156c244c7ad36bd368cf6e24531dbd566a` (version 2.3), built for macOS,
accepted and successfully programmed the same HEX using `--mcu=TEENSY41 -w -v`.
Use a current official uploader for this larger bank; the ordinary bundled
PlatformIO upload command may still fail until its uploader is updated.
