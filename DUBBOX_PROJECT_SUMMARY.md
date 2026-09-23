# Dub-Box — Project Summary (context handoff doc)

**Purpose of this file:** paste this into a brand-new Claude Code session (no other
context) so it understands the whole project — what it is, the hardware, both
firmware codebases, the link protocol between them, the UI, build/flash steps, and
what's actually been verified on real hardware vs. just written. Last updated
2026-09-22.

---

## 1. What Dub-Box is

A **Teensy 4.1 based hardware 4-track DAW** ("dub" as in dub-style mixing/effects,
"Box" as in a standalone hardware unit) with a **separate ESP32 touchscreen** for
the UI. Not a simulation or a Raspberry Pi project — this is real embedded firmware
running on real, bench-tested hardware, built incrementally with a human owner who
verifies each feature over serial/USB before moving on.

Two independent PlatformIO projects, linked by a UART cable:

```
DubBoc claude code/
  dubbox-firmware/     Teensy 4.1: audio engine, SD, drum machine/synths, physical controls
  dubbox-display/      ESP32: touchscreen UI, WiFi upload page
  life-tracker/         <- unrelated project, ignore
```

**Owner's working style** (worth knowing before making changes): iterate fast,
flash both boards after every change, verify behavior over USB serial with small
Python test scripts before declaring something done, and be told honestly which
parts are hardware-verified vs. just compiled/untested. Never claim something
works because it built — only because it was seen/heard/tested on the actual
boards.

---

## 2. Hardware inventory

- **Teensy 4.1** — main MCU, real-time audio + SD + physical controls.
- **Teensy Audio Shield Rev D2** (SGTL5000 codec) — mounted on tall stacking
  headers (confirmed by physical inspection, not direct-soldered). Pins under its
  footprint are still reachable via its pass-through sockets.
- **SD card** — the Teensy 4.1's **built-in SDIO slot** (`BUILTIN_SDCARD`), not the
  Audio Shield's own microSD slot. Holds WAV tracks and `/PROJECTS/*.PRJ` files.
- **ESP32 (classic DevKitC/WROOM, not S3)** — drives the touchscreen and runs the
  WiFi upload web server. Talks to the Teensy over UART only; never touches audio
  samples or the SD card directly.
- **Display: ER-TFT050-10-6304** — 5" IPS TFT, 720×1280 portrait panel, **LT7683**
  graphics controller with 16MB onboard display RAM, driven over SPI (up to
  50MHz spec, running at 20MHz — see §7). Touch: **GT911** capacitive, I2C address
  0x5D.
- 4× linear fader potentiometers, 4× rotary encoders (with push buttons), 4×
  fx-select buttons, 4× drum pad buttons (momentary, physical pads).
- **Not yet built**: speaker amplifier, mic preamp, Bluetooth audio output,
  transport/power buttons (need an MCP23017 I2C GPIO expander — direct GPIO is
  fully spent).

### 2.1 Teensy 4.1 pin map (confirmed wired)

| Signal | Pin(s) | Notes |
|---|---|---|
| Fader 1-4 | A10, A11, A12, A13 (pins 24-27) | Top header row, past the Audio Shield's footprint |
| Encoder 1 A/B/push | 33 / 34 / 35 | Bottom solder-pad section near the SD slot — needs soldered wires, not friction-fit |
| Encoder 2 A/B/push | 36 / 37 / 38 | Same bottom solder-pad section |
| Encoder 3 A/B/push | 6 / 9 / 10 | Normal header pins |
| Encoder 4 A/B/push | 22 / 39 / 40 | A on header pin 22; B/push on bottom solder pads |
| Fx-select buttons 1-4 | 2, 3, 4, 5 | `INPUT_PULLUP`, LOW = pressed |
| Drum pad buttons 1-4 | 14, 41, 16, 17 (A0, A17, A2, A3) | Pad 2 is pin 41, a bottom solder pad, not a plain jumper pin |
| ESP32 link: Teensy TX1 → ESP32 RX | Teensy pin 1 → ESP32 GPIO4 | UART, 2,000,000 baud |
| ESP32 link: ESP32 TX → Teensy RX1 | ESP32 GPIO17 → Teensy pin 0 | Common ground required |
| I2C (SDA/SCL) | 18 / 19 | Shared with the SGTL5000's control interface; reserved for a future MCP23017 expander |

