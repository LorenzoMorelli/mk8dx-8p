# Step 1 – catalogo "max 4 giocatori locali": MENU / FLUSSO, SELEZIONE PERSONAGGI, GIOCATORE→KART

Main 4.0.1 (BID 158C0C73…), offset nel main decompresso; IPS = +0x100. Dati grezzi in `step1_data/menu_*.txt`.
Nessuna modifica al gioco.

## 1. Flusso dal pulsante "N giocatori" alla gara [VERIFICATO]

Pagina modalità 0x4d7ba4 (stringhe `L_Single_00/L_Multi_00/L_Online_00/L_LocalCom_00`, `L_SubBtn_00..06`) crea
4 sotto-pagine "numero giocatori" (classe ctor 0x437e24, alloc 0x418 a 0x5902d0) con parametro 0/1/2/3
(0x4d7c74/0x4d7cd8/0x4d7d38/0x4d7d9c → `str w8,[x20,#0x3cc]` a 0x437e6c). La pagina 0x4383c4 fa switch su
[this+0x3cc] (tabella 0xf27094):

```
stato 0 -> 0x438860 : 1 pulsante (L_Btn_00)                [Single]
stato 1 -> 0x43891c : 7 pulsanti L_Btn_00..06 + T_Num_00   [Multi: 2..8 giocatori, i 5-8 sono sempre creati]
stato 2,3 -> 0x438410 : 2 pulsanti (L_Btn_00, 01)          [Online 1-2]
stato 4,5 -> 0x438578 : 4 pulsanti (L_Btn_00..03)          [LocalCom 1-4]
stato 6,7 -> 0x43882c : 1 pulsante
```
I pulsanti 5-8 non dipendono da flag: la pagina è stata estesa nel 4.0.1 (+1076 B, step3) e basta.

Gestore eventi 0x4d92e8 (this, ev): ev+0x8 = id, ev+0x28 = tipo. Tabella 0xf295e8 (id-0x14) → casi:
- id 0x15..0x16 → 0x4d9774: `w21` = count (2 o 3/4), `w22` = modo; a 0x4d99a8 **`str w21,[cfg+0x1fa4]`**,
  poi `0x869180(raceCfg=0x8ace28(), count, 1, 0)` e 0x13d56c(modo).
- id 0x17..0x1a (5g-8g) → 0x4d9460: dialogo, count NON memorizzato (0x4d9894 `cmp w21,#4; b.gt 0x4d9928`
  mette w22=0 per il ramo generico; il ramo 5-8 non ci arriva).
- Config singleton (0x12fae70/e78/e80 → +0x30): +0x1fa4 = numero giocatori locali (int, ctor 0x3fd32c);
  scrittori: 0x4d99a8 (diretto), e 11 siti via `add x0,cfg,#0x1fa4; bl 0x3fd5a4` (0x469340, 0x490de4,
  0x4b72d4, 0x4b7384, 0x4bb898, 0x4bbc14, 0x4bbc88, 0x4c0864, 0x4c3c14, 0x4c9318, 0x5324f4, 0x5353fc:
  `step1_data/menu_1fa4_readers.txt`). 0x3fd5a4(field*, src, count) chiama 0x8bf158(count) e 0x8be068.

