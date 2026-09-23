# Dub-Box display firmware

ESP32 touchscreen interface for the Dub-Box Teensy audio workstation. It drives
the LT7683 display and GT911 touch panel, exchanges commands with the Teensy over
UART, and hosts the WiFi audio upload interface.

## Current implementation

The main menu has NEW PROJECT, LOAD PROJECT, STEM SPLITTER, WIFI UPLOAD and SETTINGS rows.
SETTINGS opens the four-skin appearance selector: Classic, Modern, Dark and Light.
Modern is the default charcoal/amber skin. The selection applies to all shared
screen drawing and is saved in ESP32 NVS (`studio-ui/skin`). The shared link status and volume controls
remain available in the header.

## Important files

- `src/screen_menu.cpp`: main menu drawing and touch targets.
- `src/ui.cpp`: page navigation, touch dispatch and UI updates.
- `src/ui_common.h` / `src/ui_common.cpp`: screen identifiers and shared drawing.
- `platformio.ini`: ESP32 build configuration and library dependency.
- [Project overview](../DUBBOX_PROJECT_SUMMARY.md): hardware and protocol context.

## Build and use

Install PlatformIO, then run `pio run -d dubbox-display` from the repository root.
The configured board is `esp32dev`. Pass the verified ESP32 port explicitly when uploading; see [new-computer setup](../docs/NEW_COMPUTER.md).

## Validation and limitations

The settings change was checked with the host C++ compiler using minimal Arduino
and font declarations for syntax checking. This does not replace an ESP32 build.
The menu row fits within the 1280 by 720 logical display, beneath WIFI UPLOAD,
with the existing 40-pixel row spacing. Navigation and touch dispatch were
reviewed against the current source.

The full PlatformIO ESP32 build passed on 2026-09-22: 58,116 bytes RAM (17.7%)
and 952,621 bytes program flash (72.7%). PlatformIO 6.2.0 and Espressif32 7.1.3
were installed in this task's work directory. The existing SSD2828 driver emitted
two integer-to-byte conversion warnings; the build completed successfully.

The ESP32 was flashed successfully through `/dev/cu.usbserial-0001` on
2026-09-22 after elevated port access was approved. The uploader verified the
written data hashes and reset the board. Serial startup confirmed the Dub-Box UI,
LT7683 initialization, `touch init: OK`, and `link UP`. At 10 seconds uptime the
firmware reported 98 Teensy state lines over five seconds, with a 52 ms maximum
gap. One I2C read error (263) appeared during the initial observation; physical
touchscreen navigation still needs visual/touch verification. The Teensy firmware
was not flashed. SETTINGS should open an empty page and MENU should return to
the main menu.

## Live screen mirror

`screen_mirror.h/.cpp` streams the actual shared UI drawing primitives to USB
serial at 921600 baud. Font glyphs are sent compactly and rebuilt with the same
bitmap fonts on the host. Four screen layers and BTE copies are mirrored. A 64 KiB
bounded queue and UART availability checks avoid waiting for the host. Each record
has a sequence number and checksum; queue overflow invalidates the host image.
Touch down/up and sampled move events are included. This is diagnostic observation,
not remote control and not physical LCD pixel readback.

See [hardware monitor](../hardware-monitor/README.md) for the viewer, recovery and
coverage limits. The streaming build passed (123,684 bytes RAM, 953,897 bytes flash)
and was flashed and verified on 2026-09-22. A complete, zero-drop live main-menu
image was inspected on the computer. Other UI screens and sustained interaction
still need hardware exercise. Existing SSD2828 conversion warnings remain.


## Winamp-inspired Studio skins

Implemented in the existing C++ immediate-mode interface. `src/ui_theme.*` owns
four palettes and persistence; `src/screen_settings.*` owns the touch selector;
`src/ui_common.*` supplies reusable bevels, readable buttons, STUDIO chrome and
amber elapsed-time readout. The compatibility palette resolves legacy screen
colors before waveform intensity scaling. Theme changes rebuild cached playlist
layers. Piano keys retain physical black/white colors; track identities and
recording/warning colors remain distinct. Light darkens track colors for contrast.

Mixer peak bars use real Teensy level data and segmented styling, with WAV duration
metadata; existing FX, level controls, project library, playlist editing, pattern
and synth controls remain in place. There is no FFT protocol or graphic EQ in the
current app: this change does not fabricate a spectrum or add nonfunctional EQ
controls. Stem separation stays in `services/stem-engine`; WAV upload, canonical
stem order and Teensy protocol are untouched.

The referenced ChatGPT conversation provided no recoverable generated image.
This implementation follows the supplied written visual description; exact image
matching remains unverified. The active source checkout is the existing
`outputs/studio` directory, which has no `.git` metadata. No new app was scaffolded.

