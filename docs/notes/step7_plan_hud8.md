# Step 7 – HUD 8P per colonna (problema A) e aiuti alla guida dei giocatori 5-8 (problema B)

Data: 3 nov 2026. Analisi statica di `../img/main.bin` (4.0.1, offset = indirizzo) e della RomFS (`../romfs-tmp/v4/race.sarc`).
Estratti in `step7_data/u_*`. main.cpp NON modificato: qui solo piano e schizzi.

---

## A. Lato dell'HUD per cella (sinistra/destra)

### A.1 Chi decide il lato [VERIFICATO statico]
- onCreate della pagina RaceView **0x547d20**: `w20 = id - 0x65` (0x547d5c, dove RaceViewSlotHook porta gli id 0x12/0x1d/0x43/0x5b a
  0x65+4..7), crea il layout (nome da vt13 = RaceViewNameHook) e a **0x547ec0/0x547ec4** chiama `0x50efa8(lyt, w1 = slot)`.
  `0x50efa8` ha **un solo chiamante** (0x547ec4; nessun altro `bl/b`).
- onCreate del layout **0x50ee80**: lega l'animazione 0 = `0x519db8(0)` = stringa statica **"PlayerPos"** (0xee9c14) a lyt+0x70[0].
- **0x50efa8(lyt, slot)** imposta il frame di PlayerPos con `0x8ad3c4(lyt+0x70[0], frame, 0)`:
  - lyt+0xb4 (= modo Scene+0xcc == 0, 1P): frame = 0x86f7bc()+0x2a ? 1 : 0;
  - altrimenti `frame = (slot & 1) + 2`; solo se 0x86f7bc()+0x8 == 3 e +0xc == 3: `frame = slot & 1`.
  Il lato dipende quindi **solo dalla parità dello slot**. Nessun'altra lettura del viewport/slot per il lato.
  Estratto: `step7_data/u_raceview_playerpos.asm`.

### A.2 Cosa offre rc_RaceView_8P_Ml [VERIFICATO dati] — `step7_data/u_raceview_layouts.txt`
- Layout 640x720 (cella 320x360 con camera x2), FLAN unica **PlayerPos** sul gruppo `G_PlayerPos_00`, 4 frame (0..3), stessa
  convenzione del 4P_Ml/2P_Ml/6P_Ml: **frame pari = sinistra, dispari = destra**:

| pannello (8P_Ml) | frame 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| N_ItemBoxPos_00 X (Y) | -213 | +213 (273) | -190 (260) | +190 |
| L_Rank_00 X | -224 | +214 | -224 | +214 |
| L_LapCoin_00 X | -182 | +182 | -182 | +182 |
| L_BtCoinCount / BtKeidoroCoin / BtScore | sx | dx | sx | dx |

  Nessun gruppo ViewKind/LR/colonna aggiuntivo: il layout 8P ha solo i due lati, la scelta per colonna va fatta dal codice.
- Con le 8 celle: slot 0..3 = celle 1-4 (colonne 0..3), slot 4..7 = celle 5-8. Oggi: slot 1 (col. 1) e slot 5 → frame 3 (destra,
  verso il centro); slot 2 e 6 (col. 2) → frame 2 (sinistra, verso il centro). È esattamente il difetto osservato.

### A.3 Hook minimo
Trampolino su **0x50efa8** (parola originale `fc1e0fe8` = `str d8,[sp,#-0x20]!`, nessun riferimento PC-relativo: rilocabile).
Basta cambiare la parità: colonna `c = s % 4`, sinistra se `c < 2` → parità 0, destra → parità 1. Il ramo 1P (lyt+0xb4) e il caso
`+0x8 == 3 && +0xc == 3` (frame 0/1 invece di 2/3) restano del gioco, perché si cambia solo `w1`.

```cpp
HOOK_DEFINE_TRAMPOLINE(RaceViewSideHook) {               /* 0x50efa8(lyt, slot): frame PlayerPos = (slot&1)+2 */
    static void Callback(uint8_t* lyt, int32_t slot) {
        if (view8::split == 8 && slot >= 0 && slot < 8)
            slot = ((slot & 3) < 2) ? 0 : 1;              /* colonne 1-2 a sinistra, 3-4 a destra (riferimento Switch 2) */
        Orig(lyt, slot);
    }
};
/* installazione, con verifica della parola: */
if (*reinterpret_cast<uint32_t*>(base + 0x50efa8) == 0xfc1e0fe8) RaceViewSideHook::InstallAtOffset(0x50efa8);
```
Alternativa equivalente [VERIFICATO statico]: INLINE su **0x547ec0** (`2a1403e1` `mov w1,w20`) che riscrive `W[20]` prima
dell'istruzione (w20 non è più letto dopo: 0x547edc lo sovrascrive). Il trampolino è preferibile (un solo chiamante, nessuna
dipendenza dai registri).
- Il frame è impostato una sola volta all'onCreate [VERIFICATO: unico chiamante]; se in futuro le pagine 4..7 venissero ricreate,
  il hook vale comunque.
