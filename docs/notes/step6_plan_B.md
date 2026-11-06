# Step 6 B – viste, HUD e camere per 6/8 locali veri (piano statico, main 4.0.1)

Offset = main 4.0.1 decompresso (../img/main.bin). Solo lettura del disassemblato: nessuna build, nessun emulatore.
Estratti in `step6_data (dati non pubblicati)/b_*.asm|.txt` (citati per nome). Etichette: [VERIFICATO] = letto nel disassemblato,
[IPOTESI] = dedotto, [DA VERIFICARE A RUNTIME] = serve un log/watchpoint in Eden.
Il crash all'avvio della gara a 8 (oggetto per-locale con array a 4 a +0xa0, 0xaa16c/0xac290/0x9d4a0) è del collega A:
qui non è analizzato, ma §1.3 segnala un effetto della Scene attuale che può interessargli.

## 0. Risposta breve
- **Perché esce modo 2 e slot→vp={4,1,2,3}** [VERIFICATO]: 0x7e76b8 calcola `n' = n - spettatore`, modo = 0/1/2 per
  n'≤1 / ==2 / >2 e "slot" = 1/2/4, ma **il ciclo dei viewport gira n' volte (non "slot" volte)** e l'indice viewport
  è limitato con `cmp #5; csel ...,xzr` (0x7e7844): con n=8 i locali 5,6,7 riscrivono il **viewport 0** (l'ultimo è il
  locale 7), il locale 4 finisce nel viewport 4. 0x7e7af8 poi fa `+0xb0[clamp4(slot)] = vp`: vp0 (slot 7 → indice 0)
  scrive `+0xb0[0]=0`, vp4 (slot 4 → indice 0) lo sovrascrive con 4 → {4,1,2,3}.
