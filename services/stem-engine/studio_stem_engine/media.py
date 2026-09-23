"""YouTube-only acquisition adapter. No credentials, playlists, or arbitrary URLs."""
from pathlib import Path
import re
import os
import shutil
import subprocess
import imageio_ffmpeg

VIDEO_ID = re.compile(r"^[A-Za-z0-9_-]{11}$")
MAX_SECONDS = 900
MAX_BYTES = 100 * 1024 * 1024


def video_id(value):
    from urllib.parse import urlsplit, parse_qs
    if VIDEO_ID.fullmatch(value):
        return value
    u = urlsplit(value)
    if u.scheme != "https" or u.username or u.password or u.port not in (None, 443):
        raise ValueError("Use a YouTube video link")
    if u.hostname == "youtu.be":
        candidate = u.path.strip("/")
    elif u.hostname in ("youtube.com", "www.youtube.com", "m.youtube.com"):
        candidate = parse_qs(u.query).get("v", [""])[0] if u.path == "/watch" else u.path.removeprefix("/shorts/")
    else:
        raise ValueError("Only YouTube links are supported")
    if not VIDEO_ID.fullmatch(candidate):
        raise ValueError("Invalid YouTube video ID")
    return candidate


def _options():
    runtime = os.environ.get("STUDIO_JS_RUNTIME") or shutil.which("node")
    return {"js_runtimes": {"node": {"path": runtime}} if runtime else {"deno": {}}, "quiet": True, "no_warnings": True, "noplaylist": True,
            "socket_timeout": 15, "retries": 1, "fragment_retries": 1,
            "cachedir": False, "restrictfilenames": True}


def search(query):
    import yt_dlp
    query = query.strip()
    if not query or len(query) > 100:
        raise ValueError("Search must be 1 to 100 characters")
    direct = query.startswith(("https://", "http://")) or bool(VIDEO_ID.fullmatch(query))
    target = "https://www.youtube.com/watch?v=" + video_id(query) if direct else "ytsearch5:" + query
    with yt_dlp.YoutubeDL({**_options(), "extract_flat": "in_playlist", "skip_download": True}) as ydl:
        result = ydl.extract_info(target, download=False)
    entries = result.get("entries", []) if "entries" in result else [result]
    values = []
    for e in entries:
        if not e or not VIDEO_ID.fullmatch(e.get("id", "")):
            continue
        seconds = e.get("duration")
        if not seconds or seconds > MAX_SECONDS or e.get("is_live"):
            continue
        values.append({"id": e["id"], "title": str(e.get("title", "Untitled"))[:120],
                       "uploader": str(e.get("uploader") or e.get("channel") or "")[:80],
                       "duration": int(seconds)})
    return values[:5]


def acquire(identifier, folder, progress):
    import yt_dlp
    identifier = video_id(identifier)
    folder = Path(folder)
    def hook(event):
        done = event.get("downloaded_bytes", 0)
        total = event.get("total_bytes")
        if done > MAX_BYTES:
            raise ValueError("Download exceeds 100 MiB limit")
        progress({"stage": "downloading", "downloaded_bytes": done,
                  "total_bytes": total, "percent": min(100, int(done * 100 / total)) if total else None})
    def match(info, *, incomplete=False):
        if info.get("is_live") or (info.get("duration") or 0) > MAX_SECONDS:
            return "Live streams and videos over 15 minutes are not supported"
    opts = {**_options(), "format": "bestaudio/best", "outtmpl": str(folder / "source.%(ext)s"),
            "max_filesize": MAX_BYTES, "match_filter": match, "progress_hooks": [hook]}
    with yt_dlp.YoutubeDL(opts) as ydl:
        info = ydl.extract_info("https://www.youtube.com/watch?v=" + identifier, download=True)
        if not info or not info.get("duration") or info["duration"] > MAX_SECONDS:
            raise ValueError("Video duration unavailable or exceeds 15 minutes")
        source = Path(ydl.prepare_filename(info))
    if not source.is_file() or source.stat().st_size > MAX_BYTES:
        raise ValueError("Audio download missing or too large")
    progress({"stage": "decoding"})
    decoded = folder / "input.wav"
    decode(source, decoded)
    return decoded, str(info.get("title", identifier))[:120]


def decode(source, destination, input_format=None):
    """Bound file expansion before loading samples into the model."""
    import soundfile as sf
    subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), "-nostdin", "-v", "error",
                    "-protocol_whitelist", "file,pipe",
                    *(["-f", input_format] if input_format else []), "-i", str(source),
                    "-t", str(MAX_SECONDS + 1), "-vn", "-ac", "2", "-ar", "44100", "-c:a", "pcm_f32le", str(destination)],
                   check=True, timeout=180, capture_output=True)
    info = sf.info(destination)
    if not info.frames or info.duration > MAX_SECONDS:
        raise ValueError("Choose a song no longer than 15 minutes")
    return Path(destination)
