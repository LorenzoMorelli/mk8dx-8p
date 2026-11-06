# Step 6b — Lakitu e semaforo nelle celle 5-8 (piano H6b-L3)

Analisi statica del main 4.0.1 (offset = file decompresso). Estratti asm: `step6b_data/l_*.asm`, vtable: `l_vtables.txt`.
Completa `step6b_plan_hud.md` §2.3 (H6b-L1 0x10f198, H6b-L2 0x7e9b04, già nel codice).

## 0. Verdetto
**Fattibile, rischio medio-basso.** Il numero di Lakitu non è una costante: è il numero di viewport della Scene con
kart locale (`Scene+0x3c+i*0x18`, solo **i = 0..3**, codice srotolato), e l'array `mgr+0x40` è allocato su misura
(`count*8`, nessuno spazio libero). Tutto il resto del ciclo di vita del gestore (init, reset, calc, messaggi) itera
`count` in modo generico. Il punto più sicuro è un **trampolino su 0x7e9d40** (la funzione che crea i Lakitu): dopo `Orig`
si rialloca l'array a `count + extra`, si creano i Lakitu mancanti con la fabbrica del gioco (vt+0xc0 del gestore) e si
inizializzano i due campi del sotto-oggetto, esattamente come il codice originale. Init del modello, risorse, reset e
SetSlot vengono poi eseguiti **dal gioco stesso**, perché 0x7e9d40 è chiamata come primo passo dell'init del gestore
(0x1106f8) che subito dopo itera tutti i Lakitu. Nessun array interno di dimensione 4 viene sforato con slot 4..7 (le
viste del modello sono già 8 grazie a ModelCreateHook).

## 1. Dove nascono i Lakitu e perché sono 4 [VERIFICATO] — `l_create_7e9d40.asm`, `l_mgr_ctor.asm`, `l_vtables.txt`

Gestore (0x88 B):
```
0x398e58  alloc 0x88 (0x5faae0), 0x398e70 ctor base 0x7e9970 (+0x38 = 0, +0x40 = 0), vtable [0x1306548]+0x10 = 0x11b99a8,
          0x398e98 +0x30 = owner+0x1a8, 0x398ed8 owner+0x248 = mgr        (getter: 0x7e741c() = [[..+0x28]]+0x248)
vtable derivata 0x11b99a8: +0x20 0x1106f8 init | +0x28 0x110764 reset | +0x38 0x1107c4 | +0x40 0x11082c | +0x50 0x110890 msg
                           +0x70 0x1101dc CreateObjects | +0xc0 0x110298 fabbrica Lakitu   (base 0x12c90b0: +0x70 0x7e9d40, +0xc0 = 0)
campi: +0x30 ptr (copiato in ogni Lakitu), +0x38 s32 count, +0x40 ptr[count], +0x4c s32 indice di riempimento
```
Sequenza di init (0x1106f8 = vt+0x20, `l_mgr_loops.asm`):
```
vt+0x70 = 0x1101dc:  bl 0x7e9d40 (crea i Lakitu)  ; se count>=1 carica "JugemObj_FX/JugemObj_FX.bfres" (una volta, condiviso)
per i < count:       (lak+0xf0)->vt+0x60  = 0x7a911c -> sub vt+0x20 = 0x7e8538 (init per Lakitu, §3)
vt+0x78 (ret)
reset 0x110764 (vt+0x28): per i < count (lak+0xf0)->vt+0x68 -> 0x7e861c -> Lakitu vt+0xc8, poi SetSlot(vt+0xb8, +0x168)
```
**0x7e9d40 (CreateLakitus, unico chiamante 0x1101f0)**:
```
0x7e9d58  se (settings+8 | 2) == 7 → esce (modi 5/7, nessun Lakitu)
0x7e9d6c  Scene = 0x8ace60(); u16 desc[4] su stack (sp+8, 8 byte!), n = 0
          per vp 0..3 SROTOLATO (Scene +0x3c/+0x54/+0x6c/+0x84 = slot, +0x40.. attivo, +0x44.. tipo, +0x4c.. kart):
            se (attivo|2)==3 && tipo==1 && kart != -1:  desc[n++] = kart | (attivo ? slot : 0xff) << 8
            (0x7e9e58/0x7e9eb4: `cmp w11,#4` + csel → indice clamp a 0 oltre 4)
