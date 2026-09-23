# STUDIO V1 — portable firmware prototype

Integration authority: [STUDIO shared architecture I0.1](../../docs/SYSTEM_ARCHITECTURE.md). It distinguishes implemented behavior from proposed hardware and lists remaining integration gates.

This is the **Teensy implementation track**, independent of the ML separator. It delivers a working C++ core, command-line simulator, Teensy sketch, prepared-WAV importer, DSP, sequencing, storage adapters and tests. It is an engineering prototype, **not finished production firmware or a completed touchscreen application**.

Start with [scope and open decisions](docs/SCOPE.md), [controls](docs/CONTROLS.md), [architecture](docs/ARCHITECTURE.md), [audio graph](docs/AUDIO.md), [data model](docs/DATA_MODEL.md), [integration](INTEGRATION.md), and [validation results](docs/VALIDATION.md).

## Run on the Mac

Requires a C++17 compiler and Python 3; no third-party desktop libraries.

```sh
make
make test
make sanitize
python3 -m unittest discover -s tests -p 'test_*.py'
python3 tools/make_demo_assets.py demo-assets
python3 tools/validate_prepared.py demo-assets/prepared
./build/studio_sim demo-assets/prepared
```

Try `play`, `render 2`, `lane 1`, `top`, `turn 22`, `fader 0`, `render 2`, `save Demo`, `undo`, `redo`. `help` lists commands; `quit` exits. `jam`, `note 60`, `render 1`, `off 60` exercises synthesis. `record` starts/stops a selected-track event loop. `jam-controls` explicitly opts into the proposed physical mapping for simulation.

The simulator reports audio energy, peaks, and timing. It does **not** open an audio device. Its state/files live under `STUDIO-data/` in the current working directory. The demo stems are generated tones for synchronization tests; the seven synthetic sample sketches are original approximations, not finished factory instruments.

## Teensy

Open `firmware/STUDIO/STUDIO.ino` in Arduino IDE with Teensy support, or:

```sh
arduino-cli core update-index --additional-urls https://www.pjrc.com/teensy/package_teensy_index.json
arduino-cli core install teensy:avr@1.62.0 --additional-urls https://www.pjrc.com/teensy/package_teensy_index.json
sh tools/teensy-build.sh
sh tools/teensy-build.sh io-example
```

Default build is a **serial diagnostic**, with SD and I²S disabled and no inferred control/display/codec pins. The optional I/O build enables the Teensy SDIO reader and an I²S output connection, but **does not configure an audio codec or headphone hardware**. Review `BoardConfig.h` and implement the actual board adapters before using it on hardware. Nothing was flashed during this task.

With SD enabled, place validated stems in `STUDIO/Imports/Prepared/` and the seven sample WAVs in `STUDIO/Samples/Factory/`. The serial help is printed at 115200 baud. `y` accepts a recovery when available. Save journals use `STUDIO/System/`.

The tested Teensy compiler, libraries and memory reports are recorded in `docs/VALIDATION.md`. The CMake configuration is supplied for other environments; this Mac's builds used Make.

## What is usable now

- Canonical **VOCALS → MELODY → BASS → RHYTHM** import, frame validation and synchronized playback.
- Four lanes, independent FX enable/intensity, encoder browser, editable macros, pre-fader DSP returns, complete-play one-shots.
- Shared 4/4 clock, tempo/tap, count-in, metronome, MIDI input boundary, master limiter/meters and separate preview bus.
- Generic Jam tracks, sine/saw/drum/sample voices, key/scale lock, keyboard note mapping, 16-step note patterns and live event loops.
- Versioned projects, Sound/FX preset persistence, Save/Save As, undo/redo, autosave/recovery, host file operations and an SD journal adapter.
- Home/mode/settings/theme rendering framework and DAW data shell.

The six prototype DSP choices are functional but are not a finished professional effect library. Display widgets, advanced Sound Pak browsing, audio recording/export, polished synths, waveform editing, and several hardware integrations remain explicitly tracked in `docs/SCOPE.md`.
