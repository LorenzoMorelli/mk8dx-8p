# Step 2 – RenderLayerManager (main 4.0.1): layer per vista, rettangoli, limiti a 4

Analisi statica, nessuna modifica al gioco. Offset = main decompresso. Dati grezzi in `step2_data/`
(`rlm_ctor_head.asm`, `rlm_ctor_rest.asm`, `rlm_layers.txt`, `rlm_update.asm`, `fn_3cd008.asm`,
`fn_3cd194.asm`, `fn_3cd5c8.asm`, `fn_108a2c.asm`, `fn_3d230.asm`).

## 0. Quadro [VERIFICATO]
```
layerSystem = *[0x12fa5e0]                 (globale; 0x667a94 registra i layer per slot)
  +0x40 count/capacità slot, +0x48 ptr[slot] -> layer   (lookup: ldr x8,[ls+0x48]; cmp [ls+0x40],slot; csel)
  +0x38 ptr[display] -> target (+0xc78..+0xc88 rect/orientamento del display)
  +0x50/+0x58 array 0x38 per slot (0x680e30), +0x60/+0x68 ...
RenderLayerManager (mgr; ctor parte 1 0x3c96ac, parte 2 0x3ca44c "RenderLayerManager::LayerHeap")
  +0x38/+0x3c/+0x40  lista di tutti i layer (count/cap/ptr[])
  +0x120/+0x124/+0x128 lista layer 3D "Sub" (count/cap/ptr[])         (0x3cd104: enable loop)
  +0x158/+0x160 lista layer 2D "Sub"; +0x178/+0x180 terza lista (2D Main/Front?)  (0x3cd008, 0x3cdb0c)
  +0x1d8  camera/matrice condivisa (vtable [0x12fbc88], matrice identità a +0x1e0)
  +0x238  5 x 0xc0 oggetti camera/proiezione per vista 3D (ctor 0x61bd1c; +0x8 flag, +0x98.. near/far,
          +0xa0.. fov, +0xb0 aspect) : 0x238, 0x2f8, 0x3b8, 0x478, 0x538; +0x5f8/+0x6b8 (2D), +0x778, +0x838
  +0x9bc  4 x 0x20 "slot vista" (slot locale 0..3): +0x00 indice viewport Scene, +0x04 indice layer 3D
          (+0x9c0), +0x14 indice rect (+0x9d0), +0x18..+0x1b flag (+0x9d4 attivo, +0x9d5, +0x9d6, +0x9d7 tipo==2)
  +0xa38  13 x 16 B tabella rettangoli {x0,y0,x1,y1} in pixel (0x3c9e30: pieno, metà, quarti da (w,h))
  +0xb08.. altri rect (0x3ca008); +0xbd8 modo split copiato dalla Scene (0/1/2/3), +0xbdc scala
  +0xbe4 contatore, +0xbe8/+0xbe9/+0xbea flag, +0xbf4/+0xbf8/+0xbfc transizione (0x3ce87c, cattura)
Layer (base ctor 0x68c458; 3D 0x68d0e4 size 0x1090; 2D 0x3c8734 size 0x1678/0x1080)
  +0x20 layerSystem, +0x30..+0x3c rect normalizzato {x0,y0,x1,y1}, +0x40 orientamento (copiati dal
  display in 0x667a94; riscritti da 0x3cd194/0x3cd008/0x3cdb0c), +0x68 ptr camera condivisa (mgr+0x1d8),
  +0x70 ptr camera/proiezione della vista (mgr+0x238+i*0xc0 per i 3D; oggetto 0xc0 proprio per i 2D),
  +0x78 flag (bit 0x40 set in registrazione; bit1 = abilitato: 0x102dfc), +0x84 slot, +0xde4 flag 2D
Tabella globale 0x133ea90 (bss): +0x3c.. int [modo][slot] (16 B per modo, 4 slot) -> indice rect (0..12)
```

## 1. I 15 layer (ctor 0x3ca44c) [VERIFICATO: `rlm_layers.txt`]
Ogni layer: `0x5faba8(size, heap, 8)` → ctor → `0x667a94(layerSystem, layer, slot, &{heap,nome}, 0, heap)`
→ `stp mgr+0x1d8, mgr+0x238+i*0xc0, [layer+0x68]` (3D) → aggiunta alle liste → `0x6651a8`/`0x665144`.

