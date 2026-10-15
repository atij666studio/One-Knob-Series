#!/usr/bin/env bash
# build-linux.sh — Linux build script for AtiNAMatiC-Reach
# Usage:  ./build-linux.sh              (Release)
#         ./build-linux.sh Debug
#
# Prerequisites (Ubuntu/Debian):
#   sudo apt-get install -y \
#       build-essential cmake ninja-build git \
#       libasound2-dev libfreetype-dev \
#       libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
#       libgl1-mesa-dev libglu1-mesa-dev \
#       libwebkit2gtk-4.0-dev libgtk-3-dev \
#       pkg-config
#
# Install paths (after build):
#   VST3: /usr/lib/vst3/                 (system)  or ~/.vst3/  (per-user)
#   CLAP: /usr/lib/clap/                 (system)  or ~/.clap/  (per-user)
#   Standalone: wherever you copy it

set -euo pipefail

CONFIG="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="build-linux"
CPUS="$(nproc)"

echo ""
echo "==> AtiNAMatiC-Reach Linux build ($CONFIG)"
echo ""

# --------------------------------------------------------------------------
# CMake configure
# --------------------------------------------------------------------------
echo "Configuring..."
cmake -S . -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$CONFIG"

if [ $? -ne 0 ]; then
    echo "CMake configure failed." >&2
    exit 1
fi

# --------------------------------------------------------------------------
# Build
# --------------------------------------------------------------------------
echo ""
echo "Building with $CPUS CPUs..."
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$CPUS"

if [ $? -ne 0 ]; then
    echo "Build failed." >&2
    exit 1
fi

# --------------------------------------------------------------------------
# Report
# --------------------------------------------------------------------------
ARTS="$BUILD_DIR/AtiNAMatiCReach_artefacts/$CONFIG"
echo ""
echo "Build OK."
echo ""

[ -f "$ARTS/Standalone/AtiNAMatiC-Reach" ] && \
    echo "  Standalone : $ARTS/Standalone/AtiNAMatiC-Reach"
[ -d "$ARTS/VST3/AtiNAMatiC-Reach.vst3" ] && \
    echo "  VST3       : $ARTS/VST3/AtiNAMatiC-Reach.vst3"
[ -f "$ARTS/CLAP/AtiNAMatiC-Reach.clap" ] && \
    echo "  CLAP       : $ARTS/CLAP/AtiNAMatiC-Reach.clap"

echo ""
echo "To install system-wide (requires sudo):"
echo "  VST3 : sudo cp -r \"$ARTS/VST3/AtiNAMatiC-Reach.vst3\" /usr/lib/vst3/"
echo "  CLAP : sudo cp  \"$ARTS/CLAP/AtiNAMatiC-Reach.clap\"  /usr/lib/clap/"
echo ""
echo "To install per-user (no sudo):"
echo "  VST3 : mkdir -p ~/.vst3 && cp -r \"$ARTS/VST3/AtiNAMatiC-Reach.vst3\" ~/.vst3/"
echo "  CLAP : mkdir -p ~/.clap && cp  \"$ARTS/CLAP/AtiNAMatiC-Reach.clap\"  ~/.clap/"
