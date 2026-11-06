#!/bin/bash
# Build subsdk9 + main.npdm with exlaunch inside the devkitpro/devkita64 Docker image.
# Uso: scripts/build.sh [clean]          (build distribuibile: log spenti, diagnostica crash accesa)
#      MK8DX_LOG=1 scripts/build.sh       (build per Eden e build "diagnostica": log accesi)
#      VAR=valore ... scripts/build.sh     (solo per prove; i default sono quelli della mod "normale")
# Passi: copia il submodule exlaunch/ (che resta intatto) in build/exlaunch/, ci sovrappone src/program/, config.mk e config.json, applica
# le patch src/exlaunch_patch/*.patch (stub NZCV, pool trampolini, svcGetInfo per SaltyNX), poi make nel container Docker $DKP_IMAGE.
# Uscita: build/exlaunch/deploy/subsdk9 + main.npdm, log completo in build.log. Installazione in Eden: scripts/deploy.sh; avvio: scripts/run-eden.sh.
#
# Variabili (facoltative, passate a gcc come -D<nome>=<valore>):
#   MK8DX_VIEWS        8      viste massime in gara (6 o 8)
#   MK8DX_P7_MENU3D8   1      anteprime 3D a 8 nella selezione (8 KartUnit, renderer del menu a 8 viste); 0 = menu a 4 viste
#   MK8DX_P7_REC16     =MENU3D8  liste di modelli del caricatore a 16 record e anello Body/Wing a 9 voci
#   MK8DX_P7_FLOW8     1      flusso originale con N = 8 nel menu (0 solo per bisezioni)
#   MK8DX_SEL8B        1      selezione a 8 sulla pagina 8pl diretta (step 5c, opzione B)
#   MK8DX_TEXDESC      16640  descrittori texture del device (correzione dei flash in gara a 8); 0 = valore del gioco (8492)
#   MK8DX_SPECTOFF     0      prova: cella vuota 5P/7P senza camera spettatore (ripiego sul kart 0)
#   MK8DX_GIFT8        1      pagina Present a 8 regali (step 7f); 0 = originale a 4 (bisezioni)
#   MK8DX_EXCDIAG      1      diagnostica crash su console: contesto del guasto nei registri del report di Atmosphère (hook nnSdk+0x90f38)
#   MK8DX_LOG          0      righe di log '[mk8dx-8p]' e misure solo-log; 0 = build distribuibile. PER EDEN SERVE MK8DX_LOG=1 (controlli del log)
#   MK8DX_MODELDIAG    0      prova console: svcBreak se ModelCreateHook trova un modello non pronto (DEAD0001/2)
#   MK8DX_NO7CFIX      0      prova: maschera 1 = senza flag effetti 0x1088c8, 2 = senza sfera camera 0xcdd18 (step 7c)
#   MK8DX_LYT8         0      vecchia prova layout della selezione (step 5 A1): 6|8
#   MK8DX_P3TEST       0      vecchia prova del passo 3 dello step 5b
#   MK8DX_P7_EXP8INIT  0      vecchia prova dello step 7 B
#   DKP_IMAGE          devkitpro/devkita64:latest  immagine Docker usata per la build
#   DKP_NATIVE         0      1 = compila direttamente (gia' dentro devkitA64, es. CI), senza Docker
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SUB="$HERE/exlaunch"
# cartella di build: deve chiamarsi "exlaunch" (il Makefile prende il nome del target dalla cartella)
EXL="$HERE/build/exlaunch"
IMAGE="${DKP_IMAGE:-devkitpro/devkita64:latest}"
[ -f "$SUB/Makefile" ] || { echo "submodule exlaunch mancante: git submodule update --init" >&2; exit 1; }
# Il percorso assoluto della build finisce nel binario (__FILE__ dei messaggi di abort, info di debug -> GNU build-id): si compila sempre in
# /work/exlaunch. Docker: build/exlaunch montata li'. DKP_NATIVE=1 (CI, container come root): build/exlaunch e' un link a /work/exlaunch.
# Stesso commit e stessi flag -> stesso subsdk9 in locale e in CI.
if [ "${DKP_NATIVE:-0}" = "1" ]; then
    mkdir -p /work/exlaunch "$HERE/build"
    [ -L "$EXL" ] || rm -rf "$EXL"
    ln -sfn /work/exlaunch "$EXL"
