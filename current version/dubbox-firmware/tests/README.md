# WAV player regression tests

Host checks for stereo audio-memory exhaustion. Run `sh run_audio_memory.sh`
from this directory. The script compiles the production player with C++17 and
address/undefined-behavior sanitizers. `stubs/` supplies deterministic audio
allocation and an in-memory WAV file; these are not hardware simulations.

Validated: allocation failure during WAV parsing and normal playback, recovery,
valid seek metadata, stop without double release, and no outstanding blocks.
The unpatched handoff crashes; the patched player passes. Hardware testing,
real SD timing and speaker output remain separate checks.

## Flash-sample cursor

`sample_playback.cpp` compiles the production `sample_playback.h` cursor using
C++17, `-Isrc`, and address/undefined-behavior sanitizers. It checks root-note and
octave pitch increments, linear interpolation at signed PCM extremes, invalid
metadata, samples longer than 65,535 frames, end-of-buffer silence, the short
hat choke ramp, allocation-failure skipping, and absence of heap allocations.
The host cursor is 40 bytes. It does not model AudioStream scheduling, flash-cache
latency, six-voice envelopes, mixer headroom, or subjective quality; those require
firmware integration and device tests. The compiled sample bank is licensed and
tracked separately in `src/samples/README.md`.
