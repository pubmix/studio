"""Source taxonomy, conservative selection and bank/channel mapping."""
from dataclasses import dataclass, field
import math
import numpy as np

GROUPS = ("vocals", "melody", "bass", "rhythm")
FAMILIES = {
    "vocals": "vocals", "lead_vocal": "vocals", "backing_vocals": "vocals",
    "melody": "melody", "other": "melody", "guitar": "melody", "piano": "melody",
    "keys": "melody", "synth": "melody", "strings": "melody", "brass": "melody", "fx": "melody",
    "bass": "bass", "rhythm": "rhythm", "drums": "rhythm", "kick": "rhythm",
    "snare": "rhythm", "hats": "rhythm", "cymbals": "rhythm", "percussion": "rhythm",
}
ALIASES = {"lead vocals": "lead_vocal", "lead vocal": "lead_vocal",
           "backing vocals": "backing_vocals", "hi-hat": "hats"}

@dataclass
class SeparationResult:
    """Optional backend confidence is source presence, never separation quality.

    Sources must form a disjoint partition, not parent stems plus their children.
    Scores must be calibrated by the backend on held-out data before supplying them.
    """
    sources: dict
    presence_confidence: dict = field(default_factory=dict)


def label(name):
    value = ALIASES.get(name.lower(), name.lower())
    if value not in FAMILIES:
        raise ValueError(f"Unsupported backend source label: {name}")
    return value


def group_sources(raw):
    result = {}
    for name, samples in raw.items():
        group = FAMILIES[label(name)]
        result[group] = result.get(group, 0) + samples
    if set(result) != set(GROUPS):
        raise ValueError("Backend must cover vocals, melody, bass and rhythm; a two-stem vocal model is not a full separator")
    return {name: result[name] for name in GROUPS}


def select_sources(mix, raw, settings, scores=None):
    sources = {}
    for name, samples in raw.items():
        key = label(name)
        if key in sources:
            raise ValueError("Duplicate source aliases")
        sources[key] = samples
    group_sources(sources)  # Require complete coverage before any projection.
    scores = {label(k): v for k, v in (scores or {}).items()}
    if set(scores) - set(sources):
        raise ValueError("Confidence supplied for an absent source")
    for value in scores.values():
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not 0 <= value <= 1:
            raise ValueError("Presence confidence must be finite and between zero and one")
    mixture_rms = float(np.sqrt(np.mean(mix.astype(np.float64)**2)))
    measurements = {}
    for name, samples in sources.items():
        rms = float(np.sqrt(np.mean(samples.astype(np.float64)**2)))
        relative_db = 20 * math.log10(max(rms, 1e-12) / max(mixture_rms, 1e-12))
        confidence = scores.get(name)
        reason = "silent" if rms <= 1e-8 else None
        if settings.mode == "dynamic":
            if relative_db < settings.presence_db: reason = "low_energy"
            if confidence is not None and confidence < settings.min_confidence: reason = "low_confidence"
        measurements[name] = dict(rms=rms, relative_db=relative_db, presence_confidence=confidence,
                                  presence_method="backend" if confidence is not None else "energy_heuristic",
                                  separation_quality=None, suppressed_reason=reason)
    kept = {k: v for k, v in sources.items() if measurements[k]["suppressed_reason"] is None}
    folded = {group: [] for group in GROUPS}
    for k in sources:
        if k not in kept: folded[FAMILIES[k]].append(k)
    # Suppressed estimates are retained in group remainders; never discard their audio.
    def count():
        return len(kept) + sum(bool(v) and g not in kept for g, v in folded.items())
    while count() > settings.stem_depth:
        candidates = [k for k in kept if k != FAMILIES[k]]
        if not candidates: raise ValueError("Cannot fit source mapping")
        name = min(candidates, key=lambda k: (measurements[k]["rms"], k))
        folded[FAMILIES[name]].append(name)
        del kept[name]
        measurements[name]["suppressed_reason"] = "channel_limit"
    for group, names in folded.items():
        if names:
            kept[group] = kept.get(group, 0) + sum(sources[n] for n in names)
    residual = mix.astype(np.float64) - sum(v.astype(np.float64) for v in sources.values())
    residual_rms = float(np.sqrt(np.mean(residual**2)))
    # Keep reconstruction correction out of isolated children.
    if settings.consistency != "none" and np.max(np.abs(residual)) > 1e-8:
        group = "melody"
        if group not in kept and "other" not in kept and len(kept) >= settings.stem_depth:
            names = [k for k in kept if FAMILIES[k] == group]
            kept[group] = sum(kept.pop(k) for k in names)
            for k in names:
                measurements[k]["suppressed_reason"] = "reconstruction_remainder"
                folded[group].append(k)
        remainder = "other" if "other" in kept else group
        kept[remainder] = kept.get(remainder, np.zeros_like(mix)) + residual
    ordered = sorted(kept, key=lambda k: (GROUPS.index(FAMILIES[k]), k))
    channels = []
    for i, name in enumerate(ordered):
        channels.append(dict(name=name.upper(), group=FAMILIES[name].upper(), channel=i+1,
                             bank=i//4+1, lane=i%4+1, folded_sources=folded.get(name, []),
                             **(measurements.get(name, {}) if not folded.get(name) else
                                {"presence_confidence": None, "presence_method": "group_remainder", "separation_quality": None})))
    return {k: kept[k].astype(np.float32) for k in ordered}, channels, measurements, residual_rms
