# Step 7b – 8 anteprime 3D nella selezione a 8: verifica delle tappe 0-2 e piano di patch (statico)

Data: 3 nov 2026. Solo analisi statica: `src/` non modificato, nessuna build, Eden e GDB non usati.

**Basi usate**: `../img/main.bin`, `../img/main.ann.asm`; piani `step5b_plan_p7.md` §5 (piano a tappe), `p5`, `p4`; `STRUCTS.md`;
`src/program/main.cpp` (namespace `proto`, `view8`, `p4`, `p5`); log già esistenti di Eden (`report/step*_data/eden_log_*`) solo per i tipi di scena.

**Verifica**: `step7b_data/p7b2_verify.py` → `step7b_data/p7b2_verify.txt`, **216 OK, 0 FAIL**. Lo script legge solo `main.bin`:
- parole a 4 byte, `bl`/`b` ricalcolati, puntatori nella GOT, stringhe;
- decodifica degli 8 preset di posa/camera: emula le istruzioni di 0x14a408 lette da `main.bin`.

Etichette: **[VERIFICATO]** = istruzione citata e controllata dallo script; **[IPOTESI]** = da misurare nella tappa 0.

---

## 0. Verdetto

**Le 8 anteprime 3D sono realizzabili.** Il blocco non è una viewport per modello impossibile da estendere. Il menu usa
**lo stesso meccanismo dello split-screen di gara**, che la mod ha già portato a 8 viste in gara (step 2/6):
- ogni anteprima è un **KartUnit** visto da una **vista di scena** propria (layer 3D Sub *k*) con **camera propria**;
- le anteprime sono distinte da **maschere di visibilità per vista**: ogni modello ha un bit per vista (u16 nelle shape);
- **tutti i kart e i piedistalli stanno nello stesso punto del mondo**, (−120, 0, −120);
- ogni vista mostra solo il proprio kart, il proprio piedistallo e la propria camera, con uno "spostamento di proiezione" (lens shift) che sposta il
  modello nel quadrante.

**Quello che c'è già e serve anche nel menu** (oggi è attivo solo con `rlm::sceneType == 3`):
- renderer "Main" a N viste (0x6a8f10);
- modelli g3d a 8 viste (0x69bb0c, già sempre attivo);
- 4 Layer3D extra con camera, proiezione e rect propri (`proto::layer[]`);
- collegamento al renderer (0x3cce58 → 0x68d4f4).

**Cosa manca: tutti limiti "a 4" locali, nessuno strutturale.**

| limite | indirizzo | rimedio |
|---|---|---|
| sorgenti camera del CameraDirector per vista = 4 | 0xc8e98 `cmp w22,#4` | la mod calcola la camera delle viste 4..7 dal descrittore dell'oggetto menu-kart (+0x80), come `proto::DescCam` in gara |
| viewport della Scene = 5; traduzione maschera → bit di vista srotolata su 0..3 | 0x7e7bf8..0x7e7cc8, 0x16e2cc..0x16e3bc | la mod imposta direttamente i bit 4..7 con 0x113c88 |
| configurazione degli oggetti menu-kart srotolata su 4 | 0x13e634..0x13e6f4 | trampolino 0x13e59c |
| tabella preset `{3,4,5,6}[slot]` | 0x14771c | inline 0x147738 |
| viewer dei piedistalli a 4 voci, accessori senza controllo dell'indice | 0x430ce0 | 6 trampolini |
| heap KartWork del menu (**24,4 MiB**, non 36) | 0x13da10/0x13da24 | inline 0x13da28 |

