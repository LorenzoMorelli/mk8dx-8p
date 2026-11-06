# Step 7b – "Fantasmi" nelle viste 4..7 del menu: analisi statica del percorso di disegno per vista

Data: 4 nov 2026. Solo analisi statica (`../img/main.bin`, `../img/main.ann.asm`, `src/program/main.cpp`); Eden e GDB non usati.
Verifica: `step7b_data/ghost_verify.py` → `step7b_data/ghost_verify.txt`, **64 OK, 0 FAIL**.
Etichette: **[VERIFICATO]** = istruzioni citate e controllate dallo script; **[IPOTESI]** = da misurare (§5).

Sintomo (da Eden, dato dal chiamante): nella cella della vista k (k = 4..7) compare il modello giusto della vista k **e** il
contenuto della vista k−1 (KartUnit k−1 + piedistallo + moneta), con lo spostamento di proiezione della vista k−1 (specchiato).
Se si spegne il layer della vista 4, il fantasma nella cella 4 sparisce. Le maschere di vista per shape sono già corrette.

---

## 0. Verdetto

1. **Nel codice non c'è nessun indice "v−1" [VERIFICATO, risultato negativo].** In tutto il percorso Layer3D → renderer R →
   liste per vista → ModelObj, ogni accesso per vista usa l'indice della vista stessa (`layer+0x1088`, poi `V+0x8`).
   Il fuori-limite ha un solo comportamento: `cmp count,v; csel voce_v, voce_0` → **voce 0**, mai v−1.
   Tutti gli array per vista visti sono dimensionati con `R+0xf0` (= 8 con `RenderSceneCreateHook`).
2. **Il sintomo esclude un errore nelle liste o nelle maschere [VERIFICATO, ragionamento sui dati].**
   - Tutti i modelli del menu stanno nello stesso punto del mondo (step7b_plan §1.3).
   - Se il KartUnit k−1 entrasse nella lista della vista k, verrebbe disegnato con la camera e lo spostamento della vista k.
     Quindi finirebbe **esattamente sopra** il modello di P(k+1), non specchiato.
   - Lo spostamento della vista k−1 vuol dire che in quella cella arriva **l'immagine della vista k−1**, con il suo buffer uniform
     di camera e proiezione. Non arrivano i suoi oggetti dentro la vista k.
3. **"k−1" coincide con la vista disegnata subito prima sulla GPU, non con un indice.**
   - I segmenti GPU sono ordinati per id crescente [step6b].
   - Con `order8::Rank`, il layer 3D Sub4 (slot 14) ha rango 6, subito dopo 3D Sub3 (slot 5, rango 5); Sub5..7 seguono in fila.
   - Il predecessore GPU della vista 4 è quindi la vista 3, quello della 5 è la 4, ecc.
   - Spiega anche la gara: lì ogni vista è un'immagine opaca a tutto schermo (cielo + terreno) e copre qualunque residuo.
   - Nel menu invece la vista 3D è trasparente sopra il 2D, quindi il residuo resta visibile.
4. **Causa: [IPOTESI forte]** contenuto residuo, nel percorso di disegno della vista k, dell'immagine della vista disegnata prima.
   Le strade possibili sono due:
   - **H1**: un bersaglio o una texture condivisa non pulita per le viste extra;
   - **H2**: un passo di composizione o post-effetto che, per le viste 4..7, legge la regione o il buffer della vista precedente.
   - [VERIFICATO] Tutte le viste usano le **stesse due texture globali** `[*GOT 0x12fbe60]+0x38` / `+0xd0` (§3, 0x6f7e5c..0x6f7e6c).
   - Non ho trovato *dove* la pulizia o la regione differiscono tra le viste 0..3 e 4..7.
   - Quello che statico non distingue tra Sub e extra lo elenco in §2.4. Le misure di §5 chiudono la questione in una sessione.

---

## 1. Il percorso di disegno di una vista [VERIFICATO]

- Job del layer **0x683ecc**(display, ctx, layer, id), già agganciato da `LayerDrawJobHook`:
  - apre un segmento con l'id base;
  - chiama Layer::calc (vt+0x40 = 0x68c9ac);
  - poi `0x683c5c(..., id+1)` (0x684014): un segmento per passo, con id = id+1+passo (0x683d6c).
  - I segmenti sono allocati con un contatore atomico (0x685000 `ldxr`). L'id serve **solo** come chiave di ordinamento
    (segmento+0x160, 0x6850c4): due layer non condividono mai un segmento.
