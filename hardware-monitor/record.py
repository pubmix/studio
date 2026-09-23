#!/usr/bin/env python3
"""Bounded, reconnecting USB serial recorder; never sends application commands."""
import argparse
import fcntl
import json
import logging
from logging.handlers import RotatingFileHandler
import os
from pathlib import Path
import signal
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from datetime import datetime, timezone

import serial
from serial.tools import list_ports
from mirror import Mirror

ROOT = Path(__file__).resolve().parents[1]
DATA = Path(os.environ.get('STUDIO_MONITOR_DATA', str(ROOT / 'monitor-data')))
DEVICES = {
    'esp32': (0x10C4, 0xEA60, '0001'),
    'teensy': (0x16C0, 0x04D5, '19280080'),
}
stop = threading.Event()
lock = threading.Lock()
status = {}
mirror = Mirror(DATA / 'display-live.png')


class Viewer(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        path = self.path.split('?', 1)[0]
        if path == '/':
            body = Path(__file__).with_name('viewer.html').read_bytes()
            kind = 'text/html; charset=utf-8'
        elif path == '/status':
            with lock:
                body = json.dumps(dict(mirror=mirror.state(), boards=status)).encode()
            kind = 'application/json'
        elif path == '/frame.png':
            with mirror.lock:
                body = mirror.image
            if body is None:
                self.send_error(503, 'Waiting for a complete display frame')
                return
            kind = 'image/png'
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


def utc():
    return datetime.now(timezone.utc).isoformat()


def worker(name, identity, logger):
    def event(kind, **values):
        entry = dict(at=utc(), monotonic=time.monotonic(), board=name, kind=kind, **values)
        logger.info(json.dumps(entry, ensure_ascii=True))
        with lock:
            status[name].update(last_event=entry)
            if kind == 'serial':
                status[name]['lines'] += 1
                status[name]['last_data_at'] = entry['at']

    with lock:
        status[name] = dict(connected=False, lines=0, expected_usb=list(identity))
    while not stop.is_set():
        connection = None
        try:
            matches = [p for p in list_ports.comports()
                       if (p.vid, p.pid, p.serial_number) == identity]
            if len(matches) != 1:
                stop.wait(2)
                continue
            port = matches[0].device
            connection = serial.Serial(port=None, baudrate=921600 if name == 'esp32' else 115200, timeout=0.05)
            connection.dtr = False
            connection.rts = False
            connection.port = port
            connection.open()
            # Teensy USB Serial uses DTR to know that a host is listening.
            # ESP32 modem lines stay deasserted to avoid intentional resets.
            if name == 'teensy':
                connection.dtr = True
            with lock:
                status[name].update(connected=True, port=port, connected_at=utc())
            event('connected', port=port)
            pending = bytearray()
            while not stop.is_set():
                chunk = connection.read(4096)
                if not chunk:
                    continue
                pending.extend(chunk)
                while b'\n' in pending:
                    raw, _, rest = pending.partition(b'\n')
                    pending = bytearray(rest)
                    if name == 'esp32' and mirror.accept(raw):
                        if b',T,' in raw:
                            event('touch', text=raw.decode('ascii', errors='replace'))
                        continue
                    event('serial', text=raw.rstrip(b'\r').decode('utf-8', errors='replace'))
                if len(pending) > 16384:
                    event('framing_error', message='Discarded overlong unterminated serial data', bytes=len(pending))
                    pending.clear()
        except (serial.SerialException, OSError) as exc:
            event('error', message=str(exc))
        finally:
            if connection is not None:
                connection.close()
            if name == 'esp32':
                mirror.disconnect()
            with lock:
                status[name]['connected'] = False
        stop.wait(2)


def run():
    DATA.mkdir(parents=True, exist_ok=True)
    guard = open(DATA / 'recorder.lock', 'a')
    try:
        fcntl.flock(guard, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise SystemExit('Recorder is already running.')
    (DATA / 'pid').write_text(str(os.getpid()))
    logger = logging.getLogger('hardware')
    logger.setLevel(logging.INFO)
    handler = RotatingFileHandler(DATA / 'events.jsonl', maxBytes=5_000_000, backupCount=5)
    handler.setFormatter(logging.Formatter('%(message)s'))
    logger.addHandler(handler)
    server = ThreadingHTTPServer(('127.0.0.1', 8765), Viewer)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stop.set())
    workers = [threading.Thread(target=worker, args=(name, identity, logger), daemon=True)
               for name, identity in DEVICES.items()]
    for thread in workers:
        thread.start()
    while not stop.wait(1):
        with lock:
            snapshot = dict(updated_at=utc(), pid=os.getpid(), boards=dict(status), mirror=mirror.state())
            temp = DATA / 'status.tmp'
            temp.write_text(json.dumps(snapshot, indent=2))
            temp.replace(DATA / 'status.json')
    for thread in workers:
        thread.join(timeout=3)
    server.shutdown()
    with lock:
        (DATA / 'status.json').write_text(json.dumps(dict(updated_at=utc(), stopped=True, boards=status), indent=2))
    (DATA / 'pid').unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--background', action='store_true')
    args = parser.parse_args()
    if args.background:
        DATA.mkdir(parents=True, exist_ok=True)
        with open(DATA / 'process.log', 'ab') as log:
            child = subprocess.Popen([sys.executable, str(Path(__file__).resolve())],
                                     stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                     start_new_session=True)
        print(f'Recorder starting, PID {child.pid}; status: {DATA / "status.json"}')
    else:
        run()
