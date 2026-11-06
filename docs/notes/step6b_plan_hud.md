# Step 6b – HUD celle 5-8, intro/Lakitu, partizioni (piano statico, main 4.0.1)

Analisi statica: offset = main decompresso (`../img/main.bin`), disassemblato `../img/main.ann.asm`; layout da
`romfs-tmp/v4/race.sarc` (4.0.1). Eden non eseguito, `src/` non modificato. Estratti in `step6b_data/h_*`.
Etichette: [VERIFICATO] = letto nell'asm/nei layout; [IPOTESI] = da confermare in Eden.
Complementare a `step6b_plan.md` (altro agente: ordine di disegno, correzione **E1** = id dei segmenti per slot).

## 0. Risposta breve

| problema | causa | correzione | certezza |
|---|---|---|---|
| numeri delle celle 5-8 in **rosso** | colore per giocatore = `0x420d18(entry)` → `0x86593c` → `0x86a708(entry+0x4c)`: tabella `{0,1,2,3}`, **indice ≥ 4 → 4**, +1 → **colore 5 = squadra rossa** (frame 5 di `ColorPtn` in `rc_L_LapCoin_00`: base R=255) | INLINE **0x86a720** (`cmp w20,#4`, 0x7100129f): con split, indice 4..7 → 0..3 | causa [VERIFICATO], effetto visivo [IPOTESI forte] |
| **posizione (rank) assente** | il Rank si mostra solo da `0x5500e4` (aggiornamento per frame dell'oggetto viewport: timer +0x10c → `0x513e24`), che la pagina Race chiama **srotolato a 5** (race+0x358[0..4]) in 0x534de4: gli oggetti 5..8 non vengono mai aggiornati | INLINE **0x5350a8** che chiama 0x5500e4 sugli oggetti 5..n-1 | [VERIFICATO] |
| altri blocchi srotolati a 5 nella pagina Race | 0x53430c: fine gara (`0x551294`/`0x5512c4` and/or), `0x551378`, `0x551914`, `0x5502f0`; 0x53635c (allarme oggetti: solo 0..4 e slot < 4) | 4 INLINE + 1 trampolino (§1.4) | [VERIFICATO] |
| flash delle viste extra prima dell'intro | [IPOTESI forte] il fade nero (`L_Fade_00` della pagina comune, 2D Main slot 10) è disegnato **prima** degli extra (ordine per slot, `step6b_plan.md`) → risolto da E1; inoltre gli extra non seguono lo stato abilitato di 3D Sub0 | E1 + accensione per frame sincronizzata con Sub0 (`+0x78` bit0, `mgr+0xc72`) | parziale |
| camera d'intro solo 1-4 | il director lega i kart solo ai viewport 0..3 (`0xc89cc`, `cmp x21,#4`) e le camere "spinte" (`0xc9074`) hanno clamp vp ≥ 4 → 0; la cella 5-8 copia solo il descrittore del kart | `RelCam`: in pre-partenza copiare la camera **effettiva di Sub0** (matrice) trasportata dal kart di P1 al kart della cella | meccanismo [VERIFICATO], sorgente dell'orbita [IPOTESI] |
| Lakitu/semaforo solo 1-4 | loop 0x7e9b04 solo vp 0..3; **SetSlot 0x10f120 nasconde il Lakitu se slot ≥ 4** (0x10f198) | trampolino 0x7e9b04 + INLINE **0x10f198** | [VERIFICATO]; n. di Lakitu [IPOTESI] |
| partizioni `P_Partition_*` invisibili | pagina comune sul **2D Main (slot 10)**, disegnato prima degli extra; i pane sono a posto (genitore visibile, alpha 255, nessuna animazione li tocca, posizioni giuste per 6P/8P) | **si risolve con E1** (stesso problema del punto 1 di `step6b_plan.md`) | [VERIFICATO] layout; [IPOTESI] resto |

## 1. HUD delle celle 5-8 (punto 2)

### 1.1 Catena del colore (rosso) [VERIFICATO] — `step6b_data/h_color_chain.asm`
```
0x547f78(page, obj0xc0, vpObj)            (RaceView onCreate 0x547d20 → 0x547ed4)
  [sp+0x2c] = vpObj+0xf8 = Scene-entry+0x10 = id entry del kart (per gli extra: hud8::extraVp[j].locSlot = 0x86e8e4(d))
  0x5489e4: w21 = 0x420d18(entryId)
     0x420d18: se settings+0x27 (squadre) → 5/6 secondo entry+0x228; se raceMgr+0x178 < 2 → 0;
               altrimenti 0x86593c(entry) (+1)
     0x86593c: w0 = entry+0x4c (indice locale) → b 0x86a708
     0x86a708: cmp w20,#4 ; b.lo tabella ; altrimenti 4         ← 0x86a720 / 0x86a724
               tabella lazy 0x135b354 = {0,1,2,3}
  → colore = 1..4 per P1..P4, **5 per gli indici locali 4..7** (5 = rosso squadra, 6 = blu squadra)
  0x5489f8  0x512720(ItemBox, colore)
  0x548a04  Rank+0x1dc = colore      (0x513c24: 1..4 → frame colore-1+8/12; >4 → frame 5±1 = varianti squadra)
  0x548a14  0x512fa4(LapCoin, colore)  → frame del gruppo animazione 4 (ColorPtn/TexPtn) = colore
  0x548a24  0x514ecc(…, colore); 0x548a3c/0x548a60 0x526048(…, colore)
```
Layout `rc_L_LapCoin_00`, animazione `ColorPtn` (10 frame): `W_CoinBase_00`/`W_FlagBase_00` hanno colore vertice
nero nei frame 0..4, **R=255 al frame 5**, (0,50,255) al frame 6 (`step6b_data/h_layouts.txt`) → basi di monete e
giri rosse. 0x86a708 ha un solo chiamante (0x865940); 0x86593c ne ha due: 0x420d18 (HUD per vista: 0x5489e4,
0x52432c, 0x524d04) e 0x420da0 (0x505460, 0x50b008, 0x520600: altri elementi con colore giocatore, dove 5 = "non
locale" [IPOTESI: icone mappa/nomi]). `entry+0x4c` vale 4..7 per i locali 4..7 con la mod (CompactLocalsHook usa
`e+0x4c <= 7`).

Correzione H6b-C [VERIFICATO sito/parola; effetto IPOTESI forte]:
```cpp
/* 0x86a720 `cmp w20,#0x4` (0x7100129f), dentro 0x86a708(indiceLocale) -> colore-1. Con split gli indici 4..7
   prendono i colori di P1..P4 (stesso aspetto delle celle 1-4; ColorPtn frame 1..4 identici, Rank frame 8..15). */
HOOK_DEFINE_INLINE(PlayerColorIdxHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t l = ctx->W[20];
        if (view8::split && l >= 4 && l < 8) ctx->W[20] = l - 4;
    }
};
/* install: PlayerColorIdxHook::InstallAtOffset(0x86a720); */
```
Variante: `ctx->W[20] = 0` per tutti i 4..7 se si preferisce un colore unico. Effetto collaterale voluto: anche
0x420da0 tratta i locali 5-8 come locali (colore ≠ 5).

`0x50efa8(obj, slot)` [VERIFICATO, nessun intervento]: imposta il frame di un'animazione a `(slot & 1) + 2` (o
`slot & 1` se settings+8==3 e +0xc==3) → per slot 4..7 identico a 0..3.

### 1.2 Catena della posizione (L_Rank_00) [VERIFICATO] — `h_rank_5134d4.asm`, `h_rank_perframe.asm`
```
0x547f78: w20 = vpObj+0xf4 (kart = Scene-entry+0x14 = view8::kartOf[i]; per gli extra hud8::extraVp[j].kart)
  0x5480b4 0x50e824(page, root, "L_Rank_00", &0, &kart) → oggetto Rank (0x1e8 B, vt [0x1309900]), +0x1d0 = kart; page+0x1f8
  0x548a78 vpObj+0x38 = Rank, +0x30 = LapCoin, +0x28 = ItemBox, 0x548ae0 vpObj+0x110 = 1 (attivo)