0x7e9ee8  arr = 0x5fad70(n*8, heap=0 (corrente), align 0x40); +0x38 = n; +0x40 = arr; arr[0..n-1] = 0
0x7e9f88  +0x4c = 0; per i < n: lak = vt+0xc0(mgr, &desc[i])        ← fabbrica 0x110298
          (lak+0xf0)+0x28 = mgr ; (lak+0xf0)+0x30 = mgr+0x30 ; arr[+0x4c++] = lak   (nessun controllo NULL: lak+0xf0 se NULL → 0)
```
Quindi **count = numero di vp 0..3 con kart locale** (max 4 per costruzione: 4 voci srotolate, desc[4]), non una costante
né il numero di giocatori. Con la mod la Scene è riscritta "4P coerente" (SceneInitRaceHook) → count 4, coerente con il log
(count = 4, 0x7e9a68 NULL per i kart delle viste 4..7). Capacità di `+0x40`: **esattamente count** (count*8 byte, heap
corrente); nessun distruttore libera `+0x40` né i Lakitu (0x7ea01c/0x1106d4 chiamano solo 0x7a9100 e liberano il gestore
→ tutto vive nell'heap di gara) [VERIFICATO dtor; "heap di gara distrutto in blocco" IPOTESI].

Fabbrica 0x110298(mgr, desc*) (`l_factory.asm`): `0x5fa960(0x3d0)` (new sull'heap corrente), ctor 0x7e80ec(obj, desc):
`+0x169 = desc[0]` (kart, 0x7e817c), `+0x168 = desc[1]` (slot, 0x7e8174); poi vtable [0x12fc000] (= 0x11b9768),
sotto-vtable +0xf0 / +0x1a0, inizializzazione campi. Il kart del Lakitu viene solo da qui.

## 2. Assegnazione di slot/kart dopo la creazione [VERIFICATO]
- 0x7e9b04 (da 0x86cb5c): reset vt+0xb0 su tutti, poi per vp 0..3 (`cmp x25,#4` a 0x7e9c0c) SetSlot(vp.slot) al Lakitu
  con `+0x169 == vp.kart`. Già esteso da H6b-L2 (LakituAssignHook) alle viste 4..7.
- 0x7e9c44 (da 0x3d7784): stessa cosa senza reset, solo vp 0..3 → non tocca gli extra (nessuna correzione necessaria).
- 0x7e9a68 FindLakituByKart (indice `sxtb`, ok fino a 127) — usata anche da 0x144c10/0x144eb0/0x145104 (oggetti per
  kart che memorizzano il loro Lakitu in +0x58; NULL per i CPU): con gli extra i kart 4..7 trovano il proprio Lakitu
  (comportamento del 4P per tutti gli 8) [VERIFICATO sito, effetto IPOTESI: ripescaggio/bandiere per i kart 5-8].

## 3. Inizializzazione per Lakitu (tutta automatica se si crea dentro 0x7e9d40) [VERIFICATO] — `l_init.asm`
```
sub vt+0x20 = 0x7e8538: oggetto 0x170 (0x5fa960) 0x7ea124(obj, kart=+0x169), lak+0x160 = obj, obj+0x16c = +0x17c,
                        0x7ea6ec(obj); poi Lakitu vt+0xc0 = 0x10c27c
0x10c27c: risorsa "JugemObj/JugemObj.bfres" (0x8805f4), modello 0x8807a8 → lak+0x8 (viste: 4 fisso a 0x8807d4, ESTESO a
          MK8DX_VIEWS da ModelCreateHook 0x69bb0c, sempre attivo), shape JugemObjRod/Flag/Line/Lap/Reverse/Signal
          (+0x1f0..+0x218, indici +0x220..+0x234: il SEMAFORO è la shape "JugemObjSignal" dello stesso modello),
          animazioni 0x857e34(lak,4,4) (4 slot di animazione, non viste [IPOTESI]), +0x1d0 = kart obj (clamp kart < 12),
          +0x1d8 0x7ec97c, +0x1e0 0x7ec228, vt+0x10("JugemObj"), vt+0x80
0x10cf80: +0x1e8 = raceSys[4]+0x250 → +0x750 (pool di oggetti condiviso), emettitori +0x2e8/+0x330 (0x8c8780)
sub vt+0x28 = 0x7e861c (reset): stato, Lakitu vt+0xc8 (0x10c834), SetSlot(+0x168)
```
Nessuna registrazione esterna per-Lakitu oltre all'array del gestore: calc/draw passano dai loop generici del gestore
(0x1107c4 → 0x7a914c, 0x11082c → 0x7a9158, 0x110890 messaggi) e dal modello (lak+8) [VERIFICATO per i loop del gestore;
la registrazione del modello nella scena avviene in 0x8807a8/0x10c27c come per i 4 originali].

