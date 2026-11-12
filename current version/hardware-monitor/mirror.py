"""Render checked drawing records emitted by the actual ESP32 firmware."""
import json
from pathlib import Path
import struct
import threading
import time
import zlib
try:
    from PIL import Image
except ImportError:
    Image = None

WIDTH, HEIGHT = 1280, 720
SCREENS = ['Menu', 'New project', 'Load project', 'Mixer', 'Playlist', 'Pattern', 'Input', 'File picker', 'WiFi', 'MIDI', 'Bluetooth', 'Controls', 'Themes']


def png(pixels):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    raw = b''.join(b'\0' + pixels[y * WIDTH * 3:(y + 1) * WIDTH * 3] for y in range(HEIGHT))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 1)) + chunk(b'IEND', b''))


def persisted_png(path):
    """Accept only a complete CRC-checked RGB PNG emitted by this renderer."""
    try:
        data = path.read_bytes()
        if len(data) > 10_000_000 or data[:8] != b'\x89PNG\r\n\x1a\n':
            return None
        offset, compressed, header, ended = 8, bytearray(), False, False
        while offset + 12 <= len(data):
            size = struct.unpack('>I', data[offset:offset + 4])[0]
            kind = data[offset + 4:offset + 8]
            end = offset + 8 + size
            if end + 4 > len(data):
                return None
            payload = data[offset + 8:end]
            if zlib.crc32(kind + payload) != struct.unpack('>I', data[end:end + 4])[0]:
                return None
            if kind == b'IHDR':
                if header or size != 13 or struct.unpack('>IIBBBBB', payload) != (WIDTH, HEIGHT, 8, 2, 0, 0, 0):
                    return None
                header = True
            elif kind == b'IDAT':
                compressed.extend(payload)
            elif kind == b'IEND':
                ended = size == 0 and end + 4 == len(data)
                break
            offset = end + 4
        if not header or not ended:
            return None
        expected = (WIDTH * 3 + 1) * HEIGHT
        decoder = zlib.decompressobj()
        raw = decoder.decompress(bytes(compressed), expected + 1)
        if len(raw) != expected or not decoder.eof or decoder.unused_data:
            return None
        return data
    except (OSError, ValueError, struct.error, zlib.error):
        return None


