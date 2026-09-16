# Verification — 16 September 2026

**19 tests passed**, including the current sibling firmware's validate_prepared.py accepting generated STUDIO-P1 media.

Tested: 48k→44.1k stereo and 32k→44.1k mono; unchanged 44.1k frame count; shared gain bounded against the resampled signal and reconstructed sum; <=1.51 LSB per-sample conversion error; mono L/R equality; shared impulse arrival at frame 1470; silence; deterministic output; registry reuse/new IDs and two simultaneous producer processes; missing/corrupt hashes; missing stems; wrong role ordering; invalid types/channels; nonfinite audio; symlinks/path traversal; truncated payload with recomputed hash; wrong native subtype; corrupt existing prepared output; publication-failure cleanup; prohibition on writing inside native source. Source bytes remained identical after preparation.

Executed with the existing Stem Engine Python 3.11 environment:
NumPy 2.4.6, SciPy 1.17.1, SoundFile 0.14.0.
Full run: 19 tests in 1.266 seconds, OK.

## Read-only source evidence

Native engine.py SHA-256:
74b634bdc7e074d406f30ffdd8a47eeb618a9922b9aa6a9b20408229c66cbeb8

Firmware tools/validate_prepared.py SHA-256:
7f6735c38d1ada9bfd5892e60489432490aec865d45c90566e252a964f8cfe94

Source paths are in the sibling studio-stem-engine and studio-teensy-firmware workspaces. The firmware set ID was checked against its uint32_t StemSet.id field. The authoritative proposed conversion policy was read from hardware architecture 04-INTERFACES.md.

## Practical limits

This establishes synthetic conversion and host-validator compatibility, not actual Teensy streaming, listening quality, model performance or electrical validity. The preparation provenance sidecar is descriptive and is not authenticated. Artifact hashes detect accidental corruption; no signature/authentication scheme is provided. A local trusted filesystem and cooperating writers sharing the registry are assumed. Power-loss behavior on removable media requires the separately specified transfer journal.

No source conversation, personal music, new model work or controller changes are part of this deliverable.
