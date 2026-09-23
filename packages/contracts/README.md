# Shared contracts

`stem-set/v1/manifest.schema.json` describes the initial prepared-stem transfer manifest. The firmware validator additionally checks actual WAV headers, payload lengths and optional hashes; a schema alone cannot prove audio alignment. See the [integration guide](../../firmware/teensy/INTEGRATION.md).

[Stem-set v2](stem-set/v2/README.md) describes extended service exports with up to 16
mapped channels. Current firmware continues to accept v1 only.
