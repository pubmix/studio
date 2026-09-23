"""Single-device pilot API. One process / one concurrent job; private file storage."""
from contextlib import asynccontextmanager
from collections import deque
import hmac
import json
import os
from pathlib import Path
import secrets
import selectors
import signal
import sqlite3
import subprocess
import sys
import threading
import time
import shutil
from fastapi import FastAPI, Header, HTTPException, Depends, Query, Request
from fastapi.responses import FileResponse, HTMLResponse
from pydantic import BaseModel, Field, StrictBool
from typing import Literal
from .profiles import resolve, profiles
from . import media
from .transfer import verify_transfer

TERMINAL = {"ready", "failed", "cancelled"}
STAGES = {"decode": "decoding", "separate": "separating", "inference": "separating",
          "write": "exporting", "exporting": "exporting", "downloading": "downloading",
          "decoding": "decoding", "prepared": "exporting"}


class NewJob(BaseModel):
    video_id: str = Field(pattern=r"^[A-Za-z0-9_-]{11}$")
    mode: Literal["four", "extended", "dynamic"] = "four"
    model: str | None = Field(default=None, max_length=120)
    stem_depth: int = Field(default=16, ge=4, le=16, strict=True)
    rights_confirmed: StrictBool
    request_id: str = Field(min_length=16, max_length=64, pattern=r"^[A-Za-z0-9_-]+$")


