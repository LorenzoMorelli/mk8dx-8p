# Step 1 – catalogo "max 4 giocatori locali": INPUT / CONTROLLER / VIBRAZIONE / APPLET

Main 4.0.1 (BID 158C0C73…), offset nel main decompresso. Analisi statica, nessuna modifica al gioco.
Dati grezzi: `step1_data/input_*.txt` (disassemblati delle regioni citate, elenco chiamate nn::hid).

## 1. Architettura (tre livelli) [VERIFICATO]

```
InputSystem (ctor 0x7e4554 -> 0x7c53ec)                 lista dispositivi [+0x170] (0x5faf8c),
 ├─ Touch        0x2940 B, ctor 0xa7ff0c                  array [+0x190], count [+0x188], cap [+0x18c]
 ├─ NpadDevice   0x7128 B, ctor 0x5fbce0  <-- wrapper nn::hid, GIÀ a 9 pad (0..7 + handheld)
 ├─ Controller#0..3  0x298 B, ctor 0xa7f1a0(sys, idx=0..3)   idx in [+0x178]  (loop 0x7c54e4, cmp w24,#4)
 ├─ Controller#H     0x298 B, ctor 0xa7f1a0(sys, idx=8)      handheld (0x7c55c0, w2=8)
 └─ Misc         0x178 B, ctor 0x6254cc ([+0x138]=0x12)
```

### 1a. NpadDevice (0x5fbce0, oggetto 0x7128 B) – livello nn::hid, già dimensionato per 8+1
| offset | contenuto | prova |
|---|---|---|
| +0x28 | numero di pad interrogati (1..8); setter 0x5fc290: `cmp w1,#8; b.hi` → `str w1,[x0,#0x28]` | 0x5fc290 |
| +0x2c | hold type | 0x5fc2a0 |
| +0x30..+0x50 | stile corrente per slot 0..8 (int) | 0x5fbddc.. `str w21,[x20,#0x30+4i]` |
| +0x90, +0xcd0, +0x1910, +0x2550, +0x3190, +0x3dd0, +0x4a10, +0x5650, +0x6290 | 9 slot da 0xc40 B (stato npad, six-axis handle +0x80/+0x88, vibration handles +0x84/+0xc90) | memset 0x5fbde4..0x5fbee4, stride `mov w25,#0xc40` a 0x5fc070 |
| +0x6e98.. | code vibrazione / mutex | 0x5fbd14.. |

- Init 0x5fbd a8: `InitializeNpad`; `memcpy(sp, 0xf554ac, 0x24)` + `SetSupportedNpadIdType(ids, 9)` (0x5fbdc8): **{0,1,2,3,4,5,6,7,0x20}**; `SetSupportedNpadStyleSet(0x1f)` (0x5fbeec).
- Polling 0x5fc03c: ciclo `w22 = 0 .. [x19+0x28]` (0x5fc260-0x5fc26c, `b.ls`): per `w22 == count` usa id 0x20 e slot 8 (`csel w8,w24(0x20),w22,eq` / `csel w20,w21(8),w22,eq` a 0x5fc084/0x5fc08c); slot clamp `cmp w20,#9` (0x5fc09c). Quindi con count=4 interroga pad 0..3 + handheld; con count=8 interroga 0..7 + handheld **senza altre modifiche**.
- Vibrazione per pad 0x5fc37c: `cmp w1,#9` slot, `cmp w2,#2` lato (0x5fc394-0x5fc3a8); `SendVibrationValue` a 0x5fc510.
- Assegnazione joy-con: 0x5fc2ac/0x5fc2d8 (`SetNpadJoyAssignmentModeSingle`, `cmp w1,#7`), 0x5fc308 (`Merge…`, `orr w8,w2,w1; cmp #7`), 0x5fc350 (`DisconnectNpad`, `cmp #7`): tutte accettano id 0..7.
- [VERIFICATO] Chiamanti di 0x5fc290 (max pad): **solo 0x7c54b8** con `orr w1,wzr,#4`.

### 1b. Controller di gioco (0x298 B, ctor 0xa7f1a0)
- `str w19,[x20,#0x178]` = npad id/indice (0..3, 8=handheld); `[+0x17c]`,`[+0x17d]=5`,`[+0x138]=0x11` (0xa7f1c0-0xa7f1dc).
- Update legge lo slot del NpadDevice con il proprio id: 0x7c7b84 `ldrsw x8,[x20,#0x178]; cmp w8,#9; csel; madd x8,x8,#0xc40,x19` → slot[+0x78] stile. Nessun'altra tabella: **indice controller = npad id** (8 → slot 8 = handheld).
- Stile → tabella di salto 0xf72e70 (`cmp w21,#4` a 0x7c7bb8: 5 stili, non un limite giocatori).
- Handheld: 0x7c9040-0x7c9084 `SetNpadHandheldActivationMode(race.[+0x178]==1 ? 2 : 0)` guidato da `[x19+0x39d]`.
- Swap: 0x7c64b8-0x7c654c cerca nella lista `[sys+0x190][0..count)` il controller con `[+0x178]==id` e chiama `SwapNpadAssignment` (0x7c653c).

