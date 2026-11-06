# Step 6c – Flash/celle bianche in gara a 8 (analisi statica, main 4.0.1)

Fonti: `../img/main.ann.asm`, `../img/main.bin`, log esistenti (step2/6b), `src/program/main.cpp` (sola lettura).
Verifica: `step6c_data (dati non pubblicati)/flash_verify.py` -> `flash_verify.txt`: **130 OK, 0 FAIL** (dopo §6).
Estratti asm: `step6c_data/fn_6d1e40.asm` (post-processing), `fn_6ab60c.asm` (calc per frame di R),
`fn_6a8fec_init.asm` (init di R), `fn_6b322c.asm` (sottosistema con i gruppi).
Etichette: [V] verificato (statico o log), [I] ipotesi/inferenza, [D] dato dell'utente.

## 0. Sintesi
- [V] L'ipotesi di partenza ("dati di post-processing per vista dimensionati per 4/5") è **smentita per il renderer R**:
  ogni struttura per vista di R che contiene esposizione, HDR, bloom/glare, gruppi e viste viene allocata con
  `[arg] = numViews`, che con `RenderSceneCreateHook` vale 8 (log: `R+0xf0=8`, `gruppi R: count=8`).
  Gli indici sono `view+0x8` (0..7) con clamp alla voce 0: nessuna voce 4..7 finisce sopra le voci 2..3.
- [V] Esiste invece un meccanismo per vista che distingue **solo le viste 2 e 3**: il *time-slicing* del modo 4P.
  In modo layout 2 (`mgr+0xbd8 == 2`) `0x3cd5c8` assegna ai gruppi 0..3 la fase `+0x409 = 0,0,1,1` e mette
  `R+0x4239 = 2`. Ogni frame `0xafecc8` calcola lo stato del gruppo: vista disegnata se `contatore % 2 == fase`,
  altrimenti la vista **non si disegna** e nella sua cella viene copiata un'immagine in cache (texture "copy" presa
  da un pool globale). Il codice è srotolato su 4 gruppi: i gruppi 4..7 restano a `0xff` (disegnati ogni frame).
  Log [V]: `g[0..7].408/409 = 00/00 00/00 00/01 00/01 | 00/ff 00/ff 00/ff 00/ff R+0x4239=02`.
- [I] La causa più probabile (certezza **media**, ~60%) è questo percorso "fase 1 + cache" con 8 viste: le viste 2,3
  sono le uniche con fase 1. Nel 4P originale la fase coincide con la riga dello schermo (0,1 sopra, 2,3 sotto); nella
  griglia 4x2 le viste 2,3 sono in riga alta ma hanno la fase della riga bassa, mentre le 4..7 (riga bassa) non sono
  mai saltate. È una configurazione a metà strada, non il flusso originale con N=8.
- Cosa NON spiega ancora l'analisi statica: il meccanismo preciso del contenuto sbagliato (cache sovrascritta nel
  pool, buffer per fase condiviso, esposizione) e perché le viste 0,1 (fase 0) restano corrette. Va misurato (§4).

## 1. Dati per vista del post-processing e dell'illuminazione (domanda 1)