fi
# albero di build = exlaunch originale + i nostri file. cp -a mantiene le date (build incrementale di make); i sorgenti vengono
# ricopiati da zero a ogni build, cosi' un file tolto da src/ non resta nella build. build/exlaunch/build (oggetti) resta.
mkdir -p "$EXL"
find "$EXL/" -mindepth 1 -maxdepth 1 ! -name build ! -name deploy -exec rm -rf {} +
(cd "$SUB" && find . -mindepth 1 -maxdepth 1 ! -name .git ! -name build ! -name deploy -exec cp -a {} "$EXL/" \;)
rm -rf "$EXL/source/program"
cp -a "$HERE/src/program" "$EXL/source/program"
# correzioni a exlaunch come patch (src/exlaunch_patch/*.patch, scopo nella prima riga di ognuna): se un aggiornamento del submodule
# cambia le righe toccate, patch fallisce e la build si ferma invece di usare una versione vecchia del file
for P in "$HERE"/src/exlaunch_patch/*.patch; do
    patch -d "$EXL" -p1 --forward --batch --silent --no-backup-if-mismatch < "$P" \
        || { echo "patch non applicabile: ${P#$HERE/} (exlaunch aggiornato? rifare la patch)" >&2; exit 1; }
done
cp -a "$HERE/config.mk" "$EXL/config.mk"
cp -a "$HERE/config.json" "$EXL/config.json"
VERB=""; [ "$1" = "clean" ] && VERB="clean"
VIEWS="${MK8DX_VIEWS:-8}"
LYT8="${MK8DX_LYT8:-0}"
P3TEST="${MK8DX_P3TEST:-0}"
MENU3D8="${MK8DX_P7_MENU3D8:-1}"
EXP8INIT="${MK8DX_P7_EXP8INIT:-0}"
REC16="${MK8DX_P7_REC16:-$MENU3D8}"
FLAGS="-DMK8DX_VIEWS=$VIEWS -DMK8DX_LYT8=$LYT8 -DMK8DX_P3TEST=$P3TEST -DMK8DX_P7_MENU3D8=$MENU3D8 -DMK8DX_P7_EXP8INIT=$EXP8INIT \
-DMK8DX_P7_REC16=$REC16 -DMK8DX_P7_FLOW8=${MK8DX_P7_FLOW8:-1} -DMK8DX_SEL8B=${MK8DX_SEL8B:-1} -DMK8DX_TEXDESC=${MK8DX_TEXDESC:-16640} \
-DMK8DX_SPECTOFF=${MK8DX_SPECTOFF:-0} -DMK8DX_NO7CFIX=${MK8DX_NO7CFIX:-0} -DMK8DX_GIFT8=${MK8DX_GIFT8:-1} -DMK8DX_EXCDIAG=${MK8DX_EXCDIAG:-1} \
-DMK8DX_MODELDIAG=${MK8DX_MODELDIAG:-0} -DMK8DX_LOG=${MK8DX_LOG:-0}"
# flag diversi dall'ultima build -> build da zero (make non vede il cambio dei -D: es. hook_impl.cpp dipende da MK8DX_LOG)
STAMP="$EXL/build/.mk8dx-flags"
[ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$FLAGS" ] || VERB="clean"
CMD='export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro} DEVKITA64=${DEVKITA64:-/opt/devkitpro/devkitA64}
    export PATH=$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH
    if [ -n "$VERB" ]; then make clean; fi
    touch source/program/main.cpp
    make -j"$(nproc)" V=1 C_FLAGS="$FLAGS"'
rm -f "$EXL/deploy/subsdk9"
set +e
# DKP_NATIVE=1: gia' dentro l'ambiente devkitA64 (es. GitHub Actions con container devkitpro/devkita64), niente Docker
if [ "${DKP_NATIVE:-0}" = "1" ]; then
    (cd "$EXL" && VERB="$VERB" FLAGS="$FLAGS" bash -c "$CMD") > "$HERE/build.log" 2>&1
else
    docker run --rm -u "$(id -u):$(id -g)" -e VERB="$VERB" -e FLAGS="$FLAGS" -v "$EXL:/work/exlaunch" -w /work/exlaunch "$IMAGE" bash -c "$CMD" > "$HERE/build.log" 2>&1
fi
RC=$?
set -e
grep -E "error:|warning:|built" "$HERE/build.log" || true
if [ $RC -ne 0 ] || [ ! -f "$EXL/deploy/subsdk9" ]; then echo "build FALLITA (rc=$RC): vedi build.log" >&2; tail -20 "$HERE/build.log" >&2; exit 1; fi
mkdir -p "$EXL/build" && printf '%s' "$FLAGS" > "$STAMP"
ls -la "$EXL/deploy/"
