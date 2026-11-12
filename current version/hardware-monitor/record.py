#!/usr/bin/env python3
"""Bounded, reconnecting USB serial recorder; only sends bounded display-snapshot requests; never resets a board."""
import argparse
import fcntl
import json
import logging
import queue
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

ROOT = Path(__file__).resolve().parents[1]  # portable snapshot root
DATA = ROOT / 'work' / 'hardware-monitor'
DEVICES = {
    'esp32': (0x10C4, 0xEA60, '0001'),
    'teensy': (0x16C0, 0x04D5, '19280080'),
}
stop = threading.Event()
lock = threading.Lock()
status = {}
mirror = Mirror(ROOT / 'outputs' / 'display-live.png')


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


class ReconnectBackoff:
    """Fast initial retry; bounded polling after repeated failures, never a hot loop."""
    def __init__(self):
        self.attempt = 0

    def next_delay(self, connected_seconds=0.0):
        # Opening a flapping port is not success. Reset only after a stable session.
        if connected_seconds >= 5.0:
            self.attempt = 0
        delay = (0.1, 0.2, 0.4, 0.8, 1.0)[min(self.attempt, 4)]
        self.attempt = min(self.attempt + 1, 4)
        return delay


def is_mirror_record(raw):
    # Never write ASCII commands into a port currently carrying raw USB-MIDI.
    try:
        payload, checksum = raw.strip().rsplit(b'*', 1)
        if not payload.startswith(b'@V1,'):
            return False
        value = 2166136261
        for byte in payload:
            value = ((value ^ byte) * 16777619) & 0xffffffff
        return value == int(checksum, 16)
    except (ValueError, TypeError):
        return False


def is_resync_capability(raw):
    return is_mirror_record(raw) and raw.strip().rsplit(b'*', 1)[0].split(b',')[2:] == [b'H', b'RESYNC', b'1']


class SnapshotRequests:
    """Coalesce invalid-frame recovery; no reset, playback or project commands."""
    def __init__(self, now):
        self.next_at = now + 1.0  # Let an ordinary startup stream arrive first.
        self.attempt = 0
        self.request_id = 0
        self.healthy = False
        self.receiving = False

    def due(self, now, state):
        receiving = state.get('snapshot_active', False)
        if receiving and not self.receiving:
            self.next_at = max(self.next_at, now + 8.0)
        self.receiving = receiving
        healthy = state.get('valid', False) and not state.get('awaiting_complete', True)
        if healthy:
            self.healthy = True
            self.attempt = 0
            self.next_at = now
            return None
        if self.healthy:
            self.healthy = False
            self.next_at = now + 0.1
        if now < self.next_at:
            return None
        self.request_id = self.request_id % 4294967295 + 1
        self.next_at = now + (2.0, 4.0, 8.0)[min(self.attempt, 2)]
        self.attempt = min(self.attempt + 1, 2)
        return self.request_id