### 1.1 Lettori di cfg+0x1fa4 (25 siti, `menu_1fa4_readers.txt`, `menu_1fa4_reader_strings.txt`)
| funzione | uso | sottosistema | limite |
|---|---|---|---|
| 0x44433c | loop i<count: 0x425888(i) (`T_Text_00`) | menu: testo per giocatore | nessuno |
| 0x492204 (0x4922dc/0x49235c), 0x492380 | loop i<count: 0x8bf158(i)/0x8be248 → se controller mancante → **0x49ea60(count,count,1,0)** applet | lobby controller | 0x8bf158 clamp 5 |
| 0x4925bc | idem (`mn_Page_Dialog_Shop`, `L_WinBtn_*`) | dialogo shop/corse | – |
| 0x4b1654 | `ldrb [x19,#0x208]` (`SE_SYS_FLAG_OPEN_WAIT`) | selezione personaggi: apertura | – |
| 0x4b7218, 0x4b7de8, 0x4b94d8 (×2), 0x4ba950, 0x4bac00, 0x4bb6a0 | loop i<count sugli slot | selezione personaggi | array +0x904 (4) |
| 0x4c0898 (`L_KartPartsOKWin_00`, `L_Glider_00`), 0x4c1710 (`Parts`), 0x4c3634 | selezione kart/parti; 0x4c1710 chiama **0x422efc(count,0)** | selezione kart | 0x422efc = 4 controller |
| 0x4d3f58 (`PlayerNum`, `L_NumTeamMember_01`) | squadre | menu regole | – |
| 0x4dc2e4, 0x4dc8a4 (`mn_WifiTop_00`) | online | menu wifi | – |
| 0x4df29c (`mn_Page_WiFi_Matching`, `mn_L_CountDown_00`) | matching | online | – |
| 0x4fb290, 0x4fc2c0, 0x4fccf8, 0x4fd2d8 | `bl 0x869760(raceCfg, count)` (loop sulle entry ≤12, +0x44 tipo) | [IPOTESI] risultati/classifica | entry 12 |
| 0x53e3e0 (`SE_RC_PAUSE_OFF`) | pausa in gara | HUD/pausa | – |

Il campo è un intero: nessun lettore lo confronta con 4; i limiti stanno negli array indicizzati con esso.

## 2. Race-config (oggetto di 0x8ace28 = [0x8aba80()+0x800]) [VERIFICATO]

| offset | tipo | significato | prova |
|---|---|---|---|
| +0x00.. | 12 descrittori stride 0x1c (campi +0x30,+0x34,+0x38,+0x3c = -1 se vuoto, +0x44 tipo: 4 = vuoto, bit1 = umano) | slot kart | 0x86edb4..0x86ee88 (`madd x12,x13,#0x1c,x19`) |
| +0x50 / +0x58 | u32 count / ptr → 16 puntatori a entry (entry: +0x22c indice kart, +0x230 flag -1, +0x44 tipo, +0x60/+0x64/+0x68) | entry kart | 0x869040 (loop 16), 0x869180 |
| +0x180 | u32 totale slot validi | | 0x86ee4c |
| +0x184 | u32 slot umani (tipo&2) | | 0x86ee70 |
| +0x188[5] | u32 giocatori per console 0..4 (indice clamp 5) | **0x86f154 ritorna [+0x188] (console locale) o [+0x19c] se modo==4** | 0x86ee74..0x86ee84, 0x86f154 |
| +0x1cc[12] | u32 posizione kart → indice entry | 0x86e8e4(i), i ≤ 11 | 0x869138..0x86916c, 0x869be8 |
| +0x1f4..+0x1cc | init -1 | | 0x869104 |

0x869180(cfg, count, flag, flag2): se count<0 → 0x868f00(); crea/aggiorna `count` entry (0x8650e4, 0x864b58)
nell'array +0x58. Nessun confronto con 4: il race-config regge 8 locali per costruzione (12 slot, 16 entry,
contatore per console intero). Limite reale a valle: 0x7e76b8 (viewport 5) e input (0x422efc).

## 3. Classe selezione personaggi [VERIFICATO]

Gerarchia: base ctor 0x4bc844 (vtable 0x1278c28, GOT 0x1309338); derivate: **Single** 0x4bc80c (vtable
0x1278340, `+0x8c0 = 1`), **WiFi** 0x4bcad8 (0x1278638, `+0x8c0 = 2`), **Multi** 0x4bcc4c (0x1278930,
`+0x8c0 = 4` a 0x4bcc70). Allocazione: factory 0x4091d0/0x4091fc/0x409228, **size 0xa28** (`mov w0,#0xa28`),
align 4, allocatore 0x5902d0. Pagina layout: 0x4be854 sceglie `mn_Page_Lyt_CharaParts_{Single,WiFi,Multi}`
da 0x12797f0 (3 voci, `stato-0x14 > 2 → NULL`; classe 0x4be890, vtable 0x1279030).