### 1.1 Chi alloca (tutto in `0x6a8fec`, init di R, `[x26] = arg.numViews`)
| struttura | dove | capienza | prova |
|---|---|---|---|
| R (0x5ec8 B) | `0x6a8f10` | – | `0x6a8f60 mov w0,#0x5ec8` [V] |
| viste (0x3e0 B) | R+0xf8, conteggio R+0xf0 | numViews | `0x6aa724`, `0x6aa78c` [V] |
| sottosistema post-processing | R+0x39c8, init `0x6d1e40(obj, &numViews, heap, ..)` | – | `0x6aa1e0`, `0x6aa68c` [V] |
| ⤷ oggetto +0x280 (0x9e0 B, init 0xabedb0) | | numViews | `0x6d1e90` [V] |
| ⤷ +0x290 (0x1518 B, 0xac3cbc), +0x2b0 (0x450, 0x6ec6d4), +0x2a0 (0x1a30, 0x640078), +0x2a8 (0xbb0, 0x70e0b8) | | numViews | `0x6d1ed8/0x6d1f4c/0x6d1f98/0x6d1ff0` [V] |
| ⤷ HDRCompose +0x2e0 (0x268 B, init 0x643c30, stringa "HDRCompose") | | numViews | `0x6d20c8/0x6d20d4` [V] |
| ⤷ **AutoExposure** +0x298 (0x978 B, init 0xabc784, stringa "AutoExposure"): voci da **0x1a20** a +0x1d8, conteggio +0x1d0; per voce 2 texture 1x1 (ping-pong), 128x1 e 128x128 (riduzione luminanza) | | numViews | `0x6d2114/0x6d2120`, `0xabc7b8`, `0xabc824` [V] |
| ⤷ +0x2c8 (0x8fe0 B, 0xaaec78) | | numViews+1 | `0x6d2160` [V] |
| ⤷ +0x2d0 (0x7b0, 0xacb034), +0x2d8 (0x750, 0xac8a80) | | numViews | `0x6d21a8/0x6d21f4` [V] |
| ⤷ +0x2c0 array di puntatori | | **conteggio globale** `[0x12b03b0]` (iniziale 1), non viste | `0x6d2038` [V]; significato [I] (display/core) |
| sottosistema R+0x3fd0, init `0x6b322c` | | | `0x6aa208/0x6aa6d8` [V] |
| ⤷ [R+0x4218] voci 0x3d68 (0x6f7c14) | | numViews | `0x6b3278` [V] |
| ⤷ **gruppi** R+0x4220: conteggio R+0x4228, voci 0x410 a R+0x4230, contatore R+0x4238, fasi R+0x4239 | | numViews | `0x6b32d8/0x6b34b4/0x6b34bc`, `0xafebcc`, `0xafec44` [V] |
| ⤷ voci 0xb80 a +0xd10 (R+0x4ce0) | | numViews | `0x6b3584/0x6b35ec` [V] |
| DLM (R+0x54b8, voci 0x3b8) | | numViews | step2_crash8v §3 [V] |

Stringhe trovate: `AutoExposure` (0xf0211b), `HDRCompose`, `cExposureTexture`, `cHDRImage`, `cBloom`, `Bloom`, `BloomObj`,
`GlareFilter`, `cLuminanceWeight/Param`, `gsys_areaenv_ExtConfExposure(Enable)`, `ExposureMid`, `PostEffect`. Nessuna
porta a un array a 4/5/6 posti: le classi agl/gsys di post-processing ricevono `n` all'init.

### 1.2 Chi indicizza
- AutoExposure: `0x6d2728 ldrb w10,[view,#0x8]` -> `voce = (n > idx) ? base + idx*0x1a20 : base` (clamp, `0x6d273c/0x6d2740`);
  stesso schema nelle funzioni 0xabcb50, 0xabcc00, 0xabcf0c, 0xabd32c, 0xabd770, 0xabd940 (indice in w2) [V].
  Il flip del ping-pong (0xabc990) avviene solo per le voci elaborate nel frame (`+0x1a18`) [V].
- Gruppi: `0x6aba00-0x6aba18`, `0x6b3a54-0x6b3a6c`, `0x87e538`, `0xafed50`, `0xafee6c`: tutti `idx = view+0x8`, clamp [V].
- [V] Con R a 8 viste nessuna di queste strutture ha capienza < 8. Le viste 4..7 hanno voci proprie: **non** scrivono
  sulle voci 2..3 e le viste 2..3 non leggono dati di altre viste. L'ipotesi di partenza è smentita per R.
- Restano fuori da R (già noti, non legati al colore): mgr+0x9c0.. 4 Sub, Scene 5 viewport, 0x108a2c `cmp #4`,
  0x3d230 `slot+2` (audio/effetti, step2_renderlayers). Nessuno tocca esposizione/HDR.

## 2. Perché proprio le viste 2 e 3 (domanda 2)

