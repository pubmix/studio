# Arpeggiator scheduler test harness

## Purpose and status

The original prototype now delegates to the production core in
[`src/arp`](../../src/arp/README.md), which documents the current integration.
`arpeggiator.h` forwards to that header; `test.cpp` tests its behavior. The
remaining notes describe scheduler semantics and the original integration design.

Up, Down, UpDown (no repeated endpoints), and seeded Random traverse up to six
held pitches expanded over one to three octaves. Expanded pitches are sorted,
deduplicated and limited to MIDI 0–127. Rates are 1–8 steps per quarter-note beat,
30–300 BPM, with 1–100% gate. A seventh distinct held note is rejected without
changing the existing chord; repeated holds are idempotent, not reference-counted.

## Proposed integration contract

- One core per enabled instrument, owned by the control loop, not the audio ISR.
- Provide a synchronous `Emit(void*, Event)` callback. Reserve an existing synth
  voice and route on events to `Synth::noteOn(voice, midi)`, off to
  `Synth::noteOff(voice)`. Never share that voice with an independent allocator
  without explicit ownership arbitration. Current Synth exposes those APIs;
  DrumMachine owns the BPM and preview/timeline scheduling.
- Feed held keys using `hold`/`release`. Merge duplicate physical/MIDI sources
  upstream if source-specific release ownership is needed.
- Supply a consistent unsigned 32-bit microsecond clock to start/configure/tick.
  Start at the chosen transport grid boundary; pass DrumMachine BPM through
  Config. Changing configuration currently releases the active note and restarts
  phase immediately. The UI should avoid repeatedly configuring unchanged values.
- `stop` releases the sounding note but retains held keys; `clear` releases and
  removes all held keys. Call stop on transport pause, seek, project switch,
  instrument removal or arp bypass. Restart explicitly at the next intended grid.
- Held source-key release immediately releases its sounding expanded note.
  Notes overlapping octave expansions retain the first sorted source ownership.
- Callback must complete synchronously and must not reenter the core. There is no
  event queue to overflow: one callback pair maximum per tick, six held pitches,
  at most 18 expanded entries on the stack. Downstream deferred queues must retain
  note-off events or have an explicit panic/all-notes-off recovery policy.

## Scheduling and limitations

Tick checks gate release before retrigger. A delayed tick skips missed clock
steps instead of emitting a catch-up burst. Gate starts at actual emission time;
if the next grid event arrives sooner, that retrigger releases the prior note.
No fractional-period accumulator, swing, latch, velocity, persistence or UI yet.
Random uses fixed-seed xorshift32; missed random steps do not consume random
values, so repeatability requires the same tick schedule. Clock wrap is handled,
provided scheduling gaps stay below 2^31 microseconds (about 35.8 minutes); call
stop/start after longer suspension. This is not an interrupt-level sample clock.

The host instance measured 56 bytes, with 36 bytes of bounded note arrays in a
tick (plus normal compiler stack usage). No heap, SD reads or audio processing.
These are structural bounds, not measured Teensy CPU cost or sound-quality claims.
Device integration must preserve existing chord/voice ownership and test under
full-arrangement load before enabling this prototype.

## Validation

From this folder:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined test.cpp -o /tmp/studio-arp-test
/tmp/studio-arp-test
```

Passed on the development Mac: exact Up/Down/UpDown sequences, three-octave
expansion, pitch ceiling, seeded random repeatability, duplicate/full held-note
handling, invalid configuration, rates 1–8 with 1/50/100% gates, source release,
stop/clear/config-change note-off balance, one-note UpDown, clock wrap, and bounded
catch-up after a late tick. The isolated prototype itself had no firmware build, hardware timing, listening
test or flash. See the linked production README for subsequent integration and
build status. Hardware timing/listening checks remain outstanding.
