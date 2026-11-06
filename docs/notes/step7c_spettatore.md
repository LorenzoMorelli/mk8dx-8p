# Step 7c – cella vuota come nel 3P originale: camera spettatore e nessun HUD (analisi statica, main 4.0.1)

Offset = main 4.0.1 decompresso (`img/main.bin`, `img/main.ann.asm`). Solo disassemblato, nessuna esecuzione.
[V] = letto nel disassemblato; [I] = ipotesi/deduzione.

## Sintesi
- La quarta cella del 3P nasce dal **ciclo 2 di SceneInitRace** (0x7e7898..0x7e7904): se gli slot di split (4) sono più
  dei viewport riempiti (3), aggiunge un viewport `{slot 3, attivo 1, tipo 2, kart 0}` [V].
- Il **tipo 2 lo legge il gestore dei "controller di camera"** (`sys+0x248`, vtable 0x11b8da0, 0xb8 B, ctor 0xcef7c):
  ha **4 controller** (classe vtable 0x11b8d40, 0x190 B, ctor 0xcc834(obj, vp), `obj+8 = vp`). Il metodo 0xcf224
  (vt+0xd8) mette in **modo 1** (sotto-camera `obj+0x118`, la camera "TV" spettatore) ogni controller il cui viewport
  ha tipo 2. Il controller calcola la propria camera e la **spinge nel director** (0xce29c → 0xc9074, nodo tipo 2,
  prio 20, vp = obj+8) e **scrive lui stesso `RLM slot[vp]+0x1b = 1`** [V].
- **HUD**: 0x406870 registra RaceView/RaceViewFront solo per i viewport di **tipo 1** (`0x406ce8 ldr w8,[x22,#8]; cmp #1;
  b.ne skip`): la cella spettatore ha il layer 2D Sub3 attivo ma nessuna pagina [V]. Anche il Lakitu viene creato solo
  per il tipo 1 (0x7e9d84) [V].
- **Raccomandazione**: (a) quinto controller per vp4 + vp4 di tipo 2 nella Scene (come il ciclo 2) + blocco della
  spinta nel director per vp ≥ 4. La mod copia nella cella vuota la camera calcolata dal controller 4. L'HUD si evita
  come fa il gioco: nessuna pagina presa in prestito per la cella vuota. Patch: 1 parola (0xcf040), 2 trampolini (0xce29c,
  0xce35c), 1 trampolino facoltativo sul ctor (0xcc834) per avere il puntatore. A questo si aggiungono tre modifiche
  a main.cpp (Scene, PerFrame, RacePagesSetupHook).

## 1. Come nasce il viewport tipo 2 (SceneInitRace 0x7e76b8)
- Il ciclo 1 (0x7e77e4..0x7e7878) riempie un viewport per locale. Nel 3P vp0..2 = locali 0..2, tipo 1
  (`tipo = settings[0x44+d*0x1c] != 0 ? 2 : 1`, 0x7e7838/0x7e7840) [V].
- `w28` = slot di split: 4 con n' > 2 (0x7e7760..0x7e776c). Dopo il ciclo 1, `cmp w28, w26; b.gt 0x7e7898`
  (0x7e7880): se gli slot sono più dei viewport riempiti parte il **ciclo 2** [V]:
*(estratto di disassemblato del gioco rimosso)*
  Nel 3P: j = 0 → vp3 = {slot 3, attivo 1, tipo 2, loc buf[0]=0, kart 0}, esattamente il dump [V]. Con modo
  (settings+8) == 2 il viewport extra sarebbe di **tipo 1**, cioè una vista normale del kart 0 [V]. Che cosa sia il
  modo 2 non è noto [I]: in gara VS è 4 (log), in battaglia 3.
- 0x7e7af8: `+0xb0[clamp4(slot)] = vp` → nel 3P `+0xb0[3] = 3` (0x7e7b88..0x7e7b98); poi `+0xc0[kart] |= 1<<vp` per
  attivo|2 == 3 → `+0xc0[0] |= 8` [V].
