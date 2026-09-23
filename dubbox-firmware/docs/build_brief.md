# Dub-Box — Teensy 4.1 Hardware Build Brief

> **Historical (2026-09-03).** This is the original build plan. The touchscreen/LVGL parts are
> superseded: the UI now runs on a separate ESP32 display (see `display_architecture_brief.md`)
> and the Teensy-side ILI9488/LVGL screen was removed on 2026-09-21. Audio, fx and input parts
> still describe the hardware.

Handoff doc for a Claude Code session. Goal: port the browser-based Dub-Box
simulation (4-track hardware DAW: boot screen -> menu -> 4-track view, with
per-track fx-select button, encoder (rotate = fx intensity, push = fx
division/character), fader, mute button, plus transport controls and a
touchscreen) onto real Teensy 4.1 hardware.

## 1. Confirmed hardware

- **MCU**: Teensy 4.1
- **Audio I/O**: Teensy Audio Shield, **Rev D2** (SGTL5000 codec) —
  officially pin-compatible with the Teensy 4.1 directly, no rewiring or
  trace mods needed. Already **direct-soldered** to the Teensy (all 28
  pins both sides) rather than mounted via stacking headers — this is a
  supported, normal mounting method and doesn't conflict with anything
  else in this design, since the shield's 28-pin footprint only covers
  the Teensy 3.x-sized original header rows; the 4.1's extra pins (up to
  41) and dedicated SDIO lines are physically separate and untouched.

  > **Correction, confirmed by physical inspection 2026-07-20:** this
  > unit is actually mounted on tall stacking headers, not
  > direct-soldered — there's a visible socket/gap between the two
  > boards. Pins under the shield's footprint (including 11/12/13, used
  > for the TFT's SPI bus) ARE reachable via the shield's pass-through
  > sockets. See docs/pin_map.md for the actual wiring in use.

- **SD card**: using the **Teensy 4.1's built-in SD slot** (SDIO), not
  the audio shield's onboard microSD slot. This is the simpler path — no
  pin sharing with the shield's own SD circuitry, and it's what the
  storage/project-loading logic in this brief assumes throughout.
- **Display**: ILI9488 3.5"–4" SPI TFT with resistive or capacitive touch
  controller (XPT2046 for resistive, FT6236 for capacitive — pick based on
  which touch panel ships with your ILI9488 module).
- **Storage**: Teensy 4.1's built-in microSD slot, for track audio + saved
  projects.

## 2. Hardware gaps to flag before firmware work starts

These aren't firmware problems, but they'll block features if not decided:

- **USB connector**: Teensy 4.1 has a native **micro-USB** port, not
  USB-C. If USB-C charging/data is a hard requirement for the enclosure,
  you'll need a USB-C-to-micro-USB adapter board inline, or a separate
  USB-C PD trigger + power path feeding the Teensy's VIN, decoupled from
  data. Worth deciding now since it affects the enclosure's bottom-panel
  cutout.
- **XLR/TRS input**: the Audio Shield's line-in is line level. A
  microphone plugged into XLR is mic level (much lower) and needs a
  preamp (e.g. a small IC like the MAX9814, or a dedicated preamp board)
  before it reaches the codec. A TRS instrument input (guitar-level) also
  needs level-matching, sometimes a DI circuit. This is an analog front-end
  design task, separate from the Teensy code.
- **Speaker output**: the Audio Shield's headphone out isn't enough to
  drive a speaker directly at real volume. You'll want a small Class-D
  amp board (e.g. a PAM8302-based breakout) between the codec and the
  speaker.
- **Motorized vs. passive faders**: the simulation doesn't animate fader
  position from software (e.g. during automation playback). Passive
  linear potentiometers are far simpler and cheaper; motorized faders add
  real cost and firmware complexity (closed-loop position control). Start
  with passive unless automated fader movement is a must-have.

## 3. Pin budget

Rough tally wiring everything directly to the Teensy 4.1 (~55 digital I/O
pins available, 18 analog-capable, 3 SPI buses, 3 I2C buses, 8 UARTs):

| Component | Pins | Notes |
|---|---|---|
| Audio Shield (I2S) | 5 | Fixed pins (BCLK/LRCLK/RX/TX/MCLK), not shareable |
| Audio Shield (codec control) | 2 | I2C — shareable bus |
| SD card | 0 | Built-in SDIO on the 4.1, separate from SPI — free |
| ILI9488 screen (SPI) | ~6 | MOSI/MISO/SCK/CS/DC/RST |
| Touch controller | 0–2 | Capacitive (FT6236) rides the existing I2C bus for free; resistive (XPT2046) shares screen SPI but needs its own CS (+ optional IRQ) |
| ESP32 co-processor | ~3 | Simple UART link: TX/RX + reset |
| 4 faders | 4 | Analog pins |
| 4 encoders (A/B + push) | 12 | 3 pins each |
| 8 track buttons (fx-select + mute) | 8 | |
| Transport + power (play/pause, rewind, forward, power) | 4 | |
| **Total** | **~44–46** | Out of ~55 available |

