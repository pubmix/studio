# Actual validation results

## Desktop

- Apple Clang 16.0.0, C++17, ARM64 Mac, optimized build: **passed**.
- `make test`: **5,153 checks across 11 test groups passed**.
- AddressSanitizer + UndefinedBehaviorSanitizer build: **5,153 checks passed**, no sanitizer findings reported.
- Python unittest import suite: **8 tests passed**.
- Prepared demo set: four stereo PCM16/44100 Hz WAVs, 176,400 frames each; manifest validator returned Ready.
- Interactive command sequence exercised real-WAV playback, FX enable/intensity, dry fader cut, Save, Undo, Redo, Load and Jam synthesis. Process exit 0. See `SIMULATOR_LOG.txt`.
- That scripted run generated five seconds of audio frames in roughly 0.49 seconds of host elapsed time, including process and filesystem work. This is a simulator smoke measurement, **not a Teensy CPU benchmark**.

Test groups cover sample clock/count-in/tap/MIDI pulses, all-lane playback positions, invalid set metadata/order/offsets/rate, delay tails, intensity curves, preview/main isolation, stereo limiting, full one-shots, App mapping/history/context, Jam synthesis/loop capture, project corruption/truncation, safe file roots and operations, theme layout invariance, queue exhaustion/order, interrupted journal writes, normalizers, preset type checks and actual PCM WAV decoding. Python tests cover valid input plus missing, reordered, misaligned, wrong-rate, wrong-count, hash-failed and truncated transfers.

Test counts include per-frame assertions, not 5,153 distinct product scenarios. No UI screenshot or hardware test is implied.

## Teensy toolchain

Official `teensy:avr` **1.62.0**, supplied GCC **15.2.1**, target `teensy:avr:teensy41`. Bundled Audio 1.3, SD 2.0.0, SdFat 2.1.2, SPI 1.0, Wire 1.0, SerialFlash 0.5.

The default serial diagnostic build and optional SD/I²S configuration both compiled and linked. The optional configuration was verified with the platform's actual `build.flags.defs` property; `build.extra_flags` is ignored by this Teensy platform and is not used by the delivered build script.

Final memory reports are saved in `TEENSY_MEMORY.txt`. The SD/I²S build used:

| Region | Reported allocation | Remaining |
|---|---:|---:|
| Flash code/data/headers | 168,956 bytes | 7,957,508 bytes reported free for files |
| RAM1 variables | 153,216 bytes | 240,000 bytes free for local variables after code/padding |
| RAM1 code + padding | 131,072 bytes | Included above |
| RAM2 variables | 281,984 bytes | 242,304 bytes free for malloc/new |

These are linker/tool reports. Runtime high-water stack, interrupt latency, actual CPU utilization, cache effects and audio fidelity are unmeasured. Delay buffers reside in RAM2; the original RAM1 layout exceeded the memory limit and was corrected before the successful build.

## Fixes found during validation

- Mac SDK C++ header discovery required an explicit installed-header search path.
- Teensy's `uint32_t` type required an explicit type on the loop-length `max` operation.
- Floating clock accumulation delayed exact count-in completion; replaced by integer accumulation at 0.001 BPM resolution.
- Moved DSP state to RAM2 to satisfy actual memory constraints.
- Confirmed the platform-specific compiler flag for the optional hardware-adapter build.
- Journal tests exercise retaining the last valid save after a truncated write; WAV tests reject truncated payloads.

## Limits of this evidence

No Teensy or STUDIO hardware was flashed or played. No codec, touchscreen, headphones, physical MIDI, control pins, SD card throughput, audio quality, power interruption or firmware update was hardware-validated. CMake configuration is provided but CMake was unavailable on this Mac; Make was used. The full graphical UX, production DSP/instruments, mixed-output recording, Sound Pak browser and final hardware mappings remain open as detailed in `SCOPE.md`.
