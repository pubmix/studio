# Shared contracts

Integration authority: [STUDIO shared architecture I0.1](../../docs/SYSTEM_ARCHITECTURE.md). It distinguishes implemented behavior from proposed hardware and lists remaining integration gates.

`stem-set/v1/manifest.schema.json` describes the initial prepared-stem transfer manifest. The firmware validator additionally checks actual WAV headers, payload lengths and optional hashes; a schema alone cannot prove audio alignment. See the [integration guide](../../firmware/teensy/INTEGRATION.md).
