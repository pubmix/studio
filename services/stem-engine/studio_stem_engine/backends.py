"""Model integrations. Imports stay lazy so the public engine has no ML dependency."""
from pathlib import Path
import importlib.metadata as metadata
import os
import hashlib
import random
import re
import subprocess
import tempfile
import numpy as np
import soundfile as sf
from .audio import resample, align
from .engine import EngineError


def version(package):
    try:
        return metadata.version(package)
    except metadata.PackageNotFoundError:
        raise EngineError(f"Missing backend dependency: {package}. See installation instructions.")


class DemucsBackend:
    def identity(self, settings):
        return {"adapter": "demucs-direct-v1", "demucs": version("demucs"), "torch": version("torch")}

    def separate(self, audio, rate, model, settings, scratch, progress):
        import torch
        from demucs.pretrained import get_model
        from demucs.apply import apply_model
        random.seed(settings.seed)
        torch.manual_seed(settings.seed)
        net = get_model(model.removesuffix(".yaml"))
        net.eval()
        weight_hash = hashlib.sha256()
        for key, value in sorted(net.state_dict().items()):
            weight_hash.update(key.encode())
            weight_hash.update(value.detach().cpu().numpy().tobytes())
        self.last_provenance = {"model": model, "state_dict_sha256": weight_hash.hexdigest()}
        wave = resample(audio, rate, net.samplerate)
        if wave.shape[1] == 1:
            wave = np.repeat(wave, 2, axis=1)
        tensor = torch.from_numpy(wave.T.copy())
        ref = tensor.mean(0)
        mean, std = ref.mean(), ref.std()
        if std < 1e-8:
            # Avoid NaNs for silence/DC. Consistency allocates any DC to melody.
            return {s: np.zeros_like(audio) for s in net.sources}
        normalized = (tensor - mean) / std
        with torch.inference_mode():
            estimates = apply_model(net, normalized[None], device=settings.device,
                                    shifts=settings.shifts, overlap=settings.overlap,
                                    split=True, progress=False, num_workers=0)[0]
        estimates = estimates * std + mean
        result = {}
        for name, estimate in zip(net.sources, estimates):
            value = estimate.cpu().numpy().T
            if audio.shape[1] == 1:
                value = value.mean(axis=1, keepdims=True)
            result[name] = align(resample(value, net.samplerate, rate), len(audio), audio.shape[1])
        return result


class SeparatorBackend:
    """RoFormer, MDX/MDXC, and Demucs via public Separator API.

    Use FLOAT output and disable normalization; backend gain changes would break
    a staged residual. Model output labels are explicitly requested, never guessed
    from order. Two-source output is accepted only by the vocal extraction stage.
    """
    def __init__(self, runtime):
        self.runtime = runtime

    def identity(self, settings):
        package = "mlx-audio-separator" if self.runtime == "mlx" else "audio-separator"
        return {"adapter": "separator-v1", "package": package, "version": version(package),
                "runtime": {p: version(p) for p in (("mlx", "mlx-audio-io") if self.runtime == "mlx" else ("torch", "onnxruntime"))}}

    def separate(self, audio, rate, model, settings, scratch, progress):
        if self.runtime == "mlx":
            from mlx_audio_separator import Separator
        else:
            from audio_separator.separator import Separator
        with tempfile.TemporaryDirectory(dir=scratch, prefix="separator-") as temp:
            temp = Path(temp)
            source = temp / "input.wav"
            sf.write(source, audio, rate, subtype="FLOAT")
            model_dir = Path(os.environ.get("STUDIO_MODEL_DIR", scratch.parent / ".models")).resolve()
            model_dir.mkdir(parents=True, exist_ok=True)
            random.seed(settings.seed)
            np.random.seed(settings.seed)
            extra = {"use_soundfile": True} if self.runtime != "mlx" else {}
            demucs_params = {"segment_size": "Default", "shifts": settings.shifts, "overlap": settings.overlap, "segments_enabled": True}
            if self.runtime == "mlx":
                import mlx.core as mx
                mx.random.seed(settings.seed)
                demucs_params.update(batch_size=1, seed=settings.seed)
            else:
                import torch
                torch.manual_seed(settings.seed)
            separator = Separator(output_dir=str(temp), output_format="WAV", demucs_params=demucs_params, **extra,
                                  model_file_dir=str(model_dir), normalization_threshold=1.0,
                                  amplification_threshold=0.0, sample_rate=44100)
            # Disable per-source attenuation, including estimates above digital full scale.
            # This adapter targets the pinned Separator implementation.
            separator.normalization_threshold = float("inf")
            if self.runtime != "mlx":
                import torch
                separator.torch_device = torch.device(settings.device)
                separator.torch_device_mps = torch.device("mps") if settings.device == "mps" else None
                if settings.device == "cpu":
                    separator.onnx_execution_provider = ["CPUExecutionProvider"]
            separator.load_model(model_filename=model if "." in model else model + ".yaml")
            labels = ["Vocals", "Instrumental", "Other", "Bass", "Drums", "Guitar", "Piano", "Strings"]
            mapping = {label: label.lower() for label in labels}
            from .engine import digest
            artifacts = [p for p in model_dir.glob(Path(model).stem + ".*") if p.is_file()]
            self.last_provenance = {"model": model, "artifacts": [{"file": p.name, "sha256": digest(p)} for p in sorted(artifacts)]}
            separator.model_instance.normalization_threshold = float("inf")
            outputs = separator.separate(str(source), custom_output_names=mapping)
            result = {}
            for output in outputs:
                path = Path(output)
                if not path.is_absolute():
                    path = temp / path
                label = path.stem.lower()
                if label not in mapping.values():
                    raise EngineError(f"Unrecognized separator output: {path.name}")
                value, out_rate = sf.read(path, dtype="float32", always_2d=True)
                if audio.shape[1] == 1 and value.shape[1] == 2:
                    value = value.mean(axis=1, keepdims=True)
                result[label] = align(resample(value, out_rate, rate), len(audio), audio.shape[1])
            return result


