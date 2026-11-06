# Step 6d – Causa dei flash in gara a 8 (analisi statica + piano di misure, main 4.0.1)

Fonti: `../img/main.ann.asm`, `../img/main.bin`, `step6c_flash.md` (§1-7), `step6b_plan.md` (§1 segmenti),
`step2_crash8v.md`, `step2_draworder.md`, `STATE.md`, `STRUCTS.md`, `src/program/main.cpp` (sola lettura).
Verifica: `step6d_data (dati non pubblicati)/root_verify.py` -> `root_verify.txt`: **118 OK, 0 FAIL** (93 della prima stesura + 25 di §7).
**Aggiornamento 4 nov sera: B1 escluso da M1, C escluso da T2. Nuova pista principale in §7.**
Altri script: `step6d_data/xrefs.py` (adrp+add -> indirizzo), `step6d_data/nvnslots.py` -> `nvn_slots.txt`
(nome NVN -> slot GOT dal caricatore 0x582c94; coincide con gli slot già usati dalla mod 0x130b2c0/2c8/2d8).
Etichette: [V] verificato (statico o log), [I] ipotesi/inferenza, [D] dato dell'utente.

## 0. Sintesi
1. [V] Il gioco registra i comandi GPU di **tutti i layer** in segmenti che prendono memoria da un **budget fisso per core e per
   frame** del layerSystem (D = ls+0x80; 3 core, 2 buffer, dimensioni `cfg+0x38/+0x40` lette a 0x704ae8/0x704af0).
2. [V] Il budget non ha protezione: ogni segmento riceve `min(dimensione - usato, 4 MiB)` **con segno** (0x68505c/0x685078).
   Quando il budget finisce, `AddCommandMemory` riceve almeno 0x200 byte (0x62dd9c), e una dimensione negativa diventa un numero
   enorme senza segno (0x62dda4). La callback NVN di memoria esaurita è `ret` (GOT 0x130c610 = 0x62ddf4).
3. [V] Il gioco tiene già il **picco** di uso per core: D+0x118 per i comandi e D+0x120 per il controllo, aggiornati a 0x684ef0.
   Misurarlo costa una lettura per frame.
4. [I] Nel gioco originale la gara non registra mai più di 2 viste 3D per frame: 1P e 2P a pieno ritmo, il 4P a frame alterni.
   Con 4, 6 o 8 viste per frame il budget per core può non bastare. A quel punto i segmenti finali di un core scrivono oltre il
   loro blocco: nel buffer dell'altro frame dello stesso core (forse ancora in lettura dalla GPU di Eden), oppure nel blocco del
   core successivo, che contiene i comandi registrati prima nello stesso frame (§2.3).
