#!/usr/bin/env python3
"""Dub-Box USB MIDI bridge.

Relays raw MIDI bytes between the Dub-Box's ESP32 serial port and a virtual MIDI
port on this computer, so a DAW sees the Dub-Box as a normal MIDI device.

Setup (one time):
  1. Install loopMIDI (free, https://www.tobias-erichsen.de/software/loopmidi.html)
     and create a port named "Dub-Box" (the name field at the bottom of its window,
     then the '+' button). Leave loopMIDI running.
  2. pip install mido python-rtmidi pyserial
  3. On the Dub-Box's touchscreen: MENU -> USB MIDI -> tap "BRIDGE: OFF" to turn it ON.
     (The debug log stops while it's on -- that's expected, not a fault.)

Run:
  python dubbox_midi_bridge.py COM6
(or whatever COM port the ESP32 shows up as in Device Manager; the same one you'd
use to flash it or open its serial monitor.)

Sending Dub-Box's faders to a DAW: point a MIDI track's input at the "Dub-Box" port
and MIDI-Learn a fader onto a parameter. No extra setup.

Playing the synths from an external MIDI controller: something has to route the
controller's output into this "Dub-Box" port -- the controller and the Dub-Box are
two separate MIDI devices, they don't see each other automatically.
  - With a DAW open: set a track's input to your controller and its output to the
    "Dub-Box" port, with input monitoring/thru on.
  - Without a DAW: use a small router app, e.g. Mididash
    (https://github.com/tiagolr/mididash, free/open source) -- connect your
    controller's output to the "Dub-Box" port's input.

This is a small, from-scratch script (not a third-party GUI app) specifically so it
doesn't depend on an unmaintained tool -- see DUBBOX_PROJECT_SUMMARY.md for why.
"""
import argparse
import sys
import threading
import time

import mido
import serial


def find_loop_port(name_hint):
    names = mido.get_output_names()
    matches = [n for n in names if name_hint.lower() in n.lower()]
    if not matches:
        print(f"No MIDI port matching '{name_hint}' found. Is loopMIDI running with a")
        print(f"port named '{name_hint}'? Available ports:")
        for n in names:
            print(f"  - {n}")
        sys.exit(1)
    return matches[0]


def serial_to_midi(ser, out_port, stop):
    """Reads raw MIDI bytes from the Dub-Box and forwards them into the virtual MIDI port."""
    status = None
    data_needed = 0
    data = []
    while not stop.is_set():
        b = ser.read(1)
        if not b:
            continue
        byte = b[0]
        if byte & 0x80:
            hi = byte & 0xF0
            if hi == 0xF0:  # system/realtime: not handled here
                status = None
                continue
            status = byte
            data_needed = 1 if hi in (0xC0, 0xD0) else 2
            data = []
            continue
        if status is None:
            continue
        data.append(byte)
        if len(data) >= data_needed:
            msg_bytes = [status] + data
            try:
                out_port.send(mido.Message.from_bytes(msg_bytes))
            except (ValueError, IndexError):
                pass  # malformed message: drop it, keep going
            data = []  # running status: next data bytes reuse the same status byte


def midi_to_serial(in_port, ser, stop):
    """Reads MIDI (e.g. an external keyboard routed to this port) and sends raw bytes to the Dub-Box."""
    for msg in in_port:
        if stop.is_set():
            break
        if msg.type in ("note_on", "note_off"):
            ser.write(bytes(msg.bytes()))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="Dub-Box's serial port, e.g. COM6")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--midi-name", default="Dub-Box", help="loopMIDI port name to use (default: Dub-Box)")
    args = ap.parse_args()

    loop_name = find_loop_port(args.midi_name)
    print(f"Using MIDI port: {loop_name}")
    print(f"Opening {args.port} @ {args.baud} baud...")

    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    out_port = mido.open_output(loop_name)
    in_port = mido.open_input(loop_name)
    stop = threading.Event()

    t1 = threading.Thread(target=serial_to_midi, args=(ser, out_port, stop), daemon=True)
    t2 = threading.Thread(target=midi_to_serial, args=(in_port, ser, stop), daemon=True)
    t1.start()
    t2.start()

    print("Bridge running. Ctrl+C to stop.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        ser.close()
        out_port.close()
        in_port.close()


if __name__ == "__main__":
    main()
