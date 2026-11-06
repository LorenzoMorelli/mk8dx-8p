# Step 3 – input a 8 controller: piano di implementazione (analisi statica)

Main 4.0.1 (BID 158C0C73…), offset nel main decompresso (`../img/main.bin`). Nessuna prova a runtime in
questo documento: tutto [VERIFICATO statico] salvo dove scritto [IPOTESI] o [DA VERIFICARE A RUNTIME].
Dati grezzi: `step3_data (dati non pubblicati)/input3_*.txt` (disassemblati commentati per regione).

## 0. Correzioni allo step 1 (importanti per il piano)

| step 1 diceva | in realtà | prova |
|---|---|---|
| "InputSystem" con lista dispositivi/array controller | è **`sead::ControllerMgr`** (ctor 0x61900c, vtable 0x12afc68): lista dispositivi `OffsetList` a +0x170 (count +0x180, offset nodo +0x184), **`PtrArray<Controller>` a +0x188** (count +0x188, **capacità +0x18c**, buffer +0x190). Singleton: `**(main+0x12fbcb0)` | input3_ctrlmgr.txt |
| capacità `[sys+0x18c]` "da leggere a runtime" | **7**, costante: `ControllerMgr::Parameter.controllerMax` scritto a **0x7e4468** `orr w8,wzr,#0x7` → `str w8,[x20,#0x220]`; `ControllerMgr::initialize` 0x619060 fa `allocBuffer([param+8])` (0x6190e8 → 0x5fb2a4). Oggi la lista contiene 6 voci (pad 0..3, handheld, misc) | 0x7e4448, 0x619060 |
| "array controller `[[0x1306e70]]+0x58` ptr[5]" | non sono controller nn::hid ma **pad UI** del menu: oggetto 0x38 B (ctor 0x8beff4) con `ptr[5]` a +0..+0x20, +0x28 ptr, +0x30 int; riempito a **0x3ffaec-0x3ffb90** con 5 oggetti 0x2c8 B (ctor 0x8bde24) indici 0..4; l'indice 4 è un pad "speciale" (flag +0x1c/+0x1d spenti, riacceso dalla selezione personaggi a 0x4b82a4/0x4c17c8). L'input dei pad UI 0..3 passa da raceMgr+0x17c → giocatore → +0x200 (giocatore di input, sotto) | input3_uipads.txt |
| "tabella globale 0x12fb230 clamp 5 (0x7caf0/0x7ce88)" | **non è input**: 0x7caf4/0x7ce88 sono tabelle enum→stringa dei BGM (`BGM_MENU_SINGLE_MULTI, …` a 0xed88a4) | dump 0x7cb10 |
| tabelle `race.[+0x98/+0xa0/+0xa8]` | appartengono al **gestore giocatori di input "PIM"** (`system+0x7f0`, getter 0x8ace0c), non a raceMgr | 0x7c5d88 |

## 1. Architettura input reale (tre livelli + gioco)

```
sead::ControllerMgr  (task; Parameter a owner+0x218: controllerMax=7, delegate 0x7e4554)
 ├─ dispositivi (+0x170): Touch 0x2940 B, NpadDevice 0x7128 B (9 slot × 0xc40, già 8+1)
 └─ controller (+0x188 PtrArray, cap 7):   pos  0..3  NpadController idx 0..3   (id 0x11)
                                            pos  4     NpadController idx 8 = handheld (id 0x11)
                                            pos  5     Misc 0x178 B (id 0x12)
PIM = gestore giocatori di input (system+0x7f0; base 0x180 B ctor 0x7c4dc4, vtable di gioco 0x11b8ab8)
 ├─ +0x50 FixedPtrArray<Player,4> (count +0x50, cap +0x54 = 4, buffer inline +0x60..+0x7f)
 │      GamePlayer 0x7b8 B (ctor 0xcb5b4 ⊃ Player 0x780 ctor 0x7cdf24): +0x760 sead::Controller*,
 │      +0x73c tipo/stato, +0x768 indice giocatore, +0x770 indice dispositivo, +0x78c stato riconnessione
 ├─ +0x38/+0x40 lista task (cap 5: 4 giocatori + 1), +0x88 oggetto 0x59c0 B
 ├─ +0x90 indice (posizione) controller handheld (= owner+0x270), +0xb0 indice misc (= owner+0x268)
 ├─ +0x98 int[7] / +0xa0 u8[7] / +0xa8 int[7]: tabelle per **indice dispositivo** (= posizione in lista)
 └─ +0x130/+0x138: voci 0x18 B per controller (dimensionate su count al momento dell'init: dinamico)
raceMgr (system+0x800): +0x178 n locali, +0x17c int[4] locale→giocatore, +0x18c int[16], +0x58 giocatori[]
      giocatore+0x200 = GamePlayer (PIM)  → +0x760 = sead::Controller → slot NpadDevice [+0x178]
Pad UI (menu): [[0x1306e70]]+0x58 → ptr[5]; idx ≤3 → raceMgr+0x17c → giocatore+0x200
```

