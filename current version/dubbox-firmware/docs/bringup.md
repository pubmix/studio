# Phase 1 hardware bring-up

Step-by-step for going from "parts on the bench" to "4 tracks playing back
with fader volume control."

## 1. Assemble the stack

1. Seat the **Teensy Audio Shield** onto the **Teensy 4.1** header pins.
   Double-check pin 1 alignment before pressing down — it's easy to offset
   by one row on a bare board with no silkscreen keying visible once
   stacked. Press evenly, all pins fully seated.
2. Don't plug in USB yet.

## 2. Wire the 4 faders

Each fader is a linear potentiometer with 3 legs:

- Outer leg 1 → **3.3V** (Teensy 4.1's `3.3V` pin — **not** `VIN`/5V, the
  ADC inputs are not 5V-tolerant)
- Outer leg 2 → **GND**
- Middle leg (wiper) → analog input pin

Wiper connections, per [pin_map.md](pin_map.md):

| Fader | Teensy pin |
|---|---|
| 1 | A0 |
| 2 | A1 |
| 3 | A2 |
| 4 | A3 |

It doesn't matter electrically which outer leg goes to 3.3V vs GND — it
just flips which physical direction is "up" for volume. Wire them all the
same way so the 4 faders behave consistently.

## 3. Prepare the SD card

1. Format a microSD card FAT32 (exFAT works on the Teensy 4.1 too, but
   FAT32 is the safe default).
2. Copy 4 WAV files to the card **root**, named `TRACK1.WAV` … `TRACK4.WAV`
   (16-bit PCM, 44.1kHz, mono or stereo). See [../data/README.md](../data/README.md).
3. Insert into the Teensy 4.1's **built-in** microSD slot (bottom of the
   board, underneath) — not any slot on the Audio Shield itself.

## 4. Connect headphones/speaker

Plug headphones into the Audio Shield's headphone jack for the first test.
(Per the build brief, driving a speaker directly needs a Class-D amp board
you haven't added yet — don't wire a speaker straight to the headphone out.)

## 5. Install the Teensy USB driver (Windows)

Plug the Teensy 4.1 into your PC via micro-USB. Windows 10 usually detects
it automatically (it enumerates as a serial port + HalfKay bootloader
device). If `pio run -t upload` in the next step can't find the board,
install PJRC's driver: https://www.pjrc.com/teensy/td_download.html → grab
just the Windows serial installer (you don't need the full Arduino IDE).

## 6. Build and flash

Run these from a terminal — Windows Terminal, PowerShell, or the terminal
panel in VS Code/your editor if you have one open. Any of them work
identically since `pio` is a normal command-line tool.

1. **Open a terminal and move into the project folder.** This is the
   folder with `platformio.ini` in it — not a subfolder like `src/`:

   ```powershell
   cd "C:\Users\Kyle\Documents\DubBoc claude code\dubbox-firmware"
   ```

2. **Plug the Teensy 4.1 into your PC** via micro-USB, if it isn't already.
   (First time on this machine? See the driver note in step 5 above — most
   likely it'll just work, Windows 10 usually enumerates it fine.)

3. **Build** — this only compiles and checks for errors, it does not touch
   the board yet:

   ```powershell
   pio run
   ```

   Should end with `[SUCCESS]` (already verified clean as of 2026-07-20).
   If `pio` isn't recognized, open a **new** terminal window — it was just
   added to your PATH and existing windows won't see it until reopened.

4. **Flash** — this is the step that actually writes the firmware onto the
   Teensy:

   ```powershell
   pio run -t upload
   ```

   What you should see happen: PlatformIO rebuilds if anything changed,
   then invokes `teensy_loader_cli`, which looks for the board and
   auto-reboots it into bootloader mode to write the new firmware. The
   Teensy's LED will blink differently during the flash. It finishes with
   `[SUCCESS]` and the board immediately starts running the new code.

5. **If it hangs** at "Teensy did not respond" or similar instead of
   finding the board: press the small physical pushbutton on the Teensy
   4.1 itself. This is normal and expected on a **factory-fresh** board,
   since there's no prior sketch installed yet to respond to the
   soft-reboot signal PlatformIO sends. After this first flash, later
   uploads should auto-reboot without needing the button.

6. **If it can't find the board at all** (no COM port), check it's
   detected:

   ```powershell
   pio device list
   ```

   You should see a COM port appear when the Teensy is plugged in and
   disappear when unplugged. If nothing shows up, it's a driver/cable
   issue (see step 5 above) — try a different USB cable too, some
   micro-USB cables are charge-only with no data lines.

## 7. Check it's alive

```
pio device monitor
```

At 115200 baud you should see either nothing (success — the code stays
silent unless a track fails) or lines like:

```
Track 1: failed to start TRACK1.WAV
```

If you see failures for all 4 tracks, check: SD card seated, filenames
exact (case doesn't matter but spelling does), files are valid WAV.
If you see `Audio engine init failed — check the SD card.` and the board
sits in the `while(true)` loop, the SD card isn't mounting at all — reseat
it, try a different card, or try reformatting FAT32.

## 8. Verify audio + fader control

1. With no failures logged, you should hear all 4 tracks mixed together in
   the headphones.
2. Turn fader 1 down — track 1 should fade out while 2–4 keep playing, and
   back up smoothly (no scratchy jumps — that'd indicate the fader wiring
   or wiper connection is bad, not a code issue).
3. Repeat for faders 2–4.
4. If a fader does nothing: check continuity on its 3 legs and confirm it
   landed on the pin from the table above, not off-by-one.
5. If a fader is scratchy/jumps abruptly instead of smooth: usually a dirty
   pot, a loose wiper wire, or (rarely) needs more smoothing than the
   firmware's default EMA — that's a one-line constant change in
   [../src/input_manager.cpp](../src/input_manager.cpp) (`kFaderSmoothing`), not a wiring fix.

## Known limits at this phase

No screen, no effects, no mute/transport buttons yet — those are later
phases. This phase only proves: SD playback, the 4-track audio graph, and
live fader-to-gain control.
