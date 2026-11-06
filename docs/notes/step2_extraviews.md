# Step 2D – Perché i layer "3D Sub4..7" non disegnano il mondo + camera dei kart CPU (main 4.0.1)

Analisi statica (main decompresso, offset = indirizzi main; IPS = offset + 0x100). Nessuna modifica al gioco.
Dati grezzi: `step2_data/ev_layer3d_drawmethod.asm`, `ev_renderer_views.asm`, `ev_kartcam.asm`.

## 0. Risposta breve
- **Causa [VERIFICATO]**: un `gsys::Layer3D` disegna il mondo solo tramite il suo DrawMethod "rndr_3D"
  (layer+0x1028), che chiama il **renderer della scena** puntato da `layer+0x1020` con un **indice vista**
  `(s8)layer+0x1088`. I layer extra hanno `+0x1020 = NULL` (ctor 0x68d104), quindi il DrawMethod esce subito
  (0x68d20c `ldr x8,[x0,#0x1020]; cbz x8,ret`) e resta solo il clear/sfondo del layer. Per i 4 Sub originali il
  collegamento lo fa 0x3cce58 (chiamata da 0x3c6e2c a ogni cambio scena): `0x68d4f4(layer, R)` per i 4 layer
  indicizzati da mgr+0x9c0/+0x9e0/+0xa00/+0xa20.
- **Il renderer R ("RenderEngine Main ModelScene", 0x5ec8 B) ha un numero di viste fissato alla creazione = 4**
  (`orr w8,wzr,#0x4` a 0x3c4b84 e 0x3c4c34). Tutti gli array per vista di R sono allocati dinamicamente da quel
  numero (0x3e0 B per vista a R+0xf0/+0xf8, più sottosistemi 0x6d3a28/0x6d1e40/0x6c9650/0x6b322c/0x6b5b54/
  0x644304 che ricevono lo stesso conteggio). Quindi il fix è **piccolo**: creare R con 8 viste e collegare i 4 layer
  extra con `0x68d4f4`. La camera della vista viene poi letta da R direttamente dal layer (layer+0x68/+0x70).
- **Kart CPU [IPOTESI forte]**: per i kart CPU la KartCamUnit è creata con `unit+0xc8 = 0` (0x117fc8: flag
  `[KartUnit+8]+0xd2` = tipo kart ==1, oppure +0xe7), e 0x119524 esce subito se `unit+0xc8==0` (0x11953c): il
  descrittore resta quello del ctor (pos 0,0,10, tipo 1). In più 0x16cd14 salta tutto se un flag globale +0xe9 è
  attivo e `KartUnit+0x4c==0`. Soluzione: chiamare noi la chase-cam del gioco (0x119524) con `+0xc8` forzato a 1,
  oppure costruire la look-at dalla matrice del veicolo (offset in §3).

## 1. Catena di disegno di un layer 3D [VERIFICATO]
```
job 0x683ecc(renderer, ctx, layer, idx)
  0x667d10(&info, ...)  info: +0x0 renderStep, +0x8 u8, +0x10 render buffer, +0x18 layer, +0x20 layer+0x84,
                               +0x28 camera = 0x665090(layer) (layer+0x68), +0x30 proj = 0x6650cc(layer) (layer+0x70),
                               +0x38 layer+0x48 (viewport), +0x48 ctx
  0x61e104(layer+0x48, ...)            viewport NVN
  blr layer.vtbl[0x40] = 0x68c9ac      Layer::draw base (clear, pause texture, ecc.) -> vtbl[0xa8] (ret vuoto)
  0x683c5c(...)                        per ogni renderStep s < vtbl[0x20]() (0x68d6f8): 0x6660c4(layer,&info)
0x6660c4 Layer::drawRenderStep: step = layer->[0xa8] + s*0x820 (count layer+0xa0); se step+0x818 bit0:
         per ogni DrawMethod dm in step{+0x8 count, +0x10 ptr[]}: se dm+0x38 bit0 -> delegate dm+0x3c (blr)
```
- Render step del Layer3D (tabella nomi 0x12b5070, vtbl[0x28] 0x68d3dc): 0 CalcView, 1 CalcGPU, 2 BG, 3 Opa(Debug),
  4 Opa, 5 Xlu, 6 Xlu(Debug), 7 PostEffect, 8 DebugInfo, 9 FinalImage, 10 Text(Debug).
