# STUDIO

Integration authority: [STUDIO shared architecture I0.1](docs/SYSTEM_ARCHITECTURE.md). It distinguishes implemented behavior from proposed hardware and lists remaining integration gates.

[![Firmware checks](https://github.com/pubmix/studio/actions/workflows/firmware.yml/badge.svg)](https://github.com/pubmix/studio/actions/workflows/firmware.yml)

STUDIO (formerly Dub Box) is a portable four-lane music instrument. This repository brings its firmware, local Stem Engine, application interfaces and hardware designs together.

**Canonical stem order: VOCALS → MELODY → BASS → RHYTHM.**

## Project areas

| Area | Contents | Status |
|---|---|---|
| [firmware/teensy](firmware/teensy) | Teensy 4.1 sketch, portable C++ core, desktop simulator, tests and documentation | Prototype implemented; hardware untested |
| [services/stem-engine](services/stem-engine) | Local audio preparation and AI/ML separation | Local prototype implemented; 20 tests and synthetic model inference passed |
| [packages/contracts](packages/contracts) | Versioned interfaces shared between components | Prepared-stem transfer schema v1 |
| [services/preparation-adapter](services/preparation-adapter) | Native FLOAT32 to firmware P1 bridge | Host prototype; bounded track length |
| [apps](apps) | Future touchscreen/companion application code | Reserved |
| [hardware](hardware) | V1 system architecture, candidate BOM, pin budget, power and integration contracts | Architecture proposal; hardware untested |
| [docs](docs) | Cross-project architecture and integration conventions | Initial guidance |

## Start here

- [V1 hardware architecture and integration handoff](hardware/architecture-v1/README.md)

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
