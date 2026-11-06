# Step 6b – Piano: perché le celle 5-8 coprono il 2D a schermo intero (main 4.0.1)

Analisi statica (offset = main decompresso `../img/main.bin`, disassemblato `../img/main.ann.asm`). Nessuna modifica
a `src/`, Eden non eseguito. Estratti: `step6b_data/d_segment_ids.asm`, `d_segment_mgr.asm`,
`d_segment_sort_submit.asm`, `d_finalimage.asm`. Etichette: [VERIFICATO] = letto nell'asm; [IPOTESI].
Si parte dalle conclusioni di `step6_plan_C.md` §5-7 e dagli esperimenti 6C/6n già falliti.

## 0. Risposta breve
- **L'ordine GPU dei layer NON è l'ordine dei job sulla CPU: è l'ordine dello SLOT** [VERIFICATO].
  Ogni job Draw registra i comandi in **segmenti** di command buffer (0x684fa4) etichettati con un **id**
  `seg+0x160`; l'id è `slot*258 + 0xffff` (0x680f8c, 4° argomento di 0x683ecc) più 1..257 per gli step di
  rendering interni. A fine frame, dopo l'attesa dei worker, **0x66767c ordina la lista dei segmenti di ogni
  display per id crescente** (0x684590 → heapsort 0x689cec, cmp 0x684a64 = `a.id - b.id`) e 0x6845f8 li
  sottomette in quell'ordine.
- Conseguenza: i 3D extra (slot 14..17, id 69147.. — lo stesso "id 69147" del log 6m) vengono eseguiti dalla GPU
  **dopo 2D Main (slot 10), 2D Front (12) e 2D Overlay (13)**, quindi coprono minimappa e 3-2-1; i 2D extra
  (slot 18..21) arrivano ancora dopo, e per questo l'HUD delle celle 5-8 si vede sopra le celle.
  Spiega anche tutti i risultati precedenti:
  - priorità dei job (Layer3DPrioHook) senza effetto: la priorità ordina solo l'esecuzione sulla CPU;
  - 6C (extra disegnati dentro il job di Sub0) senza effetto: l'hook chiamava `Orig(..., ex, ex.slot*258+0xffff)`,
    cioè con l'id dello slot 14..17, e i segmenti vengono riordinati per id;
  - gruppi R (6n) senza effetto; Sub0/Sub1 a prio 5 nei dump senza conseguenze visibili;
  - in pausa gli extra stavano sopra l'immagine catturata (slot > 13) finché non li ha nascosti il bit 0x2.
- **Correzione proposta (E1)**: rimappare l'id passato a 0x683ecc (e a 0x684148) in base a un "rango" dello slot
  che metta 3D Sub4..7 subito dopo 3D Sub0..3 e 2D Sub4..7 subito dopo 2D Sub0..3. Il hook su 0x683ecc c'è già
  (LayerDrawJobHook): basta cambiare l'argomento `id`. Nessuna patch statica.
- Ipotesi a)/b) del brief (passo di composizione/post-process di R dopo il 2D, target o depth/stencil diversi):
  **non servono** per spiegare il fenomeno. FinalImage di R (0x6b55a8) scrive nel target del display dentro i
  segmenti del layer (stessa banda di id). Depth/stencil non verificati: da riprendere solo se E1 fallisce.

