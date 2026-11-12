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

### Device color themes

`theme.h/.cpp` supplies Chalk (cream/teal), Graphite (dark/amber), and Petal
(warm blush/plum), with Chalk as the first-run default. Theme selection is saved
in the `studio-ui` Preferences namespace only when it changes; failed writes do
not change the active theme, and invalid stored values fall back to Chalk.
Shared chrome and screen backgrounds, panels, labels and standard actions use
the palette. Existing track/drum identity colors and recording/warning states
remain distinct. Text/button helpers check contrast against the actual RGB565
background, retaining the requested color when readable and otherwise choosing
black or white. Piano keys retain their black/white musical identity.

These are functional colorways on the existing layouts and bitmap fonts, not
implementations of the generated concept layouts or suggested typefaces. The
theme picker rebuilds cached playlist artwork when changing themes. Physical
display navigation, persistence across reboot, and palette rendering still need
verification with the combined picker build; this section does not assert a flash.
Host validation compiled the actual `theme.cpp` with mocked Preferences under
address/undefined-behavior sanitizers: all three selections survive reload,
invalid IDs fall back or are rejected, unchanged selections do not write, and
failed storage open/write leaves the selection unchanged. This tests persistence
logic, not physical flash storage.

Marginal 5 V supply (brownout resets) and the flaky auto-reset are hardware issues, see the firmware HANDOFF.
Next: confirm the staged drawing visually, then fix the display power and reflash from this tree.

## Desktop screen mirror

The display now emits checksummed drawing records over its 460800-baud
USB diagnostics: rectangles, exact bitmap glyphs, layer copies and frame markers.
`screen_mirror.*` uses a bounded queue and permits bounded 20 ms draining when
the queue fills during startup or runtime redraws. This can slow large screen
redraws; audio runs independently on the Teensy. Records still drop if that
drain deadline expires. USB MIDI suppresses mirroring to avoid mixing protocols. The host renderer
must support five layers (including the new staging layer) and the current thirteen
screen names. A sequence gap invalidates the view; a complete startup stream is
required to recover. This is a drawing-command mirror, not physical LCD readback.
It is view-only and still requires the ESP32 to remain powered.

Host validation covered staging-to-visible copies, screen names and missing-record
detection. Built and flashed with uploader hash verification. The computer rendered the live main menu with zero dropped records at 460800 baud; extended navigation testing remains pending.

## Display session recovery

Boot and link reconnection no longer close the Teensy project. An existing open
project restores the playlist; explicitly choosing Menu still closes it. Built
and flashed with hash verification; user reports normal operation but declined
a deliberate display-reset test, so reset recovery remains unverified on hardware.

## Finger-friendly visual redesign

The user specified an active area approximately 5.5 by 2.5 inches. At 1280x720,
83x102 pixel hit areas are approximately 9mm square. Shared navigation is 100px
high and transport actions are 102px high; keyboards use 94px keys. The design
uses rounded controls, regular-weight small text, grouped home actions and a
separate mixer Effects panel. It retains Chalk, Graphite and Petal palettes.
The typography is FreeSans, not Apple's San Francisco. Timeline and piano gestures
need device evaluation; these geometry figures do not constitute accessibility certification.

Independent host checks covered the mixer's 17 action rectangles, utility screen
bounds and paging, saved-theme failure behavior, and source-rendered menu/theme
text layout. Host previews omit the shared header and are not hardware screenshots.
Rounded shapes use one mirror command rather than dozens of transmitted scanlines;
a host check confirms geometry and continued rejection of incomplete streams.
The final combined firmware build passed after integrating playlist footer actions. Flashed successfully with uploader hash verification; both boards reconnected. Hardware touch/visual validation remains pending.

Music editing now pages eight steps and two sounds/notes at a time; dragging a note
to a page edge allows lengths across all sixteen steps. Sound parameters are paged;
keyboard audition/chords/sustain remain on Keys. Playlist Cut/Undo/Record actions
are in the large bottom row. Remaining compact gesture areas include 78px piano-roll
rows, 86px audio lanes/page navigation and some 92–96px secondary controls. These
need actual finger testing, not a claim that every target is 9mm.

During the redesign flash/restart, the device log explicitly reported recovery
of the existing Teensy project after link-up. This verifies the reconnect path
for that restart; continuous audible playback through a brownout remains untested.

Preset usability follow-up: selecting any of the six sound presets now opens
the playable Keys view directly. The selecting finger must lift before notes
can trigger, preventing an accidental held note during the view change. Return
to Sound to edit parameters. Build passed; hands-on preset selection remains
the validation step. The audio master was remotely set to35% and acknowledged.

