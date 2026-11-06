# Step 2b – Da dove viene la view matrix dei layer "3D Sub i" (main 4.0.1)

Analisi statica, nessuna modifica al gioco. Dati grezzi in `step2_data/vm_*.txt`. Nomi delle classi sead
dedotti da layout e codice (LookAtCamera, PerspectiveProjection).

## 0. Catena in breve [VERIFICATO]
```
kart k (0..11)  --calc per frame-->  KartCamUnit = [kart+0x20] (0x2b0 B, ctor 0x117de8)
   0x119524(unit) -> 0x1197f8(unit, unit+0x8): scrive il descrittore desc0 (pos/at/up/fovy)
   registrazione: 0x1184e4 -> 0xc8f24(dir, idx=kart+0x40, {unit+0x8, unit+0x40, unit+0x78, &unit+0xca})
CameraDirector dir = [[0x7e7434()+0x190+6*8]+0x1f8] (0xb00 B, ctor 0xc7b74/0xc8328, vtable 0x11b88e0)
   +0x3d0 12 x 0x28 sorgenti (una per kart)   +0x70 4 x 0xd0 code di richieste per viewport
   +0x5b0 4 x 0x60 LookAtCamera (una per viewport)  +0x730 4 x 0xc0 proiezione (ctor 0x61bd1c)
   vfunc 0xc89cc: vp -> sorgente (0xc8f48) -> richiesta in coda;  vfunc 0xc8b60: per vp 0..3
   0xc8d1c(dir, vp, desc) -> LookAtCamera[vp] <- desc;  cams[vp]->doUpdateMatrix(cams[vp]+8)
RenderLayerManager mgr: +0x958 cams[4] (sead::Camera*), +0x978 projs[4]  (puntati da dir+0x60/+0x68)
   0x3cc218 (da 0x3c4918): layer3D[slot i]+0x68 = mgr.cams[vp(i)] ?: mgr+0x1d8; copia fovy/near/far
   da projs[vp] nella proiezione del layer (mgr+0x238+i*0xc0)
Job di disegno 0x683ecc -> 0x667d10: RenderInfo+0x28 = camera = layer+0x68 (0x665090), +0x30 = proj
   = layer+0x70 (0x6650cc), +0x38 = layer+0x48 viewport -> layer->vfunc[0x40](layer, &info)
VIEW MATRIX DELLA VISTA i = *(Matrix34f*)([layer3D_i+0x68] + 0x8)  (0x30 B, aggiornata ogni frame)
```

## 1. mgr+0x1d8: camera di default, non per vista [VERIFICATO]
- Ctor RLM 0x3c9770-0x3c97d8: `str vtable,[mgr+0x1d8]` (GOT 0x12fbc88 -> vtable 0x12afd40), memcpy 0x30 B
  identità (`[0x12fa188]`) in +0x1e0, pos.z = 10.0 (+0x218), up.y = 1.0. Seconda istanza uguale a mgr+0x8f8.
- Classe = sead::LookAtCamera (0x60 B): +0x0 vtable, **+0x8 Matrix34f view**, +0x38 pos, +0x44 at, +0x50 up.
  Vtable 0x12afd40: +0x00 0x619dd8, +0x08 0x619ea4 (dtor), +0x10 0x61957c, +0x18 0x619a04,
  **+0x20 0x619a08 = doUpdateMatrix(cam, Matrix34f* dst)** (legge +0x38/+0x44/+0x50, normalizza, cross).
- Non contiene array per vista e non viene riscritta per layer: è solo il **fallback** usato da 0x3cc218
  quando `mgr.cams[vp]` è nullo o lo slot non ha viewport (`str x23,[layer+0x68]` a 0x3cc298/0x3cc390).
- Array per vista nel manager: `mgr+0x958` cams[4], `mgr+0x978` projs[4] (azzerati 0x3c9df0-0x3c9e04;
  memset 0xd8 da +0x958 a 0x3c9928). Subito dopo c'è +0x998.. (altri campi): **non estendibile in loco**.

## 2. Job 0x683ecc / 0x667d10: sorgente di view e proiezione [VERIFICATO]
- 0x683ecc(renderer x0, ctx x1, **layer x2**, idx w3): 0x683fac `bl 0x667d10(&info, ctx, ..., layer)`.
- 0x667d10: info+0x18 = layer, +0x20 = layer+0x84 (slot), **+0x28 = 0x665090(layer)**,
  **+0x30 = 0x6650cc(layer)**, +0x38 = layer+0x48 (viewport). Poi 0x683fd4 `blr [layer.vtbl+0x40]`
  (disegno del layer con info) e 0x666050; 0x61e104(layer+0x48,…) imposta il viewport.