## 1. Segmenti di comandi e loro id [VERIFICATO]
### 1.1 Id assegnati dal job (`d_segment_ids.asm`)
| job (0x680f2c, switch su `job+0x18`) | chiamata | id (4° argomento / w3) |
|---|---|---|
| 0 Draw | 0x683ecc(display, ctx, layer, id) | `layer+0x84 * 0x102 + 0xffff` (0x680f80-0x680f8c: `madd w3,w8,w9,w10`, parola 0x1b092903) |
| 1 SubDraw | 0x684148(display, ctx, layer, id) | stessa formula (0x681078-0x681088); fa qualcosa solo se `display+0xcb0` ha 0x840, allora step con id+1 |
| 2 Begin | 0x684fa4(D, ctx, nome, **0**) → 0x6839b0 | 0 (0x680fd4 `mov w3,wzr`) |
| 3 End | 0x684fa4(D, ctx, nome, **0x201ff**) → 0x684170 | 0x201ff = 131583 (0x681008-0x68100c) |
| 4 GPUCalc | 0x6841d4(display, ctx, layer, slot+1) | 1..32 (0x681064-0x681068) |
Dentro 0x683ecc: segmento con id `w23` (0x683f3c / 0x683f78), poi gli step (0x683c5c, `w6 = id+1`; ogni step
apre un segmento con id `base + indice step`, 0x683d68-0x683d78) e un ultimo segmento con `vtbl[0x20]() + id`
(0x684060-0x684080). Banda di un layer: `[slot*258+0xffff, slot*258+0xffff+257]`; le bande degli slot sono
contigue (nessun buco tra slot 5 e slot 6).

### 1.2 Gestore dei segmenti D = `display+0xc60` (`d_segment_mgr.asm`)
- 0x684fa4(D, ctx, nome, id): core = TLS o `nn::os::GetCurrentCoreNumber`; `seq = D+0x80` (incremento atomico);
  voce `D+0x108 + seq*0x188` (capacità `D+0x100`, oltre → voce 0 con csel a 0x685040); memoria per core
  `D + core*0x28 + 0x88..0xb0`; scrive `seg+0x158` nome, **`seg+0x160` id**, `+0x168 = 0`, `+0x170` tick,
  `+0x180` core; `0x6265b8(ctx, seg)` dirotta il contesto di disegno sul segmento (ctx+0xf0 = seg,
  ctx+0xc0 = ctx+0x108).
- 0x685110(D, ctx, seq): chiude il segmento (0x62ddf8), aggiorna i contatori per core, `seg+0x178` tick di fine.
- 0x6838ec(display, ctx, seg, layer): se `seg+0x24` bit0 (segmento non vuoto): `seg+0x168 = layer`; se layer nullo
  o `layer+0x79` bit2 (0x483/0x4c3 → sì per tutti i layer in gara) → **push** in `display+0xfd0[display+0xfc8++]`
  (atomico, quindi nell'ordine di completamento sulla CPU).

### 1.3 Ordinamento e sottomissione (`d_segment_sort_submit.asm`)
```
0x703bdc: ... post code 0/1/2, attesa worker 0x62222c/0x6223f0 (0x704374) ... 0x7043b0 bl 0x66767c(ls)
0x66767c: if (ls+0x1bc == 1) for display in ls+0x38[0..ls+0x30): 0x684590(display)
0x684590: heapsort 0x689cec(&display+0xfc8, cmp = [0x130cc10] = 0x684a64)
0x684a64: return a->id(+0x160) - b->id(+0x160)
0x6845f8(display, cmd, flag) (da 0x66771c ← 0x705c24/0x705f08): copia i segmenti non vuoti di +0xfd0 in +0xff0
          nello stesso ordine e li sottomette in sequenza (display+0x9: 0 → 0x62e2bc; 1/2 → [0x130b150](cmd, 1, seg+0x30))
```
- 0x689cec è byte per byte identico (60 parole confrontate) all'heapsort 0x5fb66c usato per i job, che con
  `b.prio - a.prio` dà priorità decrescente → con `a.id - b.id` dà **id crescente** [VERIFICATO].
- `ls+0x1bc == 1` è la stessa condizione richiesta da 0x667358 per costruire i job (0x667398): se i job esistono
  (0x683ecc ha un solo chiamante, 0x680f94, e il log 6m lo mostra eseguito) l'ordinamento è attivo [VERIFICATO
  staticamente; deduzione per il runtime].