### Expanded Keys performance view

Keys now uses the full content width (x16–1263) from y258–610, rather than the
previous x240–1247/y358–610 piano. One toolbar retains Sound, Roll, octave,
key-count and momentary sustain controls; return to Sound or Roll for pattern,
instrument, tempo and editing controls. Existing navigation and transport remain
visible. Eight white keys are the default, giving 156 px white keys and 93 px
black keys (about 17 mm and 10 mm wide on the reported 5.5-inch-wide display).
The optional 5–14-key range remains; choosing more keys intentionally produces
smaller targets. Toolbar actions are at least 100×97 px, except read-only labels.
Preset/tab entry waits for the selecting finger to lift before playing notes.
Pattern/BPM/kit updates cannot paint editor controls over the performance view.

Host validation of production `keyboard.cpp` passed ASan/UBSan for all selectable
key counts and octaves, black/white hit mapping, drawing bounds, three-finger
chords, release debounce and release on leaving the view. This does not prove
physical touch accuracy or rendering speed. Combined device build and hands-on
playing remain required; no flash is asserted by these checks.

## Soft screen transitions

`ui_transition.*` uses the LT7683's documented memory-copy opacity operation
(REG91 low nibble 0xA; REGB5 alpha 0–32). Source:
[Levetop LT768x v4.2, pp.64/173/178](https://www.levetop.cn/uploadfiles/2023/05/LT768x_DS_V42_ENG.pdf).
The old visible frame is copied to reserved display-RAM layer 6 at byte9,216,000
before constructing the new screen in staging. Six scheduled blend steps span
approximately120ms; startup, startup project recovery and same-screen redraw do
not fade. Display drawing and BTE operations pause during the transition, while
the Teensy link continues polling without consuming its UI change flags. A held
navigation finger is ignored until release. Busy timeout recovery waits for idle
before attempting the final-frame copy. No extra ESP32 framebuffer is allocated.

The mirror supports six layers and `A,old,new,destination,alpha32` records. Pillow
permits efficient blend previews; without it, the host explicitly reports
final-frame-only transitions and marks omitted intermediate views non-live.
A200ms browser polling interval can skip a120ms transition even with Pillow.
The host retains a CRC-checked last complete PNG across recorder restarts and
stream resets. It does not publish a fresh startup view until a complete
full-screen BTE copy and frame marker arrive; stale pixels remain labeled stale.
This avoids blanking the computer image during reconnect, not physical LCD
power failures.

Validation performed without hardware access: actual extracted goTo/transition
service lifecycle passed ASan/UBSan host checks (startup, snapshot order, frozen
rendering/link polling, completion, failed start and busy recovery). Transition
state-machine tests passed. Actual Python mirror tests passed with and without
Pillow, including blend endpoints, six layers, lost records, retained frames,
incomplete startup suppression and corrupted-PNG rejection. Firmware build,
real panel fade duration/scanout tearing and extended navigation remain pending.

### Draw-only mirror recovery

The host can request an idle-time keyframe through the capability-gated mirror protocol. `ui::mirrorSnapshot(id)` waits until touch edits and transitions finish and the controller is idle. It rebuilds hidden waveform/composition layers, redraws the current screen into staging, and presents once. It does not navigate, close/open projects, consume link change flags, clear control-test results, reset typed names, change effects/selection, or start/stop audio streams. Mixer dialogs, Pattern view/chooser, keyboard text/errors, and utility pages have separate redraw entry points. WiFi/Bluetooth status may update naturally to current status while rendering.

The `S,id,1280,720` to `K,id,screen,drops` envelope is accepted only with contiguous checksum-valid records, matching IDs, no queue overflow, and a complete base-frame copy. The computer retains its previous complete image until this succeeds; partial recovery is never marked live. All six renderer layers are re-established, including playlist wave caches, so subsequent copy/blend commands have valid sources. Receiver restarts and disconnects still show the saved PNG as stale while requesting a fresh keyframe. Recovery is deferred during held touches and cannot overcome an unpowered display, persistently corrupt transport, or an unavailable controller.

Host checks: `work/studio-transition-test/check_snapshot.py` extracts the actual coordinator for a sanitizer harness covering all 13 screen routes, initialization/presentation ordering, and busy/touch gates; `test_mirror.py` checks S/K publication and rejection of mismatched IDs, missing records, overflow, and absent base frames, under both deployed Python (without Pillow) and Pillow-enabled Python. Hardware recovery validation remains separate from these checks.
