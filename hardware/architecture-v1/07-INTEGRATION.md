# Integration handoff, decision log and validation plan
Revision 0.1 • Phase A completed as an architecture proposal. Phase B is a plan, not a claim of hardware validation.

## 1. Handoff summary

Start from the [architecture](02-ARCHITECTURE.md), [pin map](03-PINS-AND-RESOURCES.md), [interfaces](04-INTERFACES.md), [power plan](05-POWER-AND-PCB.md) and [BOM](06-BOM.md). Integration should adopt a board/profile revision and schema versions together. Do not merge pin changes independently from the related driver and test changes.

Existing workstream snapshots inspected read-only:

- **STUDIO Teensy 4.1 Firmware**, task 01a0a3ae-2fb1-7d91-80f4-e90df1ab319f: BoardConfig.h explicitly disables I2S/SD until wiring review; Hal.h provides controls/MIDI/assets/storage/display boundaries. Demo manifest and validator enforce 44.1k stereo PCM16. TeensyAdapter.h emits four channels but quantizes them to PCM16.
- **Build STUDIO Stem Engine**, task 01a0a3a8-ebfd-7370-ad0a-5b0b5e01379a: native output is FLOAT32/source-rate/mono-or-stereo; metadata fields differ from firmware. MLX staged smoke ran on Apple Silicon with quality unvalidated. Whole-song arrays can require significant RAM.
- These are snapshots, not a promise that sibling files remain unchanged. Re-read them at integration, record hashes/build revisions and update the compatibility table.

No messages or code changes were sent to those workstreams.

### Concrete reconciliation list

| Conflict/gap | Resolution proposed | Owner and gate |
|---|---|---|
| Native ML media versus firmware import | Versioned preparation adapter; preserve original floats | Integration + Stem Engine/firmware acceptance |
| 16-bit output versus 24-bit converter capability | P1 remains 16-bit; P2 requires explicit driver/profile change | Firmware + Audio |
| UI abstraction only label/clear | AP renderer consumes semantic state/events | Display + firmware |
| Audio assets per-frame API may invite disk reads | Block-prefetch interface with real-time RAM ownership | Firmware |
| timeMs control timestamps | Add audio-frame timestamp/epoch at realtime boundary | Firmware |
| Default hardware unassigned | Adopt H0.1 only after bench wiring review | EE + firmware |
| AP ML runtime portability | CPU/CUDA/RKNN-specific backend deployment; no MLX assumption | Stem Engine supplies target runtime evidence |
| USB-C device-mode support | Qualify CM5 OS gadget/recovery path | System + firmware; no unsupported PC integration claim |

## 2. Decision log

| ID | Status | Decision / rationale | Evidence or reopening condition |
|---|---|---|---|
| D01 | LOCKED | Four fixed lanes and canonical stem order | Product P01/P02 |
| D02 | LOCKED | Existing FX tails survive lane fader-down | P05 |
| D03 | LOCKED | Headphone preview independently routable from main | P10 |
| D04 | RECOMMENDED | Teensy + CM5 8 GB/32 GB | Display/memory/runtime separation; ML benchmark can change AP |
| D05 | RECOMMENDED | Teensy exclusively owns performance SD | Preserves user Files requirement and restart resilience |
| D06 | RECOMMENDED | Dual stereo DAC + stereo ADC on SAI1 | Four outputs without new TDM implementation |
| D07 | RECOMMENDED | P1 44.1k PCM16 /128 frames for compatibility | Current validator/adapter; P2 OPEN |
| D08 | RECOMMENDED | Two 8 MiB PSRAMs, explicit memory caps | Storage/FX capacity; worst-case latency unmeasured |
| D09 | RECOMMENDED | USB bulk files + UART semantic controls | Separates bulk backlog from musical control |
| D10 | RECOMMENDED | 2S smart pack, 5.1 V rail, PD charging | Runtime/current tradeoff; pack fit and charging qualification |
| D11 | OPEN | Final display/stylus and mechanical components | Physical precision, wear, feel and power tests |
| D12 | NEEDS BENCHMARK | Simultaneous DSP/voice/loop capacity | No physical CPU/cache/storage timing data |
| D13 | NEEDS BENCHMARK | Onboard separation turnaround and memory | No embedded target run; TOPS not enough |
| D14 | OPEN | DAW workload, Jam overrides, retrigger/send behavior | Product definition required; not an EE invention |
| D15 | OPEN | Production boot/security/update implementation | Prototype Teensy Loader does not imply robust MCU rollback |
| D16 | RECOMMENDED | Modules first, custom carrier after bench gates | Avoids unnecessary DDR/BGA/boot risk |

## 3. Risk register