- Layer3D: **11 passi** (vt+0x20 = 0x68d6f8 `mov w0,#0xb`). I nomi stanno nella tabella 0x12b5070, lettura vt+0x28 = 0x68d3dc:
  CalcView, CalcGPU, BG, Opa(Debug), Opa, Xlu, Xlu(Debug), PostEffect, DebugInfo, FinalImage, Text(Debug).
- Dispatch **0x68d1d8**: `cmp tipo,#9` (0x68d218), poi un salto da tabella. Tutti i passi passano `(s8)layer+0x1088` al renderer R:

  | passo | chiamata | sito |
  |---|---|---|
  | CalcView | 0x6ad7cc(R, ctx, v, camera/RT) | 0x68d268 / 0x68d27c |
  | CalcGPU | 0x6ad91c(R, v) | 0x68d284 |
  | BG | 0x6ada00(R, ctx, v) | 0x68d2f8 |
  | Opa | 0x6adb80 | 0x68d314 |
  | Xlu | 0x6ae898 | 0x68d330 |
  | PostEffect | 0x6af458 | 0x68d2b0 |
  | FinalImage | 0x6af7f8 | 0x68d2c4 |

- In ogni passo: `V = R+0xf8 + v*0x3e0` con clamp sulla vista 0 (0x6ad7f4 `smaddl`, 0x6ad7f8 `cmp`, 0x6ad7fc `csel`).
  - Le sotto-chiamate usano `V+0x8`, cioè la vista stessa (es. BG 0x6ada50 `umaddl` con `V+0x8`).
  - `V+0x8 = i` è scritto una volta sola alla creazione (0x6bca74).

## 2. Cosa scala già a 8 (candidati esclusi) [VERIFICATO]

### 2.1 Strutture per vista di R
- `R+0xf0` = n viste da `arg[0]` (0x6aa78c); ogni vista è inizializzata da 0x6bc9d8 (chiamata a 0x6aa7c0).
- Le **26 code** di disegno sono allocate *dentro* la vista (0x6a1a3c, alloc a 0x6a1a68). Camera e proiezione vanno nella coda
  della vista (0x6a1b94: viewMtx + `0xc9e54(coda+0x30, proj)` + frustum 0x677d48).
- Calc per vista **0x6ab60c**:
  - per ogni vista attiva chiama 0x6bcd30 (0x6aba34), che riattiva la vista solo se `layer+0x82` è acceso (0x6bcdac);
  - camera e proiezione vengono dal **layer della vista** (`V+0x390`), tramite 0x665090 → `layer+0x68` e 0x6650cc → `layer+0x70`
    (0x6abc18..0x6abc70).
- **Vista principale** `R+0x5d1d`: la prima vista attiva. Se ne prende un'altra solo con `layer+0x1089` bit0, che vale 0 per tutti
  (0x6aba68 / 0x6aba7c / 0x6aba8c).
  - Solo lei riceve `V+0x388 |= 4` (0x6abd68) e i passi BG extra (0x6adad0 `tbz #2`). È la vista 0: non c'entra con k−1.

### 2.2 Liste di disegno (DLM)
- Reset per frame 0x721504 (0x6ab9b4). La voce n è presa in 0x721524:
  - `voce+0 = V+0x8` (0x721580);
  - `voce+2 = 1<<V+0x8` (0x721588 / 0x72158c);
  - le code della voce puntano alle code di V; `V+0x38a = n` (0x6abcd8).
- Il riempimento per modello usa `model+0x60 + v*0x58` con v = `voce+0` (step2_crash8v §1): sempre la vista vera.

### 2.3 Sottosistema `R+0x4218` (voci da 0x3d68 per vista: G-buffer/uniform di vista)
- Creato con `[arg]` = n viste (0x6b3274 / 0x6b3278); `count = n` (0x6f7cbc). Il fuori-limite andrebbe sulla voce 0.
- Blocco uniform `voce+0x3ce8`:
  - le voci i>0 copiano **solo il layout** della voce 0 (0x6f7d3c → 0x635554, che copia +0x8/+0x1c/+0x20);
  - ogni voce alloca il **proprio** buffer (0x6f7db4 → 0x635598).
  - Quindi niente uniform di camera/proiezione a 4 voci.
