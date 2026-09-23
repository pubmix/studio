# Published device and companion snapshot

This snapshot includes the currently integrated ESP32 touchscreen/Wi-Fi splitter,
lossless compressed transfer, matching Teensy firmware, companion APIs/UI, extended
separation support, skins, track pan and modular instrument changes already installed
on the shared device. It preserves the four-track device playback contract.

Prior physical validation on 2026-09-23: combined ESP32 upload hash verified; native
client pairing succeeded; generated audio separated into six sources without fallback;
four compressed WAV mixdowns saved to SD through the new panel API. No SD readback
checksum or playback/audio-quality validation is claimed. Source and pairing data
are not included in this repository.

Transfer fixture measurements: a 352,844-byte generated WAV transferred as 189,413
bytes, taking 6.97 versus 8.28 seconds. Four music stems showed 12.8–23.7% size savings.
These are fixture-specific, not promised song throughput. Decompression preserves
WAV bytes and serial/SD traffic stays uncompressed.

The repository includes a portable Mac launcher, POSIX setup, dependency declarations,
and the new-computer guide. Windows native companion support is not implemented.
Model weights download separately. No new trained model or quality superiority is claimed.

## Checks on the publication checkout

- Companion: 79 tests passed (two existing dependency deprecation warnings).
- Browser upload regression tests passed, including staging, lossless roundtrip,
  original preservation, capability checks and partial failures.
- Firmware core: 5,153 checks in 11 groups; eight importer tests passed.
- Fresh ESP32 PlatformIO build passed (51 seconds).
- Fresh Teensy 4.1 PlatformIO build passed (47 seconds).
- Launcher/setup shell syntax checks passed. These are not a clean-machine package
  installation test; the model runtime was already installed on the validation Mac.

The publication changes add setup/docs and port selection, and relocate optional
monitor output into a checkout-local ignored directory. The installed firmware's
functional code is preserved; no additional device reflash is needed to publish it.
