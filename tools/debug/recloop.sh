#!/bin/bash
# Riprende l'esecuzione; a OGNI arresto registra in rec.log pc/lr/registri/backtrace e la riga "unmapped" del log di Eden,
# poi stepi + continue. Uso: recloop.sh [secondi] ; stop con "touch tools/debug/run/rec.stop". Se il pc e' < 0x10000 (salto nullo) si ferma.
D="$(cd "$(dirname "$0")" && pwd)"; R="$D/run"
EL="${EDEN_DIR:?imposta EDEN_DIR}/user/log/eden_log.txt"
end=$((SECONDS+${1:-14400})); rm -f $R/rec.stop; : > $R/rec.log; k=0
stops() { grep -cE "received signal|hit Breakpoint|atchpoint [0-9]+:" $R/gdb.out; }
n=$(stops); WAIT=10 "$D/g.sh" "continue &" >/dev/null
while [ $SECONDS -lt $end ] && [ ! -f $R/rec.stop ]; do
  sleep 0.5; m=$(stops); [ $m -le $n ] && continue
  sleep 0.3; k=$((k+1))
  { echo "=== arresto $k $(date +%T)"; LC_ALL=C grep -aoE "(unmapped memory access|Unmapped [A-Za-z0-9]+ @) [^ ]*" $EL | tail -1;
    WAIT=15 "$D/g.sh" "info registers pc lr x0 x1 x8 x19 x20" "bt 5"; } >> $R/rec.log
  pc=$(grep -oE "^pc +0x[0-9a-f]+" $R/rec.log | tail -1 | awk '{print $2}')
  if [ -n "$pc" ] && [ $((pc)) -lt $((0x10000)) ]; then echo "PC NULLO: fermo" >> $R/rec.log; break; fi
  WAIT=10 "$D/g.sh" "stepi" >/dev/null; n=$(stops); WAIT=10 "$D/g.sh" "continue &" >/dev/null
done
echo "fine ciclo ($k arresti)" >> $R/rec.log