Fatti chiave per l'estensione:
- **`getControllerByOrder(id, n)` 0x619268** restituisce l'n-esimo controller con quell'id nell'ordine della
  lista. Con id 0x11 l'ordine **4 = handheld** ed è usato come costante in 5 siti (0xca064, 0x7ce3f0,
  0x7ce514, 0x7ce6d8, 0x7ce7ec); con indice variabile = indice giocatore in 0x7ce47c, 0x7ce5a0, 0x7ce630,
  0x7ce740, 0x7ce8f0 (input3_order_callers.txt). ⇒ i controller nuovi vanno **accodati dopo il misc**
  (posizioni 6..9, ordine 0x11 = 5..8): l'handheld resta ordine 4 e le posizioni 4/5 (copiate in
  owner+0x270/+0x268 e PIM+0x90/+0xb0) non cambiano.
- **`findControllerPort` 0x619348** = posizione nella lista → indice delle tabelle PIM a 7 voci.
  Con 4 controller accodati le posizioni arrivano a 9 ⇒ **le tabelle vanno portate a ≥10 subito**, anche
  per 1-4 giocatori: `findFreeController` 0x7c63a0 → 0x7c5f7c legge `[+0xa8][pos]`/`[+0x98][pos]` per
  **tutte** le voci della lista (0x7c604c-0x7c6060).
- Controller di gioco: `sead::Controller::calc` **0x625540** (condiviso) salva `prevHold=[+0x114]`, chiama
  `calcImpl_` (vt+0x30 = 0x7c6b64 per NpadController) e `updateDerivativeParams` 0x6259b0. Campi
  (layout sead::ControllerBase, confermato dagli accessi): **+0x8 trig, +0xc release, +0x10 repeat,
  +0x14 flag puntatore, +0x114 hold, +0x120 stick L (float2), +0x128 stick R**, +0x138 id (0x11),
  +0x140 mgr, +0x148 OffsetList addon; gioco: **+0x178 npad idx**, +0x17c bit0 connesso, +0x17d stile.
- NpadDevice (`findDevice(mgr, 0xe)` = 0x619220): slot `s` (= npad id, 8 = handheld) a `dev + s*0xc40`;
  stato `GetNpadState` a +0x58: **+0x60 pulsanti u64**, +0x68/+0x70 stick, **+0x78 attributi (bit0 connesso)**;
  stile per slot a `dev + 0x30 + 4*s` (5 = nessuno); numero pad interrogati `dev+0x28`.

## 2. Modifiche – parte input (ordine di implementazione)

### 2a. Patch di istruzioni (in `exl_main`, prima dell'avvio del gioco; verificare la parola originale)

| # | offset | originale | nuovo | effetto |
|---|---|---|---|---|
| P1 | 0x7c54ac | `940e3a45` bl SetSupportedNpadIdType(tab 5 id, 5) | `d503201f` nop | resta in vigore la chiamata del ctor NpadDevice a 0x5fbdc8: {0..7, 0x20} (9 id). Equivale alla parte "9 npad id" di ips_v2 con 1 istruzione |
| P2 | 0x7c54b0 | `321e03e1` orr w1,wzr,#4 | `321d03e1` orr w1,wzr,#8 | NpadDevice interroga pad 0..7 + handheld (setter 0x5fc290 accetta ≤8; polling 0x5fc03c già generico) |
| P3 | 0x7e4468 | `32000be8` orr w8,wzr,#7 | `52800188` mov w8,#12 | capacità PtrArray controller 7 → 12 (6 + 4 + margine) |
| P4a | 0x7c5dc0 | `321e0be0` orr w0,wzr,#0x1c | `321a03e0` orr w0,wzr,#0x40 | PIM+0x98 int[16] |
| P4b | 0x7c5dcc | `32000be0` orr w0,wzr,#0x7 | `321c03e0` orr w0,wzr,#0x10 | PIM+0xa0 u8[16] |
| P4c | 0x7c5ddc | `321e0be0` orr w0,wzr,#0x1c | `321a03e0` orr w0,wzr,#0x40 | PIM+0xa8 int[16] |