That fits as a complete, valid direct-wire design on its own — no expander
required to make this work. It leaves roughly 9–11 spare native pins for
small additions (a status LED, one more button) before expansion becomes
necessary at all.

### Future GPIO expansion (don't build this now — just leave the door open)

A **FT232H is not the right chip for on-device GPIO expansion** here: it's
a USB-to-multiprotocol *bridge* meant to give a host computer I2C/SPI/GPIO
access over USB (it's what's inside most "USB to I2C" adapters and FTDI
programming cables). It doesn't expose a bus-facing slave interface the
way an I2C GPIO expander does, so the Teensy would have to act as a USB
host speaking FTDI's MPSSE protocol to use it — not a path the Teensy USB
Host stack cleanly supports. Keep it for bench testing / driving
peripherals from a PC during development, not as an in-device expander.

The actual expansion path, when/if more I/O is ever needed: an I2C GPIO
expander (e.g. MCP23017) taps onto the *same* SDA/SCL lines the audio
codec already uses — I2C is a shared bus, so this requires zero changes to
any pin already wired for the screen, faders, encoders, buttons, or audio.
**The only thing worth doing now**: break out SDA/SCL to an accessible
spare header or test pad on the board (not just buried under the codec
footprint), so future expansion doesn't require reverse-engineering
traces. That's the entire "insurance policy" — no other design changes
needed today.

## 4. Cloud / networking architecture

Two separate cloud features are in scope long-term: **stem separation**
and **project sync / collaboration**. They should be treated as
architecturally distinct, not the same backend.

### Networking hardware

The Teensy 4.1 has no onboard WiFi/Bluetooth radio (it does have a wired
Ethernet PHY on-chip if a wired option is ever wanted, via `QNEthernet` +
an external magnetics jack, but that's not practical for a handheld
device). Wireless needs an **ESP32 module acting as a co-processor**,
talking to the Teensy over a simple UART link (already counted in the pin
budget above). Design rule: the Teensy owns real-time audio and must never
block waiting on the network; the ESP32 handles all network I/O
asynchronously and reports results back over UART when ready.

### Stem separation — must be cloud, not on-device

This is a hard constraint, not a tuning problem. Stem separation runs a
deep neural network (Demucs-class models are 80MB+ of weights alone,
before inference memory). The Teensy 4.1's single 600MHz core and ~16MB
of add-on PSRAM aren't in the same category of hardware as what these
models need — this isn't "underpowered," it's a different class of chip
(no OS, no GPU, built for real-time DSP and control, not ML inference).
Flow: device sends the source audio file up via the ESP32 -> cloud API
does separation -> 4 stem files come back -> written to SD. Needs a
"processing..." UI state since this takes real time (seconds to a couple
minutes depending on backend). Two viable backend options:

1. **Third-party API** (LALAL.AI, Moises, Audioshake, etc.) — fastest to
   integrate, pay-per-use, no infrastructure to run. Good for validating
   the feature before investing further.
2. **Self-hosted Demucs** on rented GPU compute (Replicate, Modal,
   RunPod, or a persistent GPU VM) — more control, cheaper at real scale,
   but you own hosting/uptime/cost management.

### Project sync / collaboration — separate backend

This is a conventional product backend problem (accounts, project
storage, sync), not a compute-heavy one. A Supabase + Vercel style stack
(already in use for Life Console) is a reasonable fit and should be kept
architecturally separate from whatever handles stem-separation compute —
that workload wants its own specialized endpoint rather than living in
the same service as project sync.

## 5. Suggested software stack

- **Framework**: Arduino + Teensyduino, or PlatformIO with the Teensy
  platform (PlatformIO gives better version control / CI if this becomes
  a real product).
- **Audio engine**: Teensy Audio Library. It's a real-time audio graph
  library purpose-built for this chip — this is a genuine advantage over
  the Raspberry Pi/browser approach: DSP runs in hardware-friendly blocks
  with a documented CPU/memory budget, not general-purpose Python or JS.
- **UI / touchscreen**: LVGL (has native Teensy + ILI9488 support) is
  recommended over raw Adafruit_GFX calls once you're doing multiple
  screens, waveform rendering, and drag interactions — it has widgets,
  touch input handling, and an event system that will save a lot of time
  versus hand-rolling hit detection like the browser mockup did.
