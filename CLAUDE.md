# CLAUDE.md – mk8dx-8p

Exlaunch module (`subsdk9`) that enables 5–8 player local split-screen in Mario Kart 8 Deluxe 4.0.1 on Switch 1.
Read [PLAN.md](PLAN.md) (architecture) and [STRUCTS.md](STRUCTS.md) (reconstructed structures) before changing hooks.

- **Language:** the maintainer works in Italian: answer in Italian. Code comments are in Italian; public docs (README, PLAN, STRUCTS,
  docs/) are in English.
- **Machine-specific settings** (emulator path, GPU, console) go in `CLAUDE.local.md` (git-ignored).
- **Notes:** development state and analysis reports (Italian) are in [docs/notes/](docs/notes/). Read `docs/notes/STATE.md`
  ("Prossima azione", "Problemi aperti") at the start of a session and update it at the end. `prompt.md` (repo root, git-ignored) is the
  hand-over prompt for the next session. Notes must not contain Nintendo material: no disassembly listings (single-instruction citations
  are fine), screenshots of the game, logs with game data, saves or keys. Things worth remembering across sessions go in this file.

## Working rules (from the maintainer)

- **Follow the original flow with N = 8.** Find the limit the game applies for 4 players (a `cmp #4`, a `[4]` array, a loop unrolled for
  pads 0..3) and extend that same path to 8. No patches on the visible symptom, no partial forcing that needs more patches afterwards.
- **Order of work:** understand first (static analysis + measurements in the emulator), then a plan with exact patches (address, original
  word, hook type, registers), then ask for confirmation, then implement and verify. Ask before any workaround or alternative approach.
- **Visual comparisons** are made against Switch 2 reference screenshots, normalised to 1280×720 and measured in pixels, not by eye.
  P5–P8 colours follow the Switch 2 results screen (cursor ColorChange frames 7..10: red, blue, purple, orange).
- **Testing:** the maintainer drives the menus and writes "fatto"; never send virtual input. Screenshots are taken by the maintainer
  (Ctrl+P in Eden). Give test instructions as short, numbered, self-contained steps.
  Eden input config used for 8-player tests: all 8 players share the same keys except stick-down (P1–P2 = S, P3..P8 = keys 3..8).
- **Eden settings stay at their defaults** (asynchronous shaders OFF), changed only for a targeted test and restored afterwards.
- **Subagents:** Opus only for analysis that needs reasoning (disassembly hypotheses, patch plans, crash diagnosis, structure
  reconstruction); Sonnet/Haiku for mechanical work (grep, extracting asm ranges, translating, summarising). Never Fable.
- **Commits:** one commit per sub-goal. No Nintendo code, assets, keys, saves or screenshots of reference material in this repo.
  Push only `main` (a local branch `archivio-privato`, if present, must never be pushed).

## Build and test

```sh
scripts/build.sh                    # release (MK8DX_LOG=0, MK8DX_EXCDIAG=1)
MK8DX_LOG=1 scripts/build.sh        # emulator/diagnostic build: REQUIRED for Eden, the checks below rely on the log
scripts/package.sh                  # dist/mk8dx-8p-{release,diagnostic}-<commit>/
EDEN_DIR=… scripts/deploy.sh && EDEN_DIR=… MK8D_GAME=… scripts/run-eden.sh
```
- Build runs in Docker (`devkitpro/devkita64:latest`); `DKP_NATIVE=1` builds directly inside that image (CI). `build.sh` copies the
  `exlaunch/` submodule to `build/exlaunch/` and overlays `src/program/`, `src/exlaunch_patch/*`, `config.mk`, `config.json` there (the
  submodule stays clean); output in `build/exlaunch/deploy/`. The build always runs at `/work/exlaunch` (Docker mount; with
  `DKP_NATIVE=1` `build/exlaunch` is a symlink there) because the path ends up in the binary (`__FILE__`, GNU build-id): local and CI
  builds of the same commit and flags are byte-identical. Changed flags trigger a clean build (stamp `build/exlaunch/build/.mk8dx-flags`).