Rank 0x513914 (bind): kartObj = raceSys(0x7e7434)->[0x190+4*8]->+0x218->+0x68[kart] → +0x198;
  posizione = kartObj+0x40 (u16) → +0x1d4/+0x1d6; 0x513ecc per frame segue +0x40.
Visibilità: 0x513e24 = mostra (In, +0x1d8 = 1), 0x513e78 = nasconde.
  0x551190(vpObj) (arma: +0x10c = 360)   ← ciclo CONTATO su race+0x350 a 0x534418..0x534450 (extra inclusi)
  0x5500e4(vpObj) (per frame): se +0x20 e +0x110; se !+0x111: +0x10c-- e a 0 → 0x513e24(+0x38)  ← 0x550224
  0x5500e4 è chiamato SOLO in 0x534de4 (vt per frame della pagina Race), srotolato:
     0x535044 [0], 0x535064 [1], 0x535084 [2], 0x5350a4 [3], 0x5350bc [4]     (nessun ciclo su +0x350)
```
Quindi il timer degli extra resta a 360 e il Rank non compare. Né 0x86eb1c né raceMgr+0x1cc sono nella catena
(raceMgr+0x1cc serve solo a calcolare l'id entry `0x86e8e4(d)` messo in extraVp.locSlot).
Altre funzioni per frame dentro 0x5500e4 (0x5502f0 se kart+0x3c bit0, 0x550698, 0x550844, 0x5510f8) erano
anch'esse saltate per gli extra.

### 1.3 Correzione H6b-R (per frame degli oggetti 5..8) [VERIFICATO]
`0x5350a8: ldr w8,[x19,#0x350]` (0xb9435268), raggiunto solo dopo la chiamata per l'oggetto [3] (nessun salto vi
punta); x19 = pagina Race. Il ramo che salta tutto (stato +0x120 == 3 e +0x12c == 0, 0x53501c) non passa di qui.
```cpp
namespace hud8 {
    static void EachExtraObj(uint8_t* race, void (*fn)(uint8_t*)) {      /* oggetti viewport 5..n-1 (race+0x358 a 9) */
        if (!view8::split || !race || *(int32_t*)(race + 0x354) != 9) return;
        int n = *(int32_t*)(race + 0x350); uint8_t** v = *(uint8_t***)(race + 0x358);
        for (int i = 5; i < n && i < 9; i++) if (v[i]) fn(v[i]);
    }
}
HOOK_DEFINE_INLINE(RaceVpFrameHook) {                      /* 0x5350a8 in 0x534de4: dopo 0x5500e4(obj[3]) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        hud8::EachExtraObj((uint8_t*)ctx->X[19], [](uint8_t* o) { mk8dx::Fn<void (*)(uint8_t*)>(0x5500e4)(o); });
    }
};
```

