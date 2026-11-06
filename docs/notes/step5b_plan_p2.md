# Step 5b – passo 2: pagine CharaList per gli slot 4..7, link +0x660 e code +0x928 a 8 (piano statico)

Data: 3 nov 2026. Base: `../img/main.bin` (offset = indirizzo), `../img/main.ann.asm`. `src/` non modificato, nessuna build.
Dati: `step5b_data (dati non pubblicati)/p2_verify.py` (genera e verifica tutte le parole, disassemblando orig/nuova con llvm-objdump),
`p2_table.txt` (output), `p2_sites.txt` (siti, LR, registrazioni). Dato runtime del coordinatore: dump di `0x8bcc5c()`
all'ingresso nella selezione, prima e dopo una gara a 8.
Etichette: [VERIFICATO] = letto in main.bin/main.ann.asm (indirizzo) o nel dump runtime; [IPOTESI] = da confermare in Eden.

## 0. Sintesi
- **Le "CharaWin" 0x28..0x2b sono le pagine `CharaList0..3`** (img/pages.txt), cioè la finestra per giocatore con la lista
  delle varianti del personaggio scelto (colori di Yoshi/Tipo Timido/Birdo/Inkling, varianti di Link, ecc.). Si aprono
  quando un giocatore conferma un personaggio che ha varianti: vt92 0x4bbd18 mette lo slot nello stato 5 (0x4bc028), calcola il tipo
  di finestra dal personaggio (tabella di salto 0x4bc0a4) e, se il tipo non è 0, scrive `CharaList(slot)+0x2a0 = tipo`,
  `+0x2d1 = 0` (0x4bc624/0x4bc628) e segue il link `this+0x660+slot*0x28` (0x4bc640 → 0x8ba5fc) [VERIFICATO].
- **Senza la pagina lo slot ≥ 4 non può confermare**: dopo la conferma (stato ≥ 5) vt86 legge `CharaList(0x28+i)+0x120`
  (0x4ba874 → 0x4ba888 → **0x4ba8d8 `ldr w8,[x8,#0x120]` con x8 = NULL** se il cast fallisce). Lo stesso schema (cast → NULL →
  deref senza controllo) c'è in **17 dei 18 siti** [VERIFICATO, `p2_sites.txt`]. L'alternativa (2), cioè niente pagine e rami
  null-safe, richiede 17 protezioni e toglie la scelta della variante ai giocatori 5-8. **Decisione: alternativa (1)**, con 4 pagine
  CharaList reali.
- I siti che calcolano `0x28+i` sono **18, non 17**: 0x4baea0 (vt87) riusa `w27 = 0x28+i` per una seconda chiamata a 0x8bf0ec
  [VERIFICATO]. Gli LR sono elencati in §2.2.
- Id: **0x01, 0x02, 0x5e, 0x64**. Sono liberi in tutti i casi (dump runtime prima e dopo una gara), nessuna registrazione statica
  e nessuna apertura o get con costante (scanner su 0x8bf0ec/0x8b21dc/0x8b258c/0x8b21cc/0x8b2314/0x8b3cb0) [VERIFICATO].
  **0x87 è escluso**: il numero di pagine è 0x87 (0x8bcc78), quindi 0x87 cade sulla voce 0 di PageMgr (0x8bd0c8/0x8bd0e8)
  [VERIFICATO il codice; IPOTESI forte che 0x725f30 usi proprio questa tabella].
- Rilocazione in coda: oggetto **0xa68 → 0xce8**. Layout: `+0xa68 link[8]` (8×0x28), `+0xba8 code[8]` (8×0x28).
  **39 parole** (5 link + 34 code) + 3 fabbriche (orig = parola del passo 1). Tutti gli immediati sono codificabili:
  `add` imm12 ≤ 0xbb8, `ldr/str w` 0xbb8/4 = 0x2ee, `ldr x` 0xba8/8 = 0x175. Non serve nessun hook inline per gli accessi
  [VERIFICATO, `p2_table.txt`].
- Hook: trampolino 0x40476c (registrazione delle 4 pagine), trampolino 0x8bf0ec (rimappaggio per LR), inline 0x4b28d8 (slot
  +0x1e8 delle pagine nuove), inline 0x4b7a98 (id dei link 4..7), estensione di `CharaSelBaseCtorHook` 0x4bc844 (costruzione dei
  link e delle code in coda).
- **Clamp (7): nessuno nel passo 2, tutti nel passo 4.** Con +0x8c0 = 4 e i clamp a 4 il passo 2 è inerte nel comportamento,
  sia in 1-4P sia in 5-8P a due turni: le pagine 4..7 esistono ma nessun sito produce 0x2c..0x2f.

## 1. Le pagine CharaList (0x28..0x2b): cosa sono e cosa dipende dall'indice

