# STUDIO — V1 hardware architecture
Revision 0.1 • 14 September 2026 (Pacific) • Architecture proposal, not fabrication release

## Recommendation

Build V1 around **Teensy 4.1 + Raspberry Pi Compute Module 5 with 8 GB RAM and 32 GB eMMC**, with 16 MiB Teensy PSRAM and a Teensy-owned removable microSD. Teensy owns audio, physical controls and the musical clock; CM5 owns touchscreen rendering, file workflows and preparation jobs.

Use **two independent stereo DACs and one stereo ADC**. A stereo codec whose headphone socket merely mirrors its line output cannot satisfy independent headphone preview.

Retain the working Apple Silicon Stem Engine for the first prototype. CM5 is the recommended UI/management processor and an onboard inference candidate, **not a validated high-quality separator**. On-device separation performance and memory remain a release gate. If CM5 fails, test Jetson Orin Nano before committing a production carrier; do not promise that adding an NPU fixes model compatibility.

## Read in order

1. [Product requirements and provenance](01-PRODUCT-SPEC.md)
2. [Processor recommendation, diagrams and resource calculations](02-ARCHITECTURE.md)
3. [Teensy pins, buses and conflicts](03-PINS-AND-RESOURCES.md)
4. [Hard integration contracts and existing mismatches](04-INTERFACES.md)
5. [Power, charging and physical implementation](05-POWER-AND-PCB.md)
6. [Candidate BOM and alternatives](06-BOM.md)
7. [Bring-up, decision log, risks and integration handoff](07-INTEGRATION.md)

**Status meanings:** LOCKED = user requirement or unavoidable electrical boundary; RECOMMENDED = this revision's baseline; OPEN = choice awaiting agreement; NEEDS BENCHMARK = cannot be established without hardware/runtime evidence.

## What was verified

Recovered all 60 available turns of “Dub Box Layout,” through the oldest cursor, and inspected the live workstream files read-only. Checked manufacturer documentation, pin references and installed Teensy Audio driver source. Calculations are analytical. No physical hardware was connected; no electrical, acoustic, thermal or embedded ML benchmarks were performed. No schematics, Gerbers, purchases or sibling code changes were made.

The available conversation API returned no accessible attachments. Earlier referenced renderings and any context outside that 60-turn conversation are not part of the recovered evidence. The latest explicit layout correction takes precedence.

## Key convergence findings

- Firmware already has separate main and headphone audio frames but board wiring is disabled/unassigned.
- Stem Engine FLOAT32/source-rate output does **not** meet firmware's current PCM16/44.1 kHz/stereo import contract. A versioned preparation adapter is required.
- The current ML runtime's MLX backend is Apple-specific; it cannot simply be copied to CM5 or Jetson.
- The firmware's current audio output adapter quantizes to 16 bits. Selecting 24-bit converters alone does not deliver a 24-bit end-to-end instrument.
- No scope has been silently removed: ambitious DSP, polyphony, DAW behavior, stylus precision and onboard ML remain explicit validation or product-definition gates.

