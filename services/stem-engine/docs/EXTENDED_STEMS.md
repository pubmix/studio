# Extended stem separation

## Status and compatibility

Implemented in the existing service, including the current local YouTube companion.
YouTube search/acquisition and bounded decoding are unchanged. New requests travel
through the existing job worker, Engine, verified export, and authenticated downloads.
Default mode remains `four`, with VOCALS, MELODY, BASS, RHYTHM in that order.
The current service source includes local companion changes not yet present on GitHub main.

The service supports 4–16 output channels, grouped in canonical family order, with
one-based `channel`, `bank = floor((channel-1)/4)+1`, and `lane = (channel-1)%4+1`.
This is a service-side mapping for the planned banked channel model. Both current
firmware implementations have **four playable tracks**, not sixteen. V2 sets are
not accepted by the existing device importer. The companion's Send to Studio uses
the four-stem mixdown and existing upload path; individual extended assets are
available for download. Automatic device project assignment remains unimplemented.

## Select a model and mode

```
scripts/studio song.wav --preset fast --mode extended --model htdemucs_6s
scripts/studio song.wav --mode dynamic --stem-depth 8 --presence-db -45
scripts/studio song.wav --backend roformer --model YOUR_FULL_SOURCE_MODEL.ckpt --mode extended --fallback
```

- `four`: sum supported source families into the unchanged v1 output contract.
- `extended`: retain model sources, folding silent outputs and excess channels.
- `dynamic`: additionally fold low-energy and low-confidence estimates.
- `stem_depth`: maximum number of published channels, 4–16; it never invents sources.
- The Demucs six-source model is the built-in extended default. It yields vocals,
  guitar, piano, other, bass and drums before filtering. Choosing depth 16 does not
  make it produce sixteen separated instruments.
- The experimental `best` two-stage preset remains available only in four-stem mode.
- `--fallback` is explicit for the CLI/Python API. The companion enables it. Failures
  of non-Demucs backends retry `htdemucs` in four-stem mode. Requested/effective
  settings and the failure are recorded; fallback results have a separate cache key.
  Cancellation is not retried. Benchmarks always disable fallback.

Backends implement the existing `identity` and array-based `separate` interface.
The new `roformer` backend name selects the existing pinned audio-separator runtime,
now supporting the full source taxonomy. `audio-separator` and `mlx` remain valid.
Install `.[roformer]` (audio-separator 0.45.0, same pinned version as the tested
separator adapter). Configure `STUDIO_MODEL_DIR` for weights and matching model
configuration files supported by that runtime. The runtime's source labels must
map to `sources.py`; unknown/duplicate labels or incomplete family coverage fail.
A two-source vocal/instrumental checkpoint is **not** a full-source model. It cannot
supply guitar, drums, or bass; it is rejected or triggers the explicit fallback.

Backends must return disjoint source estimates at the original sample grid and gain,
not overlapping parent and child stems. `SeparationResult(sources, presence_confidence)`
adds optional calibrated presence scores. Old dictionary-returning backends work.
Local Separator checkpoint/config hashes participate in cache identity when present;
new downloads can cause a one-time cache miss. Model provenance is recorded per stage.

## Presence and reconstruction

The current built-in models return **no calibrated confidence**. Their metadata
contains `presence_confidence: null`, `presence_method: energy_heuristic`, raw RMS,
and level relative to the mixture. Dynamic mode defaults to -45 dB relative RMS.
Optional backend presence scores below 0.5 are filtered. Scores must be finite in
[0,1]. Energy is not proof of instrument presence or separation quality. Bleed can
be loud, and a valid quiet instrument can be filtered. `separation_quality` stays null.

Suppressed audio is folded into its family remainder rather than discarded. Metadata
records source-level reasons (`silent`, `low_energy`, `low_confidence`, `channel_limit`)
and folded sources. At the channel cap, quieter sources are consolidated first.
These are fewer exposed isolated channels, not a claim that the recording is silent.
With consistency enabled, reconstruction error goes into OTHER if available, otherwise
MELODY; this may introduce a remainder channel or consolidate melody children at the cap.
Extended mode uses this policy for both `melody` and `energy`; `none` disables it for
raw-model comparison. Four-stem mode retains its existing energy-projection behavior.

## Files and API

Four-stem masters remain `studio.stems.v1`. Extended masters use `studio.stems.v2`,
with 4–16 top-level FLOAT WAVs plus a verified `four/` v1 mixdown. All share the source
sample rate, channels and frame count. Source labels are a closed filename-safe taxonomy.
Both exports use a common attenuation factor per set, preserving relative gains and
protecting the sum against PCM16 clipping. Extended and four-stem transfer gains may
be different; do not combine their WAVs for playback.

`export_transfer(..., extended=True)` writes a v2 prepared manifest. The default
export automatically selects `four/` and remains v1. `verify_transfer(..., extended=True)`
is required to accept v2; the device uploader deliberately uses the v1 verifier.

Authenticated companion API additions:

- `GET /v1/models`: server-owned model profiles, modes and channel limit.
- `POST /v1/jobs`: optional `mode`, `model` (profile ID), `stem_depth` alongside the
  existing video_id, rights_confirmed and request_id. Defaults preserve older clients.
- `POST /v1/uploads`: accepts the same options as query parameters.
- Job responses retain `stems` as four-stem downloads and add `extended_stems`,
  requested/effective settings, fallback details and source measurements.
- `GET /v1/jobs/{id}/extended/{role}`: authenticated extended WAV download.
- Reusing request_id with different video/settings returns HTTP 409.

Set `STUDIO_MODEL_PROFILES` to an operator-controlled JSON file following
`model-profiles.example.json`. HTTP clients select profile IDs, not arbitrary local
weight paths. Built-ins `demucs-four` and `demucs-six` cannot be replaced. Restart the
companion after editing source or changing profiles. No running server is restarted
by the implementation task; ongoing jobs are left undisturbed.

## Benchmark and training scaffolding

`python -m studio_stem_engine.benchmark benchmark.extended.example.json --output results`
compares identical inputs, with cache bypass and no fallback. Four-family references
are compared against the v1 mixdown even for extended models. Optional `source_references`
score exposed detailed sources and mark absent sources `not_exposed`. Metrics include
SI-SDR, SNR, transient first-difference error and stereo side-channel error. Runtime
includes initialization/downloads. No perceptual artifact score, leakage-specific metric,
peak VRAM measurement or listening-test winner is claimed. Use held-out multitracks and
blind listening before selecting a production model.

`python -m studio_stem_engine.training training.example.json` validates data.
Add `--output NEW_FOLDER --seed 42 --seconds 10` to prepare aligned mixtures and stems.
The validator requires source coverage, aligned grids, rights descriptions, recording
session groups and train/validation/test split labels; it rejects cross-split group
or byte-identical source leakage. It cannot detect different encodings of the same song.
Preparation uses seeded shared crops and per-source gain augmentation on training items,
with common headroom scaling. Validation/test use the first crop and no source augmentation.
The output manifest records hashes, crops and gains. This is a dataset preparation
interface for a future trainer, not a training loop, checkpoint or proprietary model.

Next requirements: licensed multitracks with desired isolated-source labels; a compatible
full-source RoFormer checkpoint and matching configuration; GPU training resources;
held-out calibration data and listening evaluation. Existing vocal-only RoFormer weights
cannot train or produce extra source classes without architecture/data/training work.

## Verification

See `EXTENDED_VERIFICATION.md` for exact commands and observed results. Synthetic
fixtures and real short model smoke runs establish execution and contracts, not quality.
