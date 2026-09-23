import argparse
import json
import sys
from .engine import Engine, EngineError, Settings
from .transfer import export_transfer, verify_transfer
from .device import upload_transfer, UploadError


def main():
    parser = argparse.ArgumentParser(description="STUDIO: VOCALS · MELODY · BASS · RHYTHM")
    parser.add_argument("input", nargs="?")
    parser.add_argument("--output", default="stems")
    parser.add_argument("--preset", choices=["fast", "balanced", "best"], default="balanced")
    parser.add_argument("--backend", choices=["demucs", "mlx", "audio-separator", "roformer", "spleeter"], default="demucs")
    parser.add_argument("--model")
    parser.add_argument("--mode", choices=["four", "extended", "dynamic"], default="four")
    parser.add_argument("--stem-depth", type=int, default=16)
    parser.add_argument("--presence-db", type=float, default=-45)
    parser.add_argument("--min-confidence", type=float, default=.5)
    parser.add_argument("--fallback", action="store_true", help="Fall back explicitly to four-stem Demucs on backend failure")
    parser.add_argument("--extended-transfer", action="store_true", help="Export v2 channel assets; not accepted by current device upload")
    parser.add_argument("--vocal-model", default=Settings.vocal_model)
    parser.add_argument("--vocal-backend", choices=["mlx", "audio-separator", "demucs"], default="mlx")
    parser.add_argument("--device", choices=["cpu", "mps", "cuda"], default="cpu")
    parser.add_argument("--shifts", type=int, default=1)
    parser.add_argument("--overlap", type=float, default=.25)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--consistency", choices=["melody", "energy", "none"], default="melody")
    parser.add_argument("--verify", metavar="FOLDER")
    parser.add_argument("--export", metavar="MASTERS", help="Export existing verified masters without running inference")
    parser.add_argument("--transfer-output", metavar="FOLDER", help="New directory for 44.1 kHz stereo PCM16 transfer assets")
    parser.add_argument("--set-id", type=int, help="Nonzero uint32 transfer set identity")
    parser.add_argument("--title", default="")
    parser.add_argument("--headroom-db", type=float, default=1.0)
    parser.add_argument("--upload", metavar="DEVICE", help="Upload prepared WAVs through the existing ESP32 HTTP endpoint")
    parser.add_argument("--prepared", metavar="FOLDER", help="Use an already exported transfer set (no inference)")
    args = parser.parse_args()
    modes = sum(bool(x) for x in (args.input, args.verify, args.export, args.prepared))
    if modes != 1:
        parser.error("choose exactly one of input, --verify, --export or --prepared")
    if args.extended_transfer and (not args.transfer_output or args.upload):
        parser.error("--extended-transfer requires --transfer-output and cannot use --upload")
    if args.export and not args.transfer_output:
        parser.error("--export requires --transfer-output")
    if args.transfer_output and (args.prepared or args.verify or args.set_id is None):
        parser.error("--transfer-output requires input/--export and --set-id")
    if args.upload and not (args.transfer_output or args.prepared):
        parser.error("--upload requires --transfer-output or --prepared")
    emit = lambda event: print(json.dumps(event), file=sys.stderr, flush=True)
    try:
        if args.verify:
            print(json.dumps(Engine.verify(args.verify), indent=2))
            return 0
        transfer = None
        if args.prepared:
            verify_transfer(args.prepared)
            result = transfer = args.prepared
        else:
            options = {key: getattr(args, key) for key in Settings.__dataclass_fields__}
            result = args.export or Engine(args.output).separate(args.input, Settings(**options), emit)
            if args.transfer_output:
                emit({"stage": "export"})
                transfer = export_transfer(result, args.transfer_output, set_id=args.set_id,
                                           title=args.title, headroom_db=args.headroom_db, extended=args.extended_transfer)
        response = {"output": str(result), "stems": ([e["role"] for e in verify_transfer(result)["stems"]] if args.prepared else [e["name"] for e in Engine.verify(result)["stems"]])}
        if transfer:
            response["transfer"] = str(transfer)
        if args.upload:
            response["uploaded"] = upload_transfer(transfer, args.upload, progress=emit)
            response["next"] = "Assign these files to tracks 1–4 in the existing file picker, then save the project."
        print(json.dumps(response))
        return 0
    except UploadError as exc:
        print(json.dumps({"error": str(exc), "uploaded": exc.uploaded}), file=sys.stderr)
        return 1
    except (EngineError, ValueError, OSError) as exc:
        print(json.dumps({"error": str(exc)}), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print(json.dumps({"error": "Cancelled; local exports publish atomically. Any completed device uploads remain on SD; inspect before retrying"}), file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