class Jobs:
    def __init__(self, root, command=None, timeout=1800, retention=86400):
        self.root = Path(root).resolve()
        self.root.mkdir(parents=True, exist_ok=True)
        self.lock = threading.RLock()
        self.stop = threading.Event()
        self.cancelled = set()
        self.thread = None
        self.command = command or [sys.executable, "-u", "-m", "studio_stem_engine.cloud_worker"]
        self.timeout, self.retention = timeout, retention
        with self.db() as db:
            db.execute("CREATE TABLE IF NOT EXISTS jobs (id TEXT PRIMARY KEY, request_id TEXT UNIQUE, video_id TEXT, created REAL, state TEXT, data TEXT)")
            for row in db.execute("SELECT id,data FROM jobs WHERE state NOT IN ('ready','failed','cancelled')").fetchall():
                data = json.loads(row[1]);data.update(state="failed", message="Server restarted; please start a new job", percent=None)
                db.execute("UPDATE jobs SET state='failed',data=? WHERE id=?", (json.dumps(data), row[0]))
        self.cleanup()

    def db(self):
        return sqlite3.connect(self.root / "jobs.sqlite", timeout=10)

    def cleanup(self):
        with self.lock, self.db() as db:
            rows = db.execute("SELECT id FROM jobs WHERE created < ? AND state IN ('ready','failed','cancelled')", (time.time()-self.retention,)).fetchall()
            for (identifier,) in rows:
                shutil.rmtree(self.root / identifier, ignore_errors=True)
                db.execute("DELETE FROM jobs WHERE id=?", (identifier,))

    def get(self, identifier):
        with self.lock, self.db() as db:
            row = db.execute("SELECT data FROM jobs WHERE id=?", (identifier,)).fetchone()
        if not row:
            raise HTTPException(404, "Job not found or expired")
        return json.loads(row[0])

    def save(self, data):
        with self.lock, self.db() as db:
            db.execute("UPDATE jobs SET state=?,data=? WHERE id=?", (data["state"], json.dumps(data), data["id"]))

    def create(self, request, source=None, title=None, source_key=None):
        if not request.rights_confirmed:
            raise HTTPException(400, "Confirm download and separation rights first")
        try:
            settings = resolve(request.mode, request.model, request.stem_depth)
        except ValueError as exc:
            raise HTTPException(400, str(exc)) from exc
        self.cleanup()
        with self.lock, self.db() as db:
            row = db.execute("SELECT video_id,data FROM jobs WHERE request_id=?", (request.request_id,)).fetchone()
            if row:
                previous = json.loads(row[1])
                if previous.get("source_key") != source_key or row[0] != request.video_id or previous.get("requested_settings", resolve()) != settings:
                    raise HTTPException(409, "Request ID already used with different video or separation settings")
                return json.loads(row[1])
            if self.thread and self.thread.is_alive():
                raise HTTPException(409, "One split is already running")
            if shutil.disk_usage(self.root).free < 4 * 1024**3:
                raise HTTPException(507, "Processing storage is low")
            identifier = secrets.token_hex(12)
            folder = self.root / identifier;folder.mkdir()
            set_id = secrets.randbelow(0xffffffff) + 1
            spec = {"video_id": request.video_id, "set_id": set_id, "settings": settings}
            if source is not None:
                shutil.move(str(source), folder / "input.audio")
                spec.update(local_file="input.audio", title=title or "Uploaded song")
            (folder / "request.json").write_text(json.dumps(spec))
            data = {"id": identifier, "state": "queued", "percent": None, "message": "Queued",
                    "requested_settings": settings, "set_id": set_id, "project": "ST" + identifier[:10].upper(), "title": title or "", "stems": [], "source_key": source_key}
            db.execute("INSERT INTO jobs VALUES (?,?,?,?,?,?)", (identifier, request.request_id, request.video_id, time.time(), "queued", json.dumps(data)))
            db.commit()
            self.thread = threading.Thread(target=self.run, args=(identifier,), daemon=True)
            self.thread.start()
            return data

    def cancel(self, identifier):
        with self.lock:
            data = self.get(identifier)
            if data["state"] not in TERMINAL:
                self.cancelled.add(identifier)
            return data

    def run(self, identifier):
        folder = self.root / identifier
        process = None
        data = self.get(identifier)
        try:
            data.update(state="resolving", message="Getting source audio", percent=None);self.save(data)
            env = os.environ.copy();env.pop("STUDIO_DEVICE_TOKEN", None)
            with (folder / "worker.log").open("wb") as log:
                process = subprocess.Popen([*self.command, str(folder)], stdout=subprocess.PIPE, stderr=log,
                                           env=env, start_new_session=True, bufsize=0)
                selector = selectors.DefaultSelector();selector.register(process.stdout, selectors.EVENT_READ)
                started = time.monotonic();pending = b""
                try:
                    while True:
                        if identifier in self.cancelled or self.stop.is_set():
                            raise InterruptedError("Cancelled")
                        if time.monotonic() - started > self.timeout:
                            raise TimeoutError("Processing timed out")
                        ready = selector.select(.2)
                        if ready:
                            chunk = os.read(process.stdout.fileno(), 65536)
                            if not chunk:
                                break
                            pending += chunk
                            if len(pending) > 1024*1024:
                                raise ValueError("Worker output too large")
                            while b"\n" in pending:
                                line, pending = pending.split(b"\n", 1)
                                try: event = json.loads(line)
                                except (ValueError, UnicodeError): continue
                                if not isinstance(event, dict): continue
                                stage = STAGES.get(event.get("stage"))
                                if stage:
                                    data.update(state=stage, message=stage.capitalize(), percent=event.get("percent"))
                                    data["elapsed_seconds"] = int(time.monotonic()-started)
                                    self.save(data)
                        elif process.poll() is not None:
                            break
                finally:
                    selector.close()
                if process.wait(timeout=10) != 0:
                    raise RuntimeError("Source unavailable or processing failed; try another video")
            m = verify_transfer(folder / "prepared")
            data.update(state="ready", percent=100, message="Stems ready to download", title=m.get("title", ""),
                        stems=[{**e, "bytes": (folder/"prepared"/e["file"]).stat().st_size} for e in m["stems"]])
            data.update(effective_settings=m.get("settings"), fallback=m.get("fallback"),
                        source_measurements=m.get("source_measurements", {}), extended_stems=[])
            if (folder / "extended").exists():
                extended = verify_transfer(folder / "extended", extended=True)
                data["extended_stems"] = [{**e, "bytes": (folder/"extended"/e["file"]).stat().st_size} for e in extended["stems"]]
            # Keep only verified prepared sets after success.
            for name in ("masters",): shutil.rmtree(folder / name, ignore_errors=True)
            for path in folder.glob("source.*"): path.unlink()
            (folder / "input.wav").unlink(missing_ok=True)
            (folder / "input.audio").unlink(missing_ok=True)
        except InterruptedError:
            data.update(state="cancelled", percent=None, message="Cancelled")
        except Exception as exc:
            data.update(state="failed", percent=None, message=str(exc)[:160])
        finally:
            if process and process.poll() is None:
                try: os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError: pass
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    try: os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError: pass
                    process.wait()
            if process and process.stdout: process.stdout.close()
            with self.lock:
                # A cancellation that raced with final export wins.
                if identifier in self.cancelled:
                    data.update(state="cancelled", percent=None, message="Cancelled")
                self.save(data)
                self.cancelled.discard(identifier)
            if data["state"] != "ready":
                for p in folder.iterdir():
                    if p.name not in ("request.json", "worker.log"):
                        if p.is_dir(): shutil.rmtree(p, ignore_errors=True)
                        else: p.unlink(missing_ok=True)

    def close(self):
        self.stop.set()
        if self.thread: self.thread.join(timeout=8)


