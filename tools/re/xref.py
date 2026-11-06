#!/usr/bin/env python3
# Find adrp/add (or adrp/ldr) references to given addresses in an annotated objdump listing.
# Usage: xref.py main.asm addr [addr...]
import re, sys

targets = {int(a, 16) for a in sys.argv[2:]}
lines = open(sys.argv[1]).read().split("\n")
adrp = re.compile(r"^\s*([0-9a-f]+):\s+\S+\s+adrp\s+(x\d+), 0x([0-9a-f]+)")
use = re.compile(r"^\s*([0-9a-f]+):\s+\S+\s+(add|ldr|ldrb|ldrsw|str|strb|ldp)\s+(\w+), \[?(x\d+)(?:, #(-?0x[0-9a-f]+|-?\d+))?")
for i, l in enumerate(lines):
    m = adrp.match(l)
    if not m: continue
    reg, page = m.group(2), int(m.group(3), 16)
    for j in range(i + 1, min(i + 12, len(lines))):
        u = use.match(lines[j])
        if u and u.group(4) == reg:
            off = int(u.group(5), 0) if u.group(5) else 0
            if page + off in targets:
                print(f"{hex(page + off)} <- {u.group(1)} ({u.group(2)})")
            break
