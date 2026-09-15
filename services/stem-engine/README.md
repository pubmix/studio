# STUDIO Stem Engine

Local stem preparation for STUDIO, built for Apple Silicon Mac.

Every successful job produces **exactly four synchronized FLOAT32 WAV stems**, in this order:

| Lane | Stem | File |
|---|---|---|
| 1 | VOCALS | vocals.wav |
| 2 | MELODY | melody.wav |
| 3 | BASS | bass.wav |
| 4 | RHYTHM | rhythm.wav |

MP3/WAV input; sample rate, decoded length and mono/stereo channels preserved. Metadata accompanies the four audio files. Model internals never add a user-facing “other” stem.

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

This project does not include Teensy firmware, real-time DSP or the full STUDIO UI.

## Firmware integration boundary

These are FLOAT32 master stems. The existing firmware transfer profile requires 44.1 kHz stereo PCM16 and a separate manifest. A transfer exporter remains to be implemented; do not feed these masters directly to the firmware importer. See [integration notes](docs/REPOSITORY_INTEGRATION.md).