### 1.4 Gli altri blocchi srotolati a 5 (stessa classe) [VERIFICATO] — `h_race_unrolled5.asm`, `h_53635c_alarm.asm`
Tutti in 0x53430c (vt calc/stati della pagina Race, x19 = pagina). Siti d'aggancio verificati: parola originale,
nessun salto entrante salvo dove indicato, istruzione non PC-relativa.

| sito INLINE | parola | dove | cosa fare per gli oggetti 5..n-1 |
|---|---|---|---|
| **0x534848** | 0xf9417668 `ldr x8,[x19,#0x2e8]` | dopo le 5 riduzioni (unico salto entrante 0x5347fc, stesso blocco) | se `o+0x110`: `W22 \|= o[0x111]!=0; W20 &= 0x551294(o); W21 \|= 0x5512c4(o)` (0x551294 = "arrivato da >179 frame" o inattivo; 0x5512c4 = LapCoin giro corrente == giri totali). **Senza questo la gara può chiudersi quando 1-4 hanno finito** [IPOTESI sull'effetto] |
| **0x53467c** | 0x91088261 `add x1,x19,#0x220` | dopo 5× `0x551378` (nasconde ItemBox/LapCoin/Rank/…, poi transizione 0x8ba5fc) | `0x551378(o)` |
| **0x5345e0** | 0xf941ae68 `ldr x8,[x19,#0x358]` | prima dell'indice 4 di 5× (`str wzr,[o,#0x108]; 0x551914(o)`) | `*(int32_t*)(o+0x108)=0; 0x551914(o)` (0x5345f0 è un `b`, evitato) |
| **0x5349a8** | 0xf9401108 `ldr x8,[x8,#0x20]` | fra indice 3 e 4 di 5× `0x5502f0` (salto entrante 0x534990, stesso blocco) | `if (!o[0x111]) 0x5502f0(o)`; x8 non va toccato |
| (battaglia) 0x534bb4 / 0x534ccc | 0xf81e0ff3 | 5× 0x551584 / 0x551594, solo settings+8==3 && +0xc==2 | facoltativo: trampolino, dopo Orig stesse condizioni e chiamata sugli extra con +0x110 |

Esempio (gli altri identici nello schema):
```cpp
HOOK_DEFINE_INLINE(RaceFinishReduceHook) {                 /* 0x534848 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* race = (uint8_t*)ctx->X[19];
        if (!view8::split || *(int32_t*)(race + 0x354) != 9) return;
        int n = *(int32_t*)(race + 0x350); uint8_t** v = *(uint8_t***)(race + 0x358);
        for (int i = 5; i < n && i < 9; i++) {
            uint8_t* o = v[i]; if (!o || !o[0x110]) continue;
            ctx->W[22] |= o[0x111] != 0;
            ctx->W[20] &= mk8dx::Fn<bool (*)(uint8_t*)>(0x551294)(o) ? 1 : 0;
            ctx->W[21] |= mk8dx::Fn<bool (*)(uint8_t*)>(0x5512c4)(o) ? 1 : 0;
        }
    }
};
```
**Allarme oggetti in arrivo** (0x53635c(race, which 0/1, kart), 6 chiamanti 0x5404fc..0x540b84): cerca il kart solo
in race+0x358[0..4] e poi `slot < 4` (0x5363cc `cmp w8,#4; b.hs`) → pagina 0x65+slot → 0x547aec(page, which, kart)
(parte page+0x1e0[which], `L_Alarm_00/01` [IPOTESI]). Trampolino (prima istruzione 0xf81d0ff5
`str x21,[sp,#-0x30]!`):
```cpp
HOOK_DEFINE_TRAMPOLINE(RaceAlarmHook) {                    /* 0x53635c(race, which, kart) */
    static void Callback(uint8_t* race, int32_t which, int32_t kart) {
        Orig(race, which, kart);
        if (!view8::split || kart < 0 || *(int32_t*)(race + 0x354) != 9) return;
        int n = *(int32_t*)(race + 0x350); uint8_t** v = *(uint8_t***)(race + 0x358);
        for (int i = 5; i < n && i < 9; i++) {
            if (!v[i] || *(int32_t*)(v[i] + 0xf4) != kart) continue;
            int s = *(int32_t*)(v[i] + 0x100); if (s < 4 || s > 7) return;
            int id = hud8::kIds[s - 4];
            uint8_t* pg = mk8dx::Fn<uint8_t* (*)(int*)>(0x8bf0ec)(&id);
            if (pg) mk8dx::Fn<void (*)(uint8_t*, int, int)>(0x547aec)(pg, which, kart);   /* le pagine 4..7 sono RaceView (stessa fabbrica) */
            return;
        }
    }
};
```
Nota: con `kart == -1` l'originale trova il vp4 (inattivo, kart -1) e si ferma su `slot < 4`: nessun effetto.

### 1.5 Altri usi dello slot 4..7 nelle pagine extra
- 0x54fea0 [VERIFICATO]: vpObj+0x48 = effetto `WinLogo`/`FinishLogo` ("2DLayout") con id **0x34+slot**, salvato come
  u8 in +0xfa dell'emitter (0x485df4). Anche il Lakitu usa un id per vista (0x16+slot, §2.3): sono gruppi di
  effetti per vista 0..3; 0x38..0x3b per gli extra probabilmente non esistono [IPOTESI] → logo di arrivo forse
  assente/errato nelle celle 5-8. Da osservare a fine gara; eventuale correzione: INLINE su 0x54fea0
  (`add w20,w8,#0x34`, 0x1100d114) per usare 0x2c (valore "tutte le viste" del ramo 0x54fea8).
- 0x547d5c / 0x547df4 (slot, vp) già gestiti; 0x53635c §1.4; nessun altro `cmp #4` sullo slot in 0x547f78
  (i `cmp #3/#4` lì riguardano settings+8/+0xc).

## 2. Intro, accensione degli extra, Lakitu (punto 3)

### 2.1 (a) Accensione degli extra [VERIFICATO salvo indicazione] — `h_sub_enable.asm`
- 3D Sub0..3: abilitati da **0x3cd194(mgr, …)** (chiamato da RlmSync 0x3cbd94 a 0x3cbfa8 e da 0x3cc86c):
  `enable(sub[i]) = mgr+0x9d4+i*0x20 (flag slot, scritto da RlmSync 0x3cbf4c dal tipo dell'entry Scene)
  && !mgr+0xc72` (0x3cd264..0x3cd278); **mgr+0xc72** è messo a 1 dall'evento 2 di 0x3d58e4 (che spegne anche tutti i
  layer con 0x3cd104) e azzerato dall'evento 3.
