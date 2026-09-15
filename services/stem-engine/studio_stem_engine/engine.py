from __future__ import annotations
from dataclasses import dataclass, asdict
from pathlib import Path
import hashlib
import importlib.metadata
import json
import os
import platform
import shutil
import tempfile
import time
import fcntl
import numpy as np
import soundfile as sf
from .audio import read, resample, align

STEMS = ("vocals", "melody", "bass", "rhythm")
SCHEMA = "studio.stems.v1"


class EngineError(RuntimeError):
    pass


@dataclass(frozen=True)
class Settings:
    preset: str = "balanced"
    backend: str = "demucs"
    model: str | None = None
    vocal_model: str = "model_bs_roformer_ep_317_sdr_12.9755.ckpt"
    vocal_backend: str = "mlx"
    device: str = "cpu"
    shifts: int = 1
    overlap: float = 0.25
    seed: int = 0
    consistency: str = "melody"

    def __post_init__(self):
        if self.preset not in {"fast", "balanced", "best"}:
            raise ValueError("Unknown preset")
        if self.consistency not in {"melody", "energy", "none"}:
            raise ValueError("Unknown consistency policy")
        if self.shifts < 0 or not 0 <= self.overlap < 1:
            raise ValueError("Invalid shifts or overlap")
        if self.device not in {"cpu", "mps", "cuda"}:
            raise ValueError("Device must be cpu, mps or cuda")

    def resolved_model(self):
        return self.model or {"fast": "htdemucs", "balanced": "htdemucs_ft", "best": "htdemucs_6s"}[self.preset]


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def canonical(raw):
    """Unknown source labels are rejected; accompaniment is never mistaken for melody."""
    aliases = {"vocals": "vocals", "melody": "melody", "other": "melody", "guitar": "melody",
               "piano": "melody", "strings": "melody", "keys": "melody", "synth": "melody",
               "brass": "melody", "bass": "bass", "drums": "rhythm", "percussion": "rhythm", "rhythm": "rhythm"}
    result = {}
    for name, data in raw.items():
        key = aliases.get(name.lower())
        if key is None:
            raise EngineError(f"Unsupported backend source label: {name}")
        result[key] = result.get(key, 0) + data
    if set(result) != set(STEMS):
        raise EngineError("Backend must provide vocals, melodic instruments, bass and drums")
    return {s: result[s] for s in STEMS}


def consistent(mix, stems, policy):
    residual = mix.astype(np.float64) - sum(stems[s].astype(np.float64) for s in STEMS)
    before = float(np.sqrt(np.mean(residual ** 2)))
    if policy == "melody":
        stems["melody"] = stems["melody"] + residual
    elif policy == "energy":
        # Per-sample projection; benchmark before promotion, may modulate quiet sources.
        power = np.stack([stems[s].astype(np.float64) ** 2 + 1e-12 for s in STEMS])
        weights = power / power.sum(axis=0)
        stems = {s: stems[s] + weights[i] * residual for i, s in enumerate(STEMS)}
    return {s: stems[s].astype(np.float32) for s in STEMS}, before