class SpleeterBackend:
    """Baseline in an isolated Python environment, selected by STUDIO_SPLEETER_PYTHON."""
    def identity(self, settings):
        executable = os.environ.get("STUDIO_SPLEETER_PYTHON")
        if not executable:
            raise EngineError("Set STUDIO_SPLEETER_PYTHON to the isolated Spleeter environment's Python")
        result = subprocess.run([executable, "-c", "import importlib.metadata; print(importlib.metadata.version('spleeter'))"],
                                capture_output=True, text=True, check=True)
        return {"adapter": "spleeter-worker-v1", "version": result.stdout.strip()}

    def separate(self, audio, rate, model, settings, scratch, progress):
        executable = os.environ["STUDIO_SPLEETER_PYTHON"]
        with tempfile.TemporaryDirectory(dir=scratch, prefix="spleeter-") as temp:
            temp = Path(temp)
            wave = resample(audio, rate, 44100)
            if wave.shape[1] == 1:
                wave = np.repeat(wave, 2, axis=1)
            np.save(temp / "input.npy", wave)
            code = """
import sys, numpy as np
from spleeter.separator import Separator
from pathlib import Path
p = Path(sys.argv[1])
r = Separator('spleeter:4stems').separate(np.load(p / 'input.npy'))
np.savez(p / 'estimates.npz', **r)
"""
            subprocess.run([executable, "-c", code, str(temp)], check=True)
            with np.load(temp / "estimates.npz") as estimates:
                result = {}
                for key in estimates.files:
                    value = estimates[key]
                    if audio.shape[1] == 1:
                        value = value.mean(axis=1, keepdims=True)
                    result[key] = align(resample(value, 44100, rate), len(audio), audio.shape[1])
                return result


class EnsembleBackend:
    """Explicit waveform ensemble of full four-source backends for controlled experiments.

    members: [(backend_instance, model_name, positive_weight), ...]. All members
    must preserve gain and timing. Quality is not assumed to improve by averaging.
    """
    def __init__(self, members):
        if not members or any(not np.isfinite(w) or w <= 0 for _, _, w in members):
            raise ValueError('Ensemble needs positive finite member weights')
        self.members = members

    def identity(self, settings):
        return {'adapter':'waveform-ensemble-v1', 'members':[
            {'backend':b.identity(settings),'model':m,'weight':w} for b,m,w in self.members]}

    def separate(self, audio, rate, model, settings, scratch, progress):
        from .engine import canonical, STEMS
        total=sum(w for _,_,w in self.members)
        result={s:np.zeros_like(audio) for s in STEMS}
        for backend, name, weight in self.members:
            progress({'stage':'ensemble_member','model':name})
            sources=canonical(backend.separate(audio,rate,name,settings,scratch,progress))
            for stem in STEMS:
                result[stem] += align(sources[stem],len(audio),audio.shape[1])*(weight/total)
        return result
