# Extended prepared stem sets v2

Implemented service export contract, not a supported firmware import format.
`manifest.schema.json` documents synchronized 44100 Hz stereo PCM16 WAVs with
4–16 channel entries. Each entry has a unique role, filename, one-based channel,
bank (four channels each), lane and canonical VOCALS/MELODY/BASS/RHYTHM family.
Engine verification additionally enforces taxonomy, canonical family order,
channel/bank/lane agreement, hashes and actual WAV synchronization/format.
JSON Schema alone does not validate those cross-entry or audio properties.

Use `export_transfer(..., extended=True)` and `verify_transfer(..., extended=True)`
from the existing stem-engine. V1 and the four-stem device upload are unchanged.
The four-stem mixdown is a separate set; do not play it alongside its source channels.

Validation: service contract tests and real Demucs six-source export verification.
Next: implement explicit firmware capability negotiation, banked playback and v2
project assignment before accepting these manifests on hardware. See the service's
[extended documentation](../../../../services/stem-engine/docs/EXTENDED_STEMS.md).