Full history/notes: `dubbox-firmware/docs/pin_map.md`. Source of truth for pin
numbers in code: `dubbox-firmware/src/pins.h`.

**Retired:** the Teensy used to drive its own ILI9488 SPI TFT (LVGL UI) directly.
That was fully removed 2026-09-21 in favor of the ESP32 display — pins 11-13 and
28-32 are free again as a result.

### 2.2 ESP32 → display module pin map (CON1, 20-pin FFC)

| Signal | ESP32 GPIO | Notes |
|---|---|---|
| LCM_SCS (SPI CS) | 5 | |
| LCM_SDO / SDI / SCLK (SPI) | 19 (MISO) / 23 (MOSI) / 18 (SCK) | Init at 8MHz, then switch to 20MHz once the chip's PLL is up (`lcmSetSpiHz()` in `LCD.cpp`; 40MHz hangs) |
| LCM_RESET | 16 | |
| BL_Control (backlight) | 3.3V direct | No GPIO/dimming |
| CTP_INT / CTP_RST (touch) | 27 / 26 | GT911, I2C |
| CTP_SDA / CTP_SCL (touch) | 21 / 22 | |
| SSD2828 CS / RST / DIN / SCLK | 25 / 33 / 32 / 13 | **Required** — the vendor demo bit-bangs the SSD2828 MIPI bridge at boot. Remapped off GPIO 0/2/12 (ESP32 boot-strap pins) to avoid blocking boot/flash. |
| Teensy UART link RX/TX | GPIO4 / GPIO17 | See §2.1 |

Full details/datasheet notes: `dubbox-firmware/docs/display_architecture_brief.md`.
That doc is otherwise historical (written before the link protocol/UI existed) —
treat `esp_link.h` as the current source of truth for the protocol, not that doc's
§5-6.

---

## 3. Firmware architecture

### 3.1 Teensy side (`dubbox-firmware/`)

PlatformIO env `teensy41`, Arduino framework, `-std=gnu++17`. Key libraries:
Teensy Audio Library (core), `Encoder`, `Bounce2`. Upload uses
`upload_protocol = teensy-cli` (blocks for a real pass/fail, unlike the default
Windows GUI uploader).

