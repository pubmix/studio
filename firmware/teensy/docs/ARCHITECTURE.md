# Architecture

## Ownership and execution

`App` owns Project, Session, Settings, Status, contextual file intent and bounded History. `AudioEngine` owns Transport, StemPlayer, DSP state, Jam voices, one-shots, preview and meters. Project is creative state; Session is navigation state; Settings is device policy. The DSP contains no UI naming or external-library dependency.

All model mutations, filesystem work, decoding and DSP generation happen in the **foreground** in this prototype. On Teensy, the foreground fills a four-slot single-producer/single-consumer `AudioQueue` (three usable 128-frame blocks). The AudioStream interrupt only copies ready samples to Teensy Audio blocks. Acquire/release atomics publish complete blocks. The foreground never edits buffers owned by the interrupt.

This avoids SD calls, allocation, serialization, undo snapshots, and user-interface work in the audio interrupt. It is a prototype compromise: blocking foreground work can exhaust the roughly 8.7 ms queue at 44.1 kHz. A production scheduler needs measured SD read-ahead, budgeted UI work, nonblocking save jobs and performance counters. There is no claim that arbitrary SD cards can sustain it.

`AudioEngine` resides in Teensy RAM2 (`DMAMEM`) because its four fixed delay buffers use 262,144 bytes. Control state/history remains in RAM1. No external PSRAM is required for the demonstrated short-delay graph. This arrangement was verified by compilation and memory reports, not physical latency tests.

## Hardware convergence

The [shared baseline](../../../docs/SYSTEM_ARCHITECTURE.md) maps this prototype to H0.1 Teensy + CM5. IDisplay is the local test view; the rich UI belongs to CM5. The planned UART/USB semantic link, card service, power HAL and PSRAM allocator do not exist yet. Keep BoardConfig disabled until wiring/driver review. Hardware memory reservations assume a future delay-buffer refactor; they are not additive to current RAM2 delays.

## Modules

| Module | Responsibility |
|---|---|
| Model | Canonical roles, projects/tracks/Sounds/patterns/FX/macros/settings/status |
| App | Mode transitions, controls, contextual actions, undo, save/recovery |
| Transport | Integer sample-driven musical clock, 96 PPQN, 4/4, MIDI-clock ingress |
| StemEngine | Prepared-set API, structural validation, one shared playback cursor |
| AudioEngine | Mix graph, separate preview bus, metronome, record-loop transition |
| Dsp | Replaceable IDsp API, six small prototype effects, limiter and slots |
| Jam | Eight voices per track, scales, event loops, one-row 16-step patterns |
| Persistence | Explicit little-endian versioned/checksummed serialization |
| Journal | Two-slot recovery over a potentially interrupted blob writer |
| Presets | Typed Sound and FX persistence using the project envelope |
| Controls | Button debounce, quadrature decoding, fader calibration |
| Ui | Semantic themes and a shared text-oriented display model |
| Hal | Controls, display, MIDI, storage, audio-assets and board configuration interfaces |
| Host | Cached WAV reader, host files, safe project-root paths, text display |
| TeensyAdapter | Four-channel AudioStream bridge (Main L/R, Headphone L/R) |
| SdAdapter | Fixed-capacity WAV assets, foreground SD reads, journal backing storage |

## UI architecture

All eight themes share coordinates, text, navigation and layout. Tokens change background, text, accent, muted, danger and font intent. A real display adapter must map font intent to fonts and draw touchscreen widgets with hit testing. `IDisplay` currently renders labels only. The source is an executable view skeleton, not a pretend finished graphical frontend.

Home contains DUB/JAM/DAW/FILES/SETTINGS. Music mode persists while system areas are open. A switch between Dub/Jam clears Jam voices; stems pause consumption while Jam is active and resume their source cursor upon return. This continuation behavior is a test policy awaiting product review; there is no beat-warping or seamless cross-mode transition engine.

DAW has a clean shell and `DawRegion` data boundary only. No desktop-style arrangement workflow has been invented.

## Extending DSP

Implement `IDsp::process(Stereo, const Fx&)` and `reset()` for a Faust, custom, DaisySP or other algorithm. Adapters must return a wet-only signal, allocate outside processing, have bounded memory/time, and preserve time-based state while receiving zero input. The in-tree audio engine currently owns four concrete `StudioDsp` objects; replacing that container with preallocated implementation instances is the graph integration point. UI and saved product effect IDs should remain STUDIO names.

Faust/DaisySP/Signalsmith/chowdsp are extension boundaries only; their implementations, licenses and performance have not been evaluated or bundled. The six actual prototype algorithms are not marketed as substitutes for their full professional versions.

## Error behavior

Invalid imported sets fail before Dub activation. Missing one-shot assets fail triggering. Unknown project versions/corrupt payloads are rejected without replacing the current project. Save failure retains dirty state. Audio underruns produce silence, preserve lane alignment and increment counters. Unsupported hardware services are false/disabled, not reported as operational.
