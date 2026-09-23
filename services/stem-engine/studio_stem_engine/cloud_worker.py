"""One isolated job process. Parent owns cancellation, timeout and publication."""
import json
from pathlib import Path
import sys
from . import Engine, Settings
from .media import acquire
from .transfer import export_transfer


def main():
    folder = Path(sys.argv[1]).resolve()
    request = json.loads((folder / "request.json").read_text())
    def emit(event):
        print(json.dumps(event), flush=True)
    if request.get("local_file"):
        source, title = folder / "input.audio", request["title"]
        # Decode duration before allocating model buffers; malformed files fail privately.
        from .media import decode
        source = decode(source, folder / "input.wav", Path(title).suffix.lower()[1:])
    else:
        source, title = acquire(request["video_id"], folder, emit)
    masters = Engine(folder / "masters").separate(source, Settings(**request.get("settings", {"preset": "fast"})), emit)
    emit({"stage": "exporting"})
    export_transfer(masters, folder / "prepared", set_id=request["set_id"], title=title)
    if Engine.verify(masters)["schema"] == "studio.stems.v2":
        export_transfer(masters, folder / "extended", set_id=request["set_id"], title=title, extended=True)
    emit({"stage": "prepared", "title": title})


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        # Keep provider/server internals in the private worker log.
        print(str(exc), file=sys.stderr, flush=True)
        sys.exit(1)
