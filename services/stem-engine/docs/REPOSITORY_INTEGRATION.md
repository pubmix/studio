# Repository integration

The engine lives under `services/stem-engine` and keeps its own package, dependencies and tests.

## Current interface boundary

The engine publishes four FLOAT32 WAV masters at the decoded input sample rate and channel count, with `metadata.json` (`studio.stems.v1`). The firmware transfer profile is a different interface: 44100 Hz stereo PCM16 with `manifest.json` (`api_version: 1`). These schemas are not interchangeable.

A future explicit transfer exporter must transform all four stems on the same sample grid, use one shared gain/headroom policy before integer encoding, emit the shared manifest and validate it with the firmware importer. This import does not implement that exporter or change either interface. Direct OPEN IN DUB integration is therefore still pending.

- [Firmware adapter guide](../../../../firmware/teensy/INTEGRATION.md)
- [Transfer schema](../../../../packages/contracts/stem-set/v1/manifest.schema.json)

## Validation of this import

The 20 engine correctness tests pass on the imported source. Historical synthetic model-inference results accompany the component. Generated audio, model weights, environments and compiler files are excluded. Real-music quality ranking remains unvalidated.