**Costo/rischio: 4-6 sessioni, rischio medio-alto** (invariato rispetto a p7 §5.4, ma ora i rischi sono identificati):
- **R1 (alto, blocca l'avvio).** `0x8843a8(i)` aspetta **senza limite** che le risorse del descrittore *i* siano caricate. Se si
  forzano 8 KartUnit con i descrittori 4..7 di `settings` vuoti, il menu si blocca. → T1 riempie e poi ripristina i descrittori 4..7.
- **R2 (medio).** Memoria: 8 KartUnit nell'heap KartWork del menu, che oggi ne contiene 4. → si allarga l'heap; da misurare.
- **R3 (medio).** Ordine e sfondo delle viste extra nel menu (pulizia del colore, oggetti 3D di sfondo che compaiono nelle viste 4..7) → tappa 0.
- **R4 (basso).** Accessori del viewer chiamati con slot 4..7: **corruzione di memoria** (`smaddl` senza controllo). Le guardie T2 sono obbligatorie
  già dalla tappa 1.

---

## 1. T0 – Ricognizione

### 1.1 (a) KartDirector, init 0x13d778 [VERIFICATO]
- **Conteggio.**
  - `x20 = 0x86f7bc()` (settings A), 0x13d7ac/0x13d7b0.
  - `w8 = [x20+0x180]` (0x13d7b8 `b9418288`), poi `str w8,[x19,#0xc0]` (0x13d7bc `b900c268`).
  - Con `settings+8 == 5` il ricalcolo 0x86f074 conta **al massimo 4** descrittori: 0x86f078 `cmp w10,#5`, 0x86f098 `csel x10,x11(4),x10(12),eq`.
    È per questo che nel menu `+0xc0` = 4 anche con 8 giocatori.
  - Flag del director, calcolati qui: `+0xea = (modo == 5)` (0x13d878), `+0xe9 = ((modo|2) == 7)`, cioè modo 5 o 7 (0x13d880..0x13d88c).
  - Il KartUnit ha la stessa formula in `+0x360` (0x16eca4..0x16ecb0), ed è il flag che fa applicare le maschere di vista (§1.2).
  - **[IPOTESI forte]** menu = modo 5. Da confermare con un log.
- **Heap "KartWork"** (stringa 0xedcc30):
  - `w10 = 0x1864000` (0x13da10/14), `x11 = 0x2464000` (0x13da18);
  - `csel x0,x10,x11,ne` su `+0xe9` (0x13da24), poi `0x605128` (0x13da50);
  - **nel menu (`+0xe9 = 1`) l'heap è di 0x1864000 = 24,4 MiB**. In gara è di 0x2464000 = 36,4 MiB per 12 kart. Il p7 §5.3 ("36 MB nel menu") va corretto.
- **Array.**
  - `ldrsw x21,[x19,#0xc0]; lsl x9,x21,#3; bl 0x5fa9c0` (0x13da78..0x13da90); `str x0,[x19,#0xc8]` (0x13da98). La dimensione dipende da `+0xc0`.
- **Ciclo di creazione** (0x13dab4..0x13db5c, limitato da `+0xc0`):
  1. `0x8843a8(sys = 0x8ace44(), i)` **attende** il bit `0xf73d14[i] = 1<<(i+2)` in `sys+0x5040`. È un ciclo con sleep 0x618898 e `cbnz`
     0x884460, senza timeout (esce solo se `sys+0x5050 == 0`). La tabella ha 12 voci.
  2. `new 0x368` (0x13daac `mov w22,#0x368`).
  3. `0x16e3dc(unit, i, +0xe9)`: ctor KartUnit, lo stesso della gara.
  4. `+0xc8[i] = unit`, poi `0x16ca28(unit)`.
- **Uso di *i*.**
  - È l'**indice del descrittore** di `settings` (+0x30 + i·0x1c, 12 voci).
  - `0x884bd8(sys, +0xe9, settings, +0xc0)` (0x13d98c), chiamata prima del ciclo, richiede le risorse **solo** per i descrittori con
    `tipo [+0x44] == 0` (0x884c9c/0x884ca0 `cbnz`), con clamp 12 (0x884c90).
  - L'attesa 0x8843a8 usa lo stesso *i*: un descrittore 4..7 vuoto o non locale **blocca** l'init (R1).
  - [IPOTESI forte] 0x16ca28 legge personaggio e parti dal descrittore *i*, come in gara (KartUnit+0x40 = indice kart, STRUCTS).
- **Secondo ciclo** (0x13dbd8..0x13dc18): `unit+0x18 = obj1a0+0x60[i]` solo se `i < obj1a0+0x58` (0x13dbe8/0x13dbf4 `b.ls → xzr`).
  È sicuro con *i* = 4..7.
- **Lettori di `+0xc0/+0xc8` con indici fissi** (scansione di `ldr [x,#0xc8]` seguito da un offset costante in 0x100000..0x180000, più gli
  accessori):
  - **unico sito srotolato: 0x13e59c** (configurazione degli oggetti menu-kart, §1.1.1);
  - accessori per slot 0x13d47c, 0x13d4ac, 0x13d4f8, 0x13d540: controllano `+0xc0` prima dell'accesso;
  - 0x13d424 non lo controlla: c'è già la guardia `Model3DSlotHook`;
  - i cicli del calc (0x13ee94..) sono limitati da `+0xc0`;
  - nessun `cmp #4` sugli slot nella classe (0x13d000..0x13f800).

#### 1.1.1 Oggetti menu-kart (= KartUnit+0x30, ctor 0x147678) [VERIFICATO]
- `0x13d56c(dir, n, &arg)` (`str w1,[x0,#0xec]`, `strb #1,[x0,#0xeb]`) viene chiamata dalla scelta del numero di giocatori 0x4d9a2c con
  `w22 = 4` (0x4d9914) per **ogni** multi-giocatore, quindi anche con 5-8. Al calc seguente, 0x13ee7c chiama `0x13e59c(dir, n, &arg)`.
- 0x13e59c:
  - `tipo = {0,1,2,2}[n-1]` (0x13e5d8..0x13e5e8, 0x13e618, nessun clamp);
  - poi **4 chiamate srotolate** `0x147678(+0xc8[s]+0x30, &tipo, s, &arg)` per s = 0..3, ciascuna dietro `cmp +0xc0,#s+1`
    (0x13e634/0x13e674/0x13e6b4/0x13e6f4).
  - Con 8 KartUnit, **gli oggetti 4..7 non vengono mai configurati**, quindi il loro `+0xc0` (slot) e `+0xc4` (tipo) restano quelli del ctor
    [IPOTESI: da leggere].

### 1.2 (b) Viewer 3D del menu (`[0x398898()+0x270]+0x70`) [VERIFICATO]
- **Correzione a p7.** Le voci partono da **+0x568**, con passo **0x138**: voce *i* = `v+0x568+i·0x138`, quindi 0x568/0x6a0/0x7d8/0x910. Dopo le
  4 voci, a +0xa48, c'è un **altro oggetto**.
  - ctor 0x42fc84 (unico chiamante 0x425cf0), ciclo 0x42fde8..0x42feb4: `add x24,#0x138`, `cmp x24,#0x3a8`.
- **Ogni voce è un attore "ターンテーブル/BaseTable" autonomo.**
  - vtable `[GOT 0x1308350]+0x10 = 0x1267358`:
    - vt+0x10 = 0x42cdc0 init: carica il modello con vt+0x48 = loader generico 0x4298f8, da `UI3D/BaseTable/BaseTable.bfres` (vt+0x30)
      con nome "BaseTable" (vt+0x38); alloca un'animazione 0x278 (0x8ca144/0x8cab08);
    - vt+0x18 = 0x42d04c reset;
    - vt+0x20 = 0x42d2cc calc.
  - Campi della voce (offset dalla voce):

    | offset | contenuto | init del ctor |
    |---|---|---|
    | +0x00 | vtable | — |
    | +0x08 | **modello** (= v+0x570) | 0 |
    | +0x10..+0x3c | matrice 3×4, traslazione a +0x34..+0x3c (= v+0x59c..0x5a4) | identità da GOT 0x12fa188 |
    | +0x40/+0x42 | u16 | 0xffff |
    | +0x44 | float (v+0x5ac, scritto da 0x430d7c) | — |
    | +0x48 | oggetto animazione (v+0x5b0) | — |
    | +0x50 | 0x30 B | memcpy |
    | +0x80 | oggetto | 0x8c8704 |
    | +0xc8/+0xca | animazione (v+0x630/0x632) | 0xffff |
    | +0x100 | 12 B | GOT 0x1308358 |
    | +0x10c | 0x24 B | GOT 0x12fcc58 |
    | +0x130 | byte | — |

- **Cicli srotolati sulle 4 voci** (nessun `cmp #4`):
  - init 0x430154 (vt+0x10 ×4, 0x4301a0..0x4301e0);
  - reset 0x430230 (vt+0x18 ×4);
  - **calc per frame 0x430360** (vt+0x20 ×4, 0x430374..0x4303ac; unico chiamante 0x4263b8).
  - Il disegno lo fa il renderer di scena (modello g3d con maschera di vista), non un draw del viewer [IPOTESI forte: nessun draw esplicito
    nella classe].
- **I 6 accessori.** Ognuno ha **un solo chiamante**, il thunk 0x42764x (`ldr x0,[x0,#0x70]; b …`). Nessuno controlla *i* (`smaddl x?,w1,#0x138,x0`).

  | target | thunk | effetto | chiamanti del thunk (tutti nella classe menu-kart, w1 = `obj+0xc0`) |
  |---|---|---|---|
  | 0x430cc4(v,i,on,vista) | 0x427648 | `0x68fa38(modello, on, vista)` = bit *vista* della maschera di vista | 0x14860c, 0x148648, 0x148684, 0x1486cc (viste 0..3, bit da `KartUnit+0x362`) |
  | 0x430d5c(v,i,&pos) | 0x427654 | traslazione +0x59c..+0x5a4 | 16 siti: 0x147850, 0x148418, 0x1484a8, 0x148878, 0x148960, 0x148d58, 0x148ebc, 0x148f78, 0x149010, 0x1495e4, 0x1499f4, 0x149cd8, 0x149d94, 0x149e34, 0x149f90, 0x14a020 |
  | 0x430d7c(v,i,f) | 0x42765c | +0x5ac (= `obj+0xdc`, rotazione) | 0x1483e8 |
  | 0x430d8c(v,i) | 0x427664 | stato animazioni → `[+0x5b0]+0x120+0xf8` | 0x148d8c, 0x149a88 |
  | 0x430e24(v,i,&x) | 0x42766c (`bl`) | `0x42cf6c(voce, …)` | 0x148084 (da 0x148038 = cambio personaggio) |
  | 0x430e58(v,i) | 0x427698 | animazione +0x630/+0x632 | 0x148140 (da 0x148108) |

  Con slot 4..7 si scrive fino a `v+0x568+7·0x138+0x5a4 = v+0xe24`, cioè **fuori dalle voci** (R4).
- **Maschere di vista** (meccanismo chiave):
  - `0x68fa38(m, on, b)`: per ogni shape, `strh (mask | 1<<b)` a shape+0xc (0x68fa58 `lsl w9,w9,w2`, 0x68fa74). La maschera è u16, quindi
    ci stanno 16 viste.
  - **Kart.**
    - La Scene costruisce `Scene+0xc0[12]` (u8): `mask[vp.kart] |= 1<<vp` per i 5 viewport (0x7e7af8: 0x7e7bd4, 0x7e7bf8..0x7e7cc8).
    - `0x16f000` copia `mask[unit+0x40]` in `unit+0x362` (0x16f020/0x16f028). Se `unit+0x360` (menu) chiama `0x16e184(lista, mask)`,
      che traduce bit vp → `1<<vp.slot` (0x16e1c8).
    - Poi applica **solo le viste 0..3**: `0x113c88(nodo, on, 0..3)` srotolato (0x16e2cc..0x16e3bc), con `0x113c88 = 0x68fa38(nodo+0x40, …)`.
  - **Piedistallo dello slot s.** 0x1485d4 legge `KartUnit[s]+0x362` e chiama `0x427648(v, s, bit_k, k)` per k = 0..3.
  - **I bit 4..7 non li tocca nessuno.** Il valore di default delle shape (probabilmente tutti 1) è [IPOTESI] da misurare.

### 1.3 (c) Preset di posa/camera e posizione a schermo [VERIFICATO salvo indicazione]
- **ctor 0x147678(obj, &tipo, slot, &arg)**: `stp w2,w8,[x19,#0xc0]` (+0xc0 slot, +0xc4 tipo, 0x1476a4).
  - La tabella `{3,4,5,6}` viene scritta **in modo pigro** da questo ctor in `0x131f478` (0x1476bc..0x1476d0).
  - Tipo 2: `preset = [0x131f450+0x28+slot·4]` (0x147714..0x14771c, **senza clamp**).
  - Poi `x8 = 0x131f450 + k·0x74` (0x147734); 0x74 B da `x8+0x38` copiati in `obj+0x8..+0x78` (0x147738..0x147804).
  - **Posizione: unica per tutti gli slot.** `obj+0xec..+0xf4 = 0x131f450+0x1c..+0x24` (0x147808..0x14781c).
    - Vale **(−120, 0, −120)**: 0x14a79c/0x14a7a0, decodificato.
    - Va al piedistallo con `0x427654(v, slot, obj+0xec)` (0x147850) e alla posa del kart (0x148338).
    - [IPOTESI forte] Nessun altro scrittore di `obj+0xec`: tutti i modelli del menu sono nello stesso punto.
- **Struttura dei preset (decodificata da 0x14a408).** Un preset = 3 record di 7 float (**A** `obj+0x8`, **B** `obj+0x24`, **C** `obj+0x40`)
  più 8 float (`obj+0x5c..+0x78`).
  - Il record attivo lo sceglie 0x147960 in base allo stato (+0x191, +0xd7): 0x147a14..0x147a2c.
  - Campi del record: **{offset camera xyz, pitch (gradi), fovy (rad), spostamento proiezione x, y}**.
    - Lo dicono 0x147a50/0x147a60/0x147a78: `+0x140 = rec+0xc`, `+0x150 = rec+0x10`, `+0x160 = rec+0x14`.
    - Da `+0x160` (rec+0x14) vengono i due float salvati in `obj+0xb4/+0xb8` (0x147d90).

  | preset (uso) | record A | resto (8 float, significato [IPOTESI]) |
  |---|---|---|
  | 0 (1P) | (0,15,−52.6) p −8.28 fov 0.75 sh (−0.27, 0) | −60 0 0 140 120 0 0 1 |
  | 1 / 2 (2P sx/dx) | (0,15.25,−77) p −8.28 fov 0.75 sh (±0.22, 0) | ±60 0 0 140 ±90 0 0 1 |
  | **3** (Multi slot 0, alto-sx) | (0,20,−79) p −8.46 fov 0.5 sh **(+0.22, +0.062)** | 40 0 0 140 60 0 0 1 |
  | **4** (slot 1, alto-dx) | idem, sh **(−0.22, +0.062)** | −40 0 0 140 −60 0 0 1 |
  | **5** (slot 2, basso-sx) | idem, sh **(+0.22, 0)** | 40 … 60 … |
  | **6** (slot 3, basso-dx) | idem, sh **(−0.22, 0)** | −40 … −60 … |
  | 7 (tipo 3) | (0,40,−145) p 69.5 fov 0.3 sh (0,0) | 0 0 0 200 0 0 0 1 |

  B è uguale ad A in 3..6. C (zoom/variante) ha (0,68,−92), p −32.5, stesso spostamento. Il dump completo è in `p7b2_verify.txt`.
- **Descrittore camera = `obj+0x80`** (0x147ab4), con lo stesso schema del descrittore di gara letto da `proto::DescCam`:

  | campo | offset da `obj+0x80` | offset in `obj` | prova |
  |---|---|---|---|
  | eye | +0xc | +0x8c..+0x94 | 0x147d8c/0x147d70 |
  | at | +0x18 | +0x98..+0xa0 | 0x147d74 |
  | up | +0x24 | +0xa4..+0xac, = (0, cos, −sin) | 0x147b64..0x147b7c |
  | fovy | +0x30 | +0xb0 | 0x147da8 |
  | **spostamento** | +0x34/+0x38 | +0xb4/+0xb8 | 0x147d90 |

- **Una camera e una vista per modello.** 0x147dcc confronta `obj+0xc0` con il `kart` dei 5 viewport della Scene (`Scene+0x40+k·0x18` attivo
  1|3, `+0x4c+k·0x18` kart; 0x147e24..0x147f48). Sul primo *k* uguale chiama
  `0xc9074(CameraDirector+0x1f8, k, obj+0x80, &3, 0x1e)` (0x147f58..0x147f90; CameraDirector = director 6, 0xc7a48).
  - Il gestore ha **4 slot camera da 0xd0** (`cmp w22,#4; csel` a 0xc8e98).
  - Per slot 4..7 non trova nessun viewport (0x147f48 `b.ne`), quindi nessuna camera e nessun crash.
- **Rendering della scena del menu.**
  - [VERIFICATO nei log] Il renderer "Main" del menu viene creato a **4 viste** (`render scene "Main" viste 4 -> 4 (tipo scena 1)`, step2 run5).
  - SceneInit con tipo 2 dà `modo=2, slot->vp={0,1,2,3}`, vp0..3 attivi con kart 0..3, e RLM `modo=3`: righe `proto diag … tipo=2 modo=3`
    nelle sessioni di selezione (step5 `eden_log_5a_racemgr8`, `5b_padui`).
  - [IPOTESI forte] Selezione Multi = 4 viewport di scena (layer 3D Sub0..3), uno per modello.
  - Il rect del modo 3 (tabella bss 0x133eacc) va letto a runtime. L'ipotesi "quadranti" è coerente con lo spostamento: 0.22 × 640 = 141 →
    modello a x = ±461, contro le finestre Multi a ±460.

**Risposta alla domanda chiave.** C'è **una camera e una viewport per modello**. Non sono però "non estendibili": sono esattamente le
viste per giocatore della gara. In gara la mod le ha già portate a 8 (renderer N viste, modelli 8 viste, 4 Layer3D extra con camera e
proiezione proprie, rect per frame). Nel menu basta:
1. creare il renderer a 8 viste anche per la scena del menu;
2. attivare i 4 layer extra nella selezione a 8;
3. dare loro camera e rect;
4. accendere i bit di vista 4..7 sui modelli giusti.

---

## 2. T1 – 8 KartUnit nel menu

### 2.1 Hook
1. **T 0x13d778** (`d10303ff`), consigliato rispetto all'inline 0x13d7bc perché va gestito R1.
   - Con l'opzione `MK8DX_P7_MENU3D8` e `settings+8 == 5`:
     - salvare i descrittori 4..7 e `+0x180`;
     - per ogni descrittore 4+j non valido (tipo ≠ 0 o un campo −1), copiare il descrittore j (0x1c B) con `tipo = 0`;
     - `+0x180 = 8`, poi `Orig`, poi ripristinare.
   - In questo modo `0x884bd8` richiede le risorse 4..7 e `0x8843a8(4..7)` non resta in attesa.
   - [IPOTESI] Dopo `Orig` i KartUnit non rileggono il descrittore: i cambi di personaggio passano da 0x13d4ac(mgr, slot, &pers), che riceve il
     personaggio come argomento.
   - Il KartDirector nasce **prima** della scelta del numero di giocatori, quindi l'opzione vale in tutte le sessioni del menu. Con 1-4 giocatori i
     KartUnit 4..7:
     - restano invisibili, perché `Scene+0xc0[4..7] = 0` (nessun viewport con kart 4..7) e i layer extra sono spenti;
     - si nascondono comunque con `0x14a7ac(i,false)`.
2. **I 0x13da28** (`f9000be8`): `x0` = dimensione dell'heap KartWork, letta dopo il `csel` 0x13da24. Con il punto 1 attivo:
   `x0 = MK8DX_P7_KARTWORK`, default proposto `2 × 0x1864000 = 0x30c8000`.
   - [IPOTESI] La dimensione reale va presa dalla tappa 0, con la memoria libera dell'heap dopo 4 KartUnit.
3. **T 0x13e59c** (`d10103ff`): dopo `Orig`, configura gli oggetti menu-kart 4..7 come fa lo schema srotolato 0x13e604..0x13e6f4.
4. Guardie già presenti che restano valide: `Model3DSlotHook` (0x13d424; con `+0xc0 = 8` non scatta più per 4..7) e `Model3DModeHook`
   (0x14a038).

```cpp
#ifndef MK8DX_P7_MENU3D8
#define MK8DX_P7_MENU3D8 0                 /* 1 = 8 KartUnit nel menu (tappe 1-3) */
#endif
#ifndef MK8DX_P7_KARTWORK
#define MK8DX_P7_KARTWORK 0x30c8000        /* heap KartWork del menu con 8 KartUnit (vanilla 0x1864000) */
#endif
namespace m3d8 {
    static bool init8;                                         /* questo KartDirector ha 8 KartUnit */
    static uint8_t* Settings() { return mk8dx::Fn<uint8_t* (*)()>(0x86f7bc)(); }
    static uint8_t* Desc(uint8_t* s, int i) { return s + 0x30 + i * 0x1c; }
    static bool DescOk(uint8_t* d) { for (int k = 0; k < 4; k++) if (*(int32_t*)(d + 4 * k) == -1) return false;   /* +0x30..+0x3c */
                                     return *(int32_t*)(d + 0x14) == 0; }                                             /* +0x44 tipo */
}
HOOK_DEFINE_TRAMPOLINE(KartDirInitHook) {                  /* 0x13d778 d10303ff: init KartDirector */
    static void Callback(uint8_t* dir) {
        uint8_t* s = m3d8::Settings();
        m3d8::init8 = MK8DX_P7_MENU3D8 && s && *(int32_t*)(s + 8) == 5;
        if (!m3d8::init8) { Orig(dir); return; }
        uint8_t keep[4 * 0x1c]; int32_t n0 = *(int32_t*)(s + 0x180);
        memcpy(keep, m3d8::Desc(s, 4), sizeof keep);
        for (int j = 0; j < 4; j++) if (!m3d8::DescOk(m3d8::Desc(s, 4 + j))) {
            memcpy(m3d8::Desc(s, 4 + j), m3d8::Desc(s, j), 0x1c); *(int32_t*)(m3d8::Desc(s, 4 + j) + 0x14) = 0; }
        *(int32_t*)(s + 0x180) = 8;                            /* letto a 0x13d7b8 -> +0xc0 = 8 */
        Orig(dir);
        memcpy(m3d8::Desc(s, 4), keep, sizeof keep); *(int32_t*)(s + 0x180) = n0;
        Logging.Log("[mk8dx-8p] m3d8: KartDirector %p modelli %d (prima %d)", dir, *(int32_t*)(dir + 0xc0), n0);
    }
};
HOOK_DEFINE_INLINE(KartWorkSizeHook) {                     /* 0x13da28 f9000be8: x0 = dimensione heap KartWork */
    static void Callback(exl::hook::InlineCtx* ctx) { if (m3d8::init8) ctx->X[0] = MK8DX_P7_KARTWORK; }
};
HOOK_DEFINE_TRAMPOLINE(MenuKartCfgHook) {                  /* 0x13e59c d10103ff: (dir, n posti, &arg) */
    static void Callback(uint8_t* dir, int32_t n, int32_t* arg) {
        Orig(dir, n, arg);
        int32_t cnt = *(int32_t*)(dir + 0xc0); uint8_t** u = *(uint8_t***)(dir + 0xc8);
        static constexpr int32_t kTipo[4] = { 0, 1, 2, 2 };   /* come 0x131f32c (0x13e5d8..0x13e5e8) */
        for (int i = 4; i < cnt && i < 8; i++) {
            uint8_t* obj = u && u[i] ? *(uint8_t**)(u[i] + 0x30) : nullptr; if (!obj) continue;
            int32_t t = (n >= 1 && n <= 4) ? kTipo[n - 1] : 2, a = *arg;
            mk8dx::Fn<void (*)(uint8_t*, int32_t*, int32_t, int32_t*)>(0x147678)(obj, &t, i, &a);
        }
    }
};
```
- **Installazione**: insieme a `Model3DSlotHook` (blocco p4), solo se `MK8DX_P7_MENU3D8`.
- **Rischi residui.**
  - Gli oggetti 4..7 configurati come slot i ≥ 4 chiamano subito gli accessori del viewer con *i* ≥ 4 (0x147850): **T2 va installato insieme a
    T1**.
  - Chi assume `+0xc0 <= 4` fuori dalla classe: in UI c'è solo la vecchia assunzione "modelli = posti", coperta da `p5::Hide3D`
    (0x14a7ac per i < 4). Con 8 KartUnit, `Hide3D` va esteso a i < 8.
  - [IPOTESI] Costo per KartUnit nell'heap: 24,4 MiB / 4. Il dato non è distinguibile staticamente dallo spazio libero: va misurato.

## 3. T2 – Piedistalli (viewer a 4 voci)

### 3.1 Guardie obbligatorie: 6 trampolini [VERIFICATO: unico chiamante = thunk; prime parole senza istruzioni relative al PC]
| target | prima parola | con (uint)i ≥ 4 |
|---|---|---|
| 0x430cc4 (v,i,on,vista) | a9bd57f6 | redirect (variante A: niente; variante B: voce extra) |
| 0x430d5c (v,i,&pos) | 52802708 | idem |
| 0x430d7c (v,i,f) | 52802708 | idem |
| 0x430d8c (v,i) | f81d0ff5 | idem |
| 0x430e24 (v,i,&x) | d10083ff | idem |
| 0x430e58 (v,i) | f81d0ff5 | idem |

### 3.2 Variante A (consigliata per prima): piedistallo condiviso, 0.5 sessione
Tutti i piedistalli sono nello stesso punto (−120,0,−120) e si distinguono solo per il bit di vista. Basta quindi mostrare **il piedistallo j
anche nella vista 4+j** (bit 4+j sul suo modello). Le chiamate del gioco con *i* = 4..7 vengono ignorate.

Effetto collaterale: P(4+j) eredita la rotazione e l'animazione del piedistallo di P(j+1): +0x5ac da `obj+0xdc`, +0x630.
```cpp
#define M3D8_GUARD(N, SIG, ARGS) HOOK_DEFINE_TRAMPOLINE(Vw##N##Hook) { static void Callback SIG { \
    if ((uint32_t)i >= 4) return; Orig ARGS; } };
M3D8_GUARD(Vis,  (uint8_t* v, int32_t i, bool on, int32_t vw), (v, i, on, vw))   /* 0x430cc4 */
M3D8_GUARD(Pos,  (uint8_t* v, int32_t i, float* p), (v, i, p))                   /* 0x430d5c */
M3D8_GUARD(Rot,  (uint8_t* v, int32_t i, float f), (v, i, f))                    /* 0x430d7c (s0) */
M3D8_GUARD(Anm,  (uint8_t* v, int32_t i), (v, i))                                /* 0x430d8c */
M3D8_GUARD(Set,  (uint8_t* v, int32_t i, int32_t* x), (v, i, x))                 /* 0x430e24 */
M3D8_GUARD(Anm2, (uint8_t* v, int32_t i), (v, i))                                /* 0x430e58 */
namespace m3d8 {
    static uint8_t* Viewer() { uint8_t* s = mk8dx::Fn<uint8_t* (*)()>(0x398898)(); uint8_t* o = s ? *(uint8_t**)(s + 0x270) : nullptr;
                               return o ? *(uint8_t**)(o + 0x70) : nullptr; }
    static uint8_t* Dir() { uint8_t* d = mk8dx::Fn<uint8_t* (*)()>(0x7e7434)(); d = d ? *(uint8_t**)(d + 0x190 + 4 * 8) : nullptr;
                            return d ? *(uint8_t**)(d + 0x238) : nullptr; }
    static void Pedestals(bool on8) {                           /* per frame, selezione/kart a 8 */
        uint8_t* v = Viewer(); uint8_t* d = Dir(); if (!v || !d) return;
        for (int j = 0; j < 4; j++) for (int b = 4; b < 8; b++) {
            uint8_t* o = (b - 4 == j && *(int32_t*)(d + 0xc0) > b) ? mk8dx::Fn<uint8_t* (*)(uint8_t*, int)>(0x13d47c)(d, b) : nullptr;
            bool show = on8 && o && o[0xd4];                    /* +0xd4 = visibile (0x14a7ac) [IPOTESI: intento di visibilita'] */
            VwVisHook::Orig(v, j, show, b);
        }
    }
}
```
### 3.3 Variante B: 4 voci vere, 1.5 sessioni, rischio medio
Si costruiscono 4 voci in un buffer della mod di 4×0x138 (allineamento 16).
1. **Costruzione**, nel trampolino **0x430154** (init del viewer, `f81e0ff3`) dopo `Orig`, così il loader usa lo stesso heap corrente. Per ogni voce
   si replica il ctor 0x42fdf0..0x42feb0:
   - `+0 = [GOT 0x1308350]+0x10`; `+0x8 = 0`;
   - `+0x10..+0x3c` = identità `[GOT 0x12fa188]` disposta come nel ctor (righe → colonne);
   - `+0x40/+0x42 = 0xffff`; `+0x44 = 0`; `+0x48 = 0`; `memcpy(+0x50, [GOT 0x12fa188], 0x30)`; `0x8c8704(e+0x80)`;
   - `+0xc8/+0xca = 0xffff`; `+0xfc = 0`; `+0x100 = 12 B da [GOT 0x1308358]`; `memcpy(+0x10c, [GOT 0x12fcc58], 0x24)`; `+0x130 = 0`;
   - poi `vt+0x10(e)` (0x42cdc0).
2. **Reset e calc**: trampolini **0x430230** (reset, `f81e0ff3`) e **0x430360** (calc, `fc1e0fe8`), che dopo `Orig` chiamano vt+0x18 / vt+0x20
   delle voci extra.
3. **Accessori**: con i ≥ 4 si opera sulla voce extra i−4. Si replica il corpo del target con `voce = extra + (i−4)·0x138 − 0x568 + v`: offset
   identici, perché gli accessori usano solo `v + i·0x138 + off`.
4. **Distruzione**: il viewer non conosce le voci extra.
   - [IPOTESI] Serve un trampolino sul dtor del viewer (da trovare: vtable di 0x42fc84), che liberi modello e animazione (`vt+0x08`?).
   - Altrimenti si ha un modello pendente a ogni uscita dal menu.
5. Le maschere di vista delle voci extra: `0x430cc4(v, 4+j, on, 4+j)`, più i bit 0..3 spenti.

## 4. T3 – Viste, camere e preset per il layout 8pl (indicazioni)

### 4.1 Infrastruttura (riuso di `proto`)
- **`RenderSceneCreateHook`**: oggi estende "Main" solo con `rlm::sceneType == 3`. Va esteso anche alla scena del menu: tipo 1 alla creazione
  [VERIFICATO nel log `render scene "Main" viste 4 -> 4 (tipo scena 1)`], con `MK8DX_P7_MENU3D8`.
  - `RlmAttachHook` collega già i layer extra se `R+0xf0 > 4+j`.
- **`proto::PerFrame`**: nuovo ramo "menu 8" (scena tipo 1/2, `p4::one` o selezione con +0x8c0 > 4). Ogni frame:
  1. accende `layer[0..3]` (0x6651a8) quando la selezione o la kart a 8 è aperta;
  2. imposta i rect delle 8 celle su `sub[0..3]` (sovrascrivendo il modo 3 del RLM) e su `layer[j]`, con aspetto `[proj]+0xb0` e flag
     `[proj]+0x8 = 1`;
  3. imposta la camera di `layer[j]` dal descrittore `obj(4+j)+0x80`, come `DescCam`: `cam+0x38 = obj+0x8c`, `+0x44 = obj+0x98`,
     `+0x50 = obj+0xa4`, `0x61bdd0(proj, obj+0xb0)`, `0x619a08(cam, cam+8)`;
  4. aggiunge lo spostamento della proiezione `obj+0xb4/+0xb8` → `[proj]+0xb4/+0xb8`.
     - [IPOTESI forte] Layout `sead::PerspectiveProjection`: +0xa0 fovy, +0xa4..+0xac sin/cos/tan, **+0xb0 aspetto** (già usato dalla mod),
       **+0xb4/+0xb8 offset**.
     - Da confermare in tappa 0 confrontando `[sub[k]+0x70]+0xb4` con `obj(k)+0xb4`.
- **Maschere dei kart (bit 4..7).** Per frame, per *u* = KartUnit 0..7:
  ```cpp
  int32_t sel = *(int32_t*)(u + 0x348); uint8_t* lst = u + 0x248 + ((uint32_t)sel < 2 ? sel : 0) * 0x78;   /* come 0x16f030..0x16f048 */
  for (uint8_t* nd = *(uint8_t**)(lst + 8); nd != lst; nd = *(uint8_t**)(nd + 8))
      for (int b = 4; b < 8; b++) mk8dx::Fn<void (*)(uint8_t*, bool, int)>(0x113c88)(*(uint8_t**)(nd + 0x10), on8 && i == b && obj(i)[0xd4], b);
  ```
  - Per i kart 0..3 i bit 4..7 restano spenti.
  - **Cosa sono i nodi.** 0x16e184 itera `nodo+0x8` (successivo) e passa `nodo+0x10` a 0x113c88 [VERIFICATO 0x16e288..0x16e2b4]. Il wrapper usa
    `+0x40` = modello.
- **Camere 0..3**: restano del gioco (0x147dcc / CameraDirector). I preset nuovi valgono anche per gli slot 0..3, così le 8 celle sono coerenti.

### 4.2 Preset 8pl: campi da cambiare e come derivarli
- **Sito.** Inline **0x147738** (`b9403909`, la prima lettura del preset dopo `madd` 0x147734). Se `obj+0xc4 == 2` e la modalità 8P è attiva:
  `X[8] = (uintptr)&preset8[slot] − 0x38`.
  - Così le 29 parole copiate in `obj+0x8..+0x78` vengono dal preset della mod.
  - `x22` (0x131f450) resta invariato per la posizione a 0x147808.
  - **Non** si può usare un indice k ≥ 8 a 0x14771c: dopo il preset 7 (0x131f7b4) ci sono altri dati.
- **Celle.**
  - Proposta: una **griglia 2 × 4 che copre tutto lo schermo**, celle da 640×180 unità, come i quadranti della Multi coprono lo schermo.
  - Così, se le viste puliscono lo sfondo, non compaiono bordi (R3).
  - Cella dello slot s: colonna `s&1` (0 = sinistra), riga `s>>1`. Centro (∓320, 270 / 90 / −90 / −270); in unità RLM (y = 0 in basso):
    x0 = 0 o 640, y0 = 540 / 360 / 180 / 0.
- **Derivazione dai preset 3..6**:
  - colonna sinistra (slot pari) = **preset 3**; colonna destra (slot dispari) = **preset 4**.
  - Gli 8 float finali (±40 … ±60) danno l'orientamento verso il centro e si tengono.
  - I campi da cambiare sono **solo lo spostamento (rec+0x14/+0x18) nei 3 record**:
    - offset preset +0x14/+0x18 (A), +0x30/+0x34 (B), +0x4c/+0x50 (C);
    - stesso offset nel preset della mod, perché la copia parte da +0x38 → `obj+0x8`.
  - **Calcolo dello spostamento.**
    - Obiettivo: il modello centrato su N_Window_0i = (∓480, 294 / 136 / −22 / −180).
    - In Multi 0.22 ↔ 141 unità su una cella larga 640, quindi l'unità è la larghezza o l'altezza della cella [IPOTESI forte, x: 0.22·640 = 141 → ±461
      contro ±460].
    - Scostamento dal centro della cella: dx = ∓160, dy = +24 / +46 / +68 / +90.
    - Risultato: **sh = (±0.25, 0.13 / 0.26 / 0.38 / 0.50)**, segno + a sinistra come nel preset 3. Se l'unità y fosse l'altezza piena della
      cella Multi (360): sh_y = dy/180·0.5.
  - **Scala.** fovy (rec+0x10 = 0.5) invariato: con celle alte 180 invece di 360 il modello esce a **0.5×** della Multi. Si ritocca rec+0x10
    (fovy minore = modello più grande) e/o il record C (zoom) a vista in Eden.
  - Il pitch (−8.46°) e l'offset camera (0,20,−79) restano invariati.

## 5. Tappa 0: cosa misurare a runtime (log o GDB) per chiudere le ipotesi
1. **Init del KartDirector** (trampolino di log su 0x13d778 e dopo `Orig`): `settings+8` (atteso 5 nel menu), `+0xe9/+0xea`, `+0x180`;
   descrittori 0..7 (`+0x30+i·0x1c`: parti, personaggio, tipo) all'init e all'ingresso nella selezione a 5-8.
2. **Heap KartWork** (`dir+0xd0`, 0x13da58): memoria libera dopo 4 KartUnit (sead::Heap, metodo getFreeSize della vtable, oppure
   `svcGetInfo` usata prima e dopo); `mem usata/totale` del processo nel menu.
3. **Scene nella selezione Multi** (`DumpScene` quando si apre la 0x2c..0x2e):
   - vp0..4 (attesi 4 attivi, kart 0..3), `+0xcc`, `rlm::sceneType` (1 o 2?), `mgr+0xbd8` (atteso 3);
   - **rect** `layer+0x30..+0x3c` di sub[0..3] (quadranti?), `[layer+0x70]+0xb0` (aspetto) e **+0xb4/+0xb8** (offset), da confrontare con
     `obj(k)+0xb4/+0xb8`.
4. **Maschere**:
   - `Scene+0xc0[0..11]`, `KartUnit[i]+0x360/+0x362`;
   - u16 `shape+0xc` del primo modello di un KartUnit (`[[lista+8]+0x10]+0x40` → `[+0x28]` → shape) e del piedistallo `v+0x570`;
   - **valore dei bit 4..7 di default** con `ModelCreateHook` a 8 viste.
5. **Viewer**: le 4 voci `+0x59c..+0x5a4` (attese tutte −120,0,−120), `+0x5ac`, `+0x630/+0x632`; vtable del viewer (per il dtor, variante B).
6. **Oggetti menu-kart**:
   - `obj+0xc0/+0xc4/+0xc8` (vista legata) per gli slot 0..3, e per 4..7 dopo T1 prima e dopo `MenuKartCfgHook`;
   - descrittore `obj+0x80` (eye/at/up/fovy/offset) per slot.
7. **CameraDirector** (`[0x7e7434()+0x190+6·8]+0x1f8`): le 4 sorgenti (+0x70 + k·0xd0) e la camera effettiva di 3D Sub k (`[sub[k]+0x68]+0x8`),
   per verificare che la camera della mod per le viste extra coincida con quella del gioco a parità di descrittore.
8. **Sfondo delle viste**: con una vista extra accesa sul rect di una cella, verificare se pulisce il colore (rettangolo visibile) e quali altri
   modelli 3D del menu compaiono con i bit 4..7 di default.
9. **Piedistalli subito**, senza T1: `p5::Hide3D` + `VwVisHook::Orig(v, i, false, k)` per i, k = 0..3 a turno unico. Toglie i 2 piedistalli
   visibili oggi (p7 §5.1).

## 6. Tabella finale (parole verificate da `p7b2_verify.py`)
| tappa | tipo | indirizzo | parola originale | azione |
|---|---|---|---|---|
| 0 | log (T) | 0x13d778 | d10303ff | settings+8/+0x180, descrittori, `+0xe9` |
| 1 | T | 0x13d778 `KartDirInitHook` | d10303ff | descrittori 4..7 validi (tipo 0) + `+0x180 = 8` → Orig → ripristino |
| 1 | alt. I | 0x13d7bc | b900c268 | `w8 = 8` (solo se i descrittori 4..7 sono già validi: R1) |
| 1 | I | 0x13da28 `KartWorkSizeHook` | f9000be8 | `x0 = MK8DX_P7_KARTWORK` (vanilla menu 0x1864000) |
| 1 | T | 0x13e59c `MenuKartCfgHook` | d10103ff | `0x147678(unit[i]+0x30, &tipo, i, &arg)` per i = 4..7 |
| 1 | esistente | 0x13d424 / 0x14a038 | f9406409 / 91010008 | invariati; `p5::Hide3D` → 8 KartUnit |
| 2 | T | 0x430cc4 | a9bd57f6 | i ≥ 4: ignora (A) / voce extra (B) |
| 2 | T | 0x430d5c | 52802708 | idem |
| 2 | T | 0x430d7c | 52802708 | idem |
| 2 | T | 0x430d8c | f81d0ff5 | idem |
| 2 | T | 0x430e24 | d10083ff | idem |
| 2 | T | 0x430e58 | f81d0ff5 | idem |
| 2B | T | 0x430154 / 0x430230 / 0x430360 | f81e0ff3 / f81e0ff3 / fc1e0fe8 | init / reset / calc delle 4 voci extra |
| 3 | I | 0x147738 | b9403909 | `x8 = preset8[slot] − 0x38` (tipo 2, modo 8P) |
| 3 | esistenti | 0x6a8f10 / 0x3cce58 / 0x3cc218 | (hook `proto`) | renderer 8 viste anche per la scena menu; ramo "menu 8" in `PerFrame` |
| 3 | funzioni | 0x113c88, 0x13d47c, 0x147678, 0x61bdd0, 0x619a08, 0x6651a8 | — | bit di vista 4..7, menu-kart, camera |

**Correzioni a `step5b_plan_p7.md` §5.2/5.3:**
- voci del viewer da +0x568 con passo 0x138, non "da 0x138 a +0x568";
- heap KartWork del menu da 24,4 MiB, non 36;
- la posizione `0x131f450+0x1c` è unica per tutti gli slot, non per slot;
- le posizioni 2D vengono dallo spostamento della proiezione dei preset, dentro viste di scena separate;
- c'è un sito srotolato a 4 in più (0x13e59c);
- R1: l'attesa senza limite in 0x8843a8.

**Stima** (le tappe sono quelle di p7 §5.4):

| tappa | sessioni |
|---|---|
| 0 | 0.5 |
| 1 | 0.5-1 |
| 2 | 0.5 (A) / 1.5 (B) |
| 3 | 1.5-2 |
| 4 | 0.5-1 |

Totale: **4-6 sessioni**.

---

## 7. Esito in Eden (3 nov 2026, sera) – tappe 0-3 con `MK8DX_P7_MENU3D8=1` [VERIFICATO Eden salvo indicazione]
Log in `step7b_data/eden_log_7b*.txt`, catture GDB in `step7b_data/gdb_7b1_*.txt`, screenshot `step7b_data/eden_7b3_sel8.png`.
Metodo GDB: con il debugger di Eden attivo ogni accesso non mappato ferma il thread (CheckMemoryAccess → PrefetchAbort) e
`tools/debug/aloop.sh` logga `fpchain` e riprende: PC (a fine blocco JIT) e catena di chiamata esatti.

**Tappa 0 (Multi 4P, riferimento)** – menu = `settings+8 == 5`, `+0x180 = 4`, KartDirector `e9 = ea = 1`; 3D Sub0..3 rect 640×320
(0,400 / 640,400 / 0,80 / 640,80, y in basso), aspetto 2.0, fovy 0.5; **spostamento di proiezione = proj+0xb4/+0xb8 = obj+0xb4/+0xb8**
(0.22/0.062, confermato); `unit+0x362 = 1<<slot`; descrittori 4..7 nel menu = `-1 -1 -1 -1 0 4 2`. Heap: padre (heap corrente) 0xa488410
allocabili prima di KartWork; dopo 4 KartUnit KartWork ha libero **0x42ff8** su 0x1864000 → ~6 MiB per KartUnit.

**Tappa 1 – problemi trovati e corretti**
| # | sintomo | causa (prova) | correzione |
|---|---|---|---|
| 1 | 176 letture `[NULL+0x230]` nell'init | `0x89fc4c(kart)` → raceMgr+0x58[+0x1cc[kart]] = NULL per i kart 4..7 (entry 4 nel menu), poi 0x8659ec | `KartResHook` 0x89fc4c `f81e0ff3`: kart 4..7 senza entry → `Orig(kart-4)`. (Con 0x884b50(sys, kart) l'init si blocca: `eden_log_7b1_res47_fail.txt`) |
| 2 | init bloccato / lettura @0 con KartWork 0x1864000 o 0x2464000 | heap esaurito | KartWork **0x30c8000** (libero dopo 8 KartUnit 0x86138 = 2×0x42ff8; padre ancora 0x5f40dc8 libero) |
| 3 | salto a PC = 0 nel viewer (0x42f454) e refcount `[NULL+0x180]` (0x6a8230 ← 0x68f2dc ← 0x42f3e8) | i descrittori 4..7 con tipo 0 fanno caricare a 0x884bd8 le risorse negli **slot 4..7, usati dal menu per altri attori** (anim `Shutter`/`LocalRoom_ftp`) | `ResReqHook` 0x884bd8 `fc1a0fe8`: n → 4 durante l'init a 8; `ResWaitHook` 0x8843a8 `f81b0ff9`: attesa i → i-4 |
| 4 | letture nulle per frame (0x150c7c ← 0x1837d8 ← 0x183158 ← 0x173f6c ← 0x16cd04: 0x58/0x9c/0x41…) con 8 viste | KartUnit 4..7 creati da descrittori con tipo ≠ 0 = senza sotto-oggetti del pilota | descrittori 4..7 = copie di 0..3 **con tipo 0** (KartUnit completi) + #3 |
Risultato: boot con 8 KartUnit, renderer "Main" del menu a 8 viste, layer 3D Sub4..7 collegati agli indici 4..7, **0 letture non mappate**.