- **La Scene non si può allargare in loco** [VERIFICATO]: è un oggetto da **0x128 B** (alloc 0x5faae0(0x128) in 0x8acc24,
  creato una sola volta da 0x8ac320 in system+0x7f8), l'array viewport è **5 × 0x18 a +0x38** (non +0x3c: +0x38 è
  l'indice, +0x3c lo slot) e dopo ci sono +0xb0 s32[4], **+0xc0 u8[12] maschera "kart visibile nei viewport"**, +0xc8,
  +0xcc modo, +0xd0, +0xd4, +0xd8, +0xe0..+0x11f sotto-oggetto, +0x120..+0x122. Ingrandire l'allocazione non aiuta
  (gli offset fissi seguono l'array); una tabella a 8 richiederebbe di riscrivere ~64 lettori.
- **Via più economica (consigliata)**: lasciare la Scene un **4P coerente** (locali 0..3 → viewport 0..3, +0xb0={0,1,2,3},
  modo 2) con un trampolino su 0x7e76b8, e tenere i locali 4..7 in una **tabella ombra nel modulo** usata solo dalle
  viste extra dello step 2 (camere reali dai descrittori dei kart 4..7), dall'HUD e da pochi hook mirati. Il modo split
  6/8 resta una variabile del modulo (`view8::split`), non un valore di Scene+0xcc: i 31 lettori di +0xcc (tutti
  `cmp #0/#1/#2`) continuano a vedere il 4P; solo il nome layout (0x54798c/0x548d20) e le partizioni (0x536a10) vengono
  agganciati.
- **HUD**: le pagine RaceView/Front vengono registrate da 0x406870 (ramo a 0x406c94) **una per viewport attivo di tipo 1
  con slot ≤ 3**, id = 0x65+slot / 0x6a+slot, layer = 6+vp (2D Sub vp). Gli id corretti sono **RaceView0..3 = 101..104
  (0x65..0x68), Race = 105 (0x69), RaceViewFront0..3 = 106..109 (0x6a..0x6d)** (STRUCTS riporta i numeri di riga di
  img/pages.txt, sfasati di 2). La tabella pagine ha 0x88 voci (max id 0x87, 0x8bcc78) e **nessun id libero
  dedicato**: per le pagine dei locali 4..7 servono id "presi in prestito" + 3 hook (fase H2). La fase H1 (4 pagine con
  layout 8P/6P, partizioni) è a basso costo.
- **Camere**: con 8 umani i kart 4..7 hanno `KartCamUnit+0xc8 = 1` (il flag si azzera solo se `Vehicle+0xd2` = tipo kart
  1 = CPU, 0x117fb4) [IPOTESI forte]: il loro descrittore desc0 viene calcolato ogni frame dal gioco. Basta che le viste
  extra copino il descrittore del kart del locale 4..7 come fa 0xc9568 per i viewport 0..3 (invece della ricostruzione
  dalla posa del kart 0 dello step 2). Il "preset" 0x1358e40 non è la camera del giocatore: è l'offset del Lakitu
  (oggetto 0x3d0, [obj+0x160]+0x138..) → cosmetico.

## 1. SceneInitRace 0x7e76b8 per intero (b_7e76b8_sceneinitrace.asm) [VERIFICATO]

### 1.1 Algoritmo (x19 = Scene, x20 = settingsA = 0x86f7bc())
```
n    = 0x86f154(settingsA)                       (w21; settings+8==4 -> +0x19c, altrimenti +0x188[0] clamp 5)
d0   = *(int*)(0x7e7404()->[0x20])               (sp+0x3c)
spet = 0x86a430(raceMgr)                         (w22: entry "osservata" o -1)
w9 (flag spettatore): settings+0x24 bit4 || settings+4==1 -> 0, altrimenti (d0==0) && (n<2 || spet>=0)   (0x7e7708-0x7e7948)
w25  = w9 ^ (w9 && n>1 && spet<0)                (1 = un viewport va saltato per lo spettatore)
n'   = max(n - w25, 0)                           (w26/w24, 0x7e7744 bic asr#31)
modo = n'<=1 ? 0 : (n'==2 ? 1 : 2);   slot = n'<=1 ? 1 : (n'==2 ? 2 : 4)       (0x7e774c-0x7e776c)
Scene+0xcc = modo;  Scene+0xd0 = (d0==2) ? 2 : 1                                   (0x7e7778 stp)
buf[12] (sp+0xc..sp+0x3b, 12 parole: sp+0xc=-1, resto 0): for j < settings+0x180: buf[j] = 0x86e8e4(j)  (raceMgr+0x1cc[j])
ciclo 1 (0x7e77e4..0x7e7878), i = 0 .. n'-1   <-- limite n', NON "slot"
   k = i + (w25 && k==spet)          (salta lo spettatore)
   d = settings+8==4 ? k : 0x86f64c(settings, k)      (d = indice del k-esimo descrittore con +0x44==0 = umano locale)
   tipo = settings[0x44 + clamp12(d)*0x1c] != 0 ? 2 : 1
   e = Scene + 0x38 + clamp5(i)*0x18      (0x7e7844 `cmp x26,#5; csel x9,x26,xzr,lo`)
   e{+4 slot=i, +8 attivo=1, +0xc tipo, +0x10 = buf[d], +0x14 kart=d};  0x7e7af8(Scene)
ciclo 2 (0x7e7898..0x7e7904), solo se slot > i_finale: viewport "extra" (tipo 2, o 1 se settings+8==2) che seguono
   il kart min(j, settings+0x180); slot = i_finale + j; clamp `cmp w11,#5` a 0x7e78d8.
```
Entry viewport (0x18 B, base Scene+0x38+i*0x18; ctor/init 0x8acc70.. / 0x7e7978..): **+0x0 indice (costante i), +0x4
slot locale (-1), +0x8 attivo, +0xc tipo (1 giocatore, 2 extra), +0x10 slot locale del kart (raceMgr+0x1cc), +0x14
kart**. L'entry 4 occupa +0x98..+0xaf; +0xb0 è la tabella successiva. (STRUCTS: "+0x3c 5×0x18 {+0 slot...}" = stessa
cosa vista da +0x3c.)

0x7e7af8 (chiamata dopo ogni viewport e da 0x7e7950/0x7d4c6c): `stp -1,-1 -> +0xb0..+0xbf`; per i = 0..4 se
attivo e slot != -1: `+0xb0[slot<4 ? slot : 0] = i` (0x7e7b14, 0x7e7b38, 0x7e7b60, 0x7e7b88, 0x7e7bb0); poi
`+0xc0..+0xcb = 0` e per i = 0..4 con `attivo|2 == 3` e kart != -1: `+0xc0[clamp12(kart)] |= 1<<i`
(0x7e7bc4..0x7e7ccc).

### 1.2 Cosa succede con n = 8 [VERIFICATO, coerente con il log dello step 5]
n'=8 (nessuno spettatore), modo=2, slot=4; ciclo 1 i=0..7: vp0←locale0, vp1..3←1..3, vp4←4, poi i=5,6,7 → clamp →
vp0←5, vp0←6, vp0←7. Risultato: vp0 = {slot 7, kart del locale 7}, vp1..3 = locali 1..3, vp4 = {slot 4, tipo 1, kart
del locale 4}; +0xb0 = {4,1,2,3}; ciclo 2 saltato (8 > 4). Nessuna scrittura fuori oggetto (buf 12 parole ≥ 8; indici
clampati).

### 1.3 Effetti della Scene attuale (prima di qualunque fix) [VERIFICATO staticamente; impatto IPOTESI]
- **RaceView0 e RaceViewFront0 non vengono create**: 0x406cdc `ldr w8,[x22]` (slot) `cmp #3; b.hi skip` → vp0 (slot 7)
  e vp4 (slot 4) sono scartati, solo RaceView1..3 / Front1..3 vengono registrate (b_406870_race_pages.asm). Codice che
  cerca la pagina 0x65 (es. 0x53b528, 0x5470dc) la troverebbe assente. **Utile al collega A** come pista se il crash
  coinvolge una pagina nulla.
- Camera dello slot 0: RLM 0x3cbea4 copia `+0xb0[0]=4` in mgr+0x9bc; 0x3cc284 limita `vp<4` → cams[0] = camera del
  director per vp0 = **kart del locale 7**. Director 0xc8f48/0xc900c: vp0 → kart del locale 7.
- 0x86eb1c(0) → vp4 → kart del locale 4 (34 chiamanti: HUD/audio/effetti del "giocatore 1" sul kart del locale 4).

### 1.4 Opzioni
| opzione | costo | esito |
|---|---|---|
| A. Ingrandire la Scene (0x8acc3c `mov w0,#0x128`) | 1 patch + riscrivere i lettori | inutile da sola: +0xb0/+0xc0/+0xcc sono offset fissi dopo l'array |
| B. Tabella ombra a 8/9 viewport + hook di tutti i lettori | ~64 lettori Scene (sotto), 34+12 chiamanti degli accessori | troppo costosa e rischiosa |
| C. Modi 3/4 in Scene+0xcc | 31 lettori `cmp #0/1/2` → ramo "altro"; 0x54798c/0x548d20 → "" (layout vuoto) | non serve: la geometria la fa già la mod (step 2) |
| **D. Scene 4P coerente + ombra locali 4..7 nel modulo (consigliata)** | 1 trampolino + piccole tabelle | locale i → viewport i (i<4) o vista extra i (i≥4) → kart del locale i |

### 1.5 Intervento N1: trampolino su 0x7e76b8 (prima istruzione 0xd10283ff, già verificata in main.cpp)
```cpp
namespace view8 {
    struct Vp { int32_t idx, slot, active, type, locSlot, kart; };   /* = Scene+0x38+i*0x18 */
    static int nLocal, split;          /* split: 0 = vanilla (n<=4), 6 o 8 */
    static int kartOf[8];              /* locale -> kart (= indice descrittore) */
    static Vp extra[4];                /* locali 4..7: idx 5..8 fittizi, usati da HUD H2 */
}
HOOK_DEFINE_TRAMPOLINE(SceneInitRace8) { static void Callback(uint8_t* s) {
    Orig(s);
    uint8_t* st = Fn<uint8_t*(*)()>(0x86f7bc)();
    int n = Fn<int(*)(uint8_t*)>(0x86f154)(st);
    bool offline = !(st[0x24] & 0x10) && *(int32_t*)(st + 4) != 1;          /* come 0x7e7708/0x7e7718 */
    if (n <= 4 || n > 8 || !offline) { view8::split = 0; return; }        /* 1-4P: comportamento originale */
    view8::nLocal = n; view8::split = n <= 6 ? 6 : 8;
    auto* vp = reinterpret_cast<view8::Vp*>(s + 0x38);
    for (int i = 0; i < 5; i++) vp[i] = { i, -1, 0, 0, -1, -1 };           /* come 0x7e7978..0x7e79c4 */
    for (int i = 0; i < n; i++) {
        int d = *(int32_t*)(st + 8) == 4 ? i : Fn<int(*)(uint8_t*,int)>(0x86f64c)(st, i);
        int t = *(int32_t*)(st + 0x44 + (d < 12 ? d : 0) * 0x1c) != 0 ? 2 : 1;
        int loc = Fn<int(*)(int)>(0x86e8e4)(d);
        view8::kartOf[i] = d;
        view8::Vp v = { i, i, 1, t, loc, d };
        if (i < 4) vp[i] = v; else { v.idx = 5 + (i - 4); view8::extra[i - 4] = v; }
    }
    *(int32_t*)(s + 0xcc) = 2;                                             /* modo 4P */
    Fn<void(*)(uint8_t*)>(0x7e7af8)(s);   /* +0xb0={0,1,2,3}, +0xc0 maschere; 0x7e7950 lo richiama comunque dopo */
    Logging.Log("[mk8dx-8p] view8: n=%d split=%d kart=%d,%d,%d,%d,%d,%d,%d,%d", ...);
}};
```
Nessuna patch IPS a 0x7e7844/0x7e78d8/0x7e7af8: con n≤4 il gioco resta identico; con n>4 il risultato viene sostituito.
Rischio 1-4P: nullo (ramo `n <= 4` → solo Orig). Verifica: `DumpScene` già presente in SceneInitRaceHook (atteso
`modo=2 slot->vp={0,1,2,3}`, vp[i].slot=i, vp4 attivo=0) + log kartOf.

### 1.6 I lettori dei viewport (b_vp_readers_class.txt, rifinitura di view_vp_iter.txt dello step 1)
Dei 98 siti `cmn #-1; cmp #5`: **34 non leggono la Scene** (stride 0xf0/0x790/0x78 di altre strutture: falsi positivi
dello step 1), **64 sì** (49 certi + 15 probabili). Di questi **43 passano per +0xb0[k]** (cicli srotolati sui 4 locali:
"il kart X è di un giocatore locale? quale?", 0x3d01c..0x3d140, 0x126308..0x126420, 0x14eb64..0x14fc28 `HandleELink::fade`,
0x15e5e0, 0x16d0e0..0x16d38c, 0x16fa58..0x16feb4, 0x22e244, 0x3280e8, 0x510460..0x510944 `N_ItemCount_All_00`,
0x3cbd94 RLM, 0x86eb1c) e **21 per indice viewport passato o in ciclo** (director 0xc900c/0xc92b0/0xc9430, Lakitu
0x7e9b04/0x7e9c44, RLM 0x3cbe58, 0x86e894, 0x7d2610, 0x1d928, 0x772f0, 0xcf224/0xcf32c, 0x108388/0x108b60/0x1093e4,
0x16d270, 0x1b1e38, 0x1bc14c, 0x25ad80/0x25b2dc, 0x374d98).
- **Nessuno può andare fuori memoria** (tutti limitano a 5 viewport / 4 slot): con la Scene 4P coerente di N1 vedono una
  gara 4P normale.
- **Critici per far girare la gara**: solo quelli che legano viewport → camera/rendering: RLM 0x3cbd94/0x3cc218
  (camere e rect dei 4 Sub 3D), director 0xc89cc/0xc8b60/0xc8f48 (4 code, `cmp x21,#4` a 0xc8b2c), registrazione HUD
  0x406870. Tutti corretti da N1 per i locali 0..3.
- **Cosmetici per i locali 4..7** (li trattano come CPU): effetti a schermo per vista (`HandleELink::fade`, inchiostro,
  ecc.), audio per vista, conteggio oggetti HUD 0x510460, Lakitu per vista, maschera +0xc0 (§3.4). Da estendere solo se
  si vedono difetti, con hook mirati che consultano `view8::kartOf[4..7]`.

## 2. Pagine HUD per vista (b_406870_race_pages.asm, b_raceview_pages.asm, b_race_vp_objs.asm)

### 2.1 Creazione e associazione [VERIFICATO]
```
0x406870 (setup pagine della scena gara, vtable 0x1266010; ramo per viewport 0x406c94-0x406dd8):
  per i = 0..4 (x22 = Scene+0x3c+i*0x18):
     se e.locSlot>=0 && (attivo|2)==3 && e.kart>=0 && attivo && e.slot<=3 && e.tipo==1:
        id = 0x65 + slot   -> tabella[id].factory = 0x40b498 (RaceView: 0x5fa960(0x258) + ctor 0x5479dc, vtable 0x1289240)
        0x8b3cb0(this, &id, layer = 6+i)            (2D Sub i = slot layerSystem 6..9; vp4 -> 10 = 2D Main)
        id = 0x6a + slot   -> factory 0x40b4c4 (RaceViewFront: 0x200 B, ctor 0x548d70, vtable 0x1289538)
        0x8b3cb0(this, &id, layer = 10 (2D Main) oppure 6+i secondo 0x3fe658 / settings+0x24 bit4)
  tabella = 0x8bcc5c() = *([[0x1306e70]]+0x18): +0x8+id*8 factory, +0x448+id*8 proprietario, clamp `cmp #0x88`
  0x8b3cb0 -> 0x726330(+0x10, id, layer, 0, 0): crea la pagina dall'id (vtable +0x18 del registro = info per id).
RaceView onCreate 0x547d20:  slot = page+0xe8 (id) - 0x65   (0x547d5c `sub w20,w8,#0x65`, unico lettore di +0xe8 con 0x548e48)
  race = pagina 0x69 (o 0x75 Race_Watch se settings+8==4) via 0x8bf0ec
  vp = 0x86eb84(slot) (slot<=3 -> Scene+0xb0[slot]);  page+0x1d0 = race+0x358[vp] (PtrArray: +0x350 count, +0x354 cap)
  ... 0x50efa8(obj, slot), 0x547f78(this, obj, page+0x1d0) (parti L_ItemBox_00, L_Rank_00, L_LapCoin_00, ...)
Front onCreate 0x548e2c: slot = id - 0x6a (0x548e58), 0x86eb84 a 0x548ef0.
Pagina Race (rc_RaceView_Cmn_00): 0x532818.. crea **5 oggetti per viewport** (0x118 B, ctor 0x54fcc4(obj, vp,
  &Scene.vp[vp] (Scene+0x38+vp*0x18), race)) in race+0x358 (cap 5 da 0x531924 `mov w1,#5`).
  obj+0 = vp, obj+8 = puntatore all'entry viewport, obj+0x10 = pagina Race, +0xf4/+0x100 usati da 0x53635c (slot<4 → id 0x65+slot).
Nome layout = vtable slot 13: 0x54798c / 0x548d20 leggono Scene+0xcc: 0 → 1P_00, 1 → 2P_Ml, 2 → 4P_Ml, altro → "" (0xed6b28).
```
Altri usi dell'id: 0x53635c (`cmp slot,#4; b.hs`, sicuro), 0x53b528/0x5470dc/0x53d048../0x53e02c (id base 0x65/0x6a
come costante), 0x547218 (`cmp #0x65`) (b_raceview_id_users.asm). Nessun ciclo trovato che faccia `0x65+i` con i fino a
settings+0x180.