### 2.1 Il time-slicing del 4P [V statico + log]
```
0x3cbf30 -> 0x3cd5c8(mgr, ...)            // unico chiamante 0x3cbfc8
  R = [0x3c46d4()+0x298]
  if (!mgr[0xbe8] && mgr[0xbd8] == 2) {   // modo 4P
      g[0].409 = 0; g[1].409 = 0; g[2].409 = 1; g[3].409 = 1;   // srotolato, solo 4 gruppi
      R[0x4239] = 2;                                            // 2 fasi
  } else { g[0..3].409 = 0xff; R[0x4239] = 1; }
// gruppi 4..7: 0xff dall'init (0xafec6c strh 0xff00 su +0x408)

ogni frame, calc di R 0x6ab60c -> 0x6ab8dc -> 0xafecc8(R+0x4220):
  cnt = ++R[0x4238]
  per ogni gruppo g:
     if (R[0x4239] < 2 || g.409 == -1) g.408 = 0            // sempre disegnata
     else if (cnt % R[0x4239] == g.409) g.408 = 2           // disegna e cattura
     else if (g.cache) g.408 = 1                            // SALTATA: mostra la cache
     else g.408 = 2
loop viste 0x6aba18: vista abilitata = (g.408 != 1) -> 0x6bcd30 (view+0x388)
disegno vista 0x6b3a04 -> 0xafed3c: stato 1 -> 0x64b868 copia g+0x8 (cache) nella cella;
                                    altrimenti rilascia g.cache al pool (0x64b024)
fine vista 0x6b57f8 -> 0xafee4c: stato 2 -> g.cache = 0x64ae4c(pool [0x1306018], "copy", w, h della vista)
                                    e copia l'immagine della vista nella cache
gioco: 0x87e538(engine, vista) = (g.408 != 1) usato da 0x522bf8 (aggiornamenti saltati se vista non disegnata)
```
Nel 4P originale (MK8D 3-4 giocatori a 30 fps per vista): frame pari viste 0,1 (riga alta), frame dispari viste 2,3
(riga bassa); la fase è la **metà dello schermo** (`+0x409` = indice di riga nella griglia 2x2).

### 2.2 Con 8 viste (build attuale) [V log + I]
- Il modo resta 2 (log `modo=2`), quindi: viste 0,1 fase 0; **viste 2,3 fase 1**; viste 4..7 `0xff` (ogni frame).
  Frame pari disegna 0,1,4,5,6,7 e mostra 2,3 dalla cache; frame dispari disegna 2,3,4..7 e mostra 0,1 dalla cache.
- [I] Le viste 2,3 sono le uniche con fase 1 e le uniche la cui fase non corrisponde alla loro riga (stanno in riga alta
  con la fase della riga bassa). Le viste 4..7, in riga bassa, sono le uniche che non vengono mai saltate. Qualsiasi
  risorsa condivisa per fase/metà (buffer intermedio della metà, texture del pool acquisite nel frame, dati per frame
  indicizzati con `R+0x5d1c`, che alterna ogni frame: `0x6ab640-0x6ab650`) è usata nel frame dispari dalle viste
  2,3 **insieme alle viste 4..7 della riga bassa**, cosa che nel gioco originale non succede mai.
- [I] Il contenuto mostrato (bianco "sovraesposto" con sagome ciano/rosa) è compatibile con una cache o un buffer
  intermedio HDR non valido (dati HDR/luminanza letti come colore) o con un'esposizione calcolata su una luminanza
  vuota (adattamento verso RangeMax). Con shader asincroni Eden salta i draw delle pipeline non pronte: anche una
  cattura fatta in quel frame resta sbagliata e la cella mostra la cache per un frame su due (magenta pieno).
- [V] Smentita la variante "slot = 2 + k" sulle strutture di R: nessun indice vista viene traslato; gli slot 2..5 del
  layerSystem (Sub0..3) non indicizzano dati di R.