- **Buttons**: Bounce2 library for clean debouncing.
- **Encoders**: the PJRC Encoder library for quadrature reading, with
  your own short-press/long-press or push-button GPIO handling for the
  click feature (same tap-vs-drag distinction as the simulation, but
  simpler since a real encoder's push button is just a dedicated pin).
- **I/O expansion**: with 4 encoders (3 pins each incl. push), 4 faders
  (1 analog pin each), 8 per-track buttons (fx-select + mute), 3 transport
  buttons, and 1 power button, you're at ~28 signals. The Teensy 4.1 has
  enough pins to do this directly, but an MCP23017 I2C GPIO expander (or
  two) for the buttons specifically is worth considering — it cuts wiring
  complexity a lot and keeps the analog-sensitive pins (faders) and timing
  sensitive pins (encoders) on the Teensy directly.

## 6. Mapping simulation effects to Teensy Audio Library

| Simulation effect | Teensy Audio Library approach |
|---|---|
| Reverb | `AudioEffectFreeverb` — built-in, maps well to "room size" divisions |
| Delay / Echo | `AudioEffectDelay` — built-in, maps directly to your note-division push-cycle |
| Chorus / Flanger | Build from `AudioEffectDelay` + `AudioSynthWaveform` (LFO) modulating delay time via `AudioEffectMultiply`, mixed with `AudioMixer4` — no single built-in block, needs a small custom patch |
| Phaser | No built-in block. Either approximate with cascaded `AudioFilterStateVariable` allpass-style stages modulated by an LFO, or write a custom `AudioStream`-derived class |
| Distortion | No built-in waveshaper block. Write a small custom `AudioStream` class with a lookup-table curve (same shape as the browser version's waveshaper curve) |
| Lofi | Combine `AudioFilterBiquad` (lowpass) with a custom bit-reduction `AudioStream`, or fake it cheaply by running at a lower internal sample rate for that track |

Custom `AudioStream` objects are normal practice in this ecosystem — PJRC's
docs cover how to write one — so distortion/phaser/lofi needing custom code
isn't unusual, just worth budgeting time for.

## 7. Suggested repo structure

```
dubbox-firmware/
  platformio.ini            (or .ino if staying in Arduino IDE)
  src/
    main.cpp
    audio_engine.h / .cpp    audio graph setup, per-track chains, effects
    input_manager.h / .cpp   buttons, encoders, faders -> events
    ui/
      screen_boot.h / .cpp
      screen_menu.h / .cpp
      screen_daw.h / .cpp
      waveform_render.h / .cpp
    storage.h / .cpp         SD card project load/save, track file handling
    project_state.h          shared state: track buffers, fx settings, etc.
  data/                      test WAV files for bench testing
  docs/
    pin_map.md                (fill in once wiring is finalized)
```

## 8. Suggested build order (phased, each phase testable on its own)

1. **Bring-up**: blink + confirm Audio Shield passthrough (mic/line in to
   headphone out) works before anything else.
2. **4-track playback core**: play 4 WAV files from SD simultaneously,
   faders control volume live via `AudioMixer4` gain. No screen yet —
   prove the audio graph and pot reading work.
3. **Fx core**: wire up one encoder + one fx-select button on one track,
   get reverb and delay working end-to-end (button cycles effect,
   rotate = intensity, push = division). Prove the interaction pattern on
   hardware before scaling to 4 tracks.
4. **Screen bring-up**: boot screen -> menu -> blank DAW view on the
   ILI9488, touch input confirmed working.
5. **Waveform + scrub bar**: precompute peak data for loaded WAVs (can be
   done at load time or pre-baked and stored alongside the file on SD),
   render horizontal track rows and a scrub bar, wire touch-to-seek.
6. **Transport + track reorder**: play/pause/restart/forward, and
   drag-to-reorder on the touchscreen (swapping which SD file/settings
   are assigned to which physical lane, same logic as the simulation).
7. **Recording input**: XLR/TRS -> preamp -> Audio Shield line-in -> SD
   write, once the analog front-end hardware is built.
8. **Remaining effects**: distortion, flanger, phaser, chorus, lofi via
   custom `AudioStream` classes.
9. **ESP32 co-processor bring-up**: UART link between Teensy and ESP32
   confirmed, ESP32 can join WiFi and make a basic HTTPS request
   independent of audio playback continuing uninterrupted.
10. **Stem splitter integration**: wire the "stem splitter" UI flow to a
    real third-party API call via the ESP32, with a processing-state
    screen and the 4 returned stems written to SD and loaded onto tracks.
11. **Project sync backend**: separate Supabase-style service for
    account/project storage, wired in once the above is stable.

## 9. Kickoff prompt for Claude Code

Paste this as your first message in a new Claude Code session, in an empty
project folder:

> I'm building firmware for a Teensy 4.1 based hardware 4-track DAW
> called Dub-Box. Hardware: Teensy 4.1, Teensy Audio Shield (SGTL5000),
> ILI9488 SPI TFT touchscreen, 4x rotary encoders with push buttons, 4x
> linear fader potentiometers, 4x fx-select buttons, 4x mute buttons, 3
> transport buttons, 1 power button, SD card storage, and (later) an ESP32
> co-processor over UART for WiFi. All components are wired directly to
> the Teensy's native GPIOs — no I2C GPIO expander is in the current
> design, though SDA/SCL will be broken out to a spare header for future
> expansion. I want to use PlatformIO, the Teensy Audio Library for the
> audio engine, and LVGL for the touchscreen UI. Set up the initial
> project structure per the attached build brief, starting with phase 1:
> a basic audio graph that plays 4 WAV files from SD simultaneously with
> per-track volume control from 4 analog faders. Ask me about pin
> assignments before wiring any specific GPIO numbers into the code.

Attach this brief file to that first message so Claude Code has the full
context on the effects mapping and phased plan.