- 0x665090: se `layer+0x7a` bit1 e (bit0|bit5) e `[layer+0x1e8]`≠0 → camera = `[layer+0x1e8]+0x8`
  (override, es. render-to-texture); altrimenti **`layer+0x68`** (nullo → default `[0x12fa5e8]`).
- 0x6650cc: se `layer+0x7c` bit1 → `[[layer+0x1e8]+0x100]`; altrimenti **`layer+0x70`** (default `[0x1307360]`).

| layer | +0x68 (view) scritto da 0x3cc218 | +0x70 (proj) |
|---|---|---|
| 3D Sub i (i=0..3, slot i+2) | `mgr.cams[vp_i]` (mgr+0x958+vp_i*8), vp_i = mgr+0x9bc+i*0x20; fallback mgr+0x1d8 | mgr+0x238+i*0xc0 (parametri copiati da `mgr.projs[vp_i]`) |
| 3D Back/Extra | mgr+0x1d8 (ctor) | mgr+0x538 / – |

0x3cc218 copia dalla proiezione sorgente: vfunc+0x30 (fovy) → 0x61bdd0 setFovy, +0xb4/+0xb8, vfunc+0x28
(far, +0x9c) e vfunc+0x20 (near, +0x98), con flag dirty +0x8 (0x3cc2b8-0x3cc4c0). Loop `cmp x22,#0x80` (4 slot).

## 3. Percorso di scrittura [VERIFICATO salvo dove indicato]
1. **Sorgente per kart**: KartCamUnit (ctor 0x117de8, alloc 0x2b0 in 0x16ca84, salvato in `kart+0x20`;
   `unit+0xb0` = kart; `kart+0x40` = indice kart, letto da 0x16dc98). Tre descrittori da 0x38 B a
   unit+0x8, +0x40, +0x78 (vtable GOT 0x12fc118). Layout descrittore d: **+0x8 tipo** (1 = look-at),
   **+0xc pos, +0x18 at, +0x24 up, +0x30 fovy (rad)**, +0x34/+0x38 extra (tipo 2); tipo 0: +0x10
   sead::Camera*, +0x18 proiezione* (camere esterne, es. replay/cutscene).
   Assoluti per desc0: pos = unit+0x14, at = unit+0x20, up = unit+0x2c, fovy = unit+0x38.
2. **Calcolo per frame**: 0x16cd14(kart) → 0x16cd9c `bl 0x119524(unit, kart+0x50)`: se `unit+0xc8`
   (=1 dal ctor) → 0x1197f8(unit, unit+0x8) (scrive desc0, 0x11991c-0x119964), poi desc1 (+0x40).
   0x11864c(unit, pose) = reset/teletrasporto (chiamato da 0x16d94c, 0x1732cc): scrive pos/at/fovy
   (0x118900-0x118980). [IPOTESI] gating in 0x16cd14: salta se `[dir4+0x238]+0xe9` e `kart+0x4c`==0.
3. **Registrazione**: 0x1184e4 (da 0x16cfa0, init gara del kart) → 0xc8f24 = memcpy 0x28 B in
   `dir+0x3d0+idx*0x28` (idx<12): {desc0*, desc1*, desc2*, flag*}. In 0xc89cc si usa desc1 se `*flag`
   (unit+0xca) altrimenti desc0.
4. **Scelta vp → kart**: 0xc8f48(dir, vp): Scene(0x8ace60)+0x40+vp*0x18: se flag|2==3 → kart =
   `+0x4c` (stessa tabella che usa il loop Lakitu); altrimenti 0x7d4cf0(dir4+0x228, vp)+0x40 (replay).
5. **Director** 0xc89cc (vtable +0x80 via 0xc8830): per vp 0..3 (`cmp x21,#4` a 0xc8b2c) accoda la
   richiesta (priorità 10) nella lista dir+0x70+vp*0xd0. 0xc8b60 (vtable +0x90): per vp 0..3 prende la
   testa e chiama **0xc8d1c(dir, vp, desc)**: tipo 0 → `mgr.cams[vp]=desc+0x10`, `projs[vp]=desc+0x18`;
   tipo 1 → **0xc9568**: copia pos/at/up (normalizza up) in `dir+0x5e8+vp*0x60`, setFovy(desc+0x30) su
   `dir+0x730+vp*0xc0`, poi `mgr.cams[vp] = dir+0x5b0+vp*0x60`, `projs[vp] = dir+0x730+vp*0xc0`
   (0xc9670-0xc9684); tipo 2 → 0xc9698 (uguale + 2 float in proj+0xb4/+0xb8). Coda vuota → cams[vp]=0.
   Infine (0xc8c70-0xc8d0c) per cams[0..3]: **`blr [cam.vtbl+0x20](cam, cam+8)`** = view matrix.
   Tutti gli indici vp passano per `cmp #4; csel ...,xzr` → vp≥4 ricade sulla vista 0.
