# mk8dx-8p – development

- [PLAN.md](../PLAN.md): how the mod works (areas, approach, main hooks). [STRUCTS.md](../STRUCTS.md): reconstructed game structures.
  [notes/](notes/): development state and analysis notes (Italian). [CLAUDE.md](../CLAUDE.md): working rules and debugging tips.
- Code: `src/program/main.cpp` (all hooks), `src/exlaunch_patch/*.patch` (fixes to exlaunch, see PLAN.md). The build copies the
  `exlaunch/` submodule to `build/exlaunch/` and applies the patches there; the submodule itself is never modified.
  Offsets are into the decompressed main NSO of 4.0.1 (BID `158C0C7399AABD7BF83795C2EF3484C7`).

Rule of thumb used throughout: find where the original code limits something to 4 and extend that same path to 8, instead of patching the
symptom.

## Build

Docker with `devkitpro/devkita64`:
```sh
git submodule update --init
scripts/build.sh                 # release build -> build/exlaunch/deploy/{subsdk9,main.npdm}
MK8DX_LOG=1 scripts/build.sh     # with debug log lines "[mk8dx-8p]" (svcOutputDebugString)
scripts/package.sh               # dist/mk8dx-8p-{release,diagnostic}-<commit>/ + zip
```
CI (`.github/workflows/build.yml`) builds the release package on every push (`DKP_NATIVE=1` builds inside the container without Docker)
and attaches the zip to the GitHub release when a `v*` tag is pushed.

**Build options** (environment variables, full list at the top of `scripts/build.sh`):

| variable | default | meaning |
|---|---|---|
| `MK8DX_LOG` | 0 | debug log lines; needed for emulator testing |
| `MK8DX_EXCDIAG` | 1 | on a crash, put the fault context in the Atmosphère report registers (X03 pc, X04 far, X05 esr, X06 lr, X07 sp) |
| `MK8DX_MODELDIAG` | 0 | test only: stop with a code if a 3D model is not ready when extended |
| `MK8DX_VIEWS` | 8 | maximum race views |
| `MK8DX_TEXDESC` | 16640 | texture descriptors of the graphics device (0 = game value) |
| others | | feature switches for bisection (`MK8DX_GIFT8`, `MK8DX_SEL8B`, …) |

## Testing in an emulator

`EDEN_DIR=/path/to/portable/eden scripts/deploy.sh` copies the build to Eden's mod folder and
`EDEN_DIR=… MK8D_GAME=/path/to/game.xci scripts/run-eden.sh` starts it (game, keys, firmware and emulator are not provided). Use
`MK8DX_LOG=1` builds and read `$EDEN_DIR/user/log/eden_log.txt`. Eden's GDB stub (port 6543) is useful to catch unmapped reads, which
return 0 in Eden but crash on hardware (`tools/debug/`: persistent gdb session and a loop that logs every stop).

## Reverse engineering

Extract the main NSO from the 4.0.1 update (your own keys), then `tools/re/disasm.sh main out/` produces the decompressed `main.bin` and
an annotated disassembly (`main.ann.asm`, imports named); keep them out of the repository. `tools/re/` also has `xref.py`, `fstart.py`,
`exefs.py`, `npdm2json.py`.