- 2D Sub: 0x3cd008 (lista mgr+0x160, flag mgr+0xbe9).
- Il proto accende gli extra quando `split && sceneType==3 && mgr+0xbd8>=2 && attached` e non li riallinea più.
- Causa probabile del flash [IPOTESI forte]: il fade nero della gara è `L_Fade_00` (N_All_01) dentro
  `rc_RaceView_Cmn_00`, pagina Race sul **2D Main slot 10** (0x406b1c-0x406b30 `mov w2,#0xa`); per
  `step6b_plan.md` la GPU esegue i segmenti per id di slot, quindi gli extra (14..17) e i 2D extra (18..21) escono
  **sopra** il fade e si vedono mentre il resto è nero. **E1 lo risolve.**
- In più (economico, robusto): accensione per frame sincronizzata con Sub0 / 2D Sub0. In `proto::PerFrame`, al posto
  del blocco `if (want != active) {...}` per gli enable:
```cpp
uint8_t** sub = *reinterpret_cast<uint8_t***>(m + 0x128);
uint8_t** t2 = ls ? *(uint8_t***)(ls + 0x48) : nullptr;
bool on3 = sub && sub[0] && (sub[0][0x78] & 1) && !m[0xc72];        /* stato effettivo di 3D Sub0 (0x3cd264) */
bool on2 = t2 && t2[6] && (t2[6][0x78] & 1);                          /* 2D Sub0 (slot 6) */
for (int j = 0; j < Extra; j++) {                                     /* 0x6651a8 non fa nulla se il bit non cambia */
    mk8dx::Fn<EnableFn>(0x6651a8)(layer[j], want && on3 && j < Used());
    if (hud8::layer2d[j]) mk8dx::Fn<EnableFn>(0x6651a8)(hud8::layer2d[j], want && on2 && j < Used());
}
```
  (lasciare `active = want` per il resto della logica). Copre i casi in cui il gioco spegne i Sub (evento 2,
  slot non attivi); non copre il fade (serve E1).

