# Step 5b – passo 3: 8 cursori attivi insieme sulla griglia (selettore cursori, focus, memoria, cursori grafici)

Data: 3 nov 2026. Solo analisi statica. Base: `../img/main.bin` (offset = indirizzo) e `../img/main.ann.asm`. `src/` non modificato, nessuna build.
Dati in `step5b_data (dati non pubblicati)/`:
- `p3_verify.py`: verifica tutte le parole, i siti e le prime parole degli hook, disassemblando orig/nuova con llvm-objdump. Uscita in `p3_verify.txt`, **0 errori**.
- `p3_table.inc`: tabelle C++ generate dallo script.
- `p3_scan_lsl5.{py,txt}`: indici `add x,cur,i,lsl #5` in tutto il .text.
- `p3_scan_fixed.{py,txt}`: accessi a slot fissi dopo `ldr [pagina,#0x1b0]`.
- `p3_scan_after.py` con `p3_scan_focus.txt` (mgr+0xc68), `p3_scan_gfx.txt` (+0x1d0 dopo 0x8bf228) e `p3_scan_memtab.txt` ([[0x1306e70]]+0x60).

Etichette: [VERIFICATO] = letto in main.bin/main.ann.asm (con indirizzo); [IPOTESI] = da confermare in Eden.

## 0. Sintesi
- **Oggetto selettore** (0xb8 B): ha **una sola allocazione**, `mov w0,#0xb8` a 0x8bafcc, in 0x8bafb8(pagina, capacità). Questa funzione è il metodo
  "crea cursore" che le pagine chiamano con un thunk (8 thunk; selezione Multi 0x4bcfe8 con capacità 0x32). Ha un solo ctor (0x8af218, chiamato
  solo da 0x8bafdc) e una sola init (0x8af2c4) [VERIFICATO].
- **Decisione: slot virtuali in coda, senza spostare gli slot 0..3.** Lo slot s di un pad usa l'indice virtuale `V = s` per s < 4 e `V = s + 4`
  per s = 4..7. Gli slot 4..7 finiscono quindi a `+0x28 + V*0x20` = **+0x128..+0x1a7** (oggetto 0xb8 → **0x1a8**). La zona +0xb8..+0x127 resta
  libera e contiene i metadati della mod (`+0xb8` magic `'CUR8'`, `+0xbc` n = 4/8).
  - Gli slot 0..3, le maschere +0xa8/+0xac/+0xb0 e **tutti i ~60 accessi a slot fissi** (framework e pagine) restano identici: nessuna parola di
    accesso cambia.
  - Cambia solo chi **produce l'indice**: i clamp `cmp #4`. Ognuno riceve un hook PRE sul `cmp`, con il `cmp` portato a `#8` e il registro
    sorgente del `csel` posto a V(cur,p). Con un cursore a 4 slot V vale `p < 4 ? p : 0`, cioè esattamente il clamp vanilla: **1-4P e pausa a 8
    sono invariati per costruzione**.
- La rilocazione completa (alternativa A: 8 slot contigui a +0xb8) richiederebbe di riscrivere oltre 100 accessi a campi di slot nel framework
  (stima grezza: 231 istruzioni con offset 0x28..0xa7 da classificare) e circa 60 accessi esterni a slot fissi (`p3_scan_fixed.txt`). È stata
  scartata [VERIFICATO i conteggi, IPOTESI il numero esatto].
- Lo stesso indice V indicizza altri tre array a 4 [VERIFICATO]:
  - **cursori grafici** `cm_Page_Cursor+0x1d0[V]`: con V = 8..11 l'indirizzo è +0x210..+0x228. La pagina passa da 0x1f8 a **0x230** (unica
    allocazione, 0x408efc) e i cursori grafici 04..07 vengono creati lì. Nessuna parola di accesso cambia.
  - **focus** `mgr+0xc68[V]`: array ombra `cur8::foc[4]` nel modulo, 2 hook che spostano la base delle letture (0x8b0114, 0x8b0284) e trampolini
    sul calcolo per frame (0x8bd6d8) e su setFocus (0x8bd5f8).
  - **memoria dell'indice** `(O+0x60)[idPagina][V]`, int[0x88][4]: array ombra `cur8::mem8[0x88][4]` e 3 hook che spostano la base (0x8af5d0,
    0x8af6d8, 0x8afc28).
- Totale del passo 3:
  - **24 parole**: 20 `cmp #4→#8`, 2 dimensioni di allocazione, `cmp x25,#3→#7` e uno `str→nop`.
  - **30 hook inline**: 20 clamp, 1 cursore grafico, 3 memoria, 2 focus, 4 ciclo di vita della pagina.
  - **11 trampolini**, 1 REPLACE e l'integrazione di `CursorPollHook` (0x8b0084), più 1 trampolino solo per la prova.
  - Tutti i siti sono verificati da `p3_verify.py`.
- **Il passo 3 da solo è inerte**: un cursore diventa largo (n = 8) solo quando la pagina Multi ha +0x8c0 > 4, cioè dal passo 4, oppure con
  `MK8DX_P3TEST` (§9). I 39 clamp della classe selezione (`kClamp904`), i 7 del passo 2 e i 9 siti di pagina che calcolano lo slot da soli
  (`kPageSlotFix`, §6) restano al passo 4/5.

## 1. Oggetto selettore cursori (pagina+0x1b0) – (a)

### 1.1 Creazione [VERIFICATO]
| cosa | dove |
|---|---|
| alloc + ctor + init | **0x8bafb8(pagina, w1 = capacità item)**: `mov w0,#0xb8` (0x8bafcc) → 0x5fa960 → ctor **0x8af218** → `pagina+0x1b0 = cur` (0x8bafe4) → navigatore 0x20 B (ctor 0x8b08c0) → **0x8af2c4(cur, pagina, cap)** (+0x8 = pagina, PtrArray item a +0x10 con capacità w1, +0xa8 = 0x1e5, +0xac = 0x1e1) → 0x8afbe0(cur, nav) (+0x20 = nav, nav+0x8 = cur) |
| chiamanti di 0x8bafb8 | solo thunk `mov w1,#cap; b 0x8bafb8`: 0x409f94 (0xa), 0x490044 (0x32), 0x49a0fc (0x1e), 0x4b0910 (0x15), **0x4bcfec (0x32, selezione)**, 0x4c4ed8 (kart: `+0x39c << 2`), 0x4d2a90 (0x1e), 0x4daf60 (0x14) |
| ctor 0x8af218 | unico chiamante 0x8bafdc; vtable [GOT 0x1310788]+0x10 = 0x12d34e0 (solo RTTI 0x8b0bc0/0x8b0c30, **nessun distruttore**); memset +0x8..+0x8f; slot 0..3 con il valore iniziale; +0xa8 = 0 (u64), +0xb0 = 0 |
| init 0x8af2c4 | unico chiamante 0x8bb000 |
Il cursore non viene mai copiato né azzerato per intero: l'unico memset è 0x88 B dal ctor. Non ci sono dimensioni scritte altrove.

