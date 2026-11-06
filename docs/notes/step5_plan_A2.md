# Step 5 – sotto-obiettivo A2: flusso di selezione per 8 slot, dall'input alla gara (piano statico)

Main 4.0.1 (BID 158C0C73…), offset nel main decompresso (`../img/main.bin`); IPS = offset + 0x100.
Solo analisi statica del disassemblato, più la rilettura del log Eden dello step 4. Convenzione:
**[VERIFICATO]** = letto nel disassemblato (indirizzi citati), **[IPOTESI]** = dedotto,
**[DA VERIFICARE A RUNTIME]** = da controllare in Eden. Dati in `step5_data (dati non pubblicati)/a2_*`:

| file | contenuto |
|---|---|
| `a2_statemachine.asm` | init degli slot, switch per frame sugli stati, eventi, scrittori di +0x904 |
| `a2_routing.asm` | selettore (cursori) a 4 giocatori, PageMgr +0xc68[4], catena pad UI → entry, CharaWin |
| `a2_flow_racecfg.asm` | collegamenti tra pagine, pagina kart, setter del race-config, 0x86f074 |
| `a2_422efc.asm` | corpo completo di 0x422efc e del getter 0x8bf11c |
| `a2_charsel_sites.txt`, `a2_kart_sites.txt` | catalogo dei siti (parola originale) nelle due pagine |
| `a2_eden_unmapped_step4.txt` | letture/scritture a indirizzi nulli nel log Eden dello step 4 (8g), con attribuzione |

Le parti sulla struttura della classe (rilocazione di +0x904, layout 8pl, crash 0x4bd9dc) sono del collega
A1. I pad UI 4..7 sono del collega C (`step5_plan_C.md`: `padui::extra`, `padui::active8`, 12 `cmp #3→#7`).
raceMgr+0x17c e GamePlayer 4..7 sono del collega B. Qui uso le loro API come dipendenze.

## 0. Sintesi

1. **Stati dello slot** (+0x904[i].stato) [VERIFICATO]: 0 = slot inesistente, 1 = slot vuoto (nessun giocatore),
   3 = sceglie il personaggio (cursore attivo), 5 = personaggio scelto (eventuale finestra CharaWin aperta),
   6 = attesa di 10 frame, **7 = confermato**. 2 e 4 compaiono nelle tabelle di salto ma **nel 4.0.1 nessuno li
   scrive**. +0x908 è il contatore dei 10 frame. +0x1a8 non è "pronti": è la **maschera degli input per
   giocatore della pagina** (bit31 = pagina attiva, bit p = il giocatore p può muovere il proprio cursore).
2. **Instradamento** [VERIFICATO]: l'indice è lo stesso ovunque, non c'è una tabella slot→pad. Pad UI i →
   giocatore i del **selettore** (cursori, framework 0x8ae000-0x8b1000) → evento con `ev+0x2c = i` → slot i →
   locale i → `raceMgr+0x17c[i]` → entry. **Il selettore è limitato a 4 giocatori**: stato per giocatore
   4 × 0x20 dentro un oggetto di 0xb8 B, input per frame srotolato p = 0..3 (0x8b0084), fuoco per giocatore
   PageMgr+0xc68 ptr[4], memoria dei cursori [0x88 pagine][4]. Più la finestra per slot **CharaWin**: solo 4
   pagine (id 0x28..0x2b). Per i ≥ 4 il gioco calcola 0x28+i = **0x2c..0x2f**, cioè le pagine
   selezione personaggi Single/WiFi/Multi e kart Single.
3. **Prova dal log Eden dello step 4** (8g): all'ingresso nella selezione personaggi (173.01 s) ci sono 35 accessi a
   indirizzi nulli in 1 ms (Read64 @0x1d0 ×4, Write32 @0x368..0x384 ×24 = 3 serie da 8, Read8 @0x29c ×2…). Corrispondono al ciclo
   `i < N` di 0x4b7218 (vt94), che per i = 4..7 legge la "pagina CharaWin" 0x2c..0x2f e l'entry NULL del pad 4.
   Eden li registra e va avanti; **su console sarebbero data abort** [IPOTESI forte] (`a2_eden_unmapped_step4.txt`).
4. **Avanzamento**: `w28 == N` a 0x4ba938 (w28 = slot in stato 7 con i < +0x8c0) → link this+0x1d0 → pagina
   **0x31** (kart Multi, +0x39c = 4 slot) → link → 0x32 (o 0x35/0x36 secondo cfg+0x1fb0) → … → coppa 0x3c → gara.
   La pagina kart ha **la sua macchina a stati per slot** (+0x460[4] stride 8) e avanza quando tutti gli slot
   i < +0x39c in stato 7 hanno stato kart 3.
5. **Scritture nel race-config** [VERIFICATO]: entry = `0x8be03c(0x8bf158(slot))`, poi:
   0x42022c tipo (entry+0x224 e descrittore+0x44), 0x4202f4 personaggio (+0x21c/+0x220, descrittore +0x3c/+0x40),
   0x420ac0/0x420b88/0x420c50 carrozzeria/gomme/aliante (entry +0x210/+0x214/+0x218, descrittore +0x30/+0x34/+0x38).
   Descrittore = settingsB (0x86f7d4) + 0x30 + d·0x1c con d = entry+0x22c. Ogni setter esce senza fare nulla se
   entry = -1, quindi oggi **gli slot 4..7 non scrivono niente** (0x8be03c ha `cmp #3`).
   **0x86f074(settings)** ricalcola +0x180/+0x184/+0x188[] (qui su settingsB). È uno scrittore di settings+0x180,
   che lo step 1 non aveva trovato staticamente. Azzera +0x180 se un descrittore non vuoto ha un campo a -1.
6. **Config per giocatore a 4 voci** [VERIFICATO]: cfg+0x1fb4 (personaggio), +0x1fc4, +0x1fd4 (carrozzeria),
   +0x1fe4 (gomme), +0x1ff4 (aliante), +0x2004..+0x2074 (varianti), tutti `int[4]` contigui con passo 0x10
   e indice clampato a 4. Per gli slot 4..7 il clamp riporta su slot 0 (scrittura sbagliata ma confinata).
   Se si rimuovono i clamp senza spostare gli array, si sovrascrive l'array successivo.
7. **Due strade**: **A2-min** tiene interfaccia e selettore a 4. Gli slot 4..7 vengono assegnati e confermati
   automaticamente dal modulo (personaggio e parti scritti con i setter del gioco). Il flusso arriva alla gara
   a N locali senza toccare il framework. **A2-full** richiede di estendere selettore, PageMgr, pagine
   CharaWin, pagina kart e layout (molti siti, §2.5 e §3.3). Raccomando A2-min come prima consegna dello step 5
   (§6).

## 1. Macchina a stati dello slot

### 1.1 Valori e transizioni [VERIFICATO] (`a2_statemachine.asm`)

Classe selezione personaggi (Multi = pagina 0x2e, vtable 0x1278930). Slot i: `this+0x904+i*8` (+0 stato,
+4 = +0x908 contatore), sempre con il clamp `cmp #4; csel …, xzr, lo`.

