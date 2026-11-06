#!/usr/bin/env python3
# Decompress an NSO into a flat memory image (<out>.bin) and dump its dynamic symbols (<out>.syms).
# Usage: nso.py in.nso out_prefix
import struct, sys, os
try:
    import lz4.block
except ImportError:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import lz4block
    class lz4: block = lz4block

d = open(sys.argv[1], "rb").read()
assert d[:4] == b"NSO0"
flags = struct.unpack("<I", d[0xC:0x10])[0]
segs = []
for i, (h, cs) in enumerate(zip((0x10, 0x20, 0x30), (0x60, 0x64, 0x68))):
    foff, moff, size = struct.unpack("<III", d[h:h+12])
    csize = struct.unpack("<I", d[cs:cs+4])[0]
    data = d[foff:foff+csize]
    if flags & (1 << i):
        data = lz4.block.decompress(data, uncompressed_size=size)
    segs.append((moff, data))
bss = struct.unpack("<I", d[0x3C:0x40])[0]
img = bytearray(max(m + len(s) for m, s in segs) + bss)
for m, s in segs:
    img[m:m+len(s)] = s
open(sys.argv[2] + ".bin", "wb").write(img)
print("segments:", [(hex(m), hex(len(s))) for m, s in segs])

# MOD0 -> .dynamic -> dynsym/dynstr
mod0 = struct.unpack("<I", img[4:8])[0]
assert img[mod0:mod0+4] == b"MOD0"
dyn = mod0 + struct.unpack("<i", img[mod0+4:mod0+8])[0]
tags = {}
o = dyn
while True:
    t, v = struct.unpack("<qQ", img[o:o+16]); o += 16
    if t == 0: break
    tags.setdefault(t, v)
symtab, strtab, strsz = tags[6], tags[5], tags[10]
# nchain from DT_HASH gives symbol count
nsym = struct.unpack("<I", img[tags[4]+4:tags[4]+8])[0] if 4 in tags else (strtab - symtab) // 24
with open(sys.argv[2] + ".syms", "w") as out:
    for i in range(nsym):
        name, info, other, shndx, value, size = struct.unpack("<IBBHQQ", img[symtab+i*24:symtab+i*24+24])
        s = img[strtab+name:img.index(b"\0", strtab+name)].decode(errors="replace")
        out.write(f"{'U' if shndx == 0 else 'D'} {value:#010x} {s}\n")
print("symbols:", nsym)
