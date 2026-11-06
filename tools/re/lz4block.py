#!/usr/bin/env python3
# Pure-Python LZ4 block decoder (fallback when the `lz4` module is not installed).
def decompress(src, uncompressed_size):
    dst = bytearray(uncompressed_size)
    i = o = 0
    n = len(src)
    while i < n:
        tok = src[i]; i += 1
        ll = tok >> 4
        if ll == 15:
            while True:
                b = src[i]; i += 1; ll += b
                if b != 255: break
        dst[o:o+ll] = src[i:i+ll]; i += ll; o += ll
        if i >= n: break
        off = src[i] | (src[i+1] << 8); i += 2
        ml = (tok & 15) + 4
        if ml == 19:
            while True:
                b = src[i]; i += 1; ml += b
                if b != 255: break
        s = o - off
        if off >= ml:
            dst[o:o+ml] = dst[s:s+ml]
        else:
            for k in range(ml): dst[o+k] = dst[s+k]
        o += ml
    return bytes(dst[:o])
