# STUDIO shared architecture and integration baseline

Revision I0.1 — 16 September 2026. This is the cross-workstream authority for compatibility and implementation status. Hardware H0.1 remains a **recommended, unbuilt design**; P1 is the implemented transfer profile. A planned feature is not operational simply because it appears here.

## Product and ownership

LOCKED: dedicated four-lane instrument, touchscreen above four lanes, each ordered TOP BUTTON → PUSH ROTARY ENCODER → VOLUME FADER → BOTTOM BUTTON. Exactly VOCALS, MELODY, BASS, RHYTHM in that order. Silent categories remain present.

| Component | Owns | Current status |
|---|---|---|
| Stem Engine | Decode, separation, semantic recombination, native alignment and balance | Local Mac Python; native FLOAT32 results |
| Preparation adapter | Shared resampling/gain, P1 encoding, manifest and persistent set IDs | Offline host prototype |
| Teensy firmware | Musical clock, performance DSP, physical controls and card | Portable C++/diagnostic sketch; board drivers disabled |
| System EE / Hardware | Electrical architecture, parts, pins, power and clocks | H0.1 proposal; no schematic or bench validation |
| Application processor | Rich UI, editor/file jobs and optional ML | CM5 8 GB recommended; UI/link not implemented |

Prototype preparation runs on the Mac. CM5 is the recommended application processor, not a validated separator. MLX is Apple-specific. Teensy never runs ML. Teensy owns microSD; CM5 owns eMMC and must not mount the Teensy card.

## Compatible media path

MP3/WAV → Mac Stem Engine → native FLOAT32 set → preparation adapter → P1 media → validator → host demo / future registry-and-card transfer service → Teensy DSP → independent main/headphone converters.

| Boundary | Contract |
|---|---|
| Native | metadata.json; studio.stems.v1; name fields; source rate/mono-stereo/frame grid; four FLOAT32 WAVs and mandatory SHA-256 hashes |
| Prepared | manifest.json; api_version 1; role fields; 44100 Hz stereo PCM16; equal positive frames; zero alignment_offset_frames |
| Identity | Positive uint32 set_id in one persistent library registry; preserve native content key separately |
| Conversion | Same zero-phase polyphase filter/grid; ceil(N × 44100 / source_rate); duplicate mono; no independent trimming |
| Gain | One common gain considering individual stems and reconstructed sum; no boost; nominal -1 dBFS ceiling; TPDF dither |
| Preservation | Native source files remain byte-identical |
| Audio | Float DSP; 128 frames/block; MAIN_L, MAIN_R, HP_L, HP_R |
| P2 | 48k/PCM24 remains OPEN; requires versioned reader/driver changes |

Legacy v1 hashes remain optional; the adapter always emits/requires them. The schema and firmware validator enforce the uint32 set ID limit. Actual audio headers/payloads still require media validation. Independent registries are separate ID namespaces; never merge them directly or recreate a registry beside existing sets. Set IDs and asset IDs are separate.

## Current implementation versus H0.1 target

| Area | Current | Target / unresolved work |
|---|---|---|
| Scheduling | Foreground renders; ISR copies queued blocks; 3 usable blocks (~8.7 ms) | Measured read-ahead and nonblocking foreground jobs |
| Outputs | Four logical channels; sketch only connects Main stereo | Second DAC on TX32, ADC RX8, quad/input driver and mute sequencing |
| Clock | Nominal 44100 Hz / 128 frames | Teensy sole clock master; measure actual PLL/LRCLK and converter ratios |
| Memory | Four delays consume 262144 bytes in AudioEngine RAM2; no PSRAM use | H0.1 delay/ring capacity needs PSRAM allocator and relocation; its RAM2 budget cannot be added to current delay buffers |
| Underflow | Silence all lanes for failed frame, advance shared cursor | Proposed fade/stop/rebuffer behavior is not implemented |
| Buttons | Fixed 12 ms debounce | 3–5 ms target needs qualification; ≤10 ms response cannot be claimed today |
| Encoders | Four transitions/detent | Candidate 15-pulse/30-detent part needs detent-phase/transition calibration |
| Faders | Normalized events; calibration 0..1023 | ADC14–17; calibrate actual ADC resolution; ≥10 effective bits needs measurement |
| Pins | Generic HAL unassigned; diagnostic switches off | H0.1 reservations are proposed data, not a driver |
| UI/link | Text view only; no AP link | CM5 frontend, UART/USB protocol/capabilities and state recovery |
| MIDI | Note/clock abstraction | UART1 electrical interfaces, USB host/device drivers |
| Catalog | Host/SD demo uses set 1 and assets 100–103; no manifest parsing | Production importer must preserve manifest IDs and resolve project references |
| Recording | Jam event loops; Dub recording state only | Audio recording-to-SD/recovery absent |
| FX tails | Fader-independent returns; disabled sends let state decay | Effect-switch crossfade/tail handoff absent |
| Power | No board power driver | Pack policy, charge-disable, mute and shutdown sequencing require implementation/bench work |

These are explicit implementation gaps, not competing architectures. Do not claim the production target has been achieved.

## Electrical authority

Use [H0.1 pins/resources](../hardware/architecture-v1/03-PINS-AND-RESOURCES.md) and [power design](../hardware/architecture-v1/05-POWER-AND-PCB.md). Firmware and display work must not invent independent pin assignments.

SAI1 uses TX7/32, RX8, clocks20/21/23. Encoder pins10/11 prevent default SPI0; faders16/17 prevent default Wire1. Wire18/19 handles audio/power, Wire2 25/24 panel controls. UART7 28/29 carries semantic AP messages; internal USB carries future file service traffic, not required real-time audio. Twelve panel switches use the expander, eight encoder signals direct GPIO, four faders ADC. Separate DAC paths preserve independent headphone preview.

The [machine-readable baseline](../packages/contracts/system-v1/baseline.json) records media, lanes, pins and current status for automated tests. It is neither a netlist nor an enabled board driver. H0.1 parts/power values remain recommended pending measurements.

## Decisions and release gates

| ID | Status | Decision |
|---|---|---|
| I-01 | LOCKED | Fixed lanes and four canonical roles |
| I-02 | RECOMMENDED | Teensy + CM5 target; Mac preparation prototype |
| I-03 | IMPLEMENTED / TESTED | Separate native-to-P1 conversion service |
| I-04 | IMPLEMENTED / TESTED | uint32 ID bound consistent with C++ |
| I-05 | OPEN | Manifest-aware catalog, stable asset registry and transfer |
| I-06 | OPEN | Long-track preparation; current adapter bound is ~95 s at 44.1k stereo |
| I-07 | OPEN | Quad audio/ADC, PSRAM, nonblocking read-ahead and fault handling |
| I-08 | NEEDS BENCHMARK | Controls, SD/DSP latency, analog quality, power/charging |
| I-09 | OPEN | AP wire protocol, UI and power HAL |
| I-10 | OPEN | P2, onboard ML, audio recording and schematics |

Run make test, make simulator and make integration-test with PYTHON pointing at a compatible environment. The integration suite uses the real Engine publisher with a synthetic backend, preparation service, firmware validator and C++ simulator. No models or music downloads are required.

Before physical performance or schematic-release claims: implement catalog/transfer, qualify independent outputs/input, bind reviewed pins, reconcile actual memory placement, implement bounded streaming/fault policy, and measure control/audio/power behavior. Production readiness remains unproven.

Validation evidence: [cohesion audit](COHESION_AUDIT.md).