- Non spiegato: perché le viste 0,1 (anch'esse saltate a frame alterni) restano corrette. Questo è il motivo della
  certezza media; le misure del §4 lo separano.

## 3. Correzione fedele al flusso originale (domanda 3)

### 3.1 Principio
Il gioco originale con N viste in griglia assegna a ogni gruppo la fase = riga della cella e `R+0x4239` = numero di righe.
`0x3cd5c8` è srotolato su 4 gruppi: va esteso agli 8 gruppi che esistono già (conteggio 8, nessuna memoria nuova).
Estensione per riga nella griglia 4x2: **fasi 0,0,0,0,1,1,1,1**, `R+0x4239 = 2` (ogni frame 4 viste disegnate e
4 dalla cache, come il 4P: stesso carico GPU per frame del 4P originale, meno di oggi, che ne disegna 6).
Alternativa per la sola diagnosi: tutte `0xff` e `R+0x4239 = 1` (nessuna cache, 8 viste ogni frame).

### 3.2 Hook (1 hook trampolino, nessuna patch di parola)
`0x3cd5c8` prima istruzione `d10143ff` (`sub sp,sp,#0x50`), firma `void(mgr*, bool w1, bool w2, int32_t* x3)`.
Il codice è srotolato: non si estende con patch di parole, serve l'hook.
```cpp
HOOK_DEFINE_TRAMPOLINE(HalfPhase8Hook) {                 /* 0x3cd5c8 d10143ff */
    static void Callback(uint8_t* mgr, bool a, bool b, int32_t* c) {
        Orig(mgr, a, b, c);
        uint8_t* eng = mk8dx::Fn<uint8_t* (*)()>(0x3c46d4)();
        uint8_t* R = eng ? *(uint8_t**)(eng + 0x298) : nullptr;      /* come 0x3cd5f0/0x3cd5f4 */
        if (!R || mgr != proto::mgr || !view8::split) return;
        if (mgr[0xbe8] || *(int32_t*)(mgr + 0xbd8) != 2) return;     /* solo il ramo 4P di 0x3cd5c8 */
        int n = *(int32_t*)(R + 0x4228); uint8_t* g = *(uint8_t**)(R + 0x4230);
        int used = 4 + proto::Used(); int cols = proto::Cols();
        if (!g || n < used) return;
        for (int i = 0; i < n; i++)
            g[i * 0x410 + 0x409] = (i < used) ? (uint8_t)(i / cols) : 0xff;   /* fase = riga della cella */
        R[0x4239] = 2;                                                /* 2 righe */
        Logging.Log("[mk8dx-8p] 6c: fasi gruppi %d viste, righe 2", used);
    }
};
HalfPhase8Hook::InstallAtOffset(0x3cd5c8);
```
- `0x3cd5c8` è chiamata solo da `0x3cbf30` (cambio modo/scena). Se R viene ricreato dopo (RenderSceneCreateHook) i
  gruppi tornano a `0xff` (init `0xafec6c`) finché `0x3cd5c8` non riparte: verificare con il log di §4 M1 che le fasi
  restino applicate in gara; se no, riapplicare la stessa scrittura in `proto::PerFrame` (idempotente, nessun costo).
- Memoria: zero nuove allocazioni (gruppi già 8). Cache dal pool: 8 x 480x540 = 4 x 960x540, cioè **lo stesso budget
  del 4P originale** [I: 4 byte/pixel, formato 0x1a non decodificato]; oggi 4 cache + 2 viste in più disegnate per frame.
- Effetti collaterali [I]: `0x87e538` farà saltare anche ai locali 4..7 gli aggiornamenti dei frame non disegnati,
  come ai locali 2,3 del 4P originale (comportamento del gioco, non toppa). Le viste 4..7 passano a 30 fps per vista
  come le 0..3.
- `MK8DX_GROUPFIX` (step 6, oggi 0) scriveva `.408` dei gruppi 4..7 a ogni frame: è in conflitto con `0xafecc8`
  (che riscrive `.408` nel calc di R) e va lasciato a 0.

## 4. Misure prima della correzione (solo log, niente GDB)
M1. Log in `proto::PerFrame` (gira dopo 0x3cc218, fuori dal calc di R) per i primi 8 frame dopo l'attivazione e poi
    ogni 600: `R+0x4238` (contatore), `R+0x4239`, per i=0..7 `g.408`, `g.409`, `g+0` (cache) e `g+0x1d8/+0x1dc`
    (larghezza/altezza catturate, float, scritti a `0xafef8c/0xafefbc`). Atteso oggi: 0,1 e 2,3 in stato 2/1 alternati,
    4..7 stato 0, cache non nulle solo per 0..3.
M2. Bisezione con una costante di build per prova (stessa gara, shader sincroni, poi asincroni):
    - S0: build attuale (controllo: celle 3-4 bianche).
    - S1: tutte le fasi `0xff`, `R+0x4239=1` (nessun time-slicing). Celle 3-4 corrette -> causa nel percorso fase/cache.
    - S2: fasi per riga 0,0,0,0,1,1,1,1 (correzione proposta).
    - S3: fasi per indice 0,0,1,1,0,0,1,1 (estensione "a coppie"): se S2 e S3 differiscono, conta la coerenza fase/riga.
    - S4: `MK8DX_ENABLE_EXTRA=0` con 8 locali (viste extra spente, R a 8): se le celle 3-4 restano bianche, le viste
      extra non c'entrano e conta solo la fase.
M3. Esposizione (solo se S1 non basta): ogni 60 frame per i=0..7 le voci AutoExposure
    `[[R+0x39c8+0x298]+0x1d8] + i*0x1a20`: `+0x1a10`, `+0x1a14`, `+0x1a18`; voci 2,3 con indici fermi o diversi dalle
    altre indicano un'esposizione non aggiornata.
M4. Confermare che 1P e 4P originali (MK8DX_VIEWS=8 ma 4 locali) non mostrano il difetto e loggano `R+0x4239=2`,
    fasi 0,0,1,1 (4P) / `R+0x4239=1` (1P).

## 5. Punti incerti
- Meccanismo esatto del contenuto sbagliato (pool `[0x1306018]` e sua capacità, eventuale buffer per metà, ruolo di
  `R+0x5d1c`) non ricostruito; asimmetria 0,1 vs 2,3 non dimostrata staticamente.
- Formato/dimensione della cache (`0x64ae4c` -> `0x64a680`, formato 0x1a) non decodificati.
- Significato del conteggio globale `[0x12b03b0]` usato a `0x6d2038` (valore iniziale 1; usato anche dal layerSystem):
  non legato alle viste [I].
- Causa alternativa da tenere aperta (STATE, 4 nov): hook inline di gara tra un `cmp` e il suo `b.cond` con lo stub
  vecchio che non salvava NZCV; lo stub è stato corretto, quindi un flash che persiste con lo stub nuovo la esclude.
- Il Switch 2 potrebbe non usare il time-slicing a 8; l'assegnazione per riga è l'estensione del criterio 4P, non un
  dato del gioco Switch 2.

## 6. Esiti M2 e nuova pista (4 nov)

### 6.1 Esiti [D, coordinatore/utente; shader sincroni, Mario Kart Stadium, blocco MK8DX_HALF8 in PerFrame]
| prova | viste disegnate per frame | risultato |
|---|---|---|
| S0 build attuale (0,0,1,1 + 4..7 a 0xff) | 6 | P3/P4 bianche/slavate durante l'intro |
| S1 tutte 0xff, R+0x4239=1 | 8 | 7 celle su 8 magenta/rosa, stabile; solo la vista 7 corretta |
| S2 fasi per riga 0,0,0,0,1,1,1,1 | 4 | quasi tutto corretto; flash viola intermittenti (es. 4,5,7 sbagliate, 6 corretta) |
| 4P originale | 2 | mai difetti |

- **[V] Smentita l'ipotesi del §2-3** (time-slicing come causa): senza time-slicing (S1) il difetto peggiora. La correzione
  del §3 resta valida solo come riduzione del carico; non è la correzione del difetto.
- Il difetto cresce con il numero di viste disegnate nello stesso frame. In S1 si salva solo l'ultima vista disegnata:
  è la firma "vince l'ultimo che scrive" su una risorsa unica scritta durante la registrazione delle viste e letta
  dalla GPU dopo. In S2 il difetto è intermittente e cambia vista da un frame all'altro, quindi l'ordine delle scritture
  varia: compatibile con la registrazione dei job dei layer in parallelo su 2 core (`0x667358`: priorità > 10 nella coda
  0 su 2 core; i Sub e gli extra hanno priorità 600-602) [I].

### 6.2 Risorse per frame del post-processing di R (domanda 1 del coordinatore) [V statico]
Ho ricontrollato gli init degli effetti creati da `0x6d1e40`:
- AutoExposure (`[R+0x3c60]`), HDRCompose (`[R+0x3ca8]`, voci da 0x200 per vista) e gli oggetti `+0x2a0/+0x2a8/+0x2b0/+0x2c8`
  allocano array di **n = numViews** voci; sono indicizzati con l'indice della vista e un clamp.
- Bloom (`[R+0x3c48]`, init 0xabedb0) e gli altri effetti `0xac3cbc`, `0xacb034`, `0xac8a80` **non** usano n per array:
  n finisce solo in `0x644450`, che è un `ret`. Hanno quindi un solo stato per oggetto (programma shader, parametri),
  condiviso da tutte le viste.
- AutoExposure ha anche due buffer GPU **unici** di dimensione fissa (`0x6000` e `2*0x800`, `0xabc8f8`/`0xabc944`) e a
  ogni disegno per vista chiama `0xabc9dc(AE)` sull'oggetto, non sulla voce. Per l'init sono dati statici (`0xabbe1c`,
  0x800 punti) e parametri globali, quindi in linea di principio uguali per tutte le viste.
- Contatori per frame di R+0x4d00 (`+0x758/+0x768`, azzerati a `0x6ce7f8`): capienza 0x400, non legata alle viste.
  Indice per frame `R+0x5d1c` (`0x6ab640`) → `0x6ce7cc` / `0x71fa1c`: buffer doppio per frame, non per vista.
- **Conclusione [V/I]:** non c'è nessuna capienza fissa 2/4 legata alle "viste per frame" negli oggetti di R. Le uniche
  risorse condivise tra le viste nello stesso frame sono:
  1. lo stato unico degli effetti senza array (Bloom/Glare/…);
  2. il pool globale delle texture `[0x1306018]` (temporanei di post-processing e cache "copy"), che **non** è
     ricreato con R e ha la memoria decisa all'avvio.

  Nessuna delle due è dimostrata colpevole staticamente.

### 6.3 Pista "regressione dal menu" (step 5b / step 7 B) [I, priorità]
Il dato dell'utente (gara a 8 pulita allo step 6b, 3 nov) sposta la priorità sugli effetti **persistenti** degli hook
del menu. Stato statico di ciascuno:
| hook / opzione | persiste in gara? | effetto possibile sul colore |
|---|---|---|
| **HeapCreateHook** `0x605130` con `MK8DX_P7_REC16` (= MENU3D8): TaskHeap d'avvio (tipo scena -1, lr 0x603290) `0x15b00000 -> 0x19500000` (+58 MiB) | **sì**, per tutta la sessione: l'heap nasce all'avvio | meno memoria per tutto ciò che si alloca dopo: heap di scena di R (dimensione automatica, `RenderEngine::MainModelSceneHeap`), pool texture `[0x1306018]`, buffer GPU. Con 8 viste servono più temporanei per frame: un pool esaurito o frammentato restituisce texture riusate/alias → colori casuali (magenta), peggio con più viste. **Candidato principale** |
| HeapCreateHook scena menu `0xd200000 -> MK8DX_P7_MENUHEAP` | no (heap del task menu, liberato) | frammentazione dell'heap genitore [I] |
| **ModelCreateHook** `0x69bb0c` con MENU3D8: `rlm::sceneType != 3` comprende -1 (avvio): spegne i bit 4..7 (`0x68fa38`) di tutti i modelli residenti creati all'avvio | **sì** (modelli residenti) | solo visibilità nelle viste 4..7 (moneta globale, eventuali modelli ambiente/sonde condivisi). Non spiega la tinta delle viste 0..3 in S1, ma va escluso: un modello d'ambiente invisibile nelle viste 4..7 cambia luce e riflessi solo lì |
| RenderSceneCreateHook a 8 viste nel menu (MENU3D8 && MENUR8) | no: R distrutto e ricreato a ogni scena | frammentazione dell'heap e del pool [I] |
| DrawObjHook `0x6e2430` | no: in gara chiama solo Orig (ramo `drawreg::menu8 && sceneType != 3`) [V codice] | nessuno |
| liste rec16 (record 12..15) | sì (caricatore) | memoria [I] |
| order8 E1 nel menu (`m3d8::shown`) | no: in gara rango come allo step 6b | nessuno |
| layer+0xde4 (COPY_DE4) | solo bit 0x2/0x4, in gara come allo step 6b | nessuno |

Stato che R e il post-processing condividono tra menu e gara: **solo il pool `[0x1306018]` e la memoria degli heap
genitori** (R, effetti e gruppi vengono ricreati per scena con `0x6a8f10`). Per questo la pista heap/pool è coerente con
"regressione dal menu" e con "peggiora con il numero di viste per frame".

### 6.4 Misure decisive (log leggeri, niente GDB nei percorsi caldi)
- **M5, bisezione sulle build (prima di tutto):** MENU3D8=0 (già in corso). Se il difetto sparisce:
  - a) MENU3D8=1 con `-DMK8DX_P7_REC16=0`: isola l'allargamento del TaskHeap d'avvio;
  - b) MENU3D8=1 con la parte ModelCreateHook (ciclo `0x68fa38` b=4..7) limitata a `rlm::sceneType == 1`: isola le
    maschere dei modelli d'avvio.
