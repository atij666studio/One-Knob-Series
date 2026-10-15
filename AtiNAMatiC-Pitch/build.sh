#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

CONFIG="${CONFIG:-Release}"
BUILD_DIR="build"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" -G "Unix Makefiles"
cmake --build "$BUILD_DIR" --config "$CONFIG" -j

echo
echo "Build OK."
echo "Standalone: $BUILD_DIR/AtiNAMatiCPitch_artefacts/$CONFIG/Standalone/AtiNAMatiC-Pitch.app"
