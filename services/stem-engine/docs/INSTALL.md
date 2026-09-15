# Installation and local operation

Test host: Apple Silicon arm64, macOS 15.0; Python 3.11.15 in an isolated environment. No system Python packages were changed. In this repository, run `scripts/setup` from `services/stem-engine` to create `.venv`.

## Install and run

From `services/stem-engine`:

```
scripts/setup
scripts/studio /absolute/path/song.mp3 --preset fast --output /absolute/path/prepared
scripts/studio /absolute/path/song.wav --preset balanced --output /absolute/path/prepared
scripts/studio /absolute/path/song.wav --preset best --output /absolute/path/prepared
```

The launcher sets model caches under this component's .models directory so downloaded weights are reused. First model use downloads public weights; once cached, audio processing is local. No audio is uploaded by these adapters. Demucs/Hugging Face may still make metadata checks unless `HF_HUB_OFFLINE=1` is set. The engine does not require an account or paid API.

To run elsewhere, copy this project directory, install `uv`, and run `scripts/setup`. This recreates the recorded dependency snapshot in `.venv`. It may require Xcode command-line tools for native dependencies. `scripts/studio` then selects `.venv` automatically. A fresh .venv installation defaults to this project’s .models directory. Set TORCH_HOME, HF_HOME and STUDIO_MODEL_DIR to override model cache locations.

Minimal CPU installation can omit MLX:

```
uv venv --python 3.11 .venv
uv pip install --python .venv/bin/python -e '.[demucs,separator,test]'
```

FFmpeg is bundled through imageio-ffmpeg. `scripts/setup` adds its executable to the environment. The engine's MP3 fallback uses the bundled path directly; audio-separator checks the command search path too.

## MLX SDK repair used on this Mac

The first native `mlx-audio-io` build failed with `cstddef` not found. This succeeded using an existing SDK; no system SDK was edited:

```
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk \
CXXFLAGS='-isystem /Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk/usr/include/c++/v1' \
uv pip install --python .venv/bin/python 'mlx-audio-separator[convert]==0.1.5'
```

Use a valid SDK installed on your own Mac rather than blindly assuming this path. Sandboxed execution initially could not access Metal; the approved local GPU check and inference did succeed outside the sandbox. That restriction does not imply the Mac lacks a GPU.

## Spleeter baseline

A subprocess adapter is implemented. Set STUDIO_SPLEETER_PYTHON to a separate compatible environment and use `--backend spleeter --preset fast`. The main environment intentionally does not depend on TensorFlow. Standard installation attempts failed here: Python 3.11 encountered llvmlite's `<3.11` constraint; Python 3.10 encountered a legacy numba build failure. Therefore this adapter is unverified with real Spleeter inference on this host. No baseline quality result is reported.

## Verification

```
python -m pytest tests -q
scripts/studio --verify /absolute/path/prepared/CONTENT_HASH
```

`--device mps` applies to PyTorch models; the MLX runtime always uses its Apple GPU path. It is not a CPU fallback. If acceleration fails, explicitly choose the Demucs CPU or audio-separator CPU adapter. Models and package environments use gigabytes of disk space; no automatic cleanup is performed.
