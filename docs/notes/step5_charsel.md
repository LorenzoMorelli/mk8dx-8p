# Step 5 – selezione personaggi per 5-8 giocatori: report (2 nov 2026)

Piano: step5_plan.md (sintesi) + step5_plan_A1/A2/B/C.md (analisi statiche). Codice: src/program/main.cpp
(namespace race8 / padui / sel8, hook S5.1-S5.4). Log filtrati in step5_data/eden_log_5*.txt, screenshot 5c.

## Esito [VERIFICATO in Eden, N = 8]
| criterio | stato | prova |
|---|---|---|
| raceMgr a 8 locali | OK: `race+0x178=8`, `+0x17c={0..7}`, mappa slot→entry a +0x19c `{0..7,-1×4}`; entry 4..7 con GamePlayer 4..7 (prima diventavano CPU) | eden_log_5a |
| giocatori di input 4..7 | OK: PIM 8/8 (lista figli 9/9); dopo l'applet i locali 4..7 vengono rilegati ai controller di ordine 5..8 (npad 4..7) | eden_log_5a "rilega locale", "bind locale" |
| pad UI 4..7 | OK: `active8` con N>4, `pad UI 4..7 pronto=1` tramite la catena vera (H4 disattivato), tasti 5..8 → pad UI 4..7 distinti | eden_log_5b |
| applet | una sola chiamata (8,8); con 4g torna min=max=4 | eden_log_5b |
| accessi a indirizzi nulli nel menu | 0 (step 4: 35) grazie a H-A2-0 | eden_log_5a/5b |
| layout 6pl/8pl | caricabile (opzione di build `MK8DX_LYT8=8`): 8 pannelli L_Name, parte e layout ≠ 0, nessun crash; ma le anteprime 3D restano nelle 4 posizioni Multi e i pannelli 4..7 non sono pilotati → **disattivato per default** | eden_log_5c, eden_5c_lyt8pl.png |
| 5-8 scelgono personaggio e kart in modo indipendente | OK con **due turni** (4 cursori): personaggi e0..e7 = 39, 0, 1, 2, 35, 1, 2, 3; kart dei locali 4..7 scritti al turno 2; `sB+0x180=8 +0x184=8` | eden_log_5d "A2 kart Multi -> avanti" |
| la selezione arriva alla gara | OK fino all'init della scena di gara (`SceneInitRace n_locali=8`, renderer a 8 viste, viste extra attive); poi **salto a PC=0** all'avvio della gara (nessun backtrace da Eden) → step 6 | eden_log_5d coda |
| regressione 1-4P | 4g: `+0x178=4`, mappa slot `{0..3}`, applet (4,4), cursori normali, `active8 -> 0`. **Gara 4P completa non ancora rifatta** con la mappa slot spostata (da fare all'inizio dello step 6) | eden_log_5a/5b |
| 8 cursori simultanei / 8 slot attivi nel layout 8pl | **NON raggiunto**: vedi blocco_step5.md | – |

## Perché due turni
Il framework dei cursori del menu è dimensionato a 4 giocatori in modo strutturale (selettore 0xb8 B con 4 stati,
19 clamp `cmp #4`, PageMgr+0xc68[4], memoria cursori [0x88][4], oggetti grafici [4], pagine CharaWin 0x28..0x2b con
17 siti `0x28+i`, pagina kart +0x460[4], 13 array `int[4]` della config). Il turno 2 rimappa `0x8bf158(0..3)` sui pad
extra (idx 4..7 → locali 4..7): il gioco scrive il race-config dei locali 4..7 con i propri setter.

## Modifiche (tutte con verifica della parola originale; gruppi tutto-o-niente)
| gruppo | tipo | siti | effetto |
|---|---|---|---|
| B1 | 8 patch imm + `cmp #3→#7` ×3 | 0x868e30 0x868ee4 0x869684 0x8699ec 0x86b940 (`b9018d8b→b9019d8b`), 0x86b0b4, 0x88236c, 0x882408; 0x86ec08, 0x86901c, 0x86b970 | mappa slot→entry da +0x18c a +0x19c int[12]; +0x17c int[8] |
| B1 | REPLACE 0x86af68, 0x868f00; trampolino ctor 0x8689b8 | | setter slot a 12 voci; compattazione a 8 locali; -1 iniziali |
| B2 | patch 0xca3fc/0xca410; trampolini 0xca3e0, 0x7c5340, 0x7c6218, 0x7c62bc, 0x7c5044, 0x7c60c0, 0x7ce38c, 0x7ce8c4; coda di 0x7c5794/0x7c58d0 | | PIM a 8 GamePlayer (G1-G8), ordine controller +1 per 4..7 |
| B3 | REPLACE 0x86a594 | | GamePlayer libero fra 0..7 |
| B4 | trampolini 0x86a998, 0x86adec, 0x86ad18; patch 0x86ac14 | | rilegature e flag +0x76d per i locali 4..7 |
| H-A2-0 | trampolini 0x3fd5a4, 0x3fdd70 | | cicli i<N senza pagine CharaWin/entry per gli slot 4..7 |
| C1/C2 | 12 × `71000c1f→71001c1f`, 4 × `321e03e0→321d03e0` | famiglia pad UI, siti del tocco | idx pad ≤ 7, pad del tocco = GetPad(8) |
| C3-C5 | trampolino 0x8bf040, REPLACE 0x8bf158, trampolino 0x8bf058 | | pad extra statici, mappa (round2), calc + fusione |
| A1-M1 | trampolini 0x4be854, 0x4be8d0; inline 0x4beb04 | | nome Page_Lyt 6pl/8pl, w20 = 6/8, animazione "lato" (opzione MK8DX_LYT8) |
| S5.4 | trampolini 0x4b74a4, 0x4ba77c, 0x4badb0, 0x4c0898, 0x4c2820, 0x8ba5fc, 0x4203e0 | | due turni (charsel e kart), tipi dei locali 4..7, log |

## Scoperte / correzioni ai documenti precedenti
- ips_B crashava per la rinomina del pane `mn_CharaKartSlct_Multi` (T_Name_00 NULL a 0x4bdf30), non per i layout 8pl.
- Pad UI 4 = pad del tocco (non "any-controller"); la fusione degli input (+0x2c5) la fa il pad 0 in gara.
- raceMgr+0x178 restava 4 perché 0x86a594 non trova GamePlayer oltre il 4° (entry declassata a CPU).
- `0x8ba5fc(page, link)` è la transizione di pagina generica (109 chiamanti); il kart avanza a 0x4c2a44 (+0x498=1, +0x184=0x23).
- vt92 0x4bbd18: evento tipo 0 (spostamento) scrive cfg+0x1fb4[slot] e l'anteprima; tipo 2 (decide) → stato 5. Senza spostamenti
  nel turno 2 l'entry non veniva scritta: ora il modulo scrive cfg+0x1fb4[i] con 0x4203e0 all'inizio del turno 2.
- Le pagine del menu (compresa CharaParts 0x16) si costruiscono tutte all'ingresso nel menu (0x8b3a10 → 0x4048b4 → 0x726468 → 0x7240d8):
  il nome del layout non può dipendere da N.

## Aperti (step 6)
1. Crash all'avvio della gara con 8 locali (`Cannot execute instruction at unmapped address 0`, dopo il primo frame con viste extra):
   sospetti pagine HUD RaceView0..3 (4), 0x49c938 (pronto/applet srotolati 0..3), 0x551b28 (u64[4] per locale), 0x13e59c audio ≤4,
   +0x214/+0x224 int[4]. Per localizzarlo: GDB stub di Eden (il log non ha backtrace) o hook con LogBacktrace sulle funzioni per locale.
2. Regressione gara 4P completa + risultati (mappa slot a +0x19c attiva sempre) e 1P.
3. Tocco per i pad 4..7 (0x422efc) non fatto; +0x1a8 bit 4 = pad del tocco (collisione potenziale solo con 8 slot attivi).
4. Layout 8pl: solo con 8 slot attivi (blocco_step5.md).
