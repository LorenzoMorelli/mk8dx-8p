# Step 6 – Piano C: perché i 4 layer 3D extra coprono il 2D (main 4.0.1)

Analisi statica del disassemblato (offset = main decompresso `../img/main.bin`). Nessuna modifica al gioco.
Estratti: `step6_data/c_de4_writers.asm`, `c_de4_readers.asm`, `c_layer_fields.asm`, `c_jobs.asm`,
`c_draw_target.asm`, `c_pause_capture.asm`. Etichette: [VERIFICATO] = letto nel codice, [IPOTESI],
[DA VERIFICARE A RUNTIME].

## 0. Risposta breve
- **Nessun campo del layer spiega la differenza** [VERIFICATO]. I 3D Sub del gioco e i nostri hanno lo stesso
  ctor (0x68d0e4 → 0x68c458 → 0x66446c), la stessa vtable (0x12b4fb8), lo stesso display (argomento w4 = 0 di
  0x667a94 in tutti i 14 siti del ctor RLM e nel nostro), `+0x83 = 0`, `+0x1f0 = 0`, `+0x218 = 0`,
  `+0xde4 = 0x120` di base. Per frame il gioco tocca sui Sub solo **il bit 0x4 di +0xde4** (0x3cd1f8), che finisce
  come flag 2 nell'avvio della vista di R (0x6b53b0 → 0x6f86e0): influisce sul rendering *dentro* la cella, non
  sull'ordine rispetto al 2D.
- **Tutti i layer (3D e 2D) disegnano nello stesso render buffer del display** (`display+0x10`, passato come
  `info+0x10` da 0x683f94/0x683fac). L'ipotesi "i Sub disegnano in un color buffer di scena che il display compone
  con il 2D a fine frame" è **falsa** [VERIFICATO]: R usa buffer intermedi per vista (`[R+0x3fd0+0x248]`, voci da
  0x3d68 B) ma li compone nel render buffer del display allo step FinalImage (0x6af7f8 → 0x6b55a8) **dentro il job
  Draw del layer stesso**. Begin/End del display non compongono la scena.
- Quindi l'ordine visivo dipende solo da **quando i comandi GPU del job Draw di ciascun layer finiscono nel flusso
  GPU**. Qui c'è l'unica differenza strutturale tra Sub e extra: i 3D (prio > 10) vanno nella coda 0 (ls+0x1e8)
  eseguita **in parallelo su 2 core**, e **contemporaneamente** alla coda 1 del 2D eseguita su un terzo core
  (0x704100-0x704320: le tre code sono postate una dopo l'altra, l'attesa 0x62222c/0x6223f0 è solo alla fine,
  0x704374). I job estratti uno alla volta (granularità ls+0x57c = 1, 0x6667a0; presa a blocchi 0x621a80) in ordine
  di priorità decrescente: prima i 4 Draw dei Sub (602), poi i 4 SubDraw (601), **poi i nostri** (600/599).
  [IPOTESI forte] I nostri job, ultimi della coda 0, chiudono il loro segmento di comandi dopo che il core del 2D
  ha già emesso il 2D Main/Front/Overlay: per questo stanno sopra a tutto il 2D, in gara (minimappa) come in pausa
  e nei risultati. Il meccanismo esatto di concatenazione dei command buffer dei core non è leggibile staticamente
  (`ctx+0xc0 = [[0x1308210]+0x48]`, 0x626558) → **da confermare con il log dei job (§6, L2)**.
- **Proposta minima, indipendente dal dettaglio GPU** (§5A): non lasciare ai layer extra job propri; disegnarli
  **dentro il job Draw di 3D Sub0**, subito dopo il Sub0, con un hook trampolino su **0x683ecc**. Così ereditano
  core, segmento di comandi e posizione rispetto al 2D del Sub0 (che è corretta). Restano registrati e abilitati
  (servono per +0x82/vista R attiva e per il job GPUCalc).

