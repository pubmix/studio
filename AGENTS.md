# STUDIO repository instructions

## Documentation for every aspect

Every new aspect or component folder must include a `README.md`. Keep that file updated in the same change as its implementation. Read the relevant component README before changing that component.

Each README must cover purpose and scope, current implementation status, important files, how to use/build it where applicable, validation actually performed, limitations/open decisions and next steps. Link detailed Markdown documents rather than copying stale technical descriptions. Clearly distinguish proposed work from implemented and tested work.

The current three aspects are:

- [Firmware](firmware/README.md)
- [Hardware](hardware/README.md)
- [Stem Engine](services/stem-engine/README.md)

Follow [component contribution guidance](docs/CONTRIBUTING_COMPONENTS.md) for layout and integration. Preserve the universal stem order: VOCALS, MELODY, BASS, RHYTHM.

## Firmware update preference

The user explicitly requested on 2026-09-22 that new firmware updates be flashed
after each update. After successful build and relevant checks, upload the changed
board's firmware to its verified connected USB identity and verify startup. This
is standing authorization; do not ask again for routine firmware uploads. Do not
flash an unrelated board. If disconnected or validation fails, report the blocker.
Stop the hardware recorder before taking a serial port and restart it afterwards;
follow `hardware-monitor/README.md`. Preserve user projects and device settings.
