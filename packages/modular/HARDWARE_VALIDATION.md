# Modular hardware validation — 2026-09-23

Both boards were updated with the combined modular, touchscreen pan and stem-panel
firmware. USB identities were checked before upload: Teensy 4.1 serial `19280080`
and ESP32 CP2102 serial `0001`. ESP32 upload hashes verified. Teensy upload finished
with `Booting`, returned at its expected USB serial identity, and ran the checks
below. Existing user projects were preserved. Tests used `MOD0923073143`, a new
project retained on SD for inspection.

## Checks passed on the boards

- Append four modular instruments; reject a fifth without changing the count.
- Send distinct patches and six P1 notes per instrument; read back all four exact
  cable/parameter payloads and note counts through the ESP32's `!state` diagnostic.
- Sequence 24 modular voices across four instruments.
- Reject cyclic audio routing and an audio-to-CV connection without modifying the
  valid patch.
- Close/save and reopen: preserve all four patches, all note counts and touchscreen
  pan settings `-750,500,0,0`.
- Delete slot 1: later patches and notes shift to the right instrument identities.
- Append an ordinary synth: save/reopen it alongside the remaining modular slots.
- Return to an idle, closed-project state and restart the recorder service.

## Host validation

AddressSanitizer/UBSan tests passed for graph topology, typed ports, fan-out,
serialization, atomic invalid edits, disconnected output silence, note release,
440/880 Hz oscillator pitch and six-voice finite/bounded output. Actual panel
handlers passed connection, rejection, unplug, audition and confirmed starter
restore checks. Renders cover four skins; Modern was visually inspected.

## Upload notes

The default PlatformIO Teensy uploader failed on its initial USB write. An exact
model invocation of `teensy_loader_cli --mcu=TEENSY41` succeeded after the bootloader
had settled. A later update also needed a retry after enumeration. Wait for the
normal Teensy serial identity to return before opening its port; two seconds was
not always enough on this setup. These retries did not alter SD projects.

The combined display's screen-mirror buffer remains 64 KiB, now allocated from
heap before the Wi-Fi workers start. Runtime diagnostics showed 68,480 free bytes,
a low-water mark of 58,752 bytes and a largest free allocation of 42,996 bytes during
the first checks. The restarted mirror reported LIVE, Menu, zero dropped records.

## Scope

This validates real firmware command exchange, scheduling and SD persistence.
Audio listening, physical LCD pixels and physical touch/button coverage are not
verified by these tests. The drawing-command mirror is not panel readback.
Final block-renderer tests measured 42% peak audio CPU for 24 saw/triangle voices
and 78% for 24 dual-sine voices, without WAV playback. The earlier scalar renderer
measured 87% peak for the saw/triangle test. The final optimized firmware was
uploaded successfully and both performance cases passed. Host tests additionally
compare the block renderer against the scalar reference for each waveform,
polyphony and release, within 0.00001. Maximum sustained workload still depends
on patch, tracks and effects.