### 2.2 (b) Camera d'intro [meccanismo VERIFICATO, sorgente dell'orbita IPOTESI] — `h_director_cam.asm`
Director (0xb00 B, [0xc7a48()]+0x1f8):
```
pila per viewport vp 0..3 = dir+0x70+vp*0xd0: +0x78 testa (nodo-0x10: [nodo] = descrittore, +8 priorità, +0xc tipo nodo),
  +0x80 count, +0x88 nodi liberi
0xc89cc (bind, chiamato da 0xc8830, 0xcedb0, 0x13ef00 al cambio retrovisore, 0x3d763c): per vp 0..3 (cmp x21,#4 a 0xc8b2c)
  k = 0xc8f48(dir, vp) (= Scene vp.kart, clamp 5); src = (*e[3]) ? e[1] : e[0] (e = dir+0x3d0+k*0x28);
  nodo priorità 10, tipo 1; dir+0x3b0[vp] = nodo
0xc9074(dir, vp, desc, &tipo, prio) PUSH: clamp vp>=4 -> 0. Chiamanti: 0xce318 (classe vt 0x11b8d50, obj+8 = vp, tipo 2,
  prio 20), 0x147f90 (vp 4 -> 0, tipo 3, prio 30), 0x3f056c (vp 0), 0x502f68 (vp 0, tipo 3, prio 30); 0xc8e68 = REMOVE
0xc8b60 (update): per vp 0..3 testa → 0xc8d1c: tipo desc 0 = camera esterna (dir+0x60[vp] = d+0x10, dir+0x68[vp] = d+0x18),
  1 = 0xc9568 (pos/at/up/fovy → dir+0x5b0+vp*0x60, dir+0x730+vp*0xc0), 2 = 0xc9698
KartCamUnit (0x1184e4 → 0xc8f24): e = {unit+8 desc0 (camera normale), unit+0x40 desc1, unit+0x78, &unit+0xca};
  unit+0xca = retrovisore (0x1190ec da Vehicle+0x171 bit2; suoni 0x54/0x55) → desc1 è la camera all'indietro, NON l'intro.
```
`proto::DescCam(kartOf[4+j])` riproduce solo il nodo "tipo 1" del bind. Se nella cella 1 durante l'intro vince un
nodo di priorità maggiore (tipo 2/3, spinto con vp clamp a 0..3) oppure l'orbita è calcolata solo per i kart con
vista, la cella 5-8 resta sulla camera fissa (desc0 col kart fermo) — esattamente il sintomo.
Nota: il fallback `KartCam` usa `Pose(0)`/`KartDesc(0)` (kart 0), non `kartOf[0]`: da correggere comunque.

