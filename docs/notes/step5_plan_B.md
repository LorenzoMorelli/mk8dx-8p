# Step 5 – sotto-obiettivo B: raceMgr a 8 locali e giocatori di input 4..7 (piano statico)

Main 4.0.1 (BID 158C0C73…), offset nel main decompresso `../img/main.bin`; IPS = offset + 0x100.
Solo analisi statica (2 nov 2026). Convenzione: **[VERIFICATO]** = letto nel disassemblato in questa sessione
(indirizzi citati), **[IPOTESI]** = dedotto, **[DA VERIFICARE A RUNTIME]** = serve un log in Eden.
Dati grezzi in `step5_data (dati non pubblicati)/`:

| file | contenuto |
|---|---|
| `b_racemgr_layout.asm` | alloc 0x238 (0x8ac370), ctor 0x8689b8, vtable, init 0x868b0c, reset 0x868a80, 0x86b0bc, getter 0x86ebf8/0x86e8e4, setter slot 0x86af68, allocatore slot 0x86a7f4 |
| `b_fill_869180.asm` | 0x869180 completo, 0x864fcc, 0x8650e4, 0x8651c4/0x865238, 0x86a594, i 5 riempimenti di +0x17c |
| `b_locals_readers.txt` | tutti gli accessi a raceMgr+0x178..+0x1cb e +0x214..+0x237 trovati + contesti dei lettori esterni |
| `b_unrolled_racemgr.asm` | funzioni srotolate sui locali 0..3: 0x868f00, 0x868fec, 0x8652f8/0x8653cc, 0x86a998, 0x86aac8, 0x86ac30, 0x86ad18/0x86adec, 0x86a430/0x86a48c, +0x214/+0x224, 0x551b28 |
| `b_pim_sites.asm` | PIM: ctor, vtable, init 0x7c5d88, initPlayers 0xca3e0, G4-G9, 0x7ce168, findFree 0x7c63a0/0x7c5f7c, 0x7c6484, 0x619268, 0x7ce38c, 0x7ce8c4, GamePlayer (vtable, macchina a stati) |
| `b_entry200_readers.txt` | lettori di entry+0x200 (GamePlayer*) e gestione del NULL |
| `b_rmtrack_py.txt`, `b_offscan_py.txt`, `b_ctxr_py.txt` | script usati (tracciatore euristico dei registri raceMgr/entry/PIM, scansione offset, contesti) |

---

## 0. Sintesi

1. **Perché +0x178 resta 4 con 8 entry** [VERIFICATO]: non c'è nessun `cmp #4` nel riempimento né a monte. Il menu
   chiama `0x869180(rm, 8, 1, 0)`; per le entry k..7 esegue `0x8650e4(e, &0)` → `0x864fcc(e, &tipo=0)` →
   `gp = 0x86a594(rm, e+0x48)`. 0x86a594 cerca un GamePlayer libero **solo tra gli indici 0..3** (srotolato) e
   `PIM+0x50` contiene comunque 4 giocatori: per la 5ª entry restituisce NULL e 0x864fcc **declassa l'entry a tipo 1
   (CPU)** (0x865024 `orr w8,#1` → 0x86502c `str w8,[x19,#0x44]`). Il riempimento conta solo `entry+0x44 == 0`
   (0x86963c) ⇒ 4 locali. Con 8 entry umane il riempimento senza limite sforerebbe in +0x18c: oggi non succede
   proprio perché 0x86a594 si ferma a 4.
2. **Layout raceMgr** [VERIFICATO]: oggetto da **0x238 B** (0x8ac370 `mov w0,#0x238`), ctor 0x8689b8.
   `+0x178` n locali, `+0x17c int[4]` locale→entry, **`+0x18c` slot→entry: 16 parole azzerate dal setter ma solo 12
   slot usati** (allocatore 0x86a7f4: slot 0..11; lettori 0x8821c0: `cmp x22,#0xc`), `+0x1cc int[12]` locale→kart,
   `+0x1fc/+0x1fd` byte, `+0x200..+0x213` flag (atomici +0x204/+0x208/+0x20c), **`+0x214 int[4]` e `+0x224 int[4]`
   per indice locale**, fine a 0x238. Nessun padding libero.
3. **Strategia raccomandata: (b) "rilocazione in loco della mappa slot"**: `+0x17c` diventa `int[8]`
   (+0x17c..+0x19b) **senza spostarlo**; la mappa slot→entry passa da +0x18c a **+0x19c int[12]** (+0x19c..+0x1cb),
   cioè esattamente nello spazio che le 5 `memset(+0x17c, 0xff, 0x50)` già coprono. Costo: **8 patch di immediato +
   1 `cmp` + 1 hook REPLACE (0x86af68) + 1 trampolino ctor**. Nessuna allocazione cambia; tutti i ~25 lettori diretti
   `+0x17c[i]` con ciclo su `+0x178` diventano corretti da soli. La (a) (tabella laterale) lascia quei lettori a
   leggere la mappa slot come se fosse locale→entry: scartata.
4. **Giocatori di input 4..7** (G1-G9 del piano step 3): tutti i siti confermati con parole originali. Correzioni:
   G8 riguarda **due** funzioni con indice variabile (0x7ce38c, w1 = indice locale; 0x7ce8c4, w1 = npad) e nessun'altra
   (unici chiamanti di 0x619268 con w2 variabile); 0x7c4e50 non tocca i giocatori; creare 8 GamePlayer in 1P è inerte
   (stesso stato dei giocatori 1..3 in 1P: legati al controller "misc", stato 0).
5. **0x86a594**: lo pseudocodice di step4_menu_plan §5.3 va corretto: il ciclo è su **tutte le 16 entry con `j !=
   entryId`** (non `j < entryId`) e senza controllo di `+0x50`. Invariante utile: **tipo 0 ⇒ entry+0x200 != NULL**
   (0x864fcc), quindi i lettori senza controllo del NULL sono sicuri.
6. Ordine: B1 (mappa slot) → B2 (PIM a 8) → B3 (0x86a594 a 8: da qui +0x178 = 8) → B4 (funzioni srotolate
   raceMgr). B3 **non** va attivato senza B1 (sforamento in +0x18c) né senza B2.

---

## 1. raceMgr +0x178 / +0x17c

### 1.1 Catena che produce +0x178 = 4 [VERIFICATO]

```
0x4d99bc  0x869180(rm, w1=N, w2=1, w3=0)                       (b_fill_869180.asm)
0x8691a4    0x7e73ec()+0x17c == 1 -> return                     (modo speciale, non il nostro)
0x8691bc    w22 = 0x868f00(rm)  (compattazione, ritorna n locali <= 4) ; N<0 -> N = w22 (mai: tutti i chiamanti N>=1)
0x869254    for i in [w22, N): 0x8650e4(e[i], &0, 0, 0) ; 0x864b58(e[i], 0)
0x8692c4..  for i in [N, 16): 0x864fcc(e[i], &2)  (assenti) ; 0x8694dc: for i<16 0x865238(e,0); i<N: 0x8651c4(e, slot=i)
0x869604    0x869040(rm)  (+0x22c delle entry, +0x1cc locale->kart, clamp 12)
0x86960c    memset(rm+0x17c, 0xff, 0x50) ; +0x178 = 0
0x869634    per ogni entry: tipo (+0x44)==0 -> +0x17c[+0x178++] = e+0x48, e+0x4c = indice locale ; altrimenti e+0x4c=-1
            se e+0x230 >= 0 -> +0x18c[e+0x230] = e+0x48
```
`0x8650e4` (0x865110-0x86511c) rilascia l'entry con `0x864fcc(e,&2)` e alla fine (0x8651c0) richiama
`0x864fcc(e, tipo)`. In `0x864fcc` (0x864fd8-0x86502c): tipo 0 e `e+0x200 == NULL` → `gp = 0x86a594(e+0x38, e+0x48)`
(0x864ffc-0x865004), `e+0x200 = gp`, `0x7ce99c(gp, e+0x208)`; **se gp == NULL il tipo diventa 1** (0x865024).
`0x86a594` (§3) restituisce NULL quando gli indici giocatore 0..3 sono già presi ⇒ le entry 4..7 diventano CPU.

