# Repository integration

The engine lives under `services/stem-engine` and keeps its own package, dependencies and tests.

## Current interface boundary

The engine publishes four FLOAT32 WAV masters at the decoded input sample rate and channel count, with `metadata.json` (`studio.stems.v1`). The firmware transfer profile is a different interface: 44100 Hz stereo PCM16 with `manifest.json` (`api_version: 1`). These schemas are not interchangeable.

The [preparation adapter](../../preparation-adapter/README.md) now transforms all four stems on one sample grid with common gain/headroom, emits the P1 manifest and validates it with the firmware importer. Native outputs stay unchanged. Synthetic end-to-end tests reach the host simulator; direct device OPEN IN DUB still needs the manifest-aware catalog/transfer service. See [shared architecture](../../../docs/SYSTEM_ARCHITECTURE.md) for implemented-versus-target status.

- [Firmware adapter guide](../../../firmware/teensy/INTEGRATION.md)
- [Transfer schema](../../../packages/contracts/stem-set/v1/manifest.schema.json)

## Validation of this import

The 20 engine correctness tests pass on the imported source. Historical synthetic model-inference results accompany the component. Generated audio, model weights, environments and compiler files are excluded. Real-music quality ranking remains unvalidated.