- Ordine risultante in gara (8 viste, slot attuali):
  Begin(0) → GPUCalc(1..22) → 3D Sub0..3 (2..5) → 2D Sub0..3 (6..9) → **2D Main (10)** → 2D Front (12) →
  2D Overlay (13) → **3D Sub4..7 (14..17)** → 2D Sub4..7 (18..21) → End(0x201ff).
  3D Back (0), 2D Back (1) e 3D Extra (11) sono spenti (`fl78` 4c0/4c2, log run11/6m).
- [IPOTESI forte] la sottomissione 0x66771c avviene nello stesso frame dopo 0x66767c (chiamanti 0x705c24/0x705f08,
  non ricostruiti per intero). Non cambia la conclusione: l'ordine sottomesso è quello ordinato.

## 2. R e target (punti a e b del brief)
- a) Disegno di una vista di R: tutto dentro il job Draw del Layer3D: CalcView/step → FinalImage 0x68d2c4 →
  0x6af7f8(R, ctx, vista) → 0x6b55a8(R+0x3fd0, ctx, voce vista R+0xf8+v*0x3e0, R+0x39c8, flag)
  (`d_finalimage.asm`): voce per vista `[[R+0x3fd0+0x248]+0x10] + v*0x3d68` (csel sulla voce 0 se `v >= [..+0x8]`,
  0x6b55e4-0x6b55ec), bind del target `voce+0x30` (0x61a498), viewport `voce+0x10` (0x61e104), stato 0x61a824,
  disegno di copia 0x64cf48 dalla texture `voce+0xd78`; poi 0x6f7a6c e 0xafee4c(R+0x3fd0+0x250, ...). È un passo
  degli step di rendering, quindi nella banda di id del layer [VERIFICATO]. Nessun passo "dopo il 2D" trovato.
- b) Target del 2D Main: stesso `info+0x10 = display+0x10` dei 3D (plan C §3) [VERIFICATO]. Depth/stencil del 2D
  non analizzati: non necessari, la banda di id basta a spiegare l'effetto [IPOTESI esclusa per parsimonia].
- Resta aperto (non legato al 2D): `[[R+0x3fd0+0x248]+0x8]` (voci per vista dei buffer di FinalImage). Se vale 4,
  le viste 4..7 usano la voce 0 (stessi buffer di Sub0). Sulla GPU le bande sono sequenziali e ogni vista fa
  render+copia nella propria banda, quindi non c'è conflitto di ordine; da loggare comunque (E2).

## 3. Esperimenti (in ordine di probabilità/costo)

