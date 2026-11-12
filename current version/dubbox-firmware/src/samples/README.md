# Studio acoustic sample bank

Real piano and percussion recordings from the **Versilian Community Sample Library (VCSL)** by Versilian Studios LLC. Source repository and exact per-file URLs, original Git blob hashes, SHA-256 hashes and processing details are in `manifest.json`. Source revision: `c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e`.

## License and attribution

The primary repository [README](https://github.com/sgossner/VCSL) explicitly releases this collection under CC0, including use in commercial software. The exact [source license](https://github.com/sgossner/VCSL/blob/c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e/LICENSE) is retained as `LICENSE-VCSL-CC0.txt`. Attribution is retained as provenance, although CC0 does not require it. These are recordings, not generated substitute sounds. No MusicRadar or other no-redistribution material is included.

## Contents and firmware interface

- Five Kawai grand piano anchors at MIDI36,48,60,72,84 (C2–C6 in scientific pitch notation). VCSL filenames use an octave label one lower; actual pitch was checked by spectrum/autocorrelation. Each anchor is8seconds.
- Eight acoustic percussion rows in existing device order: bass drum, snare, handclap, closed hi-hat, open hi-hat, low tom, snare rimshot, tambourine. The bass drum is the VCSL Bass Drum1 recording, not a claim of a specific modern kick-drum model.
- Mono, signed16-bit PCM at44,100Hz. All drum recordings retain their full original tails, about0.74–5.81seconds after tiny onset trimming.
- Total PCM: **5,206,432 bytes**. Data definitions live in one `studio_samples.cpp`, explicitly marked `PROGMEM` for Teensy flash. The public `studio_samples.h` exposes `Sample { pcm, frames, sampleRate, rootMidi }`, `piano[5]` and `drums[8]`. A matching player must honor sampleRate, bounds and interpolation. No RAM copy of the sample bank is intended.

## Processing

The build script averages stereo channels, removes constant DC bias, removes only leading silence (retaining2ms before onset), and normalizes each sample peak to0.75. Native44.1kHz recordings are not resampled; the48kHz handclap is low-pass filtered with a257-tap20kHz Hamming-windowed sinc before conversion. A1ms attack fade and explicit zero endpoints avoid cuts. Piano recordings originally range8.07–36.11seconds; the first8seconds are retained with a1second fade at the end. Drums retain their original tails with a30ms endpoint fade. This is a one-velocity, five-anchor mono bank, not a complete multi-velocity stereo concert-piano library. No sustain loops are invented.

## Auditions

`piano-audition.wav` plays the five anchors low to high,3seconds apart, with overlapping decay tails. `drum-kit-audition.wav` plays the eight row sounds in order,3seconds apart. `piano-source-stereo-reference.wav` is the first8seconds of the original middle anchor, level-matched using half the device gain to preserve stereo headroom; `piano-device-mono-reference.wav` is its device sample at the same half gain. The unattenuated exact device sample is in `wav/piano_c3.wav`. These comparisons expose the mono/tail tradeoff rather than hiding it.

## Validation and status

Each original download matched the source Git blob SHA-1 and recorded SHA-256. Conversion checked all frame counts, rates, non-empty data, peak headroom, zero endpoints, piano root pitches and total size. Generated WAVs and PCM have hashes in the manifest. The sample files are prepared, but this document alone does not assert successful firmware linking, hardware playback, subjective listening approval, or selectable-kit UI integration. Final firmware build must verify flash fit and that the PROGMEM bank is not copied into DTCM/RAM. Device listening remains necessary.

## Reproduction

The acquisition and conversion scripts are retained alongside the sample source under `work/soundpacks` in the working project (`fetch_vcsl.py`, `build_bank.py`, `make_auditions.py`), with original verified WAVs in its `raw` folder. Python3 and NumPy are required. Run acquisition, then conversion, then audition generation. Re-running conversion verifies original SHA-256 hashes first. Keep source downloads out of the firmware repository; generated PCM, license and manifest can be redistributed under CC0.

Portable regeneration is also included in the firmware sample folder: run `python3 fetch_sources.py --cache /path/to/cache`, then `python3 build_bank.py --cache /path/to/cache --wav-dir /path/to/auditions`. The cache must be outside the firmware source tree. Only the thirteen selected files are downloaded; no full-library clone is needed.