| nome | size | ctor | slot layerSystem | camera (+0x70) |
|---|---|---|---|---|
| 3D Main \| Sub0 | 0x1090 | 0x68d0e4 | 2 | mgr+0x238 |
| 3D Sub1 | 0x1090 | 0x68d0e4 | 3 | mgr+0x2f8 |
| 3D Sub2 | 0x1090 | 0x68d0e4 | 4 | mgr+0x3b8 |
| 3D Sub3 | 0x1090 | 0x68d0e4 | 5 (`mov w23,#5` 0x3ca740) | mgr+0x478 |
| 3D Back | 0x1090 | 0x68d0e4 | 0 (0x3ca81c) | mgr+0x538 (idx 4) |
| 3D Extra | 0x1090 | 0x68d0e4 | 11 | – |
| 2D Back | 0x1678 | 0x3c8734 | 1 | proprio (0xc0, ctor 0x61bd1c) |
| 2D Sub0..Sub3 | 0x1678 | 0x3c8734 | 6, 7, 8, 9 (0x3cb384) | proprio |
| 2D Main | 0x1678 | 0x3c8734 | 10 | proprio |
| 2D Front | 0x1678 | 0x3c8734 | 12 | proprio |
| 2D Overlay | 0x1080 | 0x3c8734 | 13 | proprio |
Il layer 3D "slot i+2" = vista locale i: 0x3d230 usa `ls->[0x48][w2+2]` (`add w9,w2,#2` a 0x3d26c);
0x108a2c cicla `x27 < 4` con layer `[x27+2]` (0x108b94). Iterazione "tutti i layer": `cmp x22,#0xe` (14) in 0x3c6a20.