(P4 cambia solo la dimensione di `new[]` 0x5fa9c0; le voci 7..15 si inizializzano con H3.)

### 2b. Hook

**H1 – creazione controller 4..7: trampolino su 0x7c53ec** `void(ControllerMgr* mgr, Out* out)`.
Dopo `Orig`, per `idx = 4..7` replicare il corpo del ciclo 0x7c54e4-0x7c5590 (senza `0x5fc2a0`, idempotente):
```cpp
auto New   = Fn<void*(*)(size_t, void*, int)>(0x5faba8);      // (size, heap=nullptr→corrente, align 8)
uintptr_t vt = *(uintptr_t*)(Base() + 0x130edc0);             // = main+0x12c6f80 rilocato
uint8_t* c = (uint8_t*)New(0x298, nullptr, 8);
Fn<void(*)(void*, void*, int)>(0xa7f1a0)(c, mgr, idx);         // ctor Controller: +0x178=idx, id 0x11
*(uintptr_t*)c = vt + 0x10;  *(uintptr_t*)(c + 0x180) = vt + 0x68;   // vtable NpadController (+secondaria)
Fn<void(*)(void*)>(0x8cd878)(c + 0x188);
*(uint32_t*)(c+0x1b8) = 0; *(uint32_t*)(c+0x1fc) = 0; *(uint32_t*)(c+0x204) = 0;
memset(c + 0x230, 0, 0x5c);
Fn<void(*)(void*)>(0x7c6a34)(c);                               // soglie stick
void* a = New(0x40, nullptr, 8);
Fn<void(*)(void*, void*, int)>(0xa7fad0)(a, c, 0);             // addon
Fn<void(*)(void*, void*)>(0x5faf8c)(c + 0x148, (uint8_t*)a + *(int32_t*)(c + 0x15c));
(*(int32_t*)(c + 0x158))++;
int32_t& n = *(int32_t*)(mgr+0x188); if (n < *(int32_t*)(mgr+0x18c)) ((void**)*(uintptr_t*)(mgr+0x190))[n++] = c;
else Logging.Log("[mk8dx-8p] lista controller piena");        // con P3 non deve accadere
```
`out` non va toccato (handheld/misc restano a posizione 4/5). Log: posizioni e `mgr+0x188` finale (atteso 10).

**H2 – log di accettazione: trampolino su 0x6192a4** (`ControllerMgr::calc`, vtable mgr slot 0xb0:
chiama vt+0x20 = calc su ogni dispositivo e poi su ogni controller). Dopo `Orig(mgr)`:
```cpp
uint8_t* dev = Fn<uint8_t*(*)(void*, int)>(0x619220)(mgr, 0xe);       // NpadDevice
for (int p = 0; p < *(int32_t*)(mgr+0x188); p++) {
  uint8_t* c = ((uint8_t**)*(uintptr_t*)(mgr+0x190))[p];
  if (*(int32_t*)(c+0x138) != 0x11) continue;                        // solo NpadController
  int id = *(int32_t*)(c+0x178); int s = id < 9 ? id : 0;
  uint32_t hold = *(uint32_t*)(c+0x114); uint8_t conn = *(uint8_t*)(c+0x17c) & 1, sty = *(uint8_t*)(c+0x17d);
  uint64_t raw = dev ? *(uint64_t*)(dev + s*0xc40 + 0x60) : 0; uint32_t attr = dev ? *(uint32_t*)(dev + s*0xc40 + 0x78) : 0;
  if (hold != last[s].hold || conn != last[s].conn || raw != last[s].raw)   // solo variazioni
    Logging.Log("[mk8dx-8p] in pos=%d npad=%d conn=%d stile=%d attr=%x hold=%08x raw=%016lx", p, id, conn, sty, attr, hold, raw);
}
```
più una riga ogni 600 chiamate (frame) con `dev+0x28` (pad interrogati, atteso 8) e count lista (atteso 10).
Il primo log conferma anche che 0x6192a4 è chiamata per frame [DA VERIFICARE A RUNTIME]. Alternativa
equivalente: trampolino su 0x625540 filtrando `+0x138 == 0x11`.

