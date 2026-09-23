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

## Modular instrument

The four instrument slots can now contain ordinary synths or six-voice modular
instruments. The modular DSP is a validated graph inside a Teensy AudioStream;
notes follow the same sequencer, keys and pads as existing synths. Typed routing,
parameters and instrument type persist in backward-compatible `MODn=` project
lines. Implementation: `src/synth.*`, `src/drum_machine.*`, `src/project.*`,
`src/esp_link.*` and `../packages/modular`. See the
[modular README](../packages/modular/README.md) for workflow, tests and current limits.

## Per-track pan (2026-09-23)

`AudioEngine::setTrackPan` accepts -1000 (left) through 0 (center) to 1000
(right). Stereo tracks use a balance law: center preserves existing unity gains;
panning attenuates the opposite channel without folding stereo to mono or boosting
levels. Mono effect returns pass through separate left/right wet mixers so tails
follow the same pan. Drum/synth buses are unchanged. Updates to both channel gains
are made with audio interrupts disabled.

The display command/state is `~,track,permille`, with bounded track/value handling
and refresh on reconnect. Projects save `PAN0` through `PAN3`; older projects
default to center. Pan changes participate in the existing debounced autosave,
which waits until playback is stopped. New projects reset all pans.

The Teensy PlatformIO build passed, including the concurrent modular-synth changes.
Flash code 221536 bytes; RAM1 variables 148352 bytes; RAM2 variables 172352 bytes.
Existing warnings in modular code, storage, and link formatting remain. Host UI
tests cover encoder bounds/center/track targeting; physical stereo listening and
SD save/reload validation are pending coordinated hardware checks.

### Pan hardware deployment and persistence

On 2026-09-23 both combined firmware images were installed. The Teensy exact-model
loader retry completed; ESP32 programming passed hash verification and startup
reported touch/link OK. Actual UART state showed pan -750,500,0,0 in test project
MOD0923073143 before and after two close/reopen cycles. The recorder was restored
to a live, valid main menu with zero dropped mirror records. Host touch and audio
gain tests passed; physical finger interaction and listening remain unverified.

## Default mixer effect tails

Track faders now control the dry mix and the input sent into each enabled effect.
The wet return stays at its selected wet level and pan. Pulling a fader to zero
stops new input while existing delay/reverb decays naturally; bypass and clip gates
also stop the send without cutting the return. Raising the fader resumes the send.
Wet level and master volume still control audible output. Effects without stored
tails (or a delay with zero feedback) only linger as their own processing permits.
Reassigning a shared effect or changing its chain can still interrupt its tail.

Validation: actual gain-method tests with sanitizer checks cover partial/zero fader,
restored send, bypass, crop, independent tracks, wet-level zero and stereo pan. The
Teensy build passed. This verifies routing; physical listening remains user validation.