5. [I] La firma osservata corrisponde. Il difetto cresce con le viste per frame (S1 > S0 > S2 > 4P = mai). "Vince l'ultimo che
   scrive": le ultime viste sovrascrivono i comandi delle prime, quindi in S1 si salva solo la vista 7. È intermittente, perché
   dipende dalla parità del frame e dal contenuto. Appare all'apertura della pausa, che aggiunge layer e catture in quel frame.
   Dipende dalla disposizione in memoria, perché la vittima dello sconfinamento dell'ultimo blocco (oltre il pool comandi) e della
   memoria di controllo (oltre il blocco CPU nell'heap) è l'allocazione successiva.
6. Piste in ordine: **B1** sconfinamento del budget comandi/controllo per core (~45%); A2 ricreazione/aliasing dei temporanei del
   pool texture con la cache texture di Eden (~15%); C sincronizzazione CPU/GPU dell'emulatore esposta dal carico (~10%); B1'
   due thread con lo stesso indice core che aprono segmenti sovrapposti (~8%); D2 lista per frame R+0x4d00 senza limite (~5%);
   D1 scritture fuori limite della mod (~5%); altro (~12%).
7. [V] **Escluse staticamente:** (i) blocchi GPU dei modelli a 8 viste: la dimensione è calcolata dopo `Initialize`, con
   ViewCount già 8 (0x69bc5c); (ii) esaurimento del pool texture: produce un crash (0x64aa40 -> accesso a `[0+0x20]`), non
   colori sbagliati; (iii) la capienza dei segmenti, 0x1000 voci (0x666ab8).
8. Prime 3 prove (§5): **M1** log del budget e dei picchi (2 trampolini, 1 chiamata per frame); **T1** budget ×2 con HALF8=1 (S1:
   oggi 7 celle magenta su 8 in modo stabile, quindi è l'interruttore più netto); **T2** Eden con `use_asynchronous_gpu_emulation=false`
   (nessun codice).
9. Il dettaglio "+6,8 KB a sys" (§7 di step6c) non è spiegato in modo diretto. Con B1 cambierebbe solo la vittima dello
   sconfinamento, non la frequenza. Inoltre la frequenza valutata a occhio è rumorosa: M1 sostituisce l'occhio con un contatore.

## 1. Cosa resta dopo step6c (punto di partenza)
- [D] Le prove S0/S1/S2 escludono il time-slicing come causa. Resta la legge "più viste per frame = più flash"; il 4P originale
  non sbaglia mai con 2 viste per frame.
- [V step6c] Le strutture per vista di R (AutoExposure, HDRCompose, gruppi, DLM, FinalImage `[R+0x4218]`) sono allocate con
  numViews = 8 e indicizzate con un clamp. Nessuna capienza "per vista" è minore di 8.
- Quindi la risorsa satura non è "per vista" ma **per frame**: si consuma in proporzione alle viste registrate nel frame. Ho
  cercato i contenitori per frame del motore (agl/gsys) che ogni vista consuma. Il più grande, condiviso da tutti i layer, è la
  memoria comandi dei segmenti (§2).

## 2. Pista B1 – Budget comandi/controllo per core e per frame (probabilità ~45%)

### 2.1 Creazione [V]
```
0x704bd0 -> 0x6668a4(ls, cfg) ... 0x666ac4: 0x684c28(D = ls+0x80, cmd = cfg+0x38, ctrl = cfg+0x40, cap = 0x1000, nbuf = cfg+0x48, heap)
  cfg+0x38 = [obj+0x4e8] (0x704ae8), cfg+0x40 = [obj+0x4f0] (0x704af0), cfg+0x48 = 2 (0x704b24/0x704b2c)   valori: da misurare (M1)
0x684c28: cmd arrotondata a 4 (0x684c48); pool comandi = cmd*nbuf*3 (0x684c54/0x684c68, memory pool 0x626fb0 a 0x684ca8)
          riempito con 0xBEEF2929 (0x684cd4/0x684ce0); controllo (CPU) = ctrl*3 dall'heap (0x5fac1c a 0x684c80)
          per core c (0..2, cmp #3 a 0x684e48), blocco p = D+0x88+c*0x28:
             +0x08 (D+0x90) blocchi[nbuf] da 0x18 {pool, offset, ..}   +0x10 usato (D+0x98)   +0x14 dimensione = cmd (D+0x9c)
             +0x18 (D+0xa0) ctrl base = D[0x78] + c*ctrl             +0x20 ctrl usato (D+0xa8) +0x24 ctrl dimensione (D+0xac)
          blocchi nel pool in sequenza: core0/buf0, core0/buf1, core1/buf0, core1/buf1, core2/buf0, core2/buf1 [I: un solo pool,
          D+0x50 = 0 -> w19 sempre 0 a 0x684e20-0x684e2c]
          D+0x100 = 0x1000 voci segmento (0x684ec8), D+0x108 voci 0x188 B
```
### 2.2 Uso per frame [V]
```
0x667358 (costruzione job, una volta per frame) -> 0x6671f0: 0x684ef0(D, flip)
   picco: D+0x118 = max(D+0x118, usato c) e D+0x120 = max(.., ctrl usato c) per c = 0..2; azzera usati e seq (D+0x80);
   se flip: D+0x111 = (D+0x111 + 1) % nbuf  (flip condizionato a ls+0x1b8 & 0x300 == 0x100, 0x6671c8-0x6671e0)
0x684fa4(D, ctx, nome, id) apre un segmento:
   c = TLS-1 oppure GetCurrentCoreNumber (0x684fe0/0x684ff0, nessun limite a 3)
   seq = D+0x80 ++ (atomico); voce = seq < cap ? D[0x108]+seq*0x188 : voce 0 (0x685040)
   libero = dimensione - usato (CON SEGNO, 0x68505c); dato = libero < 4 MiB ? libero : 4 MiB (0x685074/0x685078)
   comandi: blocco[D+0x111] + usato  (0x6850a4 -> 0x62dbf4); controllo: ctrl base + ctrl usato (0x685088 -> 0x62db40)
   BeginRecording 0x62dd24: SetMemoryCallback(0x62ddf4 = ret), AddControlMemory, AddCommandMemory(dimensione > 0x200 senza segno ?
   dimensione : 0x200)  (0x62dd9c/0x62dda4)
0x685110 chiude: usato += comandi usati (0x685188), ctrl usato += seg+0x14c (0x6851a0), nessun controllo.
```
### 2.3 Cosa succede quando il budget finisce [I, da misurare]
- Libero piccolo (< 0x200): il segmento riceve comunque 0x200 byte, oltre il proprio blocco.
- Libero negativo (un segmento precedente ha già sforato): la dimensione negativa viene confrontata senza segno con 0x200 e passata
  così. NVN crede di avere memoria enorme e non chiama più la callback. In ogni caso la callback è `ret` e non aggiunge memoria.
- Destinazione dei byte in più: il blocco che segue nel pool. core0/buf0 -> core0/buf1, cioè il frame precedente o successivo,
  che con la GPU asincrona di Eden può essere ancora in lettura. core0/buf1 -> core1/buf0, cioè i comandi registrati **prima**
  nello stesso frame dall'altro core. core2/buf1 -> **fuori dal pool**, sull'allocazione GPU successiva.
- Memoria di controllo: il blocco CPU `ctrl*3`. Il core 2 sconfina oltre il blocco, nell'heap, cioè sull'oggetto allocato dopo
  all'avvio. Questo è un effetto che dipende dalla disposizione in memoria.
- Perché "solo l'ultima vista corretta" in S1: le viste registrate per ultime su un core sono quelle che sforano, e scrivono sui
  comandi già registrati del blocco successivo (le prime viste dell'altro core). I loro comandi restano integri, quelli sovrascritti
  no. Nella GPU di Eden un flusso di comandi sporcato produce stato casuale: render target, texture o descrittori sbagliati, quindi
  celle bianche o magenta, sagome ciano e texture "a blocchi" (formato o tiling letti male).
- Perché il gioco originale non lo fa mai: in gara registra al massimo 2 viste 3D per frame (time-slicing del 4P). Il budget è
  stato tarato su quel carico. Con 8 locali le viste per frame diventano 4, 6 o 8, e in più ci sono 4 pagine HUD extra e i loro
  layer 2D.
- Pausa [D]: l'apertura aggiunge catture e il layout della pausa nello stesso frame, quindi è un picco di comandi. Coerente.
- Shader asincroni "molto peggio" [I debole]: i comandi registrati dalla guest non cambiano. Può cambiare il tempo: con la GPU che
  non si blocca sulle compilazioni, la CPU registra il frame dopo mentre la GPU legge ancora il buffer sporcato. Non dimostrato.
- "+6,8 KB a sys" [I debole]: con B1 la dimensione di sys non cambia l'uso per frame, cambia solo chi sta dopo il pool e dopo il
  blocco di controllo. Va prima confermato con un contatore (M1) che la frequenza cambi davvero; la valutazione a occhio di un
  difetto intermittente è rumorosa.

### 2.4 Variante B1' – due thread con lo stesso indice core (probabilità ~8%) [I]
L'indice è `TLS-1`, oppure, per i thread senza TLS, il core corrente. Un thread senza TLS sullo stesso core di un worker apre
segmenti con lo stesso indice. Se viene preemptato a metà segmento, i due segmenti partono dallo stesso `usato`: comandi sovrapposti,
"vince l'ultimo", e il fenomeno cresce con i job per frame. Si misura con M2.

## 3. Altre piste

### A2 – Temporanei del pool texture e cache texture di Eden (~15%) [V meccanismo, I effetto]
- Pool = oggetto globale main+0x1355170 (GOT 0x1306018). Voci da 0x228 B (+0x650 numero, +0x658 voci), acquire 0x64a680.
  L'acquire cerca una voce libera con forma identica (formato +0xa8, tipo +0x32, dimensioni +0x38/+0x3a, ...). Se non la trova,
  **ricrea** la voce libera usata meno di recente con una forma nuova (0x64a9c4 -> 0x64b134) e nuova memoria GPU (0x6631d4,
  allocatore a lista libera, allineamento opzionale 64 KiB a 0x663310).
- Con più viste per frame servono più temporanei insieme. Ogni frame si ricreano voci, quindi nascono texture di forma diversa
  sugli stessi indirizzi GPU. Su hardware è lecito; nella cache texture di Eden (yuzu) le sovrapposizioni di formato o tiling
  diversi possono essere ricostruite dalla memoria guest non aggiornata: blocchi colorati, magenta.
- L'esaurimento del pool è escluso come causa silenziosa: un'allocazione fallita porta a 0x64b650 (dump) e poi a un accesso a
  `[0+0x20]` in 0x64b558, cioè un crash [V].
- Misura M3: conteggio delle ricreazioni per frame, letto dallo stato del pool (nessun hook nel percorso caldo).

### C – Emulatore: GPU asincrona e sincronizzazione (~10%) [I]
- Eden v0.2.1 non è disponibile in sorgente. Il collo di bottiglia è VulkanWorker (STATE), quindi la GPU emulata è in ritardo
  sulla CPU. Qualunque scrittura CPU su memoria che la GPU deve ancora leggere (comandi del buffer dell'altro frame, buffer uniform
  multi-buffer, cache) diventa visibile solo con carichi pesanti come 8 viste.
- T2 (impostazione di Eden) separa C+B1 (in volo) da B1 (stesso frame, altro core).

### D2 – Lista per frame R+0x4d00 senza limite (~5%) [V codice, I effetto]
- `0x6ce848(X, ?, p)`: `X[0x760][X[0x758]++] = p` con indice atomico e **nessun controllo** (0x6ce864). Capienza 0x400
  (0x6cda68), azzerata per frame da 0x6ce7cc (chiamata a 0x6ab8f8), unico produttore 0x69a4d8 (calc dei modelli di R, flag
  0x2/0x4).
- Oltre 0x400 scrive nell'heap dopo l'array: è un difetto che dipende dalla disposizione in memoria. Non è chiaro che scali con le
  viste (sembra per modello). Misura M4 e interruttore T4.

### D1 – Scritture fuori limite della mod (~5%) [V lettura del codice]
Riletti i punti che scrivono dati per vista:
- RlmCtorHook, layer 3D 14..17 e 2D 18..21: allocazioni proprie.
- PerFrame: rect di `sub[0..3]` e dei propri layer, `+0xde4` bit 0x6.
- HALF8/GROUPFIX: `cnt >= used` verificato.
- RenderSceneCreateHook e ModelCreateHook: ViewCount temporaneo durante Orig, poi ripristinato.
- Layer3DPrioHook: solo `+0x88`.
- order8: solo l'id.
- RacePageCtorHook e AddVpObjects: buffer a 9 propri.
- Lakitu: array riallocato.
- DrawObjHook: in gara solo Orig.

Nessuna scrittura oltre una capienza trovata. Restano non verificati gli usi dell'indice viewport 5..8 restituito da
LocalSlotToVpHook in codice HUD che indicizza array a 5. Riguarda il 2D, non le tinte 3D, ma va tenuto presente.

### Escluse o ridimensionate [V]
- Blocchi GPU dei modelli g3d a 8 viste: `0x69bc5c bl 0xa721c4` (dimensione) e il pool 0x626fb0 vengono **dopo** `Initialize`
  (0x69bc3c) e dentro l'Orig del nostro ModelCreateHook, quindi sono dimensionati per 8 viste.
- Capienza dei segmenti: 0x1000 voci (0x666ab8). Il fallback alla voce 0 (0x685040) richiede oltre 4096 segmenti per frame.
  Va comunque loggato il massimo (M1).
- Strutture per vista di R: tutte a numViews (step6c §1).

## 4. Domande del brief, risposte brevi
- (a) Le risorse GPU per vista create dal gioco per le viste 4..7 hanno capienza giusta [V step6c + §3]. Il pool texture è
  condiviso e senza fallimento silenzioso; può ricreare temporanei ogni frame (A2, da misurare). Nessuna vista legge target di
  altre viste nel codice analizzato [I].
- (b) **Sì: c'è una capienza per frame consumata da ogni vista e senza protezione**, la memoria comandi e controllo dei segmenti
  per core (B1). Quando si esaurisce non c'è nessun fallimento pulito: si sovrascrive.
- (c) Memory pool del gioco: comandi `flags 4,1` (0x684c98/0x684c9c), allineamento 64 KiB opzionale nell'allocatore GPU. Un effetto
  di allineamento in Eden non è escluso, ma nessun indizio lo collega al numero di viste per frame.
- (d) Nessuna scrittura oltre i limiti trovata nella mod (D1). Il gioco ha una lista senza limite (D2).

## 5. Misure e interruttori (ordinati per informazione/costo)
Tutto il codice va incollato in `src/program/main.cpp` (blocco "step 6d"), con le opzioni in `scripts/build.sh` come le altre
(`-DMK8DX_SEGMUL=..` ecc.). Le installazioni con controllo della parola vanno in `exl_main`. Gli hook su 0x684c28 devono essere
installati prima dell'avvio del layerSystem; exl_main gira prima del main del gioco, quindi va bene.

### 5.1 M1 – Budget, picchi e sconfinamenti per frame (B1) – PRIMA PROVA
Costo: 1 trampolino all'avvio e 1 per frame (0x684ef0, thread di render, nessun percorso caldo). Esito decisivo in una gara.
```cpp
/* ==== step 6d (step6d_flash_root.md) ==== */
#ifndef MK8DX_SEGMUL
#define MK8DX_SEGMUL 1      /* T1: memoria comandi/controllo per core x N (1 = gioco) */
#endif
#ifndef MK8DX_SEGDIV
#define MK8DX_SEGDIV 1      /* T1': / N (prova contraria in 4P senza viste extra) */
#endif
namespace seg6d {
    static uint8_t* D;                                   /* ls+0x80 */
    static uint64_t ctrlEnd; static uint64_t canary[4];   /* 4 qword dopo il blocco di controllo (vicino nell'heap) */
    static uint32_t frames, maxUsed[3], maxCtrl[3], maxSeq, overCmd[3], overCtrl[3], nearCmd[3], noFlip, canaryHits;
}
HOOK_DEFINE_TRAMPOLINE(SegInitHook) {      /* 0x684c28 d10303ff (D, cmd/core, ctrl/core, cap, nbuf, heap, x6) */
    static void Callback(uint8_t* D, uint64_t cmd, uint64_t ctrl, uint32_t cap, uint32_t nbuf, void* heap, void* x6) {
        uint64_t c = cmd * MK8DX_SEGMUL / MK8DX_SEGDIV, t = ctrl * MK8DX_SEGMUL / MK8DX_SEGDIV;
        Orig(D, c, t, cap, nbuf, heap, x6);
        seg6d::D = D;
        uint8_t* cb = *(uint8_t**)(D + 0x78);
        if (cb) { seg6d::ctrlEnd = (uint64_t)cb + 3 * t; memcpy(seg6d::canary, (void*)seg6d::ctrlEnd, sizeof seg6d::canary); }
        Logging.Log("[mk8dx-8p] 6d seg: D=%p cmd/core %#lx->%#lx ctrl/core %#lx->%#lx cap=%u nbuf=%u | D+9c=%#x D+ac=%#x ctrl=%p fine=%#lx heap=%p",
                    D, cmd, c, ctrl, t, cap, nbuf & 0xff, *(uint32_t*)(D + 0x9c), *(uint32_t*)(D + 0xac), cb, seg6d::ctrlEnd, heap);
    }
};
HOOK_DEFINE_TRAMPOLINE(SegFrameHook) {     /* 0x684ef0 b9409809 (D, flip): fine frame, PRIMA dell'azzeramento degli usati */
    static void Callback(uint8_t* D, bool flip) {
        using namespace seg6d;
        if (D == seg6d::D) {
            bool over = false;
            for (int c = 0; c < 3; c++) {
                uint8_t* p = D + 0x88 + c * 0x28;
                uint32_t u = *(uint32_t*)(p + 0x10), s = *(uint32_t*)(p + 0x14), cu = *(uint32_t*)(p + 0x20), cs = *(uint32_t*)(p + 0x24);
                if (u > maxUsed[c]) maxUsed[c] = u;
                if (cu > maxCtrl[c]) maxCtrl[c] = cu;
                if (u > s) { overCmd[c]++; over = true; } else if (u + 0x10000 > s) nearCmd[c]++;
                if (cu > cs) { overCtrl[c]++; over = true; }
            }
            uint32_t seq = *(uint32_t*)(D + 0x80); if (seq > maxSeq) maxSeq = seq;
            if (!flip) noFlip++;
            if (ctrlEnd && memcmp(canary, (void*)ctrlEnd, sizeof canary)) { canaryHits++; memcpy(canary, (void*)ctrlEnd, sizeof canary); }
            frames++;
            static uint32_t loggedOver;
            if ((frames % 300) == 0 || (over && loggedOver++ < 20))
                Logging.Log("[mk8dx-8p] 6d seg f=%u viste=%d seq=%u(max %u) | usato %#x/%#x/%#x max %#x/%#x/%#x dim %#x | ctrl max %#x/%#x/%#x dim %#x"
                            " | sfora cmd %u/%u/%u ctrl %u/%u/%u vicino %u/%u/%u | picchi gioco %#lx/%#lx | noflip %u canary %u",
                            frames, proto::active ? 4 + proto::Used() : 4, seq, maxSeq,
                            *(uint32_t*)(D + 0x98), *(uint32_t*)(D + 0xc0), *(uint32_t*)(D + 0xe8), maxUsed[0], maxUsed[1], maxUsed[2], *(uint32_t*)(D + 0x9c),
                            maxCtrl[0], maxCtrl[1], maxCtrl[2], *(uint32_t*)(D + 0xac),
                            overCmd[0], overCmd[1], overCmd[2], overCtrl[0], overCtrl[1], overCtrl[2], nearCmd[0], nearCmd[1], nearCmd[2],
                            *(uint64_t*)(D + 0x118), *(uint64_t*)(D + 0x120), noFlip, canaryHits);
        }
        Orig(D, flip);
    }
};
/* exl_main:
    if (*reinterpret_cast<uint32_t*>(base + 0x684c28) == 0xd10303ff) SegInitHook::InstallAtOffset(0x684c28);
    if (*reinterpret_cast<uint32_t*>(base + 0x684ef0) == 0xb9409809) SegFrameHook::InstallAtOffset(0x684ef0);   */
```
Nota: `proto::Used()` e `proto::active` sono definiti prima di questo blocco (namespace proto, inizio file). Il blocco va quindi
messo dopo `proto`.

Protocollo (stessa pista, shader sincroni, contatori invece dell'occhio):
1. 4P originale (4 locali), una gara. Atteso: `sfora` = 0 e picco < dimensione. Il margine `dim - max` è la riserva del gioco.
2. 8 locali con HALF8=1 (S1), 30 s. Se B1 è vero: `sfora` cresce quasi a ogni frame su almeno un core.
3. 8 locali con HALF8=2 (S2): sconfinamenti sporadici, che coincidono nel tempo con i flash. Annotare l'ora dei flash, o
   registrarli con un pulsante.
4. Ripetere 2 nelle build MENU3D8=0 e MENU3D8=1 completa. Se `cmd/core` o `ctrl/core` loggati a 0x684c28 sono diversi tra le build,
   la bisezione di step6c §7 è spiegata: il budget dipenderebbe dalla memoria. Se sono uguali, conta solo la vittima
   (`canary`, `fine=`).

Interpretazione: `sfora > 0` in 8P e `= 0` in 4P implica B1 molto probabile, da chiudere con T1. `sfora = 0` e `vicino = 0` anche
in S1 esclude B1 e fa passare ad A2 (M3) e C (T2).

### 5.2 T1 – Interruttore del budget (B1) – SECONDA PROVA
- Build: `MK8DX_HALF8=1 MK8DX_SEGMUL=2` (poi 3 se il log di M1 mostra ancora `sfora`), 8 locali, stessa pista. Atteso con B1:
  le 7 celle magenta di S1 tornano corrette. Poi `HALF8=0` (build normale): spariscono anche i flash.
- Costo in memoria: `cmd*2*3*(MUL-1)` di memoria GPU più `ctrl*3*(MUL-1)` di CPU dall'heap dell'avvio. Leggere `cmd/core` nel log:
  con valori di qualche MiB sono +10..20 MiB. Se l'allocazione fallisce (crash nel riempimento 0x684ce4) usare MUL=3/2, cioè
  `cmd*3/2` (per esempio `-DMK8DX_SEGMUL=3 -DMK8DX_SEGDIV=2`).
- Prova contraria (T1'): 4P originale con `MK8DX_SEGDIV=k`, con k scelto dal picco del 4P di M1 in modo che `dim/k` sia minore
  del picco. Atteso con B1: compaiono gli stessi flash bianchi/magenta in 4P, senza nessuna vista extra.
- Correzione definitiva se confermato: SEGMUL fisso, ridotto al minimo necessario e attivo solo con la mod 8P. È un'estensione
  della stessa risorsa del gioco (budget ×N come ×N viste), senza toppe sul flusso. In alternativa, o in aggiunta, HALF8=2 (4 viste
  per frame, come il 4P) dimezza il consumo.

### 5.3 T2 – Eden, GPU sincrona (C e B1 "in volo") – TERZA PROVA (nessun codice)
- Eden chiuso, in `switch_env/eden/user/config/qt-config.ini`: `use_asynchronous_gpu_emulation=false` più la riga
  `use_asynchronous_gpu_emulation\default=false`. Stessa gara (build normale, poi S1). Al termine rimettere il valore precedente.
- Lettura dell'esito:
  - Flash spariti o molto ridotti: i dati sporcati sono "in volo" (buffer dell'altro frame letto in ritardo dalla GPU), cioè C,
    oppure B1 sul blocco core0/buf0 -> buf1.
  - Nessun cambiamento in S1: corruzione nello stesso frame (B1 sul blocco del core successivo, o A2).
- Variante: precisione GPU "Alta/Estrema" (`gpu_accuracy`), utile solo per A2.

### 5.4 M2 – Segmenti sovrapposti sullo stesso indice core (B1')
Due trampolini nel percorso dei segmenti: poche centinaia di chiamate per frame, solo operazioni atomiche, nessun log lì.
```cpp
namespace seg6d {
    static volatile int32_t openN[8]; static volatile uint64_t owner[8];
    static volatile uint32_t sameThread, crossThread, badIdx;
    static inline uint64_t Tid() { uint64_t v; asm volatile("mrs %0, tpidrro_el0" : "=r"(v)); return v; }
    static inline int IdxOf(uint8_t* D, int32_t seq) {
        if (seq < 0) return -1;
        uint32_t cap = *(uint32_t*)(D + 0x100); uint8_t* e = *(uint8_t**)(D + 0x108);
        uint8_t* s = (uint32_t)seq < cap ? e + seq * 0x188 : e;            /* come 0x685040 */
        return s ? s[0x180] : -1;
    }
}
HOOK_DEFINE_TRAMPOLINE(SegOpenHook) {      /* 0x684fa4 d10203ff (D, ctx, nome, id) -> seq o -1 */
    static int32_t Callback(uint8_t* D, void* ctx, void* name, int32_t id) {
        int32_t r = Orig(D, ctx, name, id);
        int i = seg6d::IdxOf(D, r);
        if (i < 0) return r;
        if (i >= 3) __atomic_fetch_add(&seg6d::badIdx, 1, __ATOMIC_RELAXED);
        i &= 7;
        if (__atomic_fetch_add(&seg6d::openN[i], 1, __ATOMIC_ACQ_REL) > 0) {
            if (seg6d::owner[i] == seg6d::Tid()) __atomic_fetch_add(&seg6d::sameThread, 1, __ATOMIC_RELAXED);
            else __atomic_fetch_add(&seg6d::crossThread, 1, __ATOMIC_RELAXED);
        }
        seg6d::owner[i] = seg6d::Tid();
        return r;
    }
};
HOOK_DEFINE_TRAMPOLINE(SegCloseHook) {     /* 0x685110 f81d0ff5 (D, ctx, seq) -> voce */
    static void* Callback(uint8_t* D, void* ctx, int32_t seq) {
        int i = seg6d::IdxOf(D, seq);
        void* r = Orig(D, ctx, seq);
        if (i >= 0) __atomic_fetch_sub(&seg6d::openN[i & 7], 1, __ATOMIC_ACQ_REL);
        return r;
    }
};
/* exl_main:
    if (*reinterpret_cast<uint32_t*>(base + 0x684fa4) == 0xd10203ff) SegOpenHook::InstallAtOffset(0x684fa4);
    if (*reinterpret_cast<uint32_t*>(base + 0x685110) == 0xf81d0ff5) SegCloseHook::InstallAtOffset(0x685110);
   in SegFrameHook, nella riga di log: crossThread, sameThread, badIdx */
```
`crossThread > 0` (soprattutto se cresce con le viste) indica B1'. `sameThread > 0` va confrontato con il 4P: se c'è anche nel gioco
originale, è un annidamento normale (segmento di step dentro il Draw) e va interpretato con la memoria data (§2.2).
`badIdx > 0` indica un indice ≥ 3, cioè un blocco per core letto oltre i 3 (D+0x100.. è cap/voci): corruzione certa.

### 5.5 M3 – Ricreazioni del pool texture (A2), senza hook
Una volta per frame, in `SegFrameHook` prima di `Orig`, dalla sola lettura del pool:
```cpp
namespace pool6d { static uint64_t key[512], mem[512]; static uint32_t recreate, maxUsedEntries, nLast; }
static void Pool6dScan() {
    uint8_t* pool = (uint8_t*)(mk8dx::Base() + 0x1355170);          /* GOT 0x1306018 -> oggetto globale */
    uint32_t n = *(uint32_t*)(pool + 0x650); uint8_t* e = *(uint8_t**)(pool + 0x658);
    if (!e) return;
    uint32_t used = 0;
    for (uint32_t i = 0; i < n && i < 512; i++, e += 0x228) {
        uint64_t k = (uint64_t)*(uint16_t*)(e + 0xa8) | (uint64_t)*(uint16_t*)(e + 0x32) << 16 |
                     (uint64_t)*(uint16_t*)(e + 0x38) << 32 | (uint64_t)*(uint16_t*)(e + 0x3a) << 48;
        uint64_t m = *(uint64_t*)(e + 0x180);
        if (pool6d::key[i] && (pool6d::key[i] != k || pool6d::mem[i] != m)) pool6d::recreate++;
        pool6d::key[i] = k; pool6d::mem[i] = m;
        used += e[0x1e9] & 1;
    }
    pool6d::nLast = n; if (used > pool6d::maxUsedEntries) pool6d::maxUsedEntries = used;
}
/* SegFrameHook: Pool6dScan(); e nella riga di log: pool6d::nLast, pool6d::maxUsedEntries, pool6d::recreate */
```
Lettura: nel 4P originale, a regime, `recreate` deve restare praticamente fermo. Se cresce a ogni frame in 8P (soprattutto in S1),
A2 sale: Eden riceve ogni frame texture nuove sugli stessi indirizzi. I campi `+0x38/+0x3a` sono dimensioni [I]; la chiave confronta
comunque anche formato, tipo e memoria, quindi un cambiamento indica davvero una ricreazione.

### 5.6 M4/T4 – Lista per frame di R senza limite (D2)
```cpp
namespace rl6d { static int32_t max0, max1, cap; static uint32_t over; }
HOOK_DEFINE_TRAMPOLINE(RListResetHook) {   /* 0x6ce7cc a9be4ff4 (X = R+0x4d00, w1): azzeramento per frame */
    static void Callback(uint8_t* X, uint32_t w1) {
        int32_t a = *(int32_t*)(X + 0x758), b = *(int32_t*)(X + 0x768); rl6d::cap = *(int32_t*)(X + 0x75c);
        if (a > rl6d::max0) rl6d::max0 = a; if (b > rl6d::max1) rl6d::max1 = b;
        if (a > rl6d::cap || b > rl6d::cap) rl6d::over++;
        Orig(X, w1);
    }
};
/* exl_main: if (*reinterpret_cast<uint32_t*>(base + 0x6ce7cc) == 0xa9be4ff4) RListResetHook::InstallAtOffset(0x6ce7cc);
   log in SegFrameHook: rl6d::max0, max1, cap, over */
#ifndef MK8DX_RLIST_CAP
#define MK8DX_RLIST_CAP 0   /* T4: 0 = gioco (0x400); altrimenti nuova capienza della lista R+0x4d00+0x758 (e +0x768 che la copia) */
#endif
HOOK_DEFINE_INLINE(RListCapHook) {         /* 0x6cda6c `orr w3,wzr,#0x8` dopo `orr w1,wzr,#0x400` (0x6cda68) */
    static void Callback(exl::hook::InlineCtx* ctx) { if (MK8DX_RLIST_CAP) ctx->W[1] = MK8DX_RLIST_CAP; }
};
/* exl_main: if (MK8DX_RLIST_CAP && *reinterpret_cast<uint32_t*>(base + 0x6cda6c) == 0x321d03e3) RListCapHook::InstallAtOffset(0x6cda6c); */
```
T4 serve solo se M4 mostra `over > 0`. Senza sconfinamenti questa pista è esclusa.

### 5.7 Osservazione con il debugger (opzionale, solo se M1 conferma)
Watchpoint di scrittura (Eden `use_gdbstub=true` più la riga `\default=false`, poi rimettere a false) sul primo qword dopo il
blocco di controllo (`fine=` nel log di 0x684c28) e sul primo qword dopo il pool comandi. Il PC della prima scrittura (dentro la
libreria NVN della guest) conferma lo sconfinamento e indica la vittima. L'indirizzo è freddo, quindi il watchpoint non pesa sul
percorso caldo.

## 6. Prime 3 prove (riassunto operativo)
1. **M1** (§5.1): incollare SegInitHook e SegFrameHook. Fare una gara 4P originale, poi 8P HALF8=1, poi 8P HALF8=2, con shader
   sincroni. Guardare `sfora`, `vicino`, `max` contro `dim`, e `cmd/core`.
2. **T1** (§5.2): `MK8DX_HALF8=1 MK8DX_SEGMUL=2`. Se le celle magenta di S1 spariscono, la causa è B1. Poi la build normale con
   SEGMUL fisso.
3. **T2** (§5.3): `use_asynchronous_gpu_emulation=false` sulla build normale e su S1 (nessun codice). Separa la corruzione "in volo"
   da quella "nello stesso frame".

Se M1 è negativo (nessuno sconfinamento, margine ampio), passare a M3 (pool), M2 e M4 nello stesso giro: il codice è già nel
blocco di M1.

## 7. Esiti M1/T2 (4 nov sera) e nuova pista

### 7.1 Esiti [D, coordinatore; build 1b24778, impostazioni Eden predefinite, Mario Kart Stadium]
| prova | risultato |
|---|---|
| M1, 0x684c28 | cmd/core 0x300000, ctrl/core 0x100000, cap 4096, nbuf 2 |
| M1, gara 4P (22 fps, nessun flash) | usato per frame 0x5000..0x1d000; picchi comandi 0xd0e40/0x9f23c/0xa8188, controllo 0x20d60/0x32e08/0x2cad8 |
| M1, gara 8P (7-8 fps, flash continui) | viste=8, seq=40, usato 0x33158/0x317fc/0x35cb0; per tutta la sessione sfora 0, ctrl 0, vicino 0, canary 0, noflip 1 |
| T2, `use_asynchronous_gpu_emulation=false` | flash uguali o peggiori (3-4 fps) |

- **B1 escluso [V runtime]:** l'uso resta sotto il 30% del budget e non c'è nessuno sconfinamento.
- **C escluso [V runtime]:** con la GPU sincrona il difetto resta, quindi non è una scrittura CPU su dati "in volo". È un errore
  deterministico nell'**ordine dei comandi del frame stesso**: la GPU esegue ciò che le viene dato, ma riceve dati sbagliati.
- Il dato chiave resta: in S1 è corretta solo l'ultima vista disegnata. Questa è la firma di "una risorsa usata in due momenti
  diversi della GPU, tra i quali un'altra vista la riusa".

### 7.2 Nuova pista principale A2' – Memoria GPU dei temporanei liberata al rilascio e riusata nello stesso frame (~40%)
Meccanismo [V statico]:
- `0x64b024` (242 chiamanti) salta a `0x64b028` = release del pool texture (main+0x1355170). Il release **libera subito la memoria
  GPU della voce** (`0x64b0f0 bl 0x663608`, poi `voce+0x180 = 0` a 0x64b0fc) e segna la voce libera (0x64b110). Con il bit4 di
  `pool+0x29` passa da una lista differita (+0x718), ma libera comunque (0x64b080 -> 0x64b0f0).
- Ogni acquire, anche quando trova una voce libera con la stessa forma, prende **memoria nuova** dall'allocatore a lista libera
  first-fit (`0x64aa00 str x0,[x23,#0x180]` dopo 0x6631d4).
- Quindi, dentro un frame, i temporanei di una vista occupano gli indirizzi GPU appena liberati da un'altra vista, con forma,
  formato e dimensioni diverse. L'ordine è quello di registrazione **CPU** (2 core, mutex del pool). La GPU invece esegue i segmenti
  **ordinati per id**: prima tutti i GPUCalc (id 1..32, 0x6841d4; per Layer3D è attivo il passo 1, 0x68d700), poi le bande Draw
  per slot.

Perché "solo l'ultima vista è corretta" [I]:
- Una vista che usa un temporaneo in due bande diverse (per esempio acquisito nel suo GPUCalc o in uno step e letto in un passo
  successivo) oppure tra due segmenti che la GPU esegue in ordine diverso da quello della CPU vede l'area riscritta dalle viste
  registrate dopo. Solo l'ultima registrata non viene sovrascritta da nessuno.
- Con 2 viste per frame (4P originale) le due viste girano in contemporanea sui 2 core e non si scambiano mai la memoria: mai difetti.
  Con 4 per frame lo scambio dipende dai tempi dei core: flash sparsi. Con 8 è sistematico.
- In alternativa, con la stessa radice, la cache texture di Eden: immagini con formato e tiling diversi sullo stesso indirizzo nello
  stesso frame vengono invalidate o ricostruite dalla memoria guest. Questo spiega le texture "a blocchi" e i colori magenta.
  Anche questo è indipendente dalla GPU sincrona o asincrona.
- Spiega la sensibilità alla disposizione e al tempo: gli indirizzi first-fit dipendono dallo stato dell'allocatore e i tempi dei
  core cambiano l'ordine di riuso. +6,8 KB su sys basta a spostare tempi e allocazioni.

Probabilità aggiornate: **A2' 40%**; risorsa unica per frame scritta dalla CPU per vista in un oggetto di post-processing senza
array (Bloom/Glare, step6c §6.2) 15%; cache texture di Eden come causa autonoma 10%; B1' indice core 8% (M2 è ancora utile e
costa poco); D2 lista R+0x4d00 4%; D1 mod 5%; B1 0% e C <3% (misurati); altro 15%.

Verificati ed esclusi in questa tornata [V]:
- PrevFrameBuffer per vista: oggetto `R-post+0x428`, voci 0x760, count = numViews, cioè 8 (0x6d4998 -> 0xae19ac, 0xae1cfc).
- Parametri di AutoExposure: scritti con `UpdateUniformBuffer` inline (0x62f8c8 -> slot 0x130b3d8), quindi ordinati nel flusso dei
  comandi e non una scrittura CPU condivisa.

### 7.3 M5 – Misura decisiva: riuso di indirizzi del pool nello stesso frame (A2')
Un hook inline all'uscita comune dell'acquire (0x64ac44 `mov x0,x23`, parola `aa1703e0`). Il reset e il log stanno in SegFrameHook
(§5.1, già in build), una volta per frame.
```cpp
namespace pool6d2 {
    struct U { uint64_t pool; uint32_t off, size; uint64_t key; };
    static U tab[512]; static volatile int n;
    static volatile uint32_t acq, alias, same, fAlias, frames, aliasFrames;
    static inline uint64_t Key(uint8_t* e) {
        return (uint64_t)*(uint16_t*)(e + 0xa8) | (uint64_t)*(uint16_t*)(e + 0x32) << 16 |
               (uint64_t)*(uint16_t*)(e + 0x38) << 32 | (uint64_t)*(uint16_t*)(e + 0x3a) << 48;
    }
}
HOOK_DEFINE_INLINE(PoolAcqExitHook) {      /* 0x64ac44 aa1703e0 `mov x0,x23`: uscita comune di 0x64a680, x23 = voce */
    static void Callback(exl::hook::InlineCtx* ctx) {
        using namespace pool6d2;
        uint8_t* e = (uint8_t*)ctx->X[23]; if (!e) return;
        uint8_t* m = *(uint8_t**)(e + 0x180); if (!m) return;            /* blocco: +0x18 dim, +0x20 pool, +0x28 offset */
        uint64_t pool = *(uint64_t*)(m + 0x20); uint32_t off = *(uint32_t*)(m + 0x28), sz = (uint32_t)*(uint64_t*)(m + 0x18);
        uint64_t k = Key(e);
        __atomic_fetch_add(&acq, 1, __ATOMIC_RELAXED);
        int cnt = n < 512 ? n : 512;
        for (int i = 0; i < cnt; i++) {
            U& u = tab[i];
            if (u.pool == pool && off < u.off + u.size && u.off < off + sz) {       /* indirizzo gia' usato in questo frame */
                if (u.key != k || u.off != off) __atomic_fetch_add(&alias, 1, __ATOMIC_RELAXED);
                else __atomic_fetch_add(&same, 1, __ATOMIC_RELAXED);
                break;
            }
        }
        int i = __atomic_fetch_add(&n, 1, __ATOMIC_RELAXED);
        if (i < 512) tab[i] = { pool, off, sz, k };
    }
};
/* in SegFrameHook, prima di Orig (inizio del nuovo frame):
       { using namespace pool6d2; frames++; if (alias) aliasFrames++; fAlias = alias;
         if ((frames % 300) == 0 || (alias && aliasFrames < 10))
             Logging.Log("[mk8dx-8p] 6d pool f=%u viste=%d acq=%u riuso-stessa-forma=%u alias-forma-diversa=%u frame-con-alias=%u",
                         frames, proto::active ? 4 + proto::Used() : 4, acq, same, alias, aliasFrames);
         acq = alias = same = 0; n = 0; }
   exl_main: if (*reinterpret_cast<uint32_t*>(base + 0x64ac44) == 0xaa1703e0) PoolAcqExitHook::InstallAtOffset(0x64ac44); */
```
Lettura (shader sincroni):
- 4P originale: `alias` circa 0 a regime (le 2 viste non si scambiano memoria).
- 8P S1: `alias` > 0 a ogni frame.
- 8P S2: `frame-con-alias` cresce a scatti, insieme ai flash.

Se in 8P `alias` resta 0, A2' è escluso e si passa alla pista Bloom/Glare (§7.6).
Nota: lo stub inline di exlaunch salva X0..X30 (`ctx->X[23]`); il sito è un `mov` senza flag, senza problemi con NZCV.

### 7.4 T3 – Interruttore: memoria dei temporanei liberata solo a inizio frame (conferma A2')
Le free chiamate dal release del pool (ritorno a main+0x64b0f4) vengono rimandate all'inizio del frame successivo. Dentro un frame
ogni temporaneo ha quindi un indirizzo suo e nessuna vista riusa la memoria di un'altra. Da un frame all'altro gli indirizzi si
ripetono uguali, perché la free avviene tutta insieme e l'allocazione rifà la stessa sequenza.
```cpp
#ifndef MK8DX_POOLDEFER
#define MK8DX_POOLDEFER 0   /* T3: 1 = free della memoria dei temporanei del pool rimandata a inizio frame (solo gara a 8) */
#endif
namespace pdefer { struct F { void* alloc; void* blk; uint32_t w2; }; static F q[2048]; static volatile int n; static uint32_t maxN, lost; }
HOOK_DEFINE_TRAMPOLINE(GpuFreeHook) {      /* 0x663608 a9bd57f6 (allocatore, blocco, w2): free dell'allocatore GPU */
    static void Callback(void* alloc, void* blk, uint32_t w2) {
        if (MK8DX_POOLDEFER && proto::active &&
            (uintptr_t)__builtin_return_address(0) == mk8dx::Base() + 0x64b0f4) {   /* chiamata dal release 0x64b0f0 */
            int i = __atomic_fetch_add(&pdefer::n, 1, __ATOMIC_RELAXED);
            if (i < 2048) { pdefer::q[i] = { alloc, blk, w2 }; return; }
            __atomic_fetch_add(&pdefer::lost, 1, __ATOMIC_RELAXED);                 /* coda piena: free subito */
        }
        Orig(alloc, blk, w2);
    }
};
static void PoolDeferFlush() {             /* da SegFrameHook prima di Orig: job del frame precedente terminati */
    int n = pdefer::n < 2048 ? pdefer::n : 2048;
    if (!n) return;
    uint8_t* pool = (uint8_t*)(mk8dx::Base() + 0x1355170);
    mk8dx::Fn<void (*)(void*)>(0x618088)(pool + 0x6a0);                         /* stesso lock del release (0x64b048) */
    for (int i = 0; i < n; i++) GpuFreeHook::Orig(pdefer::q[i].alloc, pdefer::q[i].blk, pdefer::q[i].w2);
    mk8dx::Fn<void (*)(void*)>(0x618098)(pool + 0x6a0);
    if ((uint32_t)n > pdefer::maxN) pdefer::maxN = n;
    pdefer::n = 0;
}
/* exl_main: if (*reinterpret_cast<uint32_t*>(base + 0x663608) == 0xa9bd57f6) GpuFreeHook::InstallAtOffset(0x663608);
   SegFrameHook: PoolDeferFlush(); log di pdefer::maxN e pdefer::lost ogni 300 frame */
```
- Esito atteso con A2': in **S1** (`MK8DX_HALF8=1 MK8DX_POOLDEFER=1`) le 7 celle magenta tornano corrette; poi la build normale
  senza flash. Con `POOLDEFER=0` torna il difetto. È l'interruttore.
- Rischi:
  - Serve più memoria GPU nel frame: la somma dei temporanei di 8 viste invece del massimo istantaneo. Se l'allocatore non basta,
    l'acquire fallisce e crasha (0x64aa40 -> dump 0x64b650): provare prima con S2 (4 viste), poi S1.
  - La free nel ramo bit4 (lista +0x718) passa anch'essa da 0x64b0f0 e viene quindi rimandata: è voluto.
  - `GpuFreeHook::Orig` chiamata da fuori l'hook: in exlaunch `Orig` è un membro statico, quindi è lecito.
  - La firma di 0x663608 è a 3 argomenti in registro (x0, x1, w2 a 0x64b0ec); nessun argomento sullo stack.
- Se T3 corregge: la correzione definitiva è questa stessa, ristretta alla gara a 8. Oppure va trovato quale passo usa un
  temporaneo oltre il suo rilascio (M6) e si allunga solo quello.

### 7.5 T5 – Ordine GPU invertito (separa "ultima sulla GPU" da "ultima sulla CPU"), costo minimo
In `order8::Rank` (codice esistente), come prima riga:
```cpp
#ifndef MK8DX_6D_ORDER
#define MK8DX_6D_ORDER 0   /* T5: 1 = bande 3D extra (14..17) PRIMA di 3D Sub0..3 sulla GPU */
#endif
    if (MK8DX_6D_ORDER == 1) { if (slot >= 2 && slot <= 5) return slot + 4; if (slot >= 14 && slot <= 17) return 2 + (slot - 14); }
```
Biiezione: 0,1 | extra -> 2..5 | Sub0..3 -> 6..9 | il resto invariato. In S1:
- Se la cella corretta passa dalla vista 7 alla vista 3 (Sub3, ora ultima sulla GPU): conta l'ordine GPU, cioè una risorsa
  sovrascritta sulla GPU (A2' o la cache di Eden).
- Se resta la vista 7: conta l'ordine CPU di registrazione, cioè dati scritti dalla CPU per vista e letti dopo (pista §7.6).

### 7.6 Se M5 dà alias = 0: oggetti di post-processing a stato unico
step6c §6.2 ha indicato Bloom (`[R+0x3c48]`, flag `+0x798`) e gli effetti 0xac3cbc/0xacb034/0xac8a80 senza array per vista.
L'interruttore è una toggle per effetto in `proto::PerFrame`, una per build, in S1:
- `ae[0x840] = 0` con `ae = *(uint8_t**)(R + 0x3c60)` (AutoExposure, M7 di step6c);
- `bl[0x798] = 0` con `bl = *(uint8_t**)(R + 0x3c48)` (Bloom, flag [I]).

Se una delle due fa sparire la tinta nelle 7 celle, si cerca nel suo oggetto il buffer CPU per frame. Gli offset `+0x798` e
`+0x3c48` sono quelli di step6c (non riverificati qui).

### 7.7 Ordine consigliato
1. **M5** (un hook inline e due righe in SegFrameHook) insieme a **T5** (una riga, build separata), in S1.
2. **T3** `POOLDEFER=1` in S2, poi in S1, poi nella build normale.
3. Solo se 1 e 2 sono negativi: §7.6 e M2 (§5.4).

## 8. Punti incerti (prima stesura)
- Valori reali di `cfg+0x38/+0x40` (`[obj+0x4e8]/[obj+0x4f0]` a 0x704ae8): non trovati staticamente (oggetto di configurazione
  gfx). Li dà M1. Non so se dipendano dalla memoria libera, come richiederebbe la bisezione di step6c §7.
- Comportamento di NVN della guest quando la callback non aggiunge memoria: si assume che continui a scrivere oltre. Potrebbe invece
  smettere di registrare (comandi persi): l'effetto visivo sarebbe simile e M1 lo vede come `vicino` invece che `sfora`.
- Disposizione dei blocchi nel pool: un solo pool (`D+0x50 = 0`) è dedotto dal codice e non verificato a runtime.
- Il segnale "shader asincroni molto peggio" e la sensibilità ai +6,8 KB non sono spiegati in modo diretto da B1.
- Le ipotesi su Eden (cache texture e sovrapposizioni, ritardo della GPU) non hanno riscontro nei sorgenti, che non sono disponibili.
- Il flip del buffer è condizionato (0x6671c8-0x6671e0): se in gara non scattasse a ogni frame, ogni frame riscriverebbe il buffer
  in lettura. M1 lo conta (`noflip`).

## 9. File
- `step6d_data (dati non pubblicati)/root_verify.py` -> `root_verify.txt` (118 OK, 0 FAIL): siti del gestore segmenti, callback, pool texture,
  lista R, blocchi dei modelli, job/segmenti, stringhe nella mod.
- `step6d_data (dati non pubblicati)/nvnslots.py` -> `nvn_slots.txt`: slot GOT delle funzioni NVN (SetMemoryCallback 0x130b0f0,
  AddCommandMemory 0x130b100, AddControlMemory 0x130b108, BeginRecording 0x130b140, ...).
- `step6d_data (dati non pubblicati)/xrefs.py`: riferimenti adrp+add/ldr a indirizzi dati.

## 10. Esiti M5/T3 in Eden (4 nov 2026, sera)
- M5 (PoolAcqExitHook 0x64ac44): `alias-forma-diversa` > 0 in quasi ogni frame **anche nel menu e nella gara a 4** (7-10 per frame,
  nessun flash); in gara a 8 44 nel frame registrato. Il riuso di indirizzi del pool con forma diversa nello stesso frame e' normale.
- T3 (MK8DX_POOLDEFER=1, GpuFreeHook 0x663608): free rimandate attive in gara a 8 (max 176 per frame, 0 perse), flash ancora frequenti
  (forse leggermente meno), 8-9 fps, nessun crash. => A2' molto improbabile come causa unica.
- Restano: T5 (ordine GPU invertito in S1), §7.6 (spegnere AutoExposure/Bloom/Glare uno alla volta in S1), B1', D1, D2.

## 11. Esito T5 e analisi Sub0..3 contro extra

### 11.1 Dati [D, coordinatore]
| prova | ordine GPU | risultato |
|---|---|---|
| S1 (HALF8=1) | Sub0..3, poi extra 4..7 | P1-P7 magenta, P8 corretta |
| S1 + T5 (MK8DX_6D_ORDER=1) | extra 4..7, poi Sub0..3 | P1-P4 magenta fisse, P5-P7 sempre corrette, P8 flash rari |
| S0 (build normale, fasi 0,0,1,1, 4..7 sempre) | Sub, poi extra | P3/P4 (Sub2/Sub3, fase 1) bianche |
| 4P originale | – | mai difetti |

Lettura:
1. Le Sub0..3 sono sbagliate in S1 qualunque sia l'ordine.
2. Le extra sono sbagliate solo se disegnate **dopo** le Sub: le Sub lasciano qualcosa di sporco che contamina le viste successive.
3. Una extra disegnata prima delle Sub, nello stesso frame, è corretta. Quindi la contaminazione **non passa da un frame al
   successivo**: viene rimessa a posto a inizio frame (o da Begin) e riscritta dalle Sub.
4. P8, l'ultima extra, si salva in S1 normale e ha flash rari in T5. È l'unico punto ancora non spiegato.

### 11.2 Cosa distingue staticamente una Sub da una extra [V]
- **RLM, per frame** (0x3cd194, solo sui 4 slot `mgr+0x9bc+i*0x20`, cioè solo le Sub; ciclo `cmp x20,#0x80`):
  - `layer+0xde4` bit 0x4 = `mgr+0x9d6+i*0x20` (0x3cd1e0-0x3cd1f8);
  - rect da `mgr+0xa38[mgr+0x9d0+i*0x20]` e aspetto `proj+0xb0`;
  - accensione = `mgr+0x9d4 && !mgr+0xc72`.

  Le extra non passano di qui. La mod copia sulle extra solo i bit 0x2/0x4 di Sub0 (COPY_DE4).
- **Setup del target per vista** 0x6b5334(R+0x3fd0, ctx, vista, ..) legge flag **del layer** e li passa a 0x6f86e0 (target e clear
  della voce FinalImage della vista):
  - bit0 = `R+0x3fd0+0x320 != 0`;
  - bit1 = `layer+0xde4` bit 0x4 (0x6b5398-0x6b53c0);
  - bit2 = clear (`layer+0x83 & 3`, colore `layer+0x8c`; oppure 0x6bd968);
  - bit3 = `layer+0x1089` bit 0x4, con le condizioni a 0x6b53e4-0x6b5434 (R+0xd18 bit2, cfg+0x1570, cfg+0xac0, R+0x8e8,
    R+0xa40 > 0, view+0x388 bit2).
- **Flag `layer+0x1089`:** azzerato dal ctor Layer3D (0x68d160). Bit0 letto dal calc di R (0x6aba84/0x6aba9c), bit1 dal getter
  0x68d564, bit2 dal setup (0x6b5424). Per i Layer3D non ho trovato scrittori: le scritture con `mov #0x1089` stanno in oggetti
  di altre classi (0x3e.., 0x42..), quindi per le Sub il valore va misurato.
- **Indice vista:** 0x68d4f4(layer, R) -> 0x6ab220 assegna la **prima vista libera** (`view+0x390 == 0`). Le Sub, collegate per
  prime, prendono le viste 0..3 di R; le extra prendono 4..7 (RlmAttachHook). Tutto ciò che R o il gioco fanno per "vista 0..3"
  (gruppi 0..3 di 0x3cd5c8, 0x87e538 per locale, voci FinalImage/AutoExposure 0..3, cache) segue l'indice, non il layer.
- Ctor base dei layer 0x66446c: `+0x80 = 0`, `+0x82..+0x9b = 0` (quindi `+0x83 = 0`, nessun clear), uguale per Sub ed extra.

Due spiegazioni restano compatibili con T5, e le prove qui sotto le separano:
- **(X) Causa legata all'indice di vista R 0..3:** stato per le viste 0..3 configurato per il 4P (time-slicing, cache, voci per
  frame), con effetti collaterali nella GPU sulle viste disegnate dopo.
- **(Y) Causa legata al layer o allo slot RLM:** flag `+0xde4`, `+0x1089`, `+0x81`, `+0x83`, oppure la config di slot
  `mgr+0x9bc..` delle Sub, che attiva nel setup del target un ramo (bit1/bit3 di 0x6f86e0) che lascia legato o scritto un target
  condiviso.

### 11.3 M7 – Confronto dello stato Sub ed extra (solo log, primo passo)
In `proto::PerFrame`, dopo il blocco delle camere e solo con `view8::split`, al frame 300 e poi ogni 1800:
```cpp
if (R && (frames == 300 || frames % 1800 == 0)) {
    uint8_t** s3 = *reinterpret_cast<uint8_t***>(m + 0x128);
    int nv = *(int32_t*)(R + 0xf0); uint8_t* views = *(uint8_t**)(R + 0xf8);
    for (int i = 0; i < 8; i++) {
        uint8_t* ly = i < 4 ? s3[i] : layer[i - 4]; if (!ly) continue;
        int v = (int8_t)ly[0x1088];
        uint8_t* vw = (v >= 0 && v < nv && views) ? views + v * 0x3e0 : nullptr;
        uint8_t* g = *(uint8_t**)(R + 0x4230); g = (g && v >= 0) ? g + v * 0x410 : nullptr;
        uint8_t* sl = i < 4 ? m + 0x9bc + i * 0x20 : nullptr;
        Logging.Log("[mk8dx-8p] 6d M7 %s%d ly=%p vista=%d fl78=%x 80..83=%02x %02x %02x %02x clr8c=%08x de4=%04x 1089=%02x 218=%p c10=%p"
                    " | view388=%02x view390=%p | g408/409=%02x/%02x | slot vp=%d l3d=%d rect=%d 9d4..9d7=%02x %02x %02x %02x",
                    i < 4 ? "Sub" : "Ext", i, ly, v, *(uint32_t*)(ly + 0x78), ly[0x80], ly[0x81], ly[0x82], ly[0x83], *(uint32_t*)(ly + 0x8c),
                    *(uint16_t*)(ly + 0xde4), ly[0x1089], *(void**)(ly + 0x218), *(void**)(ly + 0xc10),
                    vw ? vw[0x388] : 0xee, vw ? *(void**)(vw + 0x390) : nullptr, g ? g[0x408] : 0xee, g ? g[0x409] : 0xee,
                    sl ? *(int32_t*)sl : -1, sl ? *(int32_t*)(sl + 4) : -1, sl ? *(int32_t*)(sl + 0x14) : -1,
                    sl ? sl[0x18] : 0, sl ? sl[0x19] : 0, sl ? sl[0x1a] : 0, sl ? sl[0x1b] : 0);
    }
    uint8_t* F = R + 0x3fd0;                                   /* x20 di 0x6b5334 */
    Logging.Log("[mk8dx-8p] 6d M7 R: F+d18=%02x F+8e8=%02x F+a40=%.3f F+320=%p R+4239=%02x R+5d01=%02x | modo=%d", F[0xd18], F[0x8e8],
                *(float*)(F + 0xa40), *(void**)(F + 0x320), R[0x4239], R[0x5d01], *(int32_t*)(m + 0xbd8));
}
```
Gli offset 0xd18/0x8e8/0xa40/0x320 sono relativi a `F = R+0x3fd0` (registro x20 in 0x6b5334, chiamata come in step6b §2a)
[I: che x0 di 0x6b5334 sia R+0x3fd0 è dedotto dalla catena FinalImage di step6b].

Lettura: ogni campo che differisce tra Sub ed extra (in particolare `de4`, `1089`, `81`, `83`/`8c` e `9d4..9d7`) è un candidato per
T7. Con `vista` si vede anche se l'assegnazione è davvero Sub = 0..3, extra = 4..7.

### 11.4 T6 – Sub spente in S1 (separa "Sub sorgente" da "viste dopo")
In `proto::PerFrame`, dopo il blocco H6b-A (righe `if (active) {...}`):
```cpp
#ifndef MK8DX_6D_SUBOFF
#define MK8DX_6D_SUBOFF 0   /* T6: 1 = 3D Sub0..3 spenti in gara a 8 (diagnosi) */
#endif
if (MK8DX_6D_SUBOFF && active) {
    uint8_t** s3 = *reinterpret_cast<uint8_t***>(m + 0x128);
    for (int i = 0; i < 4; i++) if (s3 && s3[i]) mk8dx::Fn<EnableFn>(0x6651a8)(s3[i], false);   /* 0x3cd194 le riaccende al frame dopo */
}
```
Esito atteso in S1 (`MK8DX_HALF8=1 MK8DX_6D_SUBOFF=1`): P1-P4 nere o ferme. Poi:
- **P5-P8 tutte corrette:** la contaminazione nasce nel disegno delle Sub. Passare a T7/T8.
- **P5-P7 ancora magenta:** la contaminazione non viene dal disegno delle Sub ma da qualcosa che gira comunque per le viste 0..3
  di R (calc, GPUCalc, time-slicing): pista (X).

Nota: PerFrame gira dopo 0x3cc218, che contiene 0x3cd194, e prima della costruzione dei job del frame. Se il bit 0 di `+0x78`
non resta spento fino al disegno, spostare la scrittura in Layer3DPrioHook (0x68d474, chiamata alla costruzione dei job, §2 di
step2_draworder).

### 11.5 T8 – Scambio degli indici di vista R (separa X da Y), costo medio
In RlmAttachHook, dopo il ciclo che collega le extra, con un'opzione: si staccano le Sub, si collegano prima le extra (prendono le
viste 0..3) e poi si ricollegano le Sub (4..7). Il setter 0x68d4f4 assegna la prima vista libera (0x6ab220); con `nullptr` stacca
e libera la vista (0x6ab2d0).
```cpp
#ifndef MK8DX_6D_SWAPVIEW
#define MK8DX_6D_SWAPVIEW 0   /* T8: 1 = extra sulle viste R 0..3, Sub0..3 sulle viste 4..7 (diagnosi, solo gara con split) */
#endif
/* in RlmAttachHook::Callback, dopo il for che collega proto::layer[j]: */
if (MK8DX_6D_SWAPVIEW && r && n >= 8 && view8::split && rlm::sceneType == 3) {
    uint8_t** s3 = *reinterpret_cast<uint8_t***>(mgr + 0x128);
    auto set = mk8dx::Fn<proto::RSetFn>(0x68d4f4);
    for (int i = 0; i < 4; i++) set(s3[i], nullptr);
    for (int j = 0; j < proto::Extra; j++) set(proto::layer[j], nullptr);
    for (int j = 0; j < proto::Extra; j++) Logging.Log("[mk8dx-8p] 6d T8: extra %d -> vista %d", j, (int)set(proto::layer[j], r));
    for (int i = 0; i < 4; i++)          Logging.Log("[mk8dx-8p] 6d T8: Sub%d -> vista %d", i, (int)set(s3[i], r));
}
```
Lettura in S1:
- **Le magenta seguono gli indici 0..3**, cioè le celle P5-P8 diventano sbagliate e le Sub vengono corrette (o almeno quelle
  disegnate prima): pista (X), stato di R per le viste 0..3. Prossimo passo: confrontare con M7 le voci per vista di R
  (`view+0x388`, gruppi, FinalImage) tra 0..3 e 4..7.
- **Le magenta restano sulle Sub:** pista (Y), flag del layer o slot RLM. Prossimo passo: T7.

Rischio: il gioco può assumere "vista i = locale i" (camere CPU, 0x87e538, DLM). È solo diagnosi; un crash va trattato come esito
non valido, non come prova.

### 11.6 T7 – Flag delle Sub resi uguali a quelli delle extra (pista Y)
Dopo M7, per ciascun campo che differisce, una build sola che lo forza sulle Sub ogni frame in PerFrame (dopo Orig di 0x3cc218,
che riscrive `+0xde4`):
```cpp
#ifndef MK8DX_6D_SUBFLAGS
#define MK8DX_6D_SUBFLAGS 0   /* T7: bit 1 = de4 bit 0x4 a 0; bit 2 = 1089 a 0; bit 4 = 83 a 0; bit 8 = 81 a 0xff */
#endif
if (MK8DX_6D_SUBFLAGS && active) {
    uint8_t** s3 = *reinterpret_cast<uint8_t***>(m + 0x128);
    for (int i = 0; i < 4; i++) { uint8_t* ly = s3[i]; if (!ly) continue;
        if (MK8DX_6D_SUBFLAGS & 1) *(uint16_t*)(ly + 0xde4) &= ~0x4;
        if (MK8DX_6D_SUBFLAGS & 2) ly[0x1089] = 0;
        if (MK8DX_6D_SUBFLAGS & 4) ly[0x83] = 0;
        if (MK8DX_6D_SUBFLAGS & 8) ly[0x81] = 0xff; }
}
```
I valori di destinazione vanno presi dalle extra misurate con M7. L'esempio sopra assume extra con `de4` bit 0x4 = 0, `1089 = 0`,
`83 = 0`, `81 = 0xff`. Con COPY_DE4=1 le extra copiano il bit 0x4 da Sub0, quindi per il bit 0x4 provare anche `MK8DX_COPY_DE4=0`.

### 11.7 Probabilità aggiornate e ordine
| pista | probabilità |
|---|---|
| (X) stato di R per le viste 0..3 tarato sul 4P (time-slicing, cache, voci per frame) | 40% |
| (Y) flag del layer o slot RLM delle Sub che attivano un ramo del setup target con effetto sulle viste successive | 35% |
| Oggetti di post-processing a stato unico (§7.6) | 10% |
| Altro | 15% |

A2' (T3), B1 e C sono esclusi. Ordine delle prove:
1. **M7** e **T6**, nella stessa build S1.
2. **T8**.
3. **T7**, sui campi indicati da M7.

### 11.8 Punti incerti
- Il ramo bit1/bit3 di 0x6f86e0 non è decodificato.
- Il motivo per cui P8 si salva in S1 è aperto (posizione nel segmento o nel core, oppure fine dell'effetto).
- Gli scrittori di `layer+0x1089` per i Layer3D non sono stati trovati.
- Nessuna correzione "fedele al flusso" è proponibile prima di M7/T6/T8.

Verifica: `root_verify.py` -> **145 OK, 0 FAIL** (27 controlli aggiunti per §11).

## 12. Decodifica di view+0x388 (dopo M7) e prova T9

### 12.1 Avvertenza su M7
Le righe di M7 sono state registrate durante il **caricamento**, non in gara (T6 bloccava la partenza). In quella fase le Sub sono
spente (`fl78=c204c2`, bit0 = 0) e le extra no. Quindi le differenze `0x32` / `0x33` / `0x37` descrivono il caricamento, non la
gara. Le regole statiche qui sotto dicono comunque come sarebbero in gara.

### 12.2 Bit di view+0x388 (vista di R, passo 0x3e0) [V]
| bit | significato | scrittore | lettori principali |
|---|---|---|---|
| init | u16 `0x388/0x389 = 0x130` (0x388 = 0x30: bit4 e bit5 a 1; 0x389 = 0x01) | 0x6bc8d4/0x6bc8d8 (ctor vista) | – |
| bit1 (0x02) | "vista calcolata in questo frame": ogni frame `&= ~7; |= 2` | 0x6bcd44-0x6bcd50 (0x6bcd30, dal calc di R 0x6aba34) | – |
| bit0 (0x01) + bit3 (0x08) | **vista attiva** e "immagine finale da produrre": `|= 9` se layer collegato, `+0x82` != 0, non in pausa (de4 bit5 + `+0x218`) e abilitata (`g.408 != 1`) | 0x6bcdf4/0x6bcdfc | bit0: calc e disegno (0x6aba3c, 0x6ad94c, 0x6ae8dc, ...); bit3: 0x6af778 (FinalImage), azzerato a 0x6af7e4 |
| **bit2 (0x04)** | **vista principale del frame: una sola** | 0x6abd40-0x6abd6c (calc di R) | 0x6adad0 (0x6d084c), **0x6b56b4** (post-processing `[F+0x2a0]` 0x640310, solo sulla principale), **0x6cf15c** (0x66972c su `R+0x4d00+0x450`, solo la principale), **0x6cf274** -> 0x66a380 (la principale elabora tutti gli oggetti; le altre saltano quelli con `+0xa10 == 2`), 0x6cf5b8, 0x6b5420 (bit3 dei flag del target) |
| bit4 / bit5 | dall'init (0x30) | – | 0x6b3e34/0x6b40c8 (bit4), 0x6cbf0c (bit5) |

**Scelta della vista principale [V]** (0x6aba60-0x6abab4, con `[x25]` inizializzato a 0xff):
- In ordine di indice 0..n-1, tra le viste **attive**, diventa principale la **prima**.
- Una successiva la sostituisce solo se il suo layer ha `+0x1089` bit0 e uno slot (`layer+0x84`) maggiore o uguale a quello della
  principale corrente (0x6aba74-0x6aba8c). Con bit0, il riferimento diventa 0x80000000 (0x6abab0) e nessun'altra la sostituisce.
- Il bit2 viene messo solo su quella vista (0x6abd68). `layer+0x1089` è 0 per tutti i Layer3D (ctor 0x68d160), quindi:
  - 4P originale: principale = vista 0 nei frame pari, vista 2 nei dispari (la prima delle 2 attive);
  - gara a 8, S1 e S0: **principale = vista 0 (Sub0)** in tutti i frame, o vista 2 nei frame dispari di S0;
  - caricamento con Sub spente: principale = vista 4 (Ext4). È il `0x37` di M7, e spiega perché Ext4/P5 si comportava in modo
    diverso anche per le monete (lavoro "per frame" fatto dalla vista principale).

### 12.3 Collegamento con T5 [I, forte]
La vista principale fa nel **proprio** job Draw (la propria banda di id) un lavoro condiviso per il frame:
- 0x640310 sull'oggetto di post-processing `[F+0x2a0]` (0x1a30 B, init 0x640078);
- 0x66972c e gli oggetti di tipo 2 della lista di `R+0x4d00+0x450` (oggetti per vista da 0xbc0 B in 0x68881c);
- il ramo di 0x6d084c.

T5 coincide con "tutto ciò che la GPU disegna **dalla banda della vista principale in poi** è sbagliato":
- S1 normale: Sub0 (principale), Sub1..3 ed Ext4..6 sbagliate; Ext7, l'ultima, corretta.
- T5 (extra prima): Ext4..7 corrette (prima della principale), poi Sub0 (principale) e Sub1..3 sbagliate.
- S0: nei frame dispari la principale è la vista 2. Sub2/Sub3 (P3/P4) e le extra disegnate dopo sono a rischio; P3/P4 sono
  proprio le celle bianche osservate.

Non spiegato:
- Perché la vista disegnata per ultima si salva (Ext7 in S1, solo flash rari in T5). Ipotesi: il lavoro della principale lascia
  sporco qualcosa che si sistema nella banda successiva oppure scade.
- Perché nel 4P originale la vista disegnata dopo la principale (1 o 3) non viene contaminata. Ipotesi: il lavoro condiviso dipende
  dal numero di viste attive o di viste di R (8 al posto di 2/4); oppure, con 2 viste in parallelo su 2 core, il riuso non avviene
  nello stesso modo.

### 12.4 T9 – Interruttore: spostare la vista principale sull'ultima vista disegnata
Si usa il meccanismo del gioco stesso (`layer+0x1089` bit0, l'unico lettore del bit0 è la selezione 0x6aba84/0x6abaa0). Va in
`proto::PerFrame`, ogni frame, dopo il blocco delle camere:
```cpp
#ifndef MK8DX_6D_MAINVIEW
#define MK8DX_6D_MAINVIEW -1   /* T9: indice dell'extra (0..3) che diventa vista principale di R (layer+0x1089 bit0); -1 = gioco */
#endif
if (MK8DX_6D_MAINVIEW >= 0 && active)
    for (int j = 0; j < Used(); j++) {
        uint8_t& f = layer[j][0x1089];                         /* bit0 letto solo da 0x6aba88/0x6abaa0 (selezione) */
        f = (j == MK8DX_6D_MAINVIEW) ? (f | 1) : (f & ~1);
    }
```
Prove in S1 (`MK8DX_HALF8=1`):
- **T9a `MK8DX_6D_MAINVIEW=3`** (principale = Ext7, l'ultima disegnata). Atteso con la pista "vista principale": P1-P7 corrette,
  al più P8 sbagliata.
- **T9b `MK8DX_6D_MAINVIEW=0`** (principale = Ext4, ordine normale). Atteso: P1-P4 corrette, P5..P7 sbagliate dalla banda di
  Ext4 in poi.
- Se nessuna delle due cambia qualcosa, la pista è esclusa: la contaminazione non è legata alla vista principale.

Effetti collaterali [I]: il lavoro condiviso del frame (probe e luci della lista `R+0x4d00`, post `[F+0x2a0]`) verrebbe prodotto
dopo le altre viste, quindi le altre vedono il dato del frame precedente: un frame di ritardo, accettabile per la diagnosi. Il bit
resta anche fuori gara, ma `active` lo limita alla gara con split.

### 12.5 M8 – Conferma in gara (solo log)
In `proto::PerFrame`, con `active && on3` (Sub0 accesa, quindi in gara e non in caricamento), ogni 600 frame:
```cpp
{
    int nv = *(int32_t*)(R + 0xf0); uint8_t* vws = *(uint8_t**)(R + 0xf8);
    char b[200]; int q = 0, mainIdx = -1;
    for (int v = 0; v < nv && v < 8 && vws; v++) {
        uint8_t f = vws[v * 0x3e0 + 0x388];
        if (f & 4) mainIdx = v;
        q += snprintf(b + q, sizeof b - q, "%d:%02x ", v, f);
    }
    Logging.Log("[mk8dx-8p] 6d M8 view388 %s| principale=%d", b, mainIdx);
}
```
Atteso in S1 senza T9: principale = 0 e tutte le viste `0x3b` (0x30|0x8|0x2|0x1, con bit3 ancora a 1 se il log arriva prima
del FinalImage) oppure `0x33`, e la vista 0 con il bit 0x04. Con T9a: principale = 7.

### 12.6 Correzione, se T9a conferma
La regola del gioco ("principale = prima vista attiva") con 8 viste mette il lavoro condiviso nella banda della Sub0, prima delle
altre 7. La correzione minima è dare il bit0 di `+0x1089` all'ultima vista disegnata (l'ultimo extra in uso, `Used()-1`) solo nella
gara a 8: è la stessa leva che il gioco offre per scegliere la principale, nessuna patch di codice.

Prima di renderla definitiva va capito perché il lavoro della principale sporca le viste successive. Il prossimo passo statico è
0x640310 (`[F+0x2a0]`) e 0x66972c, alla ricerca di risorse indicizzate per vista o con un conteggio legato a 4.

### 12.7 Probabilità aggiornate
| pista | probabilità |
|---|---|
| Lavoro "vista principale" (bit2) che contamina la propria banda e le successive | 45% |
| (X) altro stato di R per le viste 0..3 | 20% |
| (Y) flag del layer: M7 mostra layer identici, quindi meno probabile | 10% |
| Altro | 25% |

Verifica: `root_verify.py` -> **176 OK, 0 FAIL** (31 controlli aggiunti per §12).

## 13. Esito T9a in Eden (4 nov 2026, sera)
S1 (`MK8DX_HALF8=1`) + `MK8DX_6D_MAINVIEW=3`: M8 in gara `0:33 1:33 2:33 3:33 4:33 5:33 6:33 7:37 | principale=7` (la leva layer+0x1089
funziona). A video: tutte le celle flashano a intermittenza (non piu' magenta fisse), P5 di rado. Confronto S1 senza T9: P1-P7 magenta
fisse, P8 corretta. => la vista principale influisce (da corruzione fissa a intermittente) ma non e' la causa unica.
Prossimo passo statico: cosa fa il lavoro condiviso della vista principale (0x640310 su [F+0x2a0], 0x66972c su R+0x4d00+0x450, oggetti di
tipo 2 in 0x66a380, ramo 0x6d084c) e quale risorsa resta condivisa fra le viste; prove §7.6 (effetti spenti uno alla volta).

## 14. T10: lavoro della vista principale ed effetti spenti (4 nov 2026) [V statico + D runtime]
Statica: la vista principale (view+0x388 bit2) fa da sola 0x640310 (post `[F+0x2a0]`, uniform inline + DrawElements), 0x6d084c
(`[R+0x4d00+0x418]`, cfg+0x560), 0x66972c (luci `[E+0x450]`) e gli oggetti di tipo 2 in 0x66a380. Tutti gli array per vista sono creati
con numViews: FinalImage `[F+0x248]` (0x3d68), luci (+0x178/+0x180, voci 0xbc0, da 0x685a50 con count = numViews), post (+0x16f8, 0x1c8),
Bloom `[F+0x280]` (base effetto 0xacda48: +0xc = n, +0x10 = buffer, +0x18 maschera; voci +0x588 da 0xf58).
Opzione `MK8DX_6D_FXOFF` (bitmask, default 0) e misura M9 (solo log):
| prova | risultato |
|---|---|
| M9 | nv=8, FinalImage 8, post 16f8=0 e +0x218=0 (in gara 0x640310 non fa nulla), Bloom c=8 buffer=8, 18 luci da 8 voci tutte di tipo 2, `[R+0x3c60]` = 0 (offset AE di step6c sbagliato) |
| S1 + FXOFF=0x3f / 0x22 | gara ferma nella schermata di caricamento (5,9 fps sotto): causa 0x6d084c saltata |
| S1 + FXOFF=0x20 (Bloom spento) | **tutte le 8 celle corrette** |
| build normale + Bloom spento | niente bianco/magenta, restano flash **neri** |

## 15. Causa: anello dei descrittori texture esaurito in gara a 8 (4 nov 2026) [V statico + V runtime]
Meccanismo [V statico]:
- Device grafico, ctor 0x6044a8(dev, cfg): `dev+0xb0 = [cfg+8]` = descrittori texture, `dev+0xb4 = 0x1000` sampler (0x6044f8); il pool
  di memoria dei descrittori è dimensionato da questi (0x604640) e inizializzato a 0x604710 (nvnTexturePoolInitialize). Chiamato da 0xa83fbc
  con `[cfg+8] = [sys+0xe4]`.
- Anello dinamico (0x628a88-0x628b4c): base = `0x60476c(dev)` (descrittori statici, 256), dimensione = potenza di 2 <= `dev+0xb0 - base`.
- Registrazione 0x6290bc (nvnTextureBuilderSetStorage/nvnTextureInitialize nell'acquire del pool 0x64b558 -> 0x634a44, RegisterTexture a
  0x6291a8): cursore atomico `+0x194`; prima di occupare lo slot `i` libera lo slot `i + dim/2` se ha riferimenti a 0 (0x62914c); se non
  trova slot liberi in un giro intero restituisce **0** (0x629204). AddRef/Release: 0x62922c / 0x629264.
- Con id 0 la texture (temporanei del pool: Bloom e post-processing) viene campionata con il descrittore 0: celle bianche/magenta, o nere
  con meno effetti.

Misure M10 (hook di solo log su 0x6290bc, contatori per frame in SegFrameHook) e T11 (hook sul ctor 0x6044a8):
| condizione | descrittori del gioco | anello | in uso (+0x19c) | registrazioni/frame | falliti |
|---|---|---|---|---|---|
| gara 4P | 8492 | 8192 | ~7400 | ~24 | 0 |
| gara 8P | 8492 | 8192 | ~8180 (pieno) | ~100 | 3447 -> 15794 (~15/s) |
| gara 8P, `MK8DX_6D_TEXDESC=16640` | 16640 | 16384 | ~8250 | ~107 | **0** |
Verifica dell'utente con 16640 (build normale, Bloom acceso): nessun difetto grafico in una gara a 8.

Spiega tutti i dati precedenti: cresce con le viste e i modelli (8P: 1461 modelli contro 669), sensibile alle allocazioni d'avvio
(numero di texture vive), "solo l'ultima vista corretta" in S1 (i temporanei registrati per ultimi trovano gli slot appena liberati),
T3 inutile (non è la memoria), Bloom spento = meno registrazioni.

Correzione (flusso del gioco, limite esteso): `GfxDeviceCtorHook` 0x6044a8 alza `[cfg+8]` a **16640** prima della creazione
(`MK8DX_6D_TEXDESC`, default 16640, 0 = valore del gioco); pool di memoria e anello si dimensionano dal codice originale. Costo: circa
+256 KiB di memoria per i descrittori. Attivo sempre (anche 1-4P: solo un anello più grande).
Da fare: regressione 1P/4P, gara a 8 completa, rimuovere/ridurre le opzioni diagnostiche 6d se non servono più.
