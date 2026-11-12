#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
exe=$(mktemp "${TMPDIR:-/tmp}/dubbox-audio.XXXXXX")
trap 'rm -f "$exe"' EXIT
if [ "$(uname)" = Darwin ]; then
  export CPLUS_INCLUDE_PATH="$(xcrun --show-sdk-path)/usr/include/c++/v1${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
fi
"${CXX:-c++}" -std=c++17 -fsanitize=address,undefined -g -Itests/stubs -Isrc tests/audio_memory.cpp src/audio_play_sd_wav_seekable.cpp -o "$exe"
"$exe"