### 1c. Accessori di gioco (limiti duri)
| sito | codice | limite |
|---|---|---|
| 0x8bf158 | `bl 0x8bf09c` (= `[[0x1306e70]]+0x58` = array controller) ; `cmp w19,#5; csel x8,x8,xzr,lo; ldr x0,[x0,x8,lsl#3]` | **array di 5 puntatori** (0..3 pad, 4 handheld); indice ≥5 → entry 0 |
| 0x8be248 | `ldr w0,[x0,#8]; cmp w0,#3; b.hi fail` → `bl 0x86ebf8` → `race.[+0x58][idx]→[+0x200]` (controller del giocatore) | **indice locale ≤3** |
| 0x86ebf8 | `cmp w19,#3; b.ls` → `race.[+0x17c + i*4]` | tabella locale→giocatore **4 int** |
| 0x7caf0 / 0x7ce88 | `cmp w19,#5; b.ls` (indice dispositivo 0..5) su tabella globale `[0x12fb230]` | 6 voci |
| 0x7c5794 | azzera 7 voci di `race.[+0x98]` (int), `[+0xa0]` (byte), `[+0xa8]` (int) indicizzate per **indice dispositivo** (0x619348: posizione del controller nella lista `[sys+0x190]`), poi assegna i giocatori 0..3 (`cmp [x19+0x50],#2/#3/#4` → 0x7ce168) | 7 voci; loop srotolato a 4 |
| 0x7c5740 | `race.[+0xa0][dev]=1`, `[+0xa8][dev]=0xf` con dev = 0x619348(sys, ctrl) | idem |
| 0x7c6550 | controller del giocatore: `player.[+0xb0]` = indice dispositivo, confrontato con `[sys+0x188]` | – |

### 1d. Applet controller (0x49ea60) [VERIFICATO]
Argomenti (w0=min, w1=max, w2=permit_joy_dual, w3=?) → struttura `ControllerSupportArg` sullo stack:
`[+0]=min` `[+1]=max` `[+2]=1 take_over` `[+3]=1 left_justify` `[+4]=1` `[+5]=(max==1) single_mode`
`[+6]=1 identification_color` `[+7..+0x16]` **4 colori** da 0x1341f78 (`ldp w9,w10,[x8,#0x28]` / `[x8,#0x30]`, 0x49ebe8-0x49ec1c), `[+0x1f]` (config+0x1f99); poi `SetNpadJoyHoldType(1)`, `race.[+0x210/+0x211]`, 0x8ab9e8 → thread applet → `ShowControllerSupport` a 0x86105c (arg a obj+0xf5, esito in `[obj+0x311]` ok, `[obj+0x314]` = player count restituito).
Chiamanti: 0x492390 `(w22,w22,1,0)` con w22 = count; 0x4baa6c `(w22,w22,?,0)`; 0x4dc460 `(w21,w21,?,0)`; 0x49ca2c/0x49cc38 `([x19+0x1628], idem, 0, 0)` (singolo giocatore). Con count 5–8 min/max ≥5 sono già passati come interi: l'applet NON è il collo di bottiglia, lo è il loop a valle (0x4ba77c, step 4). I colori di identificazione coprono solo 4 giocatori (5–8 → 0 = nero).

### 1e. Vibrazione [VERIFICATO]
- Manager: ctor 0x7ca0c0 costruisce **18 `VibrationTarget`** (stride 0x48, +0x0..+0x4c8) = 9 pad × 2 lati.
- Collegamento handle 0x7cae18: `cmp w23,#9` (pad), target = `pad*2 (+lato)` con `cmp w8,#0x12; csel` (0x7cae5c) a `[x19+0x48d8]`; handle dal NpadDevice slot `[+0xc90]`.
- Node/Player/Writer (0x7cc360-0x7ccf70): per controller, nessun confronto con 4.
→ nessuna assunzione "4 giocatori": è per pad, già a 9.

### 1f. Confronti duri nella regione controller 0x7c5000-0x7d0000
`cmp #3/#4/#5/#8`: 0x7c7e0/0x7c894/0x7c914 (accessori config `modo == N`, non limiti), 0x7cb10/0x7cea4 (clamp indice dispositivo 5), più 0x7c55b8 (`cmp w24,#4` loop costruzione controller), 0x7c5574/0x7c564c/0x7c56b4 (`cmp count,cap` lista dispositivi).

