#!/usr/bin/env bash
# Build every ONE KNOB plug-in (macOS / Linux) and stage the results:
#
#   dist/stage/AtiNAMatiC-<Name>/VST3/AtiNAMatiC-<Name>.vst3
#                               /CLAP/AtiNAMatiC-<Name>.clap
#                               /LV2/AtiNAMatiC-<Name>.lv2
#                               /AU/AtiNAMatiC-<Name>.component   (macOS only)
#                               /AtiNAMatiC-<Name>-Manual.pdf
#
# Extra arguments are passed to CMake, e.g. on macOS:
#   packaging/build-all.sh -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/dist/stage"
DEPS="$ROOT/dist/deps"
if command -v nproc >/dev/null; then JOBS=$(nproc); else JOBS=$(sysctl -n hw.logicalcpu); fi
mkdir -p "$STAGE" "$DEPS"

# JUCE and clap-juce-extensions are fetched once and shared by all builds.
if [ ! -f "$DEPS/JUCE/CMakeLists.txt" ]; then
  git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git "$DEPS/JUCE"
fi
if [ ! -f "$DEPS/clap-juce-extensions/CMakeLists.txt" ]; then
  git clone https://github.com/free-audio/clap-juce-extensions.git "$DEPS/clap-juce-extensions"
  git -C "$DEPS/clap-juce-extensions" checkout 7adee3a1bd4684d4caa5601100e364abccff4b4d
  git -C "$DEPS/clap-juce-extensions" submodule update --init --recursive
fi

PLUGINS=$(grep -v '^#' "$ROOT/packaging/plugins.txt" | cut -d'|' -f1)

for N in $PLUGINS; do
  P="AtiNAMatiC-$N"
  echo "::group::Build $P"
  cmake -S "$ROOT/$P" -B "$ROOT/$P/build-release" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DJUCE_PATH="$DEPS/JUCE" \
    -DFETCHCONTENT_SOURCE_DIR_CLAP_JUCE_EXTENSIONS="$DEPS/clap-juce-extensions" \
    "$@"
  cmake --build "$ROOT/$P/build-release" --parallel "$JOBS"
  echo "::endgroup::"

  A="$ROOT/$P/build-release/AtiNAMatiC${N}_artefacts/Release"
  D="$STAGE/$P"
  rm -rf "$D"; mkdir -p "$D/VST3" "$D/CLAP" "$D/LV2"
  cp -R "$A/VST3/$P.vst3" "$D/VST3/"
  cp -R "$A/CLAP/$P.clap" "$D/CLAP/"
  cp -R "$A/LV2/$P.lv2"   "$D/LV2/"
  if [ "$(uname)" = "Darwin" ]; then
    mkdir -p "$D/AU"
    cp -R "$A/AU/$P.component" "$D/AU/"
  fi
  cp "$ROOT/$P/docs/$P-Manual.pdf" "$D/"
done

echo "Staged $(echo "$PLUGINS" | wc -w | tr -d ' ') plug-ins in $STAGE"