class Mirror:
    def __init__(self, output):
        self.lock = threading.RLock()
        self.fonts = json.loads(Path(__file__).with_name('fonts.json').read_text())
        self.layers = [bytearray(WIDTH * HEIGHT * 3) for _ in range(6)]
        self.output = output
        self.image = persisted_png(output)
        self.last_complete_timestamp = output.stat().st_mtime if self.image is not None else None
        self.awaiting_complete = True
        self.complete_base = False
        self.snapshot_id = None
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
        self.transition_omitted = False

    def disconnect(self):
        with self.lock:
            self.valid = False
            self.expected = None
            self.snapshot_id = None
            self.awaiting_complete = True
            self.error = 'Display disconnected; waiting for a complete startup stream'

    def state(self):
        with self.lock:
            age = time.monotonic() - self.last if self.last else None
            live = self.valid and age is not None and age < 3 and not self.transition_omitted and not self.awaiting_complete
            return dict(valid=self.valid, live=live,
                        has_frame=self.image is not None,
                        last_complete_timestamp=self.last_complete_timestamp,
                        stale=self.image is not None and not live,
                        awaiting_complete=self.awaiting_complete,
                        snapshot_active=self.snapshot_id is not None,
                        snapshot_id=self.snapshot_id,
                        age_seconds=age, screen=self.screen, frames=self.frames,
                        commands=self.commands, dropped=self.dropped, error=self.error,
                        touch=self.touch, source='Firmware drawing-command mirror',
                        width=WIDTH, height=HEIGHT,
                        transition_rendering="Pillow blend preview" if Image is not None else "Final frame only (Pillow unavailable)",
                        transition_omitted=self.transition_omitted)

    def rect(self, layer, x1, y1, x2, y2, color):
        if not 0 <= layer < 6:
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
                if op == 'F':
                    self.dropped = int(fields[5])
                if op == 'H' and self.expected is None:
                    if fields[3:] != ['RESYNC','1']: raise ValueError('Invalid capability')
                    return True
                if op in ('Z','S'):
                    dimensions = fields[4:] if op == 'S' else fields[3:]
                    if dimensions != ['1280', '720'] or seq != 0:
                        raise ValueError('Unsupported stream dimensions or start sequence')
                    self.layers = [bytearray(WIDTH * HEIGHT * 3) for _ in range(6)]
                    self.snapshot_id = int(fields[3]) if op == 'S' else None
                    if self.snapshot_id is not None and not 0 <= self.snapshot_id <= 0xffffffff:
                        raise ValueError("Invalid snapshot id")
                    self.expected = 1
                    self.valid = True
                    self.error = "Waiting for a complete screen after display restart"
                    # Preserve the last complete image through firmware restart.
                    self.awaiting_complete = True
                    self.complete_base = False
                    self.dropped = 0
                    self.transition_omitted = False
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
                if op == 'H':
                    if fields[3:] != ['RESYNC','1']: raise ValueError('Invalid capability')
                elif op == 'K':
                    request_id, screen, drops = map(int,fields[3:])
                    if self.snapshot_id != request_id or drops or not self.complete_base:
                        raise ValueError('Incomplete snapshot')
                    self.image = png(self.layers[0])
                    temp = self.output.with_suffix('.tmp')
                    temp.write_bytes(self.image)
                    temp.replace(self.output)
                    self.frames += 1
                    self.last = self.last_publish = time.monotonic()
                    self.last_complete_timestamp = time.time()
                    self.screen = SCREENS[screen] if 0 <= screen < len(SCREENS) else str(screen)
                    self.snapshot_id = None
                    self.awaiting_complete = False
                    self.error = None
                    self.dirty = False
                elif op == 'R':
                    self.rect(*map(int, fields[3:]))
                elif op == 'O':
                    layer,x1,y1,x2,y2,radius,color = map(int, fields[3:])
                    if not (0 <= radius <= min((x2-x1+1)//2,(y2-y1+1)//2)):
                        raise ValueError('Invalid rounded rectangle')
                    self.rect(layer,x1,y1+radius,x2,y2-radius,color)
                    for y in range(radius):
                        inset = radius - int((radius*radius-(radius-y)**2)**0.5)
                        self.rect(layer,x1+inset,y1+y,x2-inset,y1+y,color)
                        self.rect(layer,x1+inset,y2-y,x2-inset,y2-y,color)
                elif op == 'G':
                    self.glyph(*map(int, fields[3:]))
                elif op == 'B':
                    src, dst, x, y, w, h = map(int, fields[3:])
                    if not (0 <= src < 6 and 0 <= dst < 6 and 0 <= x < WIDTH and 0 <= y < HEIGHT
                            and w > 0 and h > 0 and x + w <= WIDTH and y + h <= HEIGHT):
                        raise ValueError('Invalid layer copy')
                    rows = [bytes(self.layers[src][((y + yy) * WIDTH + x) * 3:((y + yy) * WIDTH + x + w) * 3]) for yy in range(h)]
                    for yy, row in enumerate(rows):
                        offset = ((y + yy) * WIDTH + x) * 3
                        self.layers[dst][offset:offset + len(row)] = row
                    if dst == 0:
                        if x == 0 and y == 0 and w == WIDTH and h == HEIGHT:
                            self.complete_base = True
                        self.transition_omitted = False
                        self.dirty = True
                elif op == 'A':
                    old, new, dst, alpha = map(int, fields[3:])
                    if not (0 <= old < 6 and 0 <= new < 6 and 0 <= dst < 6
                            and len({old, new, dst}) == 3 and 0 <= alpha <= 32):
                        raise ValueError('Invalid layer blend')
                    if alpha == 32:
                        self.layers[dst][:] = self.layers[new]
                        self.transition_omitted = False
                    elif alpha == 0:
                        self.layers[dst][:] = self.layers[old]
                        self.transition_omitted = False
                    elif Image is not None:
                        a = Image.frombytes('RGB', (WIDTH, HEIGHT), bytes(self.layers[old]))
                        b = Image.frombytes('RGB', (WIDTH, HEIGHT), bytes(self.layers[new]))
                        self.layers[dst][:] = Image.blend(a, b, alpha / 32.0).tobytes()
                        self.transition_omitted = False
                    else:
                        # Never present stale pixels as a faithful live transition.
                        self.transition_omitted = True
                    if dst == 0 and self.complete_base and not self.transition_omitted and self.snapshot_id is None:
                        self.dirty = True
                        now = time.monotonic()
                        if alpha == 32 or now - self.last_publish >= 0.033:
                            self.image = png(self.layers[0])
                            self.frames += 1
                            temp = self.output.with_suffix('.tmp')
                            temp.write_bytes(self.image)
                            temp.replace(self.output)
                            self.last_publish = now
                            self.last_complete_timestamp = time.time()
                            if alpha == 32:
                                self.awaiting_complete = False
                                self.error = None
                            self.dirty = False
                elif op == 'T':
                    self.touch = dict(phase=fields[3], x=int(fields[4]), y=int(fields[5]), device_ms=int(fields[6]))
                elif op == 'F':
                    ms, screen, self.dropped = map(int, fields[3:])
                    if self.dropped:
                        raise ValueError('Display diagnostic queue overflowed')
                    self.last = time.monotonic()
                    self.screen = SCREENS[screen] if 0 <= screen < len(SCREENS) else str(screen)
                    if self.dirty and self.complete_base and not self.transition_omitted and self.snapshot_id is None and self.last - self.last_publish >= 0.333:
                        self.image = png(self.layers[0])
                        self.frames += 1
                        temp = self.output.with_suffix('.tmp')
                        temp.write_bytes(self.image)
                        temp.replace(self.output)
                        self.last_publish = self.last
                        self.last_complete_timestamp = time.time()
                        self.awaiting_complete = False
                        self.error = None
                        self.dirty = False
                else:
                    raise ValueError('Unsupported mirror operation')
            except (ValueError, TypeError, IndexError, UnicodeError) as exc:
                self.valid = False
                self.error = str(exc)
            return True
