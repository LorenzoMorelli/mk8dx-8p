#!/bin/bash
# Launch Eden (portable) with MK8D and tail its log. Usage: scripts/run-eden.sh
HERE="$(cd "$(dirname "$0")/.." && pwd)"
# EDEN_DIR = cartella di un Eden portabile (contiene eden.AppImage e user/)
EDEN="${EDEN_DIR:-}"
[ -n "$EDEN" ] && [ -x "$EDEN/eden.AppImage" ] || { echo "imposta EDEN_DIR alla cartella di un Eden portabile (con eden.AppImage e user/)" >&2; exit 1; }
# MK8D_GAME = file del gioco base (.xci/.nsp); di default il primo "Mario Kart 8 Deluxe*.xci" in $EDEN/../games
GAME="${MK8D_GAME:-$(ls "$EDEN/../games/Mario Kart 8 Deluxe"*.xci 2>/dev/null | head -1)}"
[ -f "$GAME" ] || { echo "gioco non trovato: imposta MK8D_GAME" >&2; exit 1; }
cd "$EDEN" || exit 1
mkdir -p user/log; : > user/log/eden_log.txt 2>/dev/null
# RenderDoc pronto di default: cattura di UN fotogramma con F12 (tasto di RenderDoc), file in /tmp/RenderDoc. NON usare il tasto
# di Eden "Toggle Renderdoc Capture" (renderdoc_hotkey): la cattura avvia/ferma su Eden copia 128 MB per fotogramma (6 nov: PC bloccato).
# serve per catturare il difetto grafico intermittente (STATE "Problemi aperti"). RENDERDOC=0 per avviare senza.
if [ "${RENDERDOC:-1}" = "1" ] && [ -f /etc/vulkan/implicit_layer.d/renderdoc_capture.json ]; then export ENABLE_VULKAN_RENDERDOC_CAPTURE=1; fi
./eden.AppImage -g "$GAME" >/dev/null 2>&1 &
echo "eden pid $!  log: $EDEN/user/log/eden_log.txt"
