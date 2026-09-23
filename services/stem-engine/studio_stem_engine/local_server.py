"""Portable API runner. Defaults to loopback; explicit LAN binding is optional."""
import argparse
import fcntl
import os
from pathlib import Path
import secrets
import threading
import webbrowser
import uvicorn


def main():
    parser = argparse.ArgumentParser(description="Run Pub Mix Studio's companion server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8766)
    parser.add_argument("--data", type=Path, default=Path.home()/"Library/Application Support/Pub Mix Studio")
    parser.add_argument("--no-browser", action="store_true")
    parser.add_argument("--device", help="Studio WiFi origin, e.g. http://dubbox.local")
    args = parser.parse_args()
    args.data.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(args.data, 0o700)
    with (args.data/"server.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print("Opening the existing Studio companion.", flush=True)
            if not args.no_browser:
                token = (args.data/"device-token").read_text().strip()
                webbrowser.open(f"http://127.0.0.1:{args.port}#"+token)
            return
        secret_file = args.data/"device-token"
        if not secret_file.exists():
            fd = os.open(secret_file, os.O_WRONLY|os.O_CREAT|os.O_EXCL, 0o600)
            with os.fdopen(fd, "w") as stream:
                stream.write(secrets.token_hex(32))
        os.chmod(secret_file, 0o600)
        token = secret_file.read_text().strip()
        from .cloud import create_app
        app = create_app(root=args.data/"jobs", token=token, device_url=args.device)
        url = f"http://127.0.0.1:{args.port}"
        print(f"Pub Mix Studio is available at {url}. Keep this window open.", flush=True)
        if args.host != "127.0.0.1":
            print("LAN access enabled: use only a trusted private network. Use HTTPS for cloud hosting.", flush=True)
        if not args.no_browser:
            # Fragment stays in the browser; it is never sent in HTTP URLs or server logs.
            threading.Timer(1, lambda: webbrowser.open(url+"#"+token)).start()
        uvicorn.run(app, host=args.host, port=args.port, access_log=False)


if __name__ == "__main__":
    main()
