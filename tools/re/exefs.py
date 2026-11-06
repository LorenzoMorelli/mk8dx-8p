#!/usr/bin/env python3
# Extract exefs (section 0 PFS0) from an NCA using system libcrypto. Usage: exefs.py keys nca titlekey outdir
import ctypes, ctypes.util, struct, sys, os

lib = ctypes.CDLL(ctypes.util.find_library("crypto"))
lib.EVP_CIPHER_CTX_new.restype = ctypes.c_void_p
lib.EVP_get_cipherbyname.restype = ctypes.c_void_p
lib.EVP_get_cipherbyname.argtypes = [ctypes.c_char_p]
lib.EVP_CipherInit_ex.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
lib.EVP_CipherUpdate.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.c_char_p, ctypes.c_int]
lib.EVP_CIPHER_CTX_set_padding.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.EVP_CIPHER_CTX_free.argtypes = [ctypes.c_void_p]

def crypt(name, key, iv, data, enc=0):
    ctx = lib.EVP_CIPHER_CTX_new()
    c = lib.EVP_get_cipherbyname(name.encode())
    assert c, name
    assert lib.EVP_CipherInit_ex(ctx, c, None, key, iv, enc) == 1
    lib.EVP_CIPHER_CTX_set_padding(ctx, 0)
    out = ctypes.create_string_buffer(len(data) + 32); n = ctypes.c_int()
    assert lib.EVP_CipherUpdate(ctx, out, ctypes.byref(n), data, len(data)) == 1
    lib.EVP_CIPHER_CTX_free(ctx)
    return out.raw[:n.value]

keys = {}
for l in open(sys.argv[1]):
    if "=" in l:
        k, v = [x.strip() for x in l.split("=", 1)]
        keys[k] = bytes.fromhex(v)

p = sys.argv[2]; off = 0
if "@" in p: p, off = p.rsplit("@", 1); off = int(off, 0)
class F:
    def __init__(s): s.f = open(p, "rb")
    def seek(s, o): s.f.seek(o + off)
    def read(s, n): return s.f.read(n)
f = F()
f.seek(0)
raw = f.read(0xC00)
hk = keys["header_key"]
hdr = b"".join(crypt("aes-128-xts", hk, i.to_bytes(16, "big"), raw[i*0x200:(i+1)*0x200]) for i in range(6))
assert hdr[0x200:0x204] == b"NCA3", hdr[0x200:0x204]
ctype, kgen_old, kgen_new = hdr[0x205], hdr[0x206], hdr[0x220]
rights = hdr[0x230:0x240]
mk = max(kgen_old, kgen_new); mk = mk - 1 if mk else 0
print(f"content type {ctype}, masterkey {mk:02x}, rights {rights.hex()}")
tk = crypt("aes-128-ecb", keys[f"titlekek_{mk:02x}"], None, bytes.fromhex(sys.argv[3]))

start, end = struct.unpack("<II", hdr[0x240:0x248])
fs = hdr[0x400:0x600]
fstype, htype, etype = fs[2], fs[3], fs[4]
print(f"section0 fs={fstype} hash={htype} enc={etype} range {start*0x200:#x}-{end*0x200:#x}")
pfs_off, pfs_size = struct.unpack("<QQ", fs[0x8 + 0x38:0x8 + 0x48])
sec_ctr = fs[0x140:0x148][::-1]
base = start * 0x200

def read(off, size):
    a = (base + off) & ~0xF
    f.seek(a); data = f.read((base + off + size - a + 0xF) & ~0xF)
    iv = sec_ctr + (a >> 4).to_bytes(8, "big")
    dec = crypt("aes-128-ctr", tk, iv, data)
    return dec[(base + off) - a:(base + off) - a + size]

pfs = read(pfs_off, 0x4000)
assert pfs[:4] == b"PFS0", pfs[:4]
n, st = struct.unpack("<II", pfs[4:12]); h = 0x10 + n * 0x18
os.makedirs(sys.argv[4], exist_ok=True)
for i in range(n):
    o, s, no = struct.unpack("<QQI", pfs[0x10+i*0x18:0x10+i*0x18+20])
    name = pfs[h+no:pfs.index(b"\0", h+no)].decode()
    open(os.path.join(sys.argv[4], name), "wb").write(read(pfs_off + h + st + o, s))
    print(f"  {name} {s}")