### 1.2 Campi [VERIFICATO ctor 0x8af218, reset 0x8af308, 0x8af3f4, 0x8b0238]
| offset | tipo | significato |
|---|---|---|
| +0x00 | ptr | vtable (0x12d34e0) |
| +0x08 | ptr | pagina |
| +0x10/+0x14/+0x18 | s32/s32/ptr | PtrArray degli item: numero, capacità, array. Item = controllo; **item+0x58** maschera dei pad (default 0x8000000f, ctor 0x8ad888), **+0x60** cur (0 di default, 0x8ad894), **+0x68** indice, +0x78 visibile |
| +0x20 | ptr | navigatore (vt+0: `next(idx, tasti)`) |
| **+0x28 + V·0x20** | 0x20 B | slot. +0 stato (0 nessuno, 1/2 inattivo, 3/4 attivo, 5/6 decide, 7 deciso); +4 u8 flag "riattiva"; +8 indice corrente; +0xc indice precedente (−1); +0x10 −1 (indice dell'animazione, 0x8af63c); +0x14..+0x1c posizione float3 = [GOT 0x1310790] = 0xf73e28 = (−FLT_MAX)×3 |
| +0xa8 / +0xac | u32 | tasti di movimento (0x1e5) / di riattivazione (0x1e1) |
| +0xb0 | u32 | bit p = "focus appena preso" (0x8aff4c), azzerato da 0x8b01ec |
| +0xb4 | – | padding (oggetto 0xb8) |
| **mod: +0xb8 / +0xbc** | u32 | magic `'CUR8'` / n slot (4 o 8) |
| **mod: +0x128..+0x1a7** | 4×0x20 | slot 4..7 (V = 8..11) |

### 1.3 Funzioni e accessi
Funzioni della classe (x0 = cur, salvo dove indicato) [VERIFICATO]:
| funzione | ruolo | accesso agli slot | passo 3 |
|---|---|---|---|
| 0x8af218 | ctor | 0..3 srotolati | TRAMPOLINE: inizializza la coda |
| 0x8af308 | reset (svuota gli item, item+0x60 = 0) | 0..3 srotolati | TRAMPOLINE: reset della coda |
| 0x8af3c0 | aggiunge un item | – | – |
| 0x8af3f4 (cur,p,idx,tasti,anim,forza) | seleziona | clamp **0x8af474/0x8af490** → x27 = slot + memoria (0x8af5d0, 0x8af6d8) + cursore grafico (0x8af690) | clamp F6/F7 + memoria A/B |
| 0x8af87c | cerca l'item successivo con il bit p in +0x58 | `1<<p` non limitato | – |
| 0x8afaf0 (cur,p) | rilascia | clamp 0x8afb48 | F8 |
| 0x8afbec (cur,p) | ripristina l'indice dalla memoria | clamp 0x8afc1c + memoria 0x8afc28 | F9 + memoria C |
| 0x8afc54 | ripristina i 4 slot all'ingresso della pagina (da 0x8bb454) | **srotolato 0..3** | TRAMPOLINE |
| 0x8afdd0 | attiva i 4 slot (da 0x8bb4fc) | **srotolato 0..3**, chiama 0x8af3f4(cur,s,…) | TRAMPOLINE |
| 0x8aff4c (cur,p) | +0xb0 \|= 1<<p (da 0x8ba61c, focus preso) | – | – |
| 0x8aff64 (cur,p) | focus perso: 6→7, 5→4 (da 0x8ba62c) | clamp 0x8aff70 | F10 |
| 0x8affa4 (cur,p) | decide (animazione del cursore grafico) | clamp 0x8affcc → cursore grafico 0x8affdc + slot | F11 |
| 0x8b0034 (cur,p) | fine decide | clamp 0x8b0040 | F12 |
| **0x8b0084** | per frame: 0x8b00e0(cur,0..3) **srotolato** (da 0x8bc45c) | – | CursorPollHook (esiste, si integra) |
| 0x8b00e0 (cur,p) | lettura del pad p: focus `mgr+0xc68[clamp]`, 0x8ba634, slot, GetPad(p), nav, 0x8af3f4 | clamp 0x8b0108 → focus 0x8b0114 + slot | F13 + focus P |
| **0x8b01ec** | per frame: 0x8b0238(cur,0..3) **srotolato**, poi +0xb0 = 0 (da 0x8bdac0/0x8bdba4) | – | REPLACE |
| 0x8b0238 (cur,p) | disegno: focus, cursore grafico, slot | clamp 0x8b0278 → focus 0x8b0284 + cursore grafico 0x8b02d0 + slot x25 | F14 + focus D |
| 0x8b0888 (x,p) | cursore grafico di p (da 0x4b3058, 0x4b9554, 0x4ba390) | clamp 0x8b08a4 | F15 (globale) |
| 0x8ae1d0 / 0x8ae21c / 0x8ae264 / 0x8ae328 (item,p) | seleziona se non selezionato / selezionato da p / rilascia / deciso | clamp 0x8ae1e4 / 0x8ae228 / 0x8ae274 / 0x8ae334 | F1..F4 |
| 0x8ae2a4 (item) | "selezionato da un pad qualsiasi" (da 0x43b7c0, 0x43b83c) | **srotolato 0..3** (+0x28/+0x48/+0x68/+0x88) | TRAMPOLINE |
| 0x8ae628 (item,p) | vt+0x70 dei controlli: posa il cursore grafico | clamp 0x8ae64c → cursore grafico | F5 |

Accessi fuori dalla classe:
- **Framework pagina** (x0/x19 = pagina) [VERIFICATO]:
  - 0x8ba614/0x8ba624: focus preso/perso → 0x8aff4c/0x8aff64.
  - 0x8ba77c (clamp 0x8ba7a4): evento all'item del cursore di p.
  - 0x8ba814 (clamp 0x8ba828): item deciso da p.
  - 0x8bb454/0x8bb4fc: init e attivazione all'ingresso della pagina.
  - Ciclo di vita, **4 slot fissi**: 0x8bb8c8..0x8bb90c (uscita: si attende finché uno slot è in 5..6), 0x8bb984..0x8bbaa0 (attivazione:
    slot 6/7 → item vt+0x28(item, s)), 0x8bbbf4..0x8bbc38 (attesa: slot in 5..6).
  - 0x8bb51c..0x8bb564: setFocus per i pad 0..3, srotolato.
- **Classi con un clamp proprio**: 0x44355c, controllo con +0xb8 pagina e +0xc0 giocatore, vtable 0x126a1f8 (clamp 0x443590); 0x49df40, pagina
  con giocatore a +0x1e4 (clamp 0x49dfbc) [VERIFICATO].
- **Slot fissi nelle pagine** (`p3_scan_fixed.txt`): 42 siti in 0x48..0x4f leggono lo slot 0 (+0x28 stato / +0x30 indice) del proprio cursore,
  più 18 fuori dal menu (0x1474e0.. e 0x12755c, 0x54fa54: altre classi con un campo +0x1b0). Con gli slot virtuali **non cambiano**.
- **Indici calcolati nelle pagine** (`p3_scan_lsl5.txt`): regali 0x4b1d3c; selezione 0x4b8858, 0x4ba45c, 0x4ba5fc; kart 0x4c1928, 0x4c1b04,
  0x4c2bd4, 0x4c3dcc, 0x4c3e48. Tutti con indici limitati a 4 dai clamp di pagina (`kClamp904` o kart): restano al passo 4/5 (§6).
  0x384e10.., 0x38c400, 0x7159b0, 0x7b35.., 0x90dc54.., 0x9a8f58.., 0xaff45c.. sono altre classi (base da x0/[x0], senza +0x1b0).

## 2. I clamp `cmp #4` – (b)

### 2.1 0x8ae1e4..0x8b16b4: 20 `cmp #4`, di cui 15 clamp e 5 no [VERIFICATO]
- **Non sono clamp**: 0x8af620 (`stato == 4`), 0x8afff0 (`(stato & ~1) == 4`), 0x8b0310 (`stato ≥ 4`), 0x8b1694/0x8b16b4 (`pagina+0x120 < 4`,
  stato della pagina). Non si toccano.
- **I 15 clamp di pad** (F1..F15) e i **5 generici fuori intervallo** (G1..G5) sono nella tabella §2.3. Array indicizzati oltre allo slot:
  - F5, F11, F15, G5: cursore grafico +0x1d0.
  - F7: x27 indicizza slot, memoria e cursore grafico.
  - F9: memoria.
  - F13: focus.
  - F14: focus e cursore grafico.

### 2.2 Meccanismo (identico a 4 per costruzione)
Ogni sito ha questa forma [VERIFICATO da `p3_verify.py`]:

```
sxtw xS, wP   (oppure ldrsw xS)
…
cmp wP, #4
…
csel xD, xS, xZR, lo
```

La sorgente xS è distinta dal pad wP, oppure coincide con wP ed è morta dopo il `csel` (G3). Il piano:
1. la parola del `cmp` diventa `cmp wP,#8`;
2. un hook INLINE **sul `cmp`** pone `xS = V(cur, p)`. L'istruzione spostata è il `cmp #8`, eseguito dopo la callback: i flag sono corretti. Lo
   stesso schema c'è già con `PlayerColorIdxHook` a 0x86a720, provato in Eden.
3. Il `csel` dà `p < 8 ? V : 0`.

```
V(cur,p) = p < 4 ? p : (4 ≤ p < 8 && n(cur) == 8) ? p + 4 : 0
```

Con n = 4: p < 4 → p; p = 4..7 (tocco nel vanilla) → 0; p ≥ 8 o negativo → 0. È il clamp originale. **Ordine obbligatorio: prima la patch
`cmp #8` (con Flush), poi l'hook**, altrimenti l'istruzione spostata resta `cmp #4`.
Per F15 e G5 il cursore non è disponibile: `Vg(p)` usa `cur8::wideCur` (non nullo solo quando c'è un cursore largo).

### 2.3 Tabella (da `p3_table.inc`, verificata su main.bin)
```cpp
namespace cur8 {
    struct C { uint32_t off, orig, repl; };
    static const C kClampCmp[] = {                    /* `cmp wP,#4` -> `#8`; hook PRE: X[src] = V(cur, W[pad]) */
        {0x8ae1e4, 0x7100103f, 0x7100203f},   /* F1  pad w1  src x8  cur X0        0x8ae1d0(item,p) seleziona se non selezionato */
        {0x8ae228, 0x7100103f, 0x7100203f},   /* F2  pad w1  src x10 cur X9        0x8ae21c(item,p) item selezionato da p */
        {0x8ae274, 0x7100103f, 0x7100203f},   /* F3  pad w1  src x9  cur X0        0x8ae264(item,p) rilascia se selezionato */
        {0x8ae334, 0x7100103f, 0x7100203f},   /* F4  pad w1  src x10 cur X9        0x8ae328(item,p) deciso da p */
        {0x8ae64c, 0x7100129f, 0x7100229f},   /* F5  pad w20 src x8  cur [X19+0x60] 0x8ae628(item,p) cursore grafico */
        {0x8af474, 0x710012bf, 0x710022bf},   /* F6  pad w21 src x10 cur X19       0x8af3f4 seleziona: stato */
        {0x8af490, 0x710012bf, 0x710022bf},   /* F7  pad w21 src x10 cur X19       0x8af3f4 seleziona: x27 slot+memoria+grafico */
        {0x8afb48, 0x7100127f, 0x7100227f},   /* F8  pad w19 src x9  cur X20       0x8afaf0 rilascia */
        {0x8afc1c, 0x7100103f, 0x7100203f},   /* F9  pad w1  src x8  cur X0        0x8afbec ripristina indice (memoria) */
        {0x8aff70, 0x7100103f, 0x7100203f},   /* F10 pad w1  src x8  cur X0        0x8aff64 focus perso */
        {0x8affcc, 0x7100129f, 0x7100229f},   /* F11 pad w20 src x8  cur X19       0x8affa4 decide (grafico) */
        {0x8b0040, 0x7100103f, 0x7100203f},   /* F12 pad w1  src x8  cur X0        0x8b0034 fine decide */
        {0x8b0108, 0x7100127f, 0x7100227f},   /* F13 pad w19 src x8  cur X20       0x8b00e0 lettura pad (focus) */
        {0x8b0278, 0x710012bf, 0x710022bf},   /* F14 pad w21 src x8  cur X19       0x8b0238 disegno (focus+grafico) */
        {0x8b08a4, 0x7100127f, 0x7100227f},   /* F15 pad w19 src x8  globale       0x8b0888(x,p) cursore grafico di p */
        {0x8ba7a4, 0x7100127f, 0x7100227f},   /* G1  pad w19 src x10 cur X8        0x8ba77c(page,p) evento all'item del cursore */
        {0x8ba828, 0x7100103f, 0x7100203f},   /* G2  pad w1  src x10 cur X8        0x8ba814(page,p) item deciso */
        {0x443590, 0x7100113f, 0x7100213f},   /* G3  pad w9  src x9  cur X8        0x44355c controllo (+0xb8 pagina, +0xc0 giocatore) */
        {0x49dfbc, 0x7100129f, 0x7100229f},   /* G4  pad w20 src x8  cur X9        0x49df40 pagina (+0x1e4 giocatore) */
        {0x49abdc, 0x7100103f, 0x7100203f},   /* G5  pad w1  src x8  globale       0x49abc4(cp,p,f) scala del cursore grafico */
    };
}
```
Note sul cursore nei registri (verificate a mano nell'asm):
- F1/F3: x0 viene da `ldr x0,[x8,#0x60]`.
- F5: x19 = item; +0x60 = cur, 0 se l'item non è registrato in un cursore (0x8ad894).
- F13: x20 = cur, x0 = mgr.
- G1/G2: x8 = `[x0,#0x1b0]`.
- G3: x8 = `[[x19+0xb8]+0x1b0]` (0x44358c).
- G4: x9 = `[x19,#0x1b0]`.

Sono **escluse dalle patch**:
- **0x8bd610** (setFocus): gestito da un trampolino (§3).
- **I clamp di pagina** (selezione, kart, regali): 0x4b1d20, 0x4b87d4, 0x4ba404, 0x4ba5a4, 0x4c18d0, 0x4c1aa4 (in `kClamp904`), più 0x4c2b70,
  0x4c3dc4, 0x4c3e40 (kart). Indicizzano anche +0x904, +0x460, la config o la pagina 0x8bf2c8+0x1d0: vanno al passo 4/5 (§6).

## 3. Focus per pad `mgr+0xc68` – (c)
UI manager: 0x8bdc8c(), 0xda0 B, ctor 0x8bcd80. `+0xc68` = ptr[4] (pagina con il focus del pad p), poi `+0xc88` (altro campo, ptr: 0x8bcfe8,
0x8bd078, 0x8bd08c). Il ctor fa memset di 0x70 B da +0xc68 (0x8bcdf4..0x8bce3c).

Tutti gli accessi a +0xc68..+0xc87 [VERIFICATO: grep di 0x8a0000-0x8c8000, `p3_scan_focus.txt` su tutto il .text]:
| sito | funzione | indice |
|---|---|---|
| 0x8b0114 `ldr x8,[x8,#0xc68]` | 0x8b00e0 lettura pad | clamp F13 |
| 0x8b0284 `ldr x8,[x8,#0xc68]` | 0x8b0238 disegno | clamp F14 |
| 0x8bd61c / 0x8bd62c→0x8bd63c | **0x8bd5f8(mgr,p,pagina)** setFocus: clamp 0x8bd610; perdita (0x8ba624) / scrittura / acquisto (0x8ba614) | clamp proprio |
| 0x8bd894 / 0x8bd8ec→0x8bd934, 0x8bd94c, 0x8bd860, 0x8bd870 (x25) | **0x8bd6d8** per frame (unico chiamante 0x8b110c), ciclo p = 0..3 (`cmp x20,#4` 0x8bd96c). Se la pagina del focus è ancora nella lista la tiene; altrimenti prende la pagina più in alto con il bit p in +0x190; senza pagina nasconde il cursore grafico p (0x8bd87c → 0x443290) | p < 4 |
| 0x8bcdf4 memset | ctor | – |
Non sono mgr: 0x8addf0/0x8ade08/0x8ae1bc/0x8baaac/0x8b9d18 (adrp+GOT o .bss).

Estensione: un ombra `cur8::foc[4]` nel modulo per i pad 4..7, **attiva solo se esiste un cursore largo** (`wideCur`):
- **Lettura**: hook INLINE a 0x8b0114 e 0x8b0284. x0 = mgr e x8 = mgr + V·8, quindi `V = (x8 - x0) >> 3`; per V = 8..11 si pone
  `x8 = &foc[V-8] - 0xc68`. L'istruzione `ldr` spostata legge l'ombra.
- **Per frame**: TRAMPOLINE 0x8bd6d8 (`d105c3ff`, nessuna uscita anticipata prima del ciclo [VERIFICATO]). Dopo `Orig`, `FocusExtra(mgr)` ripete
  la logica di 0x8bd890..0x8bd970 per p = 4..7.
- **setFocus**: TRAMPOLINE 0x8bd5f8 (`f81d0ff5`). Con un cursore largo e p = 4..7 scrive l'ombra; altrimenti chiama `Orig`, così il vanilla
  (p ≥ 4 → pad 0) resta identico.
- **Ingresso della pagina**: hook INLINE 0x8bb568 (`f940b268`, punto di confluenza dopo i 4 setFocus srotolati 0x8bb51c..0x8bb564; x19 = pagina,
  x20 = mgr). Se il cursore della pagina è largo, setFocus per i pad 4..7 con il bit in +0x190.
- Gli eventi di perdita e acquisto (0x8ba624 → 0x8aff64, 0x8ba614 → 0x8aff4c) per p ≥ 4 si mandano **solo alle pagine con cursore largo o senza
  cursore**. Su un cursore a 4 slot V vale 0 e toccherebbe lo slot 0. Se il pad passa a una pagina senza slot 4..7, il suo cursore grafico si
  nasconde (0x443290) [IPOTESI: 0x443290 = Out/nascondi, è la chiamata del vanilla nello stesso caso].

## 4. Memoria dell'indice `[[0x1306e70]]+0x60` – (d)
[GOT 0x1306e70] = 0x135bb20 (globale O). `O+0x60` = puntatore alla tabella **int[0x88][4]**: per id pagina (`cmp #0x88; csel → 0`, passo 16 B)
l'ultimo indice di ciascuno slot. Chi la alloca o inizializza non è stato cercato [IPOTESI: −1 o 0; con i lettori entrambi danno l'indice 0].
Accessi, tutti nel framework [VERIFICATO, `p3_scan_memtab.txt` e lettura dell'asm]:
| sito | operazione | indice slot |
|---|---|---|
| 0x8af550→**0x8af5d0** `str w0,[x8,x27,lsl #2]`, 0x8af5b8→0x8af5d0 | scrittura (0x8af3f4, selezione cambiata) | x27 (F7) |
| 0x8af6bc→**0x8af6d8** `str w8,[x9,x27,lsl #2]` | scrittura (0x8af3f4) | x27 |
| 0x8afc04→**0x8afc28** `ldr w9,[x9,x8,lsl #2]` | lettura (0x8afbec) | x8 (F9) |
| 0x8afc94/0x8afcac, 0x8afce4/0x8afd00, 0x8afd38/0x8afd54, 0x8afd8c/0x8afda8 | lettura srotolata 0..3 (0x8afc54) | costante |
0x401a6c e 0x7e4e94 sono `[vt+0x60]` dell'oggetto O (metodo), non la tabella.

Estensione: `cur8::mem8[0x88][4]` (slot 4..7) inizializzata a −1. Hook INLINE sui tre accessi con indice: se l'indice vale 8..11, la base
diventa `base - tbl + mem8 - 0x20`, con `tbl = *(O+0x60)`. In questo modo `base + V*4 = &mem8[pid][V-8]`. Il ciclo srotolato 0x8afc54 riceve un
TRAMPOLINE (§7).

## 5. Cursori grafici `cm_Page_Cursor` – (e)
Pagina: alloc 0x1f8 a 0x408efc (unica; ctor 0x49a8c4 chiamato solo da 0x408f08), +0x1d0 ptr[4], +0x1f0 float frame Loop, +0x1f4 byte.
L'onCreate 0x49a8fc ha un ciclo su i = 7..0 (x22 = 0x41..0x3a, x25 = i):
- per i ≤ 3 (0x49a968 `cmp x25,#3; b.hi`): alloc 0xc8 (0x5902d0), ctor 0x442a84(obj, i) (+0xb4 = i), 0x8bbf04(pagina, obj+0x30, nome),
  **`str x21,[x19,x22,lsl #3]`** (0x49a9c0, cioè +0x1d0+i·8) e 0x442ca0(obj, i+1) (frame di ColorChange);
- per i ≥ 4: pane con 0x725ad0(pagina, nome) e `+0x58 &= ~1` (nascosto).
[VERIFICATO]

Con gli slot virtuali il cursore grafico di p = 4..7 è `+0x1d0 + (p+4)·8` = **+0x210 + (p−4)·8**. Piano:
| parola/hook | effetto |
|---|---|
| 0x408efc `321d17e0` → `52804600` | pagina 0x1f8 → 0x230 (+0x1f8..+0x22f nuovi; +0x210..+0x228 = cursori grafici 04..07) |
| 0x49a968 `f1000f3f` → `f1001f3f` | `cmp x25,#7`: crea anche 04..07, con il percorso nativo (alloc, ctor, bind 0x8bbf04) |
| 0x49a9c0 `f8367a75` → `d503201f` + INLINE | la scrittura la fa la callback: i < 4 a +0x1d0+i·8 (come l'originale); i ≥ 4 a +0x210+(i−4)·8 e **w1 = i+3** (frame 7..10 = rosso, blu, viola, arancio = P5..P8, step 7 [IPOTESI forte]) |
| TRAMPOLINE 0x49a8fc (`d10203ff`) | dopo `Orig`: pane 04..07 **nascosti** come nel vanilla (0x725ad0 + `+0x58 &= ~1`) e log |

Accessi a +0x1d0[...] [VERIFICATO]:
- **Con indice da clamp**: 0x8ae658 (F5), 0x8af694 (x27 F7), 0x8affdc (F11), 0x8b02d0 (F14), 0x8b08b0 (F15), 0x49abe8/0x49ac74 (G5). Con V puntano
  da soli a +0x210.. e non serve nessuna patch.
- **Con indice < 4 non limitato**: 0x8bd880 (focus per frame). Per 4..7 lo gestisce `FocusExtra` con `cur8::Gfx(p)`.
- **Indice fisso 0**: 0x49e054, 0x4e6dac, `pause8::TintCursor0`.
- **Cicli srotolati della pagina**:
  - 0x49aa1c, metodo in vtable 0x12720b0: sincronizza il frame Loop su +0x1f0.
  - 0x49ab04: "un cursore grafico occupato" (da 0x49d33c).
  - 0x49ab68(cp, b): attiva o disattiva tutti (da 0x4234f4).

  Ricevono un TRAMPOLINE che, solo con un cursore largo, ripete l'operazione su 04..07.
- Non si toccano: 0x4c2ba8 (pagina **0x8bf2c8**+0x1d0, un'altra pagina: passo 5) e 0x49ae1c.. (pagina 0xe).
**In 1-4P** i 04..07 esistono ma con il pane nascosto, come nel vanilla. Nessuno li anima: il disegno ha n = 4 e i cicli estesi sono condizionati
[IPOTESI forte: nessuna differenza visibile; verificare con uno screenshot]. Alternativa se non va: lasciare 0x49a968 vanilla e crearli in C++ in
`SetWide`.

## 6. Numero di giocatori, passo 3 e passo 4 – (f)
Il selettore **non ha un conteggio**: il 4 è strutturale (slot, cicli e clamp). Un pad p partecipa se valgono tutte queste condizioni
[VERIFICATO]:
1. la pagina può ricevere l'input di p (0x8ba634): bit 31 e bit p in **pagina+0x1a8**, stato della pagina +0x120 == 4, e p non ha già agito in
   questo frame (`[0x8bf09c()]+0x30` & (1<<p|bit31) == 0, impostato da 0x8af3f4 e azzerato ogni frame);
2. **focus** di p sulla pagina (bit p in pagina+0x190, default 0xffffffff da 0x8ba43c);
3. **maschera dell'item** +0x58 con il bit p (0x8af87c, 0x8af4f8; default 0x8000000f);
4. **slot attivato** (0x8afdd0 → 0x8af3f4);
5. GetPad(p) non nullo.

Nella selezione il ciclo 0x4b80e4..0x4b8118 accende già i bit 0..N−1 di +0x1a8 (lsl non limitato, N = cfg+0x1fa4). Il pad del tocco usa idx 8
(C2: GetPad(4) → GetPad(8) a 0x4b82a4/0x4bbcdc/0x4c17c8/0x4c3c98): **in 5-8P il pad UI 4 è P5**, sia nel framework sia in GetPadHook
[VERIFICATO main.cpp]. Il tocco a idx 8 non viene letto dai cicli del framework (poll fino a n−1 ≤ 7, scanner 0..7): in 5-8P il tocco nei menu
non funziona, ma succedeva già dallo step 5 C.

Riparto:
| cosa | passo 3 | passo 4 |
|---|---|---|
| framework: slot virtuali, clamp F/G, focus, memoria, cursori grafici, lettura e disegno a n, ciclo di vita | **sì** (inerte con n = 4) | – |
| `SetWide(pagina)`: n = 8, maschere item +0x58 \|= bit 4..N−1, pane 04..07 visibili, attivazione e focus degli slot 4..N−1 | **funzione pronta**; chiamata solo con `MK8DX_P3TEST` | chiamata in CharselEnterHook se `+0x8c0 > 4` |
| +0x8c0 = N, 39 `kClamp904` + 7 clamp del passo 2, fine dei due turni | – | sì |
| siti di pagina che calcolano lo slot da soli (`add x,cur,i,lsl #5` + `[x,#0x30]`): 9 hook INLINE "x += 0x80 se l'indice ≥ 4" (`kPageSlotFix`) | – | sì (selezione 0x4b885c, 0x4ba460, 0x4ba600; regali 0x4b1d40); kart al passo 5 |
| gestore di vt92 0x4bbd18 per p ≥ 4 (anteprima, cfg+0x1fb4[slot], stato 5) | eventi p ≥ 4 **scartati** solo nella prova | tramite clamp → +0x904 rilocato |
| maschera input delle CharaList 4..7 (0x4b29e0), colore del pulsante 0x43ce4c | – | sì (passo 2 §4) |
`kPageSlotFix` (verificata, **da non installare nel passo 3**):
```cpp
    static const H kPageSlotFix[] = {          /* ldr/ldrsw [xT,#0x30] dopo add xT,cur,xI,lsl #5: se xT-cur >= 0x80 -> xT += 0x80 */
        {0x4b1d40, 0xb9803129},   /* regali    x9  cur=x8              (indice x9, clamp 0x4b1d20) */
        {0x4b885c, 0xb9803108},   /* selezione x8  cur=x9              (clamp 0x4b87d4) */
        {0x4ba460, 0xb9403102},   /* selezione x8  cur=x0              (x23, clamp 0x4ba404) */
        {0x4ba600, 0xb9403102},   /* selezione x8  cur=x0              (x23, clamp 0x4ba5a4) */
        {0x4c192c, 0xb9803108},   /* kart      x8  cur=[x19+0x1b0]     (x25, clamp 0x4c18d0) */
        {0x4c1b08, 0xb9803108},   /* kart      x8  cur=x9              (clamp 0x4c1aa4) */
        {0x4c2bd8, 0xb9803129},   /* kart      x9  cur=x8              (x22, clamp 0x4c2b70) */
        {0x4c3dd0, 0xb980314a},   /* kart      x10 cur=x9              (clamp 0x4c3dc4) */
        {0x4c3e4c, 0xb9803129},   /* kart      x9  cur=x8              (clamp 0x4c3e40) */
    };
```

## 7. Codice (passo 3)
```cpp
/* ---- step 5b passo 3 (step5b_plan_p3.md): selettore cursori a 8 slot "virtuali". Lo slot s>=4 usa l'indice V=s+4 (cur+0x128..),
 * gli slot 0..3 non si spostano. Clamp `cmp #4`->`#8` + hook PRE: V(cur,p) = p<4 ? p : (p<8 && n==8) ? p+4 : 0 (n=4 -> clamp vanilla). */
namespace cur8 {
    constexpr uint32_t kMagic = 0x38525543;                 /* 'CUR8' a cur+0xb8 */
    static bool on;                                          /* passo 3 installato */
    static uint8_t* wideCur;                                 /* cursore largo attivo (selezione Multi), nullptr = nessuno */
    static uint8_t* foc[4];                                  /* focus dei pad 4..7 (ombra di mgr+0xc68[4]) */
    static int32_t mem8[0x88][4];                            /* ultimo indice per pagina degli slot 4..7 (ombra di (O+0x60)[0x88][4]) */
    static int32_t kFive = 5;
    inline bool Wide(const uint8_t* c) { return c && *(const uint32_t*)(c + 0xb8) == kMagic && *(const int32_t*)(c + 0xbc) == 8; }
    inline uint64_t V(const uint8_t* c, uint32_t p) { return p < 4 ? p : (p < 8 && Wide(c)) ? p + 4 : 0; }
    inline uint64_t Vg(uint32_t p) { return p < 4 ? p : (p < 8 && wideCur) ? p + 4 : 0; }
    inline uint8_t* Slot(uint8_t* c, int s) { return c + 0x28 + (s < 4 ? s : s + 4) * 0x20; }
    inline uint8_t* ItemCur(uint64_t item) { return item ? *(uint8_t**)(item + 0x60) : nullptr; }      /* 0 se non registrato (0x8ad894) */
    static uint8_t* GfxPage() { return mk8dx::Fn<uint8_t* (*)()>(0x8bf228)(); }
    static uint8_t* Gfx(int s) { uint8_t* cp = GfxPage(); return cp ? *(uint8_t**)(cp + 0x1d0 + (s < 4 ? s : s + 4) * 8) : nullptr; }
    static uintptr_t MemTab() {                              /* [[GOT 0x1306e70]]+0x60 */
        uint8_t** g = *(uint8_t***)(mk8dx::Base() + 0x1306e70); uint8_t* o = g ? *g : nullptr;
        return o ? *(uintptr_t*)(o + 0x60) : 0;
    }
    static void InitSlot(uint8_t* sl) {                      /* come 0x8af218/0x8af308 */
        const uint32_t* g = *(const uint32_t**)(mk8dx::Base() + 0x1310790);                   /* (-FLT_MAX)x3 */
        *(int32_t*)sl = 0; sl[4] = 0; *(int32_t*)(sl + 8) = 0; *(int32_t*)(sl + 0xc) = -1; *(int32_t*)(sl + 0x10) = -1;
        for (int k = 0; k < 3; k++) *(uint32_t*)(sl + 0x14 + 4 * k) = g[k];
    }
    static void ShowPanes(bool v) {                          /* pane cm_L_BtnCursor_04..07 (nel vanilla nascosti a 0x49a9cc) */
        uint8_t* cp = GfxPage(); if (!cp) return;
        char nm[] = "cm_L_BtnCursor_00";
        for (int i = 4; i < 8; i++) {
            nm[16] = '0' + i;
            uint8_t* pane = mk8dx::Fn<uint8_t* (*)(uint8_t*, const char*)>(0x725ad0)(cp, nm);   /* come 0x49a9d4 */
            if (pane) pane[0x58] = v ? (pane[0x58] | 1) : (pane[0x58] & ~1);
        }
    }
    static bool OwnsSlots(uint8_t* pg) { uint8_t* c = pg ? *(uint8_t**)(pg + 0x1b0) : nullptr; return !c || Wide(c); }
    static void SetFoc(int p, uint8_t* pg) {                 /* 0x8bd5f8 per p = 4..7, sull'ombra */
        uint8_t*& f = foc[p - 4];
        if (f == pg) return;
        if (f && OwnsSlots(f)) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8ba624)(f, p);
        f = pg;
        if (pg && OwnsSlots(pg)) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8ba614)(pg, p);
        else if (uint8_t* g = Gfx(p)) mk8dx::Fn<void (*)(uint8_t*)>(0x443290)(g);              /* pagina senza slot p: nasconde */
    }
    static void FocusExtra(uint8_t* mgr) {                   /* 0x8bd890..0x8bd970 per p = 4..7 */
        int32_t n = *(int32_t*)(mgr + 0xb50); uint8_t** list = *(uint8_t***)(mgr + 0xb58);
        for (int p = 4; p < 8; p++) {
            uint8_t* f0 = foc[p - 4];
            if (f0) {                                        /* 0x8bd894..0x8bd8e0: tiene il focus se la pagina e' ancora in lista */
                int32_t id = *(int32_t*)(f0 + 0xe8);
                uint8_t* pg = mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&id);
                bool in = false;
                for (int i = 0; pg && i < n; i++) if (list[i] == pg) { in = true; break; }
                if (in) continue;
            }
            uint8_t* top = nullptr;                          /* 0x8bd8fc..0x8bd930: dall'alto, bit p in +0x190 */
            for (int k = n - 1; k >= 0 && !top; k--) if (list[k] && ((*(uint32_t*)(list[k] + 0x190) >> p) & 1)) top = list[k];
            if (top) SetFoc(p, top);
            else if (!f0) { if (uint8_t* g = Gfx(p)) mk8dx::Fn<void (*)(uint8_t*)>(0x443290)(g); }   /* 0x8bd860..0x8bd888 */
        }
    }
    static bool AnyDeciding(uint8_t* c) {
        if (!Wide(c)) return false;
        for (int s = 4; s < 8; s++) if ((uint32_t)(*(int32_t*)Slot(c, s) - 5) < 2) return true;
        return false;
    }
    static void SetWide(uint8_t* page, bool w, int nLocal) { /* passo 4 (o prova): cursore della pagina a 8 slot */
        uint8_t* c = page ? *(uint8_t**)(page + 0x1b0) : nullptr;
        if (!on || !c || *(uint32_t*)(c + 0xb8) != kMagic) return;
        int32_t cnt = *(int32_t*)(c + 0x10); uint8_t** it = *(uint8_t***)(c + 0x18);
        uint32_t bits = 0; for (int p = 4; p < nLocal && p < 8; p++) bits |= 1u << p;
        if (w) {
            *(int32_t*)(c + 0xbc) = 8; wideCur = c;
            for (int k = 0; k < cnt; k++) if (it[k]) *(uint32_t*)(it[k] + 0x58) |= bits;      /* 0x8af87c: bit p negli item */
            ShowPanes(true);
            for (int p = 4; p < nLocal && p < 8; p++) {                                        /* come 0x8bb51c.. + 0x8afdd0 */
                if ((*(uint32_t*)(page + 0x190) >> p) & 1) SetFoc(p, page);
                uint8_t* sl = Slot(c, p);
                if (*(int32_t*)sl < 3) {
                    *(int32_t*)sl = 1;
                    mk8dx::Fn<bool (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(c, p, *(int32_t*)(sl + 8), 0, 1, 0);
                    if (*(int32_t*)sl == 1) *(int32_t*)sl = 2;
                }
            }
            Logging.Log("[mk8dx-8p] cur8: cursore %p della pagina 0x%x a 8 slot (N=%d, item=%d)", c, *(int32_t*)(page + 0xe8), nLocal, cnt);
        } else if (Wide(c)) {
            for (int p = 4; p < 8; p++) {
                InitSlot(Slot(c, p));
                if (uint8_t* g = Gfx(p)) mk8dx::Fn<void (*)(uint8_t*)>(0x443290)(g);
                if (foc[p - 4] == page) foc[p - 4] = nullptr;
            }
            for (int k = 0; k < cnt; k++) if (it[k]) *(uint32_t*)(it[k] + 0x58) &= ~0xf0u;
            *(int32_t*)(c + 0xbc) = 4; if (wideCur == c) wideCur = nullptr;
            ShowPanes(false);
            Logging.Log("[mk8dx-8p] cur8: cursore %p di nuovo a 4 slot", c);
        }
    }
    static void Reset() { wideCur = nullptr; for (auto& f : foc) f = nullptr; if (on) ShowPanes(false); }   /* SceneInitHook */
}
#define CUR8_CLAMP(N, PAD, SRC, CUR) HOOK_DEFINE_INLINE(Cur8Clamp##N) { \
    static void Callback(exl::hook::InlineCtx* ctx) { ctx->X[SRC] = cur8::V((const uint8_t*)(CUR), ctx->W[PAD]); } };
