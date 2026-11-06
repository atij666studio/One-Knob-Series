#!/usr/bin/env bash
# ONE KNOB Series -- Linux installer
#
#   ./install.sh            ask which plug-ins and formats to install (for you only)
#   ./install.sh --system   install for all users (/usr/lib/...; asks for sudo)
#   ./install.sh --all      install everything without asking
#   ./install.sh --uninstall  remove ONE KNOB plug-ins installed by this script
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
SCOPE=user; ALL=no; UNINSTALL=no
for a in "$@"; do
  case "$a" in
    --system) SCOPE=system ;;
    --all) ALL=yes ;;
    --uninstall) UNINSTALL=yes ;;
    -h|--help) sed -n '2,8p' "$0"; exit 0 ;;
    *) echo "Unknown option: $a"; exit 1 ;;
  esac
done

if [ "$SCOPE" = system ]; then
  VST3=/usr/lib/vst3; CLAP=/usr/lib/clap; LV2=/usr/lib/lv2; SUDO=sudo
else
  VST3="$HOME/.vst3"; CLAP="$HOME/.clap"; LV2="$HOME/.lv2"; SUDO=
fi

ask() {   # ask "Question" default(y/n)  -> returns 0 for yes
  local def="$2" ans
  [ "$ALL" = yes ] && return 0
  read -r -p "$1 [$( [ "$def" = y ] && echo Y/n || echo y/N )] " ans || true
  ans="${ans:-$def}"
  [[ "$ans" =~ ^[Yy] ]]
}

mapfile -t PLUGINS < <(ls -1 "$DIR/VST3" | sed 's/\.vst3$//')

if [ "$UNINSTALL" = yes ]; then
  for p in "${PLUGINS[@]}"; do
    $SUDO rm -rf "$VST3/$p.vst3" "$CLAP/$p.clap" "$LV2/$p.lv2"
  done
  echo "Removed ONE KNOB plug-ins from $VST3, $CLAP and $LV2."
  exit 0
fi

echo "ONE KNOB Series -- ${#PLUGINS[@]} plug-ins"
echo "Installing for: $( [ "$SCOPE" = system ] && echo 'all users' || echo 'you only') (use --system for all users)"
echo

CHOSEN=()
if ask "Install all ${#PLUGINS[@]} plug-ins?" y; then
  CHOSEN=("${PLUGINS[@]}")
else
  for p in "${PLUGINS[@]}"; do
    ask "  Install $p?" y && CHOSEN+=("$p")
  done
fi
[ "${#CHOSEN[@]}" -eq 0 ] && { echo "Nothing selected."; exit 0; }

echo
DO_VST3=no; DO_CLAP=no; DO_LV2=no
ask "Install VST3 versions? (most DAWs)" y && DO_VST3=yes
ask "Install CLAP versions? (Bitwig, Reaper, ...)" y && DO_CLAP=yes
ask "Install LV2 versions? (Ardour, Carla, ...)" y && DO_LV2=yes

echo
for p in "${CHOSEN[@]}"; do
  if [ "$DO_VST3" = yes ]; then $SUDO mkdir -p "$VST3"; $SUDO rm -rf "$VST3/$p.vst3"; $SUDO cp -r "$DIR/VST3/$p.vst3" "$VST3/"; fi
  if [ "$DO_CLAP" = yes ]; then $SUDO mkdir -p "$CLAP"; $SUDO cp "$DIR/CLAP/$p.clap" "$CLAP/"; fi
  if [ "$DO_LV2"  = yes ]; then $SUDO mkdir -p "$LV2";  $SUDO rm -rf "$LV2/$p.lv2";  $SUDO cp -r "$DIR/LV2/$p.lv2" "$LV2/"; fi
  echo "  installed $p"
done
echo
echo "Done. Rescan plug-ins in your DAW. Manuals are in the Manuals folder next to this script."