## 2. Catena vista → rettangolo [VERIFICATO]
1. **0x3cbd94(mgr)** "sincronizza dalla Scene": legge Scene (0x8ace60) +0xb0..+0xbc (slot→viewport, 4),
   +0xcc modo → `mgr+0xbd8` (0/1; 2 → 2 oppure 3 se c'è uno slot attivo), +0xd8; per slot `i` in 0..3
   (loop `x10 += 0x20`, `cmp x10,#0x80` a 0x3cbf8c): `mgr+0x9bc+i*0x20 = vpIndex`, flag da viewport+0x40
   (1/2/3) e +0x44, **indice rect = tabella[0x133eacc + modo*16 + i*4]** (0x3cbf60-0x3cbf74) in
   `+0x9d0`. Poi chiama 0x3cd194 (rect ai layer 3D/2D Sub), 0x3cd5c8 (HUD/pause per modo), 0x3cd008,
   0x3cdb0c (rect pieno alle altre liste), 0x3ce258.
2. **0x3cd194(mgr)**: per slot i: layer = `mgr->[0x128][mgr+0x9c0+i*0x20]`; se attivo (+0x9d4):
   rect = `mgr+0xa38 + rectIdx*16` (`cmp #0xd` → 13 rect) × scala `mgr+0xbdc` → `stp [layer+0x30/+0x38]`,
   aspect = (x1−x0)/(y1−y0) → `[layer+0x70]+0xb0`, flag `[layer+0x70]+0x8 = 1`; poi
   `0x6651a8(layer, attivo)`. Stesso per i 2D Sub (+0x9d5). (0x3cd1d0-0x3cd344)
3. **0x3c9e30(mgr, size)**: costruisce i 13 rect a +0xa38 da (w,h) = `[0x12fab00]` e `size`:
   pieno, metà larghezza (×0.5), metà altezza, quarti … (0x3c9e6c-0x3c9fa0); 0x3ca008 aggiunge rect
   HUD (+0xb08.., con 0.625 e costanti da 0x133ea9c).
4. Job di rendering: 0x680f94 (switch comando) → 0x683ecc → **0x61e104(rect, ctx, target)** con
   `x0 = obj+0x48` dove la Viewport {+0x8..+0x14 float, +0x18 orientamento} viene dal layer (copiata
   da layer+0x30 via 0x61e004-style; il job riceve `[cmd+0x20]`) → pixel = norm × dimensione target →
   `nvnCommandBufferSetViewport` (slot 0x1346008). Conferma runtime: 2P 960×1080 ×2, 4P 960×540 ×4.

## 3. Limiti a 4 nel manager [VERIFICATO]
| sito | limite | note |
|---|---|---|
| 0x3cbd94 loop `cmp x10,#0x80`; Scene +0xb0..+0xbc | 4 slot vista | array mgr+0x9bc 4×0x20, subito seguito da altri campi (+0xa38 rect table) → **non estendibile in loco** |
| tabella 0x133eacc `[modo][4]` | 4 slot × modi 0..3 | bss, valori da leggere a runtime (nessun writer statico trovato tra gli 8 xref: sono tutti lettori/`add`) |
| 0x3cd194/0x3cd5c8 `+0x9d0..+0x9d7` per slot | 4 | stesso array |
| 13 rect a mgr+0xa38 (`cmp #0xd`) | nessun rect 3×2 / 4×2 | aggiungere 6+8 rect (tabella propria) |
| layer 3D Sub0..3 (slot 2..5), 2D Sub0..3 (slot 6..9) | 4+4 layer | servono 4+4 layer nuovi con slot ≥14; capacità `[ls+0x40]` da leggere a runtime (bounds-check in 0x667a94 e in tutti i lookup) |
| camere per vista mgr+0x238 5×0xc0 (ctor 0x61bd1c), 0x3ccb1c fov per 5 | 5 | per le viste 5-8: 4 oggetti 0xc0 propri (ctor 0x61bd1c + 0x61bf00) |
| 0x108a2c `cmp x27,#4`, 0x3d230 `slot+2` | 4 | consumatori per vista (audio/effetti), vanno estesi o lasciati a 4 nel prototipo |

## 4. Abilitazione per frame e camera [VERIFICATO parziale]
- `0x6651a8(layer, bool)` / `0x665144(layer, bool)` = abilita/disabilita (chiamate da 0x3cd194,
  0x3cd008, 0x3cdb0c, 0x3cd104 con il flag `+0x9d4/+0x9d5` o 0/1); `0x102dfc` legge `layer+0x78`
  bit1 per sapere se lo slot 4 (3D Sub2) è attivo.
- La camera della vista i entra nel layer tramite `layer+0x70` (mgr+0x238+i*0xc0): 0x3ccb1c scrive
  fov/proiezione nei 5 oggetti (`cmp #5`, tanf(fov·0.5)); 0x7e9b04 (loop Lakitu) e 0x7e8b9c.. scelgono
  i preset. [IPOTESI] la view-matrix per layer arriva da `layer+0x68` (mgr+0x1d8 condivisa) aggiornata
  per vista nel job; da confermare con hook su 0x61e104 (x1 ctx) e dump di `[cmd+0x20]`.

## 5. Proposta (prototipo 6P/8P nel modulo)
1. **Layer extra**: dopo il ctor 0x3ca44c (hook trampolino sul suo chiamante o su 0x667a94 dell'ultimo
   layer) allocare 4 layer 3D (0x5faba8(0x1090,heap,8) + 0x68d0e4) e 4 2D (0x1678 + 0x3c8734),
   registrarli con 0x667a94(ls, layer, slot 14..21, &{heap,"3D Sub4".."2D Sub7"}, 0, heap), copiare
   `+0x68` (mgr+0x1d8) e assegnare a `+0x70` 4 oggetti 0xc0 propri (ctor 0x61bd1c, poi 0x61bf00 con
   gli stessi argomenti usati a 0x3c9d00..). Prerequisito: `[ls+0x40]` ≥ 22, altrimenti rilocare
   l'array `[ls+0x48]` (hook REPLACE dei lookup: tutti usano il pattern `cmp [ls+0x40], idx; csel`).
2. **Slot vista 0..7**: sostituire 0x3cbd94 e 0x3cd194 con versioni del modulo che usano un array
   proprio di 8 slot (stesso layout 0x20) e una tabella rect propria (13 originali + 6 rect 3×2 +
   8 rect 4×2 in pixel da (w,h) = 1920×1080 / 1280×720: 640×540 e 480×540), mappa `[modo 3|4][slot]`.
   Scene: modi 3/4 in 0x7e76b8 (hook: n 5-6 → modo 3 con 6 slot, 7-8 → modo 4 con 8 slot) e tabella
   slot→viewport a 8 (Scene+0xb0 è 4×s32: usare la tabella del modulo, hook REPLACE su 0x86eb1c/
   0x86eb84/0x86ebf8 e sul loop 0x7e7af8).
3. **Camere 5-8**: nel prototipo (giocatori fantasma) usare gli oggetti camera extra con la stessa
   proiezione dei Sub e view-matrix di un kart CPU (Lakitu: ctor 0x7e80ec, loop 0x7e9b04 `cmp x25,#4`
   → 8).
4. **HUD**: 0x3cd5c8 posiziona le pagine RaceView per modo (`cmp mgr+0xbd8, #1/#2/#3`): per i modi
   3/4 lasciare il ramo 4P e gestire i layout 6P/8P nel passo HUD dello step 6.
5. Hook di verifica allo step 2: trampolino su 0x3cbd94 (log di modo, slot, rectIdx, tabella
   0x133eacc), su 0x3cd194 (rect scritti per layer) e lettura di `[ls+0x40]`, `mgr+0xa38..` (13 rect),
   `0x133eacc` (4 modi × 4) a runtime: i valori bss non sono ricavabili staticamente.

## 6. Non determinato staticamente
- Valori della tabella `[modo][slot]` 0x133eacc e dei 13 rect (dipendono da (w,h) runtime).
- Capacità `[ls+0x40]` del layerSystem (ctor non individuato: nessun `str` a `[0x12fa5e0]` trovato
  con il pattern adrp+str; probabilmente scritto tramite registro).
- Come il job copia `layer+0x30` nella Viewport usata da 0x61e104 (`[cmd+0x20]+0x48`): da confermare
  con il backtrace già disponibile (0x683fc4 ← 0x680f98) e un hook su 0x61e104 che logghi x0−0x48.