Validation: full ESP32 PlatformIO build passed; existing two SSD2828 integer
conversion warnings remain. `make test` passed 5,153 checks in 11 firmware groups
and eight Python importer tests. [Host display checks](tests/README.md) exercise
skin persistence/error paths and render actual UI code with sample data. Four-skin
mixer and settings renders were visually inspected; overlapping legacy meter
labels and Light track contrast were fixed. No firmware was flashed for this
change. Physical touch, NVS across power cycles, sustained redraw timing and other
screens on the LCD remain follow-up hardware checks.

Final build: 123,700 bytes RAM (37.8%), 956,449 bytes flash (73.0%). The
existing stem-engine suite also passed all 41 tests after integration.

## Skin firmware deployment

On 2026-09-22 the user authorized routine flashing after each validated firmware
update. The skin build (956,453 bytes program flash) was uploaded to the verified
ESP32 `/dev/cu.usbserial-0001`; every programmed region passed uploader hash
verification. The hardware recorder was stopped before upload and restarted.
Startup reported touch initialization OK and Teensy link UP. The drawing-command
mirror showed the new STUDIO main menu, valid/live with zero dropped records.
The first boot reported a missing NVS namespace, which falls back to Modern as
designed; selecting a skin creates the preference. Physical panel verification
and skin persistence across power cycles are still pending. Teensy was not flashed.

## Wi-Fi upload and stem controls (2026-09-23)

The existing upload page now pairs with the existing stem engine for four/extended/
dynamic splits. It preserves compatible original WAVs and offers explicitly labeled
lossless compressed transfer with on-ESP32 decompression. Hardware remains four-track;
deep sources are downloads and the device receives the four-track mixdown. Size savings
are shown per file; serial/SD traffic is unchanged, so total speedup is not guaranteed.
See [usage, protocol, measured tradeoffs and validation](WIFI_UPLOAD.md).
Important files: `src/upload_page.h`, `src/wifi_upload.cpp`, `src/transfer_blocks.h`.

The final update was flashed to the verified ESP32 on 2026-09-23. Touch init and
Teensy link startup passed; the recorder resumed with a live valid mirror. Both physical
transfer modes and a real six-source engine job → four compressed SD transfers passed;
corrupt compressed data was rejected. Six named test WAVs remain for inspection; no
playback or project assignment was performed. See the linked detailed validation.

## Modular instrument editor

PATTERN → + now offers SYNTH or MODULAR. Modular slots have PATCH / ROLL / KEYS;
PATCH shows a nine-module rack with virtual cables, typed sockets, parameter
editing, audition and starter-patch restore. Existing synth, drums, skins and
sequencing remain available. Source: `src/modular_panel.*`, `src/screen_pattern.cpp`
and `src/teensy_link.*`. See [usage, protocol, validation and limits](../packages/modular/README.md).
The USB debug line buffer also accommodates complete modular patch transactions.

## Touchscreen track pan (2026-09-23)

Mixer now has a virtual pan encoder beneath each track's level/peak bars. Drag
left/right (100 pixels from center to either extreme); the center has a small
detent. Tap CENTER to reset. Values show PAN C, L %, or R %. Touch messages are
rate-limited to 20 Hz, with a final value sent on release; Teensy reports the
authoritative value over `~,track,permille` (-1000..1000). Physical encoders retain
their existing effects behavior. The four skins use the existing palette.

Actual-source host rendering and touch tests passed: left/right clamp, center reset,
and independent track selection. Sixteen skin/screen previews were generated and
the Modern mixer was visually inspected. Combined firmware deployment is being
coordinated with the simultaneous modular-synth task; hardware touch/listening
checks remain separate from these host tests.

## Native STEM SPLITTER screen

The physical main menu now includes STEM SPLITTER. Search YouTube, choose staged
computer files/recent jobs, select four/extended/dynamic splitting, watch/cancel jobs,
and save the four-track mixdown using original or lossless compressed transfer.
AI runs on the paired LAN companion. See [setup, scope and validation](STEM_TOUCHSCREEN.md).
The full 64 KiB mirror buffer is preserved but heap-allocated to fit the combined
firmware's static DRAM limit. The feature does not add SD source readback or 16-track playback.

### Pan hardware deployment and persistence

On 2026-09-23 both combined firmware images were installed. The Teensy exact-model
loader retry completed; ESP32 programming passed hash verification and startup
reported touch/link OK. Actual UART state showed pan -750,500,0,0 in test project
MOD0923073143 before and after two close/reopen cycles. The recorder was restored
to a live, valid main menu with zero dropped mirror records. Host touch and audio
gain tests passed; physical finger interaction and listening remain unverified.
