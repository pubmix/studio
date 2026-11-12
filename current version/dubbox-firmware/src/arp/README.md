# Live instrument arpeggiator

## Scope and current status

Optional live-key arpeggiator, integrated in DrumMachine and the touchscreen Sound
pages. Off by default and session-only: project reset/load and instrument removal
turn it off. Existing saved projects and recorded patterns retain their original
meaning. No firmware flash or listening check is claimed by this implementation.

`arpeggiator.h` is the allocation-free scheduler. `../drum_machine.*` owns the
integration, `../esp_link.*` exposes the protocol, and display `synth_panel.cpp`
and `teensy_link.*` provide controls and state. The original scheduler tests live
in `../../prototypes/arpeggiator/test.cpp` and include this production core.
`test_integration.py` extracts actual DrumMachine methods into a fake-synth host
harness rather than maintaining a second implementation of them.

## Use and behavior

Open an instrument's Sound view. Previous from Presets reaches Arpeggiator 2/2;
Previous once more reaches Arpeggiator 1/2. Tap controls to cycle On/Off,
Up/Down/UpDown/Random, 1–3 octaves, 1/2/4/8 steps per beat and 25/50/75/100% gate.
Return to Keys and hold a chord. Tempo follows project BPM, beginning immediately
with the first held key. Timing is tempo-synchronized, not quantized to the song's
bar boundary. MIDI and routed physical pad notes share the same live-note path.

While enabled, the instrument's sixth voice is reserved exclusively for arp;
sequenced notes and auditions retain the other five voices. Off preserves the
original six-voice allocation. Enabling/reconfiguring an active arp releases that
instrument's live inputs and any previous note on the reserved voice; re-hold the
chord afterwards. Editing settings while Off does not cut normal notes. Patterns
are not transformed; live recording retains input chord notes, not generated arp
steps. Sustain retains the held chord until pedal release.

Stop/seek releases arp output and clears its held inputs, so it cannot restart
unexpectedly on the next loop. Project reset/removal clears settings. Display
keepalive timeout continues through the existing live-note release path. Synthesis
envelopes, samples, effects and legacy project serialization are unchanged.

## Protocol and resource bounds

`&,instrument,on,pattern,octaves,stepsPerBeat,gatePercent` is the command and
periodic state response. Instrument indexes are zero-based, pattern 0–3,
octaves 1–3, rate 1–8, gate 1–100; on is 0/1. UI offers a smaller musical subset.
Invalid commands are rejected. No persistence or automation of these settings yet.

Maximum six held pitches, eighteen expanded pitches and one sounding arp voice
per instrument. Overflow rejects the newest distinct held pitch. Callbacks are
synchronous with no event queue or allocation. The host scheduler occupies 56
bytes; four instances are embedded in DrumMachine. Tick skips missed steps instead
of making a catch-up burst. Unsigned clock wrap works if gaps remain below 2^31
microseconds; stop/start is needed after a longer suspension. No CPU, audio quality
or maximum-arrangement performance claim is made without a hardware load test.

## Validation and next steps

Run `python3 test_integration.py` here and compile/run the prototype's `test.cpp`
with a C++17 compiler and address/undefined-behavior sanitizers. Both passed.
Actual integration-method tests check reserved-voice exclusion, untouched other
voices/instruments, Off behavior, MIDI/display entry paths, display refresh
ownership, sustain release, stop/seek clearing, mode changes and invalid arguments.
Both Teensy 4.1 and ESP32 PlatformIO builds passed after integration. The ESP32
build retains existing SSD2828 dimension-conversion warnings; no new arp warning
was observed. No flash was performed.
The core suite checks exact patterns, seeded random reproducibility, octave/pitch
bounds, rate/gate timing, overflow, note-off balance, wrap and delayed ticks.
Mocks cannot validate UART, actual MIDI transport, touch hardware, audio envelopes
or processor load. Build both boards before installation, then verify those paths,
full-arrangement playback and disconnect recovery on the device. Firmware install
must wait for the coordinating agent's review.
