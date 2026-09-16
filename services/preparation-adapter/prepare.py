#!/usr/bin/env python3
"""Offline STUDIO-P1 preparation. Native FLOAT32 results are never modified."""
import argparse
import contextlib
import fcntl
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import shutil
import sqlite3
import tempfile
import wave

import numpy as np
import scipy
from scipy.signal import resample_poly
import soundfile as sf

ROLES = ("VOCALS", "MELODY", "BASS", "RHYTHM")
RATE = 44100
CEILING = 10 ** (-1 / 20)
PROFILE = "STUDIO-P1"
VERSION = 1
MAX_DECODED_BYTES = 128 * 1024 * 1024
HEX = re.compile(r"[0-9a-f]{64}")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def json_read(data):
    def pairs(items):
        out = {}
        for k, v in items:
            if k in out:
                raise ValueError("Duplicate JSON key: " + k)
            out[k] = v
        return out
    return json.loads(data, object_pairs_hook=pairs,
                      parse_constant=lambda x: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))


def positive_int(x):
    return type(x) is int and x > 0


def read_regular(path, limit):
    """Read a bounded immutable byte snapshot, rejecting symlinked assets."""
    if path.is_symlink() or not path.is_file():
        raise ValueError("Missing or symlinked file: " + path.name)
    with path.open("rb") as f:
        data = f.read(limit + 1)
    if len(data) > limit:
        raise ValueError("Input exceeds offline adapter size limit")
    return data


def native(folder):
    root = Path(folder).resolve(strict=True)
    raw = read_regular(root / "metadata.json", 1024 * 1024)
    m = json_read(raw)
    if not isinstance(m, dict) or m.get("schema") != "studio.stems.v1":
        raise ValueError("Unsupported native schema")
    frames, rate, channels = (m.get(k) for k in ("frames", "sample_rate", "channels"))
    if not positive_int(frames) or not positive_int(rate) or not 8000 <= rate <= 192000:
        raise ValueError("Invalid native frames/rate (supported: 8–192 kHz)")
    if type(channels) is not int or channels not in (1, 2) or m.get("format") != "WAV FLOAT32":
        raise ValueError("Require native mono/stereo WAV FLOAT32")
    if not isinstance(m.get("key"), str) or not HEX.fullmatch(m["key"]):
        raise ValueError("Missing native content key")
    if frames * channels * 4 * 4 > MAX_DECODED_BYTES:
        raise ValueError("Native set exceeds 128 MiB decoded input limit")
    target_frames = (frames * RATE + rate - 1) // rate
    if target_frames * 2 * 4 * 4 > MAX_DECODED_BYTES:
        raise ValueError("Resampled set exceeds 128 MiB decoded output limit")
    stems = m.get("stems")
    if not isinstance(stems, list) or len(stems) != 4:
        raise ValueError("Exactly four native stems required")
    if {p.name for p in root.glob("*.wav")} != {r.lower()+".wav" for r in ROLES}:
        raise ValueError("Incomplete or additional WAV assets")
    arrays, hashes = [], []
    for role, item in zip(ROLES, stems):
        if not isinstance(item, dict) or item.get("name") != role or item.get("file") != role.lower()+".wav":
            raise ValueError("Canonical role order/filename mismatch")
        sha = item.get("sha256")
        if not isinstance(sha, str) or not HEX.fullmatch(sha):
            raise ValueError("Missing native hash")
        blob = read_regular(root / item["file"], frames * channels * 4 + 1024 * 1024)
        if digest(blob) != sha:
            raise ValueError("Native checksum mismatch: " + role)
        with sf.SoundFile(io.BytesIO(blob)) as f:
            if (f.frames, f.samplerate, f.channels, f.subtype, f.format) != (frames, rate, channels, "FLOAT", "WAV"):
                raise ValueError("Native WAV header mismatch: " + role)
            data = f.read(dtype="float64", always_2d=True)
        if data.shape != (frames, channels) or not np.isfinite(data).all():
            raise ValueError("Truncated or nonfinite native samples: " + role)
        arrays.append(data)
        hashes.append(sha)
    return m, digest(raw), arrays, hashes, target_frames


def validate(folder):
    root = Path(folder).resolve(strict=True)
    m = json_read(read_regular(root / "manifest.json", 1024 * 1024))
    if not isinstance(m, dict) or type(m.get("api_version")) is not int or m["api_version"] != 1:
        raise ValueError("Unsupported prepared schema")
    if not positive_int(m.get("set_id")) or m["set_id"] > 0xFFFFFFFF:
        raise ValueError("Invalid uint32 set_id")
    if type(m.get("frames")) is not int or m["frames"] <= 0:
        raise ValueError("Invalid prepared frame count")
    if m["frames"] * 2 * 4 * 4 > MAX_DECODED_BYTES:
        raise ValueError("Prepared set exceeds decoded output limit")
    if m.get("sample_rate") != RATE or type(m.get("alignment_offset_frames")) is not int or m["alignment_offset_frames"] != 0:
        raise ValueError("Require aligned 44100 Hz assets")
    if not isinstance(m.get("stems"), list) or len(m["stems"]) != 4:
        raise ValueError("Exactly four prepared stems required")
    for role, item in zip(ROLES, m["stems"]):
        if not isinstance(item, dict) or item.get("role") != role or item.get("file") != role.lower()+".wav":
            raise ValueError("Prepared roles/filenames out of order")
        blob = read_regular(root / item["file"], m["frames"]*4+1024*1024)
        if not isinstance(item.get("sha256"), str) or digest(blob) != item["sha256"]:
            raise ValueError("Prepared checksum missing or mismatched")
        with wave.open(io.BytesIO(blob), "rb") as f:
            if (f.getnchannels(), f.getsampwidth(), f.getframerate(), f.getnframes(), f.getcomptype()) != (2, 2, RATE, m["frames"], "NONE"):
                raise ValueError("Prepared WAV format mismatch")
            if len(f.readframes(m["frames"]+1)) != m["frames"]*4:
                raise ValueError("Truncated prepared WAV")
    return m


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, allow_nan=False)+"\n")


