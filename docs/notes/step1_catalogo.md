# Step 1 – Catalogo delle assunzioni "massimo 4 giocatori locali" (main 4.0.1)

Analisi statica, nessuna modifica al gioco. Offset = main decompresso; IPS = +0x100.
Dettagli e disassemblati: `step1_input.md`, `step1_menu.md`, `step1_race_view.md`, `step1_race_core.md`,
dati grezzi in `step1_data/`. Strutture ricostruite: `../STRUCTS.md`.

## 1. Oggetti chiave (catena di accesso) [VERIFICATO]
- `system = (*(*0x12fb388))->[0x28]`; tabella sottosistemi a `system+0x190`.
- **settingsA** = `system+0x240` (accessor 0x86f7bc, 1398 chiamanti = impostazioni correnti);
  **settingsB** = `system+0x248` (0x86f7d4, in modifica dal menu). Layout (reset 0x86ec8c): +0x8 modo,
  +0x24 flag, +0x44 12 entry kart × 0x1c, +0x180 n locali, +0x184 umani, +0x188 u32[5] locali per
  console (0x86f154 legge [0] o +0x19c se modo==4), +0x1a0 percorso.
- **raceMgr** = `system->[0x190]->[0x800]` (0x8ace28, 319 chiamanti): +0x50 n kart, +0x58 12 ptr kart,
  +0x17c s32[4] locale→giocatore (0x86ebf8), +0x1cc s32[12] locale→kart (0x86e8e4, clamp 12),
  tabelle per dispositivo +0x98/+0xa0/+0xa8 (7 voci).
- **Scene** = `system->[0x7f8]` (0x8ace60): +0x3c viewport 5 × 0x18, +0xb0 s32[4] slot→viewport,
  +0xcc modo split (0/1/2), +0xd4 tipo scena (tabella 0xf732dc, 7 casi).
- **InputSystem** (ctor 0x7e4554 → 0x7c53ec): lista dispositivi +0x190/+0x188/+0x18c; NpadDevice
  (0x7128 B, ctor 0x5fbce0) **già a 9 slot** {0..7,0x20}; 4 Controller (0x298 B, idx 0..3) + handheld (8).
- **Config menu** (singleton 0x12fae70 → +0x30): +0x1fa4 conteggio (int, mai confrontato con 4),
  +0x1fa8 modo.
- **Selezione personaggi**: classe base ctor 0x4bc844, Multi 0x4bcc4c (+0x8c0 = 4), alloc 0xa28
  (factory 0x4091d0/0x4091fc/0x409228); unico array per giocatore +0x904 stride 8 × 4.

## 2. Già pronto per 8 (nessuna modifica) [VERIFICATO]
| cosa | prova |
|---|---|
| Wrapper nn::hid: 9 npad id, 9 slot da 0xc40, setter `cmp #8`, join/merge/disconnect id ≤ 7 | 0x5fbdc8, 0x5fbde4.., 0x5fc290 |
| Vibrazione: 18 VibrationTarget (9 pad × 2), clamp 9 | 0x7ca0c0, 0x7cae18 |
| raceMgr locale→kart 12, pagine di gara per locale 0..11 | 0x86e8e4, 0x54fea8..0x5500a4 |
| settings: 12 entry kart, 16 entry, 5 console | 0x869180/0x869040 |
| Pagina numero giocatori: 7 pulsanti `L_Btn_00..06` (2–8) sempre creati | 0x4383c4 stato 1 |
| Config menu +0x1fa4: int, 25 lettori nessuno con `cmp #4` | fieldmap_401.txt |
| Risultati `L_Result_%02d` × 12, loghi vittoria per 12 kart | 0x54fea8 |
| Eden: `Player7/Player8` in hid_types.h, `player_0..7_*` in qt-config.ini | eden-repository |

## 3. Tabella consolidata delle modifiche
Tipo: C = costante, A = array fisso da rilocare, L = nuova logica (hook). Rischio B/M/A.