| stato | significato | chi lo scrive | uscita |
|---|---|---|---|
| 0 | slot oltre +0x8c0 (non esiste) | 0x4bc958 (ctor base 0x4bc844); 0x4b81f8/0x4b8230/0x4b8248/0x4b8274/0x4b8284 (vt77, i in [+0x8c0,4), **srotolato** con `cmp #3`) | – |
| 1 | slot vuoto (i ≥ N) | 0x4b81dc (vt77, i in [N, +0x8c0), bit i di +0x1a8 tolto); 0x4ba488/0x4ba628 (4→1) | – |
| 2 | ritorno in selezione | **nessuno scrittore** nel 4.0.1 | 0x4ba430/0x4ba5d0: se `0x14a858(i)` → 3, bit i, cursore `0x8af3f4(sel,i,…)` |
| 3 | sceglie il personaggio | 0x4b80c8/0x4b80fc/0x4b8108 (vt77, i < N, bit i acceso); 0x4b9510 (vt80); 0x4b564c (CharaWin vt83); 0x4ba440/0x4ba5e0 (2→3); 0x4ba730 (7→3) | evento "decide" → 5 |
| 4 | uscita del giocatore | **nessuno scrittore** | 0x4ba47c: se `0x14a8f4(i)` → 1 |
| 5 | personaggio scelto | **0x4bc028** (vt92 0x4bbd18, evento tipo 2 sull'icona; slot = ev+0x2c) | CharaWin (pagina 0x28+i)+0x120 fuori da [3,7] (ramo A) / [3,8) (ramo B) → 6, +0x908 = 0, `0x14b46c(i)` |
| 6 | attesa | 0x4ba558/0x4ba6e4 | +0x908 > 9 → 7 (0x4ba4fc/0x4ba69c) |
| 7 | **confermato** | 0x4ba4fc/0x4ba69c | solo ramo A: pad UI i `+0x10 bit2` (annulla, [IPOTESI] B) → 0x4ba71c: 3, bit i, `0x8ba814(this,i)`, `0x14b508(i)` |

- Stato 5: 0x4bc074 toglie il bit i da +0x1a8 (il cursore del giocatore si ferma). Poi, secondo il personaggio
  (tabella 0xf2919c, id 4..0x34), apre la finestra varianti: 0x4bc59c..0x4bc648 prende la pagina
  `0x8bf0ec(0x28+slot)`, scrive +0x2a0 = tipo e +0x2d1 = 0, e segue il link `this+0x660+clamp4(slot)*0x28`.
  Senza varianti la finestra non si apre e il passaggio 5→6 avviene subito.
- Rami A/B dello switch (vt93 0x4b99c8, tabelle 0xf29164 / 0xf29150): A se `[this+0x120] == 4` e
  `!0x4b1504()` (0x4ba3a0-0x4ba3cc), altrimenti B. B non ha il caso 7 e usa le soglie 3..7 / ≥10.
- 0x4b80c4 (citata nello step 1) non è l'inizio di una funzione: è dentro vt77 **0x4b74a4** (onEnter),
  blocco 0x4b80a0-0x4b82c4. Il `csinc` a 0x4b8100 è il secondo elemento di un ciclo srotolato a coppie (i, i+1)
  che scrive 3 per i < N. Il `tbnz w8,#0` a 0x4b80b0 gestisce N dispari scrivendo prima lo slot 0.
- Il bit i di +0x1a8 (maschera input della pagina) segue lo stato: acceso con 3, spento con 1 e 5. Il
  selettore lo controlla in 0x8af430/0x8af45c, 0x8ba634 controlla anche bit31. Nota del collega C: **il bit 4
  è usato per il pad tocco** (campo pagina +0x118 = 4). Con 8 slot il bit 4 dello slot 4 collide
  [DA VERIFICARE A RUNTIME] (§6, rischi).

### 1.2 Cicli sugli slot nella selezione personaggi [VERIFICATO] (`a2_charsel_sites.txt`)

| funzione (vtable) | limite | cosa fa per slot i | problema per i ≥ 4 |
|---|---|---|---|
| 0x4b6970 (da vt77) | 4 fisso (0x4b70a8/0x4b71f0 `cmp x23,#4`; 0x4b6a28..0x4b6f48 srotolato 0x28..0x2b) | convalida cfg+0x2004[i]/+0x2034… e azzera i dati della CharaWin | – (4 fisso) |
| vt77 0x4b74a4 | i < +0x8c0 (0x4b75e0) | cfg+0x1fb4[i] valido (0x8636bc) o lo azzera | cfg clamp 4 |
| vt77 | i < N, solo i ≤ 1 (0x4b77c0) | copia per l'online (+0x9e0[2]…) | – |
| vt77 | i < +0x8c0 (0x4b7c7c) | cursore i sull'icona di cfg+0x1fb4[i] (`0x8af3f4(+0x1b0, i, …)`), icone +0x740[0x30] | selettore a 4 |
| vt77 | i < +0x8c0 (0x4b7d6c) | `0x8bb3b8(pagina 0x28+i)` | **pagine 0x2c..0x2f** |
| vt77 | init (0x4b80a0-0x4b8294) | stati 3/1/0 | clamp + srotolato 4 |
| vt77 | i < +0x8c0, stato 3 (0x4b8330) | entry = 0x8be068(pad i) → personaggio (0x4203e0/0x420948) e parti (entry+0x210/214/218 ← cfg+0x1fd4/1fe4/1ff4[i]) | entry NULL / cfg clamp |
| vt77 | 0x4b8658-0x4b8678: i in [+0x8c0, 4) | `0x14a7ac(i, 0)` | 4 fisso |
| vt77 | i < +0x8c0 (0x4b87d4) | stato 3: oggetto `this+0x728` → +0x1f0[i] (vettore per slot, conteggio +0x1e8), `0x4be370/0x4be55c`; altrimenti `0x4b43b4` | layout (A1) |
| vt94 0x4b7218 | **i < N** (0x4b7350/0x4b7460) | `0x3fd5a4(cfg+0x1fa4, CharaWin(0x28+i)+0x1d0, i)` | **pagine 0x2c..0x2f → NULL+0x1d0; entry NULL per i=4** (log Eden) |
| vt94 → 0x3fdd70 | **i < N** (0x3fe018) | entry = 0x8be068(pad i): scrive entry+0x208/+0x20c/+0x209 | entry NULL per i=4 (log Eden), entry del locale 0 per i=5..7 |
| vt93 0x4b99c8 | i < +0x8c0 (0x4b9b78 e 0x4ba404/0x4ba5a4) | amiibo/Mii (+0x928+i·0x28), switch stati | clamp 4 sugli array per slot (A1) |
| vt86 0x4ba77c | i < +0x8c0 (0x4ba860) | conta gli stati 7 (w28), +0x900, CharaWin(0x28+i)+0x120 | pagine 0x2c..0x2f (sola lettura +0x120 su pagina sbagliata o NULL) |
| vt86 | i < N (0x4ba994) | `0x8be248(0x8bf158(i))` pronto → applet | step 4 (H4) |
| vt86 | 0x4baaa0-0x4baaf4 | `0x8be478` (tocco) su pad UI 0..3 **srotolato** | i 4..7 mai |
| vt81 0x4bab18 | i < +0x8c0 (0x4bac38) | chiude CharaWin(0x28+i): +0x2d0 = 1, `0x8ba5fc(p, p+0x2a8)` | **pagine 0x2c..0x2f: scrittura + link su pagina sbagliata** |
| vt81 | – | `0x49e960(0x8bf368(), &1, N)`: +0x1628 = N, u8[4] +0x162c..+0x162f = (N > i) **srotolato** | 4 flag (step 6) |
| vt87 0x4badb0 | i < +0x8c0 (0x4bae18) | come vt81 (CharaWin aperte) | **come vt81** |
| vt87 stadio 1 | 0x4bb000-0x4bb0f4: entry 0..15 → tipo 4 (srotolato); i < +0x8c0 stato 7 (0x4bb108) → `0x42022c(0x8be03c(pad i), &0)`, cfg+0x1434++ | tipo dei locali | entry -1 per i ≥ 4 → saltato |
| vt87 stadio 2 | i < +0x8c0 (0x4bb1e8) | attende la chiusura delle CharaWin | pagine 0x2c..0x2f |
| vt87 0x4bb460 | i < +0x8c0, stato 7 | `0x4104a8(…, entry+0x21c, 0x41ffd8(entry+0x210))` [IPOTESI: preparazione modelli] | entry NULL → saltato |
| vt84 0x4bb6a0 | i < N, solo i ≤ 1 | online | – |
| vt92 0x4bbd18 | slot = ev+0x2c (selettore, < 4) | stato 5, cfg+0x1fb4/+0x1fc4[slot clamp 4], CharaWin | – finché il selettore è a 4 |

Funzioni per slot dell'audio/modelli del menu (0x14a7ac, 0x14a858, 0x14a8f4, 0x14b46c, 0x14b508, 0x13d424,
0x13d47c): passano per `0x7e7434()+0x190[4]+0x238` e l'array +0xc0/+0xc8. **0x13d47c restituisce NULL se
slot ≥ conteggio** e 0x14a858 in quel caso risponde 1 [VERIFICATO]: per gli slot 4..7 non vanno in crash,
semplicemente non fanno nulla. Il conteggio massimo (≤4) è nel catalogo di 0x13e59c (sites_step5).

