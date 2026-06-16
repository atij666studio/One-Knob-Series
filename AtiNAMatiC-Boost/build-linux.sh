#!/usr/bin/env bash
# build-linux.sh — AtiNAMatiC-Boost Linux build + install script
#
# Dependencies (Debian/Ubuntu):
#   sudo apt-get install -y build-essential cmake git ninja-build \
#     libfreetype6-dev libx11-dev libxext-dev libxrandr-dev \
#     libxcomposite-dev libxcursor-dev libxinerama-dev \
#     libgl1-mesa-dev libglu1-mesa-dev libasound2-dev \
#     libwebkit2gtk-4.0-dev libcurl4-openssl-dev pkg-config
#
# Usage:
#   chmod +x build-linux.sh
#   ./build-linux.sh            (Release)
#   ./build-linux.sh Debug      (Debug)

set -euo pipefail

CONFIG="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-linux"

echo ""
echo "=== AtiNAMatiC-Boost Linux Build ($CONFIG) ==="
echo ""

# ------------------------------------------------------------------
# Configure
# ------------------------------------------------------------------
echo "Configuring..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE="$CONFIG" \
      -G Ninja

# ------------------------------------------------------------------
# Build
# ------------------------------------------------------------------
echo "Building..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

ARTEFACTS="$BUILD_DIR/AtiNAMatiCBoost_artefacts/$CONFIG"

# ------------------------------------------------------------------
# Install VST3 — user folder (~/.vst3) so no sudo needed
# ------------------------------------------------------------------
VST3_SRC="$ARTEFACTS/VST3/AtiNAMatiC-Boost.vst3"
VST3_DST="$HOME/.vst3/AtiNAMatiC-Boost.vst3"

if [ -d "$VST3_SRC" ]; then
    mkdir -p "$(dirname "$VST3_DST")"
    rm -rf "$VST3_DST"
    cp -r "$VST3_SRC" "$VST3_DST"
    echo "VST3 installed to $VST3_DST"
fi

# ------------------------------------------------------------------
# Install CLAP — user folder (~/.clap) so no sudo needed
# ------------------------------------------------------------------
CLAP_SRC="$ARTEFACTS/CLAP/AtiNAMatiC-Boost.clap"
CLAP_DST="$HOME/.clap/AtiNAMatiC-Boost.clap"

if [ -f "$CLAP_SRC" ]; then
    mkdir -p "$(dirname "$CLAP_DST")"
    cp -f "$CLAP_SRC" "$CLAP_DST"
    echo "CLAP installed to $CLAP_DST"
fi

# ------------------------------------------------------------------
# Done
# ------------------------------------------------------------------
echo ""
echo "Build OK."
echo "Standalone : $ARTEFACTS/Standalone/AtiNAMatiC-Boost"
echo "VST3       : $VST3_DST"
echo "CLAP       : $CLAP_DST"
