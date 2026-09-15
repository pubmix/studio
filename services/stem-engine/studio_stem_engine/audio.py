from pathlib import Path
import subprocess
import tempfile
import numpy as np
import soundfile as sf
import imageio_ffmpeg
from scipy.signal import resample_poly
from math import gcd


def read(path: Path):
    if path.suffix.lower() not in {".wav", ".mp3"}:
        raise ValueError("Input must be MP3 or WAV")
    try:
        data, rate = sf.read(path, dtype="float32", always_2d=True)
    except (RuntimeError, sf.LibsndfileError):
        with tempfile.TemporaryDirectory() as temp:
            decoded = Path(temp) / "decoded.wav"
            subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), "-v", "error", "-i", str(path),
                            "-c:a", "pcm_f32le", str(decoded)], check=True, capture_output=True)
            data, rate = sf.read(decoded, dtype="float32", always_2d=True)
    if not len(data) or data.shape[1] not in (1, 2) or not np.isfinite(data).all():
        raise ValueError("Audio must be finite, nonempty mono or stereo")
    return data, rate


def resample(data, source_rate, target_rate):
    if source_rate == target_rate:
        return data.copy()
    factor = gcd(source_rate, target_rate)
    return resample_poly(data, target_rate // factor, source_rate // factor, axis=0).astype(np.float32)


def align(data, frames, channels):
    if data.ndim != 2 or data.shape[1] != channels or not np.isfinite(data).all():
        raise ValueError("Backend returned invalid samples or channel count")
    # Only tolerate resampling roundoff, never silently pad a truncated inference.
    if abs(len(data) - frames) > 2:
        raise ValueError(f"Backend alignment error: expected {frames} frames, got {len(data)}")
    return np.pad(data[:frames], ((0, max(0, frames-len(data))), (0, 0)))
