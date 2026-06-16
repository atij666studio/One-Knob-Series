#!/usr/bin/env bash
# build-linux.sh -- AtiNAMatiC-Tight Linux build script
# Usage: ./build.sh   (or CONFIG=Debug ./build.sh)
set -euo pipefail
cd "$(dirname "$0")"

CONFIG="${CONFIG:-Release}"
BUILD_DIR="build"
TARGET="AtiNAMatiCTight"

echo "[1/2] Configuring..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" -G "Ninja"
if [ $? -ne 0 ]; then echo "CMake configure failed." ; exit 1 ; fi

echo "[2/2] Building $CONFIG..."
cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$TARGET" -j
if [ $? -ne 0 ]; then echo "CMake build failed." ; exit 1 ; fi

echo ""
echo "Build OK."
echo "VST3:       $BUILD_DIR/${TARGET}_artefacts/$CONFIG/VST3/AtiNAMatiC-Tight.vst3"
echo "Standalone: $BUILD_DIR/${TARGET}_artefacts/$CONFIG/Standalone/AtiNAMatiC-Tight"
echo "CLAP:       $BUILD_DIR/${TARGET}_artefacts/$CONFIG/CLAP/AtiNAMatiC-Tight.clap"
echo ""
echo "To install (run as yourself, not root):"
echo "  cp -r $BUILD_DIR/${TARGET}_artefacts/$CONFIG/VST3/AtiNAMatiC-Tight.vst3 ~/.vst3/"
echo "  cp    $BUILD_DIR/${TARGET}_artefacts/$CONFIG/CLAP/AtiNAMatiC-Tight.clap ~/.clap/"