### 2.2 Fase H1 – 4 pagine esistenti con layout 6P/8P (economica)
1. **Nome layout**: REPLACE 0x54798c e 0x548d20 (prima istruzione 0xa9bf7bfd, `const char* (void* page)`):
   `if (view8::split==8) return "rc_RaceView_8P_Ml"; if (==6) return "rc_RaceView_6P_Ml"; return orig;` e per Front
   `"rc_RaceView_Front_6P_Ml"` in entrambi i casi (non esiste Front_8P nella RomFS) [IPOTESI: il Front_6P va bene
   anche in 8P]. Stesso schema del CharaParts dello step 5 (0x4be854). Le stringhe devono vivere nel modulo.
   Compatibilità pane [VERIFICATO staticamente]: tutte le parti che il main cerca (0x547f78: L_ItemBox_00, L_Rank_00,
   L_LapCoin_00, L_BtKeidoroCoin_00, L_BtScore_00, L_BtCoinCount_00, L_ShinePosition_00, L_Alarm_00/01,
   L_RivalName_00; 0x5486c4: L_Fade_00, L_BtScore_00, L_Coin_%02d; N_Pause_00/01) esistono in 6P/8P; mancano
   L_CameraPlay_00..03 ma 0x5488d8.. li cerca solo per nasconderli (0x8ade74 è null-safe: `cbz x0`); N_RivalName_00,
   N_RankPos_00, N_LapCoinPos_00 non sono referenziati dal main. [DA VERIFICARE A RUNTIME] L_Coin_%02d dentro
   L_BtScore_00 e l'assenza di abort al caricamento.
