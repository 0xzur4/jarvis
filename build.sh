#!/bin/bash
# build.sh - cross-compile Jarvis for Windows 64-bit using MinGW-w64.
# Whisper static libs live in tools/whisper/{include,lib} (persisted in workspace).
set -e
CXX=${CXX:-/opt/llvm-mingw/bin/x86_64-w64-mingw32-g++}
DIR="$(dirname "$0")"
SRC="$DIR/src"
OUT="$DIR/dist"
WHISPER="$DIR/tools/whisper"

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "ERROR: $CXX not found. Install llvm-mingw first." >&2
    exit 1
fi
if [ ! -f "$WHISPER/lib/libwhisper.a" ]; then
    echo "ERROR: $WHISPER/lib/libwhisper.a missing." >&2
    exit 1
fi

"$CXX" -std=c++17 -O2 -Wall -Wno-unknown-pragmas \
    -municode -mwindows -static-libgcc -static-libstdc++ \
    -I"$SRC" -I"$WHISPER/include" \
    "$SRC/main.cpp" \
    "$SRC/config.cpp" \
    "$SRC/pixel_face.cpp" \
    "$SRC/setup_dialog.cpp" \
    "$SRC/atria_client.cpp" \
    "$SRC/audio_in.cpp" \
    "$SRC/mic_enum.cpp" \
    "$SRC/voice_enum.cpp" \
    "$SRC/stt_whisper.cpp" \
    "$SRC/tts_sapi.cpp" \
    "$SRC/syscontrol.cpp" \
    "$SRC/brain.cpp" \
    -o "$OUT/jarvis.exe" \
    -L"$WHISPER/lib" -lwhisper -lggml-cpu -lggml-base -lggml \
    -lwinhttp -lwinmm -lole32 -lcrypt32 -lmsimg32 -lgdi32 -luuid

echo "BUILD OK: $OUT/jarvis.exe"
ls -la "$OUT/jarvis.exe"
file "$OUT/jarvis.exe"