Layout dell'oggetto (`menu_charasel_arrays.txt`, istogramma offset in `step1_data`):
| offset | dim | significato | accessi |
|---|---|---|---|
| +0x1a8 | u32 bitmask | giocatori pronti (bit i) | 0x4b81b4 |
| +0x398 | ptr | oggetto finestra kart (0x4bdb94) | 4 |
| +0x8c0 | s32 | numero slot (1/2/4) | 34 letture, 3 scritture (ctor) |
| +0x8c4/+0x8c5/+0x8c8 | u8/u8/u32 | stato pagina | 14/12/14 |
| **+0x904[4]** | stride 8: +0 stato slot (-1 vuoto, 1 … 3 confermato), +4 valore | **array per giocatore** | 20 accessi indicizzati + 17 clamp `cmp x,#4; csel ..,xzr,lo` |
| +0x928..+0x9c8 | campi vari | dopo l'array | ~25 siti |
| +0x9cc | s32 | indice slot corrente | 0x4b94d8 |
| +0x9e0,+0x9e8,+0x9f0,+0x9f8,+0xa00,+0xa08,+0xa10,+0xa18,+0xa20 | s32[2] | [IPOTESI] parti scelte per i 2 giocatori online (indice clamp 2 a 0x4b7894; legge cfg+0x1fb4) | 0x4b7830 |
| 0xa28 | fine oggetto | | alloc |

I 17 clamp patchati da `ips_A` (`menu_ipsA_sites.txt`) sono tutti `cmp xIdx,#0x4; csel xIdx,xIdx,xzr,lo;
add x8,x19,xIdx,lsl #3; ldr/str [x8,#0x904]`: il limite è la **dimensione dell'array +0x904 (4×8 = 0x20 B)**,
non una costante di logica. Le 95 `cmp #4/#5` della classe (0x4b1000-0x4bf800, `menu_charasel_cmp4_funcs.txt`)
si raggruppano in 30 funzioni; le più dense: 0x4b9a5c (4), 0x4b80c4 (4: inizializza gli slot a 0x4b80e4/
0x4b80f4 con `csinc` sul successivo), 0x4ba77c (0x4ba860 clamp + 0x4ba874 `cmp w8,#5` stato), 0x4b7590,
0x4b7830, 0x4ba3bc, 0x4ba574, 0x4b99c8, 0x4bbe70, 0x4bc1a4, 0x4be760/0x4be794 (classe layout CharaParts).
Le funzioni 0x4b11cc/0x4b19a4 usano array +0x218 (stride 8) e +0x2a8 (stride 16) di un altro oggetto
(finestra selezione) [IPOTESI]: da verificare nello step 5.

Perché ips_B crasha (0x4bd9dc, finestra animazioni `In_Win/Out_Win/…`): con `+0x8c0=8` e 4 slot nell'array,
o con il layout 8pl al posto del Multi, la ricerca dei pane `L_KartSlot_0i`/`L_CameraPlay_0i` (0x4bee60,
null-safe) e delle finestre per slot restituisce NULL per i ≥ 4 → deref a 0x4bd9dc [IPOTESI con dati].

## 4. Assegnazione giocatore locale → controller/kart [VERIFICATO]
- Controller: 0x422efc(count, x): per i = 0..3 `0x8bf158(i)` + `0x8be478(ctrl, count > i)`: **4 iterazioni
  fisse** (cset `hi` su 0/1/2/3); chiamato da 0x4c1710 (selezione kart) con cfg+0x1fa4.
- Lobby: 0x492380 richiede l'applet con `(count, count, 1, 0)` → 0x49ea60 (legge cfg+0x1fa8 a 0x49eb4c,
  `cmp #7`); con count 5-8 l'applet non può essere soddisfatto perché 0x7c53ec registra 5 NPad id.
- Kart: 0x869180 crea `count` entry; 0x869040 ricostruisce +0x1cc[12] (posizione → entry); 0x7e76b8 usa
  0x86f154 (= cfg+0x188[0]) e 0x86e8e4(i) per i viewport.

