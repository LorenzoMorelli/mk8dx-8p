#!/usr/bin/env python3
# Map PLT stubs to imported symbol names and annotate a llvm-objdump listing.
# Usage: plt.py main.bin main.asm out.asm  (writes annotated asm + prints stub map size)
import struct, sys, re, subprocess

img = open(sys.argv[1], "rb").read()
mod0 = struct.unpack("<I", img[4:8])[0]
dyn = mod0 + struct.unpack("<i", img[mod0+4:mod0+8])[0]
tags = {}
o = dyn
while True:
    t, v = struct.unpack("<qQ", img[o:o+16]); o += 16
    if t == 0: break
    tags.setdefault(t, v)
symtab, strtab = tags[6], tags[5]

def symname(i):
    name = struct.unpack("<I", img[symtab+i*24:symtab+i*24+4])[0]
    return img[strtab+name:img.index(b"\0", strtab+name)].decode()

got = {}
for start, size in ((tags.get(23), tags.get(2)), (tags.get(7), tags.get(8))):
    if not start: continue
    for r in range(start, start + size, 24):
        off, info, add = struct.unpack("<QQq", img[r:r+24])
        if info >> 32:
            got[off] = symname(info >> 32)

names = sorted(set(got.values()))
dem = dict(zip(names, subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.split("\n")))

lines = open(sys.argv[2]).read().split("\n")
stub = {}
pat_adrp = re.compile(r"^\s*([0-9a-f]+):\s+\S+\s+adrp\s+x16, 0x([0-9a-f]+)")
pat_ldr = re.compile(r"ldr\s+x17, \[x16, #0x([0-9a-f]+)\]")
for i, l in enumerate(lines[:-1]):
    m = pat_adrp.match(l)
    if m:
        m2 = pat_ldr.search(lines[i+1])
        if m2:
            g = int(m.group(2), 16) + int(m2.group(1), 16)
            if g in got:
                stub[int(m.group(1), 16)] = dem[got[g]]
print("stubs:", len(stub))
pat_bl = re.compile(r"\b(bl|b)\s+0x([0-9a-f]+)")
with open(sys.argv[3], "w") as out:
    for l in lines:
        m = pat_bl.search(l)
        if m and int(m.group(2), 16) in stub:
            l = l.split("<")[0] + " ; -> " + stub[int(m.group(2), 16)]
        else:
            l = re.sub(r" <_binary_[^>]*>", "", l)
        out.write(l + "\n")