Correzione H6b-I (indipendente dalla sorgente): se la camera effettivamente usata da 3D Sub0 non è quella del kart
di P1, le celle extra copiano **la camera di Sub0 trasportata** dal kart `kartOf[0]` al kart della cella (stessa
fase d'orbita). La camera di Sub0 è `sub[0]+0x68` (0x3cc29c: `mgr+0x958[vp]` o `mgr+0x1d8`), con matrice vista a
+0x8 (Matrix34f) [VERIFICATO STRUCTS]; si ricava eye/at/up dalla matrice, così vale anche per camere non LookAt.
```cpp
static bool RelCam(int k, uint8_t* c, uint8_t* pj, uint8_t* sub0) {
    uint8_t* c0 = *(uint8_t**)(sub0 + 0x68); uint8_t* p0 = *(uint8_t**)(sub0 + 0x70);
    float* P0 = Pose(view8::kartOf[0]); float* Pk = Pose(k); uint8_t* d0 = KartDesc(view8::kartOf[0]);
    if (!c0 || !P0 || !Pk) return false;
    const float* M = (const float*)(c0 + 0x8);                 /* righe: X, Y, Z camera; colonna 3 = traslazione */
    float eye[3], at[3], up[3];
    for (int i = 0; i < 3; i++) eye[i] = -(M[0 + i] * M[3] + M[4 + i] * M[7] + M[8 + i] * M[11]);
    for (int i = 0; i < 3; i++) { at[i] = eye[i] - M[8 + i]; up[i] = M[4 + i]; }
    if (d0 && *(int32_t*)(d0 + 0x8) != 0) {                    /* stessa camera del kart di P1 → nessuna intro in corso */
        float dx = eye[0] - ((float*)(d0 + 0xc))[0], dy = eye[1] - ((float*)(d0 + 0xc))[1], dz = eye[2] - ((float*)(d0 + 0xc))[2];
        if (dx * dx + dy * dy + dz * dz < 1.0f) return false;
    }
    /* come KartCam: coordinate nel sistema di P0 (assi X,U,F = P[0..8], T = P[9..11]) e riapplicate a Pk */
    float le[3], la[3], lu[3], ve[3], va[3];
    for (int i = 0; i < 3; i++) { ve[i] = eye[i] - P0[9 + i]; va[i] = at[i] - P0[9 + i]; }
    for (int a = 0; a < 3; a++) { le[a] = Dot(ve, P0 + 3 * a); la[a] = Dot(va, P0 + 3 * a); lu[a] = Dot(up, P0 + 3 * a); }
    float* E = (float*)(c + 0x38); float* A = (float*)(c + 0x44); float* U = (float*)(c + 0x50);
    for (int i = 0; i < 3; i++) {
        E[i] = Pk[9 + i]; A[i] = Pk[9 + i]; U[i] = 0;
        for (int a = 0; a < 3; a++) { E[i] += Pk[3 * a + i] * le[a]; A[i] += Pk[3 * a + i] * la[a]; U[i] += Pk[3 * a + i] * lu[a]; }
    }
    if (p0) mk8dx::Fn<void (*)(void*, float)>(0x61bdd0)(pj, *(float*)(p0 + 0xa0));   /* fovy di Sub0 [IPOTESI: +0xa0 come Layer2D] */
    mk8dx::Fn<UpdMtxFn>(0x619a08)(c, c + 8);
    return true;
}
/* PerFrame, per la cella 4+j:  if (!RelCam(k, cam[j], proj[j], sub[0])) { if (!DescCam(k, cam[j], proj[j])) KartCam(k, cam[j]); }
   (la proiezione va ricopiata prima e l'aspect riapplicato da SetRect, come oggi) */
```
Rischio: con il retrovisore di P1 attivo, `KartDesc` sceglie già desc1 → nessun falso positivo; con camere
"esterne" a schermo intero (flyover del percorso) gli extra sono comunque spenti (modo < 2) [IPOTESI].
Log di verifica (prima di scrivere RelCam): ogni 30 frame in pre-partenza, per vp 0:
`head = *(uint8_t**)(dir+0x78)`, `node = head-0x10`, `[node]`, `node+8` (prio), `node+0xc` (tipo nodo), tipo del
descrittore `[[node]+8]`, confronto con `KartDesc(kartOf[0])` e con `KartDesc(kartOf[4])`; più `unit+0xc8` e desc0
pos dei kart 4..7. Se la testa resta tipo nodo 1 = desc0 del kart di P1 che orbita mentre desc0 del kart 4 è fermo,
l'orbita è nella KartCamUnit e dipende dalla vista: allora RelCam (che confronta con d0) non scatta e va usata la
regola "pre-partenza" (es. stato Lakitu di kartOf[0] `+0x158 < 7`, §2.3) [IPOTESI].

### 2.3 (b) Lakitu e semaforo [VERIFICATO salvo indicazione] — `h_lakitu.asm`
```
gestore Lakitu (x0 di 0x7e9b04, = [..+0x248], chiamato da 0x86cb5c): +0x38 count, +0x40 ptr[]
  oggetto (fabbrica 0x110298, 0x3d0 B, ctor 0x7e80ec; vt [0x12fc000] = 0x11b9768): +0x168 slot vista, +0x169 kart
0x7e9a68(mgr, kart) = Lakitu con +0x169 == kart (o NULL)
0x7e9b04: per ogni Lakitu vt+0xb0 (reset 0x10f1e0); poi per vp 0..3 di Scene (cmp x25,#4 a 0x7e9c0c) con tipo==1:
          per ogni Lakitu con +0x169 == vp.kart → vt+0xb8(lak, vp.slot)
vt+0xb8 = 0x10f120 (override di 0x7e8868):
   0x7e8868: +0x168 = slot
   0x68fb7c(lak+8 (modello), 1<<slot)          maschera viste del modello (u16 per sotto-modello, +0xc)
   effetti [lak+0xb0]+0x120: +0xfa = slot<=3 ? 0x16+slot : 0 ;  +0xf8 = slot<=3 ? 1<<slot : 0xffff
   0x10f194: ldrb w8,[x19,#0x168]; 0x10f198: cmp w8,#4; b.lo ret; else +0xa4 &= ~2; vt+0x50(lak, 0)  ← NASCONDE
Altri lettori di +0x168: 0x7e8608/0x7e86a0 (ri-applicano vt+0xb8), 0x7e928c (+0x16e = 300+slot: frame d'uscita,
  innocuo), 0x10d0a0 (vt+0xd0 per frame: Scene+0xb0[slot, clamp <4 → 0] → vp di P1 → pila del director: nasconde se
  la testa è tipo nodo 2), 0x10d20c/0x10f5c8/0x10f920 (0x690c6c(modello, on, slot) per vista → 0x6dc7d8)
```
Correzione H6b-L:
```cpp
HOOK_DEFINE_INLINE(LakituHideGateHook) {                   /* 0x10f198 `cmp w8,#0x4` (0x7100111f) in SetSlot 0x10f120 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (view8::split && ctx->W[8] >= 4 && ctx->W[8] < 8) ctx->W[8] = 0;   /* b.lo: nessun nascondi per le viste 5..8 */
    }
};
HOOK_DEFINE_TRAMPOLINE(LakituAssignHook) {                 /* 0x7e9b04(mgr), prima istruzione 0xa9ba6ffc */
    static void Callback(uint8_t* mgr) {
        Orig(mgr);
        if (!view8::split) return;
        for (int j = 0; j < view8::split - 4 && j < 4; j++) {
            uint8_t* lak = mk8dx::Fn<uint8_t* (*)(uint8_t*, int)>(0x7e9a68)(mgr, view8::kartOf[4 + j]);
            if (!lak) { Logging.Log("[mk8dx-8p] L: nessun Lakitu per il kart %d (count %d)", view8::kartOf[4 + j], *(int32_t*)(mgr + 0x38)); continue; }
            auto setSlot = reinterpret_cast<void (*)(uint8_t*, int)>(*(uintptr_t*)(*(uintptr_t*)lak + 0xb8));
            setSlot(lak, 4 + j);                                /* maschera vista 1<<(4+j) = vista R dell'extra j */
        }
    }
};
```
Condizioni/rischi: (1) quanti Lakitu esistono (`mgr+0x38`, uno per kart o solo per i locali?) [IPOTESI]: se
0x7e9a68 restituisce NULL per i kart 4..7 servirebbe crearli con la fabbrica 0x110298 (fuori da questo piano);
(2) la maschera `1<<(4+j)` presuppone che il layer extra j sia la vista R 4+j (log "proto: collega 3D SubN … indice")
e che il modello del Lakitu sia stato esteso a 8 viste da ModelCreateHook (0x690c6c/0x6dc7d8 indicizzano per vista);
(3) effetti del Lakitu negli extra con gruppo 0 / maschera 0xffff (fallback del gioco: visibili in tutte le viste)
[VERIFICATO]; (4) la visibilità per frame (0x10cfc0) guarda la vista di P1 (clamp) [VERIFICATO].

## 3. Partizioni P_Partition_* (punto 4)
[VERIFICATO]
- Pagina Race 0x69 (`rc_RaceView_Cmn_00`) registrata sul layer **10 = 2D Main** (0x406b1c `mov w20,#0x69`,
  0x406b30 `mov w2,#0xa`, 0x406b40 fabbrica). Le partizioni sono pane `pic1` figli di `N_All_00` (visibile, alpha 255),
  a schermo intero 1280×720: `x` 1280×2 a y=0, `y` 2×720 a x=0, `y3l/y3r` a x=∓213 (6P: colonne 426,7), `y4l/y4r` a
  x=∓320 (8P: colonne 320), tutte alpha 255; y3*/y4* invisibili di default (`step6b_data/h_layouts.txt`).