class Engine:
    """Synchronous local API. Progress callback receives stage events, not fake percentages.

    Custom backends implement identity(settings)->JSON and separate(audio, rate,
    model, settings, scratch, progress)->{source_label: samples[frames, channels]}.
    They must preserve input gain and return the requested rate and exact frame grid.
    """
    def __init__(self, output_root, cache_root=None, backends=None):
        self.output_root = Path(output_root).resolve()
        self.cache_root = Path(cache_root or self.output_root / ".cache").resolve()
        if backends is None:
            from .backends import DemucsBackend, SeparatorBackend, SpleeterBackend
            backends = {"demucs": DemucsBackend(), "mlx": SeparatorBackend("mlx"),
                        "audio-separator": SeparatorBackend("audio-separator"), "spleeter": SpleeterBackend()}
        self.backends = backends

    def separate(self, source, settings=None, progress=None):
        settings = settings or Settings()
        emit = progress or (lambda event: None)
        source = Path(source).resolve()
        started = time.monotonic()
        try:
            if not source.is_file():
                raise EngineError(f"Input does not exist: {source}")
            selected = {settings.backend}
            if settings.preset == "best":
                selected.add(settings.vocal_backend)
            identities = {name: self.backends[name].identity(settings) for name in sorted(selected)}
            provenance = {"engine": "0.1.0", "implementation_sha256": hashlib.sha256(b"".join(p.read_bytes() for p in sorted(Path(__file__).parent.glob("*.py")))).hexdigest(), "schema": SCHEMA, "input_sha256": digest(source),
                          "settings": asdict(settings), "backends": identities,
                          "audio_libraries": {p: importlib.metadata.version(p) for p in ("numpy", "scipy", "soundfile")}}
            key = hashlib.sha256(json.dumps(provenance, sort_keys=True).encode()).hexdigest()
            target = self.output_root / key
            self.output_root.mkdir(parents=True, exist_ok=True)
            self.cache_root.mkdir(parents=True, exist_ok=True)
            # OS releases this lock on process exit; no stale lock recovery is needed.
            with open(self.cache_root / (key + ".lock"), "a") as lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
                if target.exists():
                    metadata = self.verify(target)
                    emit({"stage": "cache_hit", "output": str(target)})
                    return target
                emit({"stage": "decode"})
                mix, rate = read(source)
                if digest(source) != provenance["input_sha256"]:
                    raise EngineError("Input changed while decoding; retry with a stable file")
                with tempfile.TemporaryDirectory(prefix=".pending-", dir=self.output_root) as tmp:
                    scratch = Path(tmp)
                    stages = []
                    def run(name, audio, model, stage):
                        emit({"stage": stage, "backend": name, "model": model})
                        values = self.backends[name].separate(audio, rate, model, settings, scratch, emit)
                        stages.append({"stage": stage, "backend": name, "model": model, "weights": getattr(self.backends[name], "last_provenance", {})})
                        return {k: align(v, len(mix), mix.shape[1]) for k, v in values.items()}
                    if settings.preset == "best":
                        vocal_raw = run(settings.vocal_backend, mix, settings.vocal_model, "extract_vocals")
                        if "vocals" not in vocal_raw:
                            raise EngineError("Vocal model did not return vocals")
                        vocals = vocal_raw["vocals"]
                        raw = run(settings.backend, mix - vocals, settings.resolved_model(), "split_instruments")
                        stems = canonical(raw)
                        # Second-stage vocal estimate retains missed vocals; this is an explicit
                        # experimental policy whose bleed must be assessed on real multitracks.
                        stems["vocals"] += vocals
                    else:
                        stems = canonical(run(settings.backend, mix, settings.resolved_model(), "separate"))
                    stems, residual_rms = consistent(mix, stems, settings.consistency)
                    if not all(np.isfinite(x).all() for x in stems.values()):
                        raise EngineError("Non-finite backend output after reconstruction")
                    emit({"stage": "write"})
                    result = scratch / "result"
                    result.mkdir()
                    files = []
                    for name in STEMS:
                        path = result / f"{name}.wav"
                        sf.write(path, stems[name], rate, subtype="FLOAT")
                        files.append({"name": name.upper(), "file": path.name, "sha256": digest(path),
                                      "peak": float(np.max(np.abs(stems[name])))})
                    reconstruction = mix.astype(np.float64) - sum(x.astype(np.float64) for x in stems.values())
                    metadata = {**provenance, "key": key, "sample_rate": rate, "frames": len(mix),
                                "channels": mix.shape[1], "stages": stages, "format": "WAV FLOAT32", "stems": files,
                                "seconds": time.monotonic() - started, "residual_rms_before": residual_rms,
                                "reconstruction_max_abs": float(np.max(np.abs(reconstruction))),
                                "platform": platform.platform(), "quality_validated": False}
                    (result / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
                    self.verify(result)
                    os.rename(result, target)
                emit({"stage": "complete", "output": str(target)})
                return target
        except (KeyboardInterrupt, EngineError):
            raise
        except Exception as exc:
            raise EngineError(f"Separation failed: {exc}") from exc

    @staticmethod
    def verify(folder):
        try:
            return Engine._verify(folder)
        except EngineError:
            raise
        except (OSError, ValueError, KeyError, TypeError, RuntimeError) as exc:
            raise EngineError(f"Invalid or incomplete stem set: {exc}") from exc

    @staticmethod
    def _verify(folder):
        folder = Path(folder)
        data = json.loads((folder / "metadata.json").read_text())
        if not isinstance(data, dict):
            raise EngineError("Metadata must be an object")
        if data.get("schema") != SCHEMA or [s["name"] for s in data["stems"]] != [s.upper() for s in STEMS] or [s["file"] for s in data["stems"]] != [f"{s}.wav" for s in STEMS]:
            raise EngineError("Invalid stem set schema or order")
        if {p.name for p in folder.glob("*.wav")} != {f"{s}.wav" for s in STEMS}:
            raise EngineError("Stem set must contain exactly four WAV files")
        for stem in data["stems"]:
            path = folder / stem["file"]
            info = sf.info(path)
            if (info.frames, info.samplerate, info.channels, info.subtype) != (data["frames"], data["sample_rate"], data["channels"], "FLOAT"):
                raise EngineError("Stem synchronization/format verification failed")
            if digest(path) != stem["sha256"]:
                raise EngineError("Cached stem checksum mismatch; use a new output root or remove the corrupt set")
        return data