- **M6, memoria (già quasi pronta):** HeapCreateHook logga gli heap ≥ 1 MiB con "allocabile prima". Confrontare tra
  MENU3D8=0 e 1:
  - la riga di `RenderEngine::MainModelSceneHeap` della gara (dimensione e allocabile);
  - la riga "misura: ... mem usata" di PerFrame.

  Aggiungere un trampolino di solo log su `0x64ae4c` (pool acquire, prima istruzione `d10143ff`) che conti, per frame
  di gara, le acquisizioni e i ritorni nulli. In alternativa, per non toccare un percorso caldo: un contatore atomico
  e un log ogni 600 frame.
- **M7, effetto colpevole:** in `proto::PerFrame` (dopo 0x3cc218) scrivere a ogni frame
  `uint8_t* ae = *(uint8_t**)(R + 0x3c60); if (ae) ae[0x840] = 0;`.
  `+0x840` è il flag IsEnable letto a 0xabcb68/0xabcc28/0xabcf30/0xabd78c/0xabd964 e da 0x6d270c (HDRCompose). Se la
  tinta sparisce, la causa è nell'esposizione; altrimenti ripetere sul Bloom (`[R+0x3c48]`, flag di abilitazione
  `+0x798` scritto a 1 all'init 0xabee30 [I]).
