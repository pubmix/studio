"""Explicit FLOAT master -> prepared-set v1 adapter; no inference or hardware I/O."""
from pathlib import Path
import json
import math
import os
import tempfile
import wave
import numpy as np
import soundfile as sf
from .audio import resample
from .engine import Engine, EngineError, STEMS, digest

RATE = 44100


def verify_transfer(folder, *, extended=False):
    """Validate portable transfer assets before any device writes."""
    root = Path(folder).resolve()
    try:
        m = json.loads((root / "manifest.json").read_text())
        if (m["api_version"] != (2 if extended else 1) or type(m["set_id"]) is not int or not 0 < m["set_id"] <= 0xffffffff
                or m["sample_rate"] != RATE or m["alignment_offset_frames"] != 0
                or type(m["frames"]) is not int or m["frames"] <= 0):
            raise ValueError("Invalid prepared-set v1 metadata")
        if extended:
            from .sources import FAMILIES, GROUPS
            roles = [e["role"].lower() for e in m["stems"]]
            if not 4 <= len(roles) <= 16 or len(set(roles)) != len(roles) or any(r not in FAMILIES for r in roles):
                raise ValueError("Invalid extended stems")
            if roles != sorted(roles, key=lambda k: (GROUPS.index(FAMILIES[k]), k)):
                raise ValueError("Invalid extended order")
            for i, e in enumerate(m["stems"]):
                if (e["channel"], e["bank"], e["lane"], e["group"]) != (i+1, i//4+1, i%4+1, FAMILIES[roles[i]].upper()):
                    raise ValueError("Invalid channel mapping")
        else:
            roles = STEMS
            if len(m["stems"]) != 4:
                raise ValueError("Exactly four stems required")
        for role, entry in zip(roles, m["stems"]):
            if entry["role"] != role.upper() or entry["file"] != role + ".wav":
                raise ValueError("Invalid canonical stem order or filename")
            p = root / entry["file"]
            if p.is_symlink() or p.resolve().parent != root:
                raise ValueError("Transfer asset must be inside its folder")
            with wave.open(str(p), "rb") as w:
                if (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes(), w.getcomptype()) != (2, 2, RATE, m["frames"], "NONE"):
                    raise ValueError("Expected aligned stereo 44100 Hz PCM16 WAVs")
                total = 0
                while block := w.readframes(8192):
                    total += len(block)
                if total != m["frames"] * 4:
                    raise ValueError("Truncated transfer WAV")
            if digest(p) != entry["sha256"]:
                raise ValueError("Transfer checksum mismatch")
        return m
    except (ValueError, OSError, KeyError, TypeError, wave.Error, EOFError) as exc:
        raise EngineError(f"Invalid transfer set: {exc}") from exc


def export_transfer(masters, destination, *, set_id, title="", headroom_db=1.0, extended=False):
    """Publish four aligned PCM16 files atomically, preserving relative stem gains.

    One attenuation factor protects both each stem and their sum from clipping.
    It never amplifies quiet input. Resampling is identical for all four lanes.
    """
    if type(set_id) is not int or not 0 < set_id <= 0xffffffff:
        raise ValueError("set_id must be a nonzero uint32")
    if not math.isfinite(headroom_db) or not 0 <= headroom_db <= 60:
        raise ValueError("headroom_db must be finite and between 0 and 60")
    metadata = Engine.verify(masters)
    if metadata["schema"] == "studio.stems.v2" and not extended:
        masters = Path(masters) / "four"
        metadata = Engine.verify(masters)
    if extended and metadata["schema"] != "studio.stems.v2":
        raise EngineError("Extended transfer requires extended masters")
    roles = [e["name"].lower() for e in metadata["stems"]]
    if metadata["frames"] <= 0 or metadata["channels"] not in (1, 2):
        raise EngineError("Masters must be nonempty mono or stereo")
    target = Path(destination).resolve()
    if target.exists():
        raise EngineError(f"Transfer destination already exists: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".transfer-", dir=target.parent) as tmp:
        staging = Path(tmp) / "result"
        staging.mkdir()
        # Disk-backed intermediates keep memory bounded to a stem plus the sum.
        frames = (metadata["frames"] * RATE + metadata["sample_rate"] - 1) // metadata["sample_rate"]
        summed = np.memmap(Path(tmp) / "sum.raw", mode="w+", dtype="float64", shape=(frames, 2))
        summed[:] = 0
        peak = 0.0
        for role in roles:
            samples, rate = sf.read(Path(masters) / (role + ".wav"), dtype="float32", always_2d=True)
            samples = resample(samples, rate, RATE)
            if samples.shape[1] == 1:
                samples = np.repeat(samples, 2, axis=1)
            if samples.shape != (frames, 2) or not np.isfinite(samples).all():
                raise EngineError("Invalid master samples or resampled alignment")
            peak = max(peak, float(np.max(np.abs(samples))))
            summed += samples
            sf.write(Path(tmp) / (role + ".wav"), samples, RATE, subtype="FLOAT")
            del samples
        for start in range(0, frames, 65536):
            peak = max(peak, float(np.max(np.abs(summed[start:start + 65536]))))
        del summed
        gain = min(1.0, (10 ** (-headroom_db / 20) - len(roles) / 32768) / peak) if peak else 1.0
        entries = []
        for role in roles:
            out = staging / (role + ".wav")
            with sf.SoundFile(Path(tmp) / out.name) as source, sf.SoundFile(out, mode="w", samplerate=RATE, channels=2, subtype="PCM_16", format="WAV") as dest:
                for block in source.blocks(blocksize=65536, dtype="float32", always_2d=True):
                    dest.write(block * gain)
            entries.append({"role": role.upper(), "file": out.name, "sha256": digest(out)})
        if extended:
            for entry, master in zip(entries, metadata["stems"]):
                entry.update({k: v for k, v in master.items() if k not in {"file", "sha256", "name", "peak"}})
        m = {"api_version": 2 if extended else 1, "set_id": set_id, "title": title, "sample_rate": RATE,
             "frames": frames, "alignment_offset_frames": 0, "stems": entries,
             "source_key": metadata["key"], "fallback": metadata.get("fallback"), "settings": metadata["settings"],
             "source_measurements": metadata.get("source_measurements", {}), "gain": gain, "headroom_db": headroom_db}
        (staging / "manifest.json").write_text(json.dumps(m, indent=2) + "\n")
        verify_transfer(staging, extended=extended)
        # Masters may be user-managed; detect changes during export too.
        if Engine.verify(masters) != metadata:
            raise EngineError("Masters changed during export")
        os.rename(staging, target)
    return target
