# Step 5b passo 8 – palette a 8 colori in gara e nei risultati (piano statico, main 4.0.1)

Solo analisi statica: `src/` non modificato, nessuna build, Eden non avviato.
Fonti: `../img/main.bin` / `../img/main.ann.asm`; RomFS 4.0.1 `../romfs-tmp/v4/race.sarc`, `../romfs-tmp/chg4/_UI_cmn_ending.sarc`,
`/UI/USen/race.sarc` (contiene `rc_L_Rank_00`), estratto ex novo dal XCI + NSP 4.0.1 con `tools/bktr.py`
(base `secure/2f13e39e….nca@0x17018600`, update `9b5ca186….nca@0x1a23d0`, titlekey dal `.tik` dell'NSP; solo questo file).
Confronto con 3.0.5: `../romfs-tmp/v3/`, `chg3/`.
Dati: `step5b_data (dati non pubblicati)/p8_verify.py` (+ `.txt`, **0 errori**), `p8_romfs_frames.txt` (tabelle colore per frame),
`p8_anims.py` (elenco animazioni/frame di una sarc), `p8_colortab.py` (valore per frame di un'animazione colore).
Etichette: [VERIFICATO] = letto nell'asm o nei layout; [IPOTESI] = da confermare in Eden.

## 0. Risposta breve

- **La RomFS 4.0.1 contiene già i colori di P5-P8 per tutti gli elementi di gara/risultati** [VERIFICATO]: rispetto al 3.0.5
  sei animazioni colore sono cresciute di esattamente 4 frame (+8 per il Rank) e i frame nuovi sono rosso, blu, viola, arancio,
  gli stessi del menu (ColorChange 7..10).
- Il main 4.0.1 però usa **due codifiche** del colore giocatore e nessuna conosce P5-P8:
  - **codice HUD** (`0x420d18`, celle di gara): 0 = 1 giocatore, 1..4 = P1..P4, 5/6 = squadra rossa/blu, **CPU impossibile qui**;
    i frame nuovi sono **7..10** (ItemBox, LapCoin, BtScore, Alarm: frame = codice) e **16..19/20..23** per il Rank (texture
    `tc_RC_Metal5P..8P` / `tc_RC_5P..8P`);
  - **codice righe** (`0x420da0`, classifica, icone della minimappa, classifica finale GP): 0 = 1 giocatore, 1..4 = P1..P4,
    5 = CPU, 6/7 = squadre, 8/9 = CPU delle squadre; i frame nuovi sono **10..13** (Result, TotalResult; mappa = codice-1 = 9..12).
- Entrambe passano per un unico punto: `0x86a708(entry+0x4c)` (tabella {0,1,2,3}, indice ≥ 4 → 4). Oggi `PlayerColorIdxHook`
  (0x86a720) rimappa 4..7 → 0..3.
- **Piano**: 5 hook inline (uno sostituisce `PlayerColorIdxHook`), tutti attivi solo per indice locale 4..7 (le CPU hanno
  `entry+0x4c = -1`): P1-P4 e CPU restano identici bit per bit, e nessun valore esce dal numero di frame del layout 4.0.1.
  Nessuna tabella del main viene indicizzata col colore (tutti i consumatori lo usano solo come frame di animazione) [VERIFICATO].

## 1. Colori di P5-P8 dagli screenshot Switch 2 (regola dell'utente)

`step7_data (dati non pubblicati)/ref_switch2_gara8.png` (gara a 8, P1-P4 sopra, P5-P8 sotto; personaggi Mario, Yoshi, Luigi, Roy / Bowser,
Re Boo, Pipino Piranha, Wiggler) e `ref_switch2_classifica8.png` (classifica a 12) [VERIFICATO a vista]:

| giocatore | gara: anello oggetto (ItemBox) | gara: numero posizione (Rank) | classifica: riga | colore |
|---|---|---|---|---|
| P1 Mario | giallo | "1st" oro (texture metallo P1) | gialla | giallo |
| P2 Yoshi | ciano | "6th" ciano | ciano | ciano |
| P3 Luigi | rosa | "9th" rosa | rosa chiaro | rosa |
| P4 Roy | verde | "12th" verde | verde | verde |
| **P5 Bowser** | (nessun oggetto) | **"4th" rosso** | **rossa** (Bowser, 8°) | **rosso** (255,50,30) |
| **P6 Re Boo** | **blu** | glifo blu/lavanda | **blu** (Re Boo, 7°) | **blu** (10,80,250) |
| **P7 Pipino** | **viola** | **"10th" viola/magenta** | **viola** (Pipino, 10°) | **viola** (140,20,255) |
| **P8 Wiggler** | **arancio** | (coperto) | **arancio** (Wiggler, 9°) | **arancio** (240,65,0) |
| CPU (Kamek, Peach, Wendy, Abitante) | – | – | scure (nero a=180) | frame CPU invariato |

Basi di monete/giri (LapCoin): nere per tutti, P5-P8 compresi. Coincide con i colori del menu (P5 rosso, P6 blu, P7 viola,
P8 arancio) e con i frame 4.0.1 qui sotto.

## 2. Consumatori del colore giocatore: elenco completo [VERIFICATO]

Catena unica [VERIFICATO, `p8_verify.py` §0]: `0x86593c` (`ldr w0,[x0,#0x4c]`) → `b 0x86a708`. `0x86a708` ha un solo
chiamante (0x865940), `0x86593c` due (0x420d88 in `0x420d18`, 0x420dec in `0x420da0`). Nessun puntatore u64 a
0x86593c/0x86a708/0x420d18/0x420da0 nel binario (i puntatori di vtable come 0x513914 invece ci sono): nessuna chiamata indiretta.
Ricerca di altre conversioni "indice locale → colore" (`ldr wN,[x,#0x4c]` seguito da `+1`/`csinc`): solo 0x52082c (pausa, §2.3).

### 2.1 Codice HUD: `0x420d18(entryId)` → c = idx+1
`settings+0x27` (squadre) → 5/6 senza passare da 0x86a708; `raceMgr+0x178 < 2` → 0; altrimenti `0x86a708(+0x4c) + 1`.
Chiamanti: **0x5489e4** (RaceView 0x547f78, una volta per cella), 0x52432c e 0x524d04 (**solo con squadre**: `cbz` su
`settings+0x27` a 0x524324/0x524cfc → sempre 5/6, non toccati).

| # | consumatore (sito) | layout / animazione | uso di c | frame 3.0.5 → 4.0.1 | frame nuovi P5..P8 | c per P5-P8 oggi | necessario |
|---|---|---|---|---|---|---|---|
| H1 | ItemBox `0x512720` (0x5489f8) | `rc_L_ItemBox_00` `G_Color_00` (anello, base numero) | frame = c | 6 → **10** | 7 rosso (255,50,30), 8 blu (10,80,250), 9 viola (140,20,255), 10 arancio (240,65,0) | 5 → rosso squadra | c = 7..10 |
| H2 | Rank `+0x1dc` (0x548a04) → `0x513b20` (bind 0x5139c8, per frame 0x513f78, 0x51413c) | `rc_L_Rank_00` (USen/race.sarc) `G_Rank_00` FLTP `P_Rank_Mpg_00` | c=0: min(pos,3); c 1..4: c-1+8 (pos<3, metallo) / +12; **c>4: c∓1** (varianti squadra) | 15 → **23** (texture 16 → 24) | **16..19 = Metal5P..8P, 20..23 = 5P..8P** | 5 → MetalRed/Red | hook dedicato (H8-C) |
| H3 | LapCoin `0x512fa4` (0x548a14) | `rc_L_LapCoin_00` `G_ColorPtn_00` | frame = c | 6 → **10** | 7..10 neri (come 1..4) | 5 → basi **rosse** | c = 7..10 |
| H4 | BtScore `0x514ecc` (0x548a24) | `rc_L_BtScore_00` `G_ColorPtn_00` (battaglia) | frame = c | 6 → **10** | 7..10 neri | 5 → rosso | c = 7..10 |
| H5 | Alarm_00/01 `0x526048` (0x548a3c, 0x548a60) | `rc_L_BackAlarm_00` `G_Alarm_00` (#8, luce allarme oggetti) | frame = c | 6 → **10** | 7 (255,10,0), 8 (10,80,250), 9 (120,0,255), 10 (240,65,0) | 5 → rosso | c = 7..10 |

Nomi dei pane [VERIFICATO]: stringhe caricate nella stessa funzione 0x547f78 (`L_ItemBox_00` 0x548004, `L_Rank_00`
0x548068, `L_LapCoin_00` 0x548184, `L_BtScore_00` 0x5482fc, `L_Alarm_00/01` 0x548468/0x54850c). Tutte le funzioni impostano
`frame = (float)(c & 0xff)` (ucvtf a 0x512744, 0x512fcc, 0x514f08, 0x52606c) senza tabelle né limiti.

### 2.2 Codice righe: `0x420da0(entryId)` → c = idx+1, CPU = 5
`raw = 0x86a708(+0x4c)`; `c = raw+1`; se `cfg+0x1a40` (squadre): `c = 6/7 + (c==5 ? 2 : 0)`; altrimenti se c==1 e
`raceMgr+0x178 == 1` → 0.

| # | consumatore (sito) | layout / animazione | uso di c | frame 3.0.5 → 4.0.1 | frame nuovi P5..P8 | c per P5-P8 oggi | necessario |
|---|---|---|---|---|---|---|---|
| R1 | riga risultati `0x5205cc` (0x520600; pagine Race_Result, Award e classifica nella pausa) | `rc_L_Result_00` `G_PlayerColorChange_00` (W_BG_00/01, testi) | frame = c (0x520880) | 9 → **13** | **10 (255,50,30), 11 (10,80,250), 12 (140,20,255), 13 (240,65,0)** | 5 → nero CPU | c = 10..13 |
| R2 | icona minimappa `0x50ad24` (0x50b008; il ramo 0x420edc è l'online) | `rc_L_DRC_MapIconChara_00` `G_EdgeColorChange_00` (gruppo 1 slot 0, registrato a 0x50a420) | c==5/8/9 → `+0x114 = 1` (non umano); se c≠5 frame = c - (c>5) | 8 → **12** | **9 (220,0,0), 10 (10,10,200), 11 (120,0,255), 12 (240,65,0)** | 5 → nessun frame, trattata da CPU | c = 10..13 |
| R3 | classifica finale GP `0x50541c` (0x505460) | `ed_L_TotalResult_00` (ending.sarc) `G_ColorPtn_00` slot 0 + `Loop` slot 1 | +0xd0 = c; frame = c (0x50577c); `Loop` (luce barra) solo se c≠5 e **c≤7** (0x5057a8) | 9 → **13** | **10 (255,20,30), 11 (10,50,250), 12 (140,0,255), 13 (240,35,0)** (W_BarBase) | 5 → barra nera, niente Loop | c = 10..13 e Loop anche per 10..13 |

Appartenenza di 0x50ad24 alla classe MapIconChara: [IPOTESI forte] (stesso gruppo 1 / slot 0 = `EdgeColorChange` del ctor
0x50a3d0, formula c-(c>5) coerente con la tabella a 12 frame: squadre 6/7 → 5/6, CPU squadra 8/9 → 7/8).

### 2.3 Colore diretto dall'indice locale (senza 0x86a708)
| # | sito | uso | oggi P5-P8 | necessario |
|---|---|---|---|---|
| P1 | `0x5205cc` con arg4 = 1 (unico chiamante variabile 0x53fdbc, classifica nella pausa): 0x52082c `ldr w8,[x8,#0x4c]`, 0x520830 `cmp w8,#0`, 0x520834 `csinc w20,w20,w8,lt` | frame = l+1 (sovrascrive il colore di R1) | l=4..7 → frame 5..8 (nero/rosso sq./blu sq./rosso sq. semitrasp.) | frame 10..13 |

### 2.4 Esclusi (nessun intervento) [VERIFICATO salvo indicazione]
- 0x52432c / 0x524d04: solo squadre (c = 5/6).
- `0x50efa8` (frame `(slot&1)+2`), i cicli `cmp w8,#3` su `+0x4c` a 0x868fc0/0x86901c/0x86b970 (chiamano 0x8653cc, non colore).
- `rc_L_DRC_MapIconPrison_00` (8 frame, invariato, battaglia Guardie e ladri), `G_TeamColor_*` (2 frame, squadre),
  `rc_L_RivalName_00` (2 frame), `cm_L_CameraPlay_00` `G_Color_00` (14 frame, nuovo, GameChat/camera: mai attivo su Switch 1
  [IPOTESI]; nessuna chiamata a 0x420d18/0x420da0 nel suo percorso).
- Menu (cursori `ColorChange` 7..10, `KPSlotCursor` 5..8) e pausa SgOunce: già trattati negli step 5b/7.

## 3. Mappa indice locale → codice → frame (obiettivo)

| locale (entry+0x4c) | raw 0x86a708 | codice HUD 0x420d18 | frame ItemBox/LapCoin/BtScore/Alarm | frame Rank pos<3 / pos≥3 | codice righe 0x420da0 = frame Result/TotalResult | frame mappa |
|---|---|---|---|---|---|---|
| 0..3 (P1-P4) | 0..3 | 1..4 | 1..4 | 8..11 / 12..15 | 1..4 | 1..4 |
| -1 (CPU) | 4 | (5, ma l'HUD esiste solo per i locali) | – | – | 5 | nessuno (CPU) |
| **4..7 (P5-P8)** | **6..9** (H8-A) | **7..10** | **7..10** | **16..19 / 20..23** (H8-C) | **10..13** (H8-B) | **9..12** |

Tutti i massimi coincidono con il numero di frame 4.0.1 (frame N con N = frameSize è valido: il 3.0.5 usa già il frame 6 di
`G_ColorPtn_00` con frameSize 6 per la squadra blu) [VERIFICATO]. Su un layout 3.0.5 i frame oltre il massimo verrebbero
limitati dal runtime ui2d (nessun accesso fuori limite, solo colore sbagliato) [IPOTESI].

## 4. Piano esatto (hook, parole verificate con `p8_verify.py`, 0 errori)

Condizione comune: solo indice locale 4..7 **e** più di 4 locali (`raceMgr+0x178 > 4`; nel vanilla un locale ha sempre
`+0x4c` 0..3 e le CPU -1, quindi anche senza la seconda condizione nulla cambierebbe; la teniamo come cintura di sicurezza,
e non usiamo `view8::split` perché su Race_Result/Award non è garantito [IPOTESI step 7]).

| id | tipo | sito (prima parola) | registro | azione | effetto |
|---|---|---|---|---|---|
| **H8-A** | INLINE | **0x86a770** `str w8,[x19]` (**0xb9000268**), in 0x86a708; bersaglio anche del `b` a 0x86a72c | W20 = indice locale (0x86a718 `mov w20,w0`), W8 = raw | `if (W20 ∈ [4,8) && Local8()) W8 = W20 + 2` | raw 6..9 → HUD 7..10, righe (con H8-B) 10..13. **Sostituisce `PlayerColorIdxHook` a 0x86a720 (0x7100129f), da non installare più** |
| **H8-B** | INLINE | **0x420df4** `orr w8,wzr,#3` (**0x320007e8**), in 0x420da0, subito dopo 0x420df0 `ldr w20,[sp,#8]` | W20 = raw | `if (W20 ∈ [6,9]) W20 += 3` | 0x420e60 `add w20,w20,#1` → 10..13; squadre: c≠5 → 6/7 (umano) come P1-P4 |
| **H8-C** | INLINE | **0x513ca8** `ucvtf s0,w8` (**0x1e230100**), ramo `c > 4` di 0x513b20 (bersaglio di 0x513c50) | W8 = c, X19 = Rank | `if (W8 ∈ [7,10]) W8 += (*(int16_t*)(X19+0x1d4) < 3) ? 10 : 12` | poi `±1` (0x513cb0..0x513cc0): c+9 = 16..19 (Metal5P..8P) se pos<3, c+13 = 20..23 (5P..8P) |
| **H8-D** | INLINE | **0x520830** `cmp w8,#0` (**0x7100011f**), 0x5205cc ramo pausa | W8 = entry+0x4c | `if (W8 ∈ [4,8) && Local8()) W8 += 5` | `csinc` → frame l+6 = 10..13 (righe della classifica nella pausa) |
| **H8-E** | INLINE | **0x5057a8** `cmp w8,#7` (**0x71001d1f**), 0x50541c classifica finale | W8 = c (+0xd0) | `if (W8 ∈ [10,13]) W8 = 1` | `Loop` della barra anche per P5-P8 (w8 è riletto a 0x5057b0: nessun altro uso) |

Nessun intervento serve su H1, H3, H4, H5, R1, R2 (frame = c o c-1 già giusti con i nuovi codici) né su R3 oltre H8-E.

### 4.1 Schizzo C++ (da integrare in `main.cpp`, gruppo H6b)
```cpp
namespace pal8 {   /* step5b passo 8: palette a 8 colori (step5b_plan_p8.md) */
    static bool Local8() { uint8_t* rm = race8::RaceMgr(); return rm && *(int32_t*)(rm + 0x178) > 4; }
}
/* H8-A: 0x86a770 `str w8,[x19]` (0xb9000268) in 0x86a708(indice locale) -> raw colore. Locali 4..7 -> 6..9
 * (codice HUD 0x420d18 = 7..10: frame P5-P8 di ItemBox/LapCoin/BtScore/Alarm 4.0.1). Sostituisce il vecchio 0x86a720. */
HOOK_DEFINE_INLINE(PlayerColorIdxHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t l = ctx->W[20];
        if (l >= 4 && l < 8 && pal8::Local8()) ctx->W[8] = l + 2;
    }
};
/* H8-B: 0x420df4 `orr w8,wzr,#3` (0x320007e8) in 0x420da0: raw 6..9 -> 9..12 (codice righe 10..13 dopo il +1) */
HOOK_DEFINE_INLINE(RowColorP8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t r = ctx->W[20];
        if (r >= 6 && r <= 9) ctx->W[20] = r + 3;
    }
};
/* H8-C: 0x513ca8 `ucvtf s0,w8` (0x1e230100) in 0x513b20 (Rank, ramo c > 4): c 7..10 -> Metal5P..8P (16..19) / 5P..8P (20..23) */
HOOK_DEFINE_INLINE(RankColorP8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t c = ctx->W[8];
        if (c >= 7 && c <= 10) ctx->W[8] = c + ((*(int16_t*)((uint8_t*)ctx->X[19] + 0x1d4) < 3) ? 10 : 12);
    }
};
/* H8-D: 0x520830 `cmp w8,#0` (0x7100011f) in 0x5205cc (classifica nella pausa): frame = l+1 -> l+6 per i locali 4..7 */
HOOK_DEFINE_INLINE(PauseRowColorP8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t l = ctx->W[8];
        if (l >= 4 && l < 8 && pal8::Local8()) ctx->W[8] = l + 5;
    }
};
/* H8-E: 0x5057a8 `cmp w8,#7` (0x71001d1f) in 0x50541c (ed_L_TotalResult): Loop della barra anche per 10..13 */
HOOK_DEFINE_INLINE(TotalResultLoopP8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t c = ctx->W[8];
        if (c >= 10 && c <= 13) ctx->W[8] = 1;
    }
};
```
Installazione: nella tabella `h6[]` (main.cpp ~3523) sostituire `{0x86a720, 0x7100129f}` con `{0x86a770, 0xb9000268}` e
`PlayerColorIdxHook::InstallAtOffset(0x86a720)` con `InstallAtOffset(0x86a770)`. Gli altri quattro come gruppo a parte
"tutto o niente", con verifica delle parole:
```cpp
static const W p8[] = {{0x420df4, 0x320007e8}, {0x420df0, 0xb9400bf4}, {0x420e60, 0x11000694},
                       {0x513ca8, 0x1e230100}, {0x513c48, 0x7100111f}, {0x513cac, 0x79c3aa68},
                       {0x520830, 0x7100011f}, {0x520834, 0x1a88b694},
                       {0x5057a8, 0x71001d1f}, {0x5057b0, 0xf9403a68}};
/* se tutte ok: RowColorP8Hook(0x420df4), RankColorP8Hook(0x513ca8), PauseRowColorP8Hook(0x520830), TotalResultLoopP8Hook(0x5057a8) */
```
Se il gruppo p8 non si installa, H8-A da solo darebbe: HUD corretto salvo il Rank (codice 7..10 → frame 6..9/8..11 =
Red/Blue/Metal1P..), righe e mappa con frame 7..10 (colori squadra) → **H8-A e il gruppo p8 vanno installati insieme**
(oppure, in caso di parola inattesa, ripiegare sul vecchio 0x86a720 = colori di P1-P4).

### 4.2 Garanzie
- **P1-P4 identici** [VERIFICATO]: H8-A agisce solo con W20 ∈ [4,8); H8-B solo su raw 6..9, H8-C su c 7..10, H8-E su c 10..13,
  valori che il codice vanilla non produce mai (raw ≤ 4; codice HUD ≤ 6; codice righe ≤ 9); H8-D solo con l ∈ [4,8).
- **CPU identiche** [VERIFICATO]: `entry+0x4c = -1` (STRUCTS: "+0x4c indice locale (-1)") → come u32 fuori da [4,8) → raw 4 → 5.
- **Squadre identiche** [VERIFICATO]: 0x420d18 esce prima (5/6); 0x420da0 confronta solo c==5 (CPU) → P5-P8 umani come P1-P4;
  0x52432c/0x524d04 solo squadre.
- **Nessun indice fuori limite** [VERIFICATO]: nessun consumatore usa il colore come indice di array (solo `ucvtf` → frame,
  confronti con 5/7/8/9); i frame massimi (10, 13, 12, 23) sono ≤ frameSize dei layout 4.0.1. Unica tabella sul percorso: quella
  lazy di 0x86a708 (`0x135b354`, 4 voci) letta solo nel ramo `b.lo` (indice < 4): H8-A scrive W8 **dopo** il ramo.
- `0x420d18` con `raceMgr+0x178 < 2` dà 0 prima di 0x86a708: in 1 giocatore nulla cambia.

### 4.3 Da verificare in Eden [IPOTESI]
1. Gara 8 locali: anello oggetto e luce allarme P6/P7/P8 blu/viola/arancio, Rank di P5-P8 con le texture `tc_RC_5P..8P`
   (rosso/blu/viola/arancio come nello screenshot), basi monete/giri nere (non più rosse).
2. Minimappa: bordo icona P5-P8 colorato (frame 9..12) e icone non più trattate da CPU (`+0x114 = 0`).
3. Race_Result / pausa / classifica finale GP: righe P5-P8 rosso/blu/viola/arancio, CPU scure; Loop della barra su P5-P8.
4. Che `raceMgr+0x178` resti > 4 su Race_Result/Award (se no, togliere `Local8()` da H8-A/H8-D: la condizione sull'indice
   basta da sola).
5. Colori delle texture `tc_RC_5P..8P` non decodificati (BNTX): ordine dedotto dai nomi e dallo screenshot.

## 5. Dati
- `step5b_data (dati non pubblicati)/p8_verify.py` / `p8_verify.txt`: 65 parole verificate (catena, consumatori, 5 siti), 0 errori.
- `step5b_data (dati non pubblicati)/p8_romfs_frames.txt`: tabelle per frame di `G_Color_00` (ItemBox), `G_ColorPtn_00` (LapCoin, BtScore,
  TotalResult), `G_Alarm_00`, `G_EdgeColorChange_00`, `G_PlayerColorChange_00`, `G_Rank_00` (texture per frame, base e 4.0.1)
  e diff del numero di frame 3.0.5 → 4.0.1.
- `step5b_data (dati non pubblicati)/p8_anims.py`, `p8_colortab.py`: script (usano `p7_lyt.py`/`p7_flan.py`).