- 6P (`split == 6`, griglia 3x2) [IPOTESI]: 6P_Ml ha la stessa PlayerPos a 2 lati; regola proposta `c = s % 3`: c==0 sinistra,
  c==2 destra, c==1 a scelta (es. sinistra). Non c'è riferimento Switch 2 per il 6P.
- Verifica in Eden: celle 2,3,6,7 con riquadro oggetti verso l'esterno; log `0x50efa8 slot s -> parità`.

---

## B. Aiuti alla guida dei giocatori 5-8: dove vanno, perché P6-P8 si perdono, scrittura fuori limiti

### B.1 Il toggle in pausa scrive solo nel GamePlayer [VERIFICATO statico] — `step7_data/u_aids_toggle_51cd34.asm`
L_SwitchBtn (ctor 0x51c088: +0xc8 giocatore, +0xcc tipo). Toggle **0x51cd34**: `gp = 0x8be068(0x8bf158(+0xc8))` (con le patch C1
0x8be078 e B1i 0x86ec08 i giocatori 4..7 ottengono il proprio GamePlayer), poi per tipo (tabella 0xf2a6e4):

| tipo | tasto (ctor 0x51c274, tab. 0xf2a670) | gruppo icona (w24) | scrive | messaggio |
|---|---|---|---|---|
| 0 | L (0x4c, maschera 0x800) | 3 = SwitchHandleAssist | `gp+0x20c` (s32) | 0x2432/33 |
| 1 | Y (0x59, 0x400) | 0 | `gp+0x208` (u8) e `kart(gp+0x200)+0x394` via 0x7ce99c | 0x2430/31 |
| 2 | R (0x52, 0x1000) | 4 = SwitchAutoAccelerator | `gp+0x209` (u8) | 0x2434/35 |
| 3 | X (0x58, 0x200) | 0 | `cfg+0x230c` (u8, **globale**) | 0x242a/2b |

Nomi dei gruppi da step7_plan.md §1.1 (0x51bb04). Quindi [IPOTESI forte]: L = guida assistita (gp+0x20c, default 1),
R = **accelerazione automatica** (gp+0x209), Y = terzo aiuto (probabilmente comandi di movimento: tocca anche il kart).
Il toggle **non** scrive nella config né nei 13 array cfg+0x1fb4..+0x2074 (quelli sono personaggio/parti, step 5): nessun
indice viene usato per scrivere nel toggle, quindi in pausa nessuna scrittura fuori limiti. Lo stato visivo (0x51c440, 0x51ca6c)
legge solo il GamePlayer (e 0x425888(i), che restituisce 0 per i > 3).

### B.2 Persistenza: 0x3fe040 (salva) e 0x3fdd70 (carica) [VERIFICATO statico]
Entrambe ricevono `x19 = cfg + 0x1fa4` (siti 0x4c3c14, 0x535400, 0x5324f4, 0x4b72d4: `mov w9,#0x1fa4; add x0,x8,x9`) e ciclano
`i < [cfg+0x1fa4]` = **numero di locali (8 con la mod)**, con `gp = 0x8be068(0x8bf158(i))`.
- Codifica: 0 = mai impostato (default), 1 = acceso, 2 = spento (`cmp #0; cinc w, 1, eq`, 0x3fe0f4..0x3fe110).
- i == 0: bitfield nella parola di salvataggio dell'account (save+0x13618: bit 26-27 Y, 28-29 L, 30-31 R) → persistente.
- i > 0 con account (`gp+0x29c` != 0): parola dell'account di quell'utente.
- i > 0 **ospite** (`gp+0x29c` == 0): tre array `int[4]` contigui (NON fra i 13 dello step 5):

| array | indirizzo | aiuto | salva | carica |
|---|---|---|---|---|
| A | cfg+0x2310 (= x19+0x36c) | Y (gp+0x208, kart+0x394) | 0x3fe16c | 0x3fde54 |
| B | cfg+0x2320 (= x19+0x37c) | L (gp+0x20c) | 0x3fe170 | 0x3fde58 |
| C | cfg+0x2330 (= x19+0x38c) | R (gp+0x209) | 0x3fe174 | 0x3fde5c |

  Indirizzo = `x19 + i*4 + off` (0x3fe168 `add x8,x19,x20,lsl #2`): **nessun clamp**.

