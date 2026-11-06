# How mk8dx-8p works

Target: Mario Kart 8 Deluxe 4.0.1 (title `0100152000022000`, main BID `158C0C7399AABD7BF83795C2EF3484C7`). Offsets below are into the
decompressed main NSO; IPS offset = offset + 0x100. All hooks live in `src/program/main.cpp`, grouped by namespace.

## Approach

The Switch 1 executable already has the 5–8 player menu buttons, the saved player count and the controller applet request; the RomFS
already has the 6P/8P layouts (`mn_CharaKartSlct_8pl`, `mn_Page_Lyt_CharaParts_8pl`, `rc_RaceView_6P_Ml`/`8P_Ml`, …). Everything else is
written for at most 4 local players: fixed `[4]` arrays, `cmp #4` clamps, loops unrolled for pads 0..3.

The mod keeps the game's own flow and runs it with N = 8:
- patch the clamp (`cmp #4` → `#8`) when the data behind it is already large enough;
- move fixed-size arrays to larger buffers when they are not (word patches on every access, all-or-nothing);
- extend unrolled 0..3 sequences to 4..7 with an inline hook right after them;
- never patch the visible symptom without finding the limit that causes it.

Every hook site is checked against the expected instruction word before installing; a mismatch disables that group and logs it.

## Areas

| area | namespace(s) | what changes |
|---|---|---|
| input | `input`, `padui` | 8 npads polled, input players 4..7, controller applet with min = max = N |
| menu flow | `menu8` | 5–8 buttons skip the "requires Switch 2" dialog (`0x4d9468`), N saved and propagated |
| character / kart selection | `sel8`, `sel8b`, `p4`, `p5`, `cur8`, `cw8`, `cfg8` | 8-player page `0x12` with a second control set; page cursor with 8 slots (slots 4..7 live at a shadow index); focus table for pads 4..7; variant windows (CharaList pages) for P5–P8 |
| menu 3D previews | `m3d8`, `rec16`, `ring9` | 8 kart units, model lists 12 → 16 records, Body/Wing ring 5 → 9, larger menu and boot task heaps |
| race setup | `race8`, `kart8` | race manager with 20 entries (8 locals + 12 karts) instead of 16; kart manager local array 4 → 8 |
| race views | `proto`, `view8`, `geom`, `order8`, `spect` | "Main" renderer with 8 views, extra 3D layers Sub4..7 in GPU draw order, 3×2 / 4×2 partitions, g3d models created with 8 views, spectator camera in the empty cell (5P/7P) |
| HUD | `hud8`, `pal8`, `ibox` | 6P/8P race view layouts, 2D layers per cell, Lakitu and finish logo for cells 5–8, player colours for P5–P8, mirrored item box in columns 3–4 |
| pause, results, GP | `pause8`, `hud8`, `pal8` | pause owned by any of 8 pads, 8 human rows in results and GP ranking |
| gifts | `gift8` | unlock animation (Present page) with 8 gift parts |
| graphics device | `desc6d` | texture descriptors 8492 → 16640 (the dynamic ring was exhausted with 8 views) |

## exlaunch patches (`src/exlaunch_patch/*.patch`, applied by `scripts/build.sh` to the copy in `build/exlaunch/`)

The submodule is never modified. If an exlaunch update changes the patched lines, `patch` fails and the build stops.

- `inline_asm-nzcv.patch`: the inline-hook stub also saves and restores NZCV (hooks placed between `cmp` and `b.cond`).
- `hook_impl-trampolines.patch`: off-by-one in the trampoline pool fixed; `TrampolinesUsed()`/`TrampolinesMax()` for the startup log
  (the pool size itself is `JitSize` = 0x20000 in `src/program/setting.hpp`).
- `svc-saltynx.patch`: `svcGetInfo` without the instruction pair SaltyNX searches for (otherwise SaltyNX hooked our copy instead of the
  SDK's).

## Diagnostics

- `MK8DX_LOG=1`: debug lines via `svcOutputDebugString` (Eden log). Lines `patch …` confirm each word patch at startup.
- `MK8DX_EXCDIAG=1` (default): hook on the SDK default exception handler (nnSdk `+0x90f38`); the fault context ends up in the registers of
  the Atmosphère crash report (X03 pc, X04 far, X05 esr, X06 lr, X07 sp; the kernel clears x8 and above on `svcBreak`).

## Known issues

- Intermittent lighting/tint glitch seen a few times in Eden (whole scene green or purple for one race, also in 1P); not reproduced on
  hardware so far. The texture descriptor ring is healthy when it happens. Next step: a RenderDoc capture of the affected frame.
- Not tested on hardware: 7–8 players, handheld mode.