## 4. Siti che usano lo slot (+0x168) con slot 4..7 [VERIFICATO] — `l_view_sites.asm`
| sito | uso | con slot 4..7 |
|---|---|---|
| 0x10f120 SetSlot: 0x68fb7c(model, 1<<slot) | u16 maschera viste per sotto-modello (+0xc) | ok (bit 4..7 in u16) |
| 0x10f168-0x10f190 effetti `[lak+0xb0]+0x120`: +0xfa = slot<=3 ? 0x16+slot : 0, +0xf8 = slot<=3 ? 1<<slot : 0xffff | gruppo/maschera effetti | ripiego del gioco: effetti del Lakitu visibili in **tutte** le viste (anche 1-4) — innocuo, vedi §6 |
| 0x10f198 gate `slot>=4 → nascondi` | | già patchato (H6b-L1) |
| 0x10d20c/0x10f5c8/0x10f920 0x690c6c(model,on,slot) → 0x6dc7d8 → 0x6dc1dc | indice `viste*i + slot` su array `+0x58` (0xc B) | ok se slot < viste del modello (= MK8DX_VIEWS 6/8): **slot massimo deve essere < MK8DX_VIEWS** |
| 0x10d0a0 vt+0xd0 per frame | Scene+0xb0[slot<4 ? slot : 0] → vp → pila director | clamp: gli extra seguono la vista di P1 (nascosti quando la testa di P1 è tipo nodo 2) |
| 0x10fdcc 0x2684a4(pool+0x1e8, pos, slot) → oggetto del pool +0x248 = slot; 0x2673bc visibilità **srotolata 0..4** | oggetto lanciato dal Lakitu | slot 4 visibile solo in vista 4; slot 5..7 invisibile ovunque; **nessuno sforamento** |
| 0x7e928c +0x16e = 300+slot | id frame d'uscita | innocuo |
| 0x7e8608/0x7e86a0 | ri-applicano SetSlot | ok |
Letture di +0x169 (kart): 0x10c790 (clamp <12), 0x10cfec (+0x68[kart] sulla lista kart a 12), 0x7e83ac (0x86e8e4, bound
+0x50), 0x7e89cc/0x7e8aa0/0x10fa20/0x10fe18 (raceSys clamp), 0x7e8a5c/0x7e8d4c (csel hi) → tutte limitate ai 12 kart.

## 5. Piano di hook