### B.3 Scritture fuori limiti con i = 4..7 (ospiti) [VERIFICATO statico; esecuzione a runtime IPOTESI forte]
Ogni chiamata di 0x3fe040 con 8 locali (fine selezione kart 0x4c3c1c, pagina gara 0x535404) scrive:

| i (giocatore) | A[i] finisce in | B[i] finisce in | C[i] finisce in |
|---|---|---|---|
| 4 (P5) | B[0] cfg+0x2320 | C[0] cfg+0x2330 | **cfg+0x2340** (s32 indice) |
| 5 (P6) | B[1] (L di P2) | C[1] (R di P2) | **cfg+0x2344** (u8 flag, u8 flag, 2 B) |
| 6 (P7) | B[2] (L di P3) | C[2] (R di P3) | **cfg+0x2348** (metà bassa di un puntatore) |
| 7 (P8) | B[3] (L di P4) | C[3] (R di P4) | **cfg+0x234c** (metà alta dello stesso puntatore) |

La struttura a **cfg+0x2340** (16 B) [VERIFICATO statico] — `step7_data/u_cfg2340_users.txt`:
- init `0x3fe96c` = {s32 -1, u8 0, u8 1, u64 0} (al ctor cfg 0x402ec8, a 0x53371c nella pagina gara, 0x54a484, 0x54af80);
- `+0x2340` indice in una tabella di record da 0x308 B (0x54b280: solo `cmp #5/#0xb; b.gt`);
- `+0x2344` byte che cambia il flusso di fine gara (0x539b34: 0x538088 vs 0x537fe0; anche 0x51e5c4, 0x53a6fc, 0x54ca04);
- `+0x2348` **puntatore** (scritto a 0x54afa4/0x54bb38), dereferenziato a 0x50cd7c (`ldr x9,[x21,#0x2348]; cbz x9; ldr w10,[x9,#0x60]`),
  0x50d89c, 0x50db3c, 0x50dfcc, 0x520954.

Conseguenze:
1. **Rischio crash [IPOTESI forte]**: P7/P8 ospiti scrivono 1/2 nelle due metà del puntatore → valore non nullo tipo
   0x0000000200000001 → data abort al primo lettore che non passa prima da 0x3fe96c. Raggiungibilità dei lettori in VS a 8 non
   verificata (sembrano legati a fantasmi/record).
2. **Flusso di fine gara alterato [IPOTESI]**: il codice di P6 (1/2) nel byte cfg+0x2344.
3. **Aiuti di P2-P4 sovrascritti [VERIFICATO statico]**: B[1..3] ← Y di P6-P8, C[1..3] ← L di P6-P8. Al caricamento
   l'**accelerazione automatica di P2-P4 è in realtà la guida assistita di P6-P8** (che di default vale 1 = acceso).
4. Caricamento 0x3fdd70 per i = 4..7 (quando ProfileToEntriesHook non limita a 4, cioè con `sel8::slotsReady`): stesse
   letture fuori limiti; R di P5..P8 = cfg+0x2340/0x2344/0x2348/0x234c decodificati come codici (≠ 1 → spento, 0 → default).

### B.4 Perché P5 resta e P6-P8 no [IPOTESI, coerente con B.3]
- R (auto-acc) di P5 si salva in cfg+0x2340 (s32 intero e allineato): se nessuno chiama 0x3fe96c fra salvataggio e caricamento
  il valore 1 sopravvive → P5 acceso.
- R di P6 finisce nei due byte flag (0x2344/0x2345), R di P7/P8 nel puntatore: questi campi vengono riscritti dal gioco
  (0x3fe96c: +4 = 0, +5 = 1 → int 0x100; +8 = 0; 0x54afa4 vi mette un puntatore vero) → letti come ≠ 1 o 0 → spento.
- P2-P4 accesi perché leggono C[1..3] = guida assistita di P6-P8 (vedi B.3.3), non il proprio valore.
- Gli array A/B/C non sono azzerati dall'init 0x3fd32c (che azzera x19+0x2e4..0x35c e 0x360..0x36a); se la cfg sia scritta nel
  file di salvataggio non è verificato: la persistenza fra sessioni di P2-P8 è [IPOTESI].
Conferma consigliata (GDB, una gara a 8): watchpoint su cfg+0x2340 e cfg+0x2348 (`watch *(long*)(cfg+0x2348)`) e dump
`x/16wx cfg+0x2310` prima/dopo 0x3fe040 e dopo 0x3fdd70.

