"""Render checked drawing records emitted by the actual ESP32 firmware."""
import json
from pathlib import Path
import struct
import threading
import time
import zlib

WIDTH, HEIGHT = 1280, 720
SCREENS = ['Menu', 'New project', 'Load project', 'Mixer', 'Playlist', 'Pattern', 'File picker', 'WiFi', 'Settings', 'Stems']


def png(pixels):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    raw = b''.join(b'\0' + pixels[y * WIDTH * 3:(y + 1) * WIDTH * 3] for y in range(HEIGHT))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 1)) + chunk(b'IEND', b''))


class Mirror:
    def __init__(self, output):
        self.lock = threading.RLock()
        self.fonts = json.loads(Path(__file__).with_name('fonts.json').read_text())
        self.layers = [bytearray(WIDTH * HEIGHT * 3) for _ in range(4)]
        self.output = output
        self.image = None
        self.valid = False
        self.expected = None
        self.frames = 0
        self.commands = 0
        self.last = 0
        self.last_publish = 0
        self.dirty = False
        self.error = 'Waiting for display startup'
        self.screen = 'Waiting'
        self.touch = None
        self.dropped = 0

    def disconnect(self):
        with self.lock:
            self.valid = False
            self.expected = None
            self.error = 'Display disconnected; waiting for a complete startup stream'

    def state(self):
        with self.lock:
            age = time.monotonic() - self.last if self.last else None
            return dict(valid=self.valid, live=self.valid and age is not None and age < 3,
                        age_seconds=age, screen=self.screen, frames=self.frames,
                        commands=self.commands, dropped=self.dropped, error=self.error,
                        touch=self.touch, source='Firmware drawing-command mirror',
                        width=WIDTH, height=HEIGHT)

    def rect(self, layer, x1, y1, x2, y2, color):
        if not 0 <= layer < 4:
            raise ValueError('Unknown screen layer')
        x1, y1, x2, y2 = max(0, x1), max(0, y1), min(WIDTH - 1, x2), min(HEIGHT - 1, y2)
        if x2 < x1 or y2 < y1:
            return
        r, g, b = (color >> 11) & 31, (color >> 5) & 63, color & 31
        row = bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))) * (x2 - x1 + 1)
        for y in range(y1, y2 + 1):
            offset = (y * WIDTH + x1) * 3
            self.layers[layer][offset:offset + len(row)] = row
        if layer == 0:
            self.dirty = True

    def glyph(self, layer, font, char, x, baseline, color):
        f = self.fonts[font]
        if not 32 <= char <= 126:
            raise ValueError('Unknown glyph')
        offset, width, height, advance, dx, dy = f['glyphs'][char - 32]
        bitmap = f['bitmap']
        for y in range(height):
            run = None
            for xx in range(width + 1):
                bit = y * width + xx
                on = xx < width and bitmap[offset + bit // 8] & (0x80 >> (bit % 8))
                if on and run is None:
                    run = xx
                if not on and run is not None:
                    self.rect(layer, x + dx + run, baseline + dy + y,
                              x + dx + xx - 1, baseline + dy + y, color)
                    run = None

    def accept(self, raw):
        start = raw.find(b'@V1,')
        if start < 0:
            return False
        with self.lock:
            try:
                payload, checksum = raw[start:].strip().rsplit(b'*', 1)
                h = 2166136261
                for byte in payload:
                    h = ((h ^ byte) * 16777619) & 0xffffffff
                if h != int(checksum, 16):
                    raise ValueError('Drawing record checksum mismatch')
                fields = payload.decode('ascii').split(',')
                seq, op = int(fields[1]), fields[2]
                if op == 'Z':
                    if fields[3:] != ['1280', '720'] or seq != 0:
                        raise ValueError('Unsupported stream dimensions or start sequence')
                    self.layers = [bytearray(WIDTH * HEIGHT * 3) for _ in range(4)]
                    self.expected = 1
                    self.valid = True
                    self.error = None
                    self.image = None
                    self.dropped = 0
                    self.dirty = True
                    return True
                if self.expected is None:
                    return True
                if seq != self.expected:
                    raise ValueError(f'Missing drawing records: expected {self.expected}, received {seq}')
                self.expected = seq + 1
                if not self.valid:
                    return True
                self.commands += 1
                if op == 'R':
                    self.rect(*map(int, fields[3:]))
                elif op == 'G':
                    self.glyph(*map(int, fields[3:]))
                elif op == 'B':
                    src, dst, x, y, w, h = map(int, fields[3:])
                    if not (0 <= src < 4 and 0 <= dst < 4 and 0 <= x < WIDTH and 0 <= y < HEIGHT
                            and w > 0 and h > 0 and x + w <= WIDTH and y + h <= HEIGHT):
                        raise ValueError('Invalid layer copy')
                    rows = [bytes(self.layers[src][((y + yy) * WIDTH + x) * 3:((y + yy) * WIDTH + x + w) * 3]) for yy in range(h)]
                    for yy, row in enumerate(rows):
                        offset = ((y + yy) * WIDTH + x) * 3
                        self.layers[dst][offset:offset + len(row)] = row
                    if dst == 0:
                        self.dirty = True
                elif op == 'T':
                    self.touch = dict(phase=fields[3], x=int(fields[4]), y=int(fields[5]), device_ms=int(fields[6]))
                elif op == 'F':
                    ms, screen, self.dropped = map(int, fields[3:])
                    if self.dropped:
                        raise ValueError('Display diagnostic queue overflowed')
                    self.last = time.monotonic()
                    self.screen = SCREENS[screen] if 0 <= screen < len(SCREENS) else str(screen)
                    if self.dirty and self.last - self.last_publish >= 0.2:
                        self.image = png(self.layers[0])
                        self.frames += 1
                        temp = self.output.with_suffix('.tmp')
                        temp.write_bytes(self.image)
                        temp.replace(self.output)
                        self.last_publish = self.last
                        self.dirty = False
                else:
                    raise ValueError('Unsupported mirror operation')
            except (ValueError, TypeError, IndexError, UnicodeError) as exc:
                self.valid = False
                self.error = str(exc)
            return True
