# Step 1 – sottosistema gara: viewport, render target, telecamere, HUD, mappa, pausa, risultati

Main 4.0.1 (BID 158C0C73…), offset = main decompresso. Analisi statica; nessuna modifica al gioco.
Dati grezzi in `step1_data/view_*.txt` e `view_render_loop.asm`. Strumenti nuovi: `tools/sitectx.py`
(funzione + istruzioni seguenti + stringhe per una lista di siti).

## 0. Oggetti chiave [VERIFICATO]

| oggetto | come si ottiene | campi rilevanti |
|---|---|---|
| **Scene** (oggetto "split") | `0x8ace60()` = `0x8aba80()->+0x7f8` (manager globale 0x12fb388→+0x28→slot) | +0x3c array **5 × 0x18** viewport {+0 slot locale, +4 attivo, +8 tipo (1 = giocatore, altro = extra), +0xc ?, +0x10 kart}; **+0xb0 tabella 4 × s32** slot locale → indice viewport (−1 = nessuno); +0xcc modo split 0/1/2; +0xd0 1/2; +0xd4 tipo scena |
| **RaceConfig** | `0x8ace28()` = `…+0x800`; anche `0x7e7404()->+0x10` | +0x8 modo; +0x44.. 12 kart × 0x1c; +0x180 n locali; +0x188 array 5 × u32; +0x19c; +0x1cc array 12 (kart i → slot locale, −1) |
| Preset telecamera | globale 0x1358e40 (bss), init in `0x7e98a8` | +0x14..+0x1c e +0x24..+0x60: coppie di float (40,60 / −10,… / 75,50 / 55,60 / 33,55 / 65,22 / 18,33 / 22,65 / 65,27 / 30,60): FOV/altezze; **+0x38 (modo ≠1) vs +0x50 (modo 1 = 2P)** scelti in `0x7e8b9c/0x7e8c54/0x7e8dc0/0x7e8e60/0x7e925c` |

`0x7e7af8` (helper chiamato dopo ogni viewport in `0x7e76b8`/`0x7e7950`): azzera +0xb0[0..1], poi per
ogni viewport i (0..4) con flag attivo e slot ≠ −1: `cmp slot,#4; csel` → `+0xb0[slot] = i` (0x7e7b14-
0x7e7b20, ripetuto per i 5 ingressi a 0x7e7b24.., 0x7e7b4c.., 0x7e7b74.., 0x7e7b9c..). **Due limiti
indipendenti: 5 viewport, 4 slot locali.**

## 1. Dal modo split ai rettangoli di schermo

### 1a. Chi legge il modo (+0xcc): 31 siti [VERIFICATO] (`view_split_readers.txt`)
| funzione | uso del modo | sottosistema |
|---|---|---|
| 0x54798c, 0x548d20 | modo 0/1/2 → `rc_RaceView_{1P_00,2P_Ml,4P_Ml}` / `_Front_*` (altro → "" 0xed6b28); raggiunte **solo via vtable** (slot 13 di 0x1289240 e 0x1289538, classi RaceView/RaceViewFront, 93 entry) | HUD: nome layout |
| 0x536a10, 0x536a88 | `rc_RaceView_Cmn_00`: sceglie `P_Partition_x`/`P_Partition_y`, `L_TVMap`, `L_RaceNum_00`, `L_Timer_00`, `L_BtTimer_Ml` in base a modo (==1 → partizione x) | HUD comune (separatori, mappa TV) |
| 0x53198c | pausa: `N_Pause_00`, `P_Partition_x/y`, `L_Timer_00` per modo | pausa |
| 0x547d20, 0x5486c4, 0x548e2c, 0x550438, 0x547f78 | pagine RaceView: posizionamento HUD per modo (`L_ItemBox_00`, `L_Rank_00`, `L_Coin_%02d`, `L_CameraPlay_00..03`, `L_Finish_00`, `L_BtScore_00`) | HUD per vista |
| 0x7e8b9c, 0x7e8c54, 0x7e8dc0, 0x7e8e60, 0x7e925c | modo==1 → preset +0x50 altrimenti +0x38 del globale 0x1358e40, scritti in `[obj+0x160]+0x138..0x14c` | telecamera (FOV per aspetto 2P) |
| 0xe0d14, 0x118104, 0x119978, 0x7db7ec | modo==0 / ==1 → costanti float (0xed599c: 10000, 90000, 1.2, 85, 0.85, 57.29, 110, 130; 0xed5dcc..) | telecamera/effetti (distanze, FOV, 1P/2P) |
| 0x884ec, 0xcf640, 0x1ee78c, 0x294b00, 0x534bb4, 0x534ccc, 0x5504f4, 0x108a2c, 0x328104, 0x39d4c0, 0x50f38c, 0x5198e0, 0x52759c | `cmp #1/#2` → rami 2P/4P (audio, effetti, risultati `L_Result`, battle `TeamColor`) | vari |

