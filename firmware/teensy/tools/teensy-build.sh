#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CLI=${ARDUINO_CLI:-arduino-cli}
# Use a user-installed Teensy package. The delivered binaries were compiled with 1.62.0.
# Set ARDUINO_DIRECTORIES_DATA/USER/DOWNLOADS to use an isolated installation.
if [ "${1:-}" = "io-example" ]; then
 "$CLI" compile --fqbn teensy:avr:teensy41 --build-property 'build.flags.defs=-D__IMXRT1062__ -DTEENSYDUINO=160 -DSTUDIO_ENABLE_I2S=1 -DSTUDIO_ENABLE_SD=1' --output-dir "$ROOT/build/teensy-io-example" "$ROOT/firmware/STUDIO"
else
 "$CLI" compile --fqbn teensy:avr:teensy41 --output-dir "$ROOT/build/teensy" "$ROOT/firmware/STUDIO"
fi