2. **Posizione**: le 4 pagine stanno sui layer 2D Sub0..3 (slot 6..9) con rect a quadranti; la loro camera/proiezione
   ortogonale è calcolata **una volta** dal rect nel ctor RLM (0x3cad20..0x3cae48, b_layer2d_create.asm). Due vie
   [DA VERIFICARE A RUNTIME]: (a) nel PerFrame dello step 2 impostare anche il rect dei 2D Sub i alla cella i e
   ricalcolare la proiezione come 0x3cad20 (0x61bf00 sulla proiezione, camera a +0x38..); (b) lasciare i layer e
   spostare/scalare il RootPane della pagina (layout 8P autorato 640×720 per una cella 320×360 [IPOTESI step 6 romfs]).
3. **Partizioni**: trampolino su **0x536a10** (onCreate della pagina comune, prima istruzione 0xd10143ff): l'originale
   nasconde P_Partition_x se modo==1, x e y se modo==0, niente in modo 2 (b_536a88_partition.asm). Dopo Orig, se
   `view8::split`: `root = *(this+0x320)+0x30; p = 0x8ad8e8(root, &SafeString{[0x12f9e28]+0x10, nome})` e
   `0x8ade74(p, visibile)`: 6P → y nascosta, y3l/y3r visibili, x visibile; 8P → y, y4l, y4r, x visibili. Le stringhe
   P_Partition_y3l/y3r/y4l/y4r **non esistono nel main** [VERIFICATO]: vanno nel modulo. [DA VERIFICARE] visibilità di
   default di y3*/y4* nel layout (se fossero visibili il 4P vanilla le mostrerebbe: quindi presumibilmente nascoste).