## 1. Semantica di layer+0xde4 e +0x78..+0x83
### 1.1 +0xde4 (u16) [VERIFICATO salvo dove indicato]
| bit | chi scrive | chi legge | significato |
|---|---|---|---|
| 0x2 | 0x703738 (sistema, modo cattura +0x560==1): azzerato e poi messo per i layer con **indice slot < slot del layer di cattura** del loro display (0x70391c-0x703930); per i non-Layer3D anche da vtbl[0x80]/+0x548 bit6 (0x703970-0x7039a8) | `vtbl[0x80]` = 0x3c949c (Layer2D e Layer3D) → **0x665ffc isVisible** | **nascosto**: il layer non entra nella lista del display (0x667148), quindi niente job |
| 0x4 | **0x3cd194 (RLM per frame) solo sui 4 3D Sub**: `= mgr+0x9d6+i*0x20` (0x3cd1e0-0x3cd1f8); mgr+0x9d6 = tipo viewport 1 (0x3cbf58) | 0x6b5398 (R, avvio vista): `(de4>>1)&2` → flag 2 di 0x6f86e0 | opzione di rendering della vista R [IPOTESI: post-effetto pieno vs ridotto]; non tocca l'ordine |
| 0x8 | 0x7036ac/0x7036c8 (inizio cattura sui layer sys+0x468/+0x470) | 0x68cb58/0x68cb70 Layer::draw base | **cattura**: copia il render target nella texture `+0xc10` (0x64ae4c, 0x64b868) |
| 0x10 | 0x703674/0x703690 (fine cattura) | 0x68cac4 | **rilascia** `+0xc10`; se `+0xc10 && !0x10` la texture catturata viene ridisegnata ogni frame (0x68cab4-0x68cb54) |
| 0x8/0x10 | azzerati a fine draw (0x68ced0-0x68cedc) | | |
| 0x20 | ctor base (0x120, 0x68c530) | 0x68d478 (prio=5), 0x68d1ec (salta rndr_3D), 0x68cd88 (disegna texture `+0x218`), 0x6bcdb8, 0x6ab5c8 | **"pause texture"**: con `+0x218 != 0` il layer non rende il 3D ma disegna la texture +0x218 (stringa 0xefe35f). Sempre a 1: conta solo +0x218 |
| 0x40 | – | 0x68c938 Layer::calc → flag a 0x68da18 | formato del buffer proprio (+0x430) se `+0x1f0` [IPOTESI: con depth] |
| 0x80 | 0x7038f0-0x703904: `= sys+0x550 & 0x400` | – (non trovato) | [IPOTESI] stato pausa per i layer con 0x100 |
| 0x100 | ctor base (0x120) | 0x7038d8 | il layer partecipa al blocco/cattura di 0x703738 |

Nota: il gioco **non** mette mai `+0x218` sui 3D Sub nei siti RLM trovati: la pause texture va sul **2D Overlay
(slot 13)**, catturata al draw del **2D Front (slot 12)** (0x3cc774-0x3cc82c, 0x68e094: `layer+0x428 = cattura`;
copia in Layer::draw base 0x68cd58-0x68cd80). La pausa quindi = 2D Overlay che disegna a schermo intero
l'immagine catturata + menu. Gli extra visibili *sopra* questa immagine (screenshot pausa/"Prossima corsa")
dimostrano che i loro comandi arrivano dopo il job del 2D Overlay.