### E1 – Rimappatura degli id di banda (correzione) [sito VERIFICATO, effetto IPOTESI forte]
Siti:
| offset | parola originale | prima istruzione | firma | stato |
|---|---|---|---|---|
| 0x683ecc | `d10283ff` | `sub sp, sp, #0xa0` | `void Draw(uint8_t* display, void* ctx, uint8_t* layer, int id)` | già agganciato (LayerDrawJobHook) |
| 0x684148 | `b94cb008` | `ldr w8, [x0, #0xcb0]` | `void SubDraw(uint8_t* display, void* ctx, uint8_t* layer, int id)` | nuovo, opzionale (no-op senza 0x840) |
Rango: slot 0..5 → 0..5; 3D extra 14..17 → 6..9; 2D Sub 6..9 → 10..13; 2D extra 18..21 → 14..17;
2D Main/3D Extra/Front/Overlay 10..13 → 18..21; slot ≥ 22 invariati (biiezione su 0..21, nessuna collisione).
Id massimo: `31*258 + 0xffff + 257 = 73790 < 0x201ff` (End resta ultimo) [VERIFICATO aritmetica].
```cpp
/* Step 6b E1: l'ordine GPU dei layer = id crescente dei segmenti (0x684a64), id = slot*258+0xffff (0x680f8c).
   Rango: i layer extra subito dopo i corrispondenti del gioco (step6b_plan.md §3). */
#ifndef MK8DX_SLOT_REMAP
#define MK8DX_SLOT_REMAP 1
#endif
#define MK8DX_DRAW_IN_SUB0 0            /* 6C non serve più: ogni extra torna nel proprio job */
namespace order8 {
    static int Rank(int slot) {
        if (slot >= 0 && slot <= 5)   return slot;               /* 3D Back, 2D Back, 3D Sub0..3 */
        if (slot >= 14 && slot <= 17) return 6 + (slot - 14);   /* 3D Sub4..7 */
        if (slot >= 6 && slot <= 9)   return 10 + (slot - 6);    /* 2D Sub0..3 */
        if (slot >= 18 && slot <= 21) return 14 + (slot - 18);   /* 2D Sub4..7 */
        if (slot >= 10 && slot <= 13) return 18 + (slot - 10);   /* 2D Main, 3D Extra, 2D Front, 2D Overlay */
        return slot;                                             /* 22..31 invariati */
    }
    static int Id(const uint8_t* ly, int id) {
        if (!MK8DX_SLOT_REMAP || !proto::active || !ly) return id;
        int slot = *(const int32_t*)(ly + 0x84);
        if (id != slot * 258 + 0xffff) return id;                /* solo l'id standard del job */
        return Rank(slot) * 258 + 0xffff;
    }
}
HOOK_DEFINE_TRAMPOLINE(LayerDrawJobHook) {               /* 0x683ecc(display, ctx, layer, id) */
    static void Callback(uint8_t* disp, void* ctx, uint8_t* ly, int id) {
        static unsigned long n;
        int nid = order8::Id(ly, id);
        if (nid != id && (n++ % 3600) == 0)
            Logging.Log("[mk8dx-8p] ordine: slot %d id %d -> %d", *(int32_t*)(ly + 0x84), id, nid);
        Orig(disp, ctx, ly, nid);
    }
};
HOOK_DEFINE_TRAMPOLINE(LayerSubDrawJobHook) {            /* 0x684148(display, ctx, layer, id) */
    static void Callback(uint8_t* disp, void* ctx, uint8_t* ly, int id) { Orig(disp, ctx, ly, order8::Id(ly, id)); }
};
// exl_main:
//   if (*reinterpret_cast<uint32_t*>(base + 0x683ecc) == 0xd10283ff) LayerDrawJobHook::InstallAtOffset(0x683ecc);
//   if (*reinterpret_cast<uint32_t*>(base + 0x684148) == 0xb94cb008) LayerSubDrawJobHook::InstallAtOffset(0x684148);
```
Variante minima E1' (solo i 3D extra, 2D extra invariati): `Id()` restituisce `0xffff - (4 - j) * 258` per
`ly == proto::layer[j]` (bande 64503..65534: dopo GPUCalc ≤ 32 e prima dello slot 0; 3D Back/2D Back spenti in gara).
- Segnale atteso in Eden: minimappa e 3-2-1 interi sopra le celle 5-8; HUD 5-8 ancora visibile (2D Sub4..7 dopo i 3D
  extra); in pausa/risultati gli extra non stanno più sopra l'immagine del 2D Overlay anche senza il bit 0x2
  (finiscono nella cattura del 2D Front come i Sub). Con E1 (non E1') anche l'HUD 5-8 non passa più sopra
  pausa/risultati. Log `ordine: slot 14 id 69147 -> 67083`.
- Rischio: basso. L'id entra solo in `seg+0x160` (0x6850c4) e nel confronto 0x684a64 [VERIFICATO in 0x684fa4];
  ogni Draw ri-imposta target e viewport all'inizio (0x667d10, 0x61e104, 0x68ca44 → 0x61e344), quindi spostare
  un'intera banda è sicuro come lo è l'alternanza 3D/2D vanilla. Se `proto::active` cambia a metà frame, un solo
  frame ha l'ordine misto. Nessun effetto senza 5-8 locali (gate su `proto::active`).
- Se E1 non cambia nulla: la lista non viene ordinata (ls+0x1bc ≠ 1 o sottomissione diversa) → E0.

### E0 – Log dei segmenti sottomessi (diagnostica decisiva, una build con E1 spento e acceso)
Sito: **0x6845f8** `a9bb67fc` (`stp x28, x25, [sp, #-0x50]!`), `void Submit(uint8_t* display, void* cmd, uint32_t flag)`.
A questo punto `display+0xfd0` è già ordinata (0x66767c gira prima, §1.3).
```cpp
namespace order8 { static volatile int dumpReq; }      /* PerFrame: if (frames == 600) order8::dumpReq = 1; (e al 1° frame di pausa) */
HOOK_DEFINE_TRAMPOLINE(SegSubmitHook) {                /* 0x6845f8(display, cmd, flag) */
    static void Callback(uint8_t* disp, void* cmd, uint32_t flag) {
        if (order8::dumpReq > 0 && disp) {
            order8::dumpReq--;
            int n = *(int32_t*)(disp + 0xfc8);
            uint8_t** seg = *(uint8_t***)(disp + 0xfd0);
            uint8_t* D = *(uint8_t**)(disp + 0xc60);
            Logging.Log("[mk8dx-8p] seg: display=%p n=%d modo=%d D.seq=%d D.cap=%d ls1bc=%d", disp, n, disp[0x9],
                        D ? *(int32_t*)(D + 0x80) : -1, D ? *(int32_t*)(D + 0x100) : -1, proto::ls ? proto::ls[0x1bc] : -1);
            for (int i = 0; seg && i < n && i < 96; i++) {
                uint8_t* s = seg[i]; if (!s) continue;
                uint8_t* ly = *(uint8_t**)(s + 0x168);
                Logging.Log("[mk8dx-8p] seg[%d] id=%d slot=%d core=%d pieno=%d dt=%lu", i, *(int32_t*)(s + 0x160),
                            ly ? *(int32_t*)(ly + 0x84) : -1, s[0x180], s[0x24] & 1,
                            (unsigned long)(*(uint64_t*)(s + 0x178) - *(uint64_t*)(s + 0x170)));
            }
        }
        Orig(disp, cmd, flag);
    }
};
// exl_main: if (*reinterpret_cast<uint32_t*>(base + 0x6845f8) == 0xa9bb67fc) SegSubmitHook::InstallAtOffset(0x6845f8);
```
- Atteso senza E1: id crescenti, slot 2..5, 6..9, 10, 12, 13, poi 14..17, 18..21. Con E1: 14..17 subito dopo 5.
- Campi: `+0x160/+0x168/+0x170/+0x178/+0x180/+0x24` [VERIFICATO in 0x684fa4/0x685110/0x6838ec];
  `D+0x80` contatore di sequenza e `D+0x100` capacità [VERIFICATO l'uso; azzeramento per frame IPOTESI]: se
  `D.seq` supera `D.cap` i segmenti in più ricadono sulla voce 0 (csel 0x685040) → corruzione; con 8 layer in più
  conviene controllarlo una volta.
- Nota: non agganciare 0x684590 (prima istruzione `adrp`, parola 90006441).

### E2 – Log L2 dei job di un frame (richiesto; ora secondario)
Sito: **0x680f2c** `a9bd57fc` (`stp x28, x21, [sp, #-0x30]!`), `void JobExec(uint8_t* job)` (vtbl[0] dei job,
vtable 0x12b4c10). Campi job [VERIFICATO 0x680e30/0x680eec/0x680f44-0x680f5c]: `+0x18` tipo (0 Draw, 1 SubDraw,
2 Begin, 3 End, 4 GPUCalc), `+0x20` layer (nullo per Begin/End), `+0x28` display, `+0x30` priorità.
```cpp
namespace order8 {
    struct JobRec { uint64_t t0, t1; uint8_t* ly; int32_t type, prio; uint32_t core; };
    static JobRec rec[256]; static int recN; static volatile bool recOn;
    /* PerFrame (thread principale): if (frames == 600) { recN = 0; recOn = true; } else if (recOn) { recOn = false; Dump(); } */
    static void Dump() {
        int n = recN < 256 ? recN : 256;
        uint64_t b = n ? rec[0].t0 : 0;
        for (int i = 0; i < n; i++) if (rec[i].t0 < b) b = rec[i].t0;
        Logging.Log("[mk8dx-8p] job: %d job registrati", recN);
        for (int i = 0; i < n; i++)
            Logging.Log("[mk8dx-8p] job[%d] tipo=%d slot=%d prio=%d core=%u t0=%lu t1=%lu", i, rec[i].type,
                        rec[i].ly ? *(int32_t*)(rec[i].ly + 0x84) : -1, rec[i].prio, rec[i].core,
                        (unsigned long)(rec[i].t0 - b), (unsigned long)(rec[i].t1 - b));
    }
}
HOOK_DEFINE_TRAMPOLINE(JobExecHook) {                  /* 0x680f2c(job) */
    static void Callback(uint8_t* job) {
        if (!order8::recOn) { Orig(job); return; }
        uint64_t t0 = svcGetSystemTick();
        Orig(job);
        int i = __atomic_fetch_add(&order8::recN, 1, __ATOMIC_RELAXED);
        if (i < 256) {
            order8::JobRec& r = order8::rec[i];
            r.t0 = t0; r.t1 = svcGetSystemTick(); r.ly = *(uint8_t**)(job + 0x20);
            r.type = *(int32_t*)(job + 0x18); r.prio = *(int32_t*)(job + 0x30); r.core = svcGetCurrentProcessorNumber();
        }
    }
};
// exl_main: if (*reinterpret_cast<uint32_t*>(base + 0x680f2c) == 0xa9bd57fc) JobExecHook::InstallAtOffset(0x680f2c);
```
- Atteso: l'ordine/core dei job NON coincide con l'ordine dei segmenti di E0 (conferma che conta solo l'id).
- Rischio: basso (solo lettura; `svcGetCurrentProcessorNumber` è in `exlaunch/source/lib/nx/kernel/svc.h`).
  La finestra "tra due PerFrame" copre circa una fase di disegno [IPOTESI].

### E3 – Solo se E1/E0 smentiscono l'ordinamento per id
Allora riprendere a)/b): log di `[[R+0x3fd0+0x248]+0x8]`, flag di stato di 0x6b5990 (`sp+0x8c = 0x1000000`,
probabile stato depth/blend della copia) e un hook su 0x64cf48 per vedere la copia delle viste 4..7. Non proposto ora.

## 4. Da togliere / adattare in main.cpp (quando si applica E1)
- `MK8DX_DRAW_IN_SUB0` → 0 (il ramo che chiama `Orig` per gli extra dopo Sub0 usa l'id dello slot 14..17 e,
  con E1 attivo, andrebbe comunque rimappato; più semplice lasciare a ogni extra il proprio job).
- Layer3DPrioHook e MK8DX_GROUPFIX: ininfluenti sull'ordine GPU; il primo si può tenere (stesso bucket dei Sub).
- La copia del bit 0x2 di +0xde4 da Sub0 resta utile (in cattura il gioco nasconde i 3D: coerenza).

## 5. Incertezze residue
- Chi chiama 0x66771c (0x705c24/0x705f08) e in che punto del frame: dedotto, non letto per intero.
- `D+0x80`/`D+0x100` (sequenza/capacità segmenti) a runtime con 8 layer in più (E0).
- `[[R+0x3fd0+0x248]+0x8]` (buffer di FinalImage per vista) per le viste 4..7: possibile condivisione con la voce 0.
- Effetto di `display+0x9` (modo di sottomissione 0/1/2): l'ordine della lista vale in tutti e tre i rami
  [VERIFICATO 0x6846b0-0x6847d0].