- **Chi scrive slot[3]+0x1b nel RLM.** Ci sono due scrittori [V]:
  1. **RLM sync 0x3cbd94**. Per i = 0..3 (0x3cbea4..0x3cbf90) fa `slot[i]+0 = Scene+0xb0[i]` (0x3cbeac), poi
     dall'attivo del viewport ricava +0x18/+0x19/+0x1a (0x3cbf4c..0x3cbf58: attivo 1 → 1,1,0; 2 → 0,1,0; 3 → 1,1,1) e
     **`+0x1b = (vp.tipo == 2)`** (0x3cbf3c `ldr w15,[vp,#0x44]; cmp #2; cset` → 0x3cbf5c `strb w15,[x14,#0x9d7]`).
  2. **Controller di camera**: 0xce340 `strb 1,[rlm + clamp4(obj+8)*0x20 + 0x9d7]` all'aggancio, 0xce3d0 `strb 0`
     allo sgancio.
- L'unico lettore di `+0x1b` è 0x3ce490 (in 0x3ce258, chiamata anche dopo aggancio/sgancio): sceglie per un layer
  ausiliario (`mgr+0x1b8`, cella `mgr+0xbec % 4`) la tabella di rect `mgr+0xa38` se +0x1b, altrimenti `mgr+0xb08`
  [V meccanica, I significato: cosmetico]. Il layer 3D della cella prende la camera dal director, non da +0x1b.

## 2. Chi legge il tipo e che cosa cambia
### 2.1 Camera [V salvo indicazione]
- **Gestore** (`CamCtrlMgr`): 0xb8 B, creato in 0x398ee0..0x398ee8 e salvato in `sys+0x248`, accanto al gestore
  Lakitu (`sys+0x240`). L'init 0xcefb4 (vt+0x20) fa:
  - `0x5fb2a4(mgr+0x38, 4, 0, 8)`, cioè un **array di 4 puntatori** creato a **0xcf040 `orr w1, wzr, #0x4`
    (0x321e03e1)**;
  - poi un ciclo `i < mgr+0x3c` (capacità) con `0xcc834(alloc 0x190, i)` (0xcf060..0xcf0a4).
  - Stato: +0x38 count, +0x3c cap, +0x40 ptr; +0x88 array di oggetti per kart (settings+0x180); +0xac modo forzato
    (-1 = nessuno; 0xcf8f4 lo mette a 3); +0xb0/+0xb4 flag.
- **Controller per viewport** (vtable 0x11b8d40; vt[0] a 0x11b8d50): ctor 0xcc834 → base 0x7d24ec, `obj+8 = vp`.
  - `obj+0x10` è il **descrittore di camera** spinto nel director: +0x8 tipo 0 = "camera esterna",
    +0x10 → `obj+0x20` = camera LookAt (`obj+0x30`), +0x18 → `obj+0x28` = proiezione (0xc0 B, 0x61bd1c).
  - `obj+0xe0` contiene 9 sotto-camere (indice = modo): [0] 0x70 B (0x7d01bc), **[1] `obj+0x118` 0x140 B (0x7d9950) =
    spettatore**, [2] `obj+0x120` (0x7d50bc), [3] `obj+0x130`, [4] `obj+0x128`, [5] `obj+0x138`, [6] `obj+0x140`,
    [7] `obj+0x148`, [8] `obj+0x150`.
  - Campi: +0xe8 modo, +0xf0 kart seguito, +0xf5 kart valido, +0xf7 agganciato al director.
  - vt+0x20 = 0xcce64 per frame (esce subito se `+0xe8 == 0`, 0xcce84); vt+0x28 = **0xce29c aggancio**;
    vt+0x30 = **0xce35c sgancio**; vt+0x48 = 0xcdff4 scelta del kart.
- **Lettura del tipo: 0xcf224** (vt+0xd8 del gestore, chiamata da 0xcf0b8 = vt+0x28 dopo la ripartenza). Per ogni
  controller c (`i < mgr+0x38`):
  ```
  e = Scene+0x38 + clamp5(c+8)*0x18                       (0xcf274..0xcf284 / 0xcf348..0xcf358)
  c+0xf0 = (e.attivo|2)==3 ? e.kart : -1 ; c+0xf5 = kart>=0 (0xcf2b4/0xcf2b8)
  se attivo e kart>=0:
     modo forzato (mgr+0xac >= 0): 0xcd4c0(c, tipo==2 ? &1 : &mgr+0xac, 0, 0, 1)      (0xcf2dc..0xcf30c)
     altrimenti: tipo==2 e c+0xe8==0 -> 0xcd4c0(c,&1,0,0,1), mgr+0xb4=1               (0xcf3b8..0xcf3dc)
                 tipo!=2 e c+0xe8!=0 -> 0xcd4c0(c,&0,0,0,1), mgr+0xb4=0               (0xcf3e4..0xcf400)
  ```