## 2. Eden: più di 4 controller [VERIFICATO]
- `hid_core/hid_types.h:214-215`: `Player7 = 0x6, Player8 = 0x7` (+ Handheld 0x20): l'HID emulato gestisce 8 pad.
- `qt-config.ini`: profili `player_0..player_7` con `player_N_connected`, `player_N_type` (righe 235-362). Attuale: player_0..4 connessi, 5..7 no. Per lo step 3: `player_N_connected=true` (+ `\default=false`) per N=4..7 e un mapping tastiera/pad per ognuno (Eden permette tasti diversi per profilo; `player_N_type=0` = Pro Controller).
- Applet controller: Eden mostra il dialogo Qt di selezione (frontend `QtControllerSelector`) che rispetta min/max dell'arg; con `player_N_connected` già a true il dialogo conferma. [IPOTESI] con min=max=8 serve che tutti e 8 siano "connected" prima di aprire l'applet, altrimenti il dialogo resta aperto.

## 3. Tabella riassuntiva

| indirizzo | funzione | cosa limita | tipo | alloc / ctor | modifica | rischio | dipendenze |
|---|---|---|---|---|---|---|---|
| 0x7c54a4-0x7c54ac | 0x7c53ec | `SetSupportedNpadIdType(0xf72e48, 5)` {0,1,2,3,0x20} | costante + tabella | – | hook dopo 0x7c53ec: richiamare con 0xf554ac, 9 (o IPS: tabella 0xf554ac, `mov w1,#9`) | basso | – |
| 0x7c54b0 | 0x7c53ec | `orr w1,wzr,#4` → 0x5fc290 (pad interrogati) | costante | – | 8 (IPS 1 istruzione) | basso | NpadDevice già a 9 slot [VERIFICATO] |
| 0x7c54e4-0x7c55bc | 0x7c53ec | loop `cmp w24,#4`: 4 controller + 1 handheld | logica/alloc | 0x5faba8(0x298) + 0xa7f1a0(idx) | hook post-ctor: creare controller idx 4..7 e registrarli nella lista `[sys+0x190]` (capacità `[sys+0x18c]` da leggere a runtime) | medio | ordine: prima del controller handheld? (indice dispositivo usato da 0x7c5794/0x7c5740) |
| 0x8bf158 | accessor | `cmp w19,#5` su array 5 ptr (`[[0x1306e70]]+0x58`) | array fisso 5×8 | sito di riempimento da trovare (chi scrive +0x58) | rilocare a 9 (0..7 + handheld) o riscrivere accessor con hook REPLACE | medio | 185 chiamanti usano indice ≤4 oggi; handheld passa da 4 a 8? |
| 0x8be248 / 0x86ebf8 | accessor | `cmp #3` indice locale; `race.[+0x17c]` 4 int | costante + array 4 | ctor race-system (0x8c0c0 area) | 8 (hook REPLACE delle 2 funzioni, tabella rilocata) | medio | layout race-system: +0x17c..+0x18b seguito da altri campi → serve tabella esterna |
| 0x7c5794 / 0x7c5740 | race-system | tabelle `[+0x98]/[+0xa0]/[+0xa8]` 7 voci, loop srotolato 4 | array 7 + logica | ctor race-system | tabelle rilocate a ≥10 dispositivi; loop generalizzato (hook REPLACE 0x7c5794) | medio | indice dispositivo = posizione nella lista: inserire i nuovi controller prima di handheld/misc o gestire indici >6 |
| 0x7caf0 / 0x7ce88 | accessori | `cmp w19,#5` tabella globale `[0x12fb230]` | array 6 | lazy init 0x7cb44.. | verificare uso (per dispositivo); estendere a 10 | basso | – |
| 0x49ea60 | applet | 4 colori identificazione; min/max passati interi | dati | – | passare 8 colori (facoltativo) | basso | – |
| vibrazione 0x7ca0c0/0x7cae18 | manager | 18 target = 9 pad × 2 | – | – | nessuna | – | – |
| NpadDevice 0x5fbce0 | wrapper | 9 slot, 9 id, `cmp #8` setter | – | – | nessuna | – | – |

## 4. Conclusione input
[VERIFICATO] Il livello nn::hid del gioco è già a 8 pad + handheld (tabella 9 id, 9 slot, 18 target
vibrazione, setter fino a 8): il limite a 4 è imposto solo dal livello di gioco in 4 punti compatti
(0x7c54a4/0x7c54b0, loop 0x7c54e4, accessori 0x8bf158/0x8be248/0x86ebf8, tabelle 0x7c5794). Modifica
stimata: 2 costanti + 4 controller aggiuntivi + 3 tabelle rilocate + 3 accessori riscritti. Rischio
principale: l'indice dispositivo (posizione nella lista) usato da 0x7c5794/0x7c5740/0x7c6550 e la
capacità della lista `[sys+0x18c]` (non determinata staticamente: leggere con GDB allo step 3).
