# STUDIO interface contracts
Revision 0.1 • Product invariants LOCKED; proposed protocol/profile choices RECOMMENDED until accepted in Integration.

## 1. Ownership contract

| Workstream | Owns | Must request coordinated revision for |
|---|---|---|
| System EE | Parts, pins, rails, buses, processor topology and interface versions | Any change affecting more than one subsystem |
| Stem Engine | Models, separation quality, native result metadata and preparation inputs | Canonical role count/order or media semantics |
| Teensy Software | Real-time DSP integration, transport, control meaning, drivers/HAL, card service | Pins, voltage assumptions, audio format/profile |
| Audio + DSP | Algorithms, converter/analog comparison, CPU/RAM/latency evidence | Channel count, clock topology or capacity commitments |
| Display + Controls | Display/stylus qualification, rendering, physical part feel | Fixed lane layout, bus/pin reservations, power envelope |
| Integration | Compatible release manifest, migrations, tests and accepted changes | Locked product behavior |

No workstream may silently reinterpret a RECOMMENDED value as a product LOCKED decision. An interface change names the old/new version, reason, impacted owners and migration/test evidence.

## 2. Native Stem Engine result: preserve existing work

Observed local source, 14 Sep 2026: outputs/studio-stem-engine/docs/ARCHITECTURE.md and studio_stem_engine/engine.py in the sibling workspace. Native metadata uses schema **studio.stems.v1**, role entries named **name**, FLOAT32 WAV, source sample rate, source mono/stereo count, exact shared frame grid and hashes. It can preserve peaks over 0 dBFS. Output is an atomically published complete set.

LOCKED: exactly four synchronized files, vocals.wav, melody.wav, bass.wav, rhythm.wav in VOCALS / MELODY / BASS / RHYTHM order. No independent trims, normalization, time stretching or model-specific public categories. A silent category still has a correctly sized file.

Native FLOAT32 results remain canonical archival outputs. Do not force the ML engine to discard precision to match an early firmware reader.

## 3. Preparation adapter: required bridge

Current firmware tools/validate_prepared.py expects api_version=1, positive integer set_id, 44,100 Hz, stereo PCM16, zero alignment offset and entries called **role**. This is not the native ML schema.

Recommended profile **STUDIO-P1**:

1. Verify native role order, hashes, common frame count/rate and finite sample values.
2. Decode once; duplicate mono to L/R (do not invent stereo widening).
3. Resample all four stems using the same filter, phase convention and shared output frame count; compensate common filter delay. No independent automatic leading/trailing silence removal.
4. Choose **one common gain** for the entire set. For example measure peak of every stem and reconstructed sum, then use g=min(1, 0.8913/max_peak) for a nominal −1 dBFS preparation ceiling. Store g and original peaks; preserve relative balance. This is a proposed conversion policy, not mastering or separation.
5. Dither once when quantizing to PCM16; never clip individual stems silently. Preserve FLOAT32 originals.
6. Write the four P1 WAVs plus firmware-compatible manifest to a temporary set; verify headers, decoded byte lengths and hashes; publish only on complete success.
7. Additional provenance can live in a preparation sidecar until firmware explicitly supports it. Do not change the existing validator schema invisibly.

Future **P2** = 48k stereo PCM24 files, same synchronization rules, negotiated new profile/version. It requires a reader, driver and test change; hardware capability alone does not enable it.

Existing numeric set_id should be allocated through a durable collision-free registry; also retain the native SHA-256 content key in preparation provenance. Do not truncate a hash into an integer without collision handling.

## 4. Audio/render contract

- Four lane sources are stereo, plus explicitly budgeted samples/loops, ADC input and independent preview.
- Output order: MAIN_L, MAIN_R, HP_L, HP_R. Input order: LINE_L, LINE_R.
- 128 sample frames per render block in P1. Interleaving, slot width, signedness and channel mapping belong to a named AudioProfile.
- Teensy owns a monotonic 64-bit audio frame counter and stream_epoch. Start/seek applies to all four stems at one shared frame index.
- Rendering consumes prepared blocks in RAM. Disk reads, XML/JSON parsing, allocation, UI and ML never execute in an audio callback.
- Fader gain and FX-tail state are independent. Effect replacement must have a bounded tail/crossfade policy; no implicit state reset from a UI redraw.
- Output limiter/gain and headphone mute are independent. Preview cannot reach main by default.
- Maximum voices/effects/recording streams are capabilities, not unbounded dynamic promises. Reject an unsupported graph before activation.
- Underflow: one shared fault position; fade the affected prepared set together and stop/rebuffer it. Never resume individual stems at different times or repeat stale buffers. Existing FX tails may decay.
- Reset/clock fault: hardware mute is asserted; reinitialization begins with silence.

## 5. AP ↔ Teensy control contract

Proposed STUDIO-LINK v1 over UART7, 1 Mbaud 8N1, COBS frames with zero delimiter. Little-endian header: protocol major/minor, message type, payload length (≤512 bytes), session ID, sequence number, stream_epoch and apply_at_frame; append CRC-32C. Exact byte offsets go into one shared schema before code integration.

