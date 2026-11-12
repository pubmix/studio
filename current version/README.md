# Studio — current version

Source snapshot published from the November 2026 Studio development session.
This folder is additive; previous repository folders and handoffs remain intact.
It contains the current ESP32 display, Teensy audio firmware, sound-bank licensing,
offline stem exporter, desktop screen mirror and UI design work. Build outputs,
serial logs/captures, local credentials and environment caches are excluded.

## Device status at publication

Installed and upload-verified: current sound firmware (recorded piano and acoustic
drum kit alongside classic synthesis), master speaker volume, live arpeggiator,
expanded finger-friendly Keys view, three themes and staged screen transitions.
The latest ESP32 recovery update was also flashed with hash verification and reset;
its recorder was restarted. Recovery under an actual dropped stream or display
reset still needs hardware validation. Successful upload is not proof that every
screen, full arrangement or recovery condition has been tested.

The arpeggiator defaults Off and is session-only. While On it reserves one voice,
leaving five for sequenced notes. It affects live keys/MIDI/pads, not existing
patterns; recording captures the input chord rather than generated arpeggio steps.
Current normal synth, piano, drum and transport behavior remain available.

## Contents and build

- `dubbox-firmware/`: Teensy 4.1 audio, instruments, samples, sequencing and tests.
- `dubbox-display/`: ESP32 touchscreen UI, themes, transitions and screen stream.
- `hardware-monitor/`: bounded desktop USB receiver and browser viewer, including
  drawing-stream recovery. Read its portable usage note before running it.
- `apps/stem-export/`: tested macOS offline four-stem export, with explicit manual
  SD import limitations. `firmware/teensy/tools/` supplies its validation helper.
- `design/`: concept images, SVG studies, track icons and research; concepts are
  not a promise that all pictured functionality is present on the device.

Install PlatformIO and run `pio run -d dubbox-firmware` and
`pio run -d dubbox-display` from this folder. Select the correct ports for your
hardware before any upload; the display configuration retains development COM6.
Do not run a serial recorder while flashing. See each component README for actual
hardware wiring, build dependencies, tests and outstanding limitations.

Both board builds passed in the development environment. Arpeggiator scheduler
and integration ownership tests passed with address/undefined-behavior sanitizers;
keyboard mapping/bounds/chord tests and the stem-export tests also passed during
development. Component READMEs distinguish host checks from physical verification.
Source-only publication does not rerun every historical test or establish a new
release certification. No cloud credentials or user song files are included.

The recorded sound bank derives from VCSL CC0 assets. Its license, original-source
manifest and conversion tools are in `dubbox-firmware/src/samples/`. Generated PCM
is compiled into the firmware; raw source WAV downloads and compiled firmware
binaries are omitted. Keep license/provenance alongside redistributed samples.

`SNAPSHOT-MANIFEST.json` records packaged file sizes and SHA-256 hashes. Mirror
recorder paths alone were adapted to keep generated files inside this snapshot;
firmware sources are copied directly from the working source at publication.
