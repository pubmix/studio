# Cohesion audit — 16 September 2026

## Result

The three workstreams now share an explicit architecture and executable media boundary. Hardware remains a proposal and the firmware remains a prototype; this audit does not certify physical integration.

## Corrections

- Added the native FLOAT32-to-P1 preparation component to the shared repository change.
- Unified ownership of native separation, preparation, real-time audio, card, UI and electrical interfaces.
- Aligned the transfer schema and validator with the firmware's uint32 set ID; reject boolean integer fields and invalid explicitly supplied hashes.
- Reconciled the 12 ms debounce / ≤10 ms target mismatch and encoder transition assumptions.
- Flagged the hardware target RAM2 budget's incompatibility with the prototype's existing 256 KiB delay allocation until PSRAM relocation.
- Distinguished actual silence-and-advance underflow behavior from proposed fade/stop/rebuffer.
- Distinguished four logical audio channels from the sketch's main-only physical I2S connection.
- Preserved explicit outstanding catalog/transfer, UI/link, power, recording and long-track preparation work.
- Linked component READMEs and contributor/agent instructions to the shared baseline.
- Added a cross-component CI job; its GitHub run is separate from local results.

## Verification performed

- Compared 40 firmware/desktop/native-engine source and test files byte-for-byte against GitHub hardware/architecture-v1 at e659cb1fa85e80e202490f80a8615fd94fee69bc: all matched.
- Firmware: 5,153 C++ checks across 11 groups passed.
- Firmware import: 11 Python tests passed, including new ID/type/hash regressions.
- Stem Engine: 20 existing tests passed.
- Preparation: 19 tests passed.
- Cross-component: four tests passed, including both mono and stereo through real Engine publication (synthetic backend), preparation, firmware validation and C++ simulator playback.
- 35 unique assigned edge pins and seven reservations cover all 42 edge pins; media byte-rate/role/profile checks pass.
- The native fixtures' bytes remain unchanged; prepared corruption is rejected.
- No C++ driver/DSP changes, model downloads, real-song tests, hardware measurements or KiCad work were performed.

## What remains before an integrated instrument

See SYSTEM_ARCHITECTURE.md. Most importantly: manifest-aware catalog and transfer, long-track preparation (current ~95-second 44.1k stereo bound), real ADC/dual-DAC bridge, PSRAM/read-ahead and fault policy, CM5 frontend/protocol, qualified controls, and power/charging drivers/benchmarks. These are shared release gates rather than conflicting assumptions.