4. **Pausa** 0x53198c (0x531cd8..0x531d24: nasconde x,y e li riaccende per modo 1/2): lasciare il 4P (cosmetico; il
   layout dedicato `rc_Page_RacePause_SgOunce` è uno step a parte).

### 2.3 Fase H2 – pagine per i locali 4..7
Necessario: 4 RaceView (+4 Front, facoltative: solo "FINISH") in più, legate a viewport fittizi 5..8.
1. **Id**: la tabella ha 136 id tutti nominati (img/pages.txt) e nessuno libero per costruzione. Id mai registrati via
   0x8b3cb0 nel retail (b_page_ids.txt, euristica): 0x00 DebugMenuSeq, 0x02 DebugViewer, 0x12, 0x13, 0x17..0x19,
   0x1d..0x1f, 0x43, 0x4d, 0x5b, 0x5e, 0x64 TrialTitle, ... [DA VERIFICARE A RUNTIME]: in gara dumpare
   `0x8bcc5c()+0x8+id*8` per id 0..0x87 e scegliere 8 id con factory nulla (o di pagine di menu, che il menu
   ri-registra all'ingresso: 0x4048b4).
2. **Registrazione**: trampolino su **0x406870** (prima istruzione 0xd101c3ff): dopo Orig, se `view8::split`, per j=0..split-5:
   `tab[idR_j].factory = 0x40b498; tab[idR_j].owner(+0x448) = this; 0x8b3cb0(this, &idR_j, layer2D_j)` (e lo stesso
   con 0x40b4c4 per Front). `layer2D_j`: 4 Layer2D nuovi (0x5faba8(0x1678, heap, 8) + ctor 0x3c8734 + 0x667a94 allo
   slot 18..21 come 0x3cac64..0x3cae9c, rect = cella 4+j) oppure 10 (2D Main) con RootPane spostato.
3. **Slot della pagina**: HOOK_DEFINE_INLINE a **0x547d5c** (`sub w20,w8,#0x65`, 0x51019514) e **0x548e58**
   (`sub w20,w8,#0x6a`, 0x5101a914): se `w8` è uno degli id presi in prestito → `x20 = 4+j`, altrimenti `x20 = w8-base`.
4. **Viewport della pagina**: REPLACE **0x86eb84** (`int (int slot)`, 2 chiamanti: 0x547df4, 0x548ef0, prima istruzione
   0xf81e0ff3): slot ≤3 → originale, 4..7 → `5+(slot-4)` se split; e **ingrandire race+0x358**: nel trampolino 0x406870
   (o su 0x532818) copiare i 5 puntatori in un buffer del modulo da 9, `+0x354 = 9`, e aggiungere 4 oggetti
   `0x5fa960(0x118)` + `0x54fcc4(obj, 5+j, &view8::extra[j], race)`. La pagina Race li processerà nei suoi cicli
   `i < +0x350` [IPOTESI: i cicli non indicizzano array a 5 con obj+0; da verificare 0x530004/0x530c30/0x530d28].
5. Facoltativo: REPLACE **0x86eb1c** (34 chiamanti, `int (int slot)` → kart): slot 4..7 → `view8::kartOf[slot]` quando
   split, così le funzioni HUD chiamate con lo slot della pagina 4..7 trovano il kart giusto.
Rischio: medio-alto (framework pagine). Ordine consigliato: H1 → test → H2 un locale alla volta (prima 1 pagina extra).

## 3. Camere

### 3.1 Camera giocatore vs "preset" [VERIFICATO]
- 0x1358e40 (+0x38 se modo≠1, +0x50 se modo==1) è letto da 0x7e8b9c/0x7e8c54/0x7e8dc0/0x7e8e60/0x7e925c che scrivono
  in `[obj+0x160]+0x138..+0x154` dell'**oggetto Lakitu** (classe con +0x168 slot vista, +0x169 kart; ctor 0x7e80ec,
  factory 0x110298 = 0x5fa960(0x3d0)): posizione del Lakitu rispetto alla camera, non la camera del giocatore. Con modo 2
  fisso le viste 6P/8P usano il preset 4P: cosmetico.
