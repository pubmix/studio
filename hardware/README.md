# STUDIO Hardware

## Purpose

Own the physical instrument: schematics, PCB layouts, enclosure and mechanical designs, board-specific pin maps, control wiring, audio I/O and power design.

## Current state

The [V1 hardware architecture package](architecture-v1/README.md) proposes a Teensy 4.1 + Compute Module 5 system, with a candidate BOM, Teensy pin/resource budget, power tree, system interfaces, bring-up plan and integration handoff. It preserves the fixed four-lane controls and VOCALS / MELODY / BASS / RHYTHM stem contract.

This is an architecture proposal, not a fabrication release. Recommended parts and pin assignments require integration review and bench validation. No schematic, PCB or tested hardware implementation is claimed here.

## Related components

- [Firmware](../firmware/README.md): board adapters and real-time operation.
- [Stem Engine](../services/stem-engine/README.md): local Mac separation before playback.
- [Product context](../services/stem-engine/docs/PRODUCT_CONTEXT.md): four-lane instrument direction.

## Next steps

Review the [decision log, risks and bring-up gates](architecture-v1/07-INTEGRATION.md), reconcile the [interface contracts](architecture-v1/04-INTERFACES.md) with firmware and Stem Engine, and validate the recommended processor, audio and power architecture before schematic capture. Onboard ML remains unvalidated; the working Mac separator remains the first prototype's preparation path.

## Validation

The architecture documents record manufacturer sources and analytical budgets. Internal package links were checked. No physical electrical, acoustic, thermal or embedded ML tests have been performed; see the package README for evidence and limitations.

## Documentation

Every new board, enclosure or other hardware component folder must include a README.md with purpose, current status, design-file locations, parts/connection information, validation performed, open decisions and next steps. Clearly distinguish proposed, built and tested designs. Follow the [repository convention](../docs/CONTRIBUTING_COMPONENTS.md).
