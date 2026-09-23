# STUDIO Firmware

Integration authority: [STUDIO shared architecture I0.1](../docs/SYSTEM_ARCHITECTURE.md). It distinguishes implemented behavior from proposed hardware and lists remaining integration gates.

## Purpose

Own the real-time instrument software: synchronized playback, mixing, effects, controls, sequencing, storage and board adapters. Neural stem separation runs in the [Stem Engine](../services/stem-engine/README.md) before performance.

## Current state

The [Teensy prototype](teensy/README.md) contains a portable C++ core, desktop simulator, Teensy sketch and prepared-WAV importer. It is an engineering prototype; physical hardware integration remains untested. See [validation](teensy/docs/VALIDATION.md) for the checks actually performed.

## Start here

- [Build and run](teensy/README.md)
- [Scope and open decisions](teensy/docs/SCOPE.md)
- [Architecture](teensy/docs/ARCHITECTURE.md)
- [Prepared-stem integration](teensy/INTEGRATION.md)
- [Hardware decisions](../hardware/README.md)

## Next steps

Confirm the hardware pin map and audio/display/control adapters, test on the actual board, and connect the explicit stem-transfer profile. Keep unresolved product behavior recorded in the scope document.

## Documentation

Each new firmware component folder must include a README.md describing its purpose, current status, usage, validation, open decisions and next steps. Update it with the implementation. Follow the [repository convention](../docs/CONTRIBUTING_COMPONENTS.md).