| Risk | Severity | Detection / mitigation | Owner |
|---|---|---|---|
| CM5 cannot run selected separator acceptably | High | Same-model cold/warm tests; RSS, RTF, energy, supported operators; benchmark Jetson alternative | Stem Engine + EE |
| Whole-song ML memory exceeds 8 GB | High | Test 4/10/30-minute files; process memory cap, graceful refusal; chunked backend or 16 GB option | Stem Engine |
| Audio callback overruns with ambitious FX | High | Profile worst observed block time/cache misses; capability admission before graph activation | Audio + firmware |
| SD long stalls corrupt recording or break sync | High | Two qualified card models, preallocation, large rings, fault-injection; common stem rebuffer | Firmware |
| Smart pack charged without required SMBus policy | High | Charge default disabled; simulator/EVM, manufacturer charge contract and thermal tests | EE |
| AP startup/load step browns out audio | High | Scope at rails under source/pack extremes; power gating and reserve | EE |
| Main output leaks headphone preview | High | Digitally identifiable test tones; measure all output paths and routing states | Audio |
| Fader-down or UI restart resets FX state | High | Recorded tail and AP-restart tests | Firmware |
| Active stylus expectations exceed panel capability | Medium/high | Finger chord tests and fine grid selection with intended stylus | Display |
| Controls wear/feel unsuitable | Medium/high | Cycle/force review and physical samples; replaceable control PCB | Display + mechanical |
| Shared pin/timer/cache ownership breaks drivers | High | Board profile, linker report, runtime allocation log; avoid inferred peripheral independence | Firmware + EE |
| Charger/display/USB noise enters audio | High | Noise spectrum battery vs charge/backlight/USB loads | Audio + EE |
| Brownout loses recent edits/recording | High | Generation journal, forced interruption, power reserve, recovery tests | Firmware |
| CM5 external USB device/recovery workflow incomplete | Medium | Enumerate MIDI/file services on target OS/hosts; preserve service recovery | System |
| Unspecified DAW/loop/polyphony workload expands scope | High | Explicit maximum workload before hardware release | Product + Integration |

## 4. Phase B: bench prototype, wiring and bring-up

### Bench equipment / fixtures

Current-limited bench supplies, oscilloscope with suitable probes, logic analyzer for I2S/UART/I2C, electronic load, audio interface/analyzer, resistive headphone dummy loads, thermocouples and USB PD source/analyzer. Battery simulator is preferred before real pack/charger testing. Test fixtures and EVMs are chosen for isolation and visibility; they are not final industrial design.

### B0 — documented wiring review

1. Record exact Teensy revision, converter boards/EVMs, strap states and their schematics.
2. Confirm supply and logic levels with a meter before interconnection. No 5 V on Teensy GPIO.
3. Confirm Teensy VIN/VUSB isolation and one host per USB device path.
4. Keep high-speed clock/data wiring short with adjacent grounds; do not use a full-size solderless breadboard for a production-rate USB or DSI link.
5. Review board profile/pin continuity against every component. Supply off for rewiring.

### B1 — power and supervisor only

Use bench supply/electronic load before processors/battery. Verify 5.1 V and 3.3 V rails, enable polarity, discharge, reverse blocking and fault behavior. Test rising/falling input, fast load steps, unplug/replug and weak-source current limiting. CM5 power pin target remains 4.75–5.25 V. Verify charge stays disabled without pack policy.

Do not connect a real smart pack until the host observes its charge requests and fault/temperature states correctly. Test the RRC pack initially with its supported charger; then qualify the custom smart charging implementation.

### B2 — Teensy, PSRAM and card

Install/test both PSRAM chips per their power-up requirements. Run address/data patterns across all 16 MiB, including cache/boundary stress. Confirm reported size and no corruption with SD/USB active. Exercise large SD reads/writes separately, then together. Record card identity, formatted state, fragmentation/fullness, throughput, maximum stall and power.

### B3 — four output channels, then input

Connection summary (not a substitute for converter-board schematics):

| Teensy | Destination |
|---|---|
| 7 | Main PCM5102A DIN |
| 32 | Headphone PCM5102A DIN |
| 20 | Both DAC LRCK + ADC LRCK |
| 21 | Both DAC BCK + ADC BCK |
| 23 | ADC MCLK (configured slave) |
| 8 | ADC DOUT |
| 18 /19 | ADC/amp control SDA/SCL |
| 31 | Qualified DAC unmute gate |
| 33 | Headphone enable |
| GND | Common digital/analog reference per board design |

DAC hardware straps select I2S and BCLK-derived clocking as supported; ADC must not drive shared clocks. Verify LRCLK/BCLK/MCLK with scope before unmuting. Send distinct low-level tones to all four outputs and confirm channel order. Add ADC only after output mapping passes. Test independent headphone preview and output-start/shutdown pop behavior into dummy loads.