def create_app(root=None, token=None, manager=None, searcher=None, device_url=None):
    token = token or os.environ.get("STUDIO_DEVICE_TOKEN", "")
    if len(token) < 32:
        raise ValueError("Set STUDIO_DEVICE_TOKEN to at least 32 random characters")
    manager = manager or Jobs(root or os.environ.get("STUDIO_DATA_DIR", "studio-data"))
    searcher = searcher or media.search
    searches = deque();search_lock = threading.Lock()
    delivery = {"state": "idle", "files": [], "message": ""}
    delivery_lock = threading.Lock()
    def send_to_device(identifier, transfer_mode="wav"):
        from .device import upload_transfer, UploadError
        def report(event):
            with delivery_lock:
                delivery["message"] = "Sending " + event["role"].lower()
                if event["stage"] == "upload":
                    delivery.update(wav_bytes=event.get("bytes"), wire_bytes=event.get("wire_bytes", event.get("bytes")), transfer_mode=event.get("transfer_mode", "wav"))
                if event["stage"] == "uploaded":
                    delivery["files"].append({k: event[k] for k in ("lane", "role", "file")})
        try:
            options = {"transfer_mode": transfer_mode} if transfer_mode != "wav" else {}
            files = upload_transfer(manager.root/identifier/"prepared", device_url, progress=report, **options)
            with delivery_lock:
                delivery.update(state="done", files=files, message="Files saved on Studio. Assign tracks with the device file picker.")
        except Exception as exc:
            with delivery_lock:
                delivery.update(state="failed", message=str(exc)[:300])
                if isinstance(exc, UploadError): delivery["files"] = exc.uploaded
    @asynccontextmanager
    async def lifespan(app):
        yield
        manager.close()
    app = FastAPI(title="Pub Mix Studio", version="1", docs_url=None, redoc_url=None, openapi_url=None, lifespan=lifespan)
    if device_url:
        from urllib.parse import urlsplit
        from fastapi.middleware.cors import CORSMiddleware
        origin = urlsplit(device_url)
        if origin.scheme not in ("http", "https") or not origin.hostname or origin.username or origin.password or origin.path not in ("", "/") or origin.query or origin.fragment:
            raise ValueError("Device must be an http(s) origin")
        app.add_middleware(CORSMiddleware, allow_origins=[device_url.rstrip("/")],
                           allow_methods=["GET", "POST"], allow_headers=["Authorization", "Content-Type"])
    app.state.jobs = manager
    def authenticated(authorization: str = Header(default="")):
        if not hmac.compare_digest(authorization.encode(), ("Bearer " + token).encode()):
            raise HTTPException(401, "Device pairing required")
    auth = [Depends(authenticated)]
    @app.middleware("http")
    async def bounded_body(request: Request, call_next):
        from fastapi.responses import JSONResponse
        if request.method == "POST" and request.url.path not in ("/v1/uploads", "/v1/panel/sources"):
            try: length = int(request.headers.get("content-length", "-1"))
            except ValueError: length = -1
            if not 0 <= length <= 2048:
                return JSONResponse({"detail": "Request body too large or missing length"}, status_code=413)
        if request.method == "POST" and request.url.path not in ("/v1/uploads", "/v1/panel/sources"):
            if len(await request.body()) > 2048:
                return JSONResponse({"detail": "Request too large"}, status_code=413)
        result = await call_next(request)
        result.headers["Cache-Control"] = "no-store"
        result.headers["X-Content-Type-Options"] = "nosniff"
        result.headers["X-Frame-Options"] = "DENY"
        result.headers["Referrer-Policy"] = "no-referrer"
        return result
    @app.get("/", response_class=HTMLResponse)
    def home():
        return (Path(__file__).parent / "local.html").read_text()
    @app.post("/v1/uploads", dependencies=auth, status_code=202)
    async def local_upload(request: Request, title: str = Query(default="Uploaded song", max_length=120),
                           mode: Literal["four", "extended", "dynamic"] = "four",
                           model: str | None = Query(default=None, max_length=120),
                           stem_depth: int = Query(default=16, ge=4, le=16)):
        import tempfile
        suffix = Path(title).suffix.lower()
        if suffix not in (".mp3", ".wav"):
            raise HTTPException(400, "Choose an MP3 or WAV file")
        if shutil.disk_usage(manager.root).free < 4 * 1024**3:
            raise HTTPException(507, "Processing storage is low")
        # Opaque temporary filename: never use a browser-provided path on disk.
        fd, temporary = tempfile.mkstemp(prefix="incoming-", dir=manager.root)
        try:
            size = 0
            with os.fdopen(fd, "wb") as target:
                async for block in request.stream():
                    size += len(block)
                    if size > media.MAX_BYTES:
                        raise HTTPException(413, "Choose a file under 100 MiB")
                    target.write(block)
            if not size:
                raise HTTPException(400, "The file is empty")
            return manager.create(NewJob(video_id="local_audio", rights_confirmed=True,
                                         request_id=secrets.token_hex(16), mode=mode, model=model, stem_depth=stem_depth), source=temporary, title=title)
        finally:
            Path(temporary).unlink(missing_ok=True)
    @app.get("/v1/models", dependencies=auth)
    def model_profiles():
        return {"models": profiles(), "modes": ["four", "extended", "dynamic"], "max_channels": 16}
    @app.get("/v1/device", dependencies=auth)
    def device_status():
        with delivery_lock:
            return {"configured": bool(device_url), **delivery}
    @app.post("/v1/jobs/{identifier}/send", dependencies=auth, status_code=202)
    def send(identifier: str, transfer: Literal["wav", "deflate-blocks-v1"] = "wav"):
        if not device_url: raise HTTPException(409, "Start the companion with --device http://YOUR_STUDIO_IP")
        with delivery_lock, manager.lock:
            if delivery["state"] == "sending": raise HTTPException(409, "A transfer is already running")
            job = manager.get(identifier)
            if job["state"] != "ready": raise HTTPException(409, "Stems not ready")
            delivery.update(state="sending", id=identifier, files=[], message="Checking Studio connection", wav_bytes=0, wire_bytes=0, transfer_mode=transfer)
            # Refresh retention so cleanup cannot remove assets during this transfer.
            with manager.db() as db:
                db.execute("UPDATE jobs SET created=? WHERE id=?", (time.time(), identifier))
            threading.Thread(target=send_to_device, args=(identifier, transfer), daemon=True).start()
            return dict(delivery)
    @app.get("/healthz")
    def health(): return {"service": "pubmix-studio", "api_version": 1}
    @app.get("/v1/search", dependencies=auth)
    def search(q: str = Query(min_length=1, max_length=100)):
        with search_lock:
            now = time.monotonic()
            while searches and searches[0] < now-60: searches.popleft()
            if len(searches) >= 6: raise HTTPException(429, "Wait before searching again")
            searches.append(now)
        try: return {"results": searcher(q)}
        except ValueError as exc: raise HTTPException(400, str(exc))
        except Exception: raise HTTPException(502, "Search unavailable; try again later")
    @app.get("/v1/jobs", dependencies=auth)
    def recent():
        with manager.lock, manager.db() as db:
            rows = db.execute("SELECT data FROM jobs ORDER BY created DESC LIMIT 20").fetchall()
        return {"jobs": [json.loads(row[0]) for row in rows]}
    @app.post("/v1/jobs", dependencies=auth, status_code=202)
    def create(request: NewJob): return manager.create(request)
    @app.get("/v1/jobs/{identifier}", dependencies=auth)
    def status(identifier: str): return manager.get(identifier)
    @app.post("/v1/jobs/{identifier}/cancel", dependencies=auth)
    def cancel(identifier: str): return manager.cancel(identifier)
    @app.get("/v1/jobs/{identifier}/stems/{role}", dependencies=auth)
    def asset(identifier: str, role: str):
        job = manager.get(identifier)
        if job["state"] != "ready": raise HTTPException(409, "Stems not ready")
        entry = next((e for e in job["stems"] if e["role"].lower() == role), None)
        if not entry: raise HTTPException(404, "Unknown stem")
        return FileResponse(manager.root/identifier/"prepared"/entry["file"], media_type="audio/wav")
    @app.get("/v1/jobs/{identifier}/extended/{role}", dependencies=auth)
    def extended_asset(identifier: str, role: str):
        job = manager.get(identifier)
        if job["state"] != "ready": raise HTTPException(409, "Stems not ready")
        entry = next((e for e in job.get("extended_stems", []) if e["role"].lower() == role), None)
        if not entry: raise HTTPException(404, "Unknown extended stem")
        return FileResponse(manager.root/identifier/"extended"/entry["file"], media_type="audio/wav")
    from .panel import install_panel
    install_panel(app, manager, auth, send, device_status, search)
    return app