### 1.2 Byte +0x78..+0x83 [VERIFICATO]
| campo | init (0x66446c) | scrittori | significato |
|---|---|---|---|
| +0x78 u16 | 0x483 | 0x667b8c (`|=0x40` registrato), 0x66523c (`|=0x20` display cambiato), 0x6651d4 (bit0, 0x6651a8) / 0x665178 (bit1, 0x665144), 0x6652c4 (bit 0x10) | isVisible richiede `(+0x78&3)==3` e `!(+0x78&0x10)` |
| +0x7a u16 | 0xc2 | 0x68c76c (`&=~2`), Layer3D 0x68d414 (`|=2`) | uguale per Sub e extra |
| +0x80 s8 | 0 | **0x665240** `setDisplay` (unico chiamante 0x667ba0 con w4) | indice del display |
| +0x81 | 0xff | – (solo init) | display forzato (0xff = usa +0x80) |
| +0x82 | 0 | 0x666fe4 (azzera ogni frame), 0x667180 (`|= 1<<d` se nella lista del display d) | maschera dei display del frame; R la usa per attivare la vista (0x6bcdac, 0x6ab5bc) |
| +0x83 | 0 | 0 per i 2D nel ctor RLM (0x3cac08 e altri 6) | flag di clear (bit0 colore, bit2 depth) con +0x8c colore, +0x9c depth: 0x666050 (solo se non collegato a R), 0x6b5388 (R) |

### 1.3 Confronto Sub0 / layer extra (stato atteso)
| campo | 3D Sub0 (gioco) | extra (modulo) | effetto |
|---|---|---|---|
| ctor, vtable, size | 0x68d0e4, 0x12b4fb8, 0x1090 | uguali | – |
| 0x667a94: slot / display / heap | 2 / 0 / LayerHeap | 14..17 / 0 / nullptr | lo slot conta solo in 0x703738 (cattura) e nell'ordine della lista del display |
| +0x68/+0x70 | mgr+0x1d8 / mgr+0x238+i*0xc0 | camera/proiezione proprie | – |
| +0xde4 | 0x120 + bit 0x4 per frame | 0x120 (o copia di Sub0 con `MK8DX_COPY_DE4`) | solo rendering interno |
| +0x83, +0x1f0, +0x218, +0x81 | 0, 0, 0, 0xff | uguali | – |
| +0x1020 / +0x1088 | R / 0..3 | R / 4..7 | indice vista R |
| priorità +0x88 | 602 (o 5) | 600 (Layer3DPrioHook) | stesso bucket 0 |
| posizione nella coda 0 | job 0..7 | **job 8..15 (ultimi)** | **unica differenza di esecuzione** |

## 2. Code, job ed esecuzione [VERIFICATO salvo dove indicato]
```
0x667358 (da 0x7040f0): per ogni display d nella maschera
   0x6844b8: per layer della lista del display (+0xcc0, ordine slot): vtbl[0x90] (prio), job Draw (prio p),
             job SubDraw (p-1); poi Begin(+0xcd0[0], tipo 2) ed End(+0xcd0[1], tipo 3) con prio minP-1
   bucket: p > ls+0x50c (10) -> 0;  ls+0x510 (0) < p <= 10 -> 1;  p <= 0 -> 2;  GPUCalc (0x6845a0) -> 3
   heapsort 0x5fb66c con cmp 0x6810b0 = b.prio - a.prio  => priorità DECRESCENTE (non stabile)
   push in ls+0x1e8 / +0x2b0 / +0x378 / +0x440 (sead FixedSizeJQ, push 0x62176c)
0x703bdc (stesso chiamante, 0x704100-0x704374), se sys+0x548 bit 22 e esecutore sys+0x5a0:
   maschera core coda 0 = tutti i core tranne c = sys+0x514  (c=0 -> {1,2}, 1 -> {0,2}, 2 -> {0,1})
   0x62211c(esecutore, coda0, maschera)   -> post ai worker (0x62616c), NON attende
   code 1 e 2: maschera = 1<<c (un solo core)          -> post, NON attende
   attesa di tutti i worker: 0x62222c / 0x6223f0 (0x704374)
   worker: 0x6219a4 prende blocchi contigui di `granularità` job (ls+0x57c = 1, 0x6667a0, 0x667238) e li esegue in
           ordine con vtbl[0] = 0x680f2c
0x680f2c: ctx 0x626558 (ctx+0xc0 = [[0x1308210]+0x48]); tipo 0 Draw 0x683ecc(display, ctx, layer, slot*258+0xffff),
          1 SubDraw 0x684148 (fa qualcosa solo se display+0xcb0 ha 0x840), 2 Begin 0x6839b0 (delegate display+0xff8,
          0x683a48), 3 End 0x684170 (delegate display+0x1000), 4 GPUCalc 0x6841d4
```
- Coda 3 (GPUCalc, ls+0x440) non è eseguita in 0x703bdc; il consumatore non è stato individuato [DA VERIFICARE].
- In gara: 3D Sub e extra (≥ 599) in coda 0, 2D (10/9) e Begin/End (≥ 4) in coda 1. Un Layer3D con prio 5
  (ramo `g[0x408]==1` o pause texture) finisce in coda 1 dopo il 2D in ordine di priorità.
