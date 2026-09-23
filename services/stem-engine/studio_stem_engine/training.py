"""Validate rights/splits/alignment and generate deterministic training mixtures.

This prepares data; it does not train a model or imply that weights exist.
"""
import argparse
import json
import os
import tempfile
from pathlib import Path
import re
import numpy as np
import soundfile as sf
from .audio import read
from .engine import digest
from .sources import label, group_sources


def validate(manifest_path):
    path = Path(manifest_path).resolve()
    manifest = json.loads(path.read_text())
    records, identifiers, groups, hashes = [], set(), {}, {}
    for track in manifest['tracks']:
        identifier = track['id']
        if not isinstance(identifier, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,80}', identifier) or identifier in identifiers:
            raise ValueError('Unique filesystem-safe track IDs required')
        identifiers.add(identifier)
        split = track['split']
        if split not in {'train', 'validation', 'test'}: raise ValueError('Invalid split')
        if not isinstance(track.get('rights'), str) or not track['rights'].strip():
            raise ValueError('Document permission/license for each recording')
        group = track.get('recording_group')
        if not isinstance(group, str) or not group: raise ValueError('recording_group required to prevent session leakage')
        if group in groups and groups[group] != split: raise ValueError('Recording group leaks across splits')
        groups[group] = split
        grid, sources = None, {}
        for source, filename in track['sources'].items():
            name = label(source)
            if name in sources: raise ValueError('Duplicate source aliases')
            source_path = (path.parent/filename).resolve()
            audio, rate = read(source_path)
            current = (rate, audio.shape)
            if grid is not None and current != grid: raise ValueError('Source grids differ')
            grid = current
            checksum = digest(source_path)
            if checksum in hashes and hashes[checksum] != split: raise ValueError('Audio leaks across splits')
            hashes[checksum] = split
            sources[name] = {'path': str(source_path), 'sha256': checksum}
        group_sources({k: 0 for k in sources})
        records.append({**track, 'sources': sources, 'sample_rate': grid[0], 'frames': grid[1][0], 'channels': grid[1][1]})
    if not records: raise ValueError('No training records')
    return records


def prepare(manifest, output, *, seed=0, seconds=10):
    target = Path(output).resolve()
    if target.exists(): raise ValueError('Choose a new output directory')
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.training-', dir=target.parent) as temporary:
        staging = Path(temporary)/'result'
        result = _prepare(manifest, staging, seed=seed, seconds=seconds)
        os.rename(staging, target)
    return result


def _prepare(manifest, output, *, seed=0, seconds=10):
    if not np.isfinite(seconds) or seconds <= 0: raise ValueError('seconds must be positive')
    records = validate(manifest)
    output = Path(output)
    if output.exists(): raise ValueError('Choose a new output directory')
    output.mkdir(parents=True)
    rng = np.random.default_rng(seed)
    result = {'schema': 'studio.training.v1', 'seed': seed, 'tracks': []}
    for record in records:
        count = max(1, min(record['frames'], int(seconds*record['sample_rate'])))
        start = int(rng.integers(0, record['frames']-count+1)) if record['split'] == 'train' else 0
        folder = output/record['id']; folder.mkdir()
        sources = {}
        audio_sources = {}
        gains = {}
        for name, asset in record['sources'].items():
            if digest(asset['path']) != asset['sha256']: raise ValueError('Source changed after validation')
            audio, rate = read(Path(asset['path']))
            gain = float(10**(rng.uniform(-6, 3)/20)) if record['split'] == 'train' else 1.0
            gains[name] = gain
            audio_sources[name] = audio[start:start+count]*gain
        mix = sum(audio_sources.values())
        peak = max(float(np.max(np.abs(a))) for a in [mix, *audio_sources.values()])
        shared_gain = min(1.0, .9/peak) if peak else 1.0
        for name, audio in audio_sources.items():
            sf.write(folder/(name+'.wav'), audio*shared_gain, record['sample_rate'], subtype='FLOAT')
            sources[name] = record['id']+'/'+name+'.wav'
        sf.write(folder/'mixture.wav', mix*shared_gain, record['sample_rate'], subtype='FLOAT')
        result['tracks'].append({**record, 'sources': sources, 'source_assets': record['sources'],
                                 'mixture': record['id']+'/mixture.wav', 'crop_start': start,
                                 'frames': count, 'source_gains': gains, 'shared_gain': shared_gain})
    (output/'manifest.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest'); parser.add_argument('--output')
    parser.add_argument('--seed', type=int, default=0); parser.add_argument('--seconds', type=float, default=10)
    args = parser.parse_args()
    result = prepare(args.manifest, args.output, seed=args.seed, seconds=args.seconds) if args.output else validate(args.manifest)
    print(json.dumps({'tracks': len(result['tracks'] if isinstance(result, dict) else result)}))

if __name__ == '__main__': main()
