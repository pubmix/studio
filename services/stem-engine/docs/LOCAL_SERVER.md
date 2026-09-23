> Extended update: the companion now supports four/extended/dynamic modes and model
> profiles. Existing device delivery remains four-stem; extended assets are separate
> downloads. See [current API additions and limits](EXTENDED_STEMS.md). The historical
> validation below describes the original four-stem release.

# Local companion server and future cloud migration

## Status

Implemented in the existing stem-engine service, not a second separator. This is a
single-user companion on the Mac. The browser page controls search, local MP3/WAV
uploads, background splitting, cancellation, recent jobs, authenticated WAV downloads,
and optional delivery through the existing ESP32 WiFi upload client.

The hardware touchscreen search screen and automatic four-track project assignment
are **not implemented**. Use the current device file picker after delivery. `ready`
in this API means four verified downloadable WAVs on the companion, not SD readiness.
Device delivery has its own `sending`, `done`, and `failed` status and reports actual
saved filenames. Do not confuse completion of separation with completion of delivery.

## Start on a new computer

Install Python 3.11, uv, and Node 22 or newer (for the downloader). From this directory:

```sh
scripts/setup-server
scripts/studio-server --device http://dubbox.local
```

The existing CLI remains available. The server uses the same Engine, fast Demucs
preset, FLOAT masters, prepared-set exporter, and device upload implementation.
Dependencies are declared in pyproject.toml; the `cloud` optional group contains
the portable server dependencies and is also used locally. First inference may
fetch model weights. The supplied Mac launcher reuses the already installed runtime,
model cache, and isolated server dependencies; no second model installation is needed.

Default address is http://127.0.0.1:8766. Port 8765 belongs to the existing hardware
mirror and is left alone. The runner creates a private pairing key and opens the
browser with it in a URL fragment; the page immediately removes that fragment and
keeps the key in session storage. Do not share the key. Jobs and the key default to
`~/Library/Application Support/Pub Mix Studio`; `--data` chooses another location.
The prepared launcher uses this task's private work directory. No secrets are
stored in source files. One process per data directory is enforced by a file lock.

The launcher keeps the Mac awake while its server runs. Closing its terminal or
stopping the server interrupts active work; the next start marks unfinished jobs
failed. This is not an automatic login service. Opening the launcher again opens
the existing server. Do not move/delete its referenced project and runtime folders.

## Use

1. Search by title or paste a supported YouTube link, or choose your own MP3/WAV.
2. Confirm permission and select Split. The Mac processes one job at a time.
3. Watch the current stage. Download progress has a percentage when byte totals
   are available. Separation/export use an indeterminate bar rather than invented
   percentages. Complete separation publishes four synchronized 44.1 kHz stereo
   PCM16 WAVs in VOCALS, MELODY, BASS, RHYTHM order.
4. Save the four stems, or select Send to Studio. The latter is enabled when the
   server has `--device` configured. Studio and the Mac must share a reachable LAN;
   the device must be linked and playback stopped. If mDNS is unavailable, use its
   WiFi IP address instead. `STUDIO_DEVICE_URL` overrides the supplied launcher.
5. Assign the actual returned filenames to device tracks 1–4 and save a project.

Local input is limited to 100 MiB and 15 minutes. Decode is capped before model
allocation. Jobs time out after 30 minutes. Four GiB of free disk space is required
before accepting a split. Completed artifacts become eligible for cleanup after
24 hours; cleanup happens on startup or the next submission. Save wanted results.

YouTube search returns up to five IDs, titles, uploaders and durations. Downloads
use yt-dlp directly, then decode the source audio without an intermediate MP3 encode.
Only public supported video URLs/IDs are accepted; no cookies, login or access-control
bypass is configured. Source availability and platform conditions can still prevent
a download. `STUDIO_JS_RUNTIME` can point to Node explicitly. Local uploads work
without YouTube access once model weights are cached.

## Architecture and interfaces

- `cloud.py`: authenticated `/v1` API, bounded requests, SQLite status, serial job
  admission, worker timeout/cancellation, checked publication and asset downloads.
- `cloud_worker.py`: isolated acquisition/decode -> existing Engine -> export_transfer.
  Cancellation terminates the process group; incomplete assets are not published.
- `media.py`: YouTube provider and bounded FFmpeg decoding.
- `local_server.py`: configurable host/port/data, private pairing key and browser launch.
- `local.html`: small companion control page, packaged with the existing service.
- `scripts/setup-server`, `scripts/studio-server`: portable installation/start commands.
- `tests/test_cloud.py`: auth, rights, request replay, busy rejection, cancellation,
  timeout, restart recovery, media IDs, file download and delivery adapter integration.

`GET /healthz` identifies API version 1. Other API calls use `Authorization: Bearer KEY`.
`GET /v1/search?q=...`; `POST /v1/jobs` with video_id, rights_confirmed and unique
request_id; `POST /v1/uploads?title=song.mp3` with raw file body;
`GET /v1/jobs` (20 recent); `GET /v1/jobs/{id}`; `POST /v1/jobs/{id}/cancel`;
`GET /v1/jobs/{id}/stems/{vocals|melody|bass|rhythm}`.
For local delivery, `POST /v1/jobs/{id}/send` and `GET /v1/device`.
Repeated video submissions with the same request_id return the same job. File upload
and physical delivery are not automatically retried: network errors may leave a job
or partial SD files. Review recent jobs/returned filenames before retrying.

## Future cloud move

The server already separates processing from its clients. Host address, token,
working directory and device address are configuration, not baked into the engine.
For a future server, run one API/worker process with persistent private storage and
model cache, add HTTPS and host-level resource limits, and set the device/client's
base URL to the new endpoint. Keep this `/v1` interface and prepared-set contract.

Do not expose the current HTTP listener directly to the internet. `--host 0.0.0.0`
is an explicit trusted-LAN development option, not a cloud deployment. Production
also needs device-specific accounts/keys, quotas, durable cleanup, monitoring and
backups. This SQLite/thread scheduler is a one-device pilot, not a multi-user queue.
A cloud server cannot push to `dubbox.local`: future firmware must initiate authenticated
asset downloads (or retain a local relay). That device work is still pending.
WordPress/DNS are untouched, and no paid service has been created.

## Validation on this Mac

- All 50 stem-engine/service tests passed, including nine new API/media cases;
  API lifecycle and delivery tests use explicit fixtures, no model downloads.
- Full supplied `I Got You Babe.mp3` submitted over local HTTP to the real worker:
  four outputs, each 8,339,016 frames, stereo PCM16 44.1 kHz, 33,356,108 bytes.
  Each authenticated download matched its manifest SHA-256.
- Real YouTube title search returned five results. Browser search, pairing, recovered
  completed job and four download buttons were inspected; no browser console errors.
- Existing `http://dubbox.local/status` returned linked=true, busy=false, playing=false.
  This is a connectivity check, not a physical 134 MB SD upload/playback validation.
- Full YouTube acquisition was not exercised with a separately selected authorized
  video; the complete split test used the user's local MP3. Download failures remain
  possible. No firmware was modified or flashed in this local-hosting change.

## Wi-Fi upload page pairing (2026-09-23)

The device-hosted upload page now uses this existing API for stem jobs. `--device`
also enables CORS for that exact origin only; use the same hostname/IP in the browser.
The companion's new pairing details reveal the existing key for copying to that page.
No new unauthenticated job endpoint or wildcard CORS was added. Restart an older running
companion to pick up these changes. The original YouTube page and upload client remain.
See [device setup, compression protocol, validation and limits](../../../dubbox-display/WIFI_UPLOAD.md).