**H3 – tabelle PIM per indice dispositivo a 16** (`x0` = PIM in tutte; voci 0..6 gestite dal codice originale):
- 0x7c5d88 (init, vt+0x70) **dopo** `Orig`: per i = 7..15 `a98[i]=-1, a0[i]=0, a8[i]=0` (la coda `br vt+0xd0`
  = 0xca878 scrive solo PIM+0x180, non tocca le tabelle).
- 0x7c58d0 e 0x7c5794 (reset/riassegnazione) **prima** di `Orig`: stesso azzeramento 7..15 (poi `Orig`
  riassegna e può marcare le voci ≥7 tramite 0x7c5740/0x7c5710: farlo dopo cancellerebbe i marcatori).
- 0x7c4e50 (per-frame, chiamata da 0xca050) **dopo** `Orig`: per i = 7..15 la stessa logica delle voci 0..6:
  `if (a0[i]) { a98[i]=-1; a0[i]=0; a8[i]=0; }  if (a8[i]-1 >= 0) a8[i]--;`

Con P1-P4 + H1-H3 lo step 3 "input riconosciuto" è completo per il livello controller: 8 pad Eden →
NpadDevice slot 0..7 → NpadController idx 0..7 con hold distinti, handheld invariato.

### 2c. Giocatori di input 4..7 (passo 3b: necessario per menu/gara, non per il log di accettazione)

| # | sito | limite | metodo |
|---|---|---|---|
| G1 | PIM+0x50 `FixedPtrArray<4>` (cap +0x54=4, buffer inline +0x60) | 4 giocatori | dopo initPlayers di gioco **0xca3e0** (vt+0xc8): copiare i 4 ptr in un array statico del modulo [9], `PIM+0x58 = array`, `PIM+0x54 = 8` (mai liberato: è inline in origine, nessun `delete`) |
| G2 | 0xca3fc `52800500` mov w0,#0x28 / 0xca410 `528000a8` mov w8,#5 | lista task +0x38 cap 5 | `52800900` mov w0,#0x48 / `52800128` mov w8,#9 |
| G3 | creazione GamePlayer 4..7 (in G1) | – | come 0xca4b0-0xca52c: `0x5faae0(0x7b8, 8)`, `0xcb5b4(p)`, `p+0x28=PIM`, `p+0x30=[PIM+0x30]`, inserimento nella lista task (+0x38/+0x40/+0x4c), `0x7cdfac(p, idx)`, push in +0x50 |
| G4 | 0x7c5340, 0x7c6218, 0x7c62bc | bit +0x270/+0x610 sui giocatori 0..3 **senza controllo count** | trampolino: dopo `Orig` stessa operazione su 4..count-1 |
| G5 | 0x7c5044 (da 0x8aba58) | flag +0x270 bit0/1/2, `0x7ce140`, +0x39b/+0x275 per giocatore 0..3 | trampolino: replica per 4..count-1 |
| G6 | 0x7c60c0 (ritorna bool, da 0xcb698/0xcb808) | controlla 4 giocatori | trampolino: `r = Orig(); per i≥4 stessa condizione (vt+0x90 && +0x778 != -1 && +0x778 < w1 → false)` |
| G7 | 0x7c5794 / 0x7c58d0 (parte giocatori, `cmp #2/#3/#4`) | 4 | dopo `Orig`: `0x7ce168(p)` / `0x7ce2fc(p)` per 4..count-1 |
| G8 | ricerca per indice giocatore con `getControllerByOrder(0x11, idx)` (0x7ce47c, 0x7ce5a0, 0x7ce630, 0x7ce740, 0x7ce8f0) | ordine 4 = handheld | per idx 4..7 serve ordine idx+1: trampolino su 0x7ce8c4 (w1 = idx) e sulla funzione che contiene 0x7ce3a0 (w1 = idx) con `if (4 <= idx && idx < 8) idx++` [DA VERIFICARE: inizio funzione e uso di w1] |
| G9 | PIM di gioco +0x180 u8[4] (0xca878) e +0x184 u8[4] (0xca03c, pad 0..3 connessi) | 4 byte contigui | lasciati a 4: riguardano la commutazione pad↔handheld; per i giocatori 4..7 nessuna modalità portatile |

