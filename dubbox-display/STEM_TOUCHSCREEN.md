# Stem Splitter touchscreen feature

## Access

Open **MENU → STEM SPLITTER** on the physical Dub-Box. This is a native ESP32 screen,
not an embedded browser. The companion computer performs the AI inference.

- YOUTUBE opens the existing on-screen keyboard for title/artist search. Select a result.
- REFRESH lists staged computer sources and recent processing jobs. Previous/Next page
  through six rows at a time. Select a source to split, or a recent job to inspect/deliver.
- Choose SIMPLE / 4, EXTENDED or DYNAMIC, and original WAV or compressed/lossless transfer.
- Confirm permission, then START SPLIT. Watch progress or CANCEL SPLIT. Leaving the screen
  does not cancel a job; find it again in the library.
- When ready, SAVE TO DUB-BOX sends the canonical four-track mixdown. Confirmed SD filenames
  appear on screen. Use the existing file picker to assign them to tracks.

Extended/dynamic separation uses the service's automatic supported six-source profile;
source count/fallback are reported. The hardware still plays four tracks. Deeper source
WAVs stay on the companion for download. Source selection currently supports YouTube
and staged computer files, not reading an existing SD WAV back into the separator.
This firmware has no SD audio-readback transfer interface; that would be separate work.

## Pair once

Start the companion with its existing --device origin and --host 0.0.0.0 on the trusted
local network. On the device's Wi-Fi upload page, enter the companion computer's LAN
origin (http://IP:8766), its existing pairing key, and Connect engine. Then select
“Enable touchscreen controls on this Dub-Box”. Loopback addresses are rejected because
127.0.0.1 on the device is not the computer. The key/origin are saved in ESP32 NVS and
used only for authenticated companion API requests; there is no secret-readback endpoint.
This is the existing trusted HTTP LAN architecture, not an encrypted cloud pairing system.
The companion must stay running/awake and the devices must be on the same reachable LAN.

To stage a file, use “Stage for Dub-Box touchscreen” in the companion, or the upload
page's “Stage source for touchscreen” mode. Staging does not run inference. A source
is limited to 100 MiB; the existing decoder enforces the 15-minute processing limit.
Up to 32 staged sources are retained for the service's default 24-hour period, with
cleanup on subsequent shelf access. Model jobs keep the existing one-job concurrency,
timeout, cancellation and output verification behavior.

## Implementation

- `screen_stems.*`: native selection, choices, job/progress/error/delivery views.
- `stem_client.*`: bounded asynchronous HTTP worker, saved pairing, compact responses.
- `screen_menu.cpp`, `ui.cpp`, `ui_common.h`: physical menu/navigation integration.
- `screen_keyboard.*`: existing keyboard reused for search, with original project defaults.
- `wifi_upload.cpp`: same-origin JSON pairing endpoint and read-only `/stem/status`.
- `upload_page.h`: one-time pairing and file staging controls; prior uploads preserved.
- `services/stem-engine/studio_stem_engine/panel.py`: authenticated compact panel views,
  source staging, existing job and delivery operations. No new separator implementation.
- `device.py`: original upload default, optional bounded compressed-block transport for
  touchscreen delivery with explicit legacy receiver rejection and expansion fallback.

The UI never performs network I/O on its drawing/touch loop. HTTP runs in a separate
bounded task; replies are limited to 8192 bytes and parsed with ArduinoJson 6.21.5.
API redirects are not followed with the bearer key. Unknown/truncated replies and
connection/pairing failures become readable errors. `/stem/status` reports configured,
busy and whether an API call succeeded within 30 seconds, without keys or audio data.
The ordinary `/status` and `/upload` paths continue to work.

Adding the HTTP client exceeded the constrained static ESP32 DRAM section. The existing
full 64 KiB screen-mirror queue now allocates from heap, before Wi-Fi startup; its capacity
was not reduced. Allocation failure is explicitly logged. Check actual runtime free,
minimum and largest heap block using the combined firmware's read-only `!state` command.

## Validation

79 companion/engine tests pass, including staging/auth/expiry/paging, source identity
replay conflicts, rights enforcement, cancellation, compact views and compressed
uploader byte reconstruction/capability gating. Existing browser upload tests pass.
The actual native UI code passed host tests for menu hit target, rights gate, mode and
transfer selection, job view, delivery and cancellation. Four source-rendered previews
were visually inspected; label overflow and selection visibility were fixed. These are
software renders, not LCD photographs. The original sixteen theme/screen renders pass.

A live LAN companion check staged generated audio and ran extended Demucs to six
sources without fallback. A live YouTube metadata search returned three results; no
YouTube audio was downloaded in this check. Firmware deployment and device pairing
results are recorded in the current task's touchscreen validation report.