- **M8, parallelismo:** in LayerDrawJobHook salvare in un array statico (slot, core = `svcGetCurrentProcessorNumber`,
  tick) per i layer 3D nei primi 4 frame dopo l'attivazione, e loggare una volta. Due viste registrate insieme su core
  diversi più stato unico condiviso spiegherebbero l'intermittenza di S2.

### 6.5 Correzione (condizionata alle misure)
- Se M5a lo conferma (TaskHeap d'avvio): spostare i record 12..15 di rec16 in un heap allocato solo nel menu e
  liberato prima della gara, oppure sottrarre lo stesso spazio a BackgroundLoad invece di allargare il TaskHeap
  d'avvio. Obiettivo: memoria libera all'ingresso in gara identica allo step 6b. Nessuna patch di parola sul post-processing.
- Se M5b lo conferma: limitare il ciclo `0x68fa38(model, false, 4..7)` di ModelCreateHook a `rlm::sceneType == 1`
  (modelli del menu), come prevedeva il commento dello step 7 B, e non ai modelli residenti d'avvio.
- Se M7 lo conferma (esposizione, indipendente dal menu): servono le misure M3 sulle voci AE prima di proporre una
  patch; staticamente le voci sono già 8.

### 6.6 Certezza
- Time-slicing come causa: smentito [D].
- Capienza fissa per "viste per frame" negli oggetti di R: non trovata [V statico]; non esclusa nel pool `[0x1306018]` [I].
- Causa più probabile: memoria ridotta dagli heap allargati all'avvio da MENU3D8/REC16 → pool texture/heap di scena
  insufficienti con 8 viste. Certezza **bassa-media (~40%)**, da decidere con M5/M6.

## 7. Bisezione a runtime (4 nov 2026, pomeriggio) [D, valutazione a occhio dell'utente, gara a 8 a Mario Kart Stadium]
| build | flash |
|---|---|
| MENU3D8=1 completa (REC16+RING9) | frequenti (celle bianche/viola), peggio con shader asincroni |
| HALF8=1 (8 viste per frame) | 7 celle su 8 magenta, solo la vista 7 corretta |
| HALF8=2 (fasi per riga, 4 viste per frame) | quasi pulita, flash viola sparsi |
| MENU3D8=0 | pulita (un mini flash) |
| MENU3D8=1, REC16=0 | pulita; controllo ripetuto: 3 flash in una gara lunga, e flash in almeno una vista aprendo il menu di pausa |
| REC16=1, REC16GATE=1 (record 12..15 solo nel menu) | flash piu' frequenti |
| REC16=1, RECMEM=0 (nessun heap allargato) | flash, meno frequenti |
| REC16=1, RECHOOKS=0 (nessun hook delle liste, heap allargati) | flash frequenti |
| REC16=1, RECHOOKS=0, RECMEM=0 (solo sys 0x9a50 -> 0xb4f0, +6,8 KB) | flash frequenti |
Conclusione [I forte]: difetto latente sensibile alla disposizione in memoria (o al tempo), presente gia' prima (STATE 3 nov: flash "da
qualche build"), reso piu' visibile da piccoli spostamenti delle allocazioni d'avvio; cresce con le viste disegnate per frame. Opzioni di
build per la bisezione: MK8DX_HALF8, MK8DX_P7_REC16GATE, MK8DX_P7_RECMEM, MK8DX_P7_RECHOOKS (default = comportamento normale).