Tutti i siti confrontano con 0, 1 o 2: **un modo 3/4 cade sempre nel ramo "altro"** (per lo più quello
del 4P), tranne 0x54798c/0x548d20 che restituiscono stringa vuota.

### 1b. Dove nascono i rettangoli [IPOTESI con ancoraggi verificati]
- Le chiamate NVN sono indirette: `nvnCommandBufferSetViewport` / `SetViewports` / `SetScissor` sono
  risolte in `0x5856cc..0x585730` e salvate in `[[0x130b2b8]]`, `[[0x130b2c0]]`, `[[0x130b2c8]]`
  [VERIFICATO]. L'unico lettore di `[0x130b2b8]` fuori dal loader è `0x582c94` (wrapper viewport, 2
  chiamanti: 0xa83d24, 0xa83eac = livello gfx/agl), `[0x130b2c0]` anche da `0x61e1c8` [VERIFICATO].
- Nessuna costante 1280/720/640/360 come immediato `mov` (0x44a00000: 0; 0x44340000: 4; 0x44200000: 1;
  0x43b40000: 4) e nessuna tabella di rettangoli normalizzati trovata in rodata: [IPOTESI] i rettangoli
  per vista sono calcolati dal gfx layer a partire da (modo, indice viewport) con frazioni ½/¼ costruite
  via `fmov` immediati, nel codice 0xa83xxx. **Verifica proposta per lo step 2**: hook di log su
  `0x582c94` (x/y/w/h per vista) e sul wrapper scissor, in una gara 2P e 4P: è più rapido e sicuro che
  continuare in statico.
- Render target: non individuati staticamente (allocazioni via agl, nessuna stringa). [IPOTESI] un
  unico color/depth target 1280×720 con viewport+scissor per vista (coerente con `P_Partition_*` nel
  layout comune e con il fatto che il 2P usa FOV diversi invece di target diversi).

### 1c. Loop di rendering per vista [VERIFICATO]
`0x7e9b04` (chiamata da 0x86cb5c): per ogni oggetto della lista `[this+0x40]` (count `[this+0x38]`),
per ogni viewport `i` in **0..3** (`cmp x25,#4` a 0x7e9c0c) con tipo (+0x44)==1 e flag attivo: cerca
l'oggetto con `[obj+0x169] == viewport.kart (+0x4c)` e chiama `vfunc+0xb8(slot)`. Gli oggetti sono
creati da `0x110298` (`Jugem`, alloc 0x3d0 a 0x1102a8, ctor `0x7e80ec` che copia indice e kart in
+0x168/+0x169): sono i Lakitu per vista, keyed per kart. Altri 97 siti iterano i viewport con stride
0x18 e leggono +0x40 (`view_vp_iter.txt`): 0x3cbd94 e 0x15e5e0 (4 ciascuno), effetti/suoni
(0x3d01c.., 0x107238.., 0x14ee70 `HandleELink::fade`), HUD oggetti (0x510460 `N_ItemCount_All_00`),
0x7d2610, 0x80d670, 0x7ac59c… tutti con il pattern `cmn #−1 / cmp #5` (clamp a 5 viewport).

## 2. Telecamere
- Per vista non esiste un array esplicito nel main che io abbia individuato: la telecamera è parte
  dell'oggetto view/giocatore (+0x160 → blocco con +0x12c..0x14c FOV/preset) [VERIFICATO per i campi,
  IPOTESI per la proprietà].
- Classi con nome: `RecorderKartCamera`, `ReplayCamera`, `IntroCamera`, `CameraEngine` [VERIFICATO,
  stringhe]. `ReplayCamera`/`Race_Watch` indicano che una telecamera può seguire un kart qualsiasi:
  per le viste 5-8 "fantasma" dello step 2 conviene riusare il percorso Watch/Replay (telecamera su kart
  CPU) [IPOTESI].
- Preset FOV 1P/2P: nuovo aspetto 6P (424×360 ≈ 1.18) e 8P (320×360 ≈ 0.89): serve un terzo/quarto set
  nel globale 0x1358e40 e un ramo in 0x7e8b9c & co. (5 funzioni, stesso pattern `cmp w8,#1; csel`).

## 3. HUD
- Le viste HUD sono **pagine** dell'enum pagine: `RaceView0..3` (indici 103-106) e
  `RaceViewFront0..3` (108-111) [VERIFICATO, `img/pages.txt`]: 4 istanze fisse per tipo, con vtable
  0x1289240 (RaceView) e 0x1289538 (RaceViewFront), 93 entry ciascuna. Le pagine sono create dalla
  tabella pagine del framework (come `CharaParts`, 0x4be854): per 6/8 viste servono **4 enum e 4
  istanze in più** (o riuso dinamico), più l'allocazione del page object.
