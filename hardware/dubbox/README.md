# Dub-Box hardware notes

## Purpose and scope

Wiring drawings, analog input design notes and the protoboard layout generator for the Dub-Box prototype
(Teensy 4.1 + Audio Shield, ESP32 touchscreen). The firmware that runs on it is in
[`../../dubbox-firmware`](../../dubbox-firmware/HANDOFF.md) and [`../../dubbox-display`](../../dubbox-display/README.md).

## Status

Breadboard prototype working; being condensed onto perfboards in a wooden enclosure. A set of KiCad PCB drafts was
made and then paused; **those PCB files are not in this repository**. Nothing here is a production design.

## Files

| File | Contents |
|---|---|
| `DUBBOX_INPUT_WIRING_NO_PHANTOM.svg` | Current analog input stage (INA163 instrumentation amp, TL072 buffer, ICL7660S negative supply), phantom power left out |
| `DUBBOX_INSTRUMENT_BUFFER_WIRING.svg` | Close-up of the instrument buffer |
| `DUBBOX_XLR_DYNAMIC_MIC_WIRING.svg` | XLR dynamic microphone path |
| `DUBBOX_INPUT_WIRING_FINAL.svg` | Older version that included phantom power (postponed, do not use) |
| `DUBBOX_INPUT_PROTOBOARD_LAYOUT.svg`, `..._40x26.svg` | Generated perfboard layouts for the input stage (rev 2, not built) |
| `DUBBOX_ANALOG_FRONTEND_ORDER_BRIEF.md` | Original parts brief; still describes an older MCP6002-based design |
| `tools/protoboard_layout/` | Node scripts that generate the perfboard layout (see its `README.txt`) |
| `tools/dubbox_midi_bridge.py` | PC-side serial-MIDI bridge helper |

## Validation performed

The analog stage was built on a breadboard and verified by measurement for channel 1 (XLR dynamic mic and
instrument paths). A steady 14.5 kHz tone from the charge pump leaks into the line inputs and is removed digitally
by a notch filter in the firmware; the real fix is analog supply filtering (included in the rev 2 layout, not built).

## Limitations and next steps

Second input channel and the second negative-supply chip are not built; phantom power is postponed. The enclosure
design (wooden box about 11 x 6 x 2-3 in, controls in one landscape row) still needs its measurements and layout.
Wiring of the controls to the Teensy is in `dubbox-firmware/src/pins.h` (source of truth).