class PortUnavailable(Exception):
    pass


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
    backoff = ReconnectBackoff()
    while not stop.is_set():
        connected_since = None
        connection = None
        reader_stop = threading.Event()
        reader = None
        try:
            matches = [p for p in list_ports.comports()
                       if (p.vid, p.pid, p.serial_number) == identity]
            if len(matches) != 1:
                with lock:
                    status[name].update(connection_state='waiting_for_usb', matching_ports=len(matches))
                raise PortUnavailable()
            port = matches[0].device
            connection = serial.Serial(port=None, baudrate=int(os.environ.get('STUDIO_ESP32_BAUD', '460800')) if name == 'esp32' else 115200, timeout=0.05, write_timeout=0.2)
            connection.dtr = False
            connection.rts = False
            connection.port = port
            connection.open()
            # Teensy USB Serial uses DTR to know that a host is listening.
            # ESP32 modem lines stay deasserted to avoid intentional resets.
            if name == 'teensy':
                connection.dtr = True
            connected_since = time.monotonic()
            with lock:
                status[name].update(connected=True, port=port, connected_at=utc(),
                                    connection_state='connected', retry_in_seconds=0)
            event('connected', port=port)
            # Drain USB independently of expensive glyph/layer rendering.
            # Bounded at 4 MiB; overflow invalidates rather than faking a live image.
            received = queue.Queue(maxsize=1024)
            failures = []
            def receive(connection=connection, reader_stop=reader_stop,
                        received=received, failures=failures):
                # Bind this session: a delayed old reader must never use a new port/queue.
                raw_capture = None
                raw_bytes = 0
                try:
                    raw_capture = open(DATA / 'esp32-receive.bin', 'wb') if name == 'esp32' else None
                    while not stop.is_set() and not reader_stop.is_set():
                        chunk = connection.read(4096)
                        if chunk:
                            if raw_capture is not None and raw_bytes < 5000000:
                                raw_capture.write(chunk)
                                raw_capture.flush()
                                raw_bytes += len(chunk)
                            received.put_nowait(chunk)
                except (serial.SerialException, OSError, queue.Full) as exc:
                    failures.append(str(exc) or "Serial receive queue overflow")
                finally:
                    if raw_capture is not None:
                        raw_capture.close()
            reader = threading.Thread(target=receive, daemon=True)
            reader.start()
            pending = bytearray()
            recovery = SnapshotRequests(time.monotonic())
            capability_seen_at = None
            snapshot_requests_enabled = os.environ.get('STUDIO_MIRROR_RESYNC', '1') == '1'
            while not stop.is_set():
                if snapshot_requests_enabled and name == 'esp32' and capability_seen_at is not None and time.monotonic() - capability_seen_at < 3.0:
                    request = recovery.due(time.monotonic(), mirror.state())
                    if request is not None:
                        message = f'!mirror,{request}\n'.encode('ascii')
                        if connection.write(message) != len(message):
                            raise serial.SerialException('Incomplete mirror snapshot request')
                        event('snapshot_request', request_id=request)
                        with lock:
                            status[name]['snapshot_request_id'] = request
                if failures:
                    raise serial.SerialException(failures[0])
                try:
                    chunk = received.get(timeout=0.1)
                except queue.Empty:
                    continue
                pending.extend(chunk)
                while b'\n' in pending:
                    raw, _, rest = pending.partition(b'\n')
                    pending = bytearray(rest)
                    if name == 'esp32' and is_resync_capability(raw):
                        capability_seen_at = time.monotonic()
                    if name == 'esp32' and mirror.accept(raw):
                        if b',T,' in raw:
                            event('touch', text=raw.decode('ascii', errors='replace'))
                        continue
                    event('serial', text=raw.rstrip(b'\r').decode('utf-8', errors='replace'))
                if len(pending) > 16384:
                    event('framing_error', message='Discarded overlong unterminated serial data', bytes=len(pending))
                    pending.clear()
        except PortUnavailable:
            pass  # Absence is normal during re-enumeration; do not flood the event log.
        except (serial.SerialException, OSError) as exc:
            with lock:
                status[name]['connection_state'] = 'retrying'
            event('error', message=str(exc))
        finally:
            reader_stop.set()
            if reader is not None:
                reader.join(timeout=1)
            if connection is not None:
                try:
                    connection.close()
                except (serial.SerialException, OSError) as exc:
                    event('close_error', message=str(exc))
            if reader is not None and reader.is_alive():
                reader.join(timeout=0.25)
                if reader.is_alive():
                    # Never create a second receiver while an old read remains stuck.
                    event('error', message='USB reader failed to stop; automatic reopen halted')
                    if name == 'esp32':
                        mirror.disconnect()
                    with lock:
                        status[name].update(connected=False, connection_state='reader_stuck')
                    return
            if name == 'esp32':
                mirror.disconnect()
            with lock:
                status[name]['connected'] = False
        duration = time.monotonic() - connected_since if connected_since is not None else 0.0
        delay = backoff.next_delay(duration)
        with lock:
            status[name].update(retry_in_seconds=delay, retry_attempt=backoff.attempt)
        stop.wait(delay)


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