**Tappe 2-3** – guardie variante A sui 6 accessori; preset 8pl con l'inline 0x147738 (fovy ×0.69 sul tan, sh x ±0.25, sh y
−0.221 − 0.111·riga); celle 640×180 per le 8 viste (sub 0..3 = slot 0..3, Layer extra j = slot 4+j); camera extra dal descrittore
`obj(4+j)+0x80` (eye +0x8c, at +0x98, up +0xa4, fovy +0xb0, sh +0xb4/+0xb8); bit di vista 4..7 dei KartUnit e dei piedistalli a ogni
frame (il cambio personaggio ricrea il modello con maschera di default); ordine di disegno E1 esteso al menu (`m3d8::shown`).
**A video (screenshot 23:32):** P1-P4 con anteprima 3D sopra la propria targhetta come lo Switch 2 (posizione e scala corrette), P5 con il
proprio personaggio (Bowser Jr) al posto giusto; UI sopra le viste extra.

**Aperti (prossima sessione)**
1. **Risorse dei personaggi di P5-P8.** Al cambio personaggio di uno slot 4..7 (A2: entry 4..7 → 39/40/14) il KartUnit carica le risorse
   nello slot 4..7 del caricatore (sys 0x8ace44, 12 slot, bit `sys+0x5040`), che nel menu è degli altri attori o vuoto: kart senza pilota
   (P6/P7/P8) e ~417k letture nulle per frame (famiglia 0x150c7c). Serve uno slot di risorse dedicato per P5-P8 (slot 8..11 se liberi nel
   menu [IPOTESI]) nel percorso 0x13d4ac → caricamento del personaggio, con la stessa redirezione in KartResHook/ResWaitHook.