- Per vista: `0x547f78` (ItemBox/Rank, loop `L_CameraPlay_00..03`), `0x5486c4` (Coin `L_Coin_%02d`,
  BtScore, Fade), `0x548e2c`/`0x549044` (Finish), `0x550438` (SE goal). Scala/posizione per modo via le
  partizioni del layout comune (`P_Partition_x/y`, nel 4.0.1 anche `P_Partition_y3l/y3r/y4l/y4r`
  [VERIFICATO in stepA3]), che il codice 0x536a88 sceglie con `cmp modo,#1` → serve estendere la scelta.
- Mappa: `L_TVMap` nel layout comune (0x536a88); icone per giocatore locale non isolate (ipotesi:
  nel RaceView comune, non per vista).
- Pagine risultato: `L_Result_%02d` (0x53f3b4, 0x541e1c, 0x542024, 0x542214; stride 0xe8, 12 righe =
  classifica kart, non giocatori locali), `L_Screen_%02d` (0x528e14, stride 0x360) [VERIFICATO xref;
  il bound del loop non è un `cmp #4` esplicito: da verificare a runtime].
- `WinLogo/FinishLogo` 0x54fea8: cicla **i kart 0..11** chiamando `0x86e8e4(i)` (kart → slot locale)
  e `0x852d1c(i)` → nessun limite 4 qui [VERIFICATO].

## 4. API "slot locale" (0x86e800–0x86fa00) [VERIFICATO, `view_accessors.txt`, `view_localslot_callers.txt`]
| accessor | limite | chiamanti | uso |
|---|---|---|---|
| 0x86eb1c(slot) | `slot<=3`, `+0xb0[slot]<=4`, poi viewport.kart | 34 | kart del giocatore locale `slot` (HUD 0x5198e0/0x519724/0x51fc04, menu 0x4f9ccc.., audio 0x3bc010 ×12, 0x869a14 ×12, effetti 0x27418/0x27b34/0x283a4 ×4, 0x14d0c0 ×4) |
| 0x86eb84(slot) | `slot<=3` → `+0xb0[slot]` | 2 | indice viewport del locale |
| 0x86ebf8(slot) | `slot<=3` | 12 | ? (stesso pattern) |
| 0x86e894(i) | `i<4`, legge viewport +0x3c/+0x40 | 1 | |
| 0x86e93c/970/9b8/a04/a50 | 5 funzioni "viewport k" (cmp #1/#3) | 0 (vtable) | |
| 0x86edb4 / 0x86f074 | `cmp #5, #0xc, #4` su RaceConfig (+0x180, +0x198, +0x44..) | 1 / 26 | kart ↔ locale |
| 0x86ef2c | `cmp #4` ×3 | 15 | |
| 0x86f154 | n locali: `[cfg+0x8]==4 → +0x19c` altrimenti `+0x188[clamp 5]` | 1 (0x7e76b8) | |
| 0x86f384/0x86f3e4 | `cmp #3/#2/#1/#0` → switch a 4 casi | 8 / 13 | |
| 0x86f624 | `cmp #8` | 7 | (8 = già esistente: team/online?) |
| 0x86e8e4(i) | `i<=0xb` → `+0x1cc[i]` | 39 | kart i → slot locale |

I controller per giocatore (`0x8be03c..0x8be9d0`, 12 funzioni, tutte chiamanti di 0x86eb1c) sono nel
sottosistema input (altro fork).

## 5. Pausa e risultati
- `0x53c818`: sceglie `rc_Page_RacePause` / `_Sg` (`[obj+0x24]&0x10`, `[obj+0x8]==2`) [VERIFICATO];
  `rc_Page_RacePause_SgOunce` (con `L_SwitchBtns8pl_00..07`) non è nominato: per 5-8 serve un terzo
  letterale e la logica dei pulsanti per 8 (nel layout Ounce i pane sono `L_SwitchBtns8pl_00..07`).
- `0x53198c` pausa: partizioni per modo (`cmp modo`) → estendere a 3/4.
- Risultati: `L_Result_%02d` = 12 righe per kart; i giocatori locali sono evidenziati tramite
  `0x86e8e4(kart)` (ritorna slot ≥0) → nessun array a 4 individuato qui [VERIFICATO parziale].

## 6. Proposta per i modi 3 (6P) e 4 (8P)
1. `0x7e76b8`: dopo `cmp w24,#2` aggiungere `n in 5..6 → modo 3, 6 slot` e `n in 7..8 → modo 4, 8
   slot`; estendere l'array viewport da 5 a 9 (5 + 4) **rilocando** la Scene (dimensione ignota: va
   letta dal suo allocatore; hook sul ctor) o spostando l'array in memoria nostra e reindirizzando i 98
   siti con stride 0x18 (troppi: meglio rilocare l'oggetto con padding in coda se il layout lo permette;
   i campi dopo +0xb0 vanno ricontrollati).