### Input / controller (step 3)
| indirizzo | funzione | cosa limita | tipo | dove | modifica | rischio | dip. |
|---|---|---|---|---|---|---|---|
| 0x7c54a4-ac | 0x7c53ec | `SetSupportedNpadIdType(0xf72e48,5)` | C | – | richiamare con 0xf554ac, 9 (hook post-ctor o IPS) | B | – |
| 0x7c54b0 | 0x7c53ec | `orr w1,wzr,#4` → 0x5fc290 pad interrogati | C | – | 8 (IPS 1 istr.) | B | – |
| 0x7c54e4-0x7c55bc | 0x7c53ec | loop 4 Controller (0x298 B, ctor 0xa7f1a0) + handheld | L | alloc 0x5faba8 | hook post-ctor: creare idx 4..7, registrarli in `[sys+0x190]` (capacità `[sys+0x18c]` da leggere a runtime) | M | – |
| 0x8bf158 | accessor | `cmp w19,#5`, array 5 ptr `[[0x1306e70]]+0x58` | A 5×8 | riempitore non trovato staticamente | rilocare a 9 o hook REPLACE; 185 chiamanti con idx ≤ 4 oggi | M | step 3 (GDB) |
| 0x8be248 / 0x86ebf8 | accessori | `cmp #3`; raceMgr+0x17c s32[4] | C + A 4 | ctor race-system | hook REPLACE delle 2 funzioni su tabella rilocata a 8 | M | layout raceMgr |
| 0x7c5794 / 0x7c5740 | raceMgr | tabelle +0x98/+0xa0/+0xa8 (7), loop srotolato 4 verso 0x7ce168 | A 7 + L | ctor raceMgr | tabelle ≥10, loop generalizzato | M | indice dispositivo = posizione in lista |
| 0x7caf0 / 0x7ce88 | accessori | tabella globale `[0x12fb230]` clamp 5 | A 6 | lazy init 0x7cb44 | estendere a 10 dopo verifica d'uso | B | – |
| 0x49ea60 | applet | 4 colori identificazione | dati | – | 8 colori (facoltativo) | B | – |
| 0x422efc (0x422f28-80) | selezione kart | abilita esattamente 4 controller (4 `cset hi`) | L | – | hook: loop 0..count | M | 0x8bf158 esteso |

### Menu / flusso / selezione personaggi (step 4–5)
| indirizzo | funzione | cosa limita | tipo | dove | modifica | rischio | dip. |
|---|---|---|---|---|---|---|---|
| 0x4d9460-90 | 0x4d92e8 | id 0x17..0x1a → dialogo | L | – | hook: per 0x17..0x1a eseguire il ramo 0x4d9774 con count = id-0x13 e modo multi | B | – |
| 0x4d9894 | 0x4d92e8 | `cmp w21,#4; b.gt` | C | – | 8 | B | – |
| 0x4d99a8 | 0x4d92e8 | `str count,[cfg+0x1fa4]` + 0x869180(raceCfg,count,1,0) | – | – | nessuna | – | – |
| 0x4bcc70 | ctor Multi 0x4bcc4c | `+0x8c0 = 4` slot | C | factory size 0xa28 | 4 → count (5..8) | B | – |
| 0x4091d0/1fc/228 | factory pagine | alloc 0xa28 | A | 0x5902d0 | 0xa28 → 0xa68 (+4 slot × 8) con array in coda | B | – |
| +0x904 (17 clamp + 20 accessi, 30 funzioni) | classe selezione | array 4 × 8 | A | ctor 0x4bc844 | rilocare in coda oggetto (8 voci): ~37 siti meccanici (patch IPS degli offset/clamp o hook) | M | 95 `cmp #4/#5` da rivedere uno a uno |
| 0x4be854 / 0x12797f0 | pagina layout CharaParts | 3 voci (Single/WiFi/Multi) | L | – | hook: Multi con count>4 → `mn_Page_Lyt_CharaParts_6pl/8pl` (stringhe fornite dal modulo) | M | RomFS |
| 0x4bee60 | `L_CameraPlay_00..03` | 4 nomi, null-safe | – | – | nessuna (assenti nei layout 6pl/8pl) | B | – |
| 0x4bd9dc (0x4bd9a4) | animazioni finestra per slot | pane NULL per slot ≥ 4 nel layout Multi | L | – | con layout 8pl le finestre `N_Window_00..07` esistono; verificare nomi cercati | M | step 5 |
| 0x4b11cc / 0x4b19a4 | finestra selezione | array +0x218 (stride 8) e +0x2a8 (stride 16) di un altro oggetto | A? | – | da verificare allo step 5 | M | – |