- Classe `gsys::Layer3D` (stringa 0xefe36d), ctor 0x68d0e4 (size 0x1090), vtable `[0x130cc90]+0x10` = 0x12b4fb8:
  [0x20]=0x68d6f8 numSteps, [0x30]=0x68c730 init, [0x40]=0x68c9ac draw, [0x88]=0x68d70c "attaccato?" (+0x1088 != 0xff),
  [0x90]=0x68d474 priorità (+0x88), [0x98]=0x68d3fc.
- Registrazione del DrawMethod "rndr_3D": 0x667a94 → 0x667bac `bl 0x664634` (Layer::initialize: alloca gli step
  0x820) → 0x664718 `blr vtbl[0x30]`=0x68c730 → 0x68c7c4 `blr vtbl[0x98]`=0x68d3fc → 0x68d424
  `bl 0x664f80(layer, layer+0x1028)` (aggiunge il DrawMethod a tutti gli step, 0x667e84 = pushBack).
  **Quindi anche i layer extra creati dal modulo hanno il DrawMethod**: manca solo il renderer.
- Delegate del DrawMethod: obj = layer (+0x106c), funzione `[0x130cc98]` = **0x68d1d8**:
  ```
  0x68d20c ldr x8,[x0,#0x1020] ; cbz x8 -> ret      <-- i layer extra escono qui
  0x68d214 w9 = info.renderStep ; cmp #9 ; jump table 0xf6f5c8:
     0 CalcView  -> 0x6ad7cc(R, ctx, (s8)layer+0x1088, buffer, flag)
     1 CalcGPU   -> 0x6ad91c(R, view)      2 BG -> 0x6ada00     4 Opa -> 0x6adb80     5 Xlu -> 0x6ae898
     7 PostEffect-> 0x6af458               9 FinalImage -> 0x6af7f8 (+ 0x667dc8)      3/6/8 -> niente
  ```

## 2. Il renderer della scena R e le sue viste [VERIFICATO salvo dove indicato]
### 2.1 Creazione (per scena)
- `0x3c43e8(engine, &sceneType, heap)` (da 0x3c42ac) crea la scena di rendering: `0x3c4b54` → R principale in
  `engine+0x298`; `0x3c4cbc` (solo per alcuni tipi) → R secondario in `engine+0x2a0`.
- 0x3c4b54: `0x6a8ee0(&arg)` (arg+0 = 1, arg+8 nome, arg+0x10 ...), poi **`orr w8,wzr,#0x4; str w8,[sp,#0x10]`
  a 0x3c4b84** (arg.numViews = 4), nome scena "TurboTheaterMenu"(8)/"TurboAward"(6)/"TurboMenu"(3)/altrimenti
  "Turbo" con **di nuovo `orr w8,wzr,#0x4` a 0x3c4c34**; heap "RenderEngine::MainModelSceneHeap" (0x605128, poi
  adjust via vtbl+0x28); `0x6a8f10("Main", &arg, heap)` → `str x0,[engine,#0x298]` (0x3c4c90).
  R secondario (0x3c4cbc): arg.numViews = 1 (`orr w25,wzr,#1` 0x3c4d00/0x3c4d50), nome "Sub", heap "SubModelSceneHeap".
- 0x6a8f10: alloc 0x5ec8 (ctor 0x6aac74), poi 0x6a8fec(R, arg, heap, ...) → (0x6aa5xx-0x6aa804) usa `[arg]`
  (x26) come conteggio viste per: 0x6d3a28, 0x6d1e40, 0x6c9650, 0x6b322c (alloca per vista 0xb80 B a +0xd10, ecc.),
  0x6b5b54 (per ogni elemento di una lista), **R+0xf0 = numViews, R+0xf8 = new View[numViews] (0x3e0 B, ctor
  0x6bc8b4, init 0x6bc9d8(view, idx, ..) che scrive `view+0x8 = idx` e alloca 0x1450 B per vista)**, e oggetto
  0x360 B a R+0x5d40 con +0x238 = numViews. Tutti i consumatori trovati leggono il conteggio (`ldr w,[R,#0xf0]`),
  nessun `cmp #4` sulle viste nel codice di R (0x6a8000-0x6bcfff: le 12 `cmp #4/#5` sono altro).