## 2. Instradamento dell'input

### 2.1 Catena [VERIFICATO] (`a2_routing.asm`)

```
pad UI i = 0x8bf158(i)            container [[0x1306e70]]+0x58 ptr[5]; pad+8 = i
  calc 0x8be674: pulsanti +0xc/+0x10/+0x14 da GamePlayer(idx)+0x3d8 solo se idx<=3 e +0x1c (pulsanti on)
selettore della pagina (page+0x1b0, 0xb8 B, alloc 0x8bafcc, init 0x8af2c4)
  0x8b0084: 0x8b00e0(sel, p) per p = 0,1,2,3 (SROTOLATO)  <- 0x8bc45c (input della pagina)
     0x8b00e0: PageMgr+0xc68[p] == page? (fuoco del giocatore p) ; 0x8ba634(page,p) ; pad = 0x8bf158(p)
               stato sel+0x28+p*0x20 ; maschere sel+0xa8/+0xac & pad+0x14 -> 0x8af3f4(sel, p, item, …)
  0x8af3f4 -> evento sullo stack: +0x20 sel, +0x28 tipo, +0x2c = p, +0x30 pulsanti, +0x34 flag
     -> page->vt[0x58] -> charsel vt92 0x4bbd18 (slot = ev+0x2c) / kart vt92 0x4c3cdc (slot = ev+0x2c)
slot i -> 0x8be03c/0x8be068(pad i) -> 0x86ebf8(i) [cmp #3] -> raceMgr+0x17c[i] = e -> raceMgr+0x58[e]
entry -> 0x86ebbc(e) [e<=15] -> entry+0x22c = d -> descrittore settingsB+0x30+d*0x1c
```
**Pad UI, giocatore del selettore, slot, indice di cfg, pagina CharaWin (0x28+i), oggetti audio e locale
usano sempre lo stesso i.** L'unica mappa è `raceMgr+0x17c[i]` (locale → entry), riempita da 0x869180 (step 4,
collega B). La pagina kart usa lo stesso schema (ev+0x2c, `0x8be03c(0x8bf158(slot))` in 0x4c1bfc).

### 2.2 Limiti a 4 del framework UI [VERIFICATO]