**`main.cpp`** — boot sequence (audio engine → MTP → project manager), the main
loop (input → audio engine update → link update), and the per-track fx encoder
gestures (rotate = wet level, hold+rotate = cycle effect type, tap = cycle the
active effect's primary parameter).

**`audio_engine.h/.cpp`** (`AudioEngine`) — owns the whole audio graph:
- 4× `AudioPlaySdWavSeekable` (a local fork of the stock WAV player with `seek()`
  added — see `audio_play_sd_wav_seekable.*`) → per-track fader gain → dry stereo
  mix (`mixerL_`/`mixerR_`).
- **Clips**: each track holds up to 6 clips, each a `[fileStart, fileEnd)` window
  plus a timeline offset (`timeline time = file time + offset`). One physical
  player per track; at a clip boundary it seeks/parks. This is how "snip a track
  into pieces and move them" works — always non-destructive, the WAV file is never
  touched.
- **Per-track fx chains**: up to 3 slots in series. Slot 0 can be any of Delay /
  Flange / Freeverb / Chorus; extra slots are Flange/Chorus only. Delay and
  Freeverb are **shared/exclusive** (only one track owns the real hardware
  instance at a time — RAM/audio-block budget reasons); Flange/Chorus are
  independent per track. Dub-style mute: bypassing closes a send gate but leaves
  the effect wired, so tails ring out.
- **Transport**: one shared clock across all 4 tracks + the drum machine
  (`DrumMachine::onTimeline()`), play/pause/seek/rewind.
- **Final mix** (`finalMixL_`/`finalMixR_`): input 0 = dry, 1 = wet (fx), 2 =
  drums, 3 = synth instruments → `AudioOutputI2S` → SGTL5000.
- `DrumMachine drums_` is owned here (`engine.drums()` accessor).

**`drum_machine.h/.cpp`** (`DrumMachine`) — step sequencer + up to 4 synth
instruments, entirely synthesized (no SD access, so it never competes with track
streaming):
- 8 patterns × 8 drum rows (KICK/SNARE/CLAP/CL-HAT/OP-HAT/TOM/RIM/PERC) × 16 steps.
  Drum voices: `AudioSynthSimpleDrum` (kick/snare-tone/tom/rim/perc) + shaped
  `AudioSynthNoiseWhite` through state-variable filters for snare/clap/hats.
- Up to 4 **instruments**, each a `Synth` (see below) with its own patch and its
  own notes per pattern (≤24 notes/pattern/instrument, no same-pitch overlap).
  Instrument dry outputs sum in `instMix_`; each instrument's CHORUS/REVERB
  sliders are **send levels** into one shared chorus + one shared reverb
  (`chorusBus_`/`reverbBus_` → `chorus_`/`reverb_` → `busOut_`) — this was a CPU
  optimization (per-instrument effects were too expensive; see §8).
- **Pattern clips**: place a pattern on the song timeline (`startBar`, `lenBars`),
  up to 16, locked to the transport clock.
- **Independent loop preview**: a pattern can loop on its own clock
  (`startPreview`), separate from the song transport, with recording
  (`setRecording`) — a one-bar count-in, metronome click, pad presses / notes
  snapped to the nearest 16th step, overdub-only (never erases).
- **Drum pad routing**: the 4 physical pads play either drum sounds or piano notes
  of the viewed instrument (`padMode`). Routing works by "hold the sound's label on
  the display, then press the pad" — the display reports a "hold" over the link,
  the Teensy remembers it for ~1.2s waiting for a pad press.
- **Live notes**: pads in note mode and on-screen keyboards share one `LiveNote`
  pool (8 slots), with a sustain pedal, "held forever" voice flag, and a
  display-refresh/expiry scheme (600ms) so a lost UART message can never leave a
  note stuck.

**`synth.h/.cpp`** (`Synth`) — one polyphonic synth instrument, 6 voices, 2
oscillators each (sine/triangle/saw/square), shared low-pass filter, ADSR
envelope. `SynthPatch` = 14 int params: `wave1, wave2, oct2 (-2..2), detune
(-50..50 cents), mix (0=osc1..100=osc2), attack, decay, sustain, release, cutoff,
resonance, chorus, reverb, level` (0-100 except as noted; attack/decay/release are
quadratic-scaled up to 1.5s/2s/3s; cutoff is exponential 60Hz-12kHz). `setEnabled
(bool)` silences and unwires an unused instrument's filter stage to save CPU.

**`project.h/.cpp`** (`ProjectManager`) — saved projects as plain text at
`/PROJECTS/<NAME>.PRJ`. Lines: `BEAT=`, `BPM=`, `PADS=`, `INST=n`, `SYNTH<i>=` (14
ints per instrument), `PADNOTES=`, `P<pattern>=` (8 hex4 row masks), `NT<p><i>=`
(notes as `midi:start:len,...`), `PC=` (pattern clips), `T<i>=`/`C<i>=` (track
files/clips). Old single-synth project files (`SYNTH=`, `PN<p>=`) load
automatically into instrument 1 (`oldFormat` migration path).

**`esp_link.h/.cpp`** (`EspLink`) — the UART protocol to the ESP32. See §4.

**`waveform.h/.cpp`, `storage.h/.cpp`, `input_manager.h/.cpp`, `pins.h`,
`project_state.h`** — background waveform/beat-grid scanning (only while paused,
never competing with playback), SD file listing, physical control reading
(faders/encoders/buttons via `Bounce2`/`Encoder`), pin assignments, and shared
constants (`kNumTracks = 4`).

### 3.2 ESP32 side (`dubbox-display/`)

PlatformIO env `esp32dev`. Key libraries: Adafruit GFX (fonts only — drawing is
via the LT7683's own hardware commands, not a framebuffer), plus Arduino-ESP32
core (`WiFi`, `WebServer`, `Preferences`, `ESPmDNS`).

**Rendering approach** (important, learned the hard way — see
`docs/display_architecture_brief.md` §3): the LT7683 is bandwidth-limited over
SPI (~2.5MB/s at 40MHz, and a full-frame push would take nearly a second), so the
UI is built entirely on the chip's **hardware draw commands** (fills, rects, text,
BTE block copies) with careful partial/in-place redraws, never a pixel-pushed
framebuffer or LVGL. Redraw only what changed; avoid drawing text inside
interactive (touch-driven) redraws where possible — both are established
performance/flicker lessons from this project.

**`LCD.h/.cpp`, `SSD2828.h`** — vendor-derived low-level panel driver, pruned to
only the methods the UI actually calls.

**`teensy_link.h/.cpp`** (`teensylink` namespace) — the ESP32 side of the UART
protocol: parses incoming lines into a `State` struct (tracks, fx, drum machine,
instruments/synth patches, project info, file lists, waveforms), exposes
"changed" flags the UI polls, and provides `send*()` functions for every outbound
command. Local-edit values are applied immediately (so touch feels instant) with
a short hold-off before the Teensy's own echoed report can overwrite them.

**`ui.cpp` / `ui.h` / `ui_common.h/.cpp`** — screen state machine and shared
drawing primitives (`Screen` enum, `Rect`, `drawText`, `drawButton`, `fillRect`,
the coordinate mapping — see §5), the header bar (title, link/volume, 3 colored
tabs + MENU), and the transport bar (shared PLAY/REWIND, or split into
PATTERN/REC/CLICK/pad-mode/PLAY SONG/REWIND on the Pattern screen).

**Screens** (`screen_*.h/.cpp`) — see §5 for the full UI tour.

**`keyboard.h/.cpp`** — shared on-screen piano keyboard widget (used inside the
Pattern screen's KEYS view for any instrument): per-key redraw (not whole-keyboard
repaint — fixes a flicker bug), 70ms release debounce + 150ms keepalive to survive
capacitive-touch finger dropouts, adjustable key count.

**`synth_panel.h/.cpp`** — the SOUND view inside the Pattern screen: 6 presets,
oscillator/wave buttons, 12 parameter sliders, a small keyboard.

**`wifi_upload.h/.cpp`, `upload_page.h`, `screen_wifi.h/.cpp`** — WiFi audio
upload feature. See §6.

### 3.3 Not yet built / open items

- Bluetooth audio output — see §9 for the current hardware plan (agreed, not yet
  wired: a dedicated BT transmitter module off the audio shield's `LINE_OUT`).
- Internal powered speaker — planned as an I2S class-D amp (e.g. MAX98357A) fed
  directly off the Teensy's I2S bus, per `docs/power_system_schematic_brief.md`
  §4.3. Not built.
- Mic input (XLR + 48V phantom + preamp) — designed on paper only, see
  `docs/power_system_schematic_brief.md` §4.4.
- Transport + power buttons — need an MCP23017 I2C expander (direct GPIO is
  exhausted).
- Battery power system — see `docs/power_system_schematic_brief.md` (buck-boost
  for the Teensy/codec, separate regulator for the ESP32).
- Saving master volume across restarts; MP3 support (descoped).

---

## 4. UART link protocol (Teensy ↔ ESP32)

Physical: Teensy `Serial1` (pin 0 RX, pin 1 TX) ↔ ESP32 `Serial2` (GPIO4 RX,
GPIO17 TX), **2,000,000 baud** (raised from 921,600 to support upload throughput),
common ground. Line-based ASCII protocol; **the authoritative reference is the
comment block at the top of `dubbox-firmware/src/esp_link.h`** — read that file
directly for the exact current wire format. Summary of message families:

**Teensy → ESP32** (state, sent on change or every ~2s):
- `S,<playing>,<posMs>,<extentMs>` — transport, ~20Hz
- `C,<track>,<fileLenMs>,<n clips>,...` — clip windows/offsets
- `M,<faders>,<peaks>` — mixer levels, ~15Hz
- `F,<track>,<fx chain state>` — per-track fx
- `W,<track>,<waveform chunk>` — peak envelope, chunked
- `B,<track>,<beat grid>` — bpm/downbeat/bar length
- `U,<volume>`, `J,<project open/name>`, `L,<project list>`
- `p,<pattern>,<8 row masks>` — drum pattern
- `y,<pattern clips>` — clips placed on the timeline
- `r,<bpm>,<preview>,<pattern>,<step>,<record/count-in>,<click>` — drum tempo/loop state
- `w,<instrument count>`, `g,<inst>,<14 synth params>`, `q,<pattern>,<inst>,<notes>`
- `u,<pad routes>,<padMode>,<pad notes>`
- `Q,ok|ack|done|err,...` — WiFi upload replies (see §6)

**ESP32 → Teensy** (commands):
- `P` play/pause, `Z` rewind, `B,<ms>` seek, `U,<vol>`
- `K/E/M/L` clip crop/split/merge/move
- `s,<pattern>,<row>,<step>,<0/1>` drum step edit, `w,<pattern>` clear,
  `j,<bpm>`, `f,<on>,<pattern>[,<record>]` loop/record
- `a/d/m/l,<...>` pattern-clip add/delete/move/resize
- `i,<inst>,<0/1>` add/remove instrument, `n/x/c,<pattern>,<inst>,...` note
  add/remove/clear, `t,<inst>,<midi>` audition
- `g,<inst>,<14 settings>` set synth patch
- `e,<midi>` / `h,<row>` — "finger is holding this key/drum label" (routing)
- `o,<inst>,<midi>,<0/1/2>` on-screen note off/on/still-held, `z,<0/1>` sustain
- `k,<pad>[,<0/1>]` simulate a physical pad (handy for scripted tests)
- `v,<mode>[,<inst>]` pad mode, `b,<0/1>` click on/off
- `G/N/O/Q/D/T/R` project list/create/open/close, file list, track assign/clear
- `S,<size>,<name>` / `W,<seq>,<len>,<crc16>`+raw bytes / `C` — WiFi upload relay
  (see §6)

**Debug hook**: typing a raw protocol line into the ESP32's USB serial monitor
forwards it straight to the Teensy (`main.cpp`'s `forwardDebugLines()`) — this is
how most of the project's serial-script testing works (e.g. `O,DEMO`, `P`). Also:
`!up,<seconds>` on the ESP32's USB serial triggers a synthetic WiFi-upload test
(generates and uploads a tone WAV) without needing a phone/browser — see
`wifiup::debugUpload()`.

---

## 5. UI tour (ESP32 touchscreen)

Landscape 1280×720 logical coordinates over the physical 720×1280 portrait panel;
mapping is `(px, py) = (ly, 1279 - lx)`, confirmed on hardware with the ribbon on
the right (`ui_common.h`).

**Header** (all screens except Menu/name-entry): dark bar, project title, link
status tag, master volume slider, and on project screens 3 colored tabs — **MIXER
/ PLAYLIST / PATTERN** — plus a red **MENU** button.

**Menu** → New Project (on-screen keyboard for the name) / Load Project (list of
saved projects) / **WIFI UPLOAD**.

**Mixer** — 4 channel strips: fader, live peak meter, fx chain summary, wet level.
Tap a track's "+" to open the file picker.

**Playlist** — the main timeline: 4 track lanes with real waveforms, clips with
crop handles, a snip tool, beat markers, clips draggable along the timeline, plus
a pattern lane (place/move/resize/delete pattern clips from the drum machine).
Full undo history (snip, move, pattern add/remove/move/resize).

**Pattern** — the drum machine + instruments screen, most complex screen in the
UI:
- Chip row P1-P8 picks which pattern is being edited.
- Tabs: **DRUMS** | **SYN 1** ... **SYN 4** (up to 4 instruments, "+" to add,
  hidden once at 4).
- DRUMS tab: 8×16 step grid, pad-routing labels (hold a row label, press a
  physical pad to route it).
- Each SYN tab has 3 sub-views: **SOUND** (presets/oscillators/ADSR/filter/fx
  sliders + a small keyboard, `synth_panel.*`), **ROLL** (piano roll for that
  instrument's notes in the current pattern), **KEYS** (full-width on-screen
  piano, adjustable key count, sustain pedal in the left strip).
- Transport row here is split: PATTERN (loop this pattern alone) | REC
  (count-in + record) | CLICK toggle | pad-mode toggle | PLAY SONG | REWIND.
- DELETE (instrument) requires two taps within 2.5s ("SURE?" confirm).
- Physical pads and on-screen keys always act on whichever instrument/view is
  currently shown (`setPadMode` follows the view automatically).

**File picker** — choose a WAV from the SD card for a track (or remove it);
refuses to list files while a track is playing.

**WIFI UPLOAD** (`screen_wifi.*`) — connection status + the upload address,
CHOOSE NETWORK (scan list or manual SSID entry) → full on-screen keyboard
(shift/symbols) for the password, FORGET NETWORK, and live upload progress with a
bar. Credentials persist in ESP32 NVS (`Preferences`) after first successful
connect.

---

## 6. WiFi audio upload

Lets you add audio to the Dub-Box from any phone/computer on the same WiFi,
without touching the SD card physically.

- **ESP32 side**: joins the saved WiFi network (`wifi_upload.cpp`), runs an
  Arduino `WebServer` on port 80 in its own FreeRTOS task (core 0), advertises
  `http://dubbox.local` via mDNS. Root page (`upload_page.h`, inlined HTML/JS) lets
  you pick/drop audio files; the **browser** converts them to 44.1kHz 16-bit
  stereo WAV via the WebAudio API and POSTs multipart to `/upload?name=&size=`. A
  24KB ring buffer hands bytes from the web-server task to the main loop, which
  relays them to the Teensy over the UART link (`S,size,name` then
  `W,seq,len,crc16` + raw bytes, windowed 12 chunks of 1KB in flight).
- **Teensy side** (`esp_link.cpp`): writes the incoming bytes straight to the SD
  card root as `<name>.wav` (auto-renaming `-2`, `-3`... on collision), CRC-checks
  every chunk, refuses while a track is playing, times out and deletes partial
  files after 6s of silence. Replies `Q,ok|ack|done|err,...`.
- Uploaded files show up in the normal file picker like any other SD WAV.
- Measured throughput on hardware: ~190KB/s (a 4-minute song ≈ 3.5 minutes to
  upload) at 2Mbaud.
- Test hook: `!up,<seconds>` on the ESP32's USB serial uploads a generated tone
  without needing WiFi/a browser at all.

---

## 7. Build / flash instructions and known gotchas

Both are standard PlatformIO projects (`pio run`, `pio run -t upload`).

- **Windows-specific**: run `PIP_USER=0` (or `$env:PIP_USER=0` in PowerShell)
  before `pio` commands on this machine — pip user-mode installs otherwise break
  PlatformIO's own tool installs.
- **Teensy upload** commonly **fails on the first try** ("error writing to
  Teensy") and succeeds on retry; occasionally needs the physical PROGRAM button
  pressed. Always retry 2-3× before treating a failure as real.
- **ESP32 upload**: `dubbox-display/platformio.ini` pins `upload_port = COM6`
  because the Teensy also enumerates as a serial port (COM4) and PlatformIO may
  auto-pick the wrong one, producing a "Write timeout" from esptool. Also flakes
  with "Wrong boot mode" / "No serial data received" — retry up to ~3-4×.
  Windows driver note: the Teensy needed **PJRC's own serial driver** installed
  for COM3/upload to work at all (unrelated one-time OS fix, not a firmware
  issue).
- **Windows serial ports drop briefly after a flash** — a script opening the port
  immediately after upload may need a short retry/sleep loop before the OS
  re-enumerates it.
- **Never** run a background SD scan (waveform/beat) while a track is streaming —
  it can wedge the SD bus; the firmware already guards this (`isPlaying()` checks
  before scans/MTP/uploads).
- python-editing gotcha hit repeatedly in this project's own history: heredoc/
  triple-quoted string editing tools can turn a literal `\n` (meant for a C
  `printf` format string) into a real newline, breaking the build with "missing
  terminating \" character" — write printf format strings via a direct file edit,
  not through a shell heredoc, or double-check the diff.
- After a big multi-function patch/splice, `grep -c` each function name to
  confirm nothing was duplicated or deleted — this has bitten the project before
  (duplicated `drum_machine.cpp` functions, or the reverse — orphaned functions).

---

## 8. Status: what's verified on real hardware vs. not

**Confirmed working on real hardware** (bench-tested, seen and/or heard): 4-track
WAV playback with a shared transport clock, per-track fx chains with dub-style
mute tails, snip/clip moves + undo, master volume, waveform peaks + beat markers
on the display, drum engine hits + pattern persistence, piano/instrument note
persistence and playback, drum-pad routing (incl. persistence and hold expiry),
count-in/record/click/quantization, stuck-note protections, sustain pedal, synth
patch persistence, multi-instrument add/remove/migration from old project files,
WiFi-upload relay + CRC + SD write + playback of an uploaded file (tested via the
`!up` synthetic-tone hook, not yet via a real phone/browser), CPU headroom
(~4% idle with 0 instruments, ~7-9% with 4 instruments loaded, ~9% with chords
held — after an optimization pass that shared one chorus/reverb across all
instruments instead of one pair each, down from 11-16%).

**Not yet verified by eye/ear/touch** (compiles and flashes, but untested by a
human): the WiFi connection UI screens themselves (scan list, on-screen keyboard,
password entry) and the actual browser upload page end-to-end over real WiFi;
synth preset sound quality (values were guessed, never heard); some Pattern-screen
sub-view layouts.

---

## 9. Planned but not built: audio output expansion

Agreed plan (2026-09-21/22), not yet wired — included here so a fresh session
doesn't have to re-derive it:

- Two new TRS jacks off the Audio Shield's `LINE_OUT` solder pads (wired in
  parallel — both carry the identical full stereo mix): one feeds **powered
  external speakers**, one feeds a **Bluetooth audio transmitter module**. The
  existing onboard headphone jack (`HP_OUT`) stays as a separate, volume-
  controlled wired-monitor output.
- Recommended Bluetooth part: **TinySine TSA5000** (analog 3.5mm line-in,
  aptX/aptX-LL/SBC, ~$17) — chosen over an ESP32-based DIY approach because
  Bluetooth Classic and WiFi share the ESP32's one radio (real coexistence cost
  against the WiFi-upload feature), there's no existing digital audio path from
  the Teensy to the ESP32, and a plug-in module is zero firmware risk to what
  already works. (A digital alternative, **TSA5001** with an I2S input, was also
  scoped for later — it could tap the same I2S bus as a future internal speaker
  amp — but its 48kHz-only spec sheet is not confirmed compatible with the
  Teensy Audio Library's native ~44.1kHz rate; would need checking before use.)
- This is pure analog/RF wiring — no firmware changes needed for the transmitter
  module option.

---

## 10. Key file index

| Topic | File |
|---|---|
| Teensy pin assignments | `dubbox-firmware/src/pins.h`, `docs/pin_map.md` |
| Audio graph | `dubbox-firmware/src/audio_engine.h/.cpp` |
| Drum machine + instruments | `dubbox-firmware/src/drum_machine.h/.cpp` |
| Synth voice | `dubbox-firmware/src/synth.h/.cpp` |
| Project file format | `dubbox-firmware/src/project.h/.cpp` |
| UART protocol (Teensy side, authoritative) | `dubbox-firmware/src/esp_link.h/.cpp` |
| UART protocol (ESP32 side) | `dubbox-display/src/teensy_link.h/.cpp` |
| Display driver | `dubbox-display/src/LCD.*`, `SSD2828.h` |
| UI screen state machine | `dubbox-display/src/ui.cpp`, `ui_common.h/.cpp` |
| Pattern/instruments screen | `dubbox-display/src/screen_pattern.h/.cpp`, `synth_panel.*`, `keyboard.*` |
| WiFi upload | `dubbox-display/src/wifi_upload.*`, `upload_page.h`, `screen_wifi.*` |
| Build briefs / hardware planning | `dubbox-firmware/docs/*.md` |
| PlatformIO configs | `dubbox-firmware/platformio.ini`, `dubbox-display/platformio.ini` |