#define CUR8_CLAMPG(N, PAD, SRC) HOOK_DEFINE_INLINE(Cur8Clamp##N) { \
    static void Callback(exl::hook::InlineCtx* ctx) { ctx->X[SRC] = cur8::Vg(ctx->W[PAD]); } };
CUR8_CLAMP(F1, 1, 8, ctx->X[0])   CUR8_CLAMP(F2, 1, 10, ctx->X[9])  CUR8_CLAMP(F3, 1, 9, ctx->X[0])   CUR8_CLAMP(F4, 1, 10, ctx->X[9])
CUR8_CLAMP(F5, 20, 8, cur8::ItemCur(ctx->X[19]))                    CUR8_CLAMP(F6, 21, 10, ctx->X[19]) CUR8_CLAMP(F7, 21, 10, ctx->X[19])
CUR8_CLAMP(F8, 19, 9, ctx->X[20]) CUR8_CLAMP(F9, 1, 8, ctx->X[0])   CUR8_CLAMP(F10, 1, 8, ctx->X[0])  CUR8_CLAMP(F11, 20, 8, ctx->X[19])
CUR8_CLAMP(F12, 1, 8, ctx->X[0])  CUR8_CLAMP(F13, 19, 8, ctx->X[20]) CUR8_CLAMP(F14, 21, 8, ctx->X[19]) CUR8_CLAMPG(F15, 19, 8)
CUR8_CLAMP(G1, 19, 10, ctx->X[8]) CUR8_CLAMP(G2, 1, 10, ctx->X[8])  CUR8_CLAMP(G3, 9, 9, ctx->X[8])   CUR8_CLAMP(G4, 20, 8, ctx->X[9])
CUR8_CLAMPG(G5, 1, 8)
#define CUR8_MEM(N, BASE, IDX) HOOK_DEFINE_INLINE(Cur8Mem##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    uint64_t v = ctx->X[IDX]; if (v >= 8 && v < 12) { uintptr_t t = cur8::MemTab(); \
    if (t) ctx->X[BASE] = ctx->X[BASE] - t + (uintptr_t)cur8::mem8 - 0x20; } } };