- **Perché un Layer3D con prio 600 e bucket 0 può finire sopra un Layer2D con prio 10**: la priorità ordina i job
  solo *dentro* una coda, e le code 0 e 1 girano **contemporaneamente su core diversi** senza barriera tra loro.
  L'ordine finale dei comandi GPU tra le due code dipende da come i comandi registrati dai core vengono
  concatenati/sottomessi (non visibile staticamente: un unico oggetto comandi globale `[0x1308210]+0x48`, display
  con buffer di comandi propri a display+0xd08+n*0x160, 0x684764). [IPOTESI forte] i job in coda alla coda 0 (i
  nostri, 8..15) vengono emessi dopo il 2D. Indizio coerente: prima del Layer3DPrioHook gli extra (prio 5, coda 1,
  ordinati dopo il 2D) erano sopra il 2D, e lo sono rimasti con prio 600 (fondo della coda 0).
- Indizio contrario da chiarire: i dump `layers:` di 6b/6d mostrano 3D Sub0/Sub1 con prio 5 (coda 1, dopo il 2D)
  mentre lo screenshot 6d mostra il loro HUD sopra. O il valore del dump non è quello del frame disegnato (il ramo
  `g[0x408]==1` di 0x68d4c0 può cambiare tra frame), o l'ordine della coda non decide da solo. Il log L2 (§6) lo
  chiarisce.

## 3. Render target: "scena + composizione" contro "diretto nel framebuffer"
- **Layer3D** (draw = vtbl[0x40] 0x68c9ac + render step 0x683c5c → DrawMethod 0x68d1d8):
  `info = 0x667d10(ctx, display+0x8, display+0x10, layer)` (0x683f88-0x683fac), viewport `layer+0x48`
  (0x61e104, 0x683fc0). CalcView: render buffer = `layer+0x1f0 ? layer+0x450 : info+0x10` (0x68d248-0x68d270).
  Con `+0x1f0 = 0` (Sub ed extra) è il **buffer del display**. R rende la vista nei buffer intermedi
  `[R+0x3fd0+0x248][vista]` (0x6f86e0 alloca i color buffer temporanei, 0x64a59c) e allo step 9 FinalImage
  (0x68d2c4 → 0x6af7f8 → 0x6b55a8) li ricopia sul target (0x6b5974-0x6b59d8) nella viewport del layer.
  Lo step CalcGPU (1) è escluso dal Draw (`vtbl[0x70]` = 0x68d700) e gira nel job GPUCalc.
- **Layer2D** (vtable 0x1263700): stesso 0x683ecc, stesso `info+0x10`, draw base 0x68c9ac (+0x218 pause
  texture, +0x428 cattura), poi il disegno Lyt (vtbl[0xa8]).
- `+0x1f0 != 0` (buffer proprio a +0x430/+0x450) lo usa solo un modo speciale RLM su 3D Sub0 (0x3ce054-0x3ce078,
  con link 0x68c988 al layer +0x200/+0x208), non la gara normale.
- **Conclusione [VERIFICATO]**: niente composizione "scena poi 2D" nel display: ogni layer scrive nel buffer del
  display durante il proprio job; l'ordine visivo = ordine dei comandi GPU dei job. Bit 1 di +0xde4 = nascosto
  (niente job); bit 5 = sostituisce il 3D con la texture +0x218 (usato dal gioco per il 2D Overlay in pausa).