### 1.1 Struttura [VERIFICATO]
| cosa | dove | note |
|---|---|---|
| registrazione | 0x40476c (vtable della scena menu, 0x1265f10), 0x404d50/0x404da0/0x404de8/0x404e30 | fabbrica = GOT 0x1307b38 (0x40918c), proprietario = scena (x19), `0x8b3cb0(scena, &id, w20)` con **w20 = 0xa** (unica scrittura di w20 nella funzione: 0x404798) |
| fabbrica | 0x40918c `mov w0,#0x310; bl 0x5fa960; bl 0x4b27c4` | non dipende dall'id |
| ctor | 0x4b27c4: base pagina 0x8ba39c, vtable GOT 0x1309328 → 0x1278048, `+0x1e8 = 0`, due stringhe (+0x1f0, +0x248), link di ritorno +0x2a8 (0x8b18cc), +0x2a0 = 0, scanner +0x2e0 | non dipende dall'id |
| onCreate (vt75) | 0x4b28a4: **`+0x1e8 = [+0xe8] − 0x28` (0x4b28c0..0x4b28d8)**, layout parte 0x398 B (0x5902d0, nome vuoto 0xed6b28 → +0x1d0), pulsante `L_ChangeBtn_00` 0x190 B (+0x1e0), **`0x43ce4c(btn, slot)`**, +0x300 = 4, +0x118 = 7 | l'indice viene dall'id |
| onEnter (vt77) | 0x4b29e0: +0x1d8 = pagina sorgente (0x8ba570 + cast), **maschera input +0x1a8 con il solo bit dello slot, if srotolato 0..3** (0x4b2ab8-0x4b2b6c), fuoco +0x190 \|= 1<<slot | slot ≥ 4: nessun bit di input |
| altre vt | 82 0x4b5018, 83 0x4b5610, 85 0x4b2b70, 86 0x4b3014, 88 0x4b533c, 92 0x4b31fc, 93 0x4b3160 | usano +0x1e8 in 73 punti (0x4b2998..0x4b5628) |

Usi di `+0x1e8` (slot) dentro la pagina [VERIFICATO]:
- `0x43ce4c(btn, slot)` (0x4b29a4) mette il frame del colore del pulsante = slot+1 (0x43ce84..0x43ce90) e i bit giocatore
  +0x88 solo per 0..3. Con slot 4..7 il frame vale 5..8, mentre i colori P5..P8 sono i frame 7..10 (step 7). È **cosmetico**
  [IPOTESI sul risultato visivo].
- Pad UI `0x8bf158(slot)`, config per giocatore `cfg+0x1fb4..+0x2074[clamp4(slot)]` (0x4b394c..0x4b3ffc): passi 3 e 6.
- Pagina madre: `0x4b4f08(madre, slot, valore)` (coda, 0x4b4018), `0x4b43b4(madre, slot, …)` (13 chiamate), 0x4b53c8 (coda).
- Unico uso di `[+0xe8]` oltre a onCreate: 0x4b23f8 → `0x8b258c(&id)` (la pagina usa il proprio id: con un id nuovo è corretto).

