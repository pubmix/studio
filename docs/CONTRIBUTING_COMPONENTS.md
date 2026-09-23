# Adding STUDIO components

Keep all STUDIO work in this repository. Each component owns its source, dependencies, tests and README.

1. Keep the local separator under `services/stem-engine/`, preserving its package layout, dependency files and current README.
2. Keep firmware and desktop simulation under `firmware/teensy/`. The nested `firmware/STUDIO/` directory is the Arduino sketch and must retain its sketch-folder name.
3. Put future app frontends under `apps/<app-name>/`, hardware under `hardware/<board-or-enclosure>/`, and cross-component documentation under `docs/`.
4. Version shared interfaces under `packages/contracts/`. The initial stem-set v1 schema reflects the firmware transfer profile. Coordinate incompatible changes with both implementations instead of silently changing roles, order, format or alignment.
5. Use feature branches for incoming components and review integration before merging. Keep actual model weights and song data outside Git; document reproducible downloads and configuration.

## Integration contract

The local engine prepares exactly four synchronized WAVs. Firmware consumes those files through a versioned adapter; separation does not run inside the audio processing path. See `firmware/teensy/INTEGRATION.md`.

The local engine lives under `services/stem-engine`. It preserves FLOAT32 masters and now provides an explicit PCM16 transfer exporter plus a client for the active Dub-Box ESP32 WiFi upload interface. The active `dubbox-firmware` / `dubbox-display` application uses its existing SD file picker for track assignment. The older `firmware/teensy` importer remains compatible with exported manifests. See `services/stem-engine/docs/REPOSITORY_INTEGRATION.md`.

## Validation

Run `make test` at the repository root for firmware core and import tests. Each incoming component must document its own checks. Firmware build instructions are in its README. GitHub Actions runs the firmware checks on Linux and macOS. Extend it with each incoming component's checks once its toolchain is known.

## Repository hygiene

Do not commit credentials, local environments, compiler installations, generated binaries, personal recordings, separated songs, large models or datasets. Use GitHub Releases for reviewed firmware binaries, and appropriate external storage for large assets. No project-wide software license has been selected yet.

## Required Markdown documentation

Every new aspect or component folder must contain a `README.md` from its first commit. Update the README whenever its scope, usage, status or next steps change.

Include:

- Purpose and scope.
- Current state: proposed, implemented and tested work clearly separated.
- Important files and links to deeper design documents.
- Setup, build or usage instructions where applicable.
- Verification performed and known limitations.
- Open decisions and next steps.

Current aspect entry points: [Firmware](../firmware/README.md), [Hardware](../hardware/README.md), [Stem Engine](../services/stem-engine/README.md). Firmware-specific implementation instructions remain in [Teensy](../firmware/teensy/README.md). This convention applies to future aspects as they are added.
