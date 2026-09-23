# STUDIO Stem Engine

Local stem preparation for STUDIO, built for Apple Silicon Mac.

Default four-stem jobs produce **four synchronized FLOAT32 WAV stems**, in this order:

| Lane | Stem | File |
|---|---|---|
| 1 | VOCALS | vocals.wav |
| 2 | MELODY | melody.wav |
| 3 | BASS | bass.wav |
| 4 | RHYTHM | rhythm.wav |

MP3/WAV input; sample rate, decoded length and mono/stereo channels preserved. Metadata accompanies the audio files. Extended/dynamic modes retain supported model sources, including OTHER, with up to 16 mapped channels and a separate four-stem mixdown.

## Run

From this directory, create the local environment and run:

```
scripts/setup
scripts/studio /path/to/song.mp3 --preset fast --output /path/to/stems
```

For a new installation see [installation](docs/INSTALL.md). Fast uses Demucs; Balanced uses its fine-tuned bag; **Best is an experimental staged vocal/instrument pipeline**, not a proven winner. The CLI also supports explicit model/runtime choices and reconstruction policies. `scripts/studio --help` lists them.

## Python API

```python
from studio_stem_engine import Engine, Settings

folder = Engine("prepared").separate(
    "song.wav", Settings(preset="best"), progress=print
)
metadata = Engine.verify(folder)
```

The returned directory is atomic and complete. Same input/settings reuse checked outputs. Failures raise EngineError and never publish a partial stem set. Custom backends can implement the small array-based interface; weighted ensembles are available through `EnsembleBackend`.

## What is verified

See [verification results](docs/VERIFICATION.md) for actual runtime evidence and limitations. Synthetic tests establish execution, mapping, timing and reconstruction correctness. They do **not** establish separation quality. Representative music and isolated references are still needed to select the production pipeline.

- [Engine architecture and residual tradeoffs](docs/ARCHITECTURE.md)
- [Research and model status](docs/RESEARCH.md)
- [Benchmark protocol](docs/BENCHMARKING.md) and [manifest template](benchmark.example.json)
- [Saved STUDIO product context](docs/PRODUCT_CONTEXT.md)

This service runs on the companion computer. The existing Dub-Box Teensy firmware handles playback and DSP; the ESP32 handles its touchscreen and WiFi uploads.

## Firmware integration boundary

The existing FLOAT32 master interface remains unchanged. `studio_stem_engine/transfer.py`
exports verified masters into the shared prepared-set v1 format: four aligned
44.1 kHz stereo PCM16 WAVs plus `manifest.json`. A shared attenuation factor
preserves the relative stem gains and protects the full mix from clipping.

`studio_stem_engine/device.py` uploads these WAVs sequentially through the current
Dub-Box ESP32 `/status` and `/upload` endpoints. It reuses the existing UART/CRC/SD
write path. The CLI adds no new firmware protocol or second audio engine. An optional local
HTTP companion now wraps these same adapters; see below.
Select the returned filenames in tracks 1–4 using the existing touchscreen file
picker, then save the project. Automatic four-track assignment is not implemented.

```sh
# Separate and prepare on the computer; destination must be new.
scripts/studio song.mp3 --preset fast --output masters \
  --transfer-output prepared/song --set-id 123 --title "Song"
# Explicitly transfer after connecting the device to WiFi and stopping playback.
scripts/studio --prepared prepared/song --upload http://dubbox.local
# Or export an existing master set without rerunning the model.
scripts/studio --export masters/CONTENT_HASH --transfer-output prepared/song2 --set-id 124
```

The first command can also take `--upload http://DEVICE_IP` to run all three stages.
`--prepared FOLDER` alone verifies an existing transfer set. `--verify` still verifies
FLOAT masters. Use distinct set IDs for different songs; the device adds suffixes
when names collide. The CLI reports actual saved names, including confirmed partial
uploads on failure. Inspect the SD before retrying; the device has no transaction,
rollback, remote checksum query or automatic resume endpoint.

See [integration details](docs/REPOSITORY_INTEGRATION.md) for architecture, API,
manual SD transfer, dependencies and validation. Tests: `make test-stems` from the
repository root after installing the engine test dependencies. Set `STEM_PYTHON`
for that Make target, or `STUDIO_PYTHON` for the launcher, to reuse an existing
compatible Python environment. The launcher otherwise uses this component's `.venv`.

## Next steps

Validate uploads and four-track playback on the physical device with representative
music, benchmark separation quality, and decide whether atomic set import and automatic
track assignment merit a versioned firmware protocol extension. Best remains
experimental; Spleeter remains unverified. Current integration tests use synthetic
audio and a simulated HTTP device, not a physical SD/write/playback test.

## Documentation

Each new engine component folder must include a README.md covering purpose, current status, installation/usage, verification, limitations and next steps. Keep it current with code and model changes. Follow the [repository convention](../../docs/CONTRIBUTING_COMPONENTS.md).

## Local browser companion (implemented)

Run `scripts/setup-server` once, then `scripts/studio-server --device http://dubbox.local`.
The page at http://127.0.0.1:8766 supports search, MP3/WAV uploads, background jobs,
cancellation, stem downloads and delivery through the existing WiFi uploader.
The processing API is portable for later hosting. On-device search is available through MENU → STEM SPLITTER; automatic
project assignment remains pending. See [local server setup, validation and cloud
migration](docs/LOCAL_SERVER.md). Server tests install `.[test,cloud]`; the default
CLI tests remain independent of the optional web dependencies.

## Extended modes and companion

The existing YouTube/local-file companion now exposes mode, model profile and channel
limit controls, keeping the original ingestion and four-stem device upload. See
[extended capability, configuration, API, training and limitations](docs/EXTENDED_STEMS.md)
and [validation](docs/EXTENDED_VERIFICATION.md). No proprietary model has been trained.

Quick start: `scripts/studio song.wav --mode extended --model htdemucs_6s`.
Use `scripts/studio-server` for the existing companion; its optional dependencies are
`.[cloud,demucs]`. Add `.[roformer]` and a compatible full-source checkpoint for that
backend. The `model-profiles.example.json` RoFormer entry is a placeholder, not weights.

## Device Wi-Fi page integration

The existing ESP32 Wi-Fi upload page can now pair with this service and submit four,
extended or dynamic splits directly. Start with `--device` matching the exact device
page origin to enable narrowly scoped CORS; bearer authentication remains required.
The companion page exposes the existing pairing key on request. YouTube ingestion and
server-side Send to Studio retain their existing behavior. Compression is selected in
the device page and decoded on ESP32; the CLI defaults to original WAV and the touchscreen can request compressed delivery. See [setup, protocol, measured tradeoffs and tests](../../dubbox-display/WIFI_UPLOAD.md).
Validation: 75 service tests passed, including CORS origin/auth checks.

## Physical touchscreen controller

`studio_stem_engine/panel.py` exposes bounded, authenticated source/library/job views
for MENU → STEM SPLITTER on the device. Computer files can be staged without immediately
splitting them. The existing separation/YouTube engine remains the worker. Device delivery
now optionally uses the existing lossless-block protocol; default CLI/companion delivery
remains original WAV. The companion must be reachable on the LAN. See
[native controller setup and validation](../../dubbox-display/STEM_TOUCHSCREEN.md).

For a clean installation on another computer, use the [new-computer guide](../../docs/NEW_COMPUTER.md). The portable `Start Studio Stems.command` launcher enables the LAN connection needed by the device.
