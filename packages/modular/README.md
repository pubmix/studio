# Modular instrument

A six-voice modular instrument for Dub-Box, sharing the four instrument slots with
ordinary synths. Implemented in the Teensy audio engine and ESP32 PATTERN editor.
It uses existing piano-roll notes, live keys, pad routing, pattern clips and project
saving. No existing synth or drum feature is removed.

## First patch

1. Open/create a project, choose **PATTERN**, then **+ → MODULAR**.
2. The new **MOD n** tab opens **PATCH**. The starter patch already plays:
   two oscillators → mixer → filter → VCA → output, with envelope control.
3. Tap **TEST C4** to audition, or **KEYS** to play. Use **ROLL** to enter notes
   and **PATTERN** at the bottom to loop them. Place that pattern in the playlist
   using the existing workflow.
4. Tap a module's **OUT**, then an input socket to connect a cable. An output can
   feed several inputs. Connecting an occupied input replaces its previous cable.
5. Cyan input sockets take the LFO or envelope; audio inputs take oscillator,
   noise, mixer, filter or VCA outputs. Feedback loops are refused without changing
   your patch. The output module is a destination only.
6. Tap an input then **UNPLUG** to remove its cable. **CANCEL CABLE** clears the
   selection. **START PATCH**, then **CONFIRM?**, restores the starter sound.
7. Tap a module title to select its main parameter. Tap the parameter name at the
   bottom to cycle through all ten controls, and use **− / +** to adjust.
   Parameters and cables save with the project using existing autosave timing.

The fixed rack contains oscillator 1, oscillator 2, noise, LFO, ADSR envelope,
two-input mixer, low-pass filter, VCA and output. Oscillators offer sine, triangle,
saw and square. Detune is −50 to +50 cents for oscillator 2. Other controls are
0–100 positions: LFO ~0.05–12.05 Hz, cutoff 60–12,000 Hz, attack up to 1.501 s,
decay up to 2.001 s, sustain level, release up to 3.001 s and output level.
Envelope modulation can raise the filter cutoff above its base setting.

## Files and protocol

- `patch.h`: shared schema, typed connections, topological sorting, atomic
  validation, bounded text serialization and project signature.
- `dsp.h`: allocation-free block graph evaluator (plus a scalar test reference), six independent voices,
  polyBLEP saw/square oscillators, noise, triangle LFO and one-pole filter.
- `../../dubbox-firmware/src/synth.*`: Teensy AudioStream integration.
- `../../dubbox-firmware/src/drum_machine.*`: instrument slots and note scheduling.
- `../../dubbox-firmware/src/project.*`: backward-compatible persistence.
- `../../dubbox-display/src/modular_panel.*`: visible patch cables and controls.
- `../../dubbox-display/src/screen_pattern.cpp`: instrument picker and tabs.

Both UART directions use `@,<slot>,<type>,<10 parameters>,<9 sources>`, with type
0=ordinary synth, 1=modular. The array order is declared in `patch.h`. Source −1
means disconnected. Reports repeat with the normal state refresh; edits are whole
validated transactions. `i,<slot>,1,<type>` appends an instrument. Older three-field
add commands still select the ordinary synth. Projects store this in `MOD<slot>=`
lines. Existing projects without those lines retain their ordinary synths.

## Build and validation

Build both `dubbox-firmware` and `dubbox-display` with PlatformIO. Host tests:

```
clang++ -std=c++17 -fsanitize=address,undefined tests/test_modular.cpp -o /tmp/modular-tests
/tmp/modular-tests
python3 ../../dubbox-display/tests/render_modular.py /tmp/modular-renders
```

On this Mac the compiler also needs
`-isystem /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`.
Tests passed on 2026-09-23: graph ordering, fan-out, invalid socket types, cycle
rejection without partial edits, serialization, disconnected output silence,
release completion, 440/880 Hz note pitch, six-voice output bounds, and the actual
panel handlers for connect/reject/unplug/audition/starter restoration. Four skin
renders were generated; the Modern rack was visually inspected. These are host
renders, not physical panel captures. Both board builds/uploads and real command/persistence checks passed; see
[hardware validation](HARDWARE_VALIDATION.md) for results and limits.

## Current limits and next steps

This is a fixed nine-module rack, not an unlimited module library. Each input has
one cable, each output supports fan-out, and feedback is intentionally unsupported.
The instrument is note-gated: a final safety envelope releases every voice even
when the user bypasses the VCA. Routing that same envelope to VCA gain multiplies
it a second time. It does not provide a free-running drone. Oscillator pitch CV
uses modest linear modulation; it is not calibrated volts/octave. LFOs restart per
note. The filter is a simple low-pass without resonance, and this instrument has
no chorus/reverb sends yet. The ordinary synth's effects remain available there.

Useful next improvements are a parameter popup (instead of cycling), patch presets,
more module types and draggable cables. Adding modules needs explicit CPU/memory
budgets and more graph validation. Analog audio and physical touchscreen behavior
still require listening and hands-on confirmation; the firmware mirror only proves
the drawing commands sent by the display firmware.

The USB debug command `!state` is read-only and prints the display's received
project/instrument/patch/note/pan state. It assists UART and persistence checks;
it does not claim physical button or panel validation. Only use it with the
recorder stopped so there is one serial owner, following the monitor README.

### Measured audio load

The final on-device block renderer measured 42% peak audio CPU for four modular
instruments playing six notes each (24 voices, saw/triangle oscillators and LFO
pitch modulation). Two sine oscillators per voice measured 78% peak. These runs
had no WAV tracks playing; other tracks/effects consume additional CPU. Block and
scalar output matched within 0.00001 across all four waveforms, six voices and
release tails in sanitized host tests. The final firmware was flashed and checked;
project persistence tests and known hardware limits are in the linked report.
