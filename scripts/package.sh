#!/bin/bash
# Pacchetti per Switch reale (Atmosphère). Uso: scripts/package.sh            (entrambe le varianti)
#                                              VARIANTS=release scripts/package.sh   (solo la distribuibile, come la CI)
# Varianti, stesso commit:
#   mk8dx-8p-release-<commit>/     MK8DX_LOG=0 (senza log), MK8DX_EXCDIAG=1   -> da distribuire
#   mk8dx-8p-diagnostic-<commit>/  MK8DX_LOG=1 (con log),   MK8DX_EXCDIAG=1   -> per le prove
# Ognuna: atmosphere/contents/0100152000022000/exefs/{subsdk9,main.npdm}, README.md (docs/INSTALL.md), SHA256SUMS e lo zip.
# I pacchetti di commit precedenti in dist/ vengono cancellati.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
REV="$(git -C "$HERE" rev-parse --short HEAD)$(git -C "$HERE" diff --quiet -- src scripts config.json config.mk || echo -dirty)"
mkdir -p "$HERE/dist"
find "$HERE/dist" -mindepth 1 -maxdepth 1 -name 'mk8dx-8p-*' ! -name "*-$REV" ! -name "*-$REV.zip" -exec rm -rf {} +
pack() {   # $1 = variante, $2 = MK8DX_LOG
    env -u MK8DX_VIEWS -u MK8DX_TEXDESC -u MK8DX_GIFT8 -u MK8DX_SEL8B -u MK8DX_P7_MENU3D8 -u MK8DX_P7_REC16 -u MK8DX_P7_FLOW8 \
        -u MK8DX_SPECTOFF -u MK8DX_NO7CFIX -u MK8DX_LYT8 -u MK8DX_P3TEST -u MK8DX_P7_EXP8INIT -u MK8DX_MODELDIAG \
        MK8DX_LOG="$2" MK8DX_EXCDIAG=1 "$HERE/scripts/build.sh" >/dev/null
    local OUT="$HERE/dist/mk8dx-8p-$1-$REV"
    rm -rf "$OUT" "$OUT.zip"; mkdir -p "$OUT/atmosphere/contents/0100152000022000/exefs"
    cp "$HERE/build/exlaunch/deploy/subsdk9" "$HERE/build/exlaunch/deploy/main.npdm" "$OUT/atmosphere/contents/0100152000022000/exefs/"
    { cat "$HERE/docs/INSTALL.md"; echo; echo "Build: $1, commit $REV, $(date -u +%Y-%m-%dT%H:%MZ) (MK8DX_LOG=$2, MK8DX_EXCDIAG=1)."; } > "$OUT/README.md"
    (cd "$OUT" && sha256sum atmosphere/contents/0100152000022000/exefs/* > SHA256SUMS)
    if command -v python3 >/dev/null; then (cd "$OUT" && python3 -I -m zipfile -c "$OUT.zip" atmosphere README.md SHA256SUMS); fi
    printf '%-11s %s  subsdk9 %s byte\n' "$1" "$OUT" "$(stat -c %s "$OUT/atmosphere/contents/0100152000022000/exefs/subsdk9")"
}
for v in ${VARIANTS:-release diagnostic}; do
    case "$v" in release) pack release 0 ;; diagnostic) pack diagnostic 1 ;; *) echo "variante sconosciuta: $v" >&2; exit 1 ;; esac
done