2. **"Fantasmi"**: copie dei modelli a x = ±160 nelle righe 3-4 (dietro il vetro della griglia). Da capire se le disegnano le viste extra
   (viewport/scissor più larghi del rect) o le Sub0..3 col loro viewport Multi; prova: viste extra spente con i rect 8pl delle Sub.
3. Pagina kart a 8 e finestre varianti con le anteprime (tappa 4), poi via `Hide3D`/guardie quando i modelli sono 8.
4. Taratura fine di posizione/scala (oggi da misure sugli screenshot, §4.2).
Con `MK8DX_P7_MENU3D8=0` (default di `scripts/build.sh`) nessuno di questi hook è installato: boot con 4 KartUnit, 0 letture non mappate.

### 7.1 Seguito (3-4 nov, notte)
- **Risorse P5-P8 risolte.** Sonda degli slot di risorse (sys 0x8ace44 +i*0x20: +0x160/+0x168 due caricatori, +0x170 buffer attivo,
  `[L+0xd38]` risorsa caricata; default sys+0xf8+i*8): nel menu gli slot 0..3 hanno i due buffer caricati, gli slot **4..11 sono liberi**
  (caricatori esistenti ma vuoti); con 8 giocatori entry+0x230 = 4..7. Il caricamento per frame (0x882f2c..0x883134) usa sys+0x2d8[i]
  (richiesta) e sys+0x4d1c+i*0x1c. 0x884bd8 prepara i caricatori (0x891984(L, heap, 0, 0, s0 = 1.5), costo ~2,5 MiB per slot) solo per
  i descrittori con tipo 0. Prepararli all'init del menu rompe gli attori creati dopo (anim `Shutter`: resfile NULL, salto a 0) anche
  senza caricare nulla -> `m3d8::PrepareLoaders()` li prepara al primo frame della selezione a 8 con l'heap salvato all'init.
  Risultato: slot 4..7 caricati con i personaggi di P5-P8, **0 letture non mappate** nella selezione a 8 (eden_log_7b5_loaders.txt).