### H6b-L3 TRAMPOLINE 0x7e9d40 (prima istruzione `0xd10103ff` `sub sp,sp,#0x40`) [VERIFICATO sito/parola]
Dopo `Orig`: se split, per ogni locale 4..split-1 senza Lakitu crea un Lakitu con desc = {kart, slot 4+j}. Le chiamate
della fabbrica avvengono nello stesso contesto (heap corrente) dei 4 originali; poi 0x1106f8 fa init+reset su tutti.
```cpp
/* 6b H6b-L3 (step6b_plan_lakitu.md): i Lakitu sono creati in 0x7e9d40 (vt+0x70 del gestore, chiamata da 0x1101f0) solo
   per i vp 0..3 della Scene, array mgr+0x40 di esattamente count elementi. Si rialloca e si accodano i Lakitu dei locali 4..7. */
namespace lak8 { static int created, lastCount; }
HOOK_DEFINE_TRAMPOLINE(LakituCreateHook) {                 /* 0x7e9d40(mgr), 0xd10103ff */
    static void Callback(uint8_t* mgr) {
        Orig(mgr);
        lak8::created = 0;
        int n0 = *(int32_t*)(mgr + 0x38);
        uint8_t** old = *(uint8_t***)(mgr + 0x40);
        Logging.Log("[mk8dx-8p] L3: CreateLakitus count=%d split=%d", n0, view8::split);
        if (!view8::split || n0 < 1 || n0 > 4 || !old) return;          /* split==0 qui = troppo presto: vedi nota tempi */
        uint16_t desc[4]; int ne = 0;
        for (int j = 0; j < view8::split - 4 && j < 4; j++) {
            const view8::Vp& v = hud8::extraVp[j];                       /* {idx, slot=4+j, attivo, tipo, loc, kart} */
            if ((v.active | 2) != 3 || v.type != 1 || v.kart < 0 || v.kart >= 12) continue;   /* = condizione di 0x7e9d74.. */
            if (4 + j >= proto::Views) continue;                         /* 0x6dc1dc: slot < viste del modello */
            if (mk8dx::Fn<uint8_t* (*)(uint8_t*, int)>(0x7e9a68)(mgr, v.kart)) continue;      /* già presente */
            desc[ne++] = (uint16_t)((v.kart & 0xff) | ((4 + j) << 8));    /* byte0 → +0x169 kart, byte1 → +0x168 slot */
        }
        if (!ne) return;
        uint8_t** arr = (uint8_t**)mk8dx::Fn<void* (*)(size_t, void*, int)>(0x5fad70)((size_t)(n0 + ne) * 8, nullptr, 0x40);
        if (!arr) { Logging.Log("[mk8dx-8p] L3: alloc array fallita"); return; }
        for (int i = 0; i < n0; i++) arr[i] = old[i];
        for (int i = n0; i < n0 + ne; i++) arr[i] = nullptr;
        auto create = reinterpret_cast<uint8_t* (*)(uint8_t*, uint16_t*)>((*(uintptr_t**)mgr)[0xc0 / 8]);   /* 0x110298 */
        int n = n0;
        for (int k = 0; k < ne; k++) {
            uint8_t* lak = create(mgr, &desc[k]);
            if (!lak) continue;                                           /* (il gioco non lo controlla: 0x5fa960 non fallisce) */
            *(uint8_t**)(lak + 0xf0 + 0x28) = mgr;                         /* 0x7e9fbc */
            *(uint64_t*)(lak + 0xf0 + 0x30) = *(uint64_t*)(mgr + 0x30);    /* 0x7e9fc0 */
            arr[n++] = lak;
        }
        *(uint8_t***)(mgr + 0x40) = arr;                                  /* il vecchio array resta nell'heap (come il gioco, mai liberato) */
        *(int32_t*)(mgr + 0x38) = n;
        *(int32_t*)(mgr + 0x4c) = n;
        lak8::created = n - n0; lak8::lastCount = n;
        Logging.Log("[mk8dx-8p] L3: Lakitu %d -> %d (extra %d)", n0, n, n - n0);
    }
};
/* install, nello stesso blocco di H6b-L1/L2:
   if (*(uint32_t*)(base + 0x7e9d40) == 0xd10103ff && *(uint32_t*)(base + 0x1101f0) == 0x941b66d4
       && *(uint32_t*)(base + 0x110298) == 0xf81d0ff5) LakituCreateHook::InstallAtOffset(0x7e9d40); */
```
Parole originali lette dall'asm: 0x7e9d40 `d10103ff`, 0x1101f0 `941b66d4` (`bl 0x7e9d40`), 0x110298 `f81d0ff5`,
0x7e9ef8 `f9002260` (`str x0,[x19,#0x40]`), 0x7e9fbc `f9001513`, 0x7e9fc0 `f9401a69`.

Nota tempi [IPOTESI forte]: kartOf/extraVp sono scritti in SceneInitRaceHook (0x7e76b8); 0x7e9d40 legge la Scene già
riscritta (count 4 dal log), quindi gira dopo. Il log `L3: CreateLakitus count=.. split=..` lo conferma: se compare
`split=0` con 8 giocatori, calcolare extraVp lì con la stessa formula di SceneInitRaceHook (GetSettingsA 0x86f7bc,
GetLocalPlayerCount 0x86f154, 0x86f64c, 0x86e8e4) invece di saltare.

