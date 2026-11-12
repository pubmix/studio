# Portable snapshot usage

This copy relocates recorder output under this `current version` folder. Install
Python dependencies `pyserial` and `Pillow`, then run `python3 record.py` from
this folder and open http://127.0.0.1:8765/. Close it with Ctrl-C. The current
ESP32 rate is 460800; Teensy is 115200. Device VID/PID/serial tuples in `record.py`
identify the development boards; update them explicitly for different hardware.
Do not run a second serial recorder or flash while this process owns the ports.
Generated `../work` and `../outputs` are ignored by the snapshot's `.gitignore`.
The original development notes below include historical launcher paths and rates;
use the commands in this section for this packaged copy.

# Dub-Box live screen and serial monitor

A local, view-only screen mirror at http://127.0.0.1:8765/ plus bounded, timestamped
USB diagnostics from the ESP32 and Teensy. The mirror reconstructs actual firmware
drawing commands; it is not a camera image or display-memory readback.

## Start and stop

Run `outputs/open-live-screen.command` from this Codex task. It starts the recorder
if needed and opens the viewer. Alternatively use `work/pio-env/bin/python
outputs/studio/hardware-monitor/record.py --background` from the task root.

The current ESP32 diagnostics and Teensy use 115200 baud. Older ESP32 mirror
firmware uses 921600; select it with STUDIO_ESP32_BAUD=921600. Expected USB identities
are checked before opening either port. The recorder reconnects to those same
identities. It sends no application commands. Opening the ESP32 port has been
observed to reset the board despite deasserted DTR/RTS; reconnect is not reset-free.
Teensy DTR is asserted so USB diagnostics are enabled.

A macOS login service now keeps the recorder running: `com.pubmix.dubbox-monitor`.
It starts at login and restarts after process exit, with a 15-second throttle.
It runs while this Mac is awake and the user is logged in; it does not prevent sleep.

Before flashing, unload the service with
`launchctl bootout gui/$(id -u)/com.pubmix.dubbox-monitor` and wait for the PID file
to disappear. Do not merely terminate its PID: KeepAlive would reopen the ports.
After flashing, reload it with
`launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.pubmix.dubbox-monitor.plist`.
Recording does not keep the assistant reasoning between conversation turns.
A process restart does not by itself guarantee mirror resynchronization; check LIVE.

## Screen stream

The ESP32 emits checksummed, sequenced `@V1` drawing records for rectangles, font
glyphs and layer copies. The host uses the exact two bitmap fonts from the installed
Adafruit GFX library. Four 1280 x 720 layers are reconstructed on the computer.
The ESP32 buffers at most 64 KiB of trace records and writes only when UART buffer
space is available. It drops telemetry instead of waiting if that queue fills.

Frame markers arrive at up to 10 Hz; changed PNGs are published at up to 5 Hz.
Actual refresh speed depends on firmware drawing work and USB throughput. The
viewer checks freshness, sequence continuity, checksums and the overflow counter.
A broken stream is marked invalid rather than silently presented as a current
screen. Recovery needs a complete startup stream: restart the recorder and, if
it is still waiting, reset the ESP32. This interrupts its UI session.

Use the physical touchscreen to navigate. Browser clicks do not control the
hardware. Touch down/up and sampled movement coordinates are recorded; this does
not yet include every multi-touch contact or every Teensy physical button edge.
A quiet log is not evidence that no physical button was pressed.

## Files

- `record.py`: serial ownership, reconnects, rotating logs and loopback-only viewer.
- `mirror.py`: checked protocol decoder and exact bitmap renderer.
- `viewer.html`: live image, screen name, connection state and touch coordinates.
- `fonts.json`: FreeSansBold12pt7b and FreeSansBold18pt7b bitmap data extracted from
  Adafruit GFX Library 1.12.6, preserving the firmware glyph metrics.
- `work/hardware-monitor/events.jsonl`: combined events, six 5 MB files maximum.
- `work/hardware-monitor/status.json`: latest board and mirror status each second.
- `outputs/display-live.png`: latest complete image, overwritten on updates.

Work/output paths are relative to the Codex task, not the repository. Always check
status freshness and `mirror.live` before treating the PNG as current. Drawing
records are rendered in memory, not accumulated as an unbounded video recording.

## Validation

On 2026-09-22 the ESP32 firmware built and flashed successfully, with uploader hash
verification. Live startup produced a complete full-resolution main menu including
SETTINGS. The local browser showed LIVE, zero dropped records and Teensy connected.
The generated image was visually inspected. Host checks covered RGB565 color,
layer copying, bitmap glyph rendering, PNG creation, sequence gaps, bad checksums
and reset recovery. Touch and all project screens have not yet been exercised in
a hardware test session. The Teensy firmware was not changed.

The viewer proves what drawing commands the ESP32 issued, not that the physical
panel displayed them correctly. Panel/backlight faults need physical observation.
Controller reference: https://www.levetop.cn/uploadfiles/2023/05/LT768x_DS_V42_ENG.pdf

## Stem screen / memory placement

The native Stems screen is appended to the diagnostic screen-name map. The ESP32
still uses the full 64 KiB bounded drawing-command queue, now allocated on heap before
Wi-Fi initialization so the combined touchscreen HTTP client fits the static DRAM
segment. Allocation failure is logged explicitly; verify LIVE and dropped counters
after deployment. Neither physical LCD readback nor remote touch injection was added.

## Diagnostic speed correction (2026-11-11)

The connected ESP32 firmware was directly observed emitting readable startup and
link diagnostics at 115200 baud; 921600 produced framing errors. The recorder now
defaults to 115200 for this device. Set STUDIO_ESP32_BAUD=921600 for the older
drawing-mirror firmware. Current firmware has not emitted mirror records, so a
waiting mirror is not evidence of a crashed display. Serial capture was verified
at 115200; Python syntax was checked. Firmware was not changed.

## Current handoff mirror compatibility

The host renderer supports five layers, including the current firmware staging
layer, and the twelve-screen handoff enum (Menu through Controls). The matching
firmware retains 115200-baud diagnostics. Host checks passed for staging copies,
screen names and invalidation after dropped records. Live status, rather than
the existence of a saved PNG, remains the authority for whether the view is current.

The restored drawing-mirror build uses 921600 baud to avoid overflowing its
bounded queue during full-screen startup drawing. The recorder default now
matches that build; set STUDIO_ESP32_BAUD=115200 only for the earlier diagnostic
firmware. The Teensy remains at 115200.

USB receive now runs separately from glyph/PNG rendering, with a bounded 4 MiB
queue. A receive overflow invalidates the stream instead of silently losing
records. The recorder remains the only owner of each serial port.

Final tested transfer rate: 460800 baud for the ESP32 (Teensy remains 115200).
The main-menu mirror rendered LIVE with zero dropped records after a verified
flash. Full navigation stress testing remains pending. Raw diagnostic receive
capture is bounded to approximately 5 MB per connection in work/hardware-monitor.