- **Celle 640x160 centrate sul modello** (righe a passo 160, centro y = 274 − 160 r): con celle 180 a griglia fissa il modello delle righe
  basse era tagliato in alto (P7/P8 "senza pilota" = taglio). Scala 0.81 della Multi (kFovyScale 0.6146), sh_y −0.224 per tutte le righe.
- **Fantasmi**: nel riquadro della vista k (4..7) compare anche il contenuto della vista k−1 (KartUnit, piedistallo, moneta 3D del menu)
  con lo spostamento di proiezione della vista k−1 (specchiato). Spegnendo il layer della vista 4 spariscono P5 e il fantasma: li disegna
  il job della vista 4. Non dipende dalle maschere (spente anche alla creazione dei modelli nel menu, ModelCreateHook: nessun effetto),
  ne' dai gruppi di R (+0x409 = ff per tutte), ne' da +0x1088 (0..7). Analisi statica in corso: step7b_ghost.md.

### 7.2 Seguito (4 nov, 00:00-00:40)
- **Fantasmi risolti**: prova T1 (ordine GPU delle viste extra invertito) -> il fantasma segue l'ordine di disegno, non l'indice
  (step7b_ghost.md: nessun indice v-1 nel codice). Confronto dei campi Sub3/Sub4: unica differenza non puntatore
  **layer+0xde4 bit 0x4** (0x124 contro 0x120). Copiato da Sub0 a ogni frame nelle viste extra del menu -> fantasmi spariti
  (screenshot 00:22, eden_log_7b8_de4.txt, 0 letture non mappate). Le monete restano.
