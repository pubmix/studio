"""Bounded touchscreen views over the existing companion jobs and source staging."""
import json
import os
from pathlib import Path
import secrets
import shutil
import tempfile
import time
import unicodedata
from typing import Literal
from fastapi import HTTPException, Query, Request
from pydantic import BaseModel, Field, StrictBool, model_validator
from . import media


def label(value, limit=70):
    return " ".join(unicodedata.normalize("NFKD", str(value)).encode("ascii", "ignore").decode().split())[:limit]


class PanelJob(BaseModel):
    video_id: str | None = Field(default=None, pattern=r"^[A-Za-z0-9_-]{11}$")
    source_id: str | None = Field(default=None, pattern=r"^[a-f0-9]{24}$")
    mode: Literal["four", "extended", "dynamic"] = "four"
    rights_confirmed: StrictBool
    request_id: str = Field(min_length=16, max_length=64, pattern=r"^[A-Za-z0-9_-]+$")

    @model_validator(mode="after")
    def one_source(self):
        if (self.video_id is None) == (self.source_id is None):
            raise ValueError("Choose exactly one source")
        return self


def install_panel(app, manager, auth, send, device_status, search):
    from .cloud import NewJob
    sources = manager.root / "panel-sources"
    sources.mkdir(exist_ok=True, mode=0o700)

    def staged():
        items = []
        for path in sources.glob("*.json"):
            data = json.loads(path.read_text())
            if time.time() - data["created"] > manager.retention:
                (sources / (path.stem + ".audio")).unlink(missing_ok=True)
                path.unlink()
            else:
                items.append(data)
        return sorted(items, key=lambda item: item["created"], reverse=True)

    def job_view(identifier):
        job = manager.get(identifier)
        delivery = device_status()
        return {"id": job["id"], "title": label(job.get("title") or job["project"]),
                "state": job["state"], "message": label(job["message"], 100),
                "percent": job.get("percent"), "source_count": len(job.get("extended_stems", [])) or len(job["stems"]),
                "fallback": bool(job.get("fallback")), "bytes": sum(stem.get("bytes", 0) for stem in job["stems"]),
                "delivery_state": delivery["state"] if delivery.get("id") == identifier else "idle",
                "delivery_message": label(delivery["message"], 100) if delivery.get("id") == identifier else "",
                "files": [item["file"] for item in delivery["files"]] if delivery.get("id") == identifier else [],
                "wav_bytes": delivery.get("wav_bytes", 0) if delivery.get("id") == identifier else 0,
                "wire_bytes": delivery.get("wire_bytes", 0) if delivery.get("id") == identifier else 0}

    @app.get("/v1/panel/library", dependencies=auth)
    def library(offset: int = Query(default=0, ge=0, le=100)):
        with manager.lock, manager.db() as db:
            items = [{"kind": "source", "id": item["id"], "title": label(item["title"]), "state": "choose split"} for item in staged()]
            for (raw,) in db.execute("SELECT data FROM jobs ORDER BY created DESC LIMIT 20").fetchall():
                job = json.loads(raw)
                items.append({"kind": "job", "id": job["id"], "title": label(job.get("title") or job["project"]), "state": job["state"]})
            return {"items": items[offset:offset+6], "more": len(items) > offset+6}

    @app.get("/v1/panel/search", dependencies=auth)
    def panel_search(q: str = Query(min_length=1, max_length=64)):
        result = search(q)
        return {"items": [{"kind": "video", "id": item["id"], "title": label(item["title"]), "state": str(int(item.get("duration", 0))) + " seconds"} for item in result["results"][:5]], "more": False}

    @app.post("/v1/panel/sources", dependencies=auth, status_code=201)
    async def stage_source(request: Request, title: str = Query(min_length=1, max_length=120)):
        if Path(title).suffix.lower() not in (".wav", ".mp3"):
            raise HTTPException(400, "Choose an MP3 or WAV")
        with manager.lock:
            if len(staged()) >= 32: raise HTTPException(409, "Source shelf is full; wait for the 24-hour expiry")
            if shutil.disk_usage(manager.root).free < 4 * 1024**3: raise HTTPException(507, "Processing storage is low")
        identifier = secrets.token_hex(12)
        fd, temporary = tempfile.mkstemp(prefix="staging-", dir=sources)
        try:
            size = 0
            with os.fdopen(fd, "wb") as target:
                async for block in request.stream():
                    size += len(block)
                    if size > media.MAX_BYTES: raise HTTPException(413, "Choose a file under 100 MiB")
                    target.write(block)
            if not size: raise HTTPException(400, "The file is empty")
            data = {"id": identifier, "title": title, "bytes": size, "created": time.time()}
            with manager.lock:
                if len(staged()) >= 32: raise HTTPException(409, "Source shelf is full")
                os.replace(temporary, sources / (identifier + ".audio"))
                (sources / (identifier + ".json")).write_text(json.dumps(data))
            return {"id": identifier, "title": title, "bytes": size}
        finally:
            Path(temporary).unlink(missing_ok=True)

    @app.post("/v1/panel/jobs", dependencies=auth, status_code=202)
    def panel_create(request: PanelJob):
        options = NewJob(video_id=request.video_id or "local_audio", mode=request.mode,
                         rights_confirmed=request.rights_confirmed, request_id=request.request_id)
        with manager.lock:
            if request.video_id:
                job = manager.create(options)
            else:
                item = next((item for item in staged() if item["id"] == request.source_id), None)
                if item is None: raise HTTPException(404, "Source expired; stage it again")
                fd, temporary = tempfile.mkstemp(prefix="panel-input-", dir=manager.root)
                os.close(fd)
                try:
                    shutil.copyfile(sources / (item["id"] + ".audio"), temporary)
                    job = manager.create(options, source=temporary, title=item["title"], source_key=item["id"])
                finally:
                    Path(temporary).unlink(missing_ok=True)
        return job_view(job["id"])

    @app.get("/v1/panel/jobs/{identifier}", dependencies=auth)
    def panel_status(identifier: str): return job_view(identifier)

    @app.post("/v1/panel/jobs/{identifier}/cancel", dependencies=auth)
    def panel_cancel(identifier: str):
        manager.cancel(identifier)
        return job_view(identifier)

    @app.post("/v1/panel/jobs/{identifier}/send", dependencies=auth, status_code=202)
    def panel_send(identifier: str, transfer: Literal["wav", "deflate-blocks-v1"] = "wav"):
        send(identifier, transfer)
        return job_view(identifier)
