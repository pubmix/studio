# STUDIO Stem Engine integration — prepared-set API v1

## Responsibility split

The Stem Engine owns source decoding, model choice, semantic recombination and native alignment. The [preparation adapter](../../services/preparation-adapter/README.md) owns conversion of its native FLOAT32 result into this P1 transfer profile, with shared resampling, gain and encoding. See the [shared system architecture](../../docs/SYSTEM_ARCHITECTURE.md) for the complete ownership boundary. This firmware never runs ML. There is no requirement for cloud accounts or network connectivity in Dub playback.

STUDIO accepts exactly four assets, in this order:

| Array/lane | Role | File |
|---|---|---|
| 0 / 1 | VOCALS | vocals.wav |
| 1 / 2 | MELODY | melody.wav |
| 2 / 3 | BASS | bass.wav |
| 3 / 4 | RHYTHM | rhythm.wav |

The service must recombine any internal harmonic labels into MELODY before export. User-facing APIs do not contain a fifth generic stem.

## Transfer profile

Version 1 firmware transfer assets are **44100 Hz, two channels, signed little-endian PCM16 RIFF/WAVE**, with identical nonzero frame counts and a shared frame-zero origin. The service may keep higher-resolution masters independently. Float WAV, PCM24, mono, compressed WAV, RF64 and MP3 are rejected by this prototype playback importer. Resample all four with the same transform, trim/pad together and compensate model latency before encoding. No normalization or automatic stretching occurs in firmware.

Transfer directory example:

```text
PreparedSong/
  manifest.json
  vocals.wav
  melody.wav
  bass.wav
  rhythm.wav
```

```json
{
  "api_version": 1,
  "set_id": 123,
  "title": "Prepared Song",
  "sample_rate": 44100,
  "frames": 5292000,
  "alignment_offset_frames": 0,
  "stems": [
    {"role": "VOCALS", "file": "vocals.wav"},
    {"role": "MELODY", "file": "melody.wav"},
    {"role": "BASS", "file": "bass.wav"},
    {"role": "RHYTHM", "file": "rhythm.wav"}
  ]
}
```

Each stem may include `sha256`; when supplied it must match. `tools/validate_prepared.py DIRECTORY` checks the manifest, canonical order, asset containment, WAV format, complete payload length and optional hashes. It emits JSON progress/status with `stage: ready` only after all four pass. It does not measure musical alignment or separation quality; matching frame counts cannot prove latency compensation. The producer's zero-offset declaration is part of the contract.

Publish the manifest only after all output files are closed. Validate and copy to a staging folder before exposing the set to the instrument. The shipping transfer mechanism (USB, removable SD, application processor) is not chosen here.

## Connect without changing Dub Mode

1. Implement `IStemEngine::status`, `prepared(StemSet&)`, and `cancel()` for the real engine/transfer service.
2. Expose progress via `ImportStatus`: Idle → Preparing → Copying → Validating → Ready, with Failed/Cancelled terminal alternatives. The in-tree `PreparedStemEngine` validates already prepared assets; it does not pretend to separate audio.
3. Register each WAV with an `IAudioAssets` implementation and assign stable, distinct nonzero asset IDs.
4. Fill `StemSet` with API version 1, a nonzero set ID, title and four `StemAsset` records in canonical order. Metadata must be derived from the actual WAV header, not just trusted manifest claims. `alignmentOffset` must be zero.
5. Submit through `PreparedStemEngine::submit(set, assets)` or implement equivalent validation in your engine adapter.
6. Call `App::openDub(engine)` on the foreground task. It fetches a Ready set, validates/loads it through `AudioEngine`, stops/rewinds transport, stores the set ID and enters Dub. Lane mapping is automatic.

`StemPlayer` has one frame cursor for all four assets. Each read either returns all four aligned frames or silence for all lanes on an asset failure. The cursor advances for the whole set on an underrun, preventing relative drift; there is no automatic disk-stall pause or retry policy. End of source produces silence and lets effects decay. The global clock continues until Stop.

The host `preparedFolder()` stages all four readers before committing them. It is a minimal filename-based test adapter, assigning IDs 100–103 and set ID 1; **it does not parse manifest metadata**. Use the validator before the host demo. A real service adapter must preserve manifest set identity and use a stable asset registry. The SD sketch likewise uses one explicitly configured prepared directory and test IDs. Register/import only with performance stopped; asset replacement while playing is unsupported.

## Projects and reopening

Projects persist `stemSet` and asset references, not file handles or audio bytes. The application must resolve those IDs to assets at reopen and call `loadStems` again. `App::load()` alone restores creative state; it cannot resolve an arbitrary catalog. The prototype has no network protocol, automatic cross-task discovery or runtime ML engine binding. No changes to Dub mixing/control code are needed to add the real registry/adapter.

## Future protocol changes

Reject unknown major API versions. Introduce a new transfer profile/version for additional sample formats or sample rates. Never silently reinterpret lane order, independently trim stems, treat a missing lane as optional, or start playback while files are being written. Service-specific metadata can remain outside the compact firmware `StemSet`.
