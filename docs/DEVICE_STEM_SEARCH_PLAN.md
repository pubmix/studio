# Device-controlled search and stem preparation

Status: Mac companion/API implemented; touchscreen workflow remains a plan.
See [local server](../services/stem-engine/docs/LOCAL_SERVER.md). The user selected
Mac hosting first, with cloud migration later. Browser search/upload/split/download
is available now; do not describe it as an installed touchscreen menu.

## Intended user flow

MENU > SPLIT SONG > enter a search query or supported video link > select a result >
confirm download/separation rights and creation of a new project > see job progress >
open the new four-track project. No terminal or browser should be needed for each song.

## Existing parts to reuse

- `dubbox-display/src/ui.cpp`, `screen_menu.cpp` and `ui_common.h`: touchscreen navigation.
- Existing touchscreen keyboard layout and shared drawing functions: query entry.
- `services/stem-engine/studio_stem_engine/engine.py` and `backends.py`: current separator.
- `transfer.py`: synchronized PCM16 export and manifest verification.
- ESP32 upload ring plus existing Teensy CRC/chunk/SD writer: bounded-memory transfer.
- `teensylink::sendNewProject` and `sendAssignTrack`: existing project/track commands.
- Current screen mirror: observe the actual new screen during hardware testing.

## Compute boundary

The ESP32 drives search and progress UI, HTTP control, and streaming transfer. The
Teensy continues to own SD and audio playback. Neither runs the current Python/ML
model stack. A companion Mac or hosted worker runs the existing separator.
The user is choosing that execution location; do not claim standalone processing
on the currently installed microcontrollers.

The same asynchronous job interface should support either worker location. A LAN
Mac can use the existing cached models. A hosted worker requires a deployment,
authentication, storage limits and an agreed operating budget before activation.

## Jobs and progress

States: queued, resolving, downloading, decoding, separating, exporting,
transferring, assigning, ready, cancelled, failed. Show completed bytes for download
and device transfer. The installed Demucs implementation supports callbacks for
completed model/shift/segment work, so separation progress can be based on actual
work. Export and other uninstrumented stages should show stage + elapsed time,
not a fabricated percentage or time estimate. Ready means all four assets are
confirmed on SD and assignments have been confirmed, not merely an HTTP response.

## Acquisition

Use an explicit media-provider adapter. Search returns stable video IDs, titles,
uploaders and durations; user selection must preserve that ID. Confirmation starts
the job. A maintained downloader can retrieve permitted public media; avoid a
browser-scraped converter website dependency. Decode its best available audio
straight to a supported WAV, avoiding needless MP3 re-encoding before separation.
Handle unavailable/private/live/restricted media and download failures explicitly.
No access-control bypass is part of the feature. Source-platform conditions are
separate from the user's copyright permission. The YouTube Data API is not a
permission to download or separate its audiovisual content.

## Device transfer and recovery

Prefer device-initiated requests so both LAN and remote workers work without an
internet-exposed ESP32. Stream assets into the existing ring, retaining CRC,
acknowledgements and backpressure. A new worker download endpoint must not buffer a
whole song on the ESP32. Confirmed filenames, including collision suffixes, are
authoritative. Existing projects must not be silently overwritten. Stage all four
files, create a uniquely named project only on success, then assign in canonical
VOCALS, MELODY, BASS, RHYTHM order. Partial files must be reported after failure;
the current protocol has no whole-set atomic commit or rollback.

## Verification before hardware release

Test provider failures, cancellation, job state transitions, real progress,
malformed search results, lost WiFi, reconnects, SD full, transfer timeouts, four
file completion and no assignment after a partial failure. Build the ESP32 with
the existing diagnostic mirror enabled. Exercise the actual screen and transfer
on the boards before calling the device workflow complete.