- R viene distrutto/ricreato a ogni scena; il dtor della vista (0x6bc928/0x6bc978) fa `0x68d4f4(view+0x390, NULL)`
  → **scollega da solo i layer** (anche quelli del modulo).

### 2.2 Collegamento layer ↔ vista
- `0x68d4f4(layer3D, R)` (setter): se `layer+0x1020` ≠ 0 → `0x6ab2d0(oldR, layer)` (libera lo slot) e +0x1088=0xff;
  se R ≠ 0 → `+0x1088 = 0x6ab220(R, layer)`; `layer+0x1020 = R`; ritorna l'indice (s8).
- `0x6ab220(R, layer)` (sotto lock R+0x5e68): cerca in `R->[0xf8]` (stride 0x3e0, `R+0xf0` voci) la vista con
  `+0x390 == layer`, altrimenti la prima con `+0x390 == 0` e ci scrive il layer; **nessuna libera → -1**.
  Con numViews = 4 i 4 Sub originali occupano 0..3: un 5° layer otterrebbe -1 (0xff = "non attaccato", 0x68d70c).
- Chiamanti: **0x3cce58(mgr, R)** (4 Sub 3D via mgr+0x9c0/+0x9e0/+0xa00/+0xa20 → indice in mgr+0x120/+0x128),
  0x3ccf14 (lista mgr+0x158, R secondario se scena 3), 0x3ccf88 (lista mgr+0x178, scene 7/8); tutti da
  **0x3c6da8** (cambio scena: `0x3cbd94(mgr)` poi `0x3cce58(mgr, engine+0x298)` a 0x3c6e2c).
- La camera per vista è presa dal layer: calc per-frame di R **0x6ab60c** cicla `x21 < R+0xf0` (0x6ab9b8..0x6abd3c);
  per vista attiva (`view+0x388` bit0, aggiornato da 0x6bcd30 che legge layer+0x82, +0x50..+0x60) fa
  `cam = 0x665090(view+0x390)`, `proj = 0x6650cc(view+0x390)` (0x6abc18-0x6abc28) → view matrix `cam+8`.
  Quindi la LookAtCamera/proiezione che il modulo mette in layer+0x68/+0x70 vengono usate senza altro lavoro.
- Array "gruppi" R+0x4228 (count) / +0x4230 (0x410 B), indicizzato con `view+0x8` (= idx vista): 0x6aba00-0x6aba18
  con clamp `cmp count,idx; csel → voce 0` (indice fuori range ricade sulla voce 0: innocuo). 0x3cd5c8 scrive
  `+0x409` per le voci 0..3 (4P: 0,0,1,1; altrimenti 0xff) e R+0x4239. [IPOTESI] count = numViews (allocazione
  non individuata); se resta 4, le viste 4..7 usano la voce 0.