- **Cambio modo 0xcd4c0**. Sgancia la sotto-camera vecchia (vt+0x18). Se il nuovo modo è 0 chiama 0x7d3164, che
  sgancia dal director se +0xf7 (vt+0x30). Poi `+0xe8 = modo` e attiva la sotto-camera nuova (0x7d022c, vt+0x10).
- **Aggancio**. Per frame 0xcce64 → ... → 0xcd4a4 → 0x7d2764. Questo copia l'uscita della sotto-camera nella camera del
  controller: 0x7d2928 scrive `obj+0x68..0x97` (= camera+0x38: pos/at/up) e 0x7d2944 `setFovy(obj+0x28, ...)`. Poi
  **0x7d2958: se !+0xf7 → +0xf7 = 1 e vt+0x28**.
- **0xce29c (aggancio)**:
  - `0xc9074(director, vp=obj+8, desc=obj+0x10, &tipo 2, prio 0x14)` a 0xce318; 0xc9074 fa clamp `vp>=4 → 0`
    (0xc9090);
  - `strb 1, [rlm + clamp4(vp)*0x20 + 0x9d7]` (0xce340);
  - `0x3ce258(rlm)`.
- **0xce35c (sgancio)**: `0xc8e68(director, vp, desc)` (REMOVE, clamp a 0xc8e98), `str xzr,[obj,#0xe8]`,
  `strb 0, slot+0x1b`, 0x3ce258, poi `0x19d3e8(x, 0, vp)` per ogni elemento della lista +0x170/+0x178.
- **Kart seguito / cambio di inquadratura**. In 0xcce64, se `+0xf4` e il timer `[obj+0x98]+0xd0 >= 1.0`, chiama
  vt+0x48 = 0xcdff4(c, 1). Questa sceglie il kart da seguire: primo kart k < settings+0x180 con
  `[[sys4+0x218]+0x68][k]+0x40 == 0`, confronto con `+0xf0`, soglia di distanza a 0xed599c; poi `+0xf0` e `+0xf5`
  (0xce26c/0xce270). Meccanica [V], significato dei campi (classifica/arrivati) [I]. Il kart iniziale è
  `vp.kart` = 0 [V].
- **Dal director al layer**:
  - il director, per la testa di tipo desc 0, mette `dir+0x60[vp] = desc+0x10` (camera) e
    `dir+0x68[vp] = desc+0x18` (proiezione) (0xc8d58..0xc8d7c, clamp 4);
  - l'RLM 0x3cc284 fa `sub[i]+0x68 = mgr+0x958[vp clamp4]` (o `mgr+0x1d8`);
  - che `mgr+0x958` sia l'array puntato da `dir+0x60` è [I].