## 5. Tabella riassuntiva

| indirizzo | funzione | cosa limita | tipo | alloc/ctor | modifica proposta | rischio | dipendenze |
|---|---|---|---|---|---|---|---|
| 0x4d9460..0x4d9490 | 0x4d92e8 | 5g-8g → dialogo | logica | – | hook: per id 0x17..0x1a eseguire il ramo 0x4d9774 con w21 = id-0x13 (5..8) e w22 = modo multi (2) | basso | step 4 |
| 0x4d9894 | 0x4d92e8 | `cmp w21,#4; b.gt` | costante | – | togliere/alzare a 8 | basso | – |
| 0x4d99a8 | 0x4d92e8 | `str count,[cfg+0x1fa4]` | – | ctor 0x3fd32c | nessuna (int) | – | – |
| 0x869180 / 0x869040 | race-config | entry 16, slot 12, console 5 | array fissi, già ≥8 | 0x8aba80()+0x800 | nessuna | – | – |
| 0x422efc (0x422f28..0x422f80) | abilita controller | 4 iterazioni | logica/costante | – | hook: loop 0..7 (dipende da 0x8bf158 esteso) | medio | step 3 |
| 0x492380 → 0x49ea60 | lobby applet | (count,count) | logica | – | ok se NPad esteso | basso | step 3 |
| 0x4bcc70 | ctor Multi 0x4bcc4c | `+0x8c0 = 4` | costante | factory 0x4091d0/1fc/228 size 0xa28 | 4 → count (5-8) | basso | – |
| 0x4091d0/0x4091fc/0x409228 | factory pagine | size 0xa28 | alloc | 0x5902d0 | 0xa28 → 0xa68 (+4 slot ×8) | basso | – |
| +0x904 (17 clamp: 0x4b1d20 0x4b563c 0x4b80e4 0x4b80f4 0x4b81bc 0x4b8330 0x4b87d4 0x4b901c 0x4b94dc 0x4b9b78 0x4ba404 0x4ba5a4 0x4ba860 0x4bb108 0x4bb470 0x4bc018 0x4bc630) | classe selezione | array 4×8 | array fisso | ctor base 0x4bc844 | rilocare l'array in coda (+0xa28, 8 voci): patch imm `#0x904→#0xa28` e clamp `#4→#8` nei 17 siti + 20 accessi indicizzati; oppure hook delle 30 funzioni | medio (molti siti, ma meccanici) | size alloc |
| 0x4b81b4 (+0x1a8) | bitmask pronti | bit 0..3 | logica | – | nessuna (u32) | – | – |
| 0x4be854 / 0x12797f0 | pagina layout CharaParts | 3 voci | tabella | – | hook: stato Multi con count>4 → `mn_Page_Lyt_CharaParts_6pl/8pl` (nomi non nel main: fornirli dal modulo) | medio | step 5, L_CameraPlay assenti |
| 0x4bee60 (0x4bedf4) | ricerca `L_CameraPlay_00..03` | 4 nomi | logica | – | lasciare null-safe; non cercare nei layout 6pl/8pl | basso | – |
| 0x4bd9dc | animazioni finestra per slot | pane NULL per slot ≥4 | logica | – | servono finestre per 8 slot nel layout 8pl (presenti: `N_Window_00..07`) | medio | layout |
| 0x4b7830 (+0x9e0..+0xa20) | parti online | array 2 | array fisso | – | nessuna (non locale) [IPOTESI] | – | – |
| 0x4383c4 stato 1 | pagina numero giocatori | 7 pulsanti già presenti | – | ctor 0x437e24 | nessuna | – | – |

### Stima per questo sottosistema
Costanti: 3 (0x4d9894, 0x4bcc70, size 0xa28). Rilocazione array: 1 (+0x904, ~37 siti meccanici). Logica
nuova: 3 hook (ramo 5-8 nel gestore eventi, 0x422efc a 8, scelta pagina 6pl/8pl). Nessuna struttura del
race-config da estendere. Rischio principale: step 5 (layout 8pl senza `L_CameraPlay`, finestre per slot).
