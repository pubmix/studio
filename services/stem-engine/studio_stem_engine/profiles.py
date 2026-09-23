"""Server-owned model profiles; HTTP clients cannot select arbitrary weight paths."""
from dataclasses import asdict
import json
import os
from pathlib import Path
from .engine import Settings

BUILTINS = {
    "demucs-four": {"backend": "demucs", "model": "htdemucs"},
    "demucs-six": {"backend": "demucs", "model": "htdemucs_6s"},
}


def profiles():
    result = dict(BUILTINS)
    if os.environ.get("STUDIO_MODEL_PROFILES"):
        extra = json.loads(Path(os.environ["STUDIO_MODEL_PROFILES"]).read_text())
        if not isinstance(extra, dict): raise ValueError("Profiles must be an object")
        for name, config in extra.items():
            if name in result: raise ValueError("Cannot override built-in model profiles")
            if not isinstance(config, dict) or set(config) != {"backend", "model"}:
                raise ValueError("Each profile requires backend and model")
            if config["backend"] not in {"demucs", "mlx", "audio-separator", "roformer"} or not isinstance(config["model"], str) or not config["model"]:
                raise ValueError("Invalid model profile")
            result[name] = config
    return result


def resolve(mode="four", model=None, stem_depth=16):
    model = model or ("demucs-four" if mode == "four" else "demucs-six")
    available = profiles()
    if model not in available: raise ValueError("Unknown server model profile")
    settings = Settings(preset="fast", mode=mode, stem_depth=stem_depth,
                        fallback=True, **available[model])
    return asdict(settings)