6. **Sync layer**: 0x3c4918 → 0x3cc218(mgr) (vedi §2). Array per vista nel director: lista 4×0xd0 @+0x70,
   LookAtCamera 4×0x60 @+0x5b0, proiezione 4×0xc0 @+0x730 (ctor 0x61bd1c ×4 a 0xc8590-0xc85a8), flag
   4×4 @+0xaf0; sorgenti 12×0x28 @+0x3d0. Oggetto 0xb00 B: **nessun array a 5/8, non estendibile**.
7. **"Lakitu" (0x3d0, ctor 0x7e80ec)**: vfunc+0xb8 = 0x7e8868 `strb w1,[x0,#0x168]` (salva solo il vp);
   i preset 0x7e8b9c.. scrivono offset in `[obj+0x160]+0x138..` scelti da Scene+0xcc (modo split).
   [IPOTESI forte] sono gli attori Lakitu visibili per vista, **non** producono la view matrix.

## 4. Conclusione operativa per le viste 5-8
Cosa va riempito ogni frame per un layer "3D Sub4..7" (registrato come da step2 §5):
- `layer+0x68` = puntatore a una **LookAtCamera propria** (0x60 B, vtable = `[0x12fbc88]+0x10`; init come
  0xc84b0-0xc84e8: matrice identità, up (0,1,0)). Basta assegnarlo una volta: 0x3cc218 tocca solo i 4
  layer indicizzati da mgr+0x9c0. **La view matrix è `cam+0x8`**, ricalcolata ogni frame con
  `0x619a08(cam, cam+8)` dopo aver copiato pos/at/up.
- `layer+0x70` = proiezione 0xc0 propria (ctor 0x61bd1c): fovy con **0x61bdd0(proj, fovy)**; near/far
  in +0x98/+0x9c, aspect +0xb0, flag dirty +0x8 = 1 (come 0x3cd194 / 0xc8d1c).
- Sorgente per un kart CPU k, senza rifare la matematica (il gioco calcola già la chase-cam di tutti i
  12 kart): `dir = [[0x7e7434()+0x190+0x30]+0x1f8]`; `e = dir+0x3d0+k*0x28`;
  `d = (*(u8*)e[3]) ? e[1] : e[0]`; se `d+0x8`==1/2: pos=d+0xc, at=d+0x18, up=d+0x24 (normalizzare),
  fovy=d+0x30; se 0: camera = `*(d+0x10)` (usarla direttamente come layer+0x68), proj = `*(d+0x18)`.
  Equivale a rieseguire 0xc9568 per un vp ≥4, che il gioco non può fare (clamp a 4).
- Alternativa "Watch/replay": tipo 0 del director (camere esterne) o 0x7d4cf0 → stessa via (desc tipo 0).

Offset da hookare:
| dove | perché |
|---|---|
| ritorno di 0xc8b60 (0xc8d10) oppure dopo `bl 0x3cc218` (0x3c4960) | riempire le LookAtCamera extra da `dir+0x3d0+k*0x28` e chiamare 0x619a08; settare layer+0x68/+0x70 extra |
| 0xc8f48 (REPLACE, vp≥4) | solo se si vuole riusare la logica Scene vp→kart con una tabella vp propria a 8 |
| 0x3cc218 (REPLACE) | solo se si estende mgr.cams/projs a 8 con array del modulo (oggi 4, +0x958/+0x978) |
| 0x16cd14 / 0x119524 | verifica runtime che desc0 dei kart CPU sia aggiornato ogni frame (gating `kart+0x4c`) |

## 5. Da verificare a runtime
- `mgr+0x958..+0x990` in 1P/2P/4P (atteso: dir+0x5b0+vp*0x60 / dir+0x730+vp*0xc0) e `layer+0x68` dei 4 Sub.
- `dir+0x3d0` 12 voci non nulle in gara con 12 kart; desc0 (unit+0x14..+0x38) che cambia per frame
  anche per i CPU (watchpoint su `[kart+0x20]+0x14`).
- Override `layer+0x1e8` (0x665090) mai attivo per i Sub in gara.
