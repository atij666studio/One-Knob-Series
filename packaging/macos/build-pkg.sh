#!/usr/bin/env bash
# Build the combined macOS installer (ONE-KNOB-Series-v<ver>-macOS-Installer.pkg).
#
# It opens on the "Customize" screen: every plug-in is a ticked checkbox, with
# its formats (VST3 / AU / CLAP / LV2) as ticked checkboxes underneath, so users
# can skip whole plug-ins or single formats.
#
# Usage (after packaging/build-all.sh on macOS):
#   packaging/macos/build-pkg.sh 1.0.0 dist/stage dist/release
set -euo pipefail

VERSION="$1"; STAGE="$(cd "$2" && pwd)"; OUT="$3"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="$(mktemp -d)"
mkdir -p "$OUT" "$WORK/pkgs" "$WORK/res"

xml() { sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' -e 's/"/\&quot;/g' <<<"$1"; }

# format id | stage subfolder | bundle extension | install location | label
FORMATS="vst3|VST3|vst3|/Library/Audio/Plug-Ins/VST3|VST3
au|AU|component|/Library/Audio/Plug-Ins/Components|Audio Unit (AU)
clap|CLAP|clap|/Library/Audio/Plug-Ins/CLAP|CLAP
lv2|LV2|lv2|/Library/Audio/Plug-Ins/LV2|LV2"

OUTLINE=""; CHOICES=""; REFS=""
while IFS='|' read -r NAME DESC; do
  [[ -z "$NAME" || "$NAME" == \#* ]] && continue
  P="AtiNAMatiC-$NAME"; id=$(tr '[:upper:]' '[:lower:]' <<<"$NAME")
  OUTLINE+="    <line choice=\"$id\">"$'\n'
  CHOICES+="  <choice id=\"$id\" title=\"$P\" description=\"$(xml "$DESC")\"/>"$'\n'
  while IFS='|' read -r fid sub ext loc label; do
    src="$STAGE/$P/$sub/$P.$ext"
    [ -e "$src" ] || { echo "missing $src" >&2; exit 1; }
    root="$WORK/root-$id-$fid"; mkdir -p "$root"; cp -R "$src" "$root/"
    pkgid="com.atij.oneknob.$id.$fid"
    # Keep bundles where we put them (never "relocate" to an older copy elsewhere).
    pkgbuild --analyze --root "$root" "$WORK/$id-$fid.plist" >/dev/null </dev/null
    n=$(/usr/libexec/PlistBuddy -c "Print" "$WORK/$id-$fid.plist" 2>/dev/null | grep -c "RootRelativeBundlePath" || true)
    for ((i = 0; i < n; i++)); do
      # The key isn't always present in the analyzed plist: set it, or add it.
      /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$id-$fid.plist" 2>/dev/null ||
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$WORK/$id-$fid.plist"
    done
    pkgbuild --root "$root" --component-plist "$WORK/$id-$fid.plist" \
      --install-location "$loc" --identifier "$pkgid" --version "$VERSION" \
      "$WORK/pkgs/$id-$fid.pkg" >/dev/null </dev/null
    OUTLINE+="      <line choice=\"$id.$fid\"/>"$'\n'
    CHOICES+="  <choice id=\"$id.$fid\" title=\"$label\"><pkg-ref id=\"$pkgid\"/></choice>"$'\n'
    REFS+="  <pkg-ref id=\"$pkgid\" version=\"$VERSION\" onConclusion=\"none\">$id-$fid.pkg</pkg-ref>"$'\n'
  done <<<"$FORMATS"
  OUTLINE+="    </line>"$'\n'
done < "$ROOT/packaging/plugins.txt"

# Manuals: installed to /Library/Documentation/ATIJ STUDIO/ONE KNOB Series
mkdir -p "$WORK/root-docs/ONE KNOB Series"
cp "$ROOT/docs/ONE-KNOB-Series-Manual.pdf" "$WORK/root-docs/ONE KNOB Series/"
cp "$STAGE"/*/*-Manual.pdf "$WORK/root-docs/ONE KNOB Series/"
pkgbuild --root "$WORK/root-docs" --install-location "/Library/Documentation/ATIJ STUDIO" \
  --identifier com.atij.oneknob.manuals --version "$VERSION" "$WORK/pkgs/manuals.pkg" >/dev/null
OUTLINE+="    <line choice=\"manuals\"/>"$'\n'
CHOICES+="  <choice id=\"manuals\" title=\"User manuals (PDF)\" description=\"Installed to /Library/Documentation/ATIJ STUDIO/ONE KNOB Series\"><pkg-ref id=\"com.atij.oneknob.manuals\"/></choice>"$'\n'
REFS+="  <pkg-ref id=\"com.atij.oneknob.manuals\" version=\"$VERSION\" onConclusion=\"none\">manuals.pkg</pkg-ref>"$'\n'

cat > "$WORK/res/welcome.html" <<EOF
<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2>ONE KNOB Series $VERSION</h2>
<p>23 one-knob plug-ins by ATIJ STUDIO, free and open source.</p>
<p>On the next screens, <b>tick the plug-ins and formats you want</b> and untick the rest.
Everything is ticked by default.</p>
<p>After installing, rescan plug-ins in your DAW.</p>
</body></html>
EOF
cp "$ROOT/LICENSE" "$WORK/res/LICENSE.txt"

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>ONE KNOB Series $VERSION</title>
  <organization>com.atij</organization>
  <welcome file="welcome.html" mime-type="text/html"/>
  <license file="LICENSE.txt"/>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
$OUTLINE  </choices-outline>
$CHOICES$REFS</installer-gui-script>
EOF

productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" \
  --resources "$WORK/res" "$OUT/ONE-KNOB-Series-v$VERSION-macOS-Installer.pkg"
echo "Built $OUT/ONE-KNOB-Series-v$VERSION-macOS-Installer.pkg"
rm -rf "$WORK"
