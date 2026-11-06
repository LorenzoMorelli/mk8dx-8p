# Comandi gdb: setbase <hex> (base del main), fpchain (catena dei frame x29 con indirizzi main+off).
import gdb
BASE = 0  # base del main: "monitor get info" (riga TurboS.nss), poi "setbase <hex>"
class SetBase(gdb.Command):
    """setbase <hex>: set the main module base used by the other commands."""
    def __init__(self): super().__init__("setbase", gdb.COMMAND_USER)
    def invoke(self, arg, tty):
        global BASE
        BASE = int(arg, 0); print(f"BASE={BASE:#x}")
SetBase()
class FpChain(gdb.Command):
    """Walk x29 frame-pointer chain, printing return addresses relative to main base."""
    def __init__(self): super().__init__("fpchain", gdb.COMMAND_USER)
    def invoke(self, arg, tty):
        inf = gdb.selected_inferior()
        pc = int(gdb.parse_and_eval("$pc")); lr = int(gdb.parse_and_eval("$x30")); fp = int(gdb.parse_and_eval("$x29"))
        rel = lambda a: f"main+{a-BASE:#x}" if BASE <= a < BASE + 0x1368000 else hex(a)
        out = [f"pc={rel(pc)} lr={rel(lr)}"]
        for i in range(25):
            if fp == 0 or fp & 7: break
            try:
                m = inf.read_memory(fp, 16).tobytes()
            except Exception:
                break
            nfp = int.from_bytes(m[:8], "little"); ret = int.from_bytes(m[8:], "little")
            out.append(rel(ret)); fp = nfp
        print("CHAIN: " + " <- ".join(out))
FpChain()

