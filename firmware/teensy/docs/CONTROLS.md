# Controls and state transitions

## Physical lanes

Hardware scanners supply normalized `ControlEvent`s. Press events are edges, not held-button repetitions; button release does not terminate a one-shot. Encoder detents are signed integers; faders are normalized 0–1. Debounce defaults to 12 ms and quadrature uses four transitions per detent; these are test defaults pending actual controls.

| Control | Dub | Jam proposed mapping (opt-in) | Home/Files/Settings | DAW |
|---|---|---|---|---|
| Top | Toggle FX enable, preserve all parameters | Select lane and toggle arm | Inherit last music mode | Unmapped pending review |
| Encoder rotation | Intensity ±1% per detent | Sound macro ±1% | Inherit last music mode | Unmapped |
| Encoder press | Open FX browser | Open Sound browser | Inherit last music mode | Unmapped |
| Fader | Dry channel volume | Track volume | Inherit last music mode | Unmapped |
| Bottom | Trigger assigned complete-play sample | Start/finish event loop | Inherit last music mode | Unmapped |

Final Jam mapping is explicitly unapproved: `Settings::jamPhysicalMapping=false` by default. Touch/API notes, recording and sequencing work without enabling it. No physical transport, jog wheel or LED mapping is selected. `BoardConfig` pins all start at -1.

## Browser and detail states

| Current state + action | Result |
|---|---|
| Performance + encoder press | Browser for that lane |
| Browser + that lane's encoder turn | Wrap selected available effect/Sound index |
| Browser + encoder press | Commit selection and return to performance |
| Select effect through API | Same commit path, one undo checkpoint |
| DESIGN | `Page::Design`; edit complete Fx/Sound model through App APIs |
| Sequencer | `Page::Sequencer`; `setStep` edits one of 16 steps |
| Keyboard/pads | Context derived from selected SoundKind; touch adapter supplies note events |
| Open Files for assignment | Remember target/action/return mode |
| Choose sample or Sound reference | Assign requested target; restore return mode |
| Choose Preview | Start headphone preview without changing main routing |

Browser subvariants and a dedicated Cancel/Back event are not implemented. `mode()` returns to a mode's performance page. Six effects and three basic synthesized Sound types are directly browsable; imported sample instruments use file/API assignment. DESIGN is a model-editing boundary with a label view, not a full graphical editor. Master, pads, keyboard and sequencer graphical widgets are also future display-adapter work.

## Global transport

Play starts/continues the single clock. Stop rewinds clock and stems, ends active event-loop capture, clears synth voices, and lets delay/reverb state decay. One-shot completion is independent of transport Stop in this prototype.

Record in Jam starts selected-track event capture after the configured count-in; pressing Record again ends capture and enables looping. This is a software test workflow, independent of the undecided Jam bottom-button mapping. Record in Dub currently sets recording state/count-in only: **no mixed-audio file is recorded**. Record repositions the musical clock to zero; use it before starting a performance in this prototype. Full punch-in/output-recording semantics await implementation.

BPM range 20–300 and resolution 0.001 BPM are test limits. Tap uses the last valid 200–3000 ms interval; averaging/filtering is an extension. 96 ticks = one quarter note; 24 ticks = one sixteenth; 384 ticks = one 4/4 bar. Count-in choices are off/one/two bars. Sequencing and event loops follow the same clock; stem audio remains at native speed when BPM changes.

MIDI note ingress is filtered by the configured 1-based channel. MIDI clock consumes 24 pulses per quarter when external-clock mode is explicitly enabled; Start/Stop ingress is gated by that setting. No clock source arbitration, clock-loss recovery, MIDI-thru or outgoing scheduling is selected. External pulses are not interpolated; high-resolution event timing between pulses remains a limitation.

## Undo and persistence

Creative edits use eight in-memory Project snapshots by default. Redo clears on a new edit. Live performance triggers, transport, preview and navigation are not reversible operations. Fader/encoder events currently take individual snapshots; gesture coalescing needs a real input adapter. Undo does not rewind already generated audio or DSP tails. Session navigation, device Settings and voice state are not part of project snapshots.