- **Aperti**
  1. **Monete** nelle celle di P6-P8 (non P5). [IPOTESI] segnaposto "caricamento" dell'anteprima: i modelli di P6-P8 restano spesso
     indietro (P5 mostra un personaggio vecchio, P6/P7 kart senza pilota + moneta). Con i tasti condivisi da tutti gli 8 giocatori della
     config attuale di Eden ogni pressione fa partire 8 caricamenti insieme (sys+0x5040 = 0 alla sonda di 00:36).
  2. **P8 (slot 7)**: il cursore si muove (tasto 8) ma nome e anteprima non cambiano; nessun evento A2 per lo slot 7 durante lo
     spostamento; succede anche con MK8DX_P7_MENU3D8=0, mentre nei log 5b7/5b8 lo slot 7 cambiava -> regressione fra cba7303 e oggi
     da bisecare (prima prova: build di cba7303). Non e' lo stato salvato in p58.bin (array 10 di P8 = 1: azzerarlo non cambia nulla,
     backup in step7b_data/p58_backup_0029.bin, ripristinato).
  3. **Letture nulle intermittenti** famiglia 0x150c7c (KartUnit 4..7 senza sotto-oggetti) dal boot in alcune sessioni (0 in 7b5/7b8,
     282k in 7b10): da catturare con GDB i primi 10 casi con i registri.

