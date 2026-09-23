"""Client for the existing ESP32 /status and multipart /upload endpoints."""
import http.client
import json
from pathlib import Path
import re
import uuid
import tempfile
import struct
import zlib
from urllib.parse import urlsplit, urlencode
from .engine import EngineError, digest
from .transfer import verify_transfer


class UploadError(EngineError):
    def __init__(self, message, uploaded):
        self.uploaded = list(uploaded)
        super().__init__(message)


def upload_transfer(folder, device, *, progress=None, timeout=60, transfer_mode="wav"):
    """Sequential, bounded-memory upload. No retry or automatic playback.

    Returns actual SD filenames (firmware adds collision suffixes). An error
    carries confirmed uploads; the failing request may have reached the device.
    """
    if transfer_mode not in ("wav", "deflate-blocks-v1"):
        raise ValueError("Unknown transfer mode")
    manifest = verify_transfer(folder)
    url = urlsplit(device)
    if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password or url.path not in ("", "/") or url.query or url.fragment:
        raise ValueError("Device must be an http(s) origin, e.g. http://dubbox.local")
    if timeout <= 0:
        raise ValueError("timeout must be positive")
    emit = progress or (lambda event: None)
    uploaded = []
    connection = http.client.HTTPSConnection if url.scheme == "https" else http.client.HTTPConnection

    def response(conn):
        reply = conn.getresponse()
        raw = reply.read(65537)
        if reply.status != 200 or len(raw) > 65536:
            raise EngineError(f"Device HTTP {reply.status} or oversized response")
        body = json.loads(raw)
        if not isinstance(body, dict):
            raise EngineError("Invalid device response")
        return body

    try:
        # Validate every size before sending the first file (Teensy beginUpload limits).
        for entry in manifest["stems"]:
            if not 64 <= (Path(folder) / entry["file"]).stat().st_size <= 0x7fff0000:
                raise EngineError("WAV size outside Teensy upload limits")
        for lane, entry in enumerate(manifest["stems"], 1):
            conn = connection(url.hostname, url.port, timeout=timeout)
            try:
                conn.request("GET", "/status", headers={"Cache-Control": "no-store"})
                status = response(conn)
            finally:
                conn.close()
            if status.get("linked") is not True or status.get("busy") is not False or status.get("playing") is not False:
                raise EngineError("Device must be linked, idle and stopped before upload")
            path = Path(folder) / entry["file"]
            if digest(path) != entry["sha256"]:
                raise EngineError("Transfer changed during upload")
            if transfer_mode != "wav" and transfer_mode not in status.get("transfer_modes", []):
                raise EngineError("Device does not support compressed transfer; choose Original WAV")
            name = f"S{manifest['set_id']}-{entry['role']}"
            boundary = "studio" + uuid.uuid4().hex
            prefix = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"a.wav\"\r\nContent-Type: audio/wav\r\n\r\n").encode()
            suffix = f"\r\n--{boundary}--\r\n".encode()
            size = path.stat().st_size
            with tempfile.TemporaryFile() as encoded:
                mode = transfer_mode
                wire_size = size
                if mode != "wav":
                    with path.open("rb") as source:
                        while block := source.read(8192):
                            compressed = zlib.compress(block)
                            encoded.write(struct.pack("<HH", len(compressed), len(block)))
                            encoded.write(compressed)
                    wire_size = encoded.tell()
                    if wire_size >= size:
                        mode, wire_size = "wav", size
                    encoded.seek(0)
                emit({"stage": "upload", "lane": lane, "role": entry["role"], "bytes": size,
                      "wire_bytes": wire_size, "transfer_mode": mode})
                conn = connection(url.hostname, url.port, timeout=timeout)
                try:
                    query = {"name": name, "size": size}
                    if mode != "wav": query.update(encoding=mode, wire_size=wire_size)
                    conn.putrequest("POST", "/upload?" + urlencode(query))
                    conn.putheader("Content-Type", f"multipart/form-data; boundary={boundary}")
                    conn.putheader("Content-Length", str(len(prefix) + wire_size + len(suffix)))
                    conn.endheaders()
                    conn.send(prefix)
                    with path.open("rb") as original:
                        source = encoded if mode != "wav" else original
                        while block := source.read(65536): conn.send(block)
                    conn.send(suffix)
                    result = response(conn)
                finally:
                    conn.close()
            if result.get("ok") is not True:
                raise EngineError(f"Device rejected {entry['role']}: {result.get('error', 'unknown error')}")
            actual = result.get("name")
            if not isinstance(actual, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,43}\.wav", actual):
                raise EngineError("Device returned an invalid SD filename")
            uploaded.append({"lane": lane, "role": entry["role"], "file": actual})
            emit({"stage": "uploaded", **uploaded[-1]})
    except (OSError, ValueError, EngineError, http.client.HTTPException) as exc:
        raise UploadError(f"Upload stopped: {exc}. Confirmed files remain on SD; inspect before retrying.", uploaded) from exc
    return uploaded