| sito | cosa | parola | nota |
|---|---|---|---|
| 0x8bafcc | `mov w0,#0xb8` dimensione del selettore | 52801700 | stato per giocatore +0x28..+0xa7 (4×0x20), poi +0xa8/+0xac maschere, +0xb0 |
| 0x8b00a0..0x8b00d0 | input per frame p = 0..3 (`orr w1,wzr,#1/#2/#3`, l'ultima a 0x8b00c4 `320007e1` + `b`) | – | srotolato |
| 0x8b01f8..0x8b0224 | aggiornamento cursori p = 0..3 (0x8b021c `320007e1`) | – | srotolato |
| 0x8b0108, 0x8b0278, 0x8af474, 0x8af490 | `cmp w,#4` clamp del giocatore | 7100127f / 710012bf | + altri 15 clamp in 0x8ae1e4..0x8b16b4 (`a2_routing.asm`) |
| 0x8bd5f8 (0x8bd610) / 0x8bd6d8 (0x8bd96c `cmp x20,#4`, f100129f) | PageMgr+0xc68 ptr[4]: fuoco per giocatore (subito dopo, +0xc88 è un altro campo) | – | non estendibile in loco |
| 0x8bf228()+0x1d0 ptr[4] (0x8b08a4 clamp) | oggetti grafici dei cursori | – | |
| [[0x1306e70]]+0x60 [0x88][4] int (0x8af6bc-0x8af6d8) | ultima posizione del cursore per pagina e giocatore | – | |
| pagina 0xe +0x1d0 ptr[4] (0x49ae1c, clamp 0x49ae2c) | pannelli per giocatore, chiamati 0..3 srotolati da kart 0x4c1870-0x4c18ac e 0x4c3c40-0x4c3c7c | – | |

### 2.3 Pagine CharaWin per slot e collisione degli id [VERIFICATO]

- Registrazione delle pagine (0x404d50-0x404e70): id 0x28, 0x29, 0x2a, 0x2b, tutte con la fabbrica **0x40918c**
  (GOT 0x1307b38): alloc 0x310, ctor 0x4b27c4, vtable 0x1278048 (`mn_L_CharaWin_00`, `L_ChangeBtn_00`,
  `CharaList`). Tabella delle pagine: 0x88 voci (`cmp w9,#0x88`, 0x404d18). Id registrati: 108. Liberi
  (< 0x88): 0x0-0x3, 0xa, 0xc, 0x12, 0x1d, 0x43, 0x5b, 0x5e, **0x64-0x6d**, 0x71, 0x73, 0x75, 0x79, 0x7e, 0x84,
  0x87. Che siano davvero inutilizzati è [IPOTESI]: ho controllato solo le chiamate a 0x8b3cb0.
- CharaWin: slot = id − 0x28 (0x4b28d4, `+0x1e8`). In onEnter 0x4b29e0, parent = pagina sorgente
  (0x8ba570). La maschera input +0x1a8 prende **solo il bit dello slot, con un if srotolato su 0..3**
  (0x4b2ab8-0x4b2b6c): per slot ≥ 4 resta senza bit. Il fuoco +0x190 |= 1<<slot.
- Link dalla selezione: +0x660/+0x688/+0x6b0/+0x6d8 (`0x8b21cc(link, &id, -1)`: link+0xc = id) impostati in
  vt77 a 0x4b7a38-0x4b7a94 con id 0x28..0x2b. Usati solo a 0x4bc63c (`madd` con clamp 4 dello slot).
- **0x8bf0ec(id)** = `0x8bd0e8(PageMgr, id)` (clamp al numero di pagine). Poi ogni chiamante verifica il tipo con
  `vt[0x10](GOT 0x1308cd8)` e ottiene NULL se la pagina non è una CharaWin. Siti che calcolano `0x28+i`
  (`a2_charsel_sites.txt`, 17): 0x4b4a30, 0x4b7028, 0x4b710c, 0x4b7170 (i<4), 0x4b7394 (**i<N**), 0x4b77c8
  (i≤1), 0x4b7d6c, 0x4b9efc, 0x4ba490, 0x4ba630, 0x4ba87c, 0x4baba8, 0x4bae18, 0x4bb1e8 (i < +0x8c0), 0x4bb964
  (i≤1), 0x4bc32c, 0x4bc59c (slot dell'evento). **Nell'intervallo 0x4b1298-0x4bc844 nessuna chiamata a 0x8bf0ec
  usa le costanti 0x2c..0x2f**: l'unico uso costante di 0x2d è a 0x4b10ac, fuori intervallo. Questo rende
  possibile rimappare per indirizzo di ritorno (§6, H-A2-4).
- Per N > 4, oggi: vt94 (i < N) colpisce gli id 0x2c..0x2f → NULL → `ldr x25,[NULL+0x1d0]` (0x4b73f4) →
  `0x3fd5a4(cfg, NULL, i)` scrive a NULL+0x368..0x384. Corrisponde esattamente al log Eden dello step 4
  (`a2_eden_unmapped_step4.txt`: 4 × Read64 @0x1d0, 3 × 8 Write32 @0x368..0x384 per i = 5,6,7, Read8 @0x29c
  per i = 4). Anche 0x3fdd70 (i < N) dereferenzia l'entry NULL del pad 4 (0x3fde34 `ldrb [x23,#0x29c]`,
  0x3fdec4, 0x3fdf5c, 0x3fe00c). **Questi due cicli i < N vanno protetti già ora, anche con la selezione a 4
  posti** (H-A2-0, §6).

### 2.4 Funzioni della famiglia pad UI usate dal flusso [VERIFICATO]

| funzione | uso nel flusso | limite |
|---|---|---|
| 0x8bf158(i) | ovunque, con i = slot | clamp 5 (collega C) |
| 0x8be03c(pad) → e | 0x4b9c08, 0x4bbdec, 0x4bb128, 0x4203fc, 0x420968, 0x4c1c28, 0x4b44d8 | `cmp #3` a 0x8be040 |
| 0x8be068(pad) → entry* | 0x4b8350, 0x4bb490, 0x4bb9dc, 0x3fd5cc, 0x3fde28, 0x4b1f28 | `cmp #3` a 0x8be078 |
| 0x8be248(pad, flag) pronto | 0x4ba994 (i<N), 0x4b9738 (vt80, i<N) | `cmp #3`, H4 step 4 |
| 0x8be2e8(pad, flag) reset | 0x4baa48 (pad 0) | `cmp #3` |
| 0x8be478(pad, on) tocco | 0x4baaac..0x4baaf4 (0..3), 0x4b82b0/0x4bbce8/0x4c17d4/0x4c3ca4 (pad 4 = tocco), 0x422efc | – |
| pad+0x10 bit2 (annulla), bit3/4 (0x18) | 0x4ba50c (stato 7), 0x4c29d8 (kart), 0x4c2884, 0x4baa28 (pad 0) | – |
| pad+0x2c4 = 0 | 0x4b8728, 0x4c125c (pad 0) | – |

### 2.5 Siti con confronti su 3/4/5 nel flusso da portare a 8 [VERIFICATO]

Elenco completo con parole originali in `a2_charsel_sites.txt` (146 righe) e `a2_kart_sites.txt` (183 righe).
Riassunto per tipo:

| gruppo | siti | parola orig. → nuova | chi |
|---|---|---|---|
| clamp `cmp x,#4; csel` su +0x904 (17) | 0x4b1d20 0x4b563c 0x4b80e4 0x4b80f4 0x4b81bc 0x4b8330 0x4b87d4 0x4b901c 0x4b94dc 0x4b9b78 0x4ba404 0x4ba5a4 0x4ba860 0x4bb108 0x4bb470 0x4bc018 0x4bc630 | `…#0x4` → `…#0x8` più la rilocazione dell'array | **A1** |
| init srotolato degli stati 0 (`cmp #3`/`#4`) | 0x4b81e8 (`cmp w8,#3`), 0x4b8210, 0x4b822c, 0x4b8254, 0x4b826c | da riscrivere come ciclo fino a 8 (hook, non patch) | A1/A2 |
| clamp del giocatore su +0x928+i·0x28 (amiibo) | 0x4b91e4, 0x4bb28c, 0x4bc4f0, 0x4ba1b8 (madd con x21 clamp) | array 4 × 0x28 (A1) | A1 |
| clamp cfg per giocatore (`cmp #4`, +0x1fb4..+0x2074) | 0x4b75ec, 0x4b7b44, 0x4b7888, 0x4b8330, 0x4b9b78 (x21), 0x4bbf18, 0x4bbfac, 0x4bc228, 0x4b394c…0x4b3ffc (CharaWin), 0x4204c4, 0x4209f0, 0x4c1dd8, 0x4c1e8c, 0x4c1f40, 0x4c37e0, 0x4c09f0 | array cfg ombra a 8 (§4.3) | A2 |
| bound 4 fissi nel flusso | 0x4b70a8/0x4b71f0 (`f10012ff`), 0x4b8674 (`cmp w22,#4`), 0x4b2ac0-0x4b2b6c (CharaWin onEnter), 0x4baaa0-0x4baaf4, 0x49e960 | hook | A2 |
| selettore e PageMgr | §2.2 | rilocazione framework (A2-full) | A2-full |
| pagina kart | §3.3 | | A2-full |

## 3. Avanzamento e pagine successive

### 3.1 0x4ba938 [VERIFICATO]
`w28` = slot in stato 7 con i < +0x8c0 (ciclo 0x4ba860-0x4ba924, clamp 4 a 0x4ba860). `w22` = N
(cfg+0x1fa4, salvato a 0x4ba830 e riletto a 0x4ba934). Se uguali: `0x8ba5fc(this, this+0x1d0)` (0x4ba948).
0x8ba5fc(page, link): link+8 = page+0xe8 (id sorgente), poi `link->vt[0x10]` (transizione). Il collegamento
+0x1d0 è impostato in vt77 nel ramo offline (0x4b8748): `+0x8c0 ≤ 1` → 0x2f, altrimenti `cfg+0x1fa8 == 2` → 0x30,
altrimenti **0x31** (0x4b8b2c `52800628`). Con P5b dello step 4 (5-8 → modo 3) → 0x31. Il ramo online
(0x4b79bc) usa 0x2f/0x30.

`ips_C` (`cmp w28,w28`) avanzava sempre. Senza stati 7 per tutti gli slot, però, la pagina kart non aveva slot in
stato 7, i suoi cicli non facevano nulla e l'avanzamento kart richiede "tutti gli slot in stato 7 hanno kart 3".
Con zero slot la condizione è vera subito. Questo è compatibile con "4 posti, input bloccato" [IPOTESI].

### 3.2 Sequenza delle pagine [VERIFICATO salvo indicazione]
```
0x24 (modalità Multi) -> 0x2e charsel Multi (0x409210, 0xa28, +0x8c0=4)
   vt87 stadio 1 (0x4baf64): 16 entry -> tipo 4, slot in stato 7 -> tipo 0, cfg+0x1434 = numero
   vt81 (0x4bab18): 0x49e960(obj, &1, N)  [u8[4] di presenza, step 6]
-> 0x31 kart Multi (0x409294 -> ctor 0x4c4b74, 0x4a0 B, +0x39c=4, +0x3a8 = pagina 0x19, +0x3b0 = 0x17, +0x3b8 = 0x2e)
-> 0x32 (cfg+0x1fb0 != 0,1) / 0x35 (==0) / 0x36 (==1)   [0x32 = mn_ItemSwitch_00, regole]
-> ... -> 0x3c selezione coppa (0x492204/0x4922fc: ciclo i<N su 0x8be248, applet; step 4) -> gara
```
Indietro dalla kart: link +0x1f8 → 0x2c/0x2d/0x2e secondo +0x39c (0x4c12fc-0x4c138c).

### 3.3 Pagina kart (0x2f/0x30/0x31) [VERIFICATO] (`a2_flow_racecfg.asm`, `a2_kart_sites.txt`)

Vtable 0x1279b00: 75 = 0x4c46dc (init), 77 = 0x4c0898 (onEnter), 81 = 0x4c3058, 84 = 0x4c3564 (indietro),
86 = 0x4c2820 (per frame), 92 = 0x4c3cdc (eventi: ev+0x8 parte 0/1/2 o 0x64/0x65, ev+0x2c slot), 93 = 0x4c24f4.

| campo | significato | limite |
|---|---|---|
| +0x39c | numero di slot (1/2/4: 0x4c46c8 / 0x4c49a0 / **0x4c4c70 `321e03e8`**) | ciclo su tutti i loop |
| +0x3b8 | pagina charsel: legge `+0x904[i]` == 7 (clamp 4) | A1 |
| +0x3a8 | pagina 0x19 (Multi; 0x14 Single), +0x1e8/+0x1f0 vettore per slot (modelli) | layout |
| +0x3d0/+0x3d8, +0x3e0/+0x3e8, +0x3f0/+0x3f8 | vettori per slot dei selettori carrozzeria/gomme/aliante (conteggio + ptr) | creati in 0x4bfdd0 da `L_KartSlot_0i` (i < +0x39c) [IPOTESI] |
| +0x460 + i·8 (stato), +0x464 (timer) | stato kart per slot: 4→1 (10 frame, bit i +0x1a8), 1 = sceglie, 2→3 (25 frame), 3 = confermato | **4 voci** (+0x460..+0x47f), poi +0x480 int[2] |
| +0x480/+0x488/+0x490 int[2] | parti per i ≤ 1 (online) | – |
| +0x498 | 1 quando avanza (0x4c2a48) | – |

- onEnter: +0x460[i] = (stato charsel == 7) per i < +0x39c (0x4c1548), poi 0 per i in [+0x39c, 4) **srotolato**
  (0x4c157c-0x4c15dc). +0x1a8 bit i. Maschere +0x88 dei 3 selettori per slot con `cmp w11,#4` (0x4c16d4).
  `0x422efc(N, 0)` (0x4c17c4). Cursori `0x8af3f4(sel, i, 3i+k)` per i < +0x39c (0x4c1820). Parti iniziali:
  `0x4c1bfc(this, i, 0/1/2, 1)` per gli slot in stato 7 (0x4c18f8/0x4c190c/0x4c1920).
- Per frame 0x4c2820: avanza se **per ogni i < +0x39c con stato charsel 7, +0x460[i] == 3** (0x4c2a10-0x4c2a40),
  poi +0x498 = 1, +0x184 = 0x23, tail-call `0x8ba5fc(this, this+0x1d0)` (0x4c2a6c).
- 0x4c1bfc(kart, slot, parte, flag): e = `0x8be03c(0x8bf158(slot))`; valore = elemento corrente del selettore
  di parte (+0x3d8/+0x3e8/+0x3f8)[slot]; poi 0x420ac0 / 0x420b88 / 0x420c50 (e, &valore) e cfg+0x1fd4 / +0x1fe4 /
  +0x1ff4[slot clamp 4] (0x4c1dd8, 0x4c1f40, 0x4c1e8c).

Limiti a 4 della pagina kart da portare a 8 (A2-full), parola originale → nuova:

| sito | orig | nuovo / azione |
|---|---|---|
| 0x4c4c70 `orr w8,wzr,#4` (+0x39c Multi) | 321e03e8 | dinamico: hook su onEnter 0x4c0898 (`+0x39c = N`) solo se +0x460 è rilocato |
| clamp `cmp x,#4` (`f10012..`/`f100113f`) su +0x3b8→+0x904 (24) | vedi file | come A1: 0x4c04cc 0x4c0658 0x4c0dd0 0x4c0e68 0x4c0f0c 0x4c0fe4 0x4c154c 0x4c18d0 0x4c19ec 0x4c1aa4 0x4c1b8c 0x4c27c0 0x4c2864 0x4c2a10 0x4c2a9c 0x4c2ff8 0x4c312c 0x4c31a0 0x4c3204 0x4c350c 0x4c3d74 0x4c3ed0 0x4c451c (+0x4c154c anche su +0x460) |
| clamp `cmp x11,#4` (`f100117f`) su `x8+0x238` ptr[4] per slot (oggetto non identificato) | f100117f | 0x4c2658 0x4c26cc 0x4c2748 (vt93 0x4c24f4): array esterno a 4 |
| +0x460 stride 8 a 4 voci (clamp 0x4c2948 `f100129f`, 0x4c154c, 0x4c2b70 `7100127f`, 0x4c3fe4/0x4c3e40 `7100103f`, 0x4c3008..0x4c3014) | – | rilocare in coda (0x4a0 → +0x40) |
| 0x4c157c-0x4c15dc azzeramento srotolato `cmp #3` | – | hook |
| 0x4c16d4 `cmp w11,#4` (maschere +0x88 dei selettori per slot, ciclo sui 4 giocatori) | 7100117f | `cmp w11,#8` (7100217f) solo con selettori 4..7 |
| 0x4c1870-0x4c18ac, 0x4c3c40-0x4c3c7c: 0x49ae1c(pagina 0xe, 0..3) | – | pagina 0xe +0x1d0[4] (layout) |
| 0x4c1dd8/0x4c1e8c/0x4c1f40 `cmp w19,#4` cfg parti | 7100127f | cfg ombra §4.3 |

## 4. Scritture nel race-config

### 4.1 Campi [VERIFICATO] (`a2_flow_racecfg.asm`)

| oggetto | campo | scritto da |
|---|---|---|
| entry = raceMgr+0x58[e] (e ≤ 15) | +0x210 carrozzeria, +0x214 gomme, +0x218 aliante | 0x420ac0 / 0x420b88 / 0x420c50; vt77 0x4b8478/0x4b84e0/0x4b8544 direttamente |
| | +0x21c personaggio, +0x220 u8 variante, +0x221 u8 | 0x4202f4 (da 0x4203e0, 0x420948, 0x4908xx, 0x4ff8xx) |
| | +0x224 tipo (0 = locale umano, 4 = vuoto) | 0x42022c |
| | +0x22c indice del descrittore d | 0x869040/0x869180 (step 1/4) |
| | +0x208/+0x209/+0x20c | 0x3fdd70 (vt94) |
| descrittore = settingsB(0x86f7d4)+0x30+d·0x1c (d < 12) | +0x30 carrozzeria, +0x34 gomme, +0x38 aliante, +0x3c personaggio, +0x40 u16 variante, +0x44 tipo, +0x48 | gli stessi setter (copiano e aggiornano l'intero descrittore) |
| settingsB | +0x180 totale, +0x184 umani, +0x188[tipo<5] (tipo 0 = locali) | **0x86f074** (tail-call da 0x4202f4) |

Coincide con il "descrittore a stride 0x1c, campi +0x30/+0x34/+0x38/+0x3c" dello step 1: i quattro campi sono
**carrozzeria, gomme, aliante, personaggio**. +0x44 tipo. Il descrittore è in **settingsB** (system+0x248,
getter 0x86f7d4 = `0x7e741c()->[0x10]`). Nello step 1 era attribuito a 0x8ace28: lì stanno solo le entry.
Gli entry+0x60/+0x64/+0x68 citati nella consegna non compaiono nei setter del flusso [non trovati].

### 4.2 Chi scrive, per quali slot [VERIFICATO]

| momento | funzione | slot | indice entry |
|---|---|---|---|
| ingresso in charsel (non da kart) | vt77 0x4b8330-0x4b862c | i < +0x8c0, stato 3 | 0x8be068(pad i) |
| evento "decide" icona | vt92 → 0x4203e0(slot, &pers) (0x4bc290) | slot dell'evento | 0x8be03c(pad slot) |
| finestra varianti CharaWin | 0x4b3970…0x4b3ec4 → 0x4203e0(+0x1e8) | slot della finestra | idem |
| verso la pagina kart | vt87 0x4bb000-0x4bb144: `0x42022c(j, &4)` j=0..15, poi `0x42022c(0x8be03c(pad i), &0)` per stato 7 | i < +0x8c0 | 0x8be03c |
| ingresso pagina kart | 0x4c18cc-0x4c19cc → 0x4c1bfc ×3 | i < +0x39c, stato charsel 7 | 0x8be03c |
| scelta parti (eventi kart) | 0x4c3eac → 0x4c1bfc | ev+0x2c | 0x8be03c |

Con `cmp #3` in 0x8be03c/0x8be068 gli slot 4..7 producono e = -1 o NULL e tutti i setter escono subito
(0x42024c, 0x42031c, 0x420ae0 `cmn w,#1`). **Nel 4.0.1 non esiste alcun ciclo "per i < 4" che scriva il
race-config**: tutto passa per lo slot, quindi le patch del collega C (`cmp #3→#7`) e la mappa del collega B
bastano a rendere validi gli indici. Restano i limiti di slot (+0x8c0, +0x39c) e di cfg.

### 4.3 Config per giocatore (`int[4]`, passo 0x10) [VERIFICATO]
cfg = singleton+0x30. Array: +0x1fb4 personaggio (letto/scritto in 0x4b75f8, 0x4b9db8, 0x4bbf2c), +0x1fc4
(0x4b9eb4, 0x4bbfc0, 0x4bc238; [IPOTESI] personaggio amiibo/secondario), +0x1fd4 / +0x1fe4 / +0x1ff4 parti
(0x4c1df0 / 0x4c1f58 / 0x4c1ea4; letti in 0x4b8474/0x4b84dc/0x4b8540, 0x4c0a08…, 0x4c37f0…), +0x2004, +0x2014,
+0x2024, +0x2034, +0x2044, +0x2054, +0x2064, +0x2074 (varianti per personaggio: CharaWin 0x4b3960…0x4b4008,
0x4203e0, 0x420948). Ogni accesso usa `cmp idx,#4; csel …, xzr, lo`. Dopo +0x2074+0xc viene +0x2084 (altro
campo, 16 lettori). Quindi **non si possono allargare in loco**. Proposta (A2-full): array ombra
`int cfgx[13][8]` nel modulo, sincronizzato per gli slot 4..7 con hook sui setter (0x4c1bfc, 0x4bbd18,
CharaWin vt) e letto nei punti di uso. Con A2-min bastano i valori scelti dal modulo (§6).

### 4.4 Log di accettazione "personaggi assegnati" (proposta)

**Dove**: trampolino sul per-frame della pagina kart **0x4c2820** (`stp x24,x23,[sp,#-0x40]!`, rilocabile).
Dopo `Orig`, se `page+0x498 == 1` è appena diventato 1 (0x4c2a48), stampare una volta per ingresso:
+0x498 viene azzerato in onEnter (0x4c08f4). È l'ultimo momento del menu in cui personaggio, parti e tipo di
tutti i locali sono definitivi. Seconda riga facoltativa all'avanzamento della charsel (trampolino 0x4ba77c,
quando il conteggio degli stati 7 raggiunge N).

```cpp
/* ---- step 5 A2: log personaggi assegnati (report/step5_plan_A2.md §4.4) ---- */
namespace a2 {
    static uint8_t* SettingsB() { return mk8dx::Fn<uint8_t* (*)()>(0x86f7d4)(); }
    static uint8_t* RaceMgr()   { return mk8dx::Fn<uint8_t* (*)()>(0x8ace28)(); }
    static void LogAssigned(const char* tag) {
        uint8_t* rm = RaceMgr(); uint8_t* sb = SettingsB();
        int n = menu8::LocalCount();
        int cnt = *(int32_t*)(rm + 0x50); uint8_t** ent = *(uint8_t***)(rm + 0x58);
        Logging.Log("[mk8dx-8p] A2 %s: N=%d race+0x178=%d sB+0x180=%d +0x184=%d +0x188[0]=%d entry=%d",
                    tag, n, *(int32_t*)(rm + 0x178), *(int32_t*)(sb + 0x180), *(int32_t*)(sb + 0x184),
                    *(int32_t*)(sb + 0x188), cnt);
        for (int j = 0; j < cnt && j < 16; j++) {                 /* tutte le entry: tipo 0 = locale */
            uint8_t* e = ent[j]; if (!e) continue;
            int32_t d = *(int32_t*)(e + 0x22c);
            uint8_t* ds = (d >= 0 && d < 12) ? sb + 0x30 + d * 0x1c : nullptr;
            Logging.Log("[mk8dx-8p] A2   e%d tipo=%d/%d pers=%d var=%u carr=%d gomme=%d ali=%d d=%d | desc pers=%d carr=%d gomme=%d ali=%d tipo=%d",
                j, *(int32_t*)(e + 0x224), *(int32_t*)(e + 0x44), *(int32_t*)(e + 0x21c), e[0x220],
                *(int32_t*)(e + 0x210), *(int32_t*)(e + 0x214), *(int32_t*)(e + 0x218), d,
                ds ? *(int32_t*)(ds + 0xc) : -9, ds ? *(int32_t*)(ds + 0x0) : -9, ds ? *(int32_t*)(ds + 0x4) : -9,
                ds ? *(int32_t*)(ds + 0x8) : -9, ds ? *(int32_t*)(ds + 0x14) : -9);
        }
        for (int i = 0; i < n && i < 8; i++) {                     /* vista per slot, con il percorso del gioco */
            uint8_t* pad = mk8dx::Fn<uint8_t* (*)(int)>(0x8bf158)(i);
            int e = mk8dx::Fn<int (*)(uint8_t*)>(0x8be03c)(pad);  /* -1 per i>=4 finché cmp #3 (collega C) */
            Logging.Log("[mk8dx-8p] A2   slot %d -> pad %p idx=%d -> e=%d", i, pad, pad ? *(int32_t*)(pad + 8) : -1, e);
        }
    }
}
HOOK_DEFINE_TRAMPOLINE(KartFrameHook) {                           /* 0x4c2820(page) per frame pagina kart */
    static void Callback(uint8_t* page) {
        uint8_t before = page[0x498];
        Orig(page);
        if (!before && page[0x498]) a2::LogAssigned(*(int32_t*)(page + 0xe8) == 0x31 ? "kart Multi -> avanti" : "kart -> avanti");
    }
};
/* exl_main: verificare la prima parola come per MenuEventHandlerInsn: 0x4c2820 = a9bc5ff8 (stp x24,x23,[sp,#-0x40]!),
   poi KartFrameHook::InstallAtOffset(0x4c2820). Altre prime parole: 0x4ba77c d10203ff, 0x4badb0 d10243ff,
   0x3fd5a4 d101c3ff, 0x3fdd70 d10203ff, 0x422efc d101c3ff, 0x8bf0ec f81e0ff3 [VERIFICATO in main.asm] */
```
Accettazione (N = 5..8): N entry con `tipo=0/…`, personaggio e parti ≠ -1 in entry e descrittore, `sB+0x188[0] == N`,
`sB+0x180 ≥ N`; righe "slot i → e" con e distinti e ≥ 0 per i < N (richiede C+B). Regressione 1-4P: le stesse
righe con N ≤ 4 devono coincidere con un run senza la mod per gli stessi personaggi.

## 5. 0x422efc [VERIFICATO] (`a2_422efc.asm`)

Corpo (0x422efc-0x423054): tocco del pad UI k = 0..3 con `0x8be478(0x8bf158(k), c)`, dove
c = `count != 0`, `count > 1`, `count > 2`, `count > 3` (unsigned, `cset ne/hi`). Poi i puntatori
`0x8bf11c(&k)` (`0x8bfcc0()+8+k·8`, clamp 10) → `0x8bfb20(ptr, abil, 0, &-1)` con m = `(count > 1) && flag`:
k=5 abil = !m; k=1 m && count != 0; k=2 m; k=3 m && count > 2; k=4 m && count > 3.
**Non tocca i pulsanti (+0x1c)**, solo tocco e puntatori. Chiamanti: 0x3ffbb8, 0x4b82a0 (1,0), 0x4bbcd8 (1,0),
**0x4c17c4 (N,0)**, 0x4c3c94 (1,0), 0x4d4008, 0x4d5838, 0x4dc1b0, 0x4dcaf8, 0x53e488, 0x53e818.
Con flag = 0 (tutti i chiamanti del flusso) i puntatori per giocatore 1..4 restano spenti e il 5 (condiviso)
acceso: si ottiene un solo puntatore, come in 1-4P. L'unica differenza per N > 4 è il tocco dei pad 4..7.

Proposta REPLACE (richiesta), con il pad tocco a indice 8 e gli extra del collega C:
```cpp
HOOK_DEFINE_REPLACE(TouchEnableHook) {                            /* 0x422efc(count, flag) – sostituisce l'originale */
    static void Callback(uint32_t count, uint32_t flag) {
        using PadFn = uint8_t* (*)(int); using OnFn = void (*)(uint8_t*, bool);
        int lim = (padui::active8 && count > 4) ? 8 : 4;          /* 1-4P: esattamente le 4 chiamate originali */
        for (int k = 0; k < lim; k++) {
            uint8_t* pad = mk8dx::Fn<PadFn>(0x8bf158)(k);
            mk8dx::Fn<OnFn>(0x8be478)(pad, k == 0 ? count != 0 : count > (uint32_t)k);
        }
        bool m = (count > 1) && (flag & 1);                       /* w21 = w23 & w21 (0x422f94) */
        auto ptr = [](int k) { return mk8dx::Fn<void* (*)(int*)>(0x8bf11c)(&k); };
        auto set = [](void* p, bool on) { int none = -1; mk8dx::Fn<void (*)(void*, bool, int, int*)>(0x8bfb20)(p, on, 0, &none); };
        set(ptr(5), !m);
        set(ptr(1), m && count != 0);
        set(ptr(2), m);
        set(ptr(3), m && count > 2);
        set(ptr(4), m && count > 3);                              /* puntatori 6..9: semantica ignota, non toccati */
    }
};
```
Alternativa più sicura (consigliata): **TRAMPOLINE**, `Orig(count, flag)` e poi solo
`for k in 4..min(count,8)-1: 0x8be478(0x8bf158(k), true)` se `padui::active8` (e `false` per k ≥ count).
Così 1-4P resta byte per byte l'originale. Stessa logica per il tocco a fine frame della charsel
(0x4baaa0-0x4baaf4): trampolino su 0x4ba77c, dopo `Orig`, `0x8be478(pad k, [this+0x900] == k)` per k = 4..7.
**Il tocco serve solo al touchscreen** (pad+0x1d). Per giocare con i pulsanti nessuna delle due è
indispensabile [IPOTESI forte: 0x8be674 legge i pulsanti con +0x1c, indipendente da +0x1d].

## 6. Sintesi: modifiche A2, rischi, cose da verificare

### 6.1 Ordine proposto

**H-A2-0 (subito, anche senza 8 posti; corregge il bug visto nel log step 4)** – trampolino su **0x3fd5a4**
`(field*, page1d0*, i)` e su **0x3fdd70** `(field*)`. Nel primo: se `i ≥ 4` o `page1d0 == nullptr` → return
(niente lettura dall'entry NULL, niente scrittura a NULL+0x360). Nel secondo: eseguire `Orig` con cfg+0x1fa4
temporaneamente a `min(N,4)` e ripristinarlo subito dopo. In alternativa, REPLACE del ciclo con `i<min(N,4)`.
È solo una precondizione: i dati di profilo/Mii per i locali 4..7 si gestiscono con C+B.
```cpp
HOOK_DEFINE_TRAMPOLINE(ProfileToCharaWinHook) {                    /* 0x3fd5a4(cfg+0x1fa4, CharaWin+0x1d0, i) */
    static void Callback(int32_t* field, uint8_t* obj, int32_t i) {
        if (i >= 4 || !obj) { Logging.Log("[mk8dx-8p] A2 0x3fd5a4 saltata i=%d obj=%p", i, obj); return; }
        Orig(field, obj, i);
    }
};
HOOK_DEFINE_TRAMPOLINE(ProfileToEntriesHook) {                     /* 0x3fdd70(cfg+0x1fa4) cicla i<*field */
    static void Callback(int32_t* field) {
        int32_t n = *field;
        if (n > 4) *field = 4;                                     /* i 4..7: entry NULL / entry del locale 0 */
        Orig(field);
        *field = n;
    }
};
```
(Firme dedotte da 0x3fd5b8-0x3fd5c4 e 0x3fdd90/0x3fddec. Chiamanti: 0x3fd5a4 ← 0x469350, 0x4b7458; 0x3fdd70 ← 0x4b72dc,
0x4c086c (kart onEnter), 0x5324fc. Per N ≤ 4 l'hook non cambia nulla.)
Quando C e B forniranno entry validi per 4..7, il secondo hook potrà lasciare N intero.

**Strada A2-min (consigliata per lo step 5)**: interfaccia a 4 posti, slot 4..7 automatici.
1. Precondizioni: C (pad UI 4..7, `cmp #3→#7`) e B (raceMgr+0x17c a 8 / LocalMapHook), così che
   `0x8be03c(0x8bf158(i))` dia l'entry del locale i per i < 8.
2. Avanzamento charsel – trampolino su **0x4ba77c**: prima di `Orig`, se N > 4 e gli slot 0..3 sono tutti in
   stato 7, impostare cfg+0x1fa4 = 4 solo per quella chiamata (avanza a 0x4ba938 con w28 = 4 = w22, ramo
   senza applet) e ripristinarlo dopo. Negli altri frame N resta intero, quindi applet e pronto restano come
   nello step 4. Alternativa con patch: nessuna (non c'è un registro libero per min(N,4)).
3. Tipo dei locali 4..7 – trampolino su **0x4badb0** (vt87): leggere `stage = page+0x124` e
   `dest = 0x8ba5a0(page)` (link uscente +0x168 → +0xc) prima di `Orig`. Se stage == 1, dest ∈ [0x2f,0x31] e
   N > 4, dopo `Orig`: per i = 4..N-1 `e = 0x8be03c(0x8bf158(i))`, `0x42022c(e, &0)` e cfg+0x1434++
   (come 0x4bb134-0x4bb140).
4. Personaggio e parti dei locali 4..7 – nello stesso punto: `0x4202f4(e, &pers, var, 0)` e
   `0x420ac0/0x420b88/0x420c50(e, &v)`. Valori: copia del giocatore i-4, oppure una tabella del modulo.
   **I quattro campi devono essere ≠ -1**, altrimenti 0x86f074 azzera +0x180 (0x86f14c).
5. Pagina kart: nessuna modifica. Con +0x39c = 4 decidono solo i giocatori 1-4 e l'avanzamento ignora 4..7.
   Il log 4.4 controlla che le parti dei locali 4..7 siano ancora quelle del passo 4: 0x4c1bfc agisce solo
   sugli slot < 4.
6. Facoltativo: tocco per i pad 4..7 (§5, trampolino).
7. Log 4.4 + H-A2-0.

**Strada A2-full (dopo A2-min, con A1)**: 8 cursori.
- Selettore: riallocare con dimensione 0xb8 + 4·0x20 e spostare lo stato per giocatore. Oppure secondo
  selettore per i giocatori 4..7 con indice rimappato. Più 0x8b0084/0x8b01ec da estendere (trampolini che
  chiamano 0x8b00e0/0x8b0238 per p = 4..7, ma i 19 clamp `cmp #4` dentro rimandano a p = 0: da patchare insieme
  alla rilocazione). Più PageMgr+0xc68[4] (0x8bd5f8, ciclo 0x8bd6d8) e i cursori grafici 0x8bf228()+0x1d0[4].
- Pagine CharaWin 4..7: registrare 4 pagine extra con la fabbrica 0x40918c e id liberi (es. 0x64..0x67, da
  verificare). Trampolino su 0x8bf0ec (H-A2-4): se `*id ∈ [0x2c,0x2f]` e l'indirizzo di ritorno è in
  [0x4b1298, 0x4bc844) → `*id = extra[*id-0x2c]` (§2.3: non ci sono usi costanti di 0x2c..0x2f nell'intervallo).
  Slot = id−0x28 in 0x4b28d4 diventa sbagliato per gli id extra: patch o hook del ctor 0x4b28a4.
  CharaWin onEnter 0x4b29e0: dopo `Orig`, `+0x1a8 |= 1<<slot` per slot ≥ 4. Link +0x660: trampolino su 0x4bbd18
  che per ev+0x2c ≥ 4 riscrive temporaneamente `this+0x660+0xc` (id del link 0) con l'id extra.
- Stato +0x904 a 8 (A1), init slot 4..7 (riscrivere 0x4b80a0-0x4b8294 come ciclo in un hook dopo vt77),
  `+0x8c0 = N` impostato in vt77 prima di `Orig` (non nel ctor 0x4bcc70: la pagina viene costruita una volta).
- Pagina kart: +0x39c = N in onEnter 0x4c0898, +0x460 rilocato, azzeramento srotolato, selettori per slot 4..7
  (layout `L_KartSlot_04..07`: A1), cfg ombra §4.3.
- 0x49e960 (u8[4] di presenza in gara): step 6.

### 6.2 Patch e hook (riassunto)

| id | tipo | offset | orig → nuovo / firma | dipende da |
|---|---|---|---|---|
| H-A2-0a | trampolino | 0x3fd5a4 | `(int32_t*, uint8_t*, int32_t)`, skip i≥4 / obj NULL | – |
| H-A2-0b | trampolino | 0x3fdd70 | `(int32_t*)`, N clamp 4 | – |
| H-A2-1 | trampolino | 0x4ba77c | `(uint8_t* page)`, avanzamento con slot 0..3 a 7 | C, B |
| H-A2-2 | trampolino | 0x4badb0 | `(uint8_t* page)`, tipo/personaggio/parti 4..N-1 | C, B |
| H-A2-3 | trampolino | 0x4c2820 | `(uint8_t* page)`, log 4.4 | – (log), C+B (valori) |
| H-A2-5 | trampolino (o REPLACE) | 0x422efc | `(uint32_t count, uint32_t flag)`, tocco pad 4..7 | C |
| H-A2-4 (full) | trampolino | 0x8bf0ec | `(int32_t* id)` → rimappa 0x2c..0x2f per LR in charsel | pagine extra |
| P (full) | patch | 0x4c4c70 | `321e03e8` → dinamico via hook | rilocazione +0x460 |

### 6.3 Rischi di regressione 1-4P
- H-A2-0: con N ≤ 4 la condizione `i ≥ 4` non scatta. `obj == NULL` oggi non si verifica con N ≤ 4, perché le
  pagine 0x28..0x2b esistono. Lo stesso vale per il clamp di 0x3fdd70: nessun effetto.
- H-A2-1/H-A2-2: attivi solo con N > 4. 0x4badb0 e 0x4ba77c sono condivisi da Single/WiFi/Multi: il filtro
  N > 4 basta, perché cfg+0x1fa4 ≤ 4 in Single/WiFi.
- REPLACE di 0x422efc: è una riscrittura completa. Un errore cambia il tocco o i puntatori in tutti i menu,
  quindi preferire la versione trampolino.
- H-A2-4 (full): un indirizzo di ritorno sbagliato cambierebbe la pagina in 1-4P. Va attivato solo con
  `padui::active8`.
- Bit 4 di page+0x1a8: oggi indica il "pad tocco" (collega C). Accendere bit 4..7 per gli slot 4..7 (A2-full,
  CharaWin) può dare il tocco o il controllo al giocatore sbagliato. In A2-min non si accendono.

### 6.4 [DA VERIFICARE A RUNTIME]
1. H-A2-0: rifare il run 8g dello step 4 e controllare che spariscano Read64 @0x1d0, Write32 @0x368..0x384 e
   Read8 @0x29c (`a2_eden_unmapped_step4.txt`). Gli altri accessi nulli (Write8 @0x394 a 166 s/187 s, Read64
   @0x200, Write8 0x208/0x209, Write32 0x20c) vanno attribuiti con un watchpoint (sospetti: 0x3fdd70 e un
   lettore di GamePlayer). Con il debugger: breakpoint a 0x4b73f4 e 0x3fde34 con N = 8.
2. Stati 2 e 4: watchpoint in scrittura su `+0x904+i*8`. Mi aspetto nessuna scrittura (nessuno scrittore statico).
3. Ruolo di +0x1c (pulsanti) e +0x1d (tocco) del pad UI e del bit2 di pad+0x10 come "annulla" (stato 7→3).
4. A2-min: dopo il passo 2 la pagina kart non deve bloccarsi con N > 4 (avanza con i soli slot 0..3). Righe del
   log 4.4 con `sB+0x188[0] == N`. Poi verificare in gara che i locali 4..7 abbiano i personaggi assegnati
   (dipende da B per GamePlayer e da step 2 per le viste).
5. Pagine con id 0x64..0x67 davvero libere (tabella a 0x88 voci, dump di PageMgr+0x30 a runtime).
6. Che 0x86f074 sia l'unico scrittore di settingsB+0x180 nel percorso menu → gara (watchpoint).