### B4 — fixed physical lanes

Wire encoder pairs as pin map; wipers to 14–17; panel expander to Wire2 25/24 with INT at 40. Verify each lane's top/push/turn/fader/bottom mapping through recorded event traces. Test slow/fast/reversing encoder gestures, simultaneous pushes, edge-of-travel faders and EMI while charging. Keep temporary controls in the user's fixed conceptual order.

### B5 — AP and communications

Start with CM5 IO Board and separate qualified supply. Cross AP TX to Teensy RX28 and AP RX to Teensy TX29 through power-off-safe interface; common ground. Configure exact UART route and no serial console on it. USB host from CM5 to Teensy device for bulk transfer; no raw audio dependency.

Handshake capabilities, import a verified P1 set, start all four stems together and restart the AP while playing. The existing set and physical controls must continue. Reconnect and restore the UI from Teensy state; reject stale queued notes/commands.

### B6 — full load and failure tests

| Test | Preliminary acceptance criterion / evidence |
|---|---|
| Four synchronized stems | Identical frame counters/start/seek; impulses stay aligned over full song and repeated seeks |
| Performance soak | 8 h, zero missed deadlines/xruns in defined graph; min queue depth and max render time recorded |
| DSP budget | Target max observed render ≤60% block time; no block exceeds deadline |
| Card stress | ≥8 MB/s required mixed workload target; characterize worst stalls, including nearly full/fragmented cards |
| Independent preview | Preview appears only on HP; main waveform unchanged except expected analog crosstalk floor |
| Dub tail | Fader to zero removes dry source while existing decay continues; capture output |
| Input-to-output latency | Measure loopback; propose ≤10 ms normal monitoring, include filter/queue contribution |
| Controls | No missed/reversed detents; ≥10 stable fader bits; physical response ≤10 ms target |
| AP touch response | 99th percentile ≤20 ms proposed; measure from physical touch, not only message receipt |
| AP crash/disconnect | Current set continues; AP-origin held notes release; reconnect restores state |
| Corrupt/incomplete import | Rejected atomically; no partially playable set or sibling-stem drift |
| Power loss during save/record | Prior committed project survives; last valid recording span recovered; loss window quantified |
| Charging/noise | Compare spectrum/noise with battery, PD, bright screen, USB load; Audio Lab sets final numerical limits |
| Thermal | No protection violation or uncontrolled rail drop; throttle/temperature logs at stated ambient |
| ML qualification | Same model/corpus and warm repeats; RTF/RSS/energy/failure report, no extrapolation from TOPS |
| Firmware update failure | Interrupted AP update rolls back; Teensy recoverable via Program/Loader; no false MCU rollback claim |

No hardware results are claimed here. If tests fail, revise the architecture/profile and rerun affected tests rather than hiding failures by silently reducing feature scope.

## 5. Phase gates after this document

- **Phase A — architecture:** this linked package, analytical checks and open-risk record.
- **Phase B — bench feasibility:** actual wiring review and measurements above; select AP, audio/profile, panel and pack.
- **Phase C — editable KiCad schematics:** after Phase B evidence and interface acceptance; full passive/protection/connector specifications, ERC, power sequencing and design review.
- **Phase D — PCB layout:** only when mechanical constraints, stackup, connectors and thermal arrangement are real.
- **Phase E — fabrication package and physical validation:** reviewed Gerbers/drill/assembly/BOM, then assembly, test and revisions.

This phase describes architecture/research/documentation and prototype planning; subsequent user requests determine authorization for later work. This package is sufficient to begin a disciplined bench build, but it does not certify an unbuilt V1 or authorize fabrication.

### Document validation performed

Confirmed eight linked Markdown documents, no broken relative document links, 60 recovered source turns including the oldest turn, 35 assigned edge pins plus seven reservations, and independent recalculation of 705,600 B/s P1 stem traffic, 1,152,000 B/s P2 stem traffic and 3.497 h estimated performance runtime. These are documentation/arithmetic checks only. A physical netlist/continuity check remains Phase B0.

## 6. Integration checklist

1. Adopt H0.1 and P1 provisionally; retain LOCKED product requirements separately.
2. Review the implemented [preparation adapter](../../services/preparation-adapter/README.md) and cross-component tests; retain native outputs and resolve its long-track limit.
3. Add block-prefetch and timestamp/capability boundaries to firmware.
4. Qualify dual DAC/ADC wiring and independent preview.
5. Measure DSP/card/PSRAM before fixing voice/FX/loop limits.
6. Run embedded ML and display/stylus tests before committing production CM5 carrier.
7. Qualify power/smart charging with measured loads and pack protocol.
8. Record approved changes in one shared master, including board, firmware, media and protocol revisions.