**Due slot su una pagina sola**: lo stato (+0x2a0 tipo, +0x2d0/+0x2d1, +0x120 stato pagina), la maschera di input, la madre +0x1d8
e le stringhe sono per pagina. Due giocatori che condividessero una CharaList si sovrascriverebbero tipo e varianti, e la seconda
apertura (`0x8ba5fc` su una pagina già aperta) non aprirebbe una seconda finestra [VERIFICATO lo stato per pagina; IPOTESI
sull'effetto della doppia apertura]. Quindi serve una pagina per slot.

### 1.2 Quando si apre e cosa succede per gli slot ≥ 4 [VERIFICATO]
- vt92 0x4bbd18 (evento sull'icona; slot = ev+0x2c dal selettore, quindi oggi < 4): stato 5 (0x4bc028); tipo di finestra da
  0x4bc0a4 (personaggio 4..0x34); tipo 0 = nessuna finestra (0x4bc594 `cbz`); altrimenti 0x4bc59c → CharaList(slot), +0x2a0 = tipo,
  +0x2d1 = 0, link +0x660+slot*0x28 → `0x8ba5fc`. Il link scrive la sorgente in link+8 e chiama `link->vt[2]` (0x8ba5fc..0x8ba610).
- vt86 0x4ba77c, per ogni i < +0x8c0 con stato ≥ 5: legge `CharaList(i)+0x120` (0x4ba8d8). Se vale ≤ 2 o ≥ 8 (finestra chiusa)
  conta w26. **Con i ≥ 4 e senza pagina la lettura avviene a NULL+0x120**, anche per un personaggio senza varianti: la lettura
  dipende solo dallo stato ≥ 5, non dal tipo.
- vt81 0x4bab18 / vt87 0x4badb0: chiudono o attendono la chiusura delle CharaList aperte (0x4babb4, 0x4bae24, 0x4baea0, 0x4bb1f4).
- vt94 0x4b7218 (i < N, cfg+0x1fa4): 0x4b73a0 → `0x3fd5a4(cfg, CharaList(i)+0x1d0, i)`. Oggi `CharselProfileHook` forza N = 4 e
  `ProfileToCharaWinHook` salta i ≥ 4.

### 1.3 Alternativa (2), senza pagine: perché no [VERIFICATO]
In 17 dei 18 siti, dopo il cast (`csel xR, xP, xzr, ne`), il codice dereferenzia il risultato senza controllo (tabella in
`p2_sites.txt`: `+0x120`, `+0x1d0`, `+0x250`, `str +0x2a0`). Per evitare le pagine servirebbe: forzare il tipo 0 per gli slot ≥ 4,
**più** 17 protezioni (hook inline o trampolini sulle funzioni vt86/vt81/vt87/vt92/vt93/vt77/0x4b43b4), con valori di ritorno
inventati (+0x120 "chiusa"). È più codice dell'alternativa (1), più fragile, e i giocatori 5-8 perderebbero la scelta del colore.
**Con la pagina Multi a 8 (+0x8c0 = 8 nel passo 4) serve l'alternativa (1).**

## 2. Alternativa (1): 4 pagine CharaList su id liberi + rimappaggio per LR

### 2.1 Id liberi [VERIFICATO]
- Dump runtime (coordinatore) all'ingresso nella selezione. Prima di ogni gara, voci senza fabbrica o proprietario: 0 1 2 12 1d 43
  5b 5e 64 65..6d 71 73 78..83 87. Dopo una gara a 8: 0 1 2 5e 64 71 73 78..83 87. Le voci di gara restano popolate (anche
  0x12/0x1d/0x43/0x5b, scritte da `RacePagesSetupHook` della mod).
- Registrazioni con id non costante nel main (119 chiamate a 0x8bcc5c, 8 senza `mov w8,#id`): 0x4040c4 (w20 = 0x0a), 0x404164
  (w21 = 0x0c, 0x403efc), 0x406d28 (0x65+i, i ≤ 3, 0x406ce4), 0x406d88 (0x6a+i); 0x8bcec8 legge solo il puntatore. Quindi
  **0x01, 0x02, 0x5e, 0x64 non vengono mai scritti** dal retail.
- Nessun get/apertura/link/registrazione con queste costanti: lo scanner su 0x8bf0ec (297 chiamate), 0x8b21dc (53), 0x8b258c (25),
  0x8b21cc (21), 0x8b2314 (39), 0x8b3cb0 (117) risolve tutti gli id costanti, e nessuno è 0x01/0x02/0x5e/0x64/0x87. I non risolti
  sono id calcolati (`0x28+i`, `[page+0xe8]`). Nessun confronto del framework sull'id della pagina con valori piccoli (solo
  #0x88, #0x7, #0x7f, #0x48).
- 0x71/0x73/0x78..0x83 sono liberi nel menu ma sono id di gara con registrazione statica (es. 0x406df0 `mov w8,#0x71`): scartati.
- **0x87 escluso**: `0x8bcc78` (vt+0x20 della tabella, vtable 0x12d4550, ctor 0x8bcb78) restituisce 0x87. 0x725f30 lo usa come
  numero di pagine (PageMgr+0x28, array +0x30), e `0x8bd0c8`/`0x8bd0e8` con id ≥ numero leggono e scrivono la voce **0**.
- `0x8b3cb0` aggiorna il min/max degli id del proprietario (+0x20/+0x24, 0x8b3ce8..0x8b3d1c). I cicli di gestione e di distruzione
  0x8b3a30/0x8b3b6c percorrono [min,max] e toccano solo le pagine con `page+0xf0 == proprietario`. Con 0x01 e 0x64 l'intervallo
  della scena menu si allarga, e le pagine nuove vengono gestite e distrutte con la scena, come le altre.

Scelta: `cw8::kIds = { 0x01, 0x02, 0x5e, 0x64 }` → slot 4, 5, 6, 7.

### 2.2 I 18 siti e i loro LR (indirizzo del bl + 4) [VERIFICATO]
| sito `0x28+i` | bl 0x8bf0ec | **LR** | funzione / limite di i |
|---|---|---|---|
| 0x4b4a30 | 0x4b4a3c | **0x4b4a40** | 0x4b43b4 (slot dalla CharaList o dalla classe) |
| 0x4b7028 | 0x4b7034 | **0x4b7038** | 0x4b6970, i < 4 fisso |
| 0x4b710c | 0x4b7118 | **0x4b711c** | 0x4b6970, i < 4 fisso |
| 0x4b7170 | 0x4b717c | **0x4b7180** | 0x4b6970, i < 4 fisso |
| 0x4b7394 | 0x4b73a0 | **0x4b73a4** | vt94, i < N (oggi N := 4) |
| 0x4b77c8 | 0x4b77d4 | **0x4b77d8** | vt77, i ≤ 1 |
| 0x4b7d6c | 0x4b7d78 | **0x4b7d7c** | vt77, i < +0x8c0 |
| 0x4b9efc | 0x4b9f08 | **0x4b9f0c** | vt93 |
| 0x4ba490 | 0x4ba49c | **0x4ba4a0** | vt93, i < +0x8c0 |
| 0x4ba630 | 0x4ba63c | **0x4ba640** | vt93, i < +0x8c0 |
| 0x4ba87c | 0x4ba888 | **0x4ba88c** | vt86, i < +0x8c0 |
| 0x4baba8 | 0x4babb4 | **0x4babb8** | vt81, i < +0x8c0 |
| 0x4bae18 | 0x4bae24 | **0x4bae28** | vt87, i < +0x8c0 |
| (w27 riusato) | 0x4baea0 | **0x4baea4** | vt87, stesso i |
| 0x4bb1e8 | 0x4bb1f4 | **0x4bb1f8** | vt87 stadio 2 |
| 0x4bb964 | 0x4bb970 | **0x4bb974** | i ≤ 1 |
| 0x4bc32c | 0x4bc338 | **0x4bc33c** | vt92, slot dell'evento |
| 0x4bc59c | 0x4bc5a8 | **0x4bc5ac** | vt92, slot dell'evento |

Le altre chiamate nell'intervallo 0x4b1298-0x4bc844 usano costanti 0x26 o 0x28..0x2b. Le costanti 0x2c..0x2e compaiono solo a
0x4b0fbc/0x4b10ac (regali) e 0x4c480c/0x4c4ae0/0x4c4db4 (kart): nessuna fra gli LR. Nel rimappaggio per LR, quindi, non c'è
nessun falso positivo possibile.

### 2.3 Rimappaggio: trampolino su 0x8bf0ec (prima parola `f81e0ff3`, `str x19,[sp,#-0x20]!`)
```cpp
namespace cw8 {                                       /* step5b passo 2: CharaList (0x28..0x2b) per gli slot 4..7 */
    static constexpr int kIds[4] = { 0x01, 0x02, 0x5e, 0x64 };       /* liberi prima e dopo una gara (dump 0x8bcc5c) */
    static constexpr uintptr_t kLR[18] = {                           /* bl 0x8bf0ec + 4 nei siti id = 0x28+i */
        0x4b4a40, 0x4b7038, 0x4b711c, 0x4b7180, 0x4b73a4, 0x4b77d8, 0x4b7d7c, 0x4b9f0c, 0x4ba4a0,
        0x4ba640, 0x4ba88c, 0x4babb8, 0x4bae28, 0x4baea4, 0x4bb1f8, 0x4bb974, 0x4bc33c, 0x4bc5ac };
    static bool ready;                                               /* 4 pagine registrate nella scena menu corrente */
    static bool active;                                              /* passo 2 installato (link/code rilocati) */
    static int SlotOfId(int id) { for (int j = 0; j < 4; j++) if (kIds[j] == id) return 4 + j; return -1; }
    static int hits;                                                 /* rimappaggi effettuati (0 nel passo 2) */
}
HOOK_DEFINE_TRAMPOLINE(PageGetRemapHook) {                           /* 0x8bf0ec(int* id) -> pagina */
    static uint8_t* Callback(int32_t* pid) {
        int v = *pid;
        if (v >= 0x2c && v <= 0x2f && cw8::ready) {
            uintptr_t lr = (uintptr_t)__builtin_return_address(0) - mk8dx::Base();
            for (uintptr_t x : cw8::kLR) if (x == lr) {
                int32_t nid = cw8::kIds[v - 0x2c];                   /* non si tocca *pid: alcuni siti lo riusano */
                uint8_t* p = Orig(&nid);
                if (p && *(int32_t*)(p + 0xe8) == nid) {
                    if (cw8::hits++ < 16) Logging.Log("[mk8dx-8p] cw8: 0x%x -> 0x%x (%p) da main+%lx", v, nid, p, lr);
                    return p;
                }
                break;                                               /* pagina assente: comportamento originale */
            }
        }
        return Orig(pid);
    }
};
```
0x8bf0ec viene chiamata spesso: il percorso veloce è un solo confronto di intervallo. `__builtin_return_address(0)` nei
trampolini è già usato nella mod (PageOpenHook 0x8b21dc, GetPadHook) [VERIFICATO main.cpp:468/1495].

### 2.4 Registrazione: trampolino su 0x40476c (prima parola `f81d0ff5`)
La scena menu registra (e quindi crea, `0x8b3cb0` → `0x726330` → `vt[0x48]`) tutte le sue pagine in 0x40476c, chiamata dalla
vtable 0x1265f10 a ogni costruzione del menu, anche dopo una gara (0x4048b4 → 0x726468 → 0x7240d8 è dentro questa catena). Si
registra subito dopo `Orig`, con la stessa fabbrica, lo stesso proprietario e lo stesso layer:
```cpp
HOOK_DEFINE_TRAMPOLINE(MenuPagesSetupHook) {                         /* 0x40476c(scena menu): registra le pagine del menu */
    static void Callback(uint8_t* self) {
        Orig(self);
        cw8::ready = false;
        if (!cw8::active) return;
        uint8_t* tab = mk8dx::Fn<uint8_t* (*)()>(0x8bcc5c)();
        uintptr_t fac = *(uintptr_t*)(mk8dx::Base() + 0x1307b38);   /* GOT -> 0x40918c (CharaList) */
        if (fac != mk8dx::Base() + 0x40918c || *(uintptr_t*)(tab + 0x8 + 0x28 * 8) != fac || *(uint8_t**)(tab + 0x448 + 0x28 * 8) != self) {
            Logging.Log("[mk8dx-8p] cw8: 0x40476c non e' la scena delle CharaList, niente pagine extra"); return; }
        for (int j = 0; j < 4; j++) {                                /* tutto o niente: voci libere o gia' nostre */
            uintptr_t f = *(uintptr_t*)(tab + 0x8 + cw8::kIds[j] * 8);
            if (f && f != fac) { Logging.Log("[mk8dx-8p] cw8: id 0x%x occupato (fabbrica %lx)", cw8::kIds[j], f - mk8dx::Base()); return; }
        }
        for (int j = 0; j < 4; j++) {
            int32_t id = cw8::kIds[j];
            *(uintptr_t*)(tab + 0x8 + id * 8) = fac;
            *(uint8_t**)(tab + 0x448 + id * 8) = self;
            mk8dx::Fn<void (*)(uint8_t*, int32_t*, int)>(0x8b3cb0)(self, &id, 0xa);   /* w20 = 0xa come 0x404d8c */
        }
        cw8::ready = true;
        Logging.Log("[mk8dx-8p] cw8: CharaList slot 4..7 registrate (id 01 02 5e 64)");
    }
};
```
Dopo una gara la voce resta con la fabbrica CharaList e il vecchio proprietario (la scena menu distrutta), come per le voci di gara
del retail. È innocuo: nessuno crea per id fuori dalla registrazione, e a ogni nuovo menu la voce viene riscritta [IPOTESI forte].

### 2.5 Indice +0x1e8 delle pagine nuove: hook inline su 0x4b28d8 (`b901ea68` `str w8,[x19,#0x1e8]`)
0x4b28d4 calcola `w8 = id − 0x28`, che per gli id nuovi è negativo. L'hook corregge w8 prima della scrittura, e quindi anche
`0x43ce4c(btn, slot)` (0x4b2998) riceve lo slot giusto. Nessun salto punta a 0x4b28d8 [VERIFICATO].
```cpp
HOOK_DEFINE_INLINE(CharaListSlotHook) {                              /* 0x4b28d8 onCreate CharaList: x19 = pagina, w8 = id-0x28 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        int s = cw8::SlotOfId(*(int32_t*)((uint8_t*)ctx->X[19] + 0xe8));
        if (s >= 0) { ctx->W[8] = s; Logging.Log("[mk8dx-8p] cw8: CharaList %p id 0x%x -> slot %d", (void*)ctx->X[19], *(int32_t*)((uint8_t*)ctx->X[19] + 0xe8), s); }
    }
};
```

## 3. Rilocazione dei link +0x660 e delle code +0x928

### 3.1 Struttura attuale [VERIFICATO, ctor 0x4bc844]
- **Link** (0x28 B): `0x8b18cc(link)` (+0 vtable base, +8/+0xc = −1, +0x10 = −1, +0x18 = 0, +0x1c = 0, +0x20 = −1, +0x24 = 0), poi
  vtable = `[GOT 0x1307ec8] + 0x10` (0x12d3b78). Slot 0..3 a +0x660/+0x688/+0x6b0/+0x6d8, quinto link indipendente a +0x700
  (0x4b8a14, **non si tocca**). `0x8b21cc(link, &id, p)`: link+0xc = id, link+0x20 = p. Distruttore: solo 0x8ba458 (base pagina),
  nessuna azione sui link.
- **Code** (0x28 B): +0 puntatore ai dati (= voce+0x14), +8 capacità 2, +0xc testa, +0x10 conteggio, +0x14..+0x23 dati (2 × u64).
  Elemento: u64 (push di −1 o del valore della variante, `str x,[ptr, idx, sxtw #3]`). Uso: richieste di cambio variante dalla
  CharaList (0x4b4018 → 0x4b4f08) e dall'evento (0x4bc4ec), consumate per slot in 0x4b9190 (ricarica del modello 3D, 0x13d424)
  [IPOTESI sul significato, VERIFICATO il flusso].

### 3.2 Nuovo layout (oggetto 0xa68 → **0xce8**)
| campo | vecchio | nuovo |
|---|---|---|
| stato slot (passo 1) | +0x904 [4×8] | +0xa28 [8×8] (fatto) |
| link per slot | +0x660 [4×0x28] | **+0xa68 [8×0x28]** (fino a +0xba7) |
| code per slot | +0x928 [4×0x28] | **+0xba8 [8×0x28]** (fino a +0xce7); dati della voce i a +0xbbc+i*0x28 |
| dimensione | 0xa68 | **0xce8**: `mov w0,#0xce8` = `52819d00` alle 3 fabbriche |

Le vecchie aree restano costruite dal ctor originale (code vuote, link con id −1) e diventano sentinelle (§5).

### 3.3 Tutti gli accessi (parola originale verificata su main.bin → parola nuova)
Ricerca: ogni `ldr/str/add` con immediato 0x660..0x727 e 0x928..0x9c7 in 0x4b0000-0x4c6000 (più costanti `mov` in quegli
intervalli: nessuna), con classificazione della base. Esclusi: 0x4b2834 e 0x4b74e8..0x4b7538 (GOT via adrp), 0x4b2c84/0x4b2fd4
(`+0x698` della scena, cast con typeinfo 0x12fae80), 0x4b1708/0x4b1928 (adrp, costanti float), 0x4be698 (adrp, stringa), 0x4c0c84
(`strb +0x938` sulla pagina 0x4a), 0x4c0d10 (pagina 0x48), 0x4bd788.. (adrp) [VERIFICATO]. La CharaList raggiunge la madre solo
tramite +0x1d8: 34 chiamate da (madre, slot) verso 0x4694fc, 0x4b1504, 0x4b3428, 0x4b43b4, 0x4b4f08, 0x4b52d0, 0x4be370, 0x8ba52c,
0x422108. Di queste, solo 0x4b4f08 e la lettura diretta 0x4b53c0 toccano +0x928 [VERIFICATO]. Le forme `madd x, i, #0x28, base`
non cambiano, perché il passo resta 0x28: cambia solo l'immediato del campo.

```cpp
namespace cw8 {
    /* step5b passo 2: link +0x660->+0xa68, code +0x928->+0xba8, oggetto 0xa68->0xce8 (p2_verify.py su main.bin) */
    static const sel8::P kReloc[] = {
        {0x4b7a38, 0x91198260, 0x9129a260},   /* link init vt77    add x0, x19, #0x660 -> #0xa68 */
        {0x4b7a50, 0x911a2260, 0x912a4260},   /* link init vt77    add x0, x19, #0x688 -> #0xa90 */
        {0x4b7a68, 0x911ac260, 0x912ae260},   /* link init vt77    add x0, x19, #0x6b0 -> #0xab8 */
        {0x4b7a80, 0x911b6260, 0x912b8260},   /* link init vt77    add x0, x19, #0x6d8 -> #0xae0 */
        {0x4bc640, 0x91198101, 0x9129a101},   /* link uso vt92     add x1, x8, #0x660  -> #0xa68 */
        {0x4b4f1c, 0x9124a12a, 0x912ea12a},   /* CharaList 0x4b4f08  add x10, x9, #0x928 -> #0xba8 */
        {0x4b4f20, 0x9124e12b, 0x912ee12b},   /*                     add x11, x9, #0x938 -> #0xbb8 */
        {0x4b4f24, 0xb9493929, 0xb94bb929},   /*                     ldr w9, [x9, #0x938] */
        {0x4b4f40, 0xb949318c, 0xb94bb18c},   /*                     ldr w12, [x12, #0x930] */
        {0x4b4f5c, 0xb9493508, 0xb94bb508},   /*                     ldr w8, [x8, #0x934] */
        {0x4b4f90, 0xb949318c, 0xb94bb18c},   /*                     ldr w12, [x12, #0x930] */
        {0x4b4fac, 0xb9493508, 0xb94bb508},   /*                     ldr w8, [x8, #0x934] */
        {0x4b4fdc, 0xb94931ad, 0xb94bb1ad},   /*                     ldr w13, [x13, #0x930] */
        {0x4b4ff8, 0xb9493508, 0xb94bb508},   /*                     ldr w8, [x8, #0x934] */
        {0x4b53d4, 0xb9493908, 0xb94bb908},   /* CharaList vt88      ldr w8, [x8, #0x938] (x8 = [+0x1d8] + slot*0x28) */
        {0x4b8fa8, 0xb9493a68, 0xb94bba68},   /* coda[0] 0x4b8e44    ldr w8, [x19, #0x938] (slot 0 fisso, +0x8c8 = 0x34/0x1d) */
        {0x4b8fb4, 0xb9493269, 0xb94bb269},   /*                     ldr w9, [x19, #0x930] */
        {0x4b8fc0, 0xb949366b, 0xb94bb66b},   /*                     ldr w11, [x19, #0x934] */
        {0x4b8fc8, 0xb9093a6a, 0xb90bba6a},   /*                     str w10, [x19, #0x938] */
        {0x4b8fcc, 0xf944966a, 0xf945d66a},   /*                     ldr x10, [x19, #0x928] */
        {0x4b91f0, 0xb9493909, 0xb94bb909},   /* 0x4b9190 (consumo)  ldr w9, [x8, #0x938] */
        {0x4b9200, 0x9124e116, 0x912ee116},   /*                     add x22, x8, #0x938 (poi [x22] a 0x4b9234/0x4b93f0/0x4b9418) */
        {0x4b9238, 0xf944953c, 0xf945d53c},   /*                     ldr x28, [x9, #0x928] */
        {0x4b9244, 0xb9493509, 0xb94bb509},   /*                     ldr w9, [x8, #0x934] */
        {0x4b9248, 0xb9493108, 0xb94bb108},   /*                     ldr w8, [x8, #0x930] */
        {0x4b9400, 0xb949352a, 0xb94bb52a},   /*                     ldr w10, [x9, #0x934] */
        {0x4b9404, 0xb949312c, 0xb94bb12c},   /*                     ldr w12, [x9, #0x930] */
        {0x4b9414, 0xb909352a, 0xb90bb52a},   /*                     str w10, [x9, #0x934] */
        {0x4ba1bc, 0xb9493948, 0xb94bb948},   /* vt93 0x4b99c8       ldr w8, [x10, #0x938] */
        {0x4ba1dc, 0xb9493129, 0xb94bb129},   /*                     ldr w9, [x9, #0x930] */
        {0x4ba1e8, 0x9124e14a, 0x912ee14a},   /*                     add x10, x10, #0x938 */
        {0x4ba1fc, 0xf944954b, 0xf945d54b},   /*                     ldr x11, [x10, #0x928] */
        {0x4ba200, 0xb949354a, 0xb94bb54a},   /*                     ldr w10, [x10, #0x934] */
        {0x4bb298, 0xb949396b, 0xb94bb96b},   /* vt87 0x4badb0       ldr w11, [x11, #0x938] */
        {0x4bc500, 0xb9493969, 0xb94bb969},   /* vt92 0x4bbd18       ldr w9, [x11, #0x938] */
        {0x4bc514, 0xb949314a, 0xb94bb14a},   /*                     ldr w10, [x10, #0x930] */
        {0x4bc520, 0x9124e16b, 0x912ee16b},   /*                     add x11, x11, #0x938 */
        {0x4bc534, 0xf944950b, 0xf945d50b},   /*                     ldr x11, [x8, #0x928] */
        {0x4bc538, 0xb9493508, 0xb94bb508},   /*                     ldr w8, [x8, #0x934] */
    };                                                       /* 5 link + 34 code = 39 */
    static const sel8::P kFactory[] = {                      /* orig = parola del passo 1 (mov w0,#0xa68), vanilla 0x52814500 */
        {0x4091c4, 0x52814d00, 0x52819d00}, {0x4091f0, 0x52814d00, 0x52819d00}, {0x40921c, 0x52814d00, 0x52819d00},  /* #0xce8 */
    };
}
```
Il ctor 0x4bc924..0x4bc988 **non si patcha**: continua a costruire le vecchie code, che diventano sentinelle, e le nuove le
costruisce il trampolino. Nessuna delle 42 parole coincide con hook o patch esistenti in main.cpp, salvo le 3 fabbriche,
concatenate al passo 1 [VERIFICATO].

### 3.4 Ctor: estensione di `CharaSelBaseCtorHook` (0x4bc844, già installato dal passo 1)
```cpp
        /* passo 2 (dopo il codice del passo 1): link [8] a +0xa68 e code [8] a +0xba8 */
        if (cw8::active) {
            uintptr_t vt = *(uintptr_t*)(mk8dx::Base() + 0x1307ec8) + 0x10;           /* come 0x4bc8d4..0x4bc8e4 */
            for (int i = 0; i < 8; i++) {
                uint8_t* l = self + 0xa68 + i * 0x28;
                mk8dx::Fn<void (*)(uint8_t*)>(0x8b18cc)(l);                              /* usa solo x0 */
                *(uintptr_t*)l = vt;
            }
            for (int i = 0; i < 8; i++) {
                uint8_t* q = self + 0xba8 + i * 0x28;
                memset(q, 0, 0x28);
                *(uint8_t**)q = q + 0x14;                                               /* come 0x4bc924/0x4bc930 */
                *(int32_t*)(q + 0x8) = 2;                                               /* capacita' (0x4bc93c) */
            }
        }
```

### 3.5 Id dei link: hook inline su 0x4b7a98 (`f940be68` `ldr x8,[x19,#0x178]`, subito dopo il quarto 0x8b21cc)
I 4 `add` patchati inizializzano i link 0..3 nella nuova posizione, con gli id 0x28..0x2b e −1 (w27, 0x4b7a18). Gli slot 4..7 si
inizializzano nello stesso punto del flusso. Nessun salto punta a 0x4b7a98 [VERIFICATO]. Il link riceve l'id nuovo direttamente:
`link->vt[2]` apre la pagina dal framework, fuori dagli LR della §2.2.
```cpp
HOOK_DEFINE_INLINE(CharselLinksHook) {                               /* 0x4b7a98 in vt77 0x4b74a4, x19 = pagina selezione */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* page = (uint8_t*)ctx->X[19];
        for (int j = 0; j < 4; j++) {
            int32_t id = cw8::kIds[j];
            mk8dx::Fn<void (*)(uint8_t*, int32_t*, int)>(0x8b21cc)(page + 0xa68 + (4 + j) * 0x28, &id, -1);
        }
    }
};
```

### 3.6 Installazione (in exl_main, dopo il passo 1, tutto o niente)
```cpp
    if (sel8::m2) {   /* step5b passo 2: richiede il passo 1 (fabbriche gia' a 0xa68) */
        bool ok = *reinterpret_cast<uint32_t*>(base + 0x8bf0ec) == 0xf81e0ff3 && *reinterpret_cast<uint32_t*>(base + 0x40476c) == 0xf81d0ff5
               && *reinterpret_cast<uint32_t*>(base + 0x4b28d8) == 0xb901ea68 && *reinterpret_cast<uint32_t*>(base + 0x4b7a98) == 0xf940be68;
        for (const sel8::P& p : cw8::kReloc)   if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok = false; Logging.Log("[mk8dx-8p] cw8: parola inattesa a %x", p.off); }
        for (const sel8::P& p : cw8::kFactory) if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok = false; Logging.Log("[mk8dx-8p] cw8: fabbrica inattesa a %x", p.off); }
        if (ok) {
            cw8::active = true;                                      /* prima degli hook: il ctor costruisce la coda */
            CharselLinksHook::InstallAtOffset(0x4b7a98);
            CharaListSlotHook::InstallAtOffset(0x4b28d8);
            PageGetRemapHook::InstallAtOffset(0x8bf0ec);
            MenuPagesSetupHook::InstallAtOffset(0x40476c);
            exl::patch::RandomAccessPatcher rp;                      /* stessa sequenza del passo 1 */
            for (const sel8::P& p : cw8::kReloc)   rp.Write<uint32_t>(p.off, p.repl);
            for (const sel8::P& p : cw8::kFactory) rp.Write<uint32_t>(p.off, p.repl);
            Logging.Log("[mk8dx-8p] cw8: link/code rilocati (%d parole), oggetto 0xce8", (int)(sizeof(cw8::kReloc) / sizeof(sel8::P)));
        } else Logging.Log("[mk8dx-8p] cw8: passo 2 NON attivato");
    }
```
Ordine: tutto deve stare prima della costruzione del menu (exl_main, come il passo 1). `CharselEnterHook` (0x4b74a4) e l'hook
inline 0x4b7a98 nella stessa funzione sono compatibili: il trampolino è sulla prima istruzione, l'inline a metà funzione.

## 4. Clamp degli array rilocati (da applicare nel passo 4)
| clamp | parola | nuova | indice usato per | tipo |
|---|---|---|---|---|
| 0x4b4f0c | 7100103f `cmp w1,#4` | 7100203f | coda (CharaList 0x4b4f08, slot = +0x1e8) | puro |
| 0x4b53c8 | 7100113f `cmp w9,#4` | 7100213f | coda (CharaList vt88) | puro |
| 0x4b91e4 | f100129f `cmp x20,#4` | f100229f | coda (x27 solo nei 4 madd di 0x4b9190) | puro |
| 0x4bb28c | f100113f `cmp x9,#4` | f100213f | coda (vt87) | puro |
| 0x4bc4f0 | 7100111f `cmp w8,#4` | 7100211f | coda (vt92) | puro |
| 0x4bc630 | 7100111f `cmp w8,#4` | 7100211f | link (vt92) | puro |
| 0x4b9b78 | f100129f `cmp x20,#4` | f100229f | x21: +0x904 (già in `kClamp904`), cfg+0x1fb4/+0x1fc4 (0x4b9bfc, 0x4b9ca4, 0x4b9db8, 0x4b9eb4), coda 0x4ba1b8 | **condiviso** (passo 4 + 6) |

Tutte le parole sono verificate (`p2_table.txt`). **Raccomandazione: nessuno nel passo 2.** Con +0x8c0 = 4 nessun indice supera 3:
il selettore dà slot < 4, i cicli vanno fino a +0x8c0, e le CharaList 4..7, che passerebbero 4..7 a 0x4b4f08/0x4b53c8, non si aprono
mai. I 6 puri sarebbero innocui anche subito, ma non servono a niente e allargano la differenza da provare. Vanno nel passo 4,
insieme a +0x8c0 = N e ai 39 `kClamp904` (0x4b9b78 è già fra quelli). Con #8 a 0x4b9b78 lo slot 4..7 scriverebbe cfg+0x1fb4[4..7] =
cfg+0x1fc4[0..3]: va risolto con l'ombra cfg del passo 6, oppure con un clamp separato per la config.

Nel passo 4, oltre ai clamp, per le CharaList 4..7 servono anche:
- maschera input dell'onEnter 0x4b2ab8-0x4b2b6c, solo bit 0..3: trampolino 0x4b29e0 che dopo `Orig` imposta il bit dello
  slot (attenzione al bit 4 = tocco, step 5 C2);
- `ProfileToCharaWinHook`/`CharselProfileHook`: togliere il limite a 4 quando i pad 4..7 sono veri;
- 0x4b6970: 8 siti a costante 0x28..0x2b (convalida/azzeramento per 4 slot fissi);
- colore del pulsante 0x43ce4c (frame slot+1, bit 0..3);
- config per slot nella CharaList (passo 6).

## 5. Verifiche in Eden
1. Avvio: `cw8: link/code rilocati (39 parole), oggetto 0xce8`, nessuna riga `parola inattesa`. Ingresso nel menu: 3 righe
   `M2: ctor selezione`, poi `cw8: CharaList slot 4..7 registrate` e 4 righe `cw8: CharaList … -> slot 4..7` (onCreate). Se le righe
   onCreate arrivano dopo, la creazione è differita: va bene, purché arrivino prima dell'ingresso nella selezione [IPOTESI].
2. Sentinelle, nel `CharselFrameHook` già presente, ogni 120 frame e alla prima differenza: le vecchie code devono avere
   `*(int32_t*)(page+0x938+i*0x28) == 0`, i vecchi link `*(int32_t*)(page+0x660+i*0x28+0xc) == -1` (mai inizializzati). I nuovi link
   (dopo vt77) hanno +0xc = 0x28,0x29,0x2a,0x2b,0x01,0x02,0x5e,0x64. `cw8::hits == 0` per tutto il passo 2.
3. Pagine: `0x8bf0ec(&id)` per 0x01/0x02/0x5e/0x64 ≠ NULL, `+0xe8 == id`, `+0x1e8 == 4..7`, `+0xf0 ==` scena menu. Va controllato
   anche `0x8bf0ec(&0)` (DebugMenuSeq), che deve restare quello di prima: un id fuori dal numero di pagine lo sovrascriverebbe.
4. **1P, 2P, 4P**: scegliere personaggi **con varianti** (Yoshi, Tipo Timido, Birdo, Inkling, Link) per ogni giocatore, cambiare
   colore più volte (coda: anteprima 3D aggiornata), annullare, riaprire, confermare. Poi kart, gara, ritorno al menu e di nuovo
   (pagine ricostruite: righe del punto 1 di nuovo, nessun `id occupato`). Atteso: identico al vanilla.
5. **5-8P a due turni**: come sopra in entrambi i turni (gli slot 0..3 riusati al turno 2 aprono CharaList0..3). Atteso: identico
   al passo 1.
6. Prova del rimappaggio (GDB, facoltativa): P1 conferma un personaggio (stato ≥ 5). Breakpoint a 0x4ba884 (`str w8,[sp,#0x18]`,
   vt86), `set $w8 = 0x2c`, poi `stepi` fino a 0x4ba88c: x0 deve essere la pagina id 0x01 (`x/wx $x0+0xe8` = 1) e il log deve
   mostrare `cw8: 0x2c -> 0x1`. È innocuo: legge solo +0x120 della pagina chiusa. Usare `set breakpoint always-inserted on` e
   rimuovere la trappola con `stepi` (bug dello stub).
7. Watchpoint (se una sentinella scatta): `watch *(int*)(page+0x938)` / `+0x960` / `+0x988` / `+0x9b0` (conteggi delle vecchie code).

## 6. Rischi
| rischio | prob. | mitigazione |
|---|---|---|
| accesso a +0x928/+0x660 non trovato (base calcolata altrove) | bassa: ricerca su tutti gli immediati 0x660..0x727/0x928..0x9c7 della zona, costanti `mov`, 34 chiamate CharaList→madre controllate | sentinelle (§5.2) e watchpoint |
| 0x40476c chiamata anche da un'altra scena (puntatore anche a 0xd54c88) | bassa | controllo che fabbrica e proprietario di 0x28 siano quelli di `self` |
| creazione delle pagine differita o legata all'ordine di registrazione | media | log onCreate; in alternativa, registrare con un hook inline dopo 0x404e70 (registrazione di 0x2b) |
| memoria: 4 CharaList in più (layout, pulsante, 0x310 B ciascuna) e +0x280 B × 3 pagine selezione | bassa | log dell'allocatore 0x5fa960 ≠ NULL (onCreate fallirebbe) |
| 0x87 o id ≥ numero pagine | evitato | id scelti < 0x87 |
| LR sbagliato → pagina sbagliata in 1-4P | nulla: gli LR sono solo dei siti `0x28+i`, e con +0x8c0 = 4 non producono 0x2c..0x2f | `cw8::hits` = 0 nel passo 2 |
| voce stantia (fabbrica CharaList, proprietario distrutto) dopo l'uscita dal menu | bassa (nessuno crea per id fuori dalla registrazione; i cicli 0x8b3a30/0x8b3b6c controllano `+0xf0 == proprietario`) | riscritta a ogni menu |
| CharaList 4..7 aperte nel passo 4 senza maschera di input / cfg | – (fuori dal passo 2) | §4 elenco |
