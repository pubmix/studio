# Engine design

## Stable boundary

`Engine.separate(path, Settings(...), progress=callback)` returns a completed directory. Backends accept frames×channels float arrays and must return source arrays at the input sample rate, length and gain. Native adapter sources are canonicalized before publication. Missing categories and unknown sources are errors. Default metadata and CLI expose VOCALS, MELODY, BASS, RHYTHM. Extended modes publish model sources and a four-stem mixdown; see [extended architecture](EXTENDED_STEMS.md).

Input MP3/WAV is decoded once. Outputs retain its decoded sample rate, frame count and mono/stereo channel count. MP3 synchronization refers to the decoder's gapless sample grid, not compressed bytes or an unavailable pre-encoding master. Model adapters resample as needed, restore the grid, and allow at most two samples of resampling rounding. Larger mismatches fail. This checks shape, not a model's internal perceptual phase accuracy.

FLOAT32 WAV preserves relative levels and estimates above 0 dBFS without independent stem clipping/rescaling. Playback must provide suitable mix headroom. No mastering limiter is applied to individual stems. `metadata.json` records per-stem peaks, hashes, settings, timing and reconstruction error.

## Presets

- Fast: a single htdemucs pass.
- Balanced: htdemucs_ft, a fine-tuned model bag. Conservative CPU default; `--device mps` is explicit.
- Best (experimental): BS-RoFormer vocal estimate → mixture minus vocals → htdemucs_6s → merge secondary vocals into VOCALS, guitar+piano+other into MELODY, bass into BASS, drums into RHYTHM → residual projection.

“Best” is a candidate configuration name, not a measured quality ranking. The default vocal runtime is MLX. `--vocal-backend audio-separator` uses the alternative runtime. `--model`, `--vocal-model` allow registered checkpoint choices. The Spleeter adapter always uses its 4-stem baseline.

## Residual tradeoff

Default melody projection adds `mixture - sum(estimates)` to MELODY. It preserves all mixture information to float tolerance and leaves vocal/bass/drum estimates intact. It can put remaining vocal or drum leakage into MELODY. Exact reconstruction does not prove low bleed or good separation. `energy` distributes the residual by instantaneous source energy and can modulate quiet material; `none` preserves raw estimates for comparison. Benchmark all policies before selecting a production default.

Second-stage missed vocals are added to VOCALS. That can recover vocal layers, but can also add instrumental leakage. Separating an instrumental residual differs from the training distribution; staged quality must be tested against single-pass models. Waveform `EnsembleBackend` supports explicit weighted full-source members in the Python API. Phase differences and correlated errors can make averages worse.

## Persistence and execution

Output folder is a SHA-256 key over input bytes, settings, adapter/runtime/audio-library versions and engine implementation source. Loaded checkpoint fingerprints are also recorded in metadata. Each contains exactly four WAV files plus metadata. Same-input jobs serialize via an OS advisory lock. A temporary sibling directory is verified and renamed atomically; exceptions/cancellation publish no partial set. Cache hits check all output hashes and frame headers. A corrupt set fails explicitly instead of being silently reused. Model artifacts are cached by underlying libraries separately.

Callbacks announce decode, extraction/separation, write, cache hit and completion. Model logs report downloads/inference. No fabricated percentage or ETA. Python API is synchronous; hosts can run it in a worker process. No network service or HTTP authentication surface is introduced. Each engine job loads whole-song arrays; long files/high-memory models require enough RAM. Cancellation is process/KeyboardInterrupt based, not a persisted asynchronous job queue.

Global model runtime state (random seeds, GPU caches) means application hosts should use separate processes for parallel jobs. GPU kernels may not be bitwise deterministic across versions/devices even with a fixed seed. Filenames/cache identity are deterministic; cross-device bitwise sample equality is not promised.