- After every Eden start check `$EDEN_DIR/user/log/eden_log.txt`: `LoadDir` points to the expected mod folder, the line
  `patch 7c P1 0xcf040 …`, 34 lines `patch 7d`, `7d: raceMgr … a 20 entry`, `trampolini usati N / 655`, and `Report: Device:` (GPU).
  Missing lines usually mean an old binary is loaded or a hook group was disabled by an unexpected word.
- Log lines are long: use `grep -o "key[^\[]*"` or split them with Python. Eden caps its log at 100 MB (verbose filters fill it fast).
- Close Eden before editing `user/config/qt-config.ini` (it rewrites the file on exit). To enable an option also set `<key>\default=false`.
- Kill Eden with `kill -9 $(pgrep -x eden.AppImage) $(pgrep -x eden)`. Never use `pkill -f`/`pgrep -f` with a pattern that also appears
  in your own command line (it kills the shell).

## Debugging

- **Unmapped reads return 0 in Eden but crash on hardware.** Run important tests with Eden's GDB stub (`use_gdbstub`, port 6543, ONE
  connection) and the "sanitizer": `tools/debug/start.sh`, then `tools/debug/recloop.sh` logs pc/lr/registers/backtrace at every stop into
  `tools/debug/run/rec.log` and continues (`tools/debug/g.sh` sends single gdb commands; `fp.py`: `setbase`, `fpchain`). With the stub attached do not open
  Eden's settings. Do not put breakpoints on prologues with `str …,[sp,#-N]!`; after a manual interrupt `stepi` before `continue`.
  Restore `use_gdbstub=false` at the end.
- `Unmapped Device ReadBlock` (GPU side) also happens without the mod in Eden: it is not a mod bug.
- **Inline hooks run BEFORE the hooked instruction**; the stub (`src/exlaunch_patch/inline_asm.s`) saves NZCV, which makes hooks between
  `cmp` and `b.cond` safe. Every hook site is verified against its original word before installing. Trampoline pool: 655 entries
  (startup log prints the usage).
- **Logging** (`src/program/loggers.hpp`) is serialised with a lock (newlib `snprintf` is not thread-safe); release builds compile it out.
- **Crashes on hardware:** Atmosphère writes `/atmosphere/crash_reports/*.log`. With `MK8DX_EXCDIAG=1` the report registers hold the fault
  context: X03 pc, X04 far, X05 esr, X06 lr, X07 sp (the kernel zeroes x8 and above on `svcBreak`). Module bases are listed in the report;
  offsets in `TurboS.nss` are main offsets.
- SaltyNX may be loaded in the game process on the maintainer's console; the mod must stay compatible with it (see `src/exlaunch_patch/svc.s`).
- **Graphics:** RenderDoc can be loaded by `run-eden.sh` (`RENDERDOC=1`, default). Capture ONE frame with RenderDoc's own F12 key. Never use
  Eden's "Toggle Renderdoc Capture" hotkey (`renderdoc_hotkey`): a start/stop capture on Eden copies ~128 MB per frame and froze the PC.
- **Reverse engineering:** offsets are into the decompressed main NSO of 4.0.1 (BID `158C0C7399AABD7BF83795C2EF3484C7`); IPS offset =
  offset + 0x100. `tools/re/disasm.sh <main> <dir>` builds `main.bin` and the annotated `main.ann.asm` (PLT imports named); keep them
  OUTSIDE the repo. `tools/re/`: `xref.py` (adrp/add references), `fstart.py` (function start), `exefs.py` (exefs from an NCA, own keys),
  `npdm2json.py` (npdm -> config.json). Mark every claim in analysis notes as [VERIFIED] (address, log, screenshot) or [HYPOTHESIS].