- [DA VERIFICARE A RUNTIME] `[[R+0x3fd0+0x248]+0x8]` (n. voci per vista): ogni accesso fa `csel` sulla voce 0 se
  la vista è fuori range (0x6b5368, 0x6b55ec, 0x6f8728). Se fosse 4, le viste 4..7 condividerebbero i buffer
  intermedi della vista 0 (rischio di corse con Sub0 su un altro core; non spiega però la copertura del 2D).

## 4. Cosa resta e cosa è escluso
| ipotesi | stato |
|---|---|
| campi diversi (+0x80..+0x83, +0x1f0, +0x218, +0x81, vtable, display) | **esclusa** [VERIFICATO] |
| priorità/bucket diversi | esclusa: stesso bucket 0 (log `prio 3D Sub4: orig=602 sub0=602 -> 600`) |
| composizione scena + 2D nel display End | **esclusa** [VERIFICATO] |
| bit 0x4 di +0xde4 | non riguarda l'ordine; copiarlo è corretto (§5B) |
| esecuzione: extra ultimi della coda 0, che gira in parallelo al 2D | **aperta, la più probabile** → §5A, L2 |
| cattura pausa (0x703738) basata sull'indice slot | spiega perché in modo cattura gli extra (slot 14..17 > slot cattura) non vengono mai nascosti; secondaria |

## 5. Proposta
### 5A. Disegnare gli extra dentro il job Draw di 3D Sub0 (consigliata)
Hook trampolino su **0x683ecc** `void draw(uint8_t* display, void* ctx, uint8_t* layer, int id)`:
salta i job propri degli extra e li disegna subito dopo Sub0, nello stesso job (stesso ctx, core e segmento di
comandi, quindi stessa posizione rispetto al 2D del Sub0). La chiamata annidata va a `Orig`, quindi non rientra
nella callback.
```cpp
/* Step 6C: gli extra seguono il job Draw di 3D Sub0 (report/step6_plan_C.md §5A) */
HOOK_DEFINE_TRAMPOLINE(LayerDrawJobHook) {
    static void Callback(uint8_t* disp, void* ctx, uint8_t* ly, int id) {
        if (proto::active)
            for (int j = 0; j < proto::Extra; j++)
                if (ly == proto::layer[j]) return;                 /* job proprio dell'extra: saltato */
        Orig(disp, ctx, ly, id);
        if (!proto::active || !proto::mgr || !proto::R) return;
        uint8_t* sub0 = (*reinterpret_cast<uint8_t***>(proto::mgr + 0x128))[0];
        if (ly != sub0) return;
        for (int j = 0; j < proto::Used(); j++) {
            uint8_t* ex = proto::layer[j];
            if (*(uint8_t**)(ex + 0x1020) != proto::R) continue;   /* non collegato */
            if (!(ex[0x82] & 1)) continue;                         /* non nella lista del display 0 in questo frame */
            Orig(disp, ctx, ex, *(int32_t*)(ex + 0x84) * 258 + 0xffff);
        }
    }
};
// main: LayerDrawJobHook::InstallAtOffset(0x683ecc);
```
- Gli extra restano registrati e abilitati: così `+0x82` viene messo da 0x666fc8 (R attiva la vista 4..7,
  0x6bcdac) e il loro job GPUCalc (step CalcGPU) continua a girare. Il job SubDraw (0x684148) non fa nulla senza
  display+0xcb0 & 0x840.
- In pausa/risultati gli extra vengono disegnati prima della cattura del 2D Front (sono nel job di Sub0) e quindi
  coperti dall'immagine del 2D Overlay come i Sub: comportamento identico al gioco senza altro codice. Se Sub0 è
  nascosto (bit 0x2), il suo job non esiste e gli extra non vengono disegnati: coerente.
- Costo: il job di Sub0 si allunga (gli extra non sono più in parallelo tra loro). Variante se servisse
  parallelismo: extra j nel job di Sub j (`ly == sub[j]`), solo dopo aver verificato `[[R+0x3fd0+0x248]+8] >= 8`.

