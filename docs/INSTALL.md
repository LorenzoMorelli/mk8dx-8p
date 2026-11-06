# mk8dx-8p – installation (Switch 1 with Atmosphère)

Experimental mod that unlocks **5–8 player local split-screen** in Mario Kart 8 Deluxe on Switch 1.

## Requirements
- Switch 1 running **Atmosphère** custom firmware. The mod does **not** work on stock firmware (`main.npdm` is not signed by Nintendo).
- Mario Kart 8 Deluxe (title id `0100152000022000`) with **update 4.0.1** installed, and only that version: the mod checks the game
  code before patching it. Main build id: `158C0C7399AABD7BF83795C2EF3484C7`.
- 5–8 controllers (single Joy-Con, Pro Controllers, …).

## Install
1. Back up your save data first (e.g. with JKSV): the mod is experimental.
2. Copy the `atmosphere/` folder of the package to the **root** of the microSD card, merging it with the existing one. You must end up with:
   ```
   /atmosphere/contents/0100152000022000/exefs/subsdk9
   /atmosphere/contents/0100152000022000/exefs/main.npdm
   ```
3. Boot Atmosphère and start the game. Select 5–8 players in the multiplayer menu.

To uninstall, delete `/atmosphere/contents/0100152000022000/exefs/`.

## Notes
- Do not combine it with other mods that replace the game's `subsdk9` or `main.npdm`. SaltyNX and its plugins (FPSLocker, ReverseNX) are fine.
- If the game crashes, Atmosphère writes a report in `/atmosphere/crash_reports/`: please attach the newest `.log` to the bug report
  (registers X03..X07 hold the fault PC, address, ESR, LR and SP).

## Tested
| where | what |
|---|---|
| Switch 1, Atmosphère, docked (with SaltyNX) | boot, menus, races and Grand Prix with **5 and 6 players** |
| Eden emulator v0.2.1 | everything up to **8 players** (selection, races, pause, Grand Prix with results) |

Not tested yet on hardware: 7–8 players, handheld mode.