- **Nessun controllo di tipo nel director**. Il bind 0xc89cc lega comunque al vp3 il descrittore del kart 0 (tipo 1,
  prio 10), ma il nodo del controller (prio 20) sta sopra [V priorità; ordine pila I forte, coerente con l'intro].

### 2.2 HUD [V]
- 0x406870, ciclo sui 5 viewport da 0x406cb8:
  - `0x406cbc +0x10 locSlot < 0 → salta`;
  - `0x406cc4 attivo|2 != 3 → salta`;
  - `0x406cdc slot > 3 → salta`;
  - **`0x406ce8 ldr w8,[x22,#0x8]` (tipo) `0x406cec cmp w8,#1; 0x406cf0 b.ne 0x406dd0` → salta**.
- Per il tipo 2 quindi **non si registrano RaceView (0x65+slot) né RaceViewFront (0x6a+slot)**. Il layer 2D Sub3 resta
  acceso dall'RLM (attivo 1 → +0x18 = 1) ma senza pagina, quindi vuoto.
- La pagina Race crea comunque i 5 oggetti per viewport (0x5326dc, 0x53284c..0x53294c, senza controllo del tipo):
  non serve evitarli.

### 2.3 Altro
- Lakitu: creato solo per vp 0..3 con tipo 1 (0x7e9d84 `ldr w8,[x0,#0x44]; cmp #1`) → niente Lakitu nella cella
  spettatore [V].
- RLM: +0x1b (sopra); +0x18..+0x1a come un viewport giocatore attivo [V].
- Audio/ascoltatori: nessun lettore del tipo trovato al di fuori dei siti sopra; non verificato a fondo [I].

## 3. Dalla camera del viewport al layer 3D; opzioni per una 5a-8a cella
Catena: controller(vp) `obj+0x10` → 0xc9074 pila `dir+0x70+vp*0xd0` → 0xc8b60/0xc8d1c testa → `dir+0x60/0x68[vp]` →
RLM 0x3cc218 `sub[slot]+0x68/+0x70`. Tutti con clamp 4 (0xc9090, 0xc8e98, 0xc8d64, 0x3cc284, 0xce338, 0xce3c4) [V].
Il director ha 4 pile (`dir+0x70..+0x3af`, poi `+0x3b0` nodi e `+0x3d0` descrittori dei kart), quindi non ha posto per
un vp4. Ma **la camera spettatore è calcolata dal controller stesso** (0x7d2928/0x7d2944, prima dell'aggancio), non dal
director. Un quinto controller produce quindi la camera senza toccare il director.

### (a) Quinto controller per vp4, vp4 di tipo 2 nella Scene (consigliata, massima fedeltà)
- **P1 (parola)** 0xcf040: `0x321e03e1` (`orr w1, wzr, #4`) → **`0x528000a1`** (`mov w1, #5`).
  - Il ciclo 0xcf064 crea 5 controller (`obj+8` = 0..4). cf224, 0x7d4b84 (aggiornamento: vt+0x20 su tutti) e
    0xcf9c8/0xcf9f8/0xcfa2c usano `count`/`cap` → nessun altro limite [V].
  - Con 1-4 giocatori vp4 è inattivo: c4 resta in modo 0 e 0xcce64 esce subito [V]. È lo stesso caso dei controller
    1..3 nell'1P originale.
- **P2 (trampolino)** 0xce29c (prima parola `0xf81c0ff7`): `if (*(int32_t*)(o+8) >= 4) return;` (niente spinta in vp0,
  niente `slot[0]+0x1b`). +0xf7 è già 1 (0x7d2964), quindi non viene richiamato a ogni frame.
- **P3 (trampolino)** 0xce35c (prima parola `0xf81d0ff5`): per `vp >= 4` replicare senza director/RLM:
  `*(uint64_t*)(o+0xe8) = 0;` poi per i < `*(int32_t*)(o+0x170)`: `0x19d3e8((*(void***)(o+0x178))[i], 0, vp)`; infine
  `*(int32_t*)(o+0x170) = 0; return;`. In alternativa basta chiamare Orig: 0xc8e68 con vp clamp 0 cerca il nodo di c4
  nella pila di vp0 e non lo trova [I], ma azzera `slot[0]+0x1b` (cosmetico, 0x3ce258).
- **P4 (Scene)** in `SceneInitRaceHook`, ramo `split`, prima di `0x7e7af8`, con n dispari (5 o 7):
  `vp[4] = { 4, -1, 1, *(int32_t*)(st+8) == 2 ? 1 : 2, Fn<int(*)(int)>(0x86e8e4)(0), 0 };`
  Sono i valori del ciclo 2 (kart min(0, n kart) = 0, locSlot buf[0], tipo da settings+8), con una sola
  differenza: **slot = -1**. Con slot 4, 0x7e7af8 farebbe `+0xb0[clamp4(4)=0] = 4` e ruberebbe a P1 la mappa
  slot→vp. Con slot -1 lo salta (0x7e7ba4..0x7e7bac) [V]. Effetti collaterali [V]:
  - `+0xc0[0] |= 0x10` (maschera "kart visibile", analoga a `|= 8` del 3P);
  - 0x406870: nessuna pagina (tipo 2, e comunque slot > 3);
  - Lakitu: escluso (tipo 1 / vp 0..3);
  - RLM: non lo vede (passa solo da +0xb0[0..3]).
- **Puntatore a c4**: trampolino su 0xcc834 (prima parola `0xf81c0ff7`), dopo Orig `if (idx == 4) spect::ctrl = obj;`.
  In alternativa `(*(uint8_t***)(mgr+0x40))[4]` con `mgr = sys+0x248` [I: `sys` = `0x7e7434()+0x190[4]`]. Prima
  dell'uso controllare vtable `== base+0x11b8d50` e `*(int32_t*)(o+8) == 4`.
- **Cella vuota in PerFrame** (al posto di RelCam/DescCam/KartCam per la cella con `4+j >= nLocal`):
  ```cpp
  static bool SpectCam(uint8_t* c, uint8_t* pj) {           /* camera calcolata da 0x7d2764 per il controller 4 */
      uint8_t* o = spect::ctrl;
      if (!o || *(uintptr_t*)o != mk8dx::Base() + 0x11b8d50 || *(int32_t*)(o + 8) != 4 || *(int32_t*)(o + 0xe8) != 1) return false;
      uint8_t* c4 = *(uint8_t**)(o + 0x20); uint8_t* p4 = *(uint8_t**)(o + 0x28);   /* = o+0x30, proiezione 0x61bd1c */
      if (!c4 || !p4) return false;
      memcpy(c + 0x38, c4 + 0x38, 36);                       /* pos/at/up (0x7d2928 scrive o+0x68 = c4+0x38) */
      mk8dx::Fn<void (*)(void*, float)>(0x61bdd0)(pj, *(float*)(p4 + 0xa0));
      mk8dx::Fn<UpdMtxFn>(0x619a08)(c, c + 8);                /* il director non la applica: matrice ricalcolata qui */
      return true;
  }
  ```
  Se SpectCam fallisce (modo != 1, per esempio con settings+8 == 2), il fallback attuale DescCam(kart 0) coincide con
  quello che farebbe il gioco con un viewport extra di tipo 1.
- Rischi:
  - vp4 attivo non compare mai nel retail [I]. I lettori per indice (b_vp_readers_class.txt: 0xc900c, 0x86e894,
    0x1093e4, ...) sono getter "vp → kart" con clamp 5 e vengono chiamati con indici ≤ 3 [I].
  - Il controller 4 nel per-frame legge `rlm slot[clamp4(4)=0]+0x9c8` (0xcdae4..0xcdafc), cioè l'aspetto della
    cella 0: con split 6/8 tutte le celle hanno la stessa dimensione [V lettura, I effetto].
  - 0x76770 e 0x1088c8 (da 0xcec00 = vt+0x38) sono limitati da un count [V].
  - Il caso intro: c0 è spinto con prio 20 per l'intro solo tramite i metodi che toccano `ctrl[0]` (0xcf440, 0xcf640,
    0xcf710, 0xcf850, 0xcf948), quindi c4 non è coinvolto [V].

### (a2) Come (a) ma senza vp4 nella Scene
Si sostituisce P4 con un trampolino su 0xcf224 (prima parola `0xd10183ff`). Dopo Orig, con n dispari, si replicano le
scritture di cf224 per il tipo 2: `o+0xf0 = 0; o[0xf5] = 1; if (*(int32_t*)(o+0xe8) != 1) { int m = 1;
Fn(0xcd4c0)(o, &m, 0, 0, 1); }` (stessi argomenti di 0xcf3c4..0xcf3d8). La Scene resta intatta (niente bit +0xc0,
nessun lettore sconosciuto). È meno "flusso originale" (il tipo 2 non passa dalla Scene). Da usare se vp4 attivo dà
problemi.

### (b) Estendere la director a 5 pile
Non conviene. L'oggetto ha layout fisso (pile a +0x70 per 4 × 0xd0, subito dopo +0x3b0/+0x3d0), con clamp 4 in
0xc89cc/0xc8b2c/0xc8d64/0xc8e98/0xc9090 e `mgr+0x958[4]` nell'RLM. Inoltre la mod non ha bisogno del director per le
celle extra: i layer sono suoi.

### (c) Spettatore su vp3 (Sub3 spostato nella cella vuota) e locale 3 su una cella extra
È la copia letterale del 3P (vp3 tipo 2, slot 3, `+0xb0[3] = 3`), ma **sconsigliata**:
- `+0xb0[3] = 3` fa puntare il locale 3 (P4) al viewport spettatore. I 43 lettori via +0xb0 (0x86eb1c/0x86eb84,
  fade, `N_ItemCount_All_00`, ...) attribuirebbero gli effetti di P4 al kart 0.
- P4 perderebbe RaceView3 (0x406cec), Lakitu e il bind del director.
- Servirebbe una quinta pagina presa in prestito.
- Mettere lo slot di vp3 a -1 spegnerebbe slot[3] nell'RLM, quindi Sub3 senza camera.

### (d) Spettatore calcolato dalla mod (segue il primo in classifica)
Non fedele: scartata.

## 4. HUD della cella vuota: il meccanismo originale
Il gioco **non crea la pagina**: 0x406cec salta il viewport di tipo 2 e il layer 2D della cella resta acceso ma vuoto.
Nella mod:
- `RacePagesSetupHook`: registrare le pagine prese in prestito solo per i locali reali,
  `for (j = 0; j < view8::nLocal - 4 && j < 4; j++)` invece di `split - 4`. È l'equivalente del `cmp #1` di
  0x406cec per la voce extra di tipo 2.
- PerFrame, apertura H2 (`for j < Used()` con `kIds[j]` e `0x8b21dc`): stesso limite `j < nLocal - 4`.
- `AddVpObjects` può restare a `split - 4`: il gioco crea comunque i 5 oggetti. Per coerenza
  `hud8::extraVp[nLocal-4] = { 5+j, -1, 1, 2, 0x86e8e4(0), 0 }` (oggi è a zero / `kartOf[7] = 0`).
- `hud8::layer2d[j]` della cella vuota può restare acceso (come 2D Sub3 nel 3P) senza pagina, oppure essere spento:
  nessuna differenza visibile [I].

## Piano consigliato (opzione a)
| # | sito | tipo | parola originale | modifica |
|---|---|---|---|---|
| P1 | 0xcf040 | patch | 0x321e03e1 `orr w1,wzr,#4` | 0x528000a1 `mov w1,#5` (5 controller) |
| P2 | 0xce29c | trampolino | 0xf81c0ff7 | vp ≥ 4 → return (niente 0xc9074 / +0x1b) |
| P3 | 0xce35c | trampolino | 0xf81d0ff5 | vp ≥ 4 → solo `+0xe8 = 0` e lista +0x170 (niente 0xc8e68 / +0x1b) |
| P4 | SceneInitRaceHook (0x7e76b8, 0xd10283ff già agganciato) | codice mod | – | n dispari: `vp[4] = {4,-1,1,tipo,0x86e8e4(0),0}` prima di 0x7e7af8 |
| P5 | 0xcc834 | trampolino | 0xf81c0ff7 | salva `obj` se `idx == 4` (puntatore a c4) |
| P6 | proto::PerFrame | codice mod | – | cella `4+j >= nLocal`: `SpectCam` (fallback DescCam(0)), niente RelCam |
| P7 | RacePagesSetupHook + apertura H2 | codice mod | – | pagine solo per `j < nLocal-4` |

Verifica in Eden:
- log `+0xe8` di c4 = 1 e `+0xf0` dopo cf224;
- watchpoint su `rlm+0x9d7` (deve restare 0 per lo slot 0);
- pila del director vp0 (`dir+0x80` count invariato rispetto al 6P/8P);
- cella vuota senza monete/posizione.

## Punti incerti
- Ordine SceneInitRace → 0xcf224 a inizio gara: dato per buono perché il 3P originale ne dipende [I].
- `mgr+0x958` dell'RLM = array `dir+0x60` [I]; non serve al piano (la mod copia dal controller).
- Layout della camera del controller: pos/at/up a camera+0x38/+0x44/+0x50 come per gli altri LookAtCamera. 0x7d2928
  scrive 0x30 byte da camera+0x38 e gli ultimi 12 non sono identificati [I forte].
- Significato di settings+8 == 2 (tipo 1 per il viewport extra) e dei campi letti da 0xcdff4 [I].
- vp4 attivo non è mai usato nel retail: eventuali lettori per indice con clamp 4 → 0 non ancora esaminati uno per
  uno (elenco in step6_data/b_vp_readers_class.txt). In caso di problemi passare ad (a2).
- 0x3ce258 / tabelle `mgr+0xa38` e `mgr+0xb08` per +0x1b: significato del layer `mgr+0x1b8` non identificato
  (cosmetico) [I].

## Piano da implementare (presentato all'utente il 4 nov 2026 sera, da riprendere nella prossima sessione)
Verifica del coordinatore sul binario: 0xcf040 `321e03e1` = capienza (4) del PtrArray dei controller a gestore+0x38; il ciclo
0xcf05c..0xcf0a4 crea `[+0x3c]` controller (0x190 B, ctor 0xcc834(obj, i)); prime parole 0xce29c `f81c0ff7`, 0xce35c `f81d0ff5`,
0xcc834 `f81c0ff7`, 0xcf224 `d10183ff`; 0x406cec `cmp w8,#1` (tipo del viewport) prima della registrazione delle pagine HUD. [V]

| # | dove | cosa |
|---|---|---|
| P1 | 0xcf040 | `321e03e1` (orr w1,wzr,#4) -> `528000a1` (mov w1,#5): 5 controller di camera; con 1-4P il quinto resta in modo 0 |
| P2 | trampolino 0xce29c | controller con vp >= 4: niente spinta nel director (clamp 4 -> 0 la metterebbe sulla pila di vp0) |
| P3 | trampolino 0xce35c | vp >= 4: solo `+0xe8 = 0` e pulizia lista +0x170, senza director/RLM |
| P4 | SceneInitRaceHook | con 5 o 7 locali: `vp[4] = {4, -1, 1, tipo, 0x86e8e4(0), 0}` prima di 0x7e7af8 (slot -1: con 4 scriverebbe +0xb0[0] = 4 togliendo il viewport a P1); tipo come il ciclo 2 (2, oppure 1 se settings+8 == 2) |
| P5 | trampolino ctor 0xcc834 | salva il puntatore al controller con indice 4 |
| P6 | proto::PerFrame | cella vuota (indice nLocal..split-1): camera copiata dal controller 4 (`*(c4+0x20)+0x38` pos/at/up, `*(c4+0x28)+0xa0` fovy, poi 0x619a08); fallback DescCam(kart 0) |
| P7 | RacePagesSetupHook e aperture delle pagine prese in prestito | `j < nLocal-4` invece di `split-4` (equivalente di 0x406cec): niente HUD/quadratino nella cella vuota; stesso criterio per i Lakitu extra (oggi nel 7P il Lakitu di P1 compare nella cella 8 e manca nella cella 1) |

Verifica in Eden: log del modo del controller 4 (+0xe8 = 1), RLM slot[0]+0x1b sempre 0 (watchpoint su rlm+0x9d7 se serve), pila di vp0
invariata; gara a 7 e a 5 (cella vuota = camera panoramica, nessun HUD, Lakitu 1-7 corretti), poi 3P/4P invariati (regressione).
Alternativa piu' prudente se il vp4 attivo desse problemi: (a2) trampolino su 0xcf224 che ripete per il controller 4 le scritture
del tipo 2 senza toccare la Scene.

## Esito in Eden (4 nov 2026 sera, PC Ryzen Z2 Extreme) [VERIFICATO]
Implementati P1-P7 (src/program/main.cpp: namespace `spect`, CamCtrlCtorHook 0xcc834, CamCtrlAttachHook 0xce29c, CamCtrlDetachHook
0xce35c, patch 0xcf040, P4 in SceneInitRaceHook, SpectCam in proto::PerFrame, `hud8::Pages()` in RacePagesSetupHook / apertura H2 /
LakituAssignHook). In piu': `kartOf[i >= n] = -1` e 0x86eb84/0x86eb1c restituiscono -1 per gli slot locali inesistenti (come
l'originale per slot > 3): prima `kartOf[7]` restava 0 e LakituAssignHook spostava il Lakitu di P1 nella cella 8.
- **Correzione del piano (misurato)**: obj+0x68..0x97 NON e' pos/at/up ma la **matrice di vista 3x4** (righe R|t) scritta da
  0x7d2928..0x7d2938 dopo 0xc9e54; misura in tutti i controller, anche il controller 3 del 3P originale (riga 0 = (-1,0,0,tx)).
  `*(obj+0x20) = obj+0x30`. SpectCam ricava occhio = -R^T t, at = occhio - riga 2, su = riga 1 (come RelCam) + fovy da
  `*(obj+0x28)+0xa0`. Con la lettura come pos/at/up la cella guardava il cielo da vicino l'origine.
- I controller di camera vengono ricreati a ogni gara (e all'avvio): il puntatore si aggiorna dal ctor.
- Misure (diag su 0xcce64, poi rimossa): 3P originale c3 modo 1, kart seguito 11/10 (cambia); 7P c4 modo 1, matrice che cambia.
- Verificato a video: 7P cella 8 e 5P cella 6 con inquadratura TV che segue i kart, nessun HUD, nessun Lakitu; Lakitu 1-7 / 1-5
  nelle proprie celle. Regressione 3P (quarta cella come l'originale), 4P, 1P senza errori; 2P non risulta nel log.
- Dominante verde su tutte le viste 3D (e frammenti neri una volta nel 3P) con `use_asynchronous_shaders=true` (non predefinito):
  con il default (false) spariscono con la stessa build. **Ma secondo l'utente senza la mod il verde non compare nemmeno con gli shader
  asincroni: e' una regressione della mod (visibile solo con shader asincroni), APERTA.** Comparsa con la prima build in cui la cella
  vuota disegnava davvero la scena dalla camera TV (SpectCam corretta); l'analisi statica del controller (subagente) non trova scritture
  per vista nei parametri di luce/esposizione: sospetto lato render (dati per vista >= 4) o pipeline nuove della camera TV.
- Aperto: nel 5P e nel 6P (griglia 3x2) tutte le viste tremano (non dipende dalla cella vuota: anche nel 6P).

## Bisezione del verde con shader asincroni (5 nov 2026) [VERIFICATO in Eden: non riproducibile]
Tutte le prove con `use_asynchronous_shaders=true`, gara a Mario Kart Stadium: 6P e 8P (nessuna cella vuota), 7P con SpectCam spenta
(`MK8DX_SPECTOFF=1`, ripiego sul kart 0), 7P con SpectCam e misura della matrice c4 per frame (0 frame anomali: righe ortonormali,
valori finiti, fovy valido), 7P a cache shader vuota (vecchia cache in `user/shader/0100152000022000.bak-20261105`), 7P senza
ScreenFxFlagHook (`MK8DX_NO7CFIX=1`), 7P con la build esatta del 4 nov in cui era stato visto (6d11dc6, worktree): **mai verde**.
Conclusione: non e' una regressione riproducibile della mod; quasi certamente un artefatto transitorio della compilazione asincrona
di Eden alla prima sessione su questo PC (cache appena creata, pipeline nuove della camera TV). Gli interruttori di prova
`MK8DX_SPECTOFF` e `MK8DX_NO7CFIX` restano in scripts/build.sh (default 0 = binario identico). Shader asincroni rimessi a false.

## Correzioni successive (4 nov 2026 notte) [VERIFICATO in Eden]
- **Scelta automatica del kart**: 0x86ca80..0x86ca8c legge settings+0x184 (= numero di locali; 8 nel menu) e con 3 chiama
  `0x7d4c94(sys+0x228, 3)` (0x86cbc8 `orr w1,wzr,#3`) -> `ctrl[3]+0xf4 = 1` + vt+0x48: lo spettatore cambia kart seguito.
  Hook inline 0x86ca88 (`cmp w8,#3`): con 5/7 la stessa chiamata con indice 4. Prima la cella vuota seguiva sempre P1.
  Il gestore dei controller e' `sys+0x228` (sys = 0x7e7434()+0x190[4]), non +0x248 come scritto sopra.
- **0xcdd18** (sfera della camera: 0x19d3e8(attore, 1/0, vp) nasconde gli oggetti vicini nella vista vp): per il controller 4 la vista
  e' quella della cella vuota (nLocal), non la 4 (= P5); idem nello sgancio P3. **0x1088c8** (flag effetti a schermo per vista, csel
  sulla voce 0 = P1 con vp >= count): saltato per vp >= 4, rifatto solo il conteggio 0x108388. Da analisi statica (subagente).
- **Tremolio 5P/6P**: misurato che la camera e' stabile (offset nel sistema del kart continuo); erano i bordi delle colonne a
  1280/3 = 426.67/853.33. Ora i bordi sono arrotondati a pari (426/854) in proto::Cell; 8P invariato. Verificato: tremolio sparito.
