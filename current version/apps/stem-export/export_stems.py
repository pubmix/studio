#!/usr/bin/env python3
"""Offline Mac WAV adapter; never opens USB or the SD card."""
import argparse, hashlib, json, math, re, shutil, struct, subprocess, tempfile, wave
from pathlib import Path
import numpy as np

ROLES = ('vocals', 'melody', 'bass', 'rhythm')

def decode(path, target):
    subprocess.run(['/usr/bin/afconvert', '-f', 'WAVE', '-d', 'LEF32@44100', '-c', '2',
                    str(path), str(target)], check=True, capture_output=True)
    raw = target.read_bytes()
    if raw[:4] != b'RIFF' or raw[8:12] != b'WAVE':
        raise ValueError('Decoder did not produce RIFF WAVE')
    pos, fmt, pcm = 12, None, None
    while pos + 8 <= len(raw):
        tag, size = struct.unpack_from('<4sI', raw, pos); pos += 8
        chunk = raw[pos:pos+size]
        if len(chunk) != size: raise ValueError('Truncated decoded WAV')
        if tag == b'fmt ': fmt = chunk
        if tag == b'data': pcm = chunk
        pos += size + (size & 1)
    if fmt is None or pcm is None or len(fmt) < 16: raise ValueError('Missing WAV chunks')
    code, channels, rate, _, align, bits = struct.unpack_from('<HHIIHH', fmt)
    if code == 65534 and len(fmt) >= 40: code = struct.unpack_from('<H', fmt, 24)[0]
    if (code, channels, rate, align, bits) != (3, 2, 44100, 8, 32):
        raise ValueError('Unexpected decoded WAV format')
    data = np.frombuffer(pcm, dtype='<f4').reshape(-1, 2).astype(np.float64)
    if not len(data) or not np.isfinite(data).all(): raise ValueError('Empty or nonfinite audio')
    return data

def export(paths, output, set_id, title, prefix):
    output = Path(output).resolve()
    if output.exists(): raise ValueError('Output already exists; choose a new folder')
    if set_id < 1 or not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', prefix):
        raise ValueError('Positive set ID and 1–32 character ASCII filename prefix required')
    for path in paths:
        if not Path(path).is_file(): raise ValueError(f'Missing input: {path}')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.stem-export-', dir=output.parent) as scratch:
        temp = Path(scratch)
        audio = [decode(Path(p), temp / f'decode{i}.wav') for i, p in enumerate(paths)]
        frames = max(map(len, audio))
        if max(map(len,audio)) - min(map(len,audio)) > 2:
            raise ValueError('Stem lengths differ by more than two frames; export synchronized stems from the same start')
        audio = [np.pad(a, ((0, frames-len(a)), (0,0))) for a in audio]
        # Preserve relative stem levels; protect both each file and their unity-gain sum.
        peak = max(max(float(np.max(np.abs(a))) for a in audio),
                   float(np.max(np.abs(sum(audio)))))
        gain = min(1.0, 10**(-1/20) / peak) if peak else 1.0
        publish = temp / 'publish'; publish.mkdir()
        root = publish / 'sd-root-files'; root.mkdir()
        manifest = dict(api_version=1, set_id=set_id, title=title, sample_rate=44100,
                        frames=frames, alignment_offset_frames=0, stems=[])
        for role, data in zip(ROLES, audio):
            file = publish / f'{role}.wav'
            encoded = np.rint(data * gain * 32767).astype('<i2')
            with wave.open(str(file), 'wb') as wav:
                wav.setparams((2, 2, 44100, frames, 'NONE', 'not compressed'))
                wav.writeframes(encoded.tobytes())
            digest = hashlib.sha256(file.read_bytes()).hexdigest()
            manifest['stems'].append(dict(role=role.upper(), file=file.name, sha256=digest))
            shutil.copyfile(file, root / f'{prefix}_{role}.wav')
        (publish/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        provenance = dict(shared_gain=gain, headroom_db=1, sample_grid='44100 stereo',
                          conversion='macOS afconvert float decode/resample, common gain, rounded PCM16',
                          source_files=[str(Path(p).resolve()) for p in paths],
                          source_sha256=[hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in paths],
                          note='Timing is inherited from inputs, not detected or repaired. No USB transfer performed.')
        (publish/'export.json').write_text(json.dumps(provenance,indent=2)+'\n')
        if output.exists(): raise ValueError('Output appeared during export; refusing overwrite')
        publish.rename(output)
    return manifest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for role in ROLES: p.add_argument('--'+role, required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--set-id',required=True,type=int)
    p.add_argument('--title',default='Prepared stems')
    p.add_argument('--prefix',required=True)
    a=p.parse_args()
    try: result=export([getattr(a,r) for r in ROLES],a.output,a.set_id,a.title,a.prefix)
    except (ValueError, OSError, subprocess.CalledProcessError) as e: p.exit(1,f'Export failed: {e}\n')
    print(f'Prepared {result["frames"]} synchronized frames per stem at {a.output}; not transferred.')
if __name__=='__main__': main()