- FOV/distanze per modo nella chase-cam: 0x118104 (solo modo==1), 0x1199d8 (solo modo==0) (b_cam_mode_fov.asm):
  in modo 2 si usa il ramo standard → nessun intervento.

### 3.2 Chi ha una camera "giocatore" [VERIFICATO salvo indicazione]
- KartCamUnit ctor 0x117de8: a 0x117fb4 `if (Vehicle+0xd2 || Vehicle+0xe7) unit+0xc8 = 0; if (Vehicle+0xe9) unit+0xc8 = 1`
  (b_kartcam_flag.asm). Vehicle+0xd0/+0xd2 sono `tipo==0` / `tipo==1` con tipo = `[x20+0xd54]` (0x16faf0-0x16fb20).
  [IPOTESI forte] tipo = tipo entry (0 umano, 1 CPU) → con 8 umani i kart 4..7 hanno `+0xc8 = 1` e 0x119524 aggiorna
  desc0 ogni frame (come per i kart 0..3). La camera "giocatore" esiste quindi per tutti i kart umani; manca solo il
  **consumatore**: il director ha 4 viewport (0xc8b2c `cmp x21,#4`, LookAtCamera dir+0x5b0+vp*0x60, proiezioni
  dir+0x730+vp*0xc0, tutti con clamp `cmp #4`).
- [DA VERIFICARE A RUNTIME] log per k = kartOf[4..7]: `unit = e0-8` (e = dir+0x3d0+k*0x28), `unit+0xc8`, desc0 pos che
  cambia ogni frame; `[x20+0xd54]` dei kart 4..7 = 0.

### 3.3 Intervento N3: viste extra con la camera reale del locale (proto)
Sostituire in `proto::PerFrame` `KartCam(4+j, cam[j])` con la copia del descrittore, identica a 0xc9568
(b_rlm_director_cam.asm):
```cpp
static void DescCam(int k, uint8_t* cam, uint8_t* proj, uint8_t* layer) {
    uint8_t* d = KartDesc(k);                       /* e[3] && *e[3] ? e[1] : e[0]  (desc1 se il flag +0xca) */
    if (!d) return;
    int t = *(int32_t*)(d + 0x8);
    if (t == 1 || t == 2) {
        memcpy(cam + 0x38, d + 0xc, 12); memcpy(cam + 0x44, d + 0x18, 12); memcpy(cam + 0x50, d + 0x24, 12);
        normalize((float*)(cam + 0x50));                                     /* 0xc95c4..0xc964c */
        Fn<void(*)(void*, float)>(0x61bdd0)(proj, *(float*)(d + 0x30));     /* setFovy, 0xc9664 */
        if (t == 2) { /* 0xc9698: 2 float extra in proj+0xb4/+0xb8 da d+0x34/+0x38 */ }
        Fn<void(*)(void*, void*)>(0x619a08)(cam, cam + 8);                  /* doUpdateMatrix */
    } else if (t == 0) {                                                     /* camera esterna */
        *(void**)(layer + 0x68) = *(void**)(d + 0x10); *(void**)(layer + 0x70) = *(void**)(d + 0x18);
    }
}
/* in PerFrame: int k = view8::split ? view8::kartOf[4 + j] : 4 + j;
   if (unit(k)+0xc8) DescCam(k, cam[j], proj[j], layer[j]); else KartCam(k, cam[j]);   (fallback attuale) */
```
Attenzione: oggi `memcpy(proj[j], m+0x238, 0xc0)` ogni frame copia la proiezione di Sub0: farlo **prima** di setFovy e
poi riapplicare l'aspect della cella (SetRect). Con t==0 ripristinare layer+0x68/+0x70 alle proprie cam/proj quando il
tipo torna 1. Il kart 0 non va più assunto = giocatore 1 (KartCam usa `Pose(0)`): con N1, kartOf[0] è il kart vero.

### 3.4 Altri legami kart↔vista (cosmetici) [VERIFICATO]
- Scene+0xc0[kart] (maschera viewport, 0x7e7bc4..) → KartUnit+0x362 (0x16effc, 0x16f178) → 0x16e184(mask) che la
  converte in maschera di **slot locali** (`1 << entry.slot` per i bit 0..4). Per i kart 4..7 resta 0. [IPOTESI] serve a
  distinguere "il proprio kart" nelle viste (dettaglio/trasparenza). Fix eventuale: trampolino su 0x16e184 (prima
  istruzione 0xa9bd57f6) che, se il kart è kartOf[j≥4], aggiunge il bit j al risultato (verificare prima dove viene
  usato il valore di ritorno).
