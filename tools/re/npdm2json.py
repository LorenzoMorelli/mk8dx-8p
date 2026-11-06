#!/usr/bin/env python3
# Decode a main.npdm (META/ACID/ACI0) into an npdmtool-compatible JSON (exlaunch config.json style).
# Usage: npdm2json.py main.npdm [template.json] > config.json
# With a template, the template's values are kept and only title_id, filesystem permissions,
# service_access and the decoded kernel capabilities are replaced.
import struct, sys, json
d = open(sys.argv[1], "rb").read()
assert d[:4] == b"META", "not an NPDM"
flags = d[0xC]
meta = {
    "is_64_bit": bool(flags & 1),
    "address_space_type": (flags >> 1) & 7,
    "optimize_memory_allocation": bool(flags & 0x10),
    "disable_device_address_space_merge": bool(flags & 0x20),
    "enable_alias_region_extra_size": bool(flags & 0x40),
    "main_thread_priority": d[0xE],
    "default_cpu_id": d[0xF],
    "system_resource_size": "0x%08x" % struct.unpack("<I", d[0x14:0x18])[0],
    "version": "0x%08x" % struct.unpack("<I", d[0x18:0x1C])[0],
    "main_thread_stack_size": "0x%08x" % struct.unpack("<I", d[0x1C:0x20])[0],
    "name": d[0x20:0x30].split(b"\0")[0].decode(),
}
aci_off, aci_sz, acid_off, acid_sz = struct.unpack("<IIII", d[0x70:0x80])
acid = d[acid_off:acid_off + acid_sz]
assert acid[0x200:0x204] == b"ACID"
acid_flags = struct.unpack("<I", acid[0x20C:0x210])[0]
meta["is_retail"] = bool(acid_flags & 1)
meta["pool_partition"] = (acid_flags >> 2) & 0xF
meta["title_id_range_min"] = "0x%016x" % struct.unpack("<Q", acid[0x210:0x218])[0]
meta["title_id_range_max"] = "0x%016x" % struct.unpack("<Q", acid[0x218:0x220])[0]
aci = d[aci_off:aci_off + aci_sz]
assert aci[:4] == b"ACI0"
meta["title_id"] = "0x%016x" % struct.unpack("<Q", aci[0x10:0x18])[0]
fah_off, fah_sz, sac_off, sac_sz, kc_off, kc_sz = struct.unpack("<IIIIII", aci[0x20:0x38])
fah = aci[fah_off:fah_off + fah_sz]
meta["filesystem_access"] = {"permissions": "0x%016x" % struct.unpack("<Q", fah[4:12])[0]}
sac = aci[sac_off:sac_off + sac_sz]
services, hosts = [], []
i = 0
while i < len(sac):
    c = sac[i]; n = (c & 7) + 1
    name = sac[i + 1:i + 1 + n].decode("latin-1")
    (hosts if c & 0x80 else services).append(name)
    i += 1 + n
meta["service_access"] = services
meta["service_host"] = hosts
kc = struct.unpack("<%dI" % (kc_sz // 4), aci[kc_off:kc_off + kc_sz])
caps, syscalls, maps, pages, irqs = [], {}, [], [], []
it = iter(kc)
for v in it:
    ones = 0
    while (v >> ones) & 1: ones += 1
    if ones == 3:
        caps.append({"type": "kernel_flags", "value": {
            "highest_thread_priority": (v >> 4) & 0x3F, "lowest_thread_priority": (v >> 10) & 0x3F,
            "lowest_cpu_id": (v >> 16) & 0xFF, "highest_cpu_id": (v >> 24) & 0xFF}})
    elif ones == 4:
        mask, idx = (v >> 5) & 0xFFFFFF, (v >> 29) & 7
        for b in range(24):
            if mask >> b & 1: syscalls["svc_%02x" % (idx * 24 + b)] = "0x%02x" % (idx * 24 + b)
    elif ones == 6:
        w = next(it)
        maps.append({"address": "0x%x" % ((v >> 7) & 0xFFFFFF), "is_ro": bool(v >> 31),
                     "size": "0x%x" % ((w >> 7) & 0xFFFFF), "is_io": not bool(w >> 31)})
    elif ones == 7:
        pages.append("0x%x" % ((v >> 8) & 0xFFFFFF))
    elif ones == 11:
        irqs.append([(v >> 12) & 0x3FF, (v >> 22) & 0x3FF])
    elif ones == 13:
        caps.append({"type": "application_type", "value": (v >> 14) & 7})
    elif ones == 14:
        caps.append({"type": "min_kernel_version", "value": "0x%04x" % ((v >> 15) & 0xFFFF)})
    elif ones == 15:
        caps.append({"type": "handle_table_size", "value": (v >> 16) & 0x3FF})
    elif ones == 16:
        caps.append({"type": "debug_flags", "value": {"allow_debug": bool(v >> 17 & 1), "force_debug": bool(v >> 18 & 1)}})
    else:
        caps.append({"type": "unknown", "raw": "0x%08x" % v})
if syscalls: caps.insert(1, {"type": "syscalls", "value": syscalls})
for m in maps: caps.append({"type": "map", "value": m})
for p in pages: caps.append({"type": "map_page", "value": p})
meta["kernel_capabilities"] = caps
if len(sys.argv) > 2:
    t = json.load(open(sys.argv[2]))
    for k in ("title_id", "filesystem_access", "service_access", "service_host", "kernel_capabilities"):
        t[k] = meta[k]
    meta = t
print(json.dumps(meta, indent="\t"))
