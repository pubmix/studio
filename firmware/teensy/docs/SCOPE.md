# V1 scope and decisions requiring review

## Status key

**Working** means executed in desktop tests or simulator. **Adapter** means compiled firmware interface with hardware behavior unverified. **Framework** means a data/API/view boundary exists, not a finished user experience. No physical STUDIO hardware was validated.

| Requirement | Delivered status |
|---|---|
| Global state/Home/Dub/Jam/Files/Settings | Working state manager; label-based display framework |
| DAW | Framework only, intentionally undefined |
| Transport/BPM/tap/4/4/metronome/count-in | Working sample-driven core |
| Four-lane controls | Working event mapping/normalizers; physical HAL unbound |
| Canonical stem order/sync/prepared imports | Working WAV readers and validator; service adapter boundary |
| FX toggle/intensity/browser/DESIGN/mappings | Working model/control APIs; limited DSP bank; graphical DESIGN unbuilt |
| Pre-fader tail preservation | Working and tested |
| Complete-play slots | Working WAV/memory playback; retrigger choice explicit |
| Factory sample names | Seven generated original sample sketches, not polished factory assets |
| DSP backends | Custom prototype + Teensy bridge; external library extension boundaries |
| Jam Track→Sound→Pattern | Working generic model and basic synth/sample playback |
| Keyboard/key/scale/lock | Working 24-note mapping and pitch quantization; touchscreen widget unbuilt |
| Live loops | Working selected-track note-event capture; audio looping unbuilt |
| 16 steps | Working single note row per track; drum rows/automation not yet implemented |
| Sounds/presets/Paks | Working Sound/FX persistence; Pak manifest framework only |
| Files operations/search/recents/trash | Working host implementation; SD browser framework |
| Save/Save As/autosave/recovery | Working host, tested two-slot journal, SD adapter compiled |
| Audio recording/export | Recording state and folders; no mixed-output recording writer |
| Settings categories | Shared view/model framework; actual device configuration UI unbuilt |
| Eight themes | Working semantic tokens, identical layout tests; font/texture assets unbuilt |
| Undo/redo | Working bounded snapshots; no persisted history |
| Master/meter/clip/limiter | Working audio core; graphical meter view unbuilt |
| Main/headphone/preview | Working independent logical buses; headphone hardware unbound |
| MIDI/system/firmware/storage status | Models and MIDI parser; physical MIDI/update mechanism unbound |
| Teensy build | Diagnostic and optional I/O configurations; consult validation report |

## Explicit configuration boundaries

| Decision | Current prototype behavior | Required product decision |
|---|---|---|
| Touchscreen/display | IDisplay label API; no driver/pins | Display resolution, controller, touch/stylus hardware, widgets |
| Codec/audio I/O | I²S output optional, no codec setup | Codec, analog levels, input channel count, wiring |
| Pins | All control pins -1 | Final board map, encoder direction, ADC calibration |
| Jog wheels | No inferred actions | Hardware and per-mode semantics |
| LEDs | No inferred actions | Indicator states, polarity, driver and brightness |
| One-shot retrigger | Unconfigured: first play works; retrigger while active rejected | Restart / ignore / polyphony policy |
| Physical transport | Serial/API events | Buttons and mappings |
| Headphones | Separate logical output only | Independent DAC/codec outputs and gain control |
| SD | PCM readers and journal | Capacity/format/card requirements, power-loss guarantees |
| MP3 | Rejected by firmware reader | Where conversion happens, format/quality/cache policy |
| Undo depth | Eight snapshots in RAM | Depth, gesture coalescing, history persistence |
| Autosave | 30-second dirty interval | Interval, write scheduling while playing, recovery prompt |
| Time signatures | 4/4 only | Additional signatures and pattern behavior |
| MIDI sync | Explicit internal/external setting | Source priority, loss fallback, outgoing sync |
| Jam physical controls | Disabled unless opt-in | Final select/arm/macro/loop mapping |
| Mode switching | Keep transport; clear Jam voices; source reads pause outside Dub | Preserve/stop/crossfade and resume policy |
| Stop/Record | Stop ends event loops; no output capture | Punch-in, master recording, sample stop behavior |
| FX switching/bypass | Gated send; shared delay state | Musical tail handoff, click-free transitions |
| DSP memory | Short 185 ms delays | Longer echo buffers, PSRAM, sample format/quality tradeoffs |
| Sound engine | Basic voices and nearest-frame samples | Instrument architecture, polyphony, bandlimiting/quality |
| DAW | Mode shell/data interface | User-led workflow definition |

## Next concrete hardware work

1. Select the board's display, codec, control pins and headphone topology.
2. Implement HAL scanners and touchscreen hit testing; connect the compiled bridge to verified hardware outputs.
3. Resolve stable asset catalogs on SD and build the contextual Files/Pak interfaces.
4. Measure SD stalls and DSP/voice timing; replace foreground blocking saves with bounded jobs.
5. Expand DSP and instruments, add gesture smoothing, and validate complete musical behavior with audio recordings and power-cut tests.

This is the bounded prototype scope delivered here. It deliberately records missing features rather than representing interfaces, folders or labels as finished product functionality.
