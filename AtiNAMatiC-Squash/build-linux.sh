#!/usr/bin/env bash
# build-linux.sh -- AtiNAMatiC-Squash Linux build script
set -e
ROOT="$(cd "$(dirname "$0")"; pwd)"
BUILD="$ROOT/build"
CONFIG=Release

echo "[1/2] Configuring..."
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=$CONFIG

echo "[2/2] Building..."
cmake --build "$BUILD" --config $CONFIG --parallel

echo ""
echo "Build complete."
echo "VST3: $BUILD/AtiNAMatiCSquash_artefacts/$CONFIG/VST3/AtiNAMatiC-Squash.vst3"
echo "CLAP: $BUILD/AtiNAMatiCSquash_artefacts/$CONFIG/CLAP/AtiNAMatiC-Squash.clap"
echo ""
echo "To install VST3:"
echo "  mkdir -p ~/.vst3 && cp -r \"$BUILD/AtiNAMatiCSquash_artefacts/$CONFIG/VST3/AtiNAMatiC-Squash.vst3\" ~/.vst3/"
echo "To install CLAP:"
echo "  mkdir -p ~/.clap && cp \"$BUILD/AtiNAMatiCSquash_artefacts/$CONFIG/CLAP/AtiNAMatiC-Squash.clap\" ~/.clap/"
