# STUDIO

[![Firmware checks](https://github.com/pubmix/studio/actions/workflows/firmware.yml/badge.svg)](https://github.com/pubmix/studio/actions/workflows/firmware.yml)

STUDIO (formerly Dub Box) is a portable four-lane music instrument. This repository brings its firmware, local Stem Engine, application interfaces and hardware designs together.

**Canonical stem order: VOCALS → MELODY → BASS → RHYTHM.**

## Project areas

| Area | Contents | Status |
|---|---|---|
| [firmware/teensy](firmware/teensy) | Teensy 4.1 sketch, portable C++ core, desktop simulator, tests and documentation | Prototype implemented; hardware untested |
| [services/stem-engine](services/stem-engine) | Local audio preparation and AI/ML separation | Preparation + current WiFi adapter implemented; 41 tests and synthetic inference/export passed |
| [packages/contracts](packages/contracts) | Versioned interfaces shared between components | Prepared-stem transfer schema v1 |
| [apps](apps) | Future touchscreen/companion application code | Reserved |
| [hardware](hardware) | Future schematics, PCB, enclosure and board configuration | Reserved |
| [docs](docs) | Cross-project architecture and integration conventions | Initial guidance |

## Current Dub-Box application

The current hardware application lives in `dubbox-firmware` (Teensy audio and
controls) and `dubbox-display` (ESP32 touchscreen and WiFi). See
[DUBBOX_PROJECT_SUMMARY.md](DUBBOX_PROJECT_SUMMARY.md) for current wiring and features.
The older `firmware/teensy` implementation below remains a separate prototype.

The [Stem Engine](services/stem-engine/README.md) now connects to the active
application through a PCM16 exporter and the existing WiFi uploader. Preparation
runs locally on the computer; uploaded stems are selected in tracks 1–4 through
the current touchscreen picker. See [integration architecture and usage](services/stem-engine/docs/REPOSITORY_INTEGRATION.md).

## Start here

**Moving to another computer?** Follow [new-computer setup](docs/NEW_COMPUTER.md) for the companion, model downloads, device pairing, and both firmware uploads.

- [Stem Engine quick start](services/stem-engine/README.md)
- [Stem Engine verification](services/stem-engine/docs/VERIFICATION.md)
- [Firmware quick start](firmware/teensy/README.md)
- [Working features and outstanding decisions](firmware/teensy/docs/SCOPE.md)
- [Validation results](firmware/teensy/docs/VALIDATION.md)
- [Stem Engine integration](firmware/teensy/INTEGRATION.md)
- [Adding the other project components](docs/CONTRIBUTING_COMPONENTS.md)

```sh
make test
make simulator
```

The firmware diagnostic and optional SD/I²S builds compiled with Teensyduino 1.62.0. Desktop verification passed 5,153 C++ checks and eight import tests. This remains an engineering prototype with graphical interfaces, hardware bindings and production audio features still to complete.

Generated binaries, model weights, training data and personal audio do not belong in source control. Generate test assets with `python3 firmware/teensy/tools/make_demo_assets.py firmware/teensy/demo-assets`.

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md). GitHub Actions builds and tests the firmware core on Linux and macOS. The local Stem Engine lives in `services/stem-engine/`; its README documents its separate environment and validation commands.