- **Condivisione vera:** 0x6f7e24(obj, v) lega le render-buffer della voce v (+0xd78, +0xf40, +0x1c90, +0x1e58, +0x2ba8, +0x2d70)
  alle **stesse due texture globali** `[*0x12fbe60]+0x38` e `+0xd0` (0x6f7e5c..0x6f7e6c).
  - Tutte le viste disegnano in sequenza nelle stesse texture di colore e profondità, ognuna nel proprio viewport.
  - Viewport: rect del layer `+0x50..+0x5c`, copiato da `+0x30` in 0x665f20..0x665f3c, poi in `voce+0x38..+0x44` a 0x6f7f74.
  - È il punto in cui un residuo della vista precedente può sopravvivere (H1), se pulizia o regione non coprono la cella.

### 2.4 Layer 3D Sub0..3 contro layer extra
- Il RLM crea i Sub così (0x3ca4e8..0x3ca784):
  - `0x68d0e4` → registra (0x3ca504) → `layer+0x68` = camera condivisa `mgr+0x1d8`, `+0x70` = `mgr+0x238+i*0xc0` (0x3ca520) → disabilita.
  - **Nessun altro campo.** Il ctor base è identico per tutti: `+0xde4 = 0x120` (0x68c530), `+0x428 = 0` (0x68c4fc).
  - `+0x82` ("disegnato su un display") lo scrive per frame il layerSystem (0x667180), sia per i Sub sia per gli extra.
- Differenze reali: slot (2..5 contro 14..17), priorità (142 contro 140), id di job rimappato, `+0x1088` (0..3 contro 4..7),
  oggetti camera/proiezione propri (`proto::cam/proj`, copiati ogni frame da quelli di Sub0 in `m3d8::MenuFrame`).
  - Nel log diag (eden_log_7b6) `+0x1070..+0x108c` sono uguali a parte `+0x1088`.
- **[IPOTESI]** Non ho trovato nel binario una pulizia per layer o per vista che dipenda da uno di questi campi. Va misurato (§5, D1).

### 2.5 Altri dati per vista controllati
- Gruppi `R+0x4230` (0x410 B): clamp sulla voce 0 (0x6aba00..0x6aba18; anche 0xafed58..0xafed64).
- FinalImage salva la matrice "frame precedente" per vista (0x6af8d4: `V+0x398 +0x88 → +0xb8`). È per vista, non condivisa.
- Gli oggetti temporanei del pool (0x64ae4c / 0x64b024) nel renderer stanno solo nella classe 0x675xxx: voci `R+0x2258` da 0x17c8,
  allocate con un conteggio passato dal costruttore (0x6d4e08 / 0x6d53a4).
  - [IPOTESI] Il conteggio è n viste, come gli altri sottosistemi di R; non ho risalito il chiamante.

## 3. Perché proprio k−1: ordine GPU [VERIFICATO per l'ordine, IPOTESI per il meccanismo]
- `order8::Rank` (main.cpp): slot 2..5 → 2..5, **14..17 → 6..9**, 6..9 → 10..13, …
- I segmenti sono eseguiti per id crescente, quindi l'ordine GPU è Sub0, Sub1, Sub2, Sub3, extra4, extra5, extra6, extra7.
- Tutte usano le stesse texture globali (§2.3). Il "k−1" del sintomo è il predecessore in quest'ordine.
- Le viste 1..3 non mostrano il fantasma di 0..2: qualcosa (pulizia o regione) funziona per i Sub e non per gli extra.
  - Oppure (H2) il passo di composizione o post-effetto delle viste extra legge la regione o il buffer sbagliato.

## 4. Correzione proposta

Non c'è un indice o un limite "a 4" da correggere. La correzione dipende da una sola misura (§5, T1 + D1). Ordine consigliato:

1. **T1 – inversione dell'ordine** (nessuna patch binaria, 1 riga in `order8::Rank`):
   - mettere `slot 14..17 → 9,8,7,6` (Sub4 per ultimo);
   - con H1 o H2, il fantasma nella cella 4 diventa la vista 5 e quello nella cella 7 diventa la vista 3;
   - se resta k−1 per indice, l'ipotesi d'ordine cade e il problema è nei dati della vista (allora D2).