### 2.3 Cosa fare (consigliato)
1. **R a 8 viste** (solo quando servono): hook trampolino su `0x6a8f10(SafeString* name, Arg* arg, Heap* heap)`:
   se `strcmp(name->cstr,"Main")==0 && arg->numViews==4` → `arg->numViews = Views` (6 o 8), poi Orig.
   Alternativa IPS/patch: 0x3c4b84 e 0x3c4c34 `0x321e03e8` (orr w8,wzr,#4) → `0x321d03e8` (orr w8,wzr,#8)
   (vale per tutte le scene, anche menu: più memoria sempre).
2. **Collegare i layer extra**: hook trampolino su `0x3cce58(mgr, R)`: dopo Orig, per j=0..Extra-1
   `int8_t v = 0x68d4f4(layer[j], R)`; loggare v (atteso 4..7; -1 = R ancora a 4 viste). Non serve scollegarli:
   lo fa il dtor della vista. Se R è NULL (0x3c6da8 passa engine+0x298 anche fuori gara) Orig passa NULL ai Sub:
   fare lo stesso (0x68d4f4(layer,NULL) scollega).
3. Lasciare layer+0x68 (LookAtCamera propria) e +0x70 (proiezione propria) come già fa `proto`.
4. Verifiche a runtime: `R+0xf0` (=8), `layer+0x1088` dei layer extra, `view(4..7)+0x388` bit0 dopo 1 frame,
   FPS, memoria libera dell'heap di scena (log del valore di ritorno di 0x6a8f10 ≠ 0).
- Fattibilità: **alta** per il rendering base (2 hook, nessuna rilocazione). Rischi [IPOTESI]: (a) memoria:
  ogni vista alloca oggetti CPU (0x3e0 + 0x1450 + 0xb80 ...) e forse buffer GPU (0x6ef2e8) nell'heap di scena
  creato con dimensione automatica: se non basta, 0x6a8f10/i ctor falliscono (abort); (b) costo CPU/GPU: calc
  e culling raddoppiano (8 viste) → possibile calo sotto 60 FPS; (c) ombre/postfx condivisi per "gruppo" (+0x409)
  pensati per 4 viste: possibili artefatti nelle celle 5-8 (es. ombre del gruppo 0).
- **Scartate**: ridisegnare i 4 Sub due volte per frame con camere diverse (i dati per vista di R – culling,
  uniform buffer per vista – sono calcolati nello step CalcView e consumati dal command buffer più tardi:
  sovrascriverli nello stesso frame corrompe la prima passata [IPOTESI forte]); rilocare array per vista da 4 a
  8 (non serve: sono già dinamici).

## 3. Camera dei kart CPU
### 3.1 Strutture [VERIFICATO]
```
KartUnit (ctor base 0x16c968(this, idx), derivato 0x16e3dc; calc per frame 0x16cd14 via delegate GOT 0x12fc5f8)
  +0x08 Vehicle*        +0x10 (driver/modello)   +0x20 KartCamUnit* (0x2b0 B, ctor 0x117de8(unit, KartUnit))
  +0x38 ptr  +0x40 s32 indice kart  +0x44/+0x45/+0x46/+0x4c u8 flag (=1 nel ctor)
  +0x50 Matrix34f "pose" (identità nel ctor, passata a 0x1748a4/0xd9e44/0x119524)   +0x80 Matrix34f
KartCamUnit: +0x8 desc0, +0x40 desc1, +0x78 desc2 (layout desc in step2_viewmatrix §3), +0xb0 KartUnit*,
  +0xc8 u8 "calcola camera", +0xc9, +0xca usa desc1, +0x170 up smussato, +0x288 dir
raceMgr+0x58 ptr[12] = info giocatore (NON KartUnit): +0x4c s32 slot locale (-1 = non locale) (0x119780, 0x872328)
Vehicle = [KartUnit+8] = [unit+0xb0]+8: +0xd0 (tipo==0), +0xd2/+0xd3 (tipo==1) con tipo = [x20+0xd54] (0x16fae8-
  0x16fb30), +0xe7, +0xe9 flag; **+0x2a8/+0x2ac/+0x2b0 = punto base del target della chase-cam** (0x11a138:
  copiato tale e quale in at); +0x28 = P (fisica/posa).
P = [Vehicle+0x28]: P+0x8 = 0x30 B (memcpy 0x30 a 0x118a48; 0x24 B a 0x119fe8)
```
- Orientamento [IPOTESI forte, da 0x119fa4/0x119cf0]: P+0x8 è una matrice 4 vettori×3 (colonne): **P+0x8 asse X
  (lato), P+0x14 asse Y (su: è il vettore interpolato verso l'up mondo e normalizzato in 0x11a03c-0x11a0b0),
  P+0x20 asse Z (avanti: 0x119cf0 usa |P+0x24| = pendenza), P+0x2c traslazione (posizione)**. Da confermare a
  runtime confrontando P+0x2c con Vehicle+0x2a8.

### 3.2 Perché il descrittore dei CPU non si aggiorna
- 0x119524(unit, pose): `ldrb w8,[unit,#0xc8]; cbz → strb wzr,[unit,#0xca]; ret` (0x119538/0x11953c): senza
  +0xc8 non chiama 0x1197f8 (scrittura desc0) né 0x11908c (desc1).
- Ctor KartCamUnit 0x117fb4-0x117fe0: `if (veh+0xd2 || veh+0xe7) unit+0xc8 = 0; if (veh+0xe9) unit+0xc8 = 1`.
  [IPOTESI forte] tipo ==1 (+0xd2) = kart CPU → +0xc8 = 0 → desc0 resta quello del ctor (+0x14..: 0,0,10 =
  `str w10(10.0),[x20,#0x1c]` a 0x117e60; tipo +0x10 = 1 a 0x117e0c) = esattamente quanto osservato in Eden.
- Secondo gate in 0x16cd14 (0x16cd64-0x16cd74): se `[sys->sub[4]]+0x238 → +0xe9` ≠ 0 e `KartUnit+0x4c == 0`
  → ritorno immediato (salta anche 0x1748a4 veicolo e 0xd9e44 driver). Chi azzera KartUnit+0x4c non è stato
  trovato staticamente (solo scritture a 1: 0x16c9b8, 0x16eadc, 0x16eafc).

### 3.3 Opzioni
A. **Riusare la chase-cam del gioco** (consigliata, poche righe):
   ```
   e = dir+0x3d0+k*0x28;  unit = (uint8_t*)e[0] - 8;  ku = *(uint8_t**)(unit+0xb0);
   uint8_t old = unit[0xc8]; unit[0xc8] = 1;  0x119524(unit, ku+0x50);  unit[0xc8] = old;
   ```
   da chiamare una volta per frame per k = kart mostrati nelle viste 5-8 (es. nel hook 0xc8b60 prima di Orig,
   o nel RlmFrameHook prima di leggere KartDesc). Poi `KartDesc(k)` funziona come oggi. Rischi [IPOTESI]:
   stato della chase-cam mai inizializzato per i CPU (prima chiamata: usare 0x11864c(unit, pose) = reset/teletrasporto,
   come fanno 0x16d94c/0x1732cc), side-effect di 0x12209c (solo se `veh+0xe3`), delegate `[unit+0xd0]`.
   Più radicale: hook su 0x117de8 per lasciare `+0xc8 = 1` ai kart 4..7 (ma resta il gate di 0x16cd14).
B. **Look-at propria** dal veicolo (nessun side-effect):
   `pos_k = Vehicle+0x2a8` (oppure P+0x2c), `fwd = normalize(P+0x20)`, `up = normalize(P+0x14)`;
   `eye = pos_k - fwd*D + up*H` (D≈8..10, H≈3..4 unità di gioco [IPOTESI: scala da tarare]), `at = pos_k + up*h`,
   `up = (0,1,0)` o `up` del kart; scrivere in cam+0x38/+0x44/+0x50 e chiamare 0x619a08(cam, cam+8).
   Accesso: `ku = *(uint8_t**)(((uint8_t*)e[0]-8)+0xb0)`, `veh = *(uint8_t**)(ku+8)`, `P = *(uint8_t**)(veh+0x28)`.

## 4. Azioni consigliate in ordine
1. Hook 0x6a8f10: numViews 4→Views solo per "Main" (log nome, numViews, R restituito).
2. Hook 0x3cce58: dopo Orig `0x68d4f4(layer[j], R)` per i layer extra; log indice (atteso 4..7).
3. Test in Eden 4P: le celle 5-8 devono mostrare il mondo con la camera fissa attuale (0,0,10 → vista dal centro
   pista): conferma del punto 1-2 indipendente dalla camera. Misurare FPS e verificare assenza di abort heap.
4. Camera: opzione A (0x119524 con +0xc8 forzato, reset iniziale 0x11864c); se instabile, opzione B (look-at da
   Vehicle+0x2a8 / P). Verifica: log di desc0 pos per k=4..7 che cambia a ogni frame.
5. Se compaiono artefatti di ombre/postfx nelle celle 5-8: indagare R+0x4228 (gruppi, 0x3cd5c8) e i sottosistemi
   creati con numViews (0x6b322c, 0x6d1e40).
6. A fine prototipo: verificare menu/replay (scene 3/6/7/8) con numViews=8 solo in gara.

## 5. Non determinato staticamente
- Allocazione/conteggio di R+0x4228 (gruppi 0x410 B) e budget di memoria dell'heap di scena con 8 viste.
- Valore di `[KartUnit+8]+0xd54` per CPU (1?) e chi azzera KartUnit+0x4c; significato del flag globale +0xe9.
- Semantica esatta della matrice P+0x8 (colonne vs righe): controllare a runtime.
