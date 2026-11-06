# Step 2E – Ordine di disegno dei layer (layerSystem / agl-gsys Renderer, main 4.0.1)

Analisi statica (offset = main decompresso). Nessuna modifica al gioco.

## 0. Risposta breve
- **L'ordine NON dipende dallo slot né dall'ordine di registrazione** [VERIFICATO]. Ogni frame il layerSystem
  raccoglie un job per layer, lo etichetta con la **priorità `layer+0x88`** e ordina i job per **priorità
  decrescente** (prima si disegna la priorità più alta). Lo slot (`layer+0x84`) serve solo come indice delle
  tabelle `ls+0x48` / `ls+0x58..0x78` (e come chiave del contatore GPU, 0x680f8c).
- Priorità:
  - **Layer3D** (vtable 0x12b4fb8): ricalcolata **a ogni frame** dalla virtuale `vtbl[0x90]` = **0x68d474**
    subito prima di leggere `+0x88` (0x6844f0) → `5` oppure `[R+0x55b0] + 100`.
  - **Layer2D del gioco** (vtable 0x1263700, ctor 0x3c8734): `vtbl[0x90]` = 0x3c94b0 = `ret`; la priorità resta
    quella del ctor base 0x68c458: **10** (`mov w8,#0xa; str w8,[x19,#0x88]` a 0x68c61c) [VERIFICATO: nessun altro
    scrittore di `+0x88` nel codice layer/manager 0x3c0000-0x3cffff, 0x660000-0x68dfff].
  - Quindi 3D (≥100) → 2D (10, SubDraw 9) → layer 3D con priorità 5. **I nostri layer finiscono dopo l'HUD
    perché `0x68d474` restituisce 5 per loro** [IPOTESI forte: è l'unico valore < 10 possibile]. Ramo esatto
    da confermare a runtime (§3).
- **Azione consigliata**: hook trampolino su **0x68d474** che, dopo Orig, per i nostri 4 layer copia in
  `+0x88` la priorità di 3D Sub0 (o Sub0−2 per stare sicuramente dopo i Sub e prima dei 2D). Cambiare slot,
  registrare prima o scrivere `+0x88` una volta sola **non serve** (viene riscritto ogni frame).

## 1. Il loop di disegno [VERIFICATO]
*(estratto di disassemblato del gioco rimosso)*
Job per layer (0x6844b8, display: `+0xcb8` count, `+0xcc0` ptr[] layer, riempito da 0x683808):
```
for layer in display.layers:
    layer->vtbl[0x90]()              (0x6844f0)  -> ricalcola +0x88 (Layer3D: 0x68d474; Layer2D: ret)
    p = layer+0x88
    0x680eec(layer+0xb8, display, p,   list)     job "Layer(Draw)"    (voce ls+0x58[slot], tipo 0)
    0x680eec(layer+0xc0, display, p-1, list)     job "Layer(SubDraw)" (voce ls+0x68[slot], tipo 1)
    minP = min(p)
job del display (+0xcd0, e +0xcd0+0x38 se +0xcc8>1) con priorità minP-1   ("Layer(Begin)/(End)")
```
`0x680eec(job, display, prio, list)`: `job+0x28 = display, job+0x30 = prio`, push in list.
Le voci job sono create in 0x667a94 → 0x680e30(ls+0x58/0x68/0x78 + slot*0x38, tipo 0/1/4, layer, heap)
e salvate in `layer+0xb8/+0xc0/+0xc8`. Tipi (stringhe 0xefcc0e..): 0 Draw, 1 SubDraw, 2 Begin, 3 End, 4 GPUCalc.
Esecuzione: 0x680f2c (switch tipo) → Draw: 0x683ecc(renderer, ctx, layer, slot*258+0xffff) (il 4° argomento è
un id per marker/contatore, non un ordine).

## 2. Priorità del Layer3D: 0x68d474 [VERIFICATO]
```
if ((layer[0xde4] & 0x20) && layer+0x218 != 0)            -> +0x88 = 5
R = layer+0x1020; if (!R)                                   -> +0x88 = 5
idx = (s8)layer[0x1088]                                     (indice vista assegnato da 0x68d4f4/0x6ab220)
g = (R+0x4228 count > idx) ? R+0x4230 + idx*0x410 : R+0x4230   (fuori range → gruppo 0)
if (g[0x408] == 1)                                          -> +0x88 = 5
else                                                        -> +0x88 = [R+0x55b0] + 100
```
- `R+0x55b0` = conteggio di un PtrArray del membro R+0x54b8 (ctor 0x72093c azzera +0xf8, 0x720e30 lo incrementa)
  → ≥ 0, uguale per tutti i layer dello stesso R: Sub0..3 e i nostri avrebbero la **stessa** priorità se nessun
  ramo "5" scatta.
- `g[0x408] == 1` è lo stesso test usato dal calc di R per vista (0x6aba18 → argomento di 0x6bcd30) e da
  0x87e538 ("vista i abilitata" = `g[0x408] != 1`). Se `R+0x4228` vale ≥ 8 (gruppi per le viste nuove) e i gruppi
  4..7 hanno `+0x408 == 1` (non configurati: 0x3cd5c8 tocca solo i gruppi 0..3, e solo `+0x409`), i nostri layer
  prendono **5** → disegnati dopo tutti i 2D (prio 10/9). [IPOTESI forte; alternative: ramo `+0xde4/+0x218`].
- Effetto collaterale possibile dello stesso flag: 0x6bcd30 riceve `w1 = (g[0x408] != 1)` per le viste 4..7
  (da verificare se influisce su ombre/culling).

## 3. Verifica a runtime (1 riga di log)
Nel hook di §4, una volta ogni N frame per i 4 layer extra e per Sub0 (`mgr->[0x128][0]`):
`+0x88` prima/dopo, `(s8)+0x1088`, `[R+0x4228]`, `g[0x408]`, `layer[0xde4]&0x20`, `layer+0x218`.
Per un quadro completo: `+0x84/+0x88` di tutti i layer `ls->[0x48][0..17]` (2D attesi 10).

## 4. Soluzione consigliata
Hook trampolino su 0x68d474 (è la virtuale `vtbl[0x90]` di tutti i Layer3D; il trampolino patcha il codice,
quindi vale anche per la chiamata virtuale a 0x6844f0):
```cpp
HOOK_DEFINE_TRAMPOLINE(Layer3DPrioHook) {
    static void Callback(uint8_t* ly) {
        Orig(ly);
        if (!proto::mgr) return;
        for (int j = 0; j < proto::Extra; j++) {
            if (ly != proto::layer[j]) continue;
            uint8_t* sub0 = (*reinterpret_cast<uint8_t***>(proto::mgr + 0x128))[0];   /* 3D Sub0 (slot 2) */
            if (!sub0) return;
            Orig(sub0);                                   /* idempotente: scrive solo sub0+0x88 */
            int p = *reinterpret_cast<int32_t*>(sub0 + 0x88);
            *reinterpret_cast<int32_t*>(ly + 0x88) = p > 12 ? p - 2 : p;   /* dopo i Sub, prima dei 2D (10) */
            return;
        }
    }
};
// in main: Layer3DPrioHook::InstallAtOffset(0x68d474);
```
- `p-2`: il Draw dei nostri layer (p−2) e il loro SubDraw (p−3) vengono dopo Draw/SubDraw dei Sub (p, p−1) e
  restano ben sopra i 2D (10/9) e il Begin/End del display. Con `p` uguale va bene lo stesso (le celle non si
  sovrappongono; l'ordine tra pari priorità è solo arbitrario).
- Se Sub0 stesso dovesse valere 5 (R scollegato, fuori gara), i layer extra sono comunque spenti (PerFrame).
- Alternativa senza toccare i layer del gioco: dare ai nostri layer una **copia della vtable** (0x12b4fb8, 0xb0+
  byte; copiare da `*(void**)ly` almeno fino a `+0x98`) con `[0x90]` → una nostra funzione che chiama 0x68d474 e
  poi corregge `+0x88`. Più codice, stesso effetto.

### Cosa NON funziona / non serve
- **Slot diversi** (es. 14..17 → slot intermedi): lo slot non entra nell'ordinamento; inoltre 0..13 sono tutti
  occupati (3D Back 0, 2D Back 1, 3D Sub 2..5, 2D Sub 6..9, 2D Main 10, 3D Extra 11, Front 12, Overlay 13).
- **Registrare prima**: l'ordine di registrazione conta solo a parità di priorità, e l'heapsort non è stabile.
- **Scrivere `+0x88` una volta** dopo 0x667a94: riscritto ogni frame da `vtbl[0x90]` (0x6844f0).
- **Cambiare le soglie ls+0x50c/0x510**: spostano interi gruppi di job in altre code (bucket), effetti globali.
- Correggere la causa (gruppo `R+0x4230[4..7]+0x408`) è possibile ma va prima confermata (§3) e potrebbe
  influire anche su 0x6bcd30/0x87e538; il hook di priorità è indipendente dalla causa.