- Lakitu per vista 0x7e9b04: cicla vp 0..3 (`cmp x25,#4` a 0x7e9c0c) e chiama `vfunc+0xb8(slot)` (0x7e8868 salva lo slot
  in +0x168, letto a 0x7e8608/0x7e86a0/0x7e928c). Per i locali 4..7 nessuna assegnazione: il Lakitu del loro kart non
  sa in quale vista sta (semaforo/inquadratura). Patchare a `#5` aggiungerebbe solo vp4 (inattivo con N1): inutile.
  Eventuale hook dopo 0x7e9b04 che per j≥4 chiama vfunc+0xb8(obj con +0x169==kartOf[j], j) [IPOTESI: i lettori di +0x168
  potrebbero indicizzare array a 4 → verificare prima].

## 4. Altri limiti a 4 nel percorso di gara
| sito | cosa fa [VERIFICATO] | con 8 locali | correzione minima |
|---|---|---|---|
| 0x49c938 (b_49c938_racepage.asm) + 0x49e960 | pagina con +0x1628 = n (s8), +0x162c u8[4] = `n > i` per i 0..3 (0x49e960, unico riempitore); stati 0x49c974 e 0x49cb50: per i 0..3 se presente e `0x8be248(0x8bf158(i), flag)` falso → applet 0x49ea60(n, n) | **inerte**: nessuna scrittura fuori; i pad 4..7 non vengono controllati (staccare il controller 5-8 non apre l'applet); con pad 0..3 non pronto l'applet parte con (8,8), già gestito dagli hook step 4/5 | facoltativo: trampolino su 0x49c938 che, prima di Orig, per i 4..n-1 controlla `0x8be248(0x8bf158(i),1)` (GetPad già esteso a 4..7 dallo step 5 C) e in caso negativo forza lo stesso percorso (chiamata 0x49ea60(n,n)) |
| 0x551b28 / 0x551c58 (b_551b28.asm), chiamante 0x534178 | oggetto per locale: +0x10+i*8 {stato, +0x14}, **4 voci**, +0x30 = raceMgr+0x178 (=8), +0x34; il ciclo i < +0x30 legge raceMgr+0x17c[i] (s32[8] con la mod) e scrive `+0x10 + clamp4(i)*8` (0x551bc4/0x551bec/0x551c0c); 0x551c58 rilegge con lo stesso clamp (0x551d60) | **nessun overflow**, ma i locali 4..7 **sovrascrivono lo stato del locale 0** (aliasing). Logica per locale legata a kart+0x29c / 0x8a1fc0 / 0x852d1c [IPOTESI: stato di fine gara/attesa] → possibile comportamento errato del giocatore 1 a fine gara | [DA VERIFICARE A RUNTIME] watch su obj+0x10; se serve: trampolino che salva/ripristina l'entry 0 e limita +0x30 a 4 (i locali 4..7 esclusi) oppure REPLACE di entrambe con stato in un array del modulo [8] |
| 0x13e59c / 0x13d56c (b_13d56c_audio.asm) | 0x13d56c(audio, w1, &x) salva w1 in +0xec (chiamato dal menu 0x4d9a2c con w22); 0x13ee50 → 0x13e59c(this, +0xec): **indice = w1-1 senza limite** nella tabella statica 0x131f32c {0,1,2,2} (init lazy 0x13e5d8: +0x1c=0,+0x20=1,+0x24=2,+0x28=2), poi fino a 4 oggetti audio (+0xc0 count) con 0x147678 | con P5b (0x4d9898 nop) i 5-8 seguono il ramo Multi: w22 = 4 (0x4d9914) → indice 3 → 2 = come 3P/4P: **inerte**. Senza P5b w22 = 0 → indice -1 → lettura fuori tabella (0x131f328) | nessuna (tenere P5b) |
| raceMgr +0x214 / +0x224 (b_rm214_224.asm) | s32[4] opzioni per locale: getter 0x425888 (`slot>3 → 0`), 0x8653cc(entry) copia +0x214[entry+0x4c] in gp+0x37c e gp+0x6e4 (`cmp #3; b.hi → 0`, 0x865444), setter +0x224 in 0x1197a8 (0x1197c0 `cmp #3; b.hi` salta) | **inerte**: i locali 4..7 ricevono 0 (valore di default) [IPOTESI: assistenze di guida]; nessuna scrittura fuori (l'oggetto da 0x238 B finisce a +0x234) | facoltativo: array ombra [8] nel modulo + trampolino dopo 0x8653cc (per entry con +0x4c 4..7 scrivere gp+0x37c/+0x6e4) e sui setter di +0x214 (da elencare: 1011 accessi `#0x214]/#0x224]` nel main, filtrare per raceMgr) |
| vibrazione | 18 VibrationTarget = 9 npad × 2 nel NpadDevice (clamp 9 a 0x7cae2c, `cmp #0x12` a 0x7cae0c) | già a 8+handheld; il kart vibra sul controller del suo GamePlayer (npad 4..7 dopo step 3/5) [IPOTESI] | nessuna; [DA VERIFICARE A RUNTIME] vibrazione sui pad 5-8 |

## 5. Sintesi: interventi ordinati
### 5.1 Per una gara giocabile (prima di tutto: non crashare)
0. **Crash del collega A** (array a 4 per locale a +0xa0): prerequisito, fuori da questo piano. Tenere presente §1.3
   (RaceView0/Front0 mancanti con la Scene attuale): applicare N1 **prima** di bisezionare il crash.
1. **N1** trampolino 0x7e76b8 (§1.5). Necessario: Scene 4P coerente, locale i → vp i → kart del locale i (i<4),
   pagine RaceView0..3 tutte create, camere director corrette.
2. **N2** proto: attivare le viste extra quando `view8::split` (oltre al test forzato), `Views = split` (o R sempre a 8
   in gara, come oggi), celle: locale i → cella i (Sub i per i<4, layer extra j = locale 4+j).
3. **N3** camere reali dei locali 4..7 (§3.3).
4. Log di verifica (sotto). A questo punto: gara a 8 con 8 viste corrette, HUD solo per i giocatori 1-4.
### 5.2 HUD 6P/8P
5. **H1** (§2.2): REPLACE 0x54798c/0x548d20 (nomi layout), trampolino 0x536a10 (partizioni), posizione 2D Sub0..3.
6. **H2** (§2.3): id presi in prestito, trampolino 0x406870, inline 0x547d5c/0x548e58, REPLACE 0x86eb84 (+0x86eb1c),
   race+0x358 a 9 con 4 oggetti 0x54fcc4 legati a `view8::extra`, 4 Layer2D.
7. Cosmetici: 0x16e184 (maschera kart), Lakitu, effetti per vista (43 lettori via +0xb0), pausa, 0x49c938 pad 4..7,
   0x551b28 aliasing, opzioni +0x214.

### 5.3 Tabella hook/patch
| # | offset | tipo | prima istruzione (orig) | firma | effetto |
|---|---|---|---|---|---|
| N1 | 0x7e76b8 | TRAMPOLINE | 0xd10283ff | `void(uint8_t* scene)` | Scene 4P coerente + view8 (n>4) |
| N3 | (modulo) | – | – | `proto::PerFrame` | DescCam(kartOf[4+j]) |
| H1a | 0x54798c | REPLACE | 0xa9bf7bfd | `const char*(void* page)` | 6P/8P → rc_RaceView_6P_Ml/8P_Ml |
| H1b | 0x548d20 | REPLACE | 0xa9bf7bfd | `const char*(void* page)` | → rc_RaceView_Front_6P_Ml |
| H1c | 0x536a10 | TRAMPOLINE | 0xd10143ff | `void(uint8_t* cmnPage)` | partizioni y3*/y4* |
| H2a | 0x406870 | TRAMPOLINE | 0xd101c3ff | `void(uint8_t* raceScenePages)` | registra pagine extra, race+0x358 a 9 |
| H2b | 0x547d5c | INLINE | 0x51019514 | x8 = id → x20 = slot | slot 4..7 per id presi in prestito |
| H2c | 0x548e58 | INLINE | 0x5101a914 | idem per Front | |
| H2d | 0x86eb84 | REPLACE | 0xf81e0ff3 | `int(int slot)` | slot 4..7 → vp 5..8 |
| H2e | 0x86eb1c | REPLACE (fac.) | 0xf81e0ff3 | `int(int slot)` | slot 4..7 → kartOf |
| C1 | 0x16e184 | TRAMPOLINE (fac.) | 0xa9bd57f6 | `int(void*, uint8_t mask)` | bit locale 4..7 |
Nessuna patch IPS di costanti è necessaria in questo piano (le costanti 4/5 dei 64 lettori restano: la Scene resta 4P).

### 5.4 Rischi per 1-4P
- N1: nullo (solo n>4). N3: solo con split. H1: REPLACE con ramo `split==0 → orig` (stesso codice dell'originale, 3
  stringhe). H1c/H2: attivi solo con split. Il rischio residuo è la regressione dello step 2 (R a 8 viste e modelli a 8
  viste sempre attivi), già noto.

### 5.5 Log di verifica (Eden)
1. `SceneInitRace ->` (DumpScene): `modo=2 slot->vp={0,1,2,3}`, vp[0..3].slot = 0..3, vp4 attivo=0; `view8: n=8
   split=8 kart=...` (attesi 0..7).
2. `render scene "Main" viste 4 -> 8`, `collega 3D Sub4..7 -> indice 4..7` (come step 2).
3. Una volta al secondo per k = kartOf[0..7]: `unit+0xc8`, desc tipo, desc pos; per j: cam extra pos ≈ desc pos.
4. Registrazione pagine: hook di log su 0x8b3cb0 (id, layer): attesi 0x65..0x68 (layer 6..9) e 0x6a..0x6d.
5. Nome layout restituito da H1a/H1b per ciascuna pagina; abort/assert assenti al caricamento del layout 8P.
6. Partizioni: pane trovati (puntatore ≠ 0) e flag visibilità (+0x58 bit0) prima/dopo.
7. Screenshot: celle 1-8 con camere proprie (giocatori 5-8 sterzano e la loro vista segue il proprio kart), HUD 1-4
   nelle prime 4 celle.

## 6. Non determinato staticamente
- Effetto reale dell'aliasing di 0x551b28 sul locale 0 e significato dell'oggetto (pagina 0x5340e0).
- Visibilità di default di P_Partition_y3*/y4* in rc_RaceView_Cmn_00; proiezione dei 2D Sub dopo un cambio di rect.
- Id pagina sicuri da prendere in prestito (dump della tabella 0x8bcc5c in gara).
- Lettori di Lakitu+0x168 e dell'uscita di 0x16e184 (indici fino a 4?).
- Ordine tra SceneInitRace (0x7e76b8) e la creazione del renderer "Main" (0x6a8f10) a ogni ingresso in gara.