2. `+0xb0`: tabella 4 → 8 (0x7e7af8 ×5, 0x86eb1c, 0x86eb84, 0x86ebf8 e i ~20 lettori `cmp #5`): tutti
   pattern identici `cmp #3`/`cmp #4; csel`: patch IPS di costanti dove l'array sta in coda, altrimenti
   hook.
3. `0x54798c`/`0x548d20`: hook di sostituzione: modo 3 → `rc_RaceView_6P_Ml`/`_Front_6P_Ml`, modo 4 →
   `rc_RaceView_8P_Ml` (Front 8P non esiste nel RomFS: usare `_Front_6P_Ml` o 4P [IPOTESI]).
4. `0x536a88`/`0x53198c`: partizioni `P_Partition_y3l/y3r` (6P) e `y4l/y4r` (8P) + x.
5. Pagine: 4 enum `RaceView4..7` e `RaceViewFront4..7` nella tabella pagine (hook sulla factory) oppure
   riuso ciclico delle 4 pagine con viewport diverso (da misurare).
6. Telecamere: preset 0x1358e40 +2 set, ramo nelle 5 funzioni 0x7e8b9c..; per le viste 5-8 dello step 2
   telecamere Watch/Replay su kart CPU.
7. Geometria: RootPane 424×360 (6P) e 640×720 (8P, autorato a 2×) → rettangoli 3×2 di 426.67×360 e 4×2
   di 320×360 [IPOTESI: da confermare con il log di 0x582c94].

## 7. Tabella riassuntiva
| indirizzo | funzione | cosa limita | tipo | allocazione/ctor | modifica proposta | rischio | dipendenze |
|---|---|---|---|---|---|---|---|
| 0x7e7760-0x7e776c | 0x7e76b8 | modo split max 2, slot max 4 | logica | Scene (0x8ace60) | hook: modi 3/4 | basso | tutto il resto |
| 0x7e7844, 0x7e78e0 | 0x7e76b8 | 5 viewport (`cmp #5`) | array fisso Scene+0x3c ×5 ×0x18 | ctor Scene (da trovare: scrive +0x7f8 del manager) | rilocare Scene o array | alto (98 siti) | 1 |
| 0x7e7b14.. (×5) | 0x7e7af8 | 4 slot (`cmp #4`) | array fisso Scene+0xb0 ×4 | idem | tabella 8 | medio | 2 |
| 0x86eb2c, 0x86eb94, 0x86ebf8 | accessori | `slot<=3` | costante | – | IPS `cmp #7` | basso | 3 |
| ~20 siti `cmn #-1; cmp #5` | effetti/audio/HUD | 5 viewport | costante | – | IPS/hook | medio | 2 |
| 0x7e9c0c | 0x7e9b04 | 4 viewport nel loop Lakitu | costante | – | IPS `cmp #8` | basso | 2 |
| 0x5479b4.., 0x548d48.. | 0x54798c/0x548d20 | 3 nomi layout | logica | vtable slot 13 | hook replace | basso | RomFS 6P/8P |
| 0x536a64/0x536a8c, 0x531c58 | HUD comune / pausa | partizioni per modo | logica | – | hook | basso | layout `P_Partition_y3/y4` |
| 0x7e8bd0 (+4 fn) | preset camera | 2 set FOV | tabella 0x1358e40 | 0x7e98a8 | +2 set, ramo | medio (estetico) | 1 |
| enum 103-111 | pagine RaceView/Front | 4+4 istanze | tabella pagine | factory pagine | +4+4 enum o riuso | alto (framework) | 1,3 |
| 0x53c838.. | 0x53c818 | 2 layout pausa | logica | – | terzo nome `_SgOunce` + 8 pulsanti | medio | step 7 |
| 0x582c94 | wrapper nvn viewport | – | – | – | **hook di log (step 2)** | nessuno | – |

## 8. Verdetto parziale (solo questo sottosistema)
Il lato "scena" è trattabile: due costanti (5 viewport, 4 slot) replicate in pattern uniformi e tre
funzioni di scelta (modo, nome layout, partizioni). Le incognite che decidono il costo sono (a) la
dimensione/allocazione dell'oggetto Scene (array da rilocare) e (b) le pagine HUD, che il framework
istanzia da un enum fisso. Entrambe si chiariscono con due hook di log nello step 2 (ctor Scene via
0x8aba80/+0x7f8, factory pagine) prima di scrivere logica. Non trovato staticamente: il calcolo dei
rettangoli e i render target (gfx layer 0xa83xxx; verificare con l'hook su 0x582c94).
