# Adding STUDIO components

Keep all STUDIO work in this repository. Each component owns its source, dependencies, tests and README.

1. Add the local separator under `services/stem-engine/`, preserving its package layout and dependency lockfiles. Replace that directory's placeholder README with its actual instructions.
2. Keep firmware and desktop simulation under `firmware/teensy/`. The nested `firmware/STUDIO/` directory is the Arduino sketch and must retain its sketch-folder name.
3. Put future app frontends under `apps/<app-name>/`, hardware under `hardware/<board-or-enclosure>/`, and cross-component documentation under `docs/`.
4. Version shared interfaces under `packages/contracts/`. The initial stem-set v1 schema reflects the firmware transfer profile. Coordinate incompatible changes with both implementations instead of silently changing roles, order, format or alignment.
5. Use feature branches for incoming components and review integration before merging. Keep actual model weights and song data outside Git; document reproducible downloads and configuration.

## Integration contract

The local engine prepares exactly four synchronized WAVs. Firmware consumes those files through a versioned adapter; separation does not run inside the audio processing path. See `firmware/teensy/INTEGRATION.md`.

The local engine is imported under `services/stem-engine`. It produces FLOAT32 master WAVs with its own metadata; the explicit PCM16 transfer exporter remains to be implemented. See `services/stem-engine/docs/REPOSITORY_INTEGRATION.md`.

## Validation

Run `make test` at the repository root for firmware core and import tests. Each incoming component must document its own checks. Firmware build instructions are in its README. GitHub Actions runs the firmware checks on Linux and macOS. Extend it with each incoming component's checks once its toolchain is known.

## Repository hygiene

Do not commit credentials, local environments, compiler installations, generated binaries, personal recordings, separated songs, large models or datasets. Use GitHub Releases for reviewed firmware binaries, and appropriate external storage for large assets. No project-wide software license has been selected yet.
