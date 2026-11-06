# Step 1 – Oggetto di gara, giocatori locali, item, audio, replay (main 4.0.1)

Dati grezzi: `step1_data/core_writers.txt` (euristica larga), `core_writers2.txt` (tracciamento registri),
`core_86e8e4_callers.txt`, `core_8bf158_callers.txt`. Offset = main decompresso.

## 1. Catena di accesso [VERIFICATO]

*(estratto di disassemblato del gioco rimosso)*
0x86f7bc ha 1398 chiamanti, 0x8ace28 319: sono i due oggetti centrali della gara.

## 2. Impostazioni gara (settingsA/B) – reset 0x86ec8c [VERIFICATO]

Layout dal reset (`stp/stur` a 0x86ec9c–0x86eda4) e dagli accessi:

| offset | tipo | significato | prova |
|---|---|---|---|
| +0x00 | u32 | 0 (tipo gara; ==1 a 0x54ff10) | 86eca0 |
| +0x04 | u32 | 5 | 86eca0 |
| +0x08 | u32 | modo (==4 a 0x86f158/0x7e77f8/0x869e0c; 3 = ?, 5/7 = online a 0x139c98) | 86eca0, 3df7d0 |
| +0x10..+0x1c | u32×4 | 2,2,2,0 (classe/regole) | 86ecb4 |
| +0x24 | u16 | flag (bit4 letto da 0x7e7708) calcolato da 0x86ef2c dal modo | 86ef3c–86ef58 |
| +0x28..+0x43 | | 0x2b=3, 0x28/0x40 u16, 0x30..0x3f = -1 | 86ecbc–86ecdc |
| **+0x44 + i·0x1c, i<12** | entry kart | {+0 u32 4 (tipo: 4=vuoto), +4 u32 2, +8..+0x17 s32 -1×4 (personaggio/kart/gomme/ala), +0x18 u16 0}; cbnz su +0 = slot occupato (0x86f64c), `madd ×0x1c` con clamp 12 (0x7e7824) | 86ece0–86ed90, 7e7824–7e7838 |
| **+0x180** | s32 | numero giocatori locali umani (limite dei cicli 0x7e7794, 0x8722f0) | 7e7794, 8722f0 |
| **+0x188** | u32[5] | giocatori per "set di viste": 0x86f154 ritorna [+0x188+clamp5(0)·4] = [+0x188], oppure [+0x19c] (indice 4) se modo==4 | 86f154–86f190 |
| +0x19c | u32 | come sopra per modo 4; scritto dal menu (0x4fb020, da settingsB) | 4fb00c–4fb020 |
| +0x1a0 | u32 | id percorso (cmp 0x3f/0x5a/0x5e/0x65 a 0x1730e8/0x7e8400..); scritto da 0x41ff38, 0x4f9e54, 0x4fb274, 0x501c64/0x501cf0 (B), 0x3df7bc (=0x10) | core_writers2 |
| +0x1d8 | u8 | flag (0x173118) | |
| +0x1f8 | u32/u16 | (0x1c5638, 0x1e0754, 0x256684) | |

Nota [VERIFICATO]: l'ultima entry (i=11, 0x178..0x193) si sovrappone a +0x180/+0x188: il reset la scrive
diversa dalle altre (`stp x8,xzr,[0x178]`, `stp xzr,xzr,[0x188]`). [IPOTESI] la tabella utile è di 11
entry + campi conteggio, con clamp generico a 12 nel codice; da confermare in Eden leggendo settingsA
durante una gara (watchpoint su +0x180).

Scrittori di +0x180/+0x188 [NON TROVATI via accessor]: il tracciamento (core_writers2) mostra solo
scritture a +0x1a0/+0x19c/+0x8/+0x44/+0x178 attraverso settingsB; +0x180/+0x188 vengono scritti tramite
un puntatore conservato altrove o da una copia di struttura (nessuna `memcpy` vista con base 0x240/0x248).
[IPOTESI] la pagina di selezione giocatori/personaggi scrive settingsB tramite un membro; il sito si
trova con un watchpoint hardware in Eden su settingsB+0x180 (step 4). Le funzioni che scrivono
contemporaneamente +0x180/+0x188/+0x19c/+0x1cc su altri oggetti (0x64a00, 0x3f96b0, 0x8be478, ...) NON
chiamano l'accessor (verificato: `accessor_calls=0` per tutte e 16) e appartengono ad altre classi.