HELLO/CAPABILITIES reports firmware build, board revision, media profiles, sample rate/block size, channel map, voice/FX limits and card readiness. AP requests state snapshot after every reconnect. Teensy is authority for performed controls and transport; AP is authority for editor document state until a revision is accepted.

Messages: GET_STATE, SET_PARAMETER, CONTROL_EVENT, TRANSPORT, PREVIEW_ASSET, LOAD_PREPARED_SET, SAVE_REVISION, POWER_STATUS, SHUTDOWN_REQUEST, ACK/NACK and HEARTBEAT.

- Reliable commands acknowledged with idempotent sequence IDs; retries cannot double-trigger notes or recording.
- Continuous fader/macro telemetry may coalesce; note-off, transport and fault messages must not be dropped.
- Separate bounded queues; reserve capacity for release/stop/fault traffic. Advertise command credits.
- Heartbeat every 250 ms, disconnect declared after 1 second initially. UI restart must not restart audio.
- AP-origin notes use explicit source/voice IDs; release their sustained voices on disconnect.
- Scheduled commands use audio-frame time from periodic round-trip clock estimates; AP timestamps are not sample-clock truth. Late commands execute at next safe block and report lateness.
- Target physical press-to-audio ≤10 ms and AP-touch-to-audio ≤20 ms at the 99th percentile, subject to display qualification; these are acceptance targets, not measurements.

At 100,000 B/s UART line capacity, 1,000 events/s ×32 bytes uses 32%. Metering is 20–30 Hz, not sample-rate traffic. Bound commands so >50% steady line utilization prompts coalescing/backpressure.

## 6. USB/card service

Use USB bulk (initially a framed CDC transport is acceptable) for file chunks, checksums and snapshots. USB packet boundaries are not application message boundaries. Suggested chunks 32 KiB, transfer credits two chunks initially; receiver advertises actual capacity. Two staging buffers account for 64 KiB of RAM2 USB budget.

Only Teensy opens/mutates its card filesystem. Operations: list/read/info; BEGIN_UPLOAD, WRITE_CHUNK(offset, checksum), COMMIT_SET and ABORT. Limit path length, reject traversal/absolute paths, bound sizes and check free space. Commit verifies final hashes and compatible media before directory publication. Disconnect leaves an invisible temporary object, never a half-ready stem set.

FAT rename alone is not a power-loss transaction. Use a CRC-protected generation journal, flush data then manifest then journal, retain prior committed generation, and scan/recover after brownout. Same rule applies to recordings; recover a known valid frame length and rebuild a WAV header.

During performance, allow cached listings/status and in-RAM parameter changes. Defer bulk imports, deletion, defragmentation and model preparation. Preview of a newly imported file requires conversion and staging; preview of an existing prepared card asset may stream through the dedicated HP route within the admitted I/O envelope.

USB-C PC device mode is proposed via CM5 native USB2, presenting MIDI and controlled file-transfer services. It must be qualified on the chosen OS; if unavailable, the first prototype uses removable card transfer and service USB, with PC integration explicitly incomplete. Do not export a live mounted card as USB mass storage.

## 7. HAL handoff for existing firmware

Preserve existing IControls, IMidi, IAudioAssets and IStorage concepts, adding asynchronous/block interfaces where needed:

- BoardProfile = pin map revision, electrical polarities, codec clock profile.
- AudioDevice = configure(profile), render/consume block, report xrun, hardwareMute.
- AssetStreamer = prefetch, acquireReadBlock, releaseReadBlock; never per-sample filesystem calls.
- ControlScanner = normalized lane event + frame timestamp + source ID.
- TransportClock = frame index, epoch and musical phase; no millis()-only transport authority.
- StorageService = asynchronous request, completion and durable generation.
- UiLink = semantic state/events. The existing IDisplay label/clear stub is not a rich remote rendering protocol.
- PowerManager = source contract, pack health, charge policy and staged shutdown.

Current ControlEvent uses timeMs: retain compatibility but add an audio-frame timestamp at the realtime boundary. Current StudioAudioStream converts to PCM16: retain P1 or replace deliberately for P2. BoardConfig defaults must remain disabled until an actual wired board is reviewed.

## 8. Power/update contract

CM5 requests shutdown, Teensy stops new recording, flushes/journals card state, mutes outputs and waits for AP storage sync before releasing the latch. Failed AP shutdown times out to a documented recovery path. Charging safety does not depend on the AP being alive.

AP updates use signed/versioned A/B system images with rollback. MCU update requires idle transport, stable external power or sufficiently qualified battery, image/board-ID verification and hardware mute. Prototype uses supported Teensy Loader with accessible Program button. MCU dual-bank rollback is **not** assumed from the board: production boot/update implementation remains an explicit task. Bundled release manifest records compatible AP/MCU/profile/schema versions.

