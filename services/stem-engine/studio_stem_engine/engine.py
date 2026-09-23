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
    mode: str = "four"
    stem_depth: int = 16
    presence_db: float = -45.0
    min_confidence: float = 0.5
    fallback: bool = False

    def __post_init__(self):
        if self.mode not in {"four", "extended", "dynamic"}:
            raise ValueError("Unknown separation mode")
        if type(self.stem_depth) is not int or not 4 <= self.stem_depth <= 16:
            raise ValueError("stem_depth must be between 4 and 16")
        if not np.isfinite(self.presence_db) or not -120 <= self.presence_db <= 0:
            raise ValueError("presence_db must be between -120 and 0")
        if not np.isfinite(self.min_confidence) or not 0 <= self.min_confidence <= 1:
            raise ValueError("min_confidence must be between 0 and 1")
        if self.mode != "four" and self.preset == "best":
            raise ValueError("Extended modes require a full-source model, not the staged best preset")
        if self.preset not in {"fast", "balanced", "best"}:
            raise ValueError("Unknown preset")
        if self.consistency not in {"melody", "energy", "none"}:
            raise ValueError("Unknown consistency policy")
        if self.shifts < 0 or not 0 <= self.overlap < 1:
            raise ValueError("Invalid shifts or overlap")
        if self.device not in {"cpu", "mps", "cuda"}:
            raise ValueError("Device must be cpu, mps or cuda")

    def resolved_model(self):
        return self.model or ("htdemucs_6s" if self.mode != "four" else {"fast": "htdemucs", "balanced": "htdemucs_ft", "best": "htdemucs_6s"}[self.preset])


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def canonical(raw):
    """Unknown source labels are rejected; accompaniment is never mistaken for melody."""
    from .sources import group_sources
    try:
        return group_sources(raw)
    except ValueError as exc:
        raise EngineError(str(exc)) from exc



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
            backends = {"demucs": DemucsBackend(), "mlx": SeparatorBackend("mlx", self.output_root / ".models"),
                        "audio-separator": SeparatorBackend("audio-separator", self.output_root / ".models"), "roformer": SeparatorBackend("audio-separator", self.output_root / ".models"), "spleeter": SpleeterBackend()}
        self.backends = backends

    def separate(self, source, settings=None, progress=None):
        from dataclasses import replace
        settings = settings or Settings()
        try:
            return self._separate(source, settings, progress)
        except EngineError as exc:
            if not settings.fallback or settings.backend == "demucs":
                raise
            fallback = replace(settings, backend="demucs", model="htdemucs", preset="fast",
                               mode="four", fallback=False)
            if progress:
                progress({"stage": "fallback", "reason": str(exc), "model": "htdemucs"})
            # A distinct key prevents a fallback result masking a later healthy model.
            return self._separate(source, fallback, progress, fallback_info={
                "requested": asdict(settings), "reason": str(exc)})

    def _separate(self, source, settings=None, progress=None, fallback_info=None):
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
            provenance = {"engine": "0.2.0", "implementation_sha256": hashlib.sha256(b"".join(p.read_bytes() for p in sorted(Path(__file__).parent.glob("*.py")))).hexdigest(), "schema": SCHEMA if settings.mode == "four" else "studio.stems.v2", "fallback": fallback_info, "input_sha256": digest(source),
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
                    confidence = {}
                    def run(name, audio, model, stage):
                        emit({"stage": stage, "backend": name, "model": model})
                        values = self.backends[name].separate(audio, rate, model, settings, scratch, emit)
                        from .sources import SeparationResult
                        confidence.clear()
                        if isinstance(values, SeparationResult):
                            confidence.update(values.presence_confidence)
                            values = values.sources
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
                        raw = run(settings.backend, mix, settings.resolved_model(), "separate")
                        if settings.mode == "four":
                            stems = canonical(raw)
                    channels, measurements = [], {}
                    if settings.mode == "four":
                        stems, residual_rms = consistent(mix, stems, settings.consistency)
                    else:
                        from .sources import select_sources
                        stems, channels, measurements, residual_rms = select_sources(mix, raw, settings, confidence)
                    if not all(np.isfinite(x).all() for x in stems.values()):
                        raise EngineError("Non-finite backend output after reconstruction")
                    emit({"stage": "write"})
                    result = scratch / "result"
                    result.mkdir()
                    files = []
                    for name in stems:
                        path = result / f"{name}.wav"
                        sf.write(path, stems[name], rate, subtype="FLOAT")
                        files.append({"name": name.upper(), "file": path.name, "sha256": digest(path),
                                      "peak": float(np.max(np.abs(stems[name])))})
                    if settings.mode != "four":
                        for entry, channel in zip(files, channels):
                            entry.update(channel)
                        grouped = canonical(stems)
                        four = result / "four"
                        four.mkdir()
                        group_files = []
                        for name in STEMS:
                            path = four / f"{name}.wav"
                            sf.write(path, grouped[name], rate, subtype="FLOAT")
                            group_files.append({"name": name.upper(), "file": path.name, "sha256": digest(path)})
                    reconstruction = mix.astype(np.float64) - sum(x.astype(np.float64) for x in stems.values())
                    metadata = {**provenance, "key": key, "sample_rate": rate, "frames": len(mix),
                                "channels": mix.shape[1], "stages": stages, "format": "WAV FLOAT32", "stems": files,
                                "seconds": time.monotonic() - started, "residual_rms_before": residual_rms,
                                "reconstruction_max_abs": float(np.max(np.abs(reconstruction))),
                                "platform": platform.platform(), "quality_validated": False}
                    metadata.update(source_measurements=measurements, channel_count=len(files))
                    if settings.mode != "four":
                        four_metadata = {**metadata, "schema": SCHEMA, "stems": group_files, "channel_count": 4}
                        (four / "metadata.json").write_text(json.dumps(four_metadata, indent=2) + "\n")
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
        if data.get("schema") == "studio.stems.v2":
            from .sources import FAMILIES, GROUPS
            entries = data["stems"]
            names = [e["name"].lower() for e in entries]
            if not 4 <= len(entries) <= 16 or len(set(names)) != len(names):
                raise EngineError("Invalid extended channel count")
            if any(n not in FAMILIES for n in names):
                raise EngineError("Invalid extended source name")
            if names != sorted(names, key=lambda k: (GROUPS.index(FAMILIES[k]), k)):
                raise EngineError("Invalid extended source order")
            for i, e in enumerate(entries):
                if (e["file"], e["channel"], e["bank"], e["lane"], e["group"]) != (names[i]+".wav", i+1, i//4+1, i%4+1, FAMILIES[names[i]].upper()):
                    raise EngineError("Invalid channel mapping")
            if (folder / "four").is_symlink() or json.loads((folder / "four" / "metadata.json").read_text()).get("schema") != SCHEMA:
                raise EngineError("Invalid four-stem mixdown")
            four = Engine.verify(folder / "four")
            if any(data[k] != four[k] for k in ("frames", "sample_rate", "channels", "key")):
                raise EngineError("Four-stem mixdown grid mismatch")
        elif data.get("schema") != SCHEMA or [s["name"] for s in data["stems"]] != [s.upper() for s in STEMS] or [s["file"] for s in data["stems"]] != [f"{s}.wav" for s in STEMS]:
            raise EngineError("Invalid stem set schema or order")
        if {p.name for p in folder.glob("*.wav")} != {s["file"] for s in data["stems"]}:
            raise EngineError("Unexpected stem files")
        for stem in data["stems"]:
            path = folder / stem["file"]
            if path.is_symlink() or path.resolve().parent != folder.resolve():
                raise EngineError("Stem must be inside its folder")
            info = sf.info(path)
            if (info.frames, info.samplerate, info.channels, info.subtype) != (data["frames"], data["sample_rate"], data["channels"], "FLOAT"):
                raise EngineError("Stem synchronization/format verification failed")
            if digest(path) != stem["sha256"]:
                raise EngineError("Cached stem checksum mismatch; use a new output root or remove the corrupt set")
        return data