CUR8_MEM(A, 8, 27)   /* 0x8af5d0 str w0,[x8,x27,lsl #2] */
CUR8_MEM(B, 9, 27)   /* 0x8af6d8 str w8,[x9,x27,lsl #2] */
CUR8_MEM(C, 9, 8)    /* 0x8afc28 ldr w9,[x9,x8,lsl #2]  */
#define CUR8_FOC(N) HOOK_DEFINE_INLINE(Cur8Foc##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    uint64_t v = (ctx->X[8] - ctx->X[0]) >> 3; if (v >= 8 && v < 12) ctx->X[8] = (uintptr_t)&cur8::foc[v - 8] - 0xc68; } };
CUR8_FOC(P)          /* 0x8b0114 ldr x8,[x8,#0xc68]  (x0 = mgr) */
CUR8_FOC(D)          /* 0x8b0284 ldr x8,[x8,#0xc68]  (x0 = mgr) */
HOOK_DEFINE_INLINE(Cur8GfxStoreHook) {                     /* 0x49a9c0 (gia' nop): x19 = cm_Page_Cursor, x22 = 0x3a+i, x21 = oggetto, w1 = i+1 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* cp = (uint8_t*)ctx->X[19]; int i = (int)ctx->X[22] - 0x3a;
        if (i < 4) *(uint64_t*)(cp + ctx->X[22] * 8) = ctx->X[21];                             /* +0x1d0+i*8 come l'originale */
        else { *(uint64_t*)(cp + 0x210 + (i - 4) * 8) = ctx->X[21]; ctx->W[1] = i + 3; }      /* slot virtuale, frame 7..10 */
    }
};
HOOK_DEFINE_INLINE(Cur8ExitWaitHook) {                     /* 0x8bb900 ldr w8,[x8,#0x88] (x8 = cursore): uscita, attende slot in 5..6 */
    static void Callback(exl::hook::InlineCtx* ctx) { if (cur8::AnyDeciding((uint8_t*)ctx->X[8])) ctx->X[8] = (uintptr_t)&cur8::kFive - 0x88; }
};
HOOK_DEFINE_INLINE(Cur8IdleWaitHook) {                     /* 0x8bbc2c ldr w8,[x8,#0x88]: idem (b.lo -> attesa) */
    static void Callback(exl::hook::InlineCtx* ctx) { if (cur8::AnyDeciding((uint8_t*)ctx->X[8])) ctx->X[8] = (uintptr_t)&cur8::kFive - 0x88; }
};
HOOK_DEFINE_INLINE(Cur8DecidedHook) {                      /* 0x8bbaa4 (x19 = pagina): dopo i 4 slot 6/7 -> item vt+0x28(item, s) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* c = *(uint8_t**)(ctx->X[19] + 0x1b0); if (!cur8::Wide(c)) return;
        uint32_t cnt = *(uint32_t*)(c + 0x10); uint8_t** it = *(uint8_t***)(c + 0x18);
        for (int s = 4; s < 8; s++) {
            uint8_t* sl = cur8::Slot(c, s); uint32_t i = *(uint32_t*)(sl + 8);
            if ((*(int32_t*)sl | 1) != 7 || i >= cnt || !it[i]) continue;
            (*(void (**)(uint8_t*, int))(*(uintptr_t*)it[i] + 0x28))(it[i], s);
        }
    }
};
HOOK_DEFINE_INLINE(Cur8PageFocusHook) {                    /* 0x8bb568: dopo setFocus 0..3 dell'ingresso pagina (x19 = pagina) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* pg = (uint8_t*)ctx->X[19];
        if (!cur8::Wide(*(uint8_t**)(pg + 0x1b0))) return;
        for (int p = 4; p < 8; p++) if ((*(uint32_t*)(pg + 0x190) >> p) & 1) cur8::SetFoc(p, pg);
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8CtorHook) {                     /* 0x8af218 ctor (oggetto 0x1a8 con la patch 0x8bafcc) */
    static void Callback(uint8_t* c) {
        Orig(c);
        for (int k = 0xb8; k < 0x128; k += 4) *(uint32_t*)(c + k) = 0;
        *(uint32_t*)(c + 0xb8) = cur8::kMagic; *(int32_t*)(c + 0xbc) = 4;
        for (int s = 4; s < 8; s++) cur8::InitSlot(cur8::Slot(c, s));
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8ResetHook) {                    /* 0x8af308 reset (da 0x4b5f50, 0x4f6dc4) */
    static void Callback(uint8_t* c) { Orig(c); if (*(uint32_t*)(c + 0xb8) == cur8::kMagic) for (int s = 4; s < 8; s++) cur8::InitSlot(cur8::Slot(c, s)); }
};
HOOK_DEFINE_TRAMPOLINE(Cur8RestoreHook) {                  /* 0x8afc54: slot dalla memoria all'ingresso della pagina (0x8bb454) */
    static void Callback(uint8_t* c) {
        Orig(c);
        if (!cur8::Wide(c)) return;
        uint32_t pid = *(uint32_t*)(*(uint8_t**)(c + 8) + 0xe8); if (pid >= 0x88) pid = 0;
        uint32_t cnt = *(uint32_t*)(c + 0x10);
        for (int s = 4; s < 8; s++) {
            uint8_t* sl = cur8::Slot(c, s); cur8::InitSlot(sl);
            int32_t m = cur8::mem8[pid][s - 4]; *(int32_t*)(sl + 8) = (m >= 0 && (uint32_t)m < cnt) ? m : 0;
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8ActivateHook) {                 /* 0x8afdd0: attiva gli slot (0x8bb4fc) */
    static void Callback(uint8_t* c) {
        Orig(c);
        if (!cur8::Wide(c) || !*(int32_t*)(c + 0x10)) return;
        for (int s = 4; s < 8; s++) {
            uint8_t* sl = cur8::Slot(c, s);
            if (sl[4]) { *(int32_t*)sl = 2; sl[4] = 0; continue; }
            *(int32_t*)sl = 1;
            mk8dx::Fn<bool (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(c, s, *(int32_t*)(sl + 8), 0, 1, 0);
            if (*(int32_t*)sl == 1) *(int32_t*)sl = 2;
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8AnyHook) {                      /* 0x8ae2a4(item): selezionato da un pad qualsiasi */
    static bool Callback(uint8_t* item) {
        if (Orig(item)) return true;
        uint8_t* c = *(uint8_t**)(item + 0x60); if (!cur8::Wide(c)) return false;
        int32_t idx = *(int32_t*)(item + 0x68);
        for (int s = 4; s < 8; s++) { uint8_t* sl = cur8::Slot(c, s); if (*(int32_t*)sl >= 3 && *(int32_t*)(sl + 8) == idx) return true; }
        return false;
    }
};
HOOK_DEFINE_REPLACE(Cur8DrawHook) {                        /* 0x8b01ec: 0x8b0238(cur,0..3) srotolato, poi +0xb0 = 0 */
    static void Callback(uint8_t* c) {
        int n = cur8::Wide(c) ? 8 : 4;
        for (int p = 0; p < n; p++) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b0238)(c, p);
        *(uint32_t*)(c + 0xb0) = 0;
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8FocusFrameHook) {               /* 0x8bd6d8(mgr) per frame */
    static void Callback(uint8_t* mgr) { Orig(mgr); if (cur8::wideCur) cur8::FocusExtra(mgr); }
};
HOOK_DEFINE_TRAMPOLINE(Cur8SetFocusHook) {                 /* 0x8bd5f8(mgr, p, pagina) */
    static void Callback(uint8_t* mgr, int32_t p, uint8_t* page) {
        if (cur8::wideCur && p >= 4 && p < 8) { cur8::SetFoc(p, page); return; }
        Orig(mgr, p, page);
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8GfxCreateHook) {                /* 0x49a8fc onCreate cm_Page_Cursor */
    static void Callback(uint8_t* cp) {
        Orig(cp);
        cur8::ShowPanes(false);                              /* come il vanilla: 04..07 nascosti */
        Logging.Log("[mk8dx-8p] cur8: cursori grafici 00..03 %p %p %p %p, 04..07 %p %p %p %p", *(void**)(cp + 0x1d0), *(void**)(cp + 0x1d8),
                    *(void**)(cp + 0x1e0), *(void**)(cp + 0x1e8), *(void**)(cp + 0x210), *(void**)(cp + 0x218), *(void**)(cp + 0x220), *(void**)(cp + 0x228));
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8GfxLoopHook) {                  /* 0x49aa1c: frame Loop condiviso (+0x1f0) */
    static void Callback(uint8_t* cp) {
        float f = *(float*)(cp + 0x1f0);
        Orig(cp);
        if (!cur8::wideCur) return;
        bool any = false;
        for (int i = 0; i < 4; i++) {
            uint8_t* g = *(uint8_t**)(cp + 0x210 + 8 * i);
            if (g && mk8dx::Fn<bool (*)(uint8_t*)>(0x442e70)(g) && g[0xc0]) { mk8dx::Fn<void (*)(uint8_t*, float)>(0x442ef0)(g, f); any = true; }
        }
        if (any && *(float*)(cp + 0x1f0) == 0.0f)            /* nessuno dei 00..03 attivo: avanza comunque (0x49aadc) */
            *(float*)(cp + 0x1f0) = mk8dx::Fn<float (*)(uint8_t*, float)>(0x442f10)(*(uint8_t**)(cp + 0x1d0), f);
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8GfxBusyHook) {                  /* 0x49ab04: un cursore grafico occupato */
    static bool Callback(uint8_t* cp) {
        if (Orig(cp)) return true;
        if (!cur8::wideCur) return false;
        for (int i = 0; i < 4; i++) { uint8_t* g = *(uint8_t**)(cp + 0x210 + 8 * i); if (g && mk8dx::Fn<bool (*)(uint8_t*)>(0x8adf84)(g + 0x30)) return true; }
        return false;
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8GfxEnableHook) {                /* 0x49ab68(cp, b) */
    static void Callback(uint8_t* cp, bool b) {
        Orig(cp, b);
        if (!cur8::wideCur) return;
        for (int i = 0; i < 4; i++) { uint8_t* g = *(uint8_t**)(cp + 0x210 + 8 * i); if (g) mk8dx::Fn<void (*)(uint8_t*, bool)>(0x8ae098)(g + 0x30, b); }
    }
};
```

**CursorPollHook (0x8b0084), modifica di quello esistente.** Il percorso della pausa (Owner ≥ 0) resta identico. Nel ramo normale si aggiunge
la lettura dei pad 4..7, solo per un cursore largo:
```cpp
        int p = pause8::Owner(cur);
        if (p < 0) {
            Orig(cur);
            if (cur8::Wide(cur) && *(int32_t*)(cur + 0x10))                  /* 0x8b0094: senza item niente lettura */
                for (int q = 4; q < 8; q++) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b00e0)(cur, q);   /* 0x8ba634 scarta q >= N */
            return;
        }
        /* ... resto invariato (step 7) ... */
```
Oggi `CursorPollHook` viene installato solo dentro il blocco "pausa a 8" (main.cpp:2440 circa). Lo si sposta in un'installazione propria (parola
`f81e0ff3`) con il flag `pause8::active`, impostato dal blocco della pausa e controllato da `Owner()` (`if (!pause8::active) return -1`). Così il
passo 3 funziona anche se il blocco della pausa non si attiva, e non si installano due hook su 0x8b0084.

**Selezione** (`CharselEnterHook`, 0x4b74a4, esistente), dopo `Orig(page)`:
```cpp
        {   bool w = cur8::on && padui::active8 && *(int32_t*)(page + 0xe8) == 0x2e && (MK8DX_P3TEST || *(int32_t*)(page + 0x8c0) > 4);
            cur8::SetWide(page, w, menu8::LocalCount()); }
```
Con `MK8DX_P3TEST`, in `CharselFrameHook`, nel ramo `round == 1 → 2` e prima di rimettere gli slot 0..3 a 3: `cur8::SetWide(page, false, 0);`.
**Reset**: `cur8::Reset()` in `SceneInitHook` (ogni cambio di scena; le pagine e i cursori vengono distrutti).

**Solo prova** (`#if MK8DX_P3TEST`): gli eventi della pagina con p ≥ 4 vengono scartati, così i giocatori 5-8 non scrivono nello slot 0 tramite i
clamp di pagina ancora a 4:
```cpp
HOOK_DEFINE_TRAMPOLINE(Cur8TestEventHook) {                /* 0x4bbd18 vt92 selezione (pagina, evento): ev+0x2c = slot/pad */
    static void Callback(uint8_t* page, uint8_t* ev) {
        uint32_t p = *(uint32_t*)(ev + 0x2c);
        if (p >= 4 && p < 8 && cur8::Wide(*(uint8_t**)(page + 0x1b0))) {
            static int n; if (n++ < 24) Logging.Log("[mk8dx-8p] cur8 TEST: evento id %d tipo %d di P%d scartato", *(int32_t*)(ev + 0x8), *(int32_t*)(ev + 0x28), p + 1);
            return;
        }
        Orig(page, ev);
    }
};
```

## 8. Installazione (exl_main, tutto o niente, indipendente dai passi 1-2)
```cpp
    {   /* step5b passo 3: selettore cursori a 8 slot virtuali (step5b_plan_p3.md) */
        bool ok = true;
        for (const cur8::C& c : cur8::kClampCmp) if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] cur8: parola inattesa a %x", c.off); }
        for (const cur8::C& c : cur8::kWords)    if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] cur8: parola inattesa a %x", c.off); }
        for (const cur8::H& h : cur8::kHookWords) {
            if (h.off == 0x4bbd18 && !MK8DX_P3TEST) continue;
            if (h.off == 0x8b0084 && pause8::pollHooked) continue;                       /* gia' agganciato da CursorPollHook */
            if (*reinterpret_cast<uint32_t*>(base + h.off) != h.word) { ok = false; Logging.Log("[mk8dx-8p] cur8: hook inatteso a %x", h.off); }
        }
        if (ok) {
            exl::patch::RandomAccessPatcher rp;                                          /* 1) parole: cmp #8, dimensioni, cmp x25 #7, nop */
            for (const cur8::C& c : cur8::kClampCmp) rp.Write<uint32_t>(c.off, c.repl);
            for (const cur8::C& c : cur8::kWords)    rp.Write<uint32_t>(c.off, c.repl);
            rp.Flush();                                                                  /* PRIMA degli hook inline sugli stessi indirizzi */
            Cur8ClampF1::InstallAtOffset(0x8ae1e4);  Cur8ClampF2::InstallAtOffset(0x8ae228);  Cur8ClampF3::InstallAtOffset(0x8ae274);
            Cur8ClampF4::InstallAtOffset(0x8ae334);  Cur8ClampF5::InstallAtOffset(0x8ae64c);  Cur8ClampF6::InstallAtOffset(0x8af474);
            Cur8ClampF7::InstallAtOffset(0x8af490);  Cur8ClampF8::InstallAtOffset(0x8afb48);  Cur8ClampF9::InstallAtOffset(0x8afc1c);
            Cur8ClampF10::InstallAtOffset(0x8aff70); Cur8ClampF11::InstallAtOffset(0x8affcc); Cur8ClampF12::InstallAtOffset(0x8b0040);
            Cur8ClampF13::InstallAtOffset(0x8b0108); Cur8ClampF14::InstallAtOffset(0x8b0278); Cur8ClampF15::InstallAtOffset(0x8b08a4);
            Cur8ClampG1::InstallAtOffset(0x8ba7a4);  Cur8ClampG2::InstallAtOffset(0x8ba828);  Cur8ClampG3::InstallAtOffset(0x443590);
            Cur8ClampG4::InstallAtOffset(0x49dfbc);  Cur8ClampG5::InstallAtOffset(0x49abdc);
            Cur8GfxStoreHook::InstallAtOffset(0x49a9c0);                                 /* istruzione spostata = nop */
            Cur8MemA::InstallAtOffset(0x8af5d0); Cur8MemB::InstallAtOffset(0x8af6d8); Cur8MemC::InstallAtOffset(0x8afc28);
            Cur8FocP::InstallAtOffset(0x8b0114); Cur8FocD::InstallAtOffset(0x8b0284);
            Cur8ExitWaitHook::InstallAtOffset(0x8bb900); Cur8IdleWaitHook::InstallAtOffset(0x8bbc2c);
            Cur8DecidedHook::InstallAtOffset(0x8bbaa4);  Cur8PageFocusHook::InstallAtOffset(0x8bb568);
            Cur8CtorHook::InstallAtOffset(0x8af218);     Cur8ResetHook::InstallAtOffset(0x8af308);
            Cur8RestoreHook::InstallAtOffset(0x8afc54);  Cur8ActivateHook::InstallAtOffset(0x8afdd0);
            Cur8AnyHook::InstallAtOffset(0x8ae2a4);      Cur8DrawHook::InstallAtOffset(0x8b01ec);
            Cur8FocusFrameHook::InstallAtOffset(0x8bd6d8); Cur8SetFocusHook::InstallAtOffset(0x8bd5f8);
            Cur8GfxCreateHook::InstallAtOffset(0x49a8fc); Cur8GfxLoopHook::InstallAtOffset(0x49aa1c);
            Cur8GfxBusyHook::InstallAtOffset(0x49ab04);   Cur8GfxEnableHook::InstallAtOffset(0x49ab68);
            if (!pause8::pollHooked) { CursorPollHook::InstallAtOffset(0x8b0084); pause8::pollHooked = true; }
#if MK8DX_P3TEST
            Cur8TestEventHook::InstallAtOffset(0x4bbd18);
#endif
            for (auto& r : cur8::mem8) for (auto& v : r) v = -1;                    /* nessuna memoria per gli slot 4..7 */
            cur8::on = true;
            Logging.Log("[mk8dx-8p] cur8: passo 3 attivo (cursore 0x1a8, cm_Page_Cursor 0x230, 20 clamp, test=%d)", MK8DX_P3TEST);
        } else Logging.Log("[mk8dx-8p] cur8: passo 3 NON attivato");
    }
```
Vincoli di ordine:
1. Tutto deve stare in exl_main, quindi prima della creazione di `cm_Page_Cursor` (componente globale) e di qualunque cursore.
2. Patch e Flush **prima** degli hook inline su 0x8ae1e4.. e 0x49a9c0.
3. `CursorPollHook` va installato una volta sola.

Il blocco è indipendente dai passi 1-2: tocca solo framework e `cm_Page_Cursor`.

**Capienza dei trampolini**: `JitSize` 0x8000 / (50 istruzioni × 4 B) = **163 hook con trampolino** (hook_impl.cpp:56-59) [VERIFICATO]. Oggi ci
sono circa 89 hook TRAMPOLINE o INLINE definiti (85 più i LakituMode). Il passo 3 ne aggiunge circa 42, per un totale di circa 131. Il passo 4 ne
aggiungerà altri (9 `kPageSlotFix` e altri). **Raccomandazione: `JitSize = 0x10000`** in `src/program/setting.hpp`. Il pool inline (24 B per
voce, 0x8000) basta.

## 9. Prova in Eden prima del passo 4 – (g)
**Build A (regressione, senza `MK8DX_P3TEST`)** – il passo 3 è inerte:
1. Log all'avvio: `cur8: passo 3 attivo …` e nessuna riga `parola inattesa`/`hook inatteso`. Ingresso nel menu: una riga `cur8: cursori grafici
   00..03 … 04..07 …` con 8 puntatori non nulli.
2. **1P, 2P, 4P**: menu principale, selezione personaggi (muovi, annulla, conferma), kart, regole, gara, **pausa** (P1..P4) e ritorno al menu.
   Atteso: identico al vanilla, compreso il cursore che riprende l'ultima posizione (memoria) e il colore. Screenshot della selezione 4P: nessun
   cursore extra visibile (04..07 nascosti).
3. **8P a due turni + gara + pausa di P1..P8** (step 7): identico all'ultima build (colore P5..P8 sul cursore 0, movimento, conferma).
4. Il log non deve mostrare `cur8: cursore … a 8 slot`.

**Build B (`#define MK8DX_P3TEST 1`)** – 8 cursori in 5-8P (turno 1):
1. N = 8 locali (8 controller). Alla selezione Multi: `cur8: cursore 0x… della pagina 0x2e a 8 slot (N=8, item=…)`.
2. Atteso a video [IPOTESI]: **8 cursori sulla griglia**, P1..P4 giallo, ciano, rosso, verde e P5..P8 rosso, blu, viola, arancio (frame 7..10). I
   cursori 5-8 partono dall'icona 0, oppure dall'ultima posizione dopo il primo giro. Ognuno si muove con la croce o lo stick del proprio
   controller, indipendentemente dagli altri; due cursori possono stare sulla stessa icona.
3. A, B o il movimento di P5..P8 non cambiano l'anteprima né lo stato di P1..P4: log `cur8 TEST: evento … di P5 scartato`. P1..P4 scelgono e
   confermano come prima.
4. Quando P1..P4 confermano (turno 2): `cur8: cursore … di nuovo a 4 slot`, i cursori 5-8 spariscono e il turno 2 procede come oggi (P5..P8 sui
   cursori 0..3).
5. Indietro (B di P1) fino al menu precedente e rientro: i cursori 5-8 ricompaiono nell'ultima posizione (memoria `mem8`).
6. Diagnostica in caso di problemi, nel ramo largo di `CursorPollHook`, ogni 120 frame: stato e indice degli slot V = 8..11, `foc[p-4] == pagina`,
   `Gfx(p)`, `0x8ba634(pagina,p)`.
   - Se un cursore 5-8 è fermo: il focus manca (FocusExtra/0x8bb568) oppure manca il bit in +0x1a8 o in +0x58.
   - Se è invisibile: il pane è nascosto oppure lo slot è in stato < 4.
7. Watchpoint facoltativo (GDB): `watch *(int*)(cur+0x28)` (slot 0) mentre si muove solo P5. Non deve scattare.

## 10. Rischi
| rischio | prob. | mitigazione |
|---|---|---|
| ordine patch/hook invertito (istruzione spostata = `cmp #4`): 5-8 mai attivi, nessun danno in 1-4P | media (errore di codice) | ordine §8; log del primo V ≥ 8 per sito |
| `cm_L_BtnCursor_04..07` creati in 1-4P visibili o animati | bassa | pane nascosti subito (0x49a8fc); in alternativa creazione differita in `SetWide` (0x49a968 vanilla) |
| semantica di 0x442e70/0x442ef0/0x442f10/0x443290/0x8ae098/0x8adf84 dedotta dall'uso | media | cosmetica (anima e mostra/nasconde); confronto video con i cursori 1-4 |
| `FocusExtra` diverso dal vanilla (pagina in cima con bit p) | bassa | stessa logica di 0x8bd890; più setFocus esplicito all'ingresso (0x8bb568) e in `SetWide` |
| eventi di focus p ≥ 4 su pagine con cursore a 4 slot (V = 0 → slot 0) | evitata | `OwnsSlots()`: eventi solo a pagine con cursore largo o senza cursore |
| item della selezione (vt+0x58) che reagiscono all'evento di p ≥ 4 con codice di pagina ancora a 4 | media nella prova | `Cur8TestEventHook`; nel passo 4 i clamp di pagina vanno a 8 |
| più di 163 trampolini | media (dal passo 4) | `JitSize` 0x10000 |
| oggetto cursore 0x1a8 su tutti i menu (+0xf0 B ciascuno) e pagina +0x38 B | trascurabile | – |
| `wideCur` non più valido dopo la distruzione della pagina | bassa | `Reset()` in SceneInitHook; `SetWide(false)` al turno 2 e a ogni ingresso con condizione falsa |
| prestazioni: circa 30 hook inline su percorsi chiamati per item e per pad (0x8ae21c dallo scanner) | bassa | callback di poche istruzioni; nessuna allocazione |
