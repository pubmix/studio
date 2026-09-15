import argparse
import json
import sys
from .engine import Engine, EngineError, Settings


def main():
    parser = argparse.ArgumentParser(description="STUDIO: VOCALS · MELODY · BASS · RHYTHM")
    parser.add_argument("input", nargs="?")
    parser.add_argument("--output", default="stems")
    parser.add_argument("--preset", choices=["fast", "balanced", "best"], default="balanced")
    parser.add_argument("--backend", choices=["demucs", "mlx", "audio-separator", "spleeter"], default="demucs")
    parser.add_argument("--model")
    parser.add_argument("--vocal-model", default=Settings.vocal_model)
    parser.add_argument("--vocal-backend", choices=["mlx", "audio-separator", "demucs"], default="mlx")
    parser.add_argument("--device", choices=["cpu", "mps", "cuda"], default="cpu")
    parser.add_argument("--shifts", type=int, default=1)
    parser.add_argument("--overlap", type=float, default=.25)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--consistency", choices=["melody", "energy", "none"], default="melody")
    parser.add_argument("--verify", metavar="FOLDER")
    args = parser.parse_args()
    try:
        if args.verify:
            print(json.dumps(Engine.verify(args.verify), indent=2))
            return 0
        if not args.input:
            parser.error("input is required unless --verify is used")
        options = vars(args).copy()
        for key in ("input", "output", "verify"):
            options.pop(key)
        result = Engine(args.output).separate(args.input, Settings(**options),
                    lambda event: print(json.dumps(event), file=sys.stderr, flush=True))
        print(json.dumps({"output": str(result), "stems": ["VOCALS", "MELODY", "BASS", "RHYTHM"]}))
        return 0
    except (EngineError, ValueError, OSError) as exc:
        print(json.dumps({"error": str(exc)}), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print(json.dumps({"error": "Cancelled; no partial stem set published"}), file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
