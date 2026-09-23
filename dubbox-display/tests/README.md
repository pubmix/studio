# Display host validation

`python3 tests/render_ui.py /tmp/studio-ui-previews` builds the actual theme,
shared text/button/header/transport drawing, mixer, menu, settings and project
library code with clang++. Run a PlatformIO build first to install the GFX fonts.
It replaces only hardware drawing/copy operations with an RGB565 software canvas
and stubs Preferences and Teensy state. It emits sixteen PPM images with sample
data (four screens by four skins), not live device captures.

Assertions cover Modern default, corrupt stored values, selecting all four skins,
readback through simulated restart, invalid selection, storage failure, and
settings touch targets. Preferences is mocked: real NVS persistence and panel
interaction still require device testing. Audio/serial timing is outside this
harness. The assertions and sixteen renders passed on macOS on 2026-09-22.

## Wi-Fi upload tests

Run `node tests/test_upload_page.cjs` from `dubbox-display` using Node 22+ (tested 24).
This executes the actual inline page script with simulated DOM/HTTP, real Compression
Streams, byte-exact original/compressed checks, capability and pairing errors, extended
split to ordered four-stem transfer, and confirmed partial failure reporting. It also
checks compression expansion falls back to original. It does not drive physical hardware.

Build/run the bounded C++ parser tests:

```sh
c++ -std=c++17 -fsanitize=address,undefined tests/test_transfer_blocks.cpp -lz -o /tmp/studio-transfer-tests
/tmp/studio-transfer-tests
```

On this Mac add `-isystem /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1`.
Host tests use zlib in place of ROM tinfl. Optional test CLI arguments are
`encoded-file decoded-size output-file`; use these with the page test's optional output
prefix to verify browser-compressed bytes through the actual C++ parser. All tests
passed 2026-09-23; see [protocol and hardware limitations](../WIFI_UPLOAD.md).

## Modular rack

`python3 tests/render_modular.py /tmp/studio-modular-preview` reuses the real UI
software renderer and compiles `modular_panel.cpp`. It additionally renders the
rack in all four skins and tests the actual touch handlers for valid/invalid
cables, unplugging, audition and confirmed starter restoration. These tests passed
on 2026-09-23; the Modern render was visually inspected. They do not emulate the
physical touch controller or serial link. DSP/schema tests live in
`../packages/modular/tests/test_modular.cpp` (relative to the display folder).

Pan assertions also drive the actual mixer touch handlers: full right, full left
clamping, CENTER reset, and independent second-track adjustment. UART is stubbed
with state acknowledgements. These tests do not establish physical audio behavior.

## Native stem panel tests

Run `python3 tests/render_stems.py /tmp/studio-stem-previews`. It reuses the real-font
host canvas and first validates/renders the existing screens, then compiles the actual
stem screen and keyboard with a simulated companion. Assertions cover menu entry,
rights gating, depth/transfer selection, processing/delivery and cancellation. Four
additional PPM previews are emitted for visual QA; these are not hardware captures.
All assertions passed and layouts were inspected on 2026-09-23.

Transport regression assertions in `render_ui.py` exercise the user's Play touch
coordinates, blocked song/pattern/record buttons during SD writes, and recovery
when saving ends. The test uses the real transport drawing and hit-test functions;
it does not claim a physical finger or listening test.