Copia A←B: non individuata staticamente; i chiamanti diretti di 0x7e7404/0x7e741c fuori dagli accessor
sono 22 (0x1082e4, 0x3a1f48, 0x3d5efc.., 0x3df754/78, 0x4db2b8/3ec/464, 0x4fb00c, 0x7e5340): leggono +0x8
o passano l'oggetto a callback virtuali (0x3df768 `blr [vt+0x38]`, 0x3df78c `[vt+0x48]`). [IPOTESI] la
copia avviene nel vtable chiamato a 0x3df768/0x3df78c; da confermare col debugger.

## 3. Gestore gara (raceMgr = 0x8ace28) [VERIFICATO]

| offset | tipo | significato | prova |
|---|---|---|---|
| +0x50 | s32 | numero kart in gara (confrontato con indici in 0x52d32c, 0x541c44) | 3fa00+8, 52d330 |
| +0x58 | ptr | array di 12 puntatori kart (`ldr x9,[x8,w0,sxtw #3]`), kart+0x4c, +0x68 campi | 869d08, 872320 |
| **+0x1cc + i·4** | s32 | kart del giocatore locale i (0x86e8e4: i≤11 altrimenti -1); inizializzato a -1 per 0x1cc..0x1f3 (10 voci) nel ctor 0x7eae0–0x7eb0c | 86e8e4, 7eafc |

Quindi la mappa "giocatore locale → kart" ha già 10–12 posti [VERIFICATO: clamp 12 e init 10×-1]:
NON è un limite a 4. I 39 chiamanti di 0x86e8e4 (core_86e8e4_callers.txt) passano un indice calcolato o
iterano sul conteggio (+0x180, 0x7e77ac, 0x8722f0) o su 12 (0x872334 `cmp x21,#0xc`, 0x3fa00, 0x3bc2ac,
0x5233ac...). Le 12 funzioni consecutive 0x54fea8…0x5500a4 (una per indice 0..11: `mov w0,#i; bl 0x86e8e4;
tbnz w0,#31; bl 0x852d1c(i)`, flag byte a pagina+0x70+i) dimostrano che la pagina di gara (region
0x54fxxx) è già scritta per 12 giocatori locali possibili. [IPOTESI] residuo della modalità "12 locali"
della versione Wii U/LAN; comunque utile: non va esteso.

Cicli con limite reale (sottosistema core):

| indirizzo | funzione | limite | tipo | modifica | rischio |
|---|---|---|---|---|---|
| 0x7e7844 `cmp x26,#5` / 0x7e78d8 `cmp w11,#5` | 0x7e76b8 split | 5 viewport (0x18 B a scena+0x3c) | array fisso nell'oggetto scena (ctor 0x7e7950: campi 0x3c..0xb0 inizializzati a mano) | rilocare: oggetto scena più grande (+3×0x18) o tabella esterna; estendere i 2 cicli | medio (altro fork: viewport) |
| 0x86f154 `cmp w8,#5` | n giocatori | u32[5] a settings+0x188 | indice sempre 0 o 4: nessuna estensione necessaria; il VALORE deve poter essere 5..8 | basso |
| 0x7e77c4/0x7e77a8 buffer stack sp+0xc (11 word) | 0x7e76b8 | 11 giocatori locali | buffer locale | nessuna (≥8) | – |
| 0x8722fc `cmp w8,#5` | 0x8722c8 | stesso u32[5] di +0x188 | come sopra | nessuna | – |
| 0x86e8e4 `cmp w19,#0xb` | locale→kart | 12 | nessuna | – |
| 0x8bf158 `cmp w19,#5` | controller del giocatore i | array 5 (altro fork: input) | rilocare a 8+1 | alto (30 chiamanti, core_8bf158_callers.txt: menu 0x4dc1a0–0x5034d4, pagine gara 0x51b8e8–0x54080c, framework 0x8b014c–0x8b4d68; passano w0 calcolato o 0) |