Alternativa scartata: creare i Lakitu più tardi in LakituAssignHook (0x7e9b04, inizio gara) chiamando a mano sub vt+0x60
(0x7e8538) e vt+0x68 (0x7e861c) — entrambe ignorano x1 [VERIFICATO] — ma allocazioni/caricamento del modello avverrebbero
con un heap corrente diverso e a gara avviata [IPOTESI di rischio]: usarla solo se il log mostra split=0.

### H6b-L1 / H6b-L2 (già nel codice) — restano necessari
- L1 0x10f198 (`0x7100111f`): senza, SetSlot(4..7) nasconde il Lakitu.
- L2 0x7e9b04 (`0xa9ba6ffc`): 0x7e9b04 resetta (vt+0xb0) tutti e assegna solo vp 0..3; L2 riapplica SetSlot(4+j). Con L3 i
  Lakitu extra nascono già con slot 4+j (desc), e il reset 0x110764 chiama già SetSlot(+0x168): L2 resta come rinforzo
  dopo ogni 0x7e9b04 [VERIFICATO].

### (opz.) H6b-L4 effetti solo nella propria vista [IPOTESI]
Il ripiego del gioco (gruppo 0, maschera 0xffff) mostra gli effetti del Lakitu extra anche nelle viste 1-4 (solo se il
Lakitu è nell'inquadratura: è vicino al kart 5-8). Se dà fastidio: INLINE su 0x10f190 (`strh w8,[x9,#0xf8]`, 0x7901f128)
con `if (split && W21 in 4..7) W8 = 1 << W21` — da provare solo dopo aver visto se il sistema effetti disegna le viste 4..7
con maschera per bit (0x16+slot = gruppi per vista 0..3 che per 4..7 probabilmente non esistono).

## 6. Rischi
1. **Memoria dell'heap di gara** [IPOTESI]: +4 × (0x3d0 + 0x170 + modello JugemObj a 8 viste + emettitori). 0x5fa960 non
   controlla il fallimento (ctor su NULL → crash in 0x7e80ec). Mitigazione: provare prima in Eden con 8P sul circuito più
   pesante; il log L3 stampa prima della creazione.
2. **Effetti** in tutte le viste per gli extra (§4, innocuo) [VERIFICATO meccanismo].
3. **Visibilità per frame** (0x10d0a0) legata alla vista di P1 (clamp): gli extra si nascondono/mostrano insieme alla
   camera di P1 quando la testa del director è tipo nodo 2 [VERIFICATO meccanismo, effetto IPOTESI].
4. **Oggetto del pool** (0x10fdcc → 0x2673bc srotolato 0..4): per slot 5..7 invisibile, slot 4 corretto; nessuno sforamento
   [VERIFICATO]. Correzione eventuale: hook su 0x2673bc che per +0x248 in 5..7 chiama 0x7fa9b4(obj+0x1b0, +0x1b8, v, v==slot)
   anche per v = 5..7 (da verificare la capacità per vista di 0x7fa9b4 prima).
5. **Audio/voce del Lakitu** (conto alla rovescia) moltiplicata per 8 [IPOTESI]: nessuna lettura di +0x168 nel codice audio
   trovata; il suono va per kart/ascoltatore.
6. **MK8DX_VIEWS = 6**: con split 6 gli slot sono 4..5 < 6, ok; lo sketch salta slot >= proto::Views.
7. 1-4 giocatori: tutto inerte (`view8::split == 0`).

## 7. Verifica in Eden
Log attesi: `L3: CreateLakitus count=4 split=8` → `L3: Lakitu 4 -> 8 (extra 4)`; poi `L: Lakitu del kart K = 0x... (count 8)`
(H6b-L2) per le viste 4..7. Dal vivo: `mgr = 0x7e741c()`; `[mgr+0x38] == 8`; per i 4 nuovi `+0x168 = 4..7`, `+0x169 = kartOf[4..7]`,
`[lak+8]` modello con `+0xe0 = 8` viste. Visivo: Lakitu con semaforo nelle celle 5-8 durante l'intro/partenza.
