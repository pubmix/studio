# Verification results

Verified on this Apple Silicon Mac (macOS 15.0). **20 automated tests passed.** Package compatibility check passed for all 82 installed distributions.

Real pretrained model inference completed on synthetic audio. Every listed result was reopened and checked for exactly four canonical float WAV files, equal sample grids, finite samples, file hashes and independent mixture reconstruction. These results do not measure music separation quality.

| Run | Rate / channels | Wall seconds¹ | Max reconstruction error |
|---|---|---:|---:|
| [candidate / balanced](runs/balanced-682f40f79ab6.json) | 44100 Hz / 2 | 128.54 | 1.86e-09 |
| [candidate / best-final](runs/best-final-4d8db6d9c286.json) | 44100 Hz / 2 | 58.64 | 3.32e-09 |
| [candidate / mdx](runs/mdx-34169f8c8158.json) | 44100 Hz / 2 | 104.60 | 4.66e-10 |
| [candidate / mel-band](runs/mel-band-5637d6898d90.json) | 44100 Hz / 2 | 506.94 | 3.23e-09 |
| [mdxc-smoke](runs/mdxc-smoke-79ad81e2e589.json) | 44100 Hz / 2 | 475.60 | 3.73e-09 |
| [mlx-smoke](runs/mlx-smoke-499493787f4f.json) | 44100 Hz / 2 | 27.81 | 1.86e-09 |
| [mlx-staged-smoke](runs/mlx-staged-smoke-6921073563ea.json) | 44100 Hz / 2 | 93.04 | 3.32e-09 |
| [mp3-mono-smoke](runs/mp3-mono-smoke-4337e3df5be9.json) | 48000 Hz / 1 | 9.05 | 3.7e-09 |
| [mp3-mono-smoke](runs/mp3-mono-smoke-b28cf73904bc.json) | 48000 Hz / 1 | 9.00 | 3.7e-09 |
| [smoke-stems](runs/smoke-stems-66eda1a01112.json) | 44100 Hz / 2 | 32.60 | 1.86e-09 |
| [staged-smoke](runs/staged-smoke-d7e06bbf154b.json) | 44100 Hz / 2 | 452.87 | 2.44e-09 |

¹ Includes initialization and, for some runs, downloads/conversion; cache state, shifts and runtime differ. **Do not use this table to rank speed or quality.** Full settings and implementation fingerprints are in each metadata file.

## Coverage

- 12-second stereo synthetic fixture: direct Demucs, staged BS-RoFormer plus six-source Demucs, MLX Demucs and MLX BS-RoFormer, Mel-Band RoFormer, MDX, MDX23C, and the fine-tuned Balanced bag.
- Real MP3 inference at 48 kHz mono, including restoration from the model sample rate; the original decoded length is retained.
- Final CLI cache-hit check passed with offline mode enabled.
- Tests cover MP3 decoding, six-source melodic recombination, missing/unknown sources, corrupted output caches, malformed metadata, canonical names, failure cleanup, same-input process locking, mono/stereo silence, rate round-trips, ensemble weights and metric behavior.
- The benchmark harness completed an uncached-output Fast CPU run and recorded no quality score because references were absent. See [benchmark smoke report](benchmark-smoke.json).

## Limits and failed attempts

- Spleeter installation failed on Python 3.11 (legacy llvmlite constraint) and Python 3.10 (legacy numba build). Its subprocess adapter is unverified; there is no Spleeter score.
- MLX initially failed to compile a dependency. Selecting the existing SDK headers repaired it. GPU access failed inside the sandbox, then succeeded with approved local execution.
- No representative songs/multitrack ground truth were available. No listening comparison, SDR-based winner, production quality claim, or perfect semantic isolation is asserted. Best remains experimental.
- Full-song memory/throughput, broad checkpoint compatibility, all macOS versions and independent device parity remain untested.
- Checkpoint fingerprints are recorded after loading. If replacing weights under an unchanged model name, use a new output root to avoid reusing an older valid output cache.

See [test output](test-results.txt), [machine-readable summary](verification-summary.json), [benchmark protocol](BENCHMARKING.md) and [research sources](RESEARCH.md).