- Nessuna animazione le tocca (l'unica che le nomina nel gruppo è `ViewKind`, con chiavi solo su L_RaceNum_00,
  N_BtInfoView_00, W_Check_00); 0x536a88 le nasconde solo in modo 0/1 alla creazione; 0x53198c (onCreate di una pagina
  parallela, pausa) nasconde x/y e le riaccende per modo 1/2, mai y3*/y4*.
- Quindi `hud8::ApplyPartitions` fa la cosa giusta e le posizioni non vanno ritoccate. L'invisibilità è la stessa
  del punto 1 di `step6b_plan.md`: il 2D Main (slot 10) esce prima degli extra (14..21) → **si risolve con E1**
  [IPOTESI forte]. Se dopo E1 mancassero anche le metà superiori (dove 2D Main è già sopra Sub0..3): controllare nel log
  che `hud8::cmnPage` sia l'istanza della pagina 0x69 (`0x8bf0ec(&0x69)`) — 0x536a10 e 0x53198c scrivono entrambe
  `page+0x320` — e che 0x8ad8e8 restituisca pane non nulli.

## 4. Riepilogo hook/patch

| id | tipo | offset | parola/prima istruzione | scopo | certezza |
|---|---|---|---|---|---|
| H6b-C | INLINE | 0x86a720 | 0x7100129f `cmp w20,#4` | colore P5-8 = P1-4 | VERIFICATO |
| H6b-R | INLINE | 0x5350a8 | 0xb9435268 | 0x5500e4 per oggetti 5..8 (Rank) | VERIFICATO |
| H6b-F1 | INLINE | 0x534848 | 0xf9417668 | riduzioni fine gara | VERIFICATO (effetto IPOTESI) |
| H6b-F2 | INLINE | 0x53467c | 0x91088261 | 0x551378 extra | VERIFICATO |
| H6b-F3 | INLINE | 0x5345e0 | 0xf941ae68 | +0x108=0, 0x551914 extra | VERIFICATO |
| H6b-F4 | INLINE | 0x5349a8 | 0xf9401108 | 0x5502f0 extra | VERIFICATO |
| H6b-A | TRAMPOLINE | 0x53635c | 0xf81d0ff5 | allarme oggetti 5-8 | VERIFICATO |
| H6b-E | modulo | proto::PerFrame | – | enable extra = stato Sub0 | VERIFICATO (flash: serve E1) |
| H6b-I | modulo | proto::PerFrame | – | RelCam durante l'intro | IPOTESI (log prima) |
| H6b-L1 | INLINE | 0x10f198 | 0x7100111f `cmp w8,#4` | non nascondere Lakitu slot 4..7 | VERIFICATO |
| H6b-L2 | TRAMPOLINE | 0x7e9b04 | 0xa9ba6ffc | SetSlot(4+j) ai Lakitu dei kart 4..7 | VERIFICATO (n. Lakitu IPOTESI) |
| (opz.) | INLINE | 0x54fea0 | 0x1100d114 `add w20,w8,#0x34` | id effetto FinishLogo per slot ≥ 4 | IPOTESI |

Ordine consigliato: H6b-C + H6b-R (una build, effetto immediato su colori e posizione) → F1..F4 + A → E1 (altro piano)
→ H6b-E → L1+L2 → log camera → H6b-I. Tutti gli interventi sono inerti con `view8::split == 0` (1-4P invariati).