### B.5 Correzione proposta
**Urgente (minima)**: impedire a 0x3fe040 di superare 4, come già fa ProfileToEntriesHook per 0x3fdd70 (ma sempre, non solo con
`!slotsReady`). Parola originale a 0x3fe040: `d10283ff` (`sub sp,sp,#0xa0`), rilocabile.

**Completa**: array ombra nel modulo per i giocatori 4..7, stessa codifica 0/1/2.
```cpp
namespace aid8 { static uint8_t code[3][4]; /* [0]=Y(+0x208) [1]=L(+0x20c) [2]=R(+0x209), giocatori 4..7, solo RAM */ }
static uint8_t* GpOf(int i) { return mk8dx::Fn<uint8_t* (*)(uint8_t*)>(0x8be068)(mk8dx::Fn<uint8_t* (*)(int)>(0x8bf158)(i)); }

HOOK_DEFINE_TRAMPOLINE(AidsSaveHook) {                   /* 0x3fe040(cfg+0x1fa4) */
    static void Callback(int32_t* field) {
        int32_t n = *field;
        if (n <= 4) { Orig(field); return; }
        *field = 4; Orig(field); *field = n;              /* niente scritture a cfg+0x2320..0x234c */
        if (!padui::active8) return;                      /* senza pad extra 0x8bf158(4..7) darebbe il pad 0 */
        for (int i = 4; i < n && i < 8; i++) {
            uint8_t* gp = GpOf(i); if (!gp) continue;
            uint8_t* kart = *(uint8_t**)(gp + 0x200);
            aid8::code[0][i - 4] = (kart && kart[0x394]) ? 1 : 2;   /* come 0x3fe0ec (il gioco non controlla kart nullo) */
            aid8::code[1][i - 4] = *(int32_t*)(gp + 0x20c) ? 1 : 2;
            aid8::code[2][i - 4] = gp[0x209] ? 1 : 2;
        }
    }
};
/* ProfileToEntriesHook (0x3fdd70): limitare SEMPRE a 4, poi applicare l'ombra con la stessa decodifica del gioco */
static void ApplyAids(uint8_t* cfg, int i) {
    uint8_t* gp = GpOf(i); if (!gp) return;
    uint8_t* kart = *(uint8_t**)(gp + 0x200);
    auto ctl3 = [&] { return kart && *(int32_t*)(kart + 0x73c) == 3; };            /* default di Y e R */
    int a = aid8::code[0][i - 4], b = aid8::code[1][i - 4], c = aid8::code[2][i - 4];
    bool y = a ? a == 1 : ctl3();
    gp[0x208] = y; if (kart) mk8dx::Fn<void (*)(uint8_t*, bool)>(0x7ce99c)(kart, y);
    bool off = *(int32_t*)(cfg + 0x1fb0) == 1 || mk8dx::Fn<bool (*)()>(0x4247dc)();     /* 0x3fdf20/0x3fdf34 */
    *(int32_t*)(gp + 0x20c) = off ? 0 : (b ? b == 1 : 1);
    gp[0x209] = (off || mk8dx::Fn<bool (*)()>(0x422548)()) ? 0 : (c ? c == 1 : ctl3());
}
```
- In ProfileToEntriesHook: `*field = min(n,4); Orig(field); *field = n; if (slotsReady) for i in 4..n-1: ApplyAids(cfg, i)`.
  `cfg = (uint8_t*)field - 0x1fa4`.
- Persistenza fra sessioni: l'ombra vive in RAM (dura finché il gioco resta aperto). Per sopravvivere al riavvio servirebbe un
  file su SD dal modulo (nn::fs) [non necessario per ora].
- Rischi residui: nessuna scrittura del gioco con indice ≥ 4 resta nei tre array; il toggle in pausa resta invariato.

### B.6 Pulizia del salvataggio già toccato [IPOTESI]
Dopo l'installazione: i valori R/L di P2-P4 (C[1..3]/B[1..3]) possono contenere i codici di P6-P8: basta riattivare/disattivare
gli aiuti di P2-P4 una volta. La struttura cfg+0x2340 viene reinizializzata dal gioco (0x3fe96c al ctor cfg e a 0x53371c).

---

## Riepilogo offset

| cosa | offset | parola originale | azione |
|---|---|---|---|
| frame PlayerPos per slot | 0x50efa8 | fc1e0fe8 | TRAMPOLINE, slot → parità per colonna (split 8) |
| (alternativa) argomento slot | 0x547ec0 | 2a1403e1 | INLINE, W[20] |
| salvataggio aiuti | 0x3fe040 | d10283ff | TRAMPOLINE, limite 4 + ombra 4..7 (**urgente**) |
| caricamento aiuti | 0x3fdd70 | (hook esistente) | limite 4 sempre + ApplyAids 4..7 |