### Gara: viewport / camere / HUD / pausa (step 2, 6, 7)
| indirizzo | funzione | cosa limita | tipo | dove | modifica | rischio | dip. |
|---|---|---|---|---|---|---|---|
| 0x7e7760-6c | 0x7e76b8 | modo max 2, slot max 4 | L | Scene | hook: n 5-6 → modo 3 (6 slot), 7-8 → modo 4 (8 slot) | B | tutto |
| 0x7e7844, 0x7e78e0 | 0x7e76b8 | 5 viewport (`cmp #5`) | A Scene+0x3c 5×0x18 | ctor Scene (da trovare: scrive manager+0x7f8) | rilocare la Scene (allocarla più grande) e spostare l'array; **98 siti** usano passo 0x18 / `cmn #-1; cmp #5` | **A** | 1 |
| 0x7e7b14 (×5) | 0x7e7af8 | 4 slot (`cmp #4; csel`) | A Scene+0xb0 s32[4] | idem | tabella a 8 (rilocata) | M | 2 |
| 0x86eb2c, 0x86eb94, 0x86ebf8 | accessori slot locale | `slot<=3` | C | – | `cmp #7` (IPS) | B | 3 |
| 31 lettori di +0xcc | vari | confronto con 0/1/2 | L | – | per modi 3/4 quasi tutti cadono nel ramo 4P (ok); 0x54798c/0x548d20 → hook replace con nomi 6P/8P | B | RomFS |
| 0x7e9c0c | loop Lakitu 0x7e9b04 | 4 viewport | C | – | `cmp #8` | B | 2 |
| 0x536a64/8c, 0x531c58 | HUD comune / pausa | partizioni per modo (`cmp modo,#1`) | L | – | hook: usare `P_Partition_y3*/y4*` | B | layout |
| 0x7e8bd0 (+4 fn) | preset camera | 2 set FOV (0x1358e40, init 0x7e98a8) | tabella | – | +2 set per 6/8 viste (estetico) | M | 1 |
| enum pagine 103-106 / 108-111 | factory pagine | 4 RaceView + 4 RaceViewFront fisse | tabella | factory | +4+4 pagine (o riuso) | **A** (framework) | 1,3 |
| 0x53c838 | 0x53c818 | 2 layout pausa | L | – | terzo nome `rc_Page_RacePause_SgOunce` + 8 pulsanti | M | step 7 |
| 0x582c94 | wrapper nvn viewport/scissor | geometria non in rodata | – | – | hook di log allo step 2 per ricavare i rettangoli | – | – |

### Core gara (nessuna modifica strutturale)
| indirizzo | cosa | stato |
|---|---|---|
| settings+0x180/+0x188 | scrittore non raggiunto via accessor (puntatore membro o vfunc 0x3df768/0x3df78c) | da verificare con watchpoint in Eden (step 4) |
| item | per kart (12) nella mappa/TV; slot per vista nell'HUD | step 6 |
| audio | nessun import listener: motore interno; [IPOTESI] listener per viewport nei 98 siti a passo 0x18 | step 6 |
| replay/ghost | solo stringhe UI, nessun array 4 trovato | rischio basso |
| LAN | conteggio locale nei pacchetti non tracciato | fuori scopo, rischio alto se usato |

## 4. Conteggio e stima
| tipo | n. siti | note |
|---|---|---|
| costanti (IPS o hook 1 istr.) | 9 | 0x7c54b0, 0x4d9894, 0x4bcc70, 0x86eb2c/94/f8, 0x7e9c0c, tabella npad, 0x8be248 |
| array da rilocare | 6 strutture | Scene viewport (5→9) + slot (4→8); controller ptr (5→9); raceMgr +0x17c (4→8) e +0x98/a0/a8 (7→10); selezione +0x904 (4→8); globale 0x12fb230 (6→10) |
| siti meccanici che seguono gli array | ~98 (Scene) + ~37 (selezione) + ~185 (0x8bf158, già ok se l'accessor viene sostituito) | il grosso del lavoro e del rischio |
| nuova logica (hook) | ~12 | modi 3/4, nomi layout gara/pausa/selezione, partizioni HUD, controller 4..7, loop 0x422efc, ramo 5-8 menu, camere fantasma, +pagine RaceView |
| non determinabile staticamente | 4 | ctor Scene, riempitore array controller +0x58, scrittore settings+0x180, geometria viewport (0x582c94) → GDB/hook di log |

## 5. Verdetto go/no-go
**GO, con due condizioni.** Il modello "estendere, non riscrivere" regge: il livello hid è già a 8+1, le
tabelle di gara sono a 12, la pagina numero giocatori ha già i pulsanti, il conteggio è un int. I limiti
sono costanti e sei array fissi; la logica nuova è limitata a mappe (modo→layout, modo→partizioni).
Condizioni:
1. **Scene**: rilocare l'array viewport (5→9) implica toccare ~98 siti che iterano a passo 0x18 con
   `cmp #5`. Alternativa da provare nello step 2: tenere i 5 viewport e aggiungere 4 "viewport
   extra" in un array parallelo gestito dal modulo, intercettando solo il loop per vista (0x7e9b04) e
   il renderer; se il renderer per vista è troppo intrecciato, lo step 2 lo mostra subito.
2. **Pagine HUD fisse** (4 RaceView + 4 Front): il framework pagine ha un enum e una factory; aggiungere
   8 pagine è un intervento sul framework. Alternativa: istanziare le pagine extra dal modulo riusando
   la classe RaceView (stesso ctor) senza passare dall'enum.
Rischi residui: prestazioni (8 viste su Switch 1), LAN non supportato, `L_CameraPlay` assenti nei
layout 6pl/8pl (già null-safe), scrittore di settings+0x180 da trovare a runtime.

Ordine consigliato: step 2 (prototipo viste con camere su CPU: risolve le due condizioni) → step 3
(input: tutto locale al livello di gioco) → step 4 → step 5 → step 6 → step 7.
