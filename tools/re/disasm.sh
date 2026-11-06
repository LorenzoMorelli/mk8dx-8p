#!/bin/bash
# main NSO (decompresso o no) -> main.bin + main.syms + main.ann.asm (llvm-objdump annotato con i nomi delle importazioni PLT).
# Uso: tools/re/disasm.sh <main (NSO dall'exefs dell'update 4.0.1)> <cartella di uscita>
# Serve: python3 (con il modulo lz4 oppure lz4block.py qui accanto), llvm-objcopy, llvm-objdump.
# Per estrarre l'exefs dall'NCA dell'update: tools/re/exefs.py <prod.keys> <nca> <titlekey> <outdir> (chiavi tue, non incluse).
set -e
D="$(cd "$(dirname "$0")" && pwd)"; IN="$1"; OUT="${2:-.}"
[ -f "$IN" ] || { echo "uso: $0 <main.nso> <cartella>" >&2; exit 1; }
mkdir -p "$OUT"
python3 -I "$D/nso.py" "$IN" "$OUT/main"                      # -> main.bin, main.syms
llvm-objcopy -I binary -O elf64-littleaarch64 --rename-section=.data=.text,code "$OUT/main.bin" "$OUT/main.elf"
llvm-objdump -d --triple=aarch64 "$OUT/main.elf" > "$OUT/main.asm"
python3 -I "$D/plt.py" "$OUT/main.bin" "$OUT/main.asm" "$OUT/main.ann.asm"
echo "ok: $OUT/main.bin, $OUT/main.ann.asm (indirizzo = offset nel main; IPS = offset + 0x100)"
