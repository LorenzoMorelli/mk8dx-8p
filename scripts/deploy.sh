#!/bin/bash
# Install the built subsdk9 + main.npdm as an Eden mod (LayeredExeFS). Usage: scripts/deploy.sh [remove]
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
# EDEN_DIR = cartella di un Eden portabile (contiene eden.AppImage e user/)
EDEN="${EDEN_DIR:-}"
[ -n "$EDEN" ] && [ -x "$EDEN/eden.AppImage" ] || { echo "imposta EDEN_DIR alla cartella di un Eden portabile (con eden.AppImage e user/)" >&2; exit 1; }
MOD="$EDEN/user/load/0100152000022000/mk8dx-8p/exefs"
if [ "$1" = "remove" ]; then
    rm -rf "$EDEN/user/load/0100152000022000/mk8dx-8p"; echo "mod rimossa"; exit 0
fi
test -f "$HERE/exlaunch/deploy/subsdk9" -a -f "$HERE/exlaunch/deploy/main.npdm"
mkdir -p "$MOD"
cp "$HERE/exlaunch/deploy/subsdk9" "$HERE/exlaunch/deploy/main.npdm" "$MOD/"
ls -la "$MOD"
