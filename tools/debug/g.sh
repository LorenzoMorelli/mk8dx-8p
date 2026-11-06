#!/bin/bash
# Manda comandi alla sessione di start.sh (uno per argomento) e stampa la nuova uscita. Esempi:
#   tools/debug/g.sh "interrupt" ; tools/debug/g.sh "monitor get info" "x/4gx 0x..." ; tools/debug/g.sh "continue &"
# WAIT = attesa massima in mezzi secondi (default 60). Con il bersaglio in esecuzione servono "interrupt" prima e "stepi" + "continue &" dopo.
R="$(cd "$(dirname "$0")" && pwd)/run"
n=$(wc -c < "$R/gdb.out")
for c in "$@"; do echo "$c" > "$R/in"; done
echo "echo @@DONE@@\\n" > "$R/in"
for i in $(seq 1 "${WAIT:-60}"); do tail -c +$((n+1)) "$R/gdb.out" | grep -q "@@DONE@@" && break; sleep 0.5; done
tail -c +$((n+1)) "$R/gdb.out" | grep -v "@@DONE@@"