2. **D1 – diff dello stato per vista** (trampolino diagnostico, solo log):
   - **0x6f7f2c** (setup della voce per vista in CalcView), parola originale **`d10343ff`** (`sub sp,sp,#0xd0`), firma
     `(obj=R+0x4218, v, rect*, rt*, w4, w5, x6)`;
   - dopo `Orig`, per v = 3 e 4 una volta ogni 300 frame, loggare:
     - della voce `[obj+0x10] + v*0x3d68`: `+0x0`, `+0x4`, `+0x8`, `+0x18..+0x28`, `+0x30..+0x4c`, `+0x5b0/+0x5b4`, `+0x3cd8/+0x3cdc`;
     - gli argomenti w4/w5 e `[x3+0x20]` (render target);
     - di V (`R+0xf8+v*0x3e0`): `+0x388..+0x38a`, `+0x3c8..+0x3d8`.
   - Insieme, un dump di `sub[3]` e `proto::layer[0]` (0x1090 B, campi non puntatore): `+0x78..+0x9c`, `+0x1f0`, `+0x208`,
     `+0x428`, `+0xde4`, `+0xc10`.
3. **Correzione, in base all'esito:**
   - **D1 trova un flag o un campo diverso tra v = 3 e v = 4** (pulizia, `voce+0x0` bit0/bit6 da 0x6f8178..0x6f81b0, regione):
     copiarlo per le viste 4..7.
     - Se è un campo del layer: in `proto::Create` / `m3d8::MenuFrame` copiare il campo di `sub[0]` su `proto::layer[j]`, come si fa già
       per i layer 2D (`+0xde4`, `+0x83`).
     - Se è un campo della voce o di V: trampolino sopra (`d10343ff`) che lo imposta dopo `Orig`.
     - Rischio basso: niente salti nuovi nel binario, solo dati.
   - **D1 non trova differenze CPU** (H2 lato GPU): passare alle misure D2/D3 su BG (**0x6ada00**, `a9bd57f6`) e FinalImage
     (**0x6af7f8**, `a9bd57f6`).

**Mitigazione provvisoria possibile [IPOTESI]**: verificare che `m3d8::Cell` non produca celle extra che *toccano* quelle dei Sub.
Con celle 640×160 a passo 160 le righe sono adiacenti (y1 della riga r = y0 della riga r−1). Se un passo di composizione o post-effetto
campiona oltre il bordo (filtri, bloom), si vede la riga sopra. Prova veloce: ridurre di 2 px l'altezza delle celle 4..7. Questo però
spiegherebbe solo il colore del bordo, non un modello intero: è da tenere solo se D1 mostra rect sovrapposti.

## 5. Misure a runtime (log; GDB solo per D3)

| # | misura | esito H1/H2 (ordine GPU) | esito "dati per vista" |
|---|---|---|---|
| T1 | `Rank(14..17) = 9,8,7,6` | fantasma della cella di P5 (vista 4) = P6, della cella di P8 = P4 | invariato (cella di P5 = fantasma P4, ecc.) |
| T2 | spegnere **solo Sub3** (0x6651a8(sub[3], 0), extra accesi) | cella 4: fantasma = P3 (vista 2, ora predecessore) | il fantasma della cella 4 sparisce |
| T3 | vista 3 senza KartUnit3 (bit 3 spento) | il fantasma nella cella 4 perde il kart, restano piedistallo e moneta | idem (prova di conferma, non discrimina) |
| D1 | diff voce/V/layer v = 3 contro v = 4 (§4.2) | il campo diverso è la correzione | — |
| D2 | log in BG/FinalImage (0x6ada00 / 0x6af7f8): v, `V+0x388`, `[V+0x18]+0x264`, rect | quale passo vede ancora i dati di v−1 | — |
| D3 | GDB: watchpoint in scrittura su `voce(4)+0x30..+0x4c` e su `[*0x12fbe60]+0x38` durante un frame | chi riscrive la regione | — |

T1 e T2 bastano per scegliere tra "ordine GPU" e "indice". D1 dà il campo da copiare.

## 6. File
- `step7b_data/ghost_verify.py` / `.txt`: 64 controlli, tra cui le parole dei 3 punti d'aggancio diagnostici
  (0x6f7f2c `d10343ff`, 0x6ada00 `a9bd57f6`, 0x6af7f8 `a9bd57f6`) e la tabella dei nomi dei passi Layer3D.