Non ci sono clamp a monte: 0x4d99a8 scrive `cfg+0x1fa4 = w21` e passa `w1 = w21` (0x4d99b4). I 10 chiamanti di
0x869180 passano N costanti 1/2/4 o w21 (0x4d9588 4, 0x4d99bc N, 0x4d9b84 1, 0x4d9ce0 1, 0x4d9e3c 2, 0x4d9fa4 1,
0x4da10c 2, 0x4f9e6c 4, 0x4fa260 4, 0x868b08 1) [VERIFICATO]: **N < 0 non capita mai**, quindi il ritorno di
0x868f00 serve solo come indice di partenza del ciclo 0x869254.

### 1.2 Layout dell'oggetto [VERIFICATO]

| offset | tipo | significato | prova |
|---|---|---|---|
| alloc | 0x238 B | `0x5faae0(0x238, 8)` + ctor 0x8689b8, salvato in sub0+0x800 | 0x8ac370-0x8ac3dc |
| +0x38/+0x40/+0x4c | lista figli (task) | cap 16 | 0x868b3c-0x868c34 |
| +0x50/+0x54/+0x58 | FixedPtrArray<16> entry, buffer inline +0x60 | `0x5fb28c(rm+0x50,16,rm+0x60)` | 0x8689f0-0x8689fc |
| +0xe8/+0xf0 | PtrArray<16> oggetti 0x120 B, buffer inline +0xf8 | | 0x868a00-0x868a18 |
| +0x178 | s32 | n locali (0 nel ctor) | 0x868a24 (x19=rm+0xe8 → +0x90) |
| +0x17c..+0x19b | 8 × s32 = -1 nel ctor | **locali 0..3 + 4 parole iniziali della mappa slot** | 0x868a40-0x868a4c (stur x9 a x19+0x94..+0xac) |
| +0x18c..+0x1c8 | 16 × s32 | slot→entry; setter 0x86af68 azzera 16, scrive `+0x18c[slot]` (0x86b0b4) | 0x86af68-0x86b0b8 |
| +0x1cc..+0x1fb | 12 × s32 | locale→kart (0x86e8e4 `cmp #0xb`; 0x869040 clamp 12) | 0x86e8f4, 0x869108-0x86916c |
| +0x1fc / +0x1fd | u8 | flag (0 / 1 nel ctor); +0x1fd = "rilega per indice" (0x86aeb8) | 0x868a28-0x868a2c |
| +0x200..+0x213 | | +0x204/+0x208 flag atomici letti da 0x86b0bc; +0x20c scritto atomico da 0x862008 | 0x86b0cc, 0x86b164, 0x862008 |
| +0x214..+0x223 | 4 × s32 | opzione per locale (getter 0x425888 clamp >3 → 0, setter 0x4258c8, 0x8653cc → gp+0x37c/+0x6e4) | 0x4258a0, 0x865444 |
| +0x224..+0x233 | 4 × s32 | opzione per locale con toggle (0x119790/0x1197c0 `cmp #3`) | 0x11979c, 0x1197cc |
| +0x234..+0x237 | 4 B | non scritti dal ctor (l'ultimo `str xzr` copre +0x22c..+0x233) | 0x868a50/0x868a5c |

vtable ([0x130fe10]+0x10 = 0x12cf8a8): vt+0x28 = 0x868a80 (reset → `0x869180(rm,1,0,0)`), vt+0x70 = 0x868b0c (crea le
16 entry da 0x2b0 B, ctor 0x86459c, `e+0x28 = rm`), **vt+0x90 = 0x86b0bc** (flag +0x204 → 0x86adec, +0x208 → 0x86ad18),
vt+0x98 = 0x86b198 (`ret`).

Nota STRUCTS/step1: il ctor "0x7eae0" citato allo step 1 per +0x1cc è in realtà 0x07eaa4, **un'altra classe**
(ctor base 0x0797bc, vtable 0x12fb2a0, campi float): raceMgr+0x1cc viene messo a -1 da 0x869040, non da un ctor.

### 1.3 Tutti gli accessi a +0x178, +0x17c..+0x188 e +0x18c [VERIFICATO]

Metodo: tracciatore euristico dei registri (`b_rmtrack_py.txt`: valore di ritorno di `bl 0x8ace28`, metodi con
x0 = raceMgr, propagazione mov/add/csel, entry = `[rm+0x58][i]`) + scansione degli offset nelle funzioni che
chiamano 0x8ace28 o in 0x864000-0x870000 (`b_offscan_py.txt`). Elenco completo in `b_locals_readers.txt`.

**Scrittori**: i 5 riempimenti (memset 0x50 + ciclo): 0x868db8/0x868e00/0x868e30, 0x868e6c/0x868eb4/0x868ee4,
0x86960c/0x869654/0x869684, 0x869974/0x8699bc/0x8699ec, 0x86b8c8/0x86b910/0x86b940; ctor 0x868a40-0x868a4c;
setter slot 0x86af68 (+0x18c..+0x1c8) e 0x86b0b4.

**Lettori di +0x178** (11 fuori dai riempimenti):

| sito | uso | con 8 locali |
|---|---|---|
| 0x388d0c | telemetria (play report) | valore 8 nel report |
| 0x38d3e0/0x38d3f8 | telemetria: "entry x21 è locale?" ciclo i<+0x178 su +0x17c[i] | ok con (b) |
| 0x420d6c | `>= 2` → 0x86593c | invariato |
| 0x420eb4, 0x7c904c | `== 1` (0x7c9084 SetNpadHandheldActivationMode) | invariato |
| 0x88538 | `== 3` → modo 3, altrimenti 4 | 8 → ramo "4" |
| 0x501ef8/0x501f14 | ciclo su +0x17c[i]: minimo di entry+0x58[...]+8 | ok con (b) |
| 0x50e190/0x50e1b4, poi 0x50e2c4 | ciclo: locale migliore, poi +0x17c[w19] | ok con (b) |
| 0x53b018/0x53b03c, poi 0x53b0cc | idem | ok con (b) |
| 0x551b6c/0x551b90 | ciclo su +0x178 ma array per locale **u64[4] a x19+0x10 con clamp `cmp x21,#4; csel → 0`** (0x551bc4, 0x551bec, 0x551c0c) | **locali 4..7 sovrascrivono l'indice 0** (in memoria, non fuori limite): pagina di gara, step 6 |

**Lettori di +0x17c..+0x188 a indice fisso (srotolati a 4: vedono solo i locali 0..3)**:
0x3858fc, 0x386e80, 0x387234, 0x38c31c (telemetria, solo +0x17c = locale 0); 0x50f288-0x50f2f4; 0x518a38/0x518a60;
0x534e10-0x534ed0; 0x541b50-0x541bc8; 0x543ce0, 0x543f6c (locale 0); 0x868f14-0x868f80 (compattazione, §1.5);
0x869704 (locale 0); 0x86a48c-0x86a54c; 0x86a9bc-0x86aa70; 0x86aae0; 0x86ad28-0x86adac; 0x86ae00-0x86ae8c.
**Indicizzati da variabile**: 0x4236dc (w22 = argomento), 0x86a450 (indice da 0x86a48c), 0x86ec20 (getter 0x86ebf8),
0x551d28 (ciclo `cmp x26,#4` a 0x551d60), 0x552210, 0x553024, 0x5532e0, 0x553660, 0x553c2c (pagine di gara: indice
calcolato, cicli fino a 4 nelle stesse funzioni [IPOTESI: step 6 HUD]); 0x86ab60 / 0x86ac68 (cicli 4, §1.5).

**Chiamanti di 0x86ebf8** (12, tutti pad UI, già bloccati da `cmp #3` prima della chiamata): 0x8be050, 0x8be080,
0x8be0d4, 0x8be140, 0x8be1b4, 0x8be264, 0x8be304, 0x8be390, 0x8be408, 0x8be6c4, 0x8be79c, 0x8beb98.

**Lettori di +0x18c (mappa slot)**: solo 0x88236c e 0x882408 (funzioni 0x8821c0/0x8823e0: ciclo sugli slot
0..11, `cmp x22,#0xc` a 0x8823d4) oltre al setter 0x86af68. Nessun altro accesso a +0x190..+0x1c8 su raceMgr
(gli altri `[x,#0x190]` della scansione sono system+0x190 o settings).

### 1.4 Le due strategie

**(a) tabella laterale + REPLACE 0x86ebf8 + riparazione di +0x18c** (step4_menu_plan §5.1): il riempimento
continua a scrivere +0x17c[4..7] dentro +0x18c[0..3] e +0x178 = 8; tutti i lettori diretti con ciclo su +0x178
(0x38d3f8, 0x501f14, 0x50e1b4, 0x53b03c, 0x551b90) leggerebbero per i ≥ 4 la mappa slot riparata (= entry degli
slot 0..3 = locali 0..3 duplicati); 0x86a998/0x86ad18/0x86adec andrebbero comunque riscritte sulla tabella; la
riparazione va fatta dopo ognuno dei 5 riempimenti. **Scartata**.

**(b) rilocazione** – varianti:
- (b1) spostare +0x17c[8] in fondo a un oggetto ingrandito (0x238 → 0x258): ~45 siti da ripatchare + le 5 memset
  non coprirebbero più l'array. Scartata.
- **(b2) RACCOMANDATA: +0x17c resta dov'è e diventa int[8]; la mappa slot va da +0x18c a +0x19c come int[12].**
  Gli slot sono 0..11 per costruzione (0x86a7f4 marca un `byte[12]` su stack e restituisce 0..11 o -1; lo slot
  scritto da 0x8651c4 è `i < N ≤ 8`), quindi int[12] basta e finisce a +0x1cb, prima di +0x1cc (locale→kart).
  Le 5 memset 0x50 coprono già +0x17c..+0x1cb = locali[8] + slot[12]. Nessun cambio di dimensione dell'oggetto.

### 1.5 Patch e hook per (b2) [VERIFICATO: parole lette nel main]

| # | offset | originale | nuovo | note |
|---|---|---|---|---|
| B1a | 0x868e30 | `b9018d8b` str w11,[x12,#0x18c] | `b9019d8b` str w11,[x12,#0x19c] | riempimento 1 |
| B1b | 0x868ee4 | `b9018d8b` | `b9019d8b` | riempimento 2 |
| B1c | 0x869684 | `b9018d8b` | `b9019d8b` | riempimento 3 (0x869180) |
| B1d | 0x8699ec | `b9018d8b` | `b9019d8b` | riempimento 4 (0x869760) |
| B1e | 0x86b940 | `b9018d8b` | `b9019d8b` | riempimento 5 (0x86b19c) |
| B1f | 0x86b0b4 | `b9018d01` str w1,[x8,#0x18c] | `b9019d01` | coda del setter (ridondante se B1h è REPLACE; patcharla comunque) |
| B1g | 0x88236c | `b9818d1a` ldrsw x26,[x8,#0x18c] | `b9819d1a` | lettore slot 0..11 |
| B1g' | 0x882408 | `b9818d15` ldrsw x21,[x8,#0x18c] | `b9819d15` | lettore slot 0..11 |
| B1i | 0x86ec08 | `71000e7f` cmp w19,#3 | `71001e7f` cmp w19,#7 | getter locale→entry a 8 (inerte finché i pad UI fanno `cmp #3`, sotto-obiettivo A) |

Le parole nuove sono `STR/LDRSW (imm)` con imm12 = 0x19c/4 = 0x67 (stessi registri).

**B1h – REPLACE 0x86af68** `void(rm, int id, int slot)` (prima parola `b9418c08`): l'originale azzera le 16 parole
+0x18c..+0x1c8 uguali a `id` e poi, se `slot != -1`, scrive `+0x18c[slot] = id` (0x86b0a8-0x86b0b4). Con la mappa
spostata le ultime 4 parole cadrebbero su +0x1cc..+0x1d8 (locale→kart): va sostituita.
```cpp
HOOK_DEFINE_REPLACE(SlotSetHook) {                       /* 0x86af68 */
    static void Callback(uint8_t* rm, int32_t id, int32_t slot) {
        int32_t* s = (int32_t*)(rm + 0x19c);             /* slot -> entry, 12 voci */
        for (int i = 0; i < 12; i++) if (s[i] == id) s[i] = -1;
        if (slot != -1) {
            if (slot >= 0 && slot < 12) s[slot] = id;
            else Logging.Log("[mk8dx-8p] B: slot %d fuori 0..11 (id %d)", slot, id);
        }
    }
};
```
**B1j – trampolino sul ctor 0x8689b8** (dopo `Orig`): `memset(rm + 0x19c, 0xff, 0x30)`. Il ctor originale mette
-1 su +0x17c..+0x19b (ora: i locali 0..7) ma non su +0x19c..+0x1cb (come già non lo faceva su +0x1a0..+0x1cb); la
prima `0x869180` (vt+0x28 → 0x868a80) azzera comunque tutto. Prudenza, costo nullo.

**B1k – 0x868f00 (compattazione, srotolata su +0x17c..+0x188)**: chiamata da 0x8691bc (sempre, arg2=1 nel menu) e
da 0x8698a0 (0x869760, risultati). Il valore di ritorno serve come indice di partenza in 0x869180 (0x869254: le
entry [ritorno, N) vengono **ricreate** con 0x8650e4); con il ritorno fermo a 4, rientrando nel menu con 8 locali le
entry 4..7 verrebbero ricreate ogni volta e i buchi tra 4..7 non verrebbero compattati. REPLACE (prima parola
`f81d0ff5`), stessa logica estesa a 8:
```cpp
HOOK_DEFINE_REPLACE(CompactLocalsHook) {                 /* 0x868f00 */
    static int Callback(uint8_t* rm) {
        auto swap = mk8dx::Fn<void (*)(uint8_t*, int, int)>(0x868cfc);   /* (rm, a, b): scambia e riempie */
        int k = 0;
        for (int i = 0; i < 8; i++) {
            int32_t id = ((int32_t*)(rm + 0x17c))[i];      /* rileggere: 0x868cfc riscrive l'array */
            if (id < 0) continue;
            if (id != k) swap(rm, k, id);
            k++;
        }
        for (int j = 0; j < 16; j++) {
            if ((uint32_t)j >= *(uint32_t*)(rm + 0x50)) continue;
            uint8_t* e = ((uint8_t**)*(uintptr_t*)(rm + 0x58))[j];
            if (e && (uint32_t)*(int32_t*)(e + 0x4c) <= 7) mk8dx::Fn<void (*)(uint8_t*)>(0x8653cc)(e);
        }
        return k;
    }
};
```
(equivalente all'originale per ≤ 4 locali: 0x868f14-0x868f9c fa `id>=0 && id!=k → 0x868cfc(rm,k,id); k++`, e
0x868fa0-0x868fd4 chiama 0x8653cc per `e+0x4c <= 3`.)

**B1l – stesse soglie altrove**: `cmp w8,#3` → `#7` (`71000d1f` → `71001d1f`) a **0x86901c** (0x868fec, da 0x4d8d54)
e **0x86b970** (coda di 0x86b19c). 0x8653cc per un locale 4..7 legge `+0x214[e+0x4c]` con clamp `cmp #3` (0x865444)
→ valore 0: innocuo (§2).

**Altre funzioni srotolate sui locali** (B4, dopo B2/B3; dettagli e pseudocodice in §5):
0x86a998 (rilega per indice locale), 0x86ad18 / 0x86adec (gp+0x76d), 0x86aac8 (`cmp x20,#4` a 0x86ac14), 0x86ac30
(`cmp x20,#0x10` a 0x86acdc), 0x86a48c (gp+0x73c == 0xb).

---

## 2. raceMgr +0x214 (0x8653cc, `cmp #3` a 0x865444) [VERIFICATO]

- `+0x214 int[4]` (+0x214..+0x223), subito dopo `+0x224 int[4]` (+0x224..+0x233); il ctor azzera +0x214..+0x233
  (0x868a30-0x868a70, x19 = rm+0xe8: `+0x12c..+0x14b`). Dopo resta solo +0x234..+0x237: **nessuno spazio per
  estenderli in loco**.
- Accessi a +0x214: getter **0x425888(i)** (`cmp w20,#3; b.hi → 0`, 0x4258a0), setter **0x4258c8(i, &v)** (`cmp
  w20,#4; csel → indice 0`, 0x4258e8), 0x425928 (ciclo su cfg+0x1fa4 con lo stesso clamp, 0x4259c8), 0x387f40
  (telemetria, locale 0), 0x8652f8/0x8653cc (`cmp w8,#3` a 0x865384/0x865444 → gp+0x37c e gp+0x6e4).
  Chiamanti del setter: 0x4abf1c/0x4abf40/0x4abf64/0x4abf88 con `w0` = 0,1,2,3 costanti (0x4abf18, 0x4abf38, 0x4abf5c,
  0x4abf80): **nessuna scrittura con indice ≥ 4** ⇒ nessuna corruzione dell'indice 0.
  Getter: 0x43a014.. (costanti), 0x4abd30.., 0x4e5f98/0x4e67e0/0x4e7d3c/0x51ba68 (`w0 = 0`), 0x51c690 (`[x19+0xc8]`),
  0x4443f4 (w20).
- +0x224: 0x119790/0x11979c (lettura, `cmp w20,#3`) e 0x1197c0/0x1197cc (scrittura del toggle `eor #1`).
- Significato [IPOTESI]: opzioni per giocatore locale (pausa/selezione: guida assistita, accelerazione automatica o
  simili); valore 0 = predefinito, che è anche quello che il gioco restituisce per indici > 3.

**Proposta per B: non estendere**. Con B1l i locali 4..7 passano da 0x8653cc e ricevono 0 (gp+0x37c = gp+0x6e4 = 0),
cioè il valore di default; il setter non riceve mai indici ≥ 4. Se in seguito (step 6, menu pausa per 8) servisse,
la via è ingrandire l'allocazione (0x8ac370 `52804700` mov w0,#0x238 → `52804b00` mov w0,#0x258) e spostare
+0x214[8] a +0x238 e +0x224[8] a +0x258… (0x258 + 0x20): ~12 siti (0x4258a0/0x4258ac, 0x4258e8/0x4258f4,
0x4259c8/0x4259d4, 0x865384/0x865390, 0x865444/0x865450, 0x119790/0x11979c, 0x1197c0/0x1197cc) – rimandato.

---

## 3. 0x86a594 – assegnazione del GamePlayer [VERIFICATO, disassemblato completo in b_fill_869180.asm]

```
0x86a594(rm x0, entryId w1):
  for k in 0..3 (4 cicli srotolati 0x86a59c / 0x86a5d8 / 0x86a618 / 0x86a658):
     used[k] = esiste j in 0..15, j != entryId, con gp = [rm+0x58][j]+0x200 != NULL e gp+0x768 == k
              (nessun controllo di j < rm+0x50 né di entry NULL: le 16 entry esistono sempre, vt+0x70 0x868b0c)
  k = primo indice non usato (0x86a6a0-0x86a6d8), se tutti usati -> NULL (0x86a6ac cbnz w8 -> 0x86a6f8)
  pim = 0x8ace0c(); return (k < pim+0x50) ? pim+0x58[k] : NULL      (0x86a6dc-0x86a6f8)
```
Chiamante unico: 0x865004 in 0x864fcc (tipo 0 con e+0x200 == NULL). 0x864fcc è chiamata da 17 siti (0x4a8b64,
0x8647bc, 0x864874, 0x86511c, 0x8651c0, 0x8651dc, 0x865c20, 0x869494, 0x8694b8, 0x869870, 0x869904, 0x869944,
0x869f4c, 0x86a154, 0x86a1d8, 0x86a358, 0x86b8b4), ma 0x86a594 viene raggiunta solo quando si chiede il tipo 0:
nel menu dalle `0x8650e4(e,&0)` di 0x869180 (entry [k, N)) e da 0x8651c4 (rimette il tipo corrente).

Verifica dello pseudocodice di step4_menu_plan §5.3: campo +0x768 = indice giocatore [VERIFICATO: 0x7cdfac
`str w1,[x0,#0x768]`, chiamata con 0..3 da 0xca508/0xca584/0xca600/0xca67c]; PIM+0x58 = buffer della
FixedPtrArray [VERIFICATO: 0x86a6ec]. **Errore**: il ciclo interno non è `j < entryId` ma `j != entryId` su 16.

**B3 – REPLACE 0x86a594** (prima parola `aa1f03e9`):
```cpp
HOOK_DEFINE_REPLACE(AssignGamePlayerHook) {               /* 0x86a594(rm, entryId) */
    static void* Callback(uint8_t* rm, uint32_t entryId) {
        uint8_t* pim = mk8dx::Fn<uint8_t* (*)()>(0x8ace0c)();
        uint32_t pc = *(uint32_t*)(pim + 0x50); void** pl = *(void***)(pim + 0x58);
        uint8_t** e = *(uint8_t***)(rm + 0x58);
        for (uint32_t k = 0; k < 8; k++) {
            bool used = false;
            for (uint32_t j = 0; j < 16 && !used; j++) {
                if (j == entryId || !e[j]) continue;
                uint8_t* gp = *(uint8_t**)(e[j] + 0x200);
                used = gp && *(int32_t*)(gp + 0x768) == (int32_t)k;
            }
            if (!used) return k < pc ? pl[k] : nullptr;      /* come l'originale: primo libero, NULL se oltre il count */
        }
        return nullptr;
    }
};
```
Con PIM a 4 giocatori il risultato è identico all'originale; con PIM a 8 e ≤ 4 locali anche (primo indice libero).

**Lettori di entry+0x200 e gp == NULL** (`b_entry200_readers.txt`, 54 siti): tutti i lettori esterni controllano il
NULL (`cbz`): 0x387f28, 0x423700, 0x86eafc (→ -1), 0x875120, 0x8752f8, 0x876358, 0x877934, 0x8697b4, 0x865a30,
0x865a8c, pad UI 0x8be100, 0x8be16c, 0x8be1e0, 0x8be290, 0x8be330, 0x8be3bc, 0x8be434, 0x8be6f0, 0x8be7c8,
0x8bebc4, raceMgr 0x86a4b0-0x86a570, 0x86a5ac.., 0x86a7b0, 0x86ac80, 0x864fec, 0x865044, 0x86506c, 0x86531c,
0x8653e0, 0x8646e8, 0x864700. Senza controllo: 0x864730/0x864748/0x864764 (0x8646bc: solo se `e+0x44 == 0` e
`e+0x40 ∈ {1,2,3}`), 0x865a58 (dopo il `cbz` di 0x865a34), 0x86ab04 (locale 0), 0x86aca4/0x86acc8 (dopo `cbz` a
0x86ac80), 0x8649f0 (copia nello scambio). **Invariante [VERIFICATO]: tipo 0 ⇒ gp != NULL** (0x864fcc declassa a
tipo 1), per cui i percorsi senza controllo sono sicuri anche con locali 4..7, purché B3 non restituisca mai un
puntatore non valido (con il count di PIM come limite: vero).

Con l'entry declassata (oggi): le entry 4..7 sono CPU (tipo 1), `e+0x4c = -1`, +0x178 = 4: è esattamente lo stato
osservato allo step 4.

---

## 4. Giocatori di input 4..7 (G1-G9) [VERIFICATO]

### 4.1 Sequenza di inizializzazione
*(estratto di disassemblato del gioco rimosso)*
Ordine confermato dal log dello step 3 (`eden_log_3b_ctrl8.txt`: "lista controller 10/12" prima di "tabelle PIM 16
voci"): **0xca3e0 gira dopo 0x7c53ec**.

### 4.2 Siti

| # | sito | parola originale | stato | proposta |
|---|---|---|---|---|
| G1 | PIM+0x50 FixedPtrArray<4>: ctor 0x7c4dc4 `0x5fb28c(pim+0x50, 4, pim+0x60)` (0x7c4df4-0x7c4e08); +0x80 byte subito dopo il buffer (0x7c5330, 0xca718, 0xca7bc, 0x7c63bc) | – | confermato | nel trampolino G3: `static void* pl8[8]`, copia dei 4 ptr, `PIM+0x58 = pl8`, `PIM+0x54 = 8` |
| G2 | 0xca3fc `mov w0,#0x28` / 0xca410 `mov w8,#5` | `52800500` / `528000a8` | confermato | `52800900` (0x48) / `52800128` (9) |
| G3 | creazione GamePlayer (0xca4b0-0xca52c, ×4) | – | confermato | trampolino 0xca3e0 (prima parola `f81d0ff5`), dopo `Orig` |
| G4 | 0x7c5340(pim, bit, set) (da 0x535304, 0x535d8c, 0x7e4fcc); 0x7c6218 (da 0x861e54); 0x7c62bc (da 0x86b13c) | `f9402c09` / `f9402c08` / `f9402c08` | giocatori 0..3 senza count | trampolini, dopo `Orig` stessa operazione su 4..count-1 |
| G5 | 0x7c5044(pim, w1, int* x2) (da 0x8aba58) | `f81e0ff3` | confermato | trampolino dopo `Orig` |
| G6 | 0x7c60c0(pim, w1) (da 0xcb698, 0xcb808) | `a9be4ff4` | confermato | trampolino |
| G7 | 0x7c5794 (0x7ce168 su 0..3) / 0x7c58d0 (0x7ce2fc su 0..3, poi 5× ControllerMgr vt+0xb0 a 0x7c5a00-0x7c5a54) | `f81e0ff3` / `f81e0ff3` | confermato (hook PimReset2/PimReset1 già presenti) | dopo `Orig`: stessa chiamata su 4..count-1 |
| G8 | 0x7ce38c (w1 = indice locale, `mov w20,w1` 0x7ce3b0; ordine variabile a 0x7ce47c, 0x7ce5a0, 0x7ce630, 0x7ce740; ordine 4 costante = handheld a 0x7ce3f0, 0x7ce514, 0x7ce6d8, 0x7ce7ec) e 0x7ce8c4 (w1 → 0x7ce8f0) | `a9ba6ffc` / `f81d0ff5` | **solo queste due** (gli altri 19 chiamanti di 0x619268 hanno w2 costante) | trampolini: `if (4 <= w1 && w1 < 8) w1++` |
| G9 | 0xca878 (+0x180 = 0x01010101), 0xca03c (+0x184..+0x187 = ordini 0..3 connessi, solo con handheld attivo) | – | confermato | non toccare |

`getControllerByOrder` 0x619268(mgr, id, n): n-esimo controller con `+0x138 == id` nell'ordine della lista
[VERIFICATO 0x619278-0x619298]. Con H1 l'ordine 0x11 è: 0..3 = npad 0..3, **4 = handheld**, 5..8 = npad 4..7 ⇒ per
indice locale/npad 4..7 serve l'ordine +1. Senza G8, il locale 4 verrebbe legato all'**handheld** e i locali 5..7
agli npad 4..6. In 0x7ce8c4 il w1 viene da 0x7c6484 (npad del controller scambiato, `cmp #8` a 0x7c64d4/0x7c64dc:
mai l'handheld) ⇒ lo stesso rimappaggio vale.

### 4.3 Pseudocodice (stile main.cpp)
```cpp
namespace pim8 {
    static void* pl[8];
    inline uint8_t* Pim() { return mk8dx::Fn<uint8_t* (*)()>(0x8ace0c)(); }
    inline int Count(uint8_t* p) { return *(int32_t*)(p + 0x50); }
    inline uint8_t* Player(uint8_t* p, int i) { return ((uint8_t**)*(uintptr_t*)(p + 0x58))[i]; }
}
HOOK_DEFINE_TRAMPOLINE(InitPlayersHook) {                     /* 0xca3e0 (PIM vt+0xc8) */
    static void Callback(uint8_t* pim) {
        Orig(pim);
        int n = *(int32_t*)(pim + 0x50);
        for (int i = 0; i < n && i < 8; i++) pim8::pl[i] = pim8::Player(pim, i);
        *(void***)(pim + 0x58) = pim8::pl; *(int32_t*)(pim + 0x54) = 8;           /* G1 */
        for (int idx = n; idx < 8; idx++) {                                         /* G3: come 0xca4b0-0xca52c */
            uint8_t* p = mk8dx::Fn<uint8_t* (*)(size_t, int)>(0x5faae0)(0x7b8, 8);
            if (!p) { Logging.Log("[mk8dx-8p] B: alloc GamePlayer %d fallita", idx); break; }
            mk8dx::Fn<void (*)(void*)>(0xcb5b4)(p);
            *(uint8_t**)(p + 0x28) = pim; *(uintptr_t*)(p + 0x30) = *(uintptr_t*)(pim + 0x30);
            int32_t& cnt = *(int32_t*)(pim + 0x4c);                                 /* lista figli +0x38/+0x40 (G2: cap 9) */
            void** buf = *(void***)(pim + 0x40);
            void** slot = (*(int32_t*)(pim + 0x38) > cnt) ? &buf[cnt] : &buf[0];
            if (!*slot) { *slot = p; cnt++; } else Logging.Log("[mk8dx-8p] B: lista figli PIM piena (%d)", cnt);
            mk8dx::Fn<void (*)(void*, int)>(0x7cdfac)(p, idx);                     /* +0x768 = idx */
            int32_t& c = *(int32_t*)(pim + 0x50);
            if (c < *(int32_t*)(pim + 0x54)) pim8::pl[c++] = p;
        }
        Logging.Log("[mk8dx-8p] B: giocatori PIM %d/%d, figli %d/%d", *(int32_t*)(pim + 0x50), *(int32_t*)(pim + 0x54),
                    *(int32_t*)(pim + 0x4c), *(int32_t*)(pim + 0x38));
    }
};
/* G4 */
HOOK_DEFINE_TRAMPOLINE(PimBitHook) {                          /* 0x7c5340(pim, bit, set) */
    static void Callback(uint8_t* pim, uint32_t bit, uint32_t set) {
        Orig(pim, bit, set);
        for (int i = 4; i < pim8::Count(pim); i++) { uint32_t& f = *(uint32_t*)(pim8::Player(pim, i) + 0x270);
            f = (set & 1) ? (f | (1u << bit)) : (f & ~(1u << bit)); }
    }
};
HOOK_DEFINE_TRAMPOLINE(PimSet8Hook) {                         /* 0x7c6218: |= 8 su +0x270 e +0x610 */
    static void Callback(uint8_t* pim) { Orig(pim);
        for (int i = 4; i < pim8::Count(pim); i++) { uint8_t* p = pim8::Player(pim, i);
            *(uint32_t*)(p + 0x270) |= 8; *(uint32_t*)(p + 0x610) |= 8; } }
};
HOOK_DEFINE_TRAMPOLINE(PimClr8Hook) {                         /* 0x7c62bc: &= ~8 */
    static void Callback(uint8_t* pim) { Orig(pim);
        for (int i = 4; i < pim8::Count(pim); i++) { uint8_t* p = pim8::Player(pim, i);
            *(uint32_t*)(p + 0x270) &= ~8u; *(uint32_t*)(p + 0x610) &= ~8u; } }
};
/* G5 – replica di 0x7c5050-0x7c532c per i >= 4 */
HOOK_DEFINE_TRAMPOLINE(PimFlagsHook) {                        /* 0x7c5044(pim, bool w1, int* x2) */
    static void Callback(uint8_t* pim, uint32_t w1, int32_t* x2) {
        Orig(pim, w1, x2);
        bool b0 = ((uint32_t)(*x2 - 7) < 2) || !(w1 & 1);
        uint8_t v = 0;
        if (w1 & 1) v = *(uint8_t*)(mk8dx::Fn<uint8_t* (*)()>(0x86f7bc)() + 0x26);   /* settingsA+0x26 */
        for (int i = 4; i < pim8::Count(pim); i++) {
            uint8_t* p = pim8::Player(pim, i); uint32_t& f = *(uint32_t*)(p + 0x270);
            f = b0 ? (f | 1) : (f & ~1u); f &= ~6u;
            mk8dx::Fn<void (*)(void*)>(0x7ce140)(p);
            p[0x39b] = v; p[0x275] = v;
        }
    }
};
/* G6 */
HOOK_DEFINE_TRAMPOLINE(PimOrderHook) {                        /* 0x7c60c0(pim, w1) -> bool */
    static bool Callback(uint8_t* pim, int32_t w1) {
        bool r = Orig(pim, w1);
        if (!r || w1 < 0) return r;
        for (int i = 4; i < pim8::Count(pim); i++) {
            uint8_t* p = pim8::Player(pim, i);
            bool waiting = (*(bool (**)(void*))(*(uintptr_t*)p + 0x90))(p);        /* 0xcc3ec: stato == 1 */
            int32_t o = *(int32_t*)(p + 0x778);
            if (waiting && o != -1 && o < w1) return false;
        }
        return r;
    }
};
/* G7: in coda ai PimReset2Hook (0x7c5794) / PimReset1Hook (0x7c58d0) esistenti, dopo Orig */
    for (int i = 4; i < pim8::Count(pim); i++) if (uint8_t* p = pim8::Player(pim, i)) mk8dx::Fn<void (*)(void*)>(0x7ce168 /* o 0x7ce2fc */)(p);
/* G8 */
HOOK_DEFINE_TRAMPOLINE(BindByLocalHook) {                     /* 0x7ce38c(gp, idx, a, b, c) */
    static void Callback(void* gp, int32_t idx, uint32_t a, uint32_t b, uint32_t c) { Orig(gp, (idx >= 4 && idx < 8) ? idx + 1 : idx, a, b, c); }
};
HOOK_DEFINE_TRAMPOLINE(BindByOrderHook) {                     /* 0x7ce8c4(gp, n, x2) */
    static void Callback(void* gp, int32_t n, void* x2) { Orig(gp, (n >= 4 && n < 8) ? n + 1 : n, x2); }
};
```
G8 va installato **solo se H1 ha creato i 4 controller** (lista = 10); altrimenti ordine 5..8 non esiste e +1
peggiorerebbe le cose.

### 4.4 Buffer inline +0x60 [VERIFICATO]
Nessun accesso diretto a PIM+0x60..+0x7f come giocatori: il tracciatore (`PIM` da 0x8ace0c) trova solo +0x50 (16
siti) e +0x58 (65 siti, tutti `ldr x8,[pim,#0x58]` → elemento); la scansione di 0x7c4dc4-0x7c6a34 e 0xc9f00-0xcb5b4
trova `#0x60]`/`#0x68]`/`#0x70]`/`#0x78]` solo come vtable o slot NpadDevice (0xcaa18, 0xcab74) e frame di stack, e
`+0x80` come byte "disabilitato" (0xca718, 0xca7bc, 0x7c63bc, 0x7c5330). Dopo G1 il buffer inline resta con 4
puntatori obsoleti mai letti. 0x386480 legge solo `[+0x58][0]` (giocatore 0). La PIM non viene distrutta durante la
partita [IPOTESI]: se lo fosse, la FixedPtrArray non libera il buffer (era inline).

### 4.5 8 GamePlayer sempre (anche in 1P): inerte [VERIFICATO statico]
- 0x7c4e50 (per-frame base, chiamato da 0xca050) tocca solo le tabelle per posizione 0..6 e le voci +0x138
  (0x7c5008-0x7c5030): nessun giocatore. 0xca03c tocca solo i controller d'ordine 0..4 e +0x184..+0x187.
- GamePlayer (vtable 0x11b8c50): per-frame vt+0x30 = **0xcb608**, macchina a stati su +0x78c (tabella 0xf23128:
  0→0xcb640, 1→0xcb67c, 2→0xcb6dc, 3→0xcb738, 4→0xcb7dc, 5→fine, 6→0xcb7f0, 7→0xcb854, 8→0xcb8a4). Stato 0:
  diventa 1 solo se `+0x744 != 0` o `+0x774 != 0` (0xcb640-0xcb674). Alla creazione `0x7cdfac` mette +0x744 = 0 e
  chiama vt+0x70 = 0xcbf64 → `0x7ce0e8` (+0x774 = 0, +0x778 = -1, +0x76d = 1) + `0x7ce168` (riaggancio al misc) e
  stato +0x78c = 0; +0x774 viene messo a 1 solo da 0x865a14 (richiesta di riconnessione di un'**entry**, che i giocatori 4..7 non hanno in 1P).
  Coda comune 0xcbc40: `0x7c86c0(p+0x38)` e `0x7cdfcc` → 0x7cdfd0: se `+0x760 == NULL` o `+0x770 == PIM+0xb0`
  (controller misc) va nel ramo "senza input" (0x7cdfe4-0x7cdffc).
- Il riaggancio 0x7ce168 (G7) lega ogni giocatore al controller in posizione `PIM+0xb0` = **misc** (0x7c6550):
  è lo stato in cui si trovano oggi i giocatori 1..3 in 1P. "Pronto" (vt+0x80 = 0xcc3b0) = stato ∉ {0,1}.
- findFreeController 0x7c63a0 (generico su tutta la lista, 0x7c63f4-0x7c6468) viene chiamato dai GamePlayer solo
  negli stati 1/6/8 (0xcb6d8→0xcbb88, 0xcb838, 0xcb95c): i giocatori 4..7 in stato 0 non prendono controller.
- 0x86a594 esteso restituisce sempre il primo indice libero: in 1-4P gli indici 4..7 non vengono mai assegnati.
- Costo: 4 × 0x7b8 B di heap e 4 chiamate in più a 0xcb608 per frame [IPOTESI: i figli della lista +0x38 vengono
  aggiornati dal framework dei task; da confermare con il log del §6].

---

## 5. Legame GamePlayer 4..7 ↔ NpadController npad 4..7 (posizioni 6..9)

### 5.1 Meccanismi [VERIFICATO]
1. **Per indice locale** (deterministico): `0x86a998(rm, a, b)` (unico chiamante 0x86aecc in 0x86adec, solo se
   `rm+0x1fd`): `0x7c58d0` (reset PIM: tutti al misc) poi, per i locali 0..3 srotolati,
   `0x8652f8(entry, i, a, b, 0)` → `0x7ce38c(gp, i, …)` → `getControllerByOrder(0x11, i)` → vt+0xc0 (0xcc0a8 →
   0xcbec4 assegna; NULL → vt+0x70 reset). `rm+0x1fd` è scritto dalla schermata del titolo (0x4d744c in 0x4d71c8,
   "SE_SYS_DECIDE_TITLE", seguito da 0x86aac8) e azzerato da 0x4d6ba0. 0x86adec è chiamata da 0x86b0bc
   (raceMgr vt+0x90) quando il flag atomico `rm+0x204` vale 1; prima imposta gp+0x76d = 1 per i locali 0..3
   (0x8654a0). 0x86ad18 (flag +0x208) lo rimette a 0 (0x865494).
2. **Scambio dal titolo**: 0x86aac8 (da 0x4d7454): `0x7c6484` scambia l'assegnazione npad del locale 0 con quella
   del pad che ha premuto (nn::hid::SwapNpadAssignment, esclusi npad 8), `0x7c5794` (riaggancio), poi per i < 4
   (`cmp x20,#4` a 0x86ac14) `0x86548c → 0x7ce8c4(gp, ordine)` o `0x8652f8(e, i, 1, 0, 0)`.
3. **Riconnessione libera**: 0x865a14(entry, w1) (da 0x86a930, 0x8be35c) mette gp+0x774 = 1, +0x778 =
   indice locale → stato 1 → `0x7c60c0` (G6: precedenza per indice locale) → `0x7c63a0` (primo controller libero in
   tutta la lista con il tasto della maschera) → vt+0xe8 0xcab98 assegna.

### 5.2 Applet take_over / left_justify
Dopo `0x49ea60(N, N, 1, 0)` Eden collega i pad 0..N-1 agli npad 0..N-1 (step 3 §3). I NpadController in posizioni
0..3 e 6..9 diventano connessi; nessun GamePlayer cambia da solo: serve uno dei tre meccanismi sopra.
**Chi alza `rm+0x204` non è stato trovato staticamente** (nessuna scrittura `#0x204` su raceMgr fuori da 0x86b0bc; il
solo scrittore atomico trovato è 0x862008 su `rm+0x20c`, da 0x4da8d8) → [DA VERIFICARE A RUNTIME]. Se dopo
l'applet 0x86a998 non viene chiamata, i giocatori 4..7 si legano solo con il meccanismo 3 (premendo un tasto),
che con G6 e findFreeController generico funziona per 8.

### 5.3 B4 – estensioni delle funzioni srotolate (dopo B1-B3)
```cpp
HOOK_DEFINE_TRAMPOLINE(RebindByIndexHook) {                   /* 0x86a998(rm, a, b) */
    static void Callback(uint8_t* rm, uint32_t a, uint32_t b) {
        Orig(rm, a, b);                                         /* reset PIM (con G7 anche 4..7) + locali 0..3 */
        for (int l = 4; l < 8; l++) {
            int32_t id = ((int32_t*)(rm + 0x17c))[l];
            if (id < 0 || (uint32_t)id >= *(uint32_t*)(rm + 0x50)) continue;   /* l'originale passerebbe NULL */
            uint8_t* e = ((uint8_t**)*(uintptr_t*)(rm + 0x58))[id];
            if (e && *(void**)(e + 0x200)) mk8dx::Fn<void (*)(void*, int, uint32_t, uint32_t, uint32_t)>(0x8652f8)(e, l, a & 1, b & 1, 0);
        }
    }
};
/* 0x86adec(rm, a, b) PRIMA di Orig e 0x86ad18(rm) (prima o dopo): gp+0x76d = 1 / 0 per i locali 4..7 */
    for (int l = 4; l < 8; l++) if (uint8_t* e = Entry(rm, l)) mk8dx::Fn<void (*)(void*)>(0x8654a0 /* 0x865494 */)(e);
```
| # | offset | originale | nuovo | effetto |
|---|---|---|---|---|
| B4a | 0x86ac14 | `f100129f` cmp x20,#4 | `f100229f` cmp x20,#8 | 0x86aac8: rilega anche i locali 4..7 (array +0x17c letto via x21 = rm+0x17c, ok con B1) |
| B4b | 0x86acdc | `f100429f` cmp x20,#0x10 | `f100829f` cmp x20,#0x20 | 0x86ac30 (da 0x4ad500): considera i locali 4..7; l'array su stack è protetto da `cmp w22,#1` (0x86acb4). Facoltativa |
| B4c | 0x86a48c | `b9817c08` (prima parola) | trampolino | se `Orig == -1` cercare `gp+0x73c == 0xb` tra i locali 4..7 e ritornare l'indice. Facoltativa (0xb = tipo controller [IPOTESI]) |

### 5.4 Ordine perché dopo l'applet (N=8) gli 8 locali abbiano GamePlayer e controller
1. Avvio: H1 (controller 4..7) → PIM init → **0xca3e0 + G1/G2/G3** (8 giocatori, figli 9) → tabelle (+0x130 su 10).
2. Titolo: 0x4d71c8 → `rm+0x1fd`, 0x86aac8 (B4a, G8 su 0x7ce8c4).
3. 8g: `0x869180(rm,8,1,0)` → 0x868f00 (B1k) → `0x8650e4` sulle entry k..7 → **0x86a594 (B3)** → gp 0..7 → +0x178 = 8,
   +0x17c = {0..7}, slot a +0x19c (B1).
4. Selezione personaggi: 0x4ba77c → 0x8be248 (H4 per il pad UI 4) → applet (8,8).
5. Cambio controller → (rm+0x204) → 0x86b0bc → 0x86adec (+0x76d) → 0x86a998 (B4) → 0x7c58d0 (G7) → 0x8652f8/0x7ce38c
   con G8 → controller npad l → stato ≥ 2 → "pronto" per il pad UI che lo legge (pad UI 4..7 = sotto-obiettivo A).
   In alternativa meccanismo 3 (tasto premuto).

---

## 6. Sintesi delle modifiche B (in ordine) e verifica

### 6.1 Lista
| fase | tipo | offset | originale → nuovo / firma | dipende da |
|---|---|---|---|---|
| B1a-e | patch | 0x868e30, 0x868ee4, 0x869684, 0x8699ec, 0x86b940 | `b9018d8b` → `b9019d8b` | – |
| B1f | patch | 0x86b0b4 | `b9018d01` → `b9019d01` | – |
| B1g | patch | 0x88236c, 0x882408 | `b9818d1a` → `b9819d1a`, `b9818d15` → `b9819d15` | – |
| B1h | REPLACE | 0x86af68 | `void(rm, int id, int slot)`, prima parola `b9418c08` | B1a-g (stessa build) |
| B1i | patch | 0x86ec08 | `71000e7f` → `71001e7f` | B1 |
| B1j | trampolino | 0x8689b8 (ctor, prima parola `f81e0ff3`) | dopo Orig `memset(rm+0x19c,0xff,0x30)` | B1 |
| B1k | REPLACE | 0x868f00 | `int(rm)`, prima parola `f81d0ff5` | B1 |
| B1l | patch | 0x86901c, 0x86b970 | `71000d1f` → `71001d1f` | B1 |
| B2 | patch | 0xca3fc, 0xca410 | `52800500` → `52800900`, `528000a8` → `52800128` | – |
| B2 | trampolino | 0xca3e0 | `void(pim)` G1+G3 | B2 patch, H1 |
| B2 | trampolini | 0x7c5340, 0x7c6218, 0x7c62bc, 0x7c5044, 0x7c60c0 | G4/G5/G6 (§4.3) | G3 |
| B2 | estensione hook | 0x7c5794, 0x7c58d0 | G7 dopo Orig | G3 |
| B2 | trampolini | 0x7ce38c, 0x7ce8c4 | G8 remap 4..7 → +1 | H1 (lista 10) |
| B3 | REPLACE | 0x86a594 | `void*(rm, uint32 entryId)` | **B1 e B2** |
| B4 | trampolini | 0x86a998 (dopo), 0x86adec (prima), 0x86ad18 | §5.3 | B1-B3 |
| B4 | patch | 0x86ac14 (`f100129f`→`f100229f`), 0x86acdc (`f100429f`→`f100829f`, facolt.) | | B1-B3 |

Tutte le patch con `input::Patch` (verifica della parola originale). Le REPLACE/trampolini vanno installati solo se
le patch del rispettivo gruppo sono riuscite (come per H1/H3). **B3 senza B1 = sforamento in +0x18c (e con B1h
assente, anche corruzione di +0x1cc..+0x1d8)**.

### 6.2 Rischi di regressione 1-4P e verifica
| modifica | rischio | come verificare |
|---|---|---|
| B1 (mappa slot +0x19c) | attivo in **ogni** partita: un lettore dimenticato di +0x18c leggerebbe locali 4..7 (= -1 in 1-4P) invece degli slot | log dopo ogni riempimento (hook 0x869180): `+0x178`, `+0x17c[0..7]`, `S[0..11]`; atteso in 1P/4P: S = {0..N-1, -1…}; gara 1P/4P completa; schermata risultati (0x8821c0 legge S) e "gara successiva" (0x869760) |
| B1h | firma diversa dall'originale se w2 fosse fuori 0..11 | log "slot fuori 0..11" (non deve comparire) |
| B1k | rientro nel menu: compattazione identica per ≤ 4 | 4P → indietro → 2P: `+0x17c` = {0,1,-1…} |
| B2 G1/G3 | 8 giocatori sempre; lista figli 9 | log InitPlayersHook ("giocatori 8/8, figli 9/9"); gara 1P: nessun giocatore 4..7 con stato ≠ 0 (log periodico) |
| B2 G4-G7 | operazioni ripetute su giocatori legati al misc | inerte per analogia con i giocatori 1..3 in 1P; regressione titolo→1P, pausa, disconnessione di un pad durante la gara 2P (riconnessione = G6) |
| B2 G8 | rimappaggio solo per 4..7 | 2P/4P: log "bind locale i → npad" invariato per i = 0..3 |
| B3 | ≤ 4 locali: stesso risultato | log in AssignGamePlayerHook (entryId → k) in 1P/2P/4P |
| B4 | rilega 4..7 solo se presenti | 4P: nessuna chiamata extra (locali 4..7 = -1) |

### 6.3 [DA VERIFICARE A RUNTIME] e log proposti
1. **+0x178 = 8**: estendere il log di MenuEventHook a `+0x17c[0..7]` e `S = rm+0x19c[0..11]` (atteso per 8g:
   8 / {0..7} / {0..7,-1,-1,-1,-1}).
2. **Aggiornamento per frame dei giocatori 4..7** (figli della lista +0x38 [IPOTESI]): contatore per giocatore in un
   trampolino su 0xcb608 (vt+0x30 GamePlayer), stampa ogni 600 frame `idx=+0x768 stato=+0x78c ctrl=+0x760
   npad=ctrl+0x178 pos=+0x770`.
3. **Chi chiama 0x86a998 dopo l'applet**: log in 0x86b0bc (valori di +0x204/+0x208 prima di Orig, rm+0x1fd) e in
   RebindByIndexHook; log in BindByLocalHook (idx in, ordine usato, controller restituito → npad).
4. **"Pronto" dei locali 4..7**: con A non ancora fatto, H4 guarda solo la connessione; in alternativa a H4 si può
   usare `gp = entry(local k)+0x200 && vt+0x80(gp)` (0xcc3b0) per k = 4..N-1 una volta attivi B1-B4.
5. **Ordine di inizializzazione** (già visto allo step 3) da riconfermare con il log di InitPlayersHook dopo
   "lista controller 10/12".
6. Effetti collaterali noti fuori da B (step 6): 0x551b28 (pagina di gara, array per locale u64[4] con clamp →
   locali 4..7 scrivono l'indice 0), lettori srotolati del §1.3 (vedono solo 4 locali), +0x214/+0x224 a 4 (§2).