## 4. Item per giocatore [PARZIALE]
Le funzioni della pagina gara che partono dal kart del locale i (0x532b8c: `L_TVMap`, `L_CharaIcon_%02d`,
`L_ExploIcon_%02d`, `L_Prison_%02d`; 0x54cb44/0x53646c: `P_MapDummy_00`) iterano sui 12 kart e usano
0x86e8e4 per il locale: la mappa/TV è per gara, non per vista. Lo slot item per vista non è stato
individuato in questo sottosistema (atteso nell'HUD `rc_RaceView_*`, fork HUD). [IPOTESI] gli item sono
per kart (12) e l'HUD li mostra per vista: nessun array 4 lato oggetti.

## 5. Audio [VERIFICATO parziale]
Nessun import `nn::atk`/`nn::audio` di tipo listener nel main (`grep nn3atk main_imports.txt` = 0; le
stringhe atk trovate sono simboli dentro sdk.bin). Il motore audio è integrato nel main (sead/aal):
nessuna stringa "Listener" di gioco. [IPOTESI] gli ascoltatori sono per viewport e seguono l'array
viewport (5): da verificare nello step 2 cercando chi legge scena+0x3c per l'audio.

## 6. Vibrazione [VERIFICATO parziale]
17 siti `nn::hid::VibrationTarget::VibrationTarget()` + 1 `VibrationPlayer`, 2 `SetVibrationDeviceHandle`,
9 `Stop`. [IPOTESI] un target per controller (array parallelo ai controller, fork input); da estendere
con l'array controller.

## 7. Replay/ghost/salvataggio [IPOTESI]
Stringhe: `Race_Replay`, `ReplayCamera`, `rc_Page_RaceReplay`, `UIHeap_LoadGhost`, `L_GhostRace_00`.
Nessuna struttura per giocatore locale individuata; il replay MKTV registra i 12 kart. Rischio basso;
da verificare solo quando una gara a 8 arriva alla fine (highlight reel).

## 8. Rete/LAN [VERIFICATO]
`LocalPlayerNum` compare solo nella telemetria (0x3860fc, 0x388d14: play report). Il conteggio locale
nei pacchetti LAN/Pia non è stato tracciato: rischio alto se si usa LAN con >4 locali; fuori scopo
(obiettivo: una sola console).

## Tabella riassuntiva (sottosistema core)

| indirizzo | funzione | cosa limita | tipo | ctor/alloc | modifica | rischio | dipendenze |
|---|---|---|---|---|---|---|---|
| settings+0x180 | – | conteggio locali | valore (scritto dal menu, sito da trovare) | reset 0x86ec8c | nessuna struttura; serve che il menu scriva 5..8 | basso | step 4 |
| settings+0x188[5] (0x86f154, 0x8722fc) | n per set viste | 5 set | array fisso ma indice 0/4 | 0x86ec8c | nessuna | basso | – |
| raceMgr+0x1cc[12] (0x86e8e4) | locale→kart | 12 | array fisso | 0x7eae0 | nessuna | – | – |
| 0x54fea8…0x5500a4 | pagina gara per locale i | 12 stub | logica già a 12 | – | nessuna | – | – |
| scena+0x3c[5] (0x7e7844, 0x7e78d8) | viewport | 5 | array fisso in oggetto scena | 0x7e7950 | rilocare a 8 + estendere modi | medio | fork viewport |
| 0x8bf158 | controller per giocatore | 5 | array fisso | fork input | rilocare | alto | 30 chiamanti |
| settings entry kart (+0x44, 12×0x1c) | kart in gara | 12 | array fisso | 0x86ec8c | nessuna | – | – |

Verdetto parziale (core): il nucleo dati di gara (kart, mappa locale→kart, pagina gara) è già
dimensionato a 12; i veri limiti sono fuori da questo sottosistema (viewport 5, controller 5, UI).
Resta da trovare in Eden chi scrive settings+0x180/+0x188 (watchpoint) e la copia B→A.