### 5B. Pulizia di +0xde4 (insieme a 5A)
L'esperimento `MK8DX_COPY_DE4` copia tutta la parola: porta sugli extra anche 0x8/0x10 (cattura della texture
+0xc10, pensati solo per i layer sys+0x468/+0x470) e 0x2. Copiare solo il bit 0x4 (come 0x3cd194):
```cpp
uint16_t s = *(uint16_t*)(s0 + 0xde4), e = *(uint16_t*)(layer[j] + 0xde4);
*(uint16_t*)(layer[j] + 0xde4) = (e & ~0x4) | (s & 0x4);     /* opzione: anche (s & 0x2) per nascondere con Sub0 */
```

### 5C. Esperimenti di conferma (una build ciascuno, senza 5A)
1. **Scambio**: Layer3DPrioHook con `p + 2` (extra in testa alla coda 0). Se gli extra passano sotto il 2D (e magari
   qualche Sub sopra) la causa è la posizione nella coda 0.
2. **Granularità**: portare `ls+0x57c` (byte, 1 di default) a 16 per un frame di prova: un solo worker prende
   l'intera coda 0 in ordine. Se la copertura cambia, conferma che dipende dalla distribuzione tra i core. Effetto
   globale, solo diagnostica.
(Non serve spostare gli extra in coda 1: lì sarebbero ordinati dopo il 2D, come prima del Layer3DPrioHook.)

## 6. Cosa loggare
- **L1 campi** (ogni ~600 frame, già in parte nel log `draw:`): per Sub0 e extra0: `+0x78` u16, `+0x7a`,
  `+0x80..+0x83`, `+0x88`, `+0xde4`, `+0x1f0`, `+0x200`, `+0x208`, `+0x218`, `+0x428`, `+0xc10`, `+0x1088`,
  `+0x1089`, e `+0xb8/+0xc0/+0xc8` confrontati con `ls+0x58/0x68/0x78 + slot*0x38`; più `ls+0x50/+0x60/+0x70`
  (se ≤ 14 i job degli extra ricadono sulla voce 0: csel a 0x667c28) e `[[R+0x3fd0+0x248]+0x8]`.
- **L2 ordine dei job** (decisivo): hook su 0x680f2c che, per un solo frame (es. frame 600 dopo l'attivazione e il
  primo frame di pausa), salva in un buffer statico con indice atomico: sequenza, core
  (`svcGetCurrentProcessorNumber`), tick, tipo `[job+0x18]`, slot `[[job+0x20]+0x84]` (o -1 se display), prio
  `[job+0x30]`, coda di provenienza (dedotta da prio/soglie); stampa nel frame successivo dal thread principale.
  Atteso con la causa §2: extra Draw su un core diverso da quello del 2D e terminati (tick) dopo i Draw di 2D
  Main/Front/Overlay.
- **L3 sistema**: hook su 0x7034f8 (x0 = sys): `+0x514` (core delle code 1/2), `+0x548` (bit 15/22: esecutore
  multicore attivo), `+0x560` (modo cattura), slot di `[+0x478]`. Se il bit 22 è 0 le code non passano da 0x62211c
  e l'ipotesi §2 va rivista.
- **L4 display 0** (`[ls+0x38][0]`): `+0xcb0`, `+0xcb8` e ordine degli slot nella lista `+0xcc0`.

## 7. Non determinato staticamente
- Come i comandi GPU registrati dai worker di coda 0 e coda 1 vengono ordinati nella sottomissione (agl/sead
  DrawContext, 0x626558/0xa85b2c): da L2 + esito di 5A/5C1.
- Consumatore della coda GPUCalc (ls+0x440) e valore runtime di sys+0x514/+0x548.
- Perché il dump mostra Sub0/Sub1 a prio 5 senza effetti visibili (ramo `g[0x408]==1` variabile tra frame?).