def sync_file(path):
    with path.open("rb") as f:
        os.fsync(f.fileno())


def prepare(source, destination):
    source = Path(source).resolve(strict=True)
    dest = Path(destination).resolve()
    if dest == source or source in dest.parents:
        raise ValueError("Output must not be inside the native set")
    m, metadata_sha, arrays, hashes, frames = native(source)
    versions = {"numpy": np.__version__, "scipy": scipy.__version__, "soundfile": sf.__version__}
    identity = {"metadata_sha256": metadata_sha, "source_hashes": hashes,
                "adapter": VERSION, "profile": PROFILE, "libraries": versions}
    key = digest(json.dumps(identity, sort_keys=True).encode())
    dest.mkdir(parents=True, exist_ok=True)
    # All writers sharing this destination serialize allocation and publication.
    with (dest / ".prepare.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        with contextlib.closing(sqlite3.connect(dest / "set-ids.sqlite3")) as db:
            db.execute("PRAGMA synchronous=FULL")
            with db:
                db.execute("CREATE TABLE IF NOT EXISTS sets (id INTEGER PRIMARY KEY AUTOINCREMENT, content_key TEXT UNIQUE NOT NULL)")
                db.execute("INSERT OR IGNORE INTO sets(content_key) VALUES (?)", (key,))
            set_id = db.execute("SELECT id FROM sets WHERE content_key=?", (key,)).fetchone()[0]
        if set_id > 0xFFFFFFFF:
            raise ValueError("Registry exhausted uint32 IDs")
        target = dest / f"set-{set_id:08d}"
        if target.exists() or target.is_symlink():
            if target.is_symlink():
                raise ValueError("Refuse symlinked prepared set")
            existing = validate(target)
            provenance = json_read(read_regular(target/"preparation.json", 1024*1024))
            if existing["set_id"] != set_id or provenance.get("preparation_key") != key:
                raise ValueError("Existing set identity mismatch")
            return target
        factor = math.gcd(m["sample_rate"], RATE)
        converted = []
        for data in arrays:
            if m["sample_rate"] != RATE:
                data = resample_poly(data, RATE//factor, m["sample_rate"]//factor,
                                     axis=0, window=("kaiser", 5.0), padtype="constant")
            if len(data) != frames or not np.isfinite(data).all():
                raise ValueError("Resampler frame/finite check failed")
            converted.append(data)
        peaks = [float(np.max(np.abs(x))) for x in converted]
        summed = sum(converted)
        sum_peak = float(np.max(np.abs(summed)))
        peak = max(*peaks, sum_peak)
        gain = min(1.0, CEILING/peak) if peak else 1.0
        stage = Path(tempfile.mkdtemp(prefix=".staging-", dir=dest))
        try:
            stems = []
            for index, (role, data) in enumerate(zip(ROLES, converted)):
                rng = np.random.default_rng(int(key[:16], 16)+index)
                # TPDF +/- 1 LSB, round-to-nearest. Mono dither is duplicated too.
                scaled = data * gain * 32768
                quantized = np.rint(scaled + rng.random(data.shape)-rng.random(data.shape))
                if np.max(quantized) > 32767 or np.min(quantized) < -32768:
                    raise ValueError("Quantization overflow; clipping is never implicit")
                pcm = quantized.astype("<i2")
                if m["channels"] == 1:
                    pcm = np.repeat(pcm, 2, axis=1)
                file = role.lower()+".wav"
                with wave.open(str(stage/file), "wb") as f:
                    f.setnchannels(2); f.setsampwidth(2); f.setframerate(RATE)
                    f.writeframes(pcm.tobytes())
                stems.append({"role": role, "file": file, "sha256": digest((stage/file).read_bytes())})
            manifest = {"api_version": 1, "set_id": set_id, "title": source.name,
                        "sample_rate": RATE, "frames": frames, "alignment_offset_frames": 0, "stems": stems}
            write_json(stage/"preparation.json", {
                "schema": "studio.preparation.v1", "profile": PROFILE, "adapter_version": VERSION,
                "preparation_key": key, "native_key": m["key"], "native_metadata_sha256": metadata_sha,
                "native_hashes": hashes, "source_frames": m["frames"], "source_rate": m["sample_rate"],
                "source_channels": m["channels"], "frames": frames, "common_gain": gain,
                "resampled_stem_peaks": peaks, "resampled_sum_peak": sum_peak,
                "ceiling": CEILING, "libraries": versions,
                "resampling": "scipy resample_poly; Kaiser 5; constant padding; shared ceil(N*44100/rate); zero-phase",
                "dither": "deterministic per-stem TPDF +/-1 LSB; duplicated for mono; round nearest"})
            write_json(stage/"manifest.json", manifest)
            validate(stage)
            for p in stage.iterdir():
                sync_file(p)
            os.rename(stage, target)
            fd = os.open(dest, os.O_RDONLY)
            try:
                os.fsync(fd)
            finally:
                os.close(fd)
            return target
        finally:
            if stage.exists():
                shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        print(prepare(args.source, args.destination))
    except (ValueError, OSError, RuntimeError, KeyError, TypeError, wave.Error, sqlite3.Error) as e:
        parser.exit(1, f"Preparation failed: {e}\n")


if __name__ == "__main__":
    main()