### 7.3 Seguito (4 nov, mattina)
- **P8 risolto**: stub degli hook inline senza NZCV (STATE "Problemi aperti").
- **Letture nulle intermittenti 0x150c7c risolte**: i gestori per kart del gruppo dg (0x7e7434()+0x190[4]) dimensionano i loro array con
  settings+0x180 nel proprio init (4 nel menu): dg+0x218 vt 0x1260dd8 (init 0x86d918, PtrArray<4> +0x60/+0x68 letto a 0x182cf0),
  dg+0x240 vt 0x11ad188 (init 0x2a9c, +0xe0/+0xe8 letto a 0x150f04), dg+0x228 vt 0x11b8db0 (init 0xcefb4). Hook che forzano 8 durante
  quegli init (solo menu, opzione attiva): array 8/8 verificati, 0 letture non mappate all'avvio. KartUnit 6/7 leggevano oltre la fine
  (contenuto casuale -> intermittenza).
- **Anteprime che si bloccano dopo qualche cambio**: il thread di caricamento (0x618610) muore nella callback di fine caricamento del
  KartUnit (0x8830d8 -> 0x16f100 -> 0x16ee54 -> 0x16e118 -> 0x1724e0 -> 0x122cd4: materiale m_Body / emission_intensity non trovato,
  0x68ff80 = 0xFFFF -> 0x68fffc salto a 0). Guardie 0x68fffc (NULL se non trovato) e 0x11dc0c (NULL -> ritorno: e' la Read64 @0x10
  della gara a 8). Dopo le guardie crasha piu' avanti (0x16e13c -> 0xd6318 -> 0x690ea8 -> 0x6dd30c): il KartUnit riceve una risorsa
  incoerente (moneta = default dello slot?). Analisi statica del caricatore in corso: step7b_loader.md.