Fuori dall'input (step 4/5, elencati per completezza): raceMgr+0x17c int[4] (memset 0x50 a 0x868db8,
riempimento **senza limite** a 0x868df4-0x868e18: con >4 umani sfora in +0x18c), 0x86ebf8 `cmp #3`,
0x8be248/0x8be2e8/0x8be36c/0x8be3f0/0x8be674 `cmp #3`, pad UI ptr[5] (0x8bf040/0x8bf058/0x8bf158,
riempitore 0x3ffaec), 0x86a998/0x86aac8 (assegnazioni per locale srotolate a 4), selezione personaggi.

## 3. Applet controller e loop infinito

Cosa fa oggi 0x4ba77c (per-frame selezione personaggi, x19 = pagina): `N = config+0x1fa4`; conta gli slot
`+0x904[i]` (i < `+0x8c0`, 1/2/4) in stato 7; se il conteggio == N passa avanti (0x8ba5fc). Altrimenti, se
non occupato (+0x8c4/+0x8c5), per i = 0..N-1: `ui = 0x8bf158(i)` (clamp 5: i ≥ 5 → pad UI 0) e
`0x8be248(ui, N==1)`; se una restituisce falso → **`0x49ea60(N, N, 1, 0)`** (0x4baa5c-0x4baa6c) e reset.
0x8be248: `ui.idx = [ui+8]`; **idx > 3 → falso** (0x8be25c); `p = raceMgr+0x17c[idx]` (0x86ebf8, -1 → falso);
`gp = raceMgr+0x58[p]+0x200`; falso se `gp` è handheld (vt+0xa0 = 0xcbfd0: +0x73c==2 e npad==8),
altrimenti `gp->vt+0x80` (0xcc3b0: stato +0x78c ∉ {0,1}).
**Loop**: con N=5..8 il pad UI 4 ha idx 4 → 0x8be248 falso **ad ogni frame**, indipendentemente da cosa
fa l'applet → 0x49ea60 a ogni frame. L'esito dell'applet (`+0x311` ok, `+0x314` player_count, scritti a
0x861068/0x861070) non viene mai letto da questo flusso: il gioco ricontrolla solo con 0x8be248.

Cosa serve perché 5..8 siano accettati e "connessi":
1. Eden: `player_0..7_connected=true` (+ `\default=false`), un mapping per profilo. Il frontend
   (`applet_controller.cpp:37-38`) prende `max_players` dall'header senza limitarlo a 4 anche con l'arg
   "vecchio"; `QtControllerSelector` abilita OK solo se i giocatori spuntati sono tra min e max
   (`qt_controller.cpp:349-360`); `player_count` restituito = connessi P1-P8.
2. L'arg del gioco è la versione a 4 giocatori: SDK `ShowControllerSupport` (sdk.bin 0x24f194) imposta
   `arg_size = 0x21c` (= 7 + 4×4 colori + 1 + 4×0x81), 4 colori a +7..+0x16, testo spento (+0x17=0).
   Su Eden basta (colori 5..8 assenti = nessun bordo). Su console reale [IPOTESI] l'applet con arg 0x21c
   potrebbe limitare a 4 giocatori: servirebbe l'arg nuovo (0x430 B, 8 colori/testi) e una chiamata
   all'applet costruita a mano (il core SDK 0x24eeb0 scrive sempre 0x21c). Rimandato.
3. `take_over=1, left_justify=1`: dopo l'applet i pad connessi occupano npad id 0..N-1 ⇒ con P1/P2/H1
   i NpadController 0..N-1 li leggono subito (verificabile con il log H2).
