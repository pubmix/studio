# STUDIO repository instructions

## Documentation for every aspect

Every new aspect or component folder must include a `README.md`. Keep that file updated in the same change as its implementation. Read the relevant component README before changing that component.

Each README must cover purpose and scope, current implementation status, important files, how to use/build it where applicable, validation actually performed, limitations/open decisions and next steps. Link detailed Markdown documents rather than copying stale technical descriptions. Clearly distinguish proposed work from implemented and tested work.

The current three aspects are:

- [Firmware](firmware/README.md)
- [Hardware](hardware/README.md)
- [Stem Engine](services/stem-engine/README.md)

Follow [component contribution guidance](docs/CONTRIBUTING_COMPONENTS.md) for layout and integration. Preserve the universal stem order: VOCALS, MELODY, BASS, RHYTHM.
