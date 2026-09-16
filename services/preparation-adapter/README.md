# STUDIO preparation adapter — V1 integration prototype

Converts the existing Stem Engine's native synchronized FLOAT32 WAV set into the Teensy firmware's STUDIO-P1 transfer profile. It preserves the native files and all four roles: **VOCALS / MELODY / BASS / RHYTHM**.

## Run

Requires Python 3.11+, NumPy, SciPy and SoundFile. The existing Stem Engine environment was used for validation; there is no new model or ML dependency.

```sh
python prepare.py /path/to/native-result /path/to/prepared-library
```

Input: metadata.json using studio.stems.v1, plus the four canonical FLOAT32 mono/stereo WAVs with mandatory SHA-256 hashes. Output: prepared-library/set-00000001/ containing four 44.1 kHz stereo PCM16 WAVs, firmware-compatible manifest.json and preparation.json provenance. Treat the printed directory as the only ready result.

Use the same prepared-library for every import into an instrument library. Its set-ids.sqlite3 registry allocates positive uint32 IDs under a process lock; keep and back up this registry. Independent registries are separate ID namespaces and must not be merged directly. Never delete a registry while keeping its sets.

The conversion validates the entire native set before writing, applies one zero-phase polyphase resampling convention and one shared frame count, measures both individual and reconstructed peaks, applies one common gain (no boost, nominal -1 dBFS ceiling), adds deterministic TPDF dither, and verifies every encoded output. Mono samples and their dither are duplicated to both channels. No independent trims or per-stem normalization occur.

Reuse of identical content/settings/library versions retains the set ID and validates the existing prepared media. Changes create a new set. Failures clean up staging; a process crash can leave hidden .staging-* directories and an allocated unused ID. Such directories are never ready assets. Local same-filesystem directory rename publishes all files together; this is not a FAT/SD power-loss journal.

## Files and tests

- prepare.py: CLI, strict native validation, conversion, durable ID allocation, publication and prepared validation.
- test_prepare.py: generated synthetic fixtures and 19 integration/regression tests.
- requirements.txt: exact versions used in this run.
- VERIFICATION.md: results, source snapshots and limitations.

```sh
STUDIO_FIRMWARE_VALIDATOR=/path/to/studio/firmware/teensy/tools/validate_prepared.py python -B -m unittest -v test_prepare
```

Tests intentionally require the actual sibling firmware validator, not a copied substitute. Fixture WAVs and malformed examples are generated in temporary directories; no personal audio or model weights are included.

## Integration handoff

This component lives under services/preparation-adapter/ in the shared repository. Call prepare(source_directory, prepared_library) after the Stem Engine atomically publishes a native result. Then validate/copy the returned complete set through the existing firmware transfer workflow. The native engine API and firmware source do not change.

LOCKED: canonical four roles, common alignment, preservation of native results.
RECOMMENDED: STUDIO-P1, -1 dBFS ceiling, common gain, deterministic TPDF.
OPEN: shared registry ownership across devices, longer-track streaming implementation, transfer service and UI.
NEEDS BENCHMARK: real target import/playback, memory/time on CM5, audio assessment.

## Limits

Offline Mac/Linux prototype (fcntl), not MCU code. It processes complete tracks in memory and limits both native and resampled FLOAT32 sets to 128 MiB across all four roles. At 44.1 kHz stereo this permits approximately 95 seconds; longer tracks fail explicitly before conversion. Float64 work buffers can require several times this bound. A streaming implementation is needed for production-length sessions.

Supports native rates 8–192 kHz, mono/stereo only; rejects PCM native sources, RF64, unexpected WAVs, bad hashes, nonfinite samples, path mismatches, symlinked assets and unequal headers. Equal frame grids and the impulse test verify conversion alignment, not correctness of the separator's original musical alignment.

No hardware, listening-quality, full-song or CM5 benchmark was performed. This component is included in the integration change; physical validation and release gates are tracked in [the shared baseline](../../docs/SYSTEM_ARCHITECTURE.md).
