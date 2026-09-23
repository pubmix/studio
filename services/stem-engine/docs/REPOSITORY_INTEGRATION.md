# Studio / Dub-Box stem integration

## Architecture and source of truth

This integration uses `pubmix/studio` main at
`4ce656610a5eba4b33e5003f2450bf1347bb4ce9`, plus the current local
`dubbox-display` work. The active application is `dubbox-firmware` (Teensy)
and `dubbox-display` (ESP32), as described in `DUBBOX_PROJECT_SUMMARY.md`.
The older `firmware/teensy` core is a separate prototype in this same repository;
its manifest contract and validator remain useful compatibility checks, but its
`App::openDub()` interface is not the current hardware's upload interface.

```text
MP3/WAV -> existing Engine/backends on Mac -> FLOAT32 masters + metadata.json
        -> transfer.export_transfer -> 4 x stereo PCM16/44100 + manifest.json
        -> device.upload_transfer -> ESP32 HTTP /upload -> existing CRC UART relay
        -> Teensy SD root -> existing file picker -> tracks 1, 2, 3, 4
```

The source master API (`studio.stems.v1`), shared transfer schema (`api_version: 1`),
ESP32 endpoints, UART framing, playback code and project format remain unchanged.
Separation never runs on the Teensy, ESP32 or inside the audio loop.

## Transfer API

```python
from studio_stem_engine import Engine, Settings
from studio_stem_engine.transfer import export_transfer, verify_transfer
from studio_stem_engine.device import upload_transfer

masters = Engine("masters").separate("song.wav", Settings(preset="fast"))
prepared = export_transfer(masters, "prepared/song", set_id=123, title="Song")
manifest = verify_transfer(prepared)
# Explicit device write; stop transport and drum preview first.
files = upload_transfer(prepared, "http://dubbox.local", progress=print)
```

The exporter verifies original master hashes, applies the same polyphase resampling
to every lane, duplicates mono to stereo, and retains the same frame-zero origin.
It applies one gain to all stems, based on the greatest stem or summed-mix peak.
Default headroom is 1 dB, with a small PCM quantization margin; quiet input is not
amplified. It never independently normalizes or trims stems. Frame count uses the
same ceiling resampling rule for all four lanes. Disk-backed intermediates avoid
keeping all four stems in RAM, though resampling still requires a full stem array.

The manifest records source key, shared gain, headroom and SHA-256 hashes in
addition to required v1 fields. Export is staged and atomically renamed only after
validation; an existing destination is refused. Masters are preserved. Transfer
verification requires canonical names/order, uint32 set identity, complete aligned
PCM16 payloads, local non-symlink assets and matching hashes.

## Current hardware transport

`device.py` uses standard-library HTTP streaming, with bounded 64 KiB file blocks.
Before each file it checks `/status` for linked, idle, stopped state. The current
Teensy also rejects uploads during drum preview. Files are sent as multipart field
`file` to `/upload?name=S123-VOCALS&size=...`, matching the browser upload page.
Names preserve the canonical role within the firmware's 32-character base limit.
Teensy accepts 64 through 0x7fff0000 bytes; all four sizes are checked before writing.

Uploads occur in VOCALS, MELODY, BASS, RHYTHM order. The server's returned filename
is authoritative because existing names receive suffixes. Confirmed files appear
in progress events and the CLI's final JSON (or `UploadError.uploaded` on failure).
Assign those actual names to tracks 1–4 in the existing picker and save normally.
The manifest remains on the computer: the current SD root uploader accepts WAVs,
not set manifests or project commands.

A failure stops further writes without automatic retries. Already confirmed files
remain on SD; a failed/timed-out request might also have finished on the device.
There is no remote rollback, resume, whole-set atomic commit or device hash readback.
Do not treat receipt of four HTTP confirmations as a physical playback test.

Without WiFi, copy the four prepared WAVs to the SD root with distinct song/role
filenames while playback is stopped (or safely remove the card first), then use
the same picker. Avoid overwriting other songs named `vocals.wav`, etc.

## Runtime and hardware dependencies

- Python 3.11+, NumPy/SciPy/SoundFile and the existing selected model backend.
- Model weights cached locally; first use requires their download. Fast Demucs CPU
  is supported; MPS/MLX needs suitable Apple hardware and runtime access.
- ESP32 running the existing WiFi uploader, on the same reachable LAN as the Mac.
  Use its displayed IP if mDNS does not resolve. The current HTTP server has no
  authentication; use it on the intended trusted local network.
- Teensy 4.1, writable SD card, existing 2 Mbaud UART link, and audio hardware for
  playback. Stop transport and pattern preview before transferring.
- No flash or hardware write was needed to implement this adapter. Physical WiFi
  transfer and audition remain to be verified on the actual boards.

## Validation

See the integration validation section of [VERIFICATION.md](VERIFICATION.md).
The existing firmware validator checks exported sets unchanged. These checks verify
format, hashes and sample-grid alignment; they do not establish musical source
isolation, subjective quality or phase alignment in live hardware playback.

## Related contracts

- [Shared transfer schema](../../../../packages/contracts/stem-set/v1/manifest.schema.json)
- [Legacy firmware adapter](../../../../firmware/teensy/INTEGRATION.md)
- [Current application overview](../../../../DUBBOX_PROJECT_SUMMARY.md)

Paths above are relative to this document's component; repository-wide references
are also available from the root README.
