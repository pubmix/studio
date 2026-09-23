#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -x "$ROOT/.venv/bin/python" ]; then
    printf '%s\n' 'Run services/stem-engine/scripts/setup-server first. See docs/NEW_COMPUTER.md.'
    exit 1
fi
exec "$ROOT/scripts/studio-server" --host 0.0.0.0 --device "${STUDIO_DEVICE_URL:-http://dubbox.local}" "$@"