### 7.4 Flusso originale con N = 8 (4 nov, mattina) – step7b_menu8.md
- **Causa radice dell'instabilita' delle anteprime**: con 8 KartUnit l'heap della scena menu (TaskHeap 0xd200000, voce del task creata a
  0x603290) si esauriva: HeapResident (100 MiB, 0x425c9c) non entrava (99,9 MiB allocabili), le risorse finivano nell'heap della scena e
  dopo il caricamento restavano 0 byte (eden_log_heap_exp0.txt). Con 0x10000000 HeapResident nasce con lo stesso margine interno
  dell'originale (11,3 MiB).
- **Il 4 del menu** = giocatori locali di default: 0x869180(rm, 4, 1, 0) a 0x4f9e6c (avvio) e 0x4fa260 (menu); entry 0..3 +0x44 = 0, 4..7 = 2;
  0x869a14 compila le settings dalle entry valide; 0x86f074 (e copie 0x86edb4/0x86f474) limita il modo menu a 4.
- **Patch del flusso a 8** (tutto-o-niente, MK8DX_P7_MENU3D8): 0x4f9e60/0x4fa254 `321e03e1` -> `321d03e1`, 0x86edd4/0x86f080/0x86f4ec
  `321e03eb` -> `321d03eb`; heap della scena menu 0x10000000 (HeapCreateHook); KartWork 0x30c8000. Il gioco crea da solo 8 KartUnit,
  i gestori per kart a 8, 8 slot di risorse preparati e caricati all'avvio, scheduler a 8. Tolte (non installate): KartResHook,
  ResReqHook, ResWaitHook, PrepareLoaders, scheduler forzato/limitato, Force8 x3, DirGroupInit, copia descrittori.
- **Verificato in Eden**: 4P corretto (anche sotto stress); 8P con 8 anteprime piccole nelle posizioni 8pl, un cambio alla volta corretto
  per tutti gli 8 (eden_log_flow8_rebuild.txt: ogni unita' dal proprio slot/buffer, 0 letture non mappate).
- **Aperto**: con molti cambi simultanei a 8 una ricostruzione trova un modello senza m_Body e il thread di caricamento muore (anche
  unita' 1/2); il menu originale a 4 regge lo stesso stress. Causa probabile: lista fissa di 5 cache "ModelSmartCacheList(Menu)"
  (0x884f2c/0x8852b8, ciclo i < 5 a 0x885250) per 8 unita'. Analisi in corso: step7b_cache.md. Monete su P6-P8: da ricontrollare
  dopo la correzione delle cache.
- Nota: 0x490828 azzera le parti degli slot 0..3 solo all'avvio (non e' l'azzeramento della selezione): l'estensione provata e' stata tolta.

## 8. Scala e posizione delle anteprime come Switch 2 (4 nov 2026, sera) [D misure]
Misure sui riferimenti (step5_data (dati non pubblicati)/ref_switch2_charsel_8p.jpg, ref_switch2_kart_8p.jpg) e sugli screenshot di Eden, tutti a 1280x720:
piedistallo Switch 2 largo 89-95 px, base a y = 132/290/449/607, centro x 160/1120. La mod era a ~175 px (kFovyScale 0.6146 = 0.81 della
Multi). Con la fovy dei preset Multi invariata (celle alte 160) 99-101 px; con zoom 0.925 (tan(fovy/2)/0.925) e sh_y = 0 (come i preset
Multi della riga bassa) 90-94 px, base 133/291/449/607, centro x 160/1120: coincide entro 1-2 px. Confronti in step7c_data (dati non pubblicati)/.