4. Uscita dal loop (step 4): 0x8be248 deve dare vero per i = 0..N-1:
   - minimo per test: hook REPLACE su 0x8be248: `idx < 4 → Orig`; `4 ≤ idx < 8 → ` connesso il
     NpadController con `+0x178 == idx` (`+0x17c & 1`, oppure attributo bit0 dello slot). Da solo basta per
     uscire dal loop (i = 5..7 oggi ricadono sul pad UI 0 per il clamp 5), ma il conteggio slot pronti
     resta ≤ 4 (+0x8c0) → la pagina non avanza (comportamento di ips_C): serve lo step 4.
   - completo: pad UI ptr[9] (indice "speciale" spostato da 4 a 8: i 4 siti `orr w0,wzr,#4` prima di
     `bl 0x8bf158` a 0x4b82a4, 0x4bbcdc, 0x4c17c8, 0x4c3c98), `cmp #3`→`#7` in 0x8be248/…/0x8be674 e
     0x86ebf8, raceMgr+0x17c rilocato a 8, giocatori di input 4..7 (2c) collegati ai giocatori raceMgr.

## 4. Rischi di regressione 1-4P

| modifica | rischio | mitigazione / verifica |
|---|---|---|
| P1 (9 npad id) | Eden/console collegano anche il 5° profilo connesso (oggi player_0..4 connessi): diventa npad 4 → NpadController 4 (H1). Con l'applet min=max=N il dialogo Eden mostra più giocatori spuntati di max → OK disabilitato finché non se ne toglie uno | per i test 1-4P lasciare connessi solo N profili; nessun effetto sul gioco se il pad 4 non è usato |
| P2 (polling 8) | +4 `GetNpadState` per frame; slot 4..7 prima mai scritti | trascurabile; log H2 riga periodica |
| P3/P4 | heap +40 B (lista) e +48 B (tabelle) | allocati nello stesso heap dell'originale |
| H1 | 4 controller in più nella lista: `findFreeController` 0x7c63a0 e lo swap 0x7c64b8 li considerano (un pad 5..8 premuto può "prendere" un giocatore disconnesso) | comportamento coerente con 8 pad; ordine 0x11 dell'handheld invariato (4); posizioni misc/handheld invariate (5/4) |
| H1 senza H3 | **lettura/scrittura fuori dalle tabelle a 7 voci** per posizioni 7..9 (0x7c5f7c, 0x7c5710/0x7c5740) | H3+P4 obbligatori insieme a H1 |
| ordine di init | se l'init PIM 0x7c5d88 avvenisse prima di 0x7c53ec, +0x130 sarebbe dimensionata su meno controller | [DA VERIFICARE A RUNTIME]: log in H1 e in H3-init di `mgr+0x188` (atteso 10 in entrambi, H1 prima) |
| vibrazione | 18 target (9 pad × 2) già presenti; i controller 4..7 non hanno ancora nodi/target collegati | nessuna vibrazione su 5..8 (accettabile allo step 3) |
| G1-G8 (3b) | funzioni srotolate a 4 non estese → giocatori 4..7 non aggiornati; 0x7c5340/0x7c6218/0x7c62bc accedono a 4 giocatori senza count | applicare G4-G7 insieme a G1. Poiché G3 crea sempre 8 giocatori, PIM+0x50 vale 8 anche in 1-4P: i giocatori 4..7 senza controller devono restare inerti (stato +0x78c = 0); verificare gara 1P/2P/4P dopo il 3b |
| ips_v2 | P1/P2 sono la stessa sostanza di ips_v2 (verificato "con 4 giocatori funziona") | non installare insieme ips_v2 e il modulo |

## 5. Accettazione proposta (Eden)
1. `qt-config.ini`: player_0..7 connessi, tasti distinti; avvio, menu principale (nessuna partita).
2. Log atteso: `H1 controller 4..7 creati, lista=10`, `H3 init count=10`, riga periodica `pad interrogati=8`.
3. Premere un tasto per profilo: 8 righe `in pos=… npad=0..7 hold=…` distinte (pos 0..3, 6..9), handheld pos 4.
4. Regressione: gara 1P e 4P con soli N profili connessi, nessun dialogo applet inatteso, nessun crash.
