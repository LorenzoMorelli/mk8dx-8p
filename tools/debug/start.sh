#!/bin/bash
# Avvia una sessione gdb persistente collegata allo stub GDB di Eden (use_gdbstub=true, porta 6543, UNA sola connessione).
# I comandi arrivano dalla fifo run/in (vedi g.sh), l'uscita va in run/gdb.out. Uso: tools/debug/start.sh [porta]
D="$(cd "$(dirname "$0")" && pwd)"; R="$D/run"; PORT="${1:-6543}"
mkdir -p "$R"
pkill -x gdb 2>/dev/null; sleep 0.5
rm -f "$R/in" "$R/gdb.out"; mkfifo "$R/in"; : > "$R/gdb.out"
( exec 3>"$R/in"; while kill -0 $$ 2>/dev/null; do sleep 3600; done ) &   # tiene aperta la fifo in scrittura
echo $! > "$R/keeper.pid"
gdb -q -nx -x "$D/fp.py" -ex "set pagination off" -ex "set confirm off" -ex "set architecture aarch64" \
    -ex "target remote 127.0.0.1:$PORT" < "$R/in" > "$R/gdb.out" 2>&1 &
echo $! > "$R/gdb.pid"
sleep 2; tail -5 "$R/gdb.out"
