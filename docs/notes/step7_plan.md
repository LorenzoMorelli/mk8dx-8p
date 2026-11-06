# Step 7 – pausa a 8, risultati/classifica a 8, helper DEBUG (piano statico, main 4.0.1)

Analisi statica: offset = main decompresso (`../img/main.bin`), disassemblato `../img/main.ann.asm`; layout da
`../romfs-tmp/v4/race.sarc` e `common.sarc` (4.0.1). Eden non eseguito, `src/` non modificato.
Estratti in `step7_data (dati non pubblicati)/` (`p_*` pausa, `r_*` risultati, `d_*` debug, `p_layouts_pause.txt` layout/animazioni).
Etichette: [VERIFICATO] = letto nell'asm/nei layout (parole lette da main.bin); [IPOTESI] = da confermare in Eden.
Stile degli hook come `step6b_plan_hud.md` (`mk8dx::Fn<...>(offset)`, verifica della parola originale prima di installare).

## 0. Risposta breve

| # | problema | causa | correzione | certezza |
|---|---|---|---|---|
| D | gara a 8 troppo lunga per i test (~15 min a 10 fps) | numero di giri = byte `+0x64` del controllore giri (scritto una volta in 0x8730e8 dal parametro del percorso) | **D1** INLINE 0x873120 (+ **D2** INLINE 0x512b74 per l'HUD) sotto `#define MK8DX_DEBUG_LAPS 1` | sito [VERIFICATO]; effetto [IPOTESI forte] |
| P1 | pausa in layout 4P | 0x53c818 (vt13 della pagina Race_Pause 0x6f) restituisce solo `rc_Page_RacePause` / `_Sg` | **REPLACE 0x53c818**: con `view8::split` → `"rc_Page_RacePause_SgOunce"` (la parte `rc_Pause_SgOunce_00` è esposta come pane `rc_Pause_Sg_00`, nome usato dal codice) | [VERIFICATO] |
| P2 | i 4 pannelli "controller" (L_SwitchBtns_00..03) | onCreate 0x53c9ec: ciclo `i < 4` (0x53d68c) con nome `"L_SwitchBtns_0"+i` e conteggio settingsA+0x188[0] | patch imm **0x53d68c** `#4→#8` + INLINE **0x53d3a8** / **0x53d678** (x8 = `"L_SwitchBtns8pl_0"+i`) | [VERIFICATO] |
| P3 | gruppo N_Multi8pl_00 invisibile, posizioni 4x2 | animazione `G_SingleMultiChange_00` (gruppo 5 di rc_Pause_Sg*): frame 0 Single, 1 Multi, **2 = 8pl 3x2, 3 = 8pl 4x2** (solo nel file Ounce); il codice imposta 0/1 (0x51bc64) | **TRAMPOLINE 0x51bc10**: dopo Orig `0x8ad3c4(gruppo5, split==8 ? 3 : 2, 0)` | [VERIFICATO] |
| P4 | chi ha messo in pausa = giocatore 5-8 → pausa senza padrone (blocco) | vt75 0x53e0bc: assegnazione pulsanti/maschere/cursore solo `i = 0..3` (0x53e3fc `cmp w20,#4`) | **INLINE 0x53e404**: stesso corpo per i = 4..n-1 | causa [VERIFICATO], blocco [IPOTESI forte] |
| P5 | i giocatori 6-8 non possono mettere in pausa né navigare | scanner tasti 0x8b4038 (vt+0x20 della classe 0x12d3d78) srotolato sui pad **0..4** | **TRAMPOLINE 0x8b4038**: dopo Orig, pad 5..n-1 se `padui::active8` | [VERIFICATO] |
| R | risultati/classifica a 8 | vedi §2 | vedi §2 | vedi §2 |

Ordine consigliato: **D1+D2** (subito: rende testabile tutto il resto) → **P5 + P4** (evitano il blocco della pausa e
abilitano i pad 6-8) → **P1+P2+P3** (pausa 8pl a video) → §2 risultati (prima i siti che bloccano/crashano, poi cosmetico).

## 1. Pausa a 8

### 1.1 Pagina, layout e dati RomFS [VERIFICATO] — `p_pause_name_53c818.asm`, `p_layouts_pause.txt`
- Pagina **Race_Pause = id 0x6f**: fabbrica 0x40b574 (alloc 0x520) → ctor **0x53c860**, vtable 0x1288638 (GOT 0x1309f48),
  registrata in 0x406eb0 (`mov w8,#0x6f`). Metodi propri: vt13 0x53c818 (nome layout), vt73 **0x53c9ec** (onCreate),
  vt75 **0x53e0bc** (ingresso/assegnazione giocatore), vt90 **0x53ea20** (eventi), vt68 0x53e81c, 79/81/82/85/88.
- 0x53c818: `settingsA = 0x86f7bc(); if (st[0x24] & 0x10) → "rc_Page_RacePause"; else st[8]==2 ? "rc_Page_RacePause" :
  "rc_Page_RacePause_Sg"`. Stessa condizione in onCreate a 0x53ce74 sceglie la finestra interna: `rc_Pause_Cmn_00`
  (0x53ce98) oppure **`rc_Pause_Sg_00`** (0x53d2c0, oggetto 0xb8 B ctor 0x51b880 salvato in `pause+0x1e0`).
- RomFS (race.sarc 4.0.1): `rc_Page_RacePause_SgOunce` ha i pane `rc_BG_Cmn_00`, **`rc_Pause_Sg_00`** (prt1 → file
  `rc_Pause_SgOunce_00`): il codice che cerca `rc_Pause_Sg_00` lo trova senza modifiche. `rc_Pause_SgOunce_00` = `rc_Pause_Sg_00`
  + `N_Multi8pl_00` (invisibile di default) con `L_SwitchBtns8pl_00..07` (parte `cm_L_SwitchBtns_00`, la stessa dei 4P) + `N_null_01`.
- Animazione **`G_SingleMultiChange_00`** (unico FLAN che tocca i nuovi pane):

| frame | N_Single_00 | N_Multi_00 | N_Multi8pl_00 | L_SwitchBtns8pl (x, y) | L_Btn_00/01 y |
|---|---|---|---|---|---|
| 0 | 1 | 0 | 0 | – | -173 / -273 |
| 1 | 0 | 1 | 0 | – | 50 / -50 |
| 2 | 0 | 0 | 1 | 3x2: x = -426/0/426, y ±317 (06/07 sovrapposti a 04/05) | 50 / -50 |
| 3 | 0 | 0 | 1 | **4x2**: 00..03 x = -480/-163/163/480 y=317; 04..07 stesse x, y=-317 | 50 / -50 |

  Nel file Sg (Switch 1) l'animazione ha solo i frame 0..1. Frame 2 = 6 giocatori, frame 3 = 8 giocatori [VERIFICATO dati;
  interpretazione 6P/8P IPOTESI forte: posizioni 3x2 e 4x2].
- Chi imposta il frame: controllo `rc_Pause_Sg_00` (vtable [0x1309b50] = 0x1285428): vt8 0x51bb04 crea 7 gruppi
  (2 SwitchCtrl, 3 SwitchHandleAssist, 4 SwitchAutoAccelerator, **5 SingleMultiChange**, 6 CtrlChange); vt9 **0x51bc10**:
  `g = (+0x68 > 5) ? (+0x70)[5] : (+0x70)[0]; 0x8ad3c4(g, 0x40705c() ? 1.0f : 0.0f, 0)` (0x51bc2c..0x51bc64). Nessun altro
  scrittore del frame del gruppo 5. 0x40705c = "modo multi locale" (`cfg+0x1fa8` ∈ {2,3,5,7}; con 8g il modo è 3 → vero).
  0x51be6c è un thunk `b 0x51bc10` (stesso codice).

### 1.2 I pannelli per giocatore (L_SwitchBtns) [VERIFICATO] — `p_pause_oncreate_53c9ec.asm`, `p_swbtn_pad_51c438.asm`
onCreate, ramo Sg, se 0x40705c() (pause+0x51d = 1):
```
0x53d36c  w27 = settingsA+0x188[0]              (= 0x86f154() = locali: 8 con la mod)
0x53d35c  copia "L_SwitchBtns_00" (16 B, 0xeef65a) in sp+0x158
0x53d37c  ciclo w23 = 0..3:  sp+0x166 = '0'+i
  i <  w27: alloc 0xb8 (0x5902d0), 0x723cf4, vt [0x13083e0]; 0x8bc0c4(page, part, pause[0x1e0]+0x30, &{vt, sp+0x158}, &0)
            (x8 = sp+0x158 a 0x53d398, salvato in sp+0xc0 da 0x53d3a8 `stp x28,x8,[sp,#0xb8]`)
            poi figli L_SwitchBtn_00/01/02 (+_03 solo per i==0): desc 0x51c070(&d, &tipo, i, 0) → obj 0x158 B ctor 0x51c088
            (+0xc8 = i giocatore, +0xcc = tipo) → 0x8bc0c4
  i >= w27: pane = 0x8ad8e8(root, &{vt, sp+0x158}) (x8 → sp+0xc0 a 0x53d678); 0x8ade74(pane, 0)  (null-safe)
0x53d68c  cmp w23,#0x4 (0x710012ff) ; b.ne 0x53d37c
```
- I controlli L_SwitchBtn (vtable [0x1309b58] = 0x1285518) usano `0x8bf158(+0xc8)` → `0x8be068` (entry) → GamePlayer
  (0x51c45c, 0x51ca88, 0x51cd54): con la mod GetPad(4..7) dà i pad extra → **già validi per i giocatori 5-8**.
  Unico limite interno: tipo 3 (`L_SwitchBtn_03`, solo giocatore 0) legge `0x425888(i)` (raceMgr+0x214, `cmp #3; b.hi → 0`): inerte.
- Con 8 locali oggi: w27 = 8, il ciclo crea L_SwitchBtns_00..03 in N_Multi_00 (frame 1) → **4 pannelli** (quello che si vede).

### 1.3 Chi mette in pausa e chi naviga [VERIFICATO] — `p_race_pause_trigger.asm`, `p_scanner_8b4038.asm`, `p_pause_onenter_53e0bc.asm`, `p_pause_event_53ea20.asm`
- Pagina Race: `race+0x2b8` = scanner tasti (ctor 0x8b4010, vtable [0x1310890] = 0x12d3d68, maschera tasti +0x20 = 8, id
  +0x18 = 25, registrato con 0x8bb2b8 a 0x53298c). Maschera giocatori della pagina `race+0x1a8 |= 1<<i` per i <
  `race+0x3f1` (0x532990..0x532a08), `race+0x3f1 = 0x86f76c(settings, &0)` = descrittori di tipo 0 = **8** → bit 0..7.
- **Scanner 0x8b4038** (vt+0x20 = slot 4 della vtable 0x12d3d78): per i = **0..4 srotolato**: `if (+0x10 ? 0x8ba634(+0x10, i)
  : +0x18 && 0x8ae0d0(+0x18, i)) 0x8b4174(this, i)`; 0x8ba634(page, i) = bit i di page+0x1a8 && stato pagina 4 && non
  bloccato da [0x8bf09c()]+0x30; 0x8b4174 → `0x8bf158(i)` (pad UI) → evento {+0x10 sorgente, +0x28 tipo 0, +0x2c **pad i**,
  +0x30 tasti} verso page vt+0x2d0 o controllo vt+0x58. Gemello 0x8b4c0c/0x8b4d48 (tocco/ripetizione, stato s32[5] a +0x20,
  `cmp w20,#5` clamp): non serve per i controller.
- Race vt (evento) 0x535224: sorgente == race+0x2b8, tipo 0, stato +0x124 == 5 → 0x53529c(race, pad+1): apre la pausa
  (0x8ba5fc(race, race+0x1d0)) e salva **race+0x3f3 = pad+1** (u8).
- Pausa vt75 0x53e0bc: `pause+0x51c = race+0x3f3 - 1` (0x53e1fc); ciclo **w20 = 0..3** (0x53e3fc `cmp w20,#0x4`, 0x7100129f):
  - i == pauser: OR del bit i in ogni pulsante `pause+0x1f0[k]+0x88` (k < pause+0x1e8), `pause+0x1a8 |= bit`,
    `0x8af3f4(pause+0x1b0, i, btn[0]+0x98, 0, 1, 0)` (cursore);
  - altrimenti: AND NOT del bit nei pulsanti, `pause+0x1a8` |= bit se pause+0x51d (multi: i giocatori usano il proprio
    pannello) altrimenti &= ~bit, `0x8afaf0(pause+0x1b0, i)`.
- Pausa vt90 0x53ea20: accetta solo eventi con `evento+0x2c == pause+0x51c` (scelta pulsante → +0x518, annulla → link +0x220).
- Cursore 0x8af3f4: stato per pad a `+0x28 + clamp4(i)*0x20` (0x8af474/0x8af490 `cmp w21,#4; csel` → pad ≥ 4 usa lo stato del
  pad 0): nessun overflow.

**Conseguenze oggi con 8 locali:**
- giocatori 6-8: il + non viene mai letto (scanner 0..4) → non possono mettere in pausa né premere nulla nelle pagine [VERIFICATO];
- giocatore 5 (pad 4 → pad extra 0): può mettere in pausa (bit 4 della pagina Race acceso), ma pause+0x51c = 4 non è mai
  trattato dal ciclo 0..3 → nessun pulsante gli è assegnato, bit 4 della pagina pausa mai acceso → **nessuno può riprendere**
  [causa VERIFICATO, blocco IPOTESI forte — da provare per primo in Eden];
- giocatori 5-8 non possono usare i loro interruttori (sterzata assistita ecc.) in pausa (bit 4..7 mai accesi).

### 1.4 Correzioni

**P1 – REPLACE 0x53c818** (prima parola 0xa9bf7bfd; `const char* (void* page)`):
```cpp
HOOK_DEFINE_REPLACE(PauseLayoutNameHook) {                 /* 0x53c818: vt13 di Race_Pause (0x6f) */
    static const char* Callback(void*) {
        uint8_t* st = mk8dx::Fn<uint8_t* (*)()>(0x86f7bc)();
        if (st[0x24] & 0x10) return (const char*)(mk8dx::Base() + 0xeef5f6);          /* rc_Page_RacePause */
        if (*(int32_t*)(st + 8) == 2) return (const char*)(mk8dx::Base() + 0xeef5f6);
        if (view8::split) return "rc_Page_RacePause_SgOunce";                          /* RomFS 4.0.1 */
        return (const char*)(mk8dx::Base() + 0xeef608);                                /* rc_Page_RacePause_Sg */
    }
};
```
[IPOTESI forte] la pagina pausa carica il layout dopo SceneInitRace (stessa registrazione 0x406870 delle RaceView, dove il
REPLACE 0x54798c con `split` funziona). Se in Eden il layout resta Sg: loggare la chiamata e, in alternativa, decidere da
`menu8::LocalCount() > 4` (cfg+0x1fa4, valido già nel menu).

**P2 – pannelli 8pl** (tre modifiche, gruppo tutto-o-niente):
```cpp
namespace pause8 { static char name8[] = "L_SwitchBtns8pl_00"; }      /* 18 caratteri + NUL, cifra a [17] */
/* a) 0x53d68c `cmp w23,#4` 0x710012ff -> `cmp w23,#8` 0x710022ff (input::Patch). In 1-4P le iterazioni 4..7 vanno nel
      ramo "nascondi" con nomi L_SwitchBtns_04..07 inesistenti: 0x8ad8e8 -> NULL, 0x8ade74(NULL) ritorna (null-safe, A3 §4). */
/* b) 0x53d3a8 `stp x28,x8,[sp,#0xb8]` 0xa90ba3fc: x8 = nome della parte da creare */
HOOK_DEFINE_INLINE(PauseBtnsNameCreateHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!view8::split) return;
        pause8::name8[17] = '0' + (char)ctx->W[23];
        ctx->X[8] = (uint64_t)pause8::name8;
    }
};
/* c) 0x53d678 `str x8,[sp,#0xc0]` 0xf90063e8: x8 = nome del pane da nascondere (i >= n: 06/07 in 6P) */
HOOK_DEFINE_INLINE(PauseBtnsNameHideHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!view8::split) return;
        pause8::name8[17] = '0' + (char)ctx->W[23];
        ctx->X[8] = (uint64_t)pause8::name8;
    }
};
```
x8 a 0x53d3a8 è usato solo come puntatore al nome (ricaricato a 0x53d3cc); a 0x53d678 viene subito salvato. Nessun salto entra
in 0x53d3a8/0x53d678 [VERIFICATO]. Con split i giocatori 0..n-1 hanno L_SwitchBtns8pl_0i con i figli L_SwitchBtn_00..02
(nomi dei figli invariati: la parte è la stessa `cm_L_SwitchBtns_00`); L_SwitchBtns_00..03 (N_Multi_00, nascosto dal frame
2/3) restano senza controlli. Il giocatore i del controllo = i → GetPad(i) → GamePlayer del locale i [VERIFICATO catena].

**P3 – TRAMPOLINE 0x51bc10** (0xd10143ff; `void (uint8_t* ctl)`; ctl = oggetto `rc_Pause_Sg_00`, gruppi a ctl+0x68/+0x70):
```cpp
HOOK_DEFINE_TRAMPOLINE(PauseSgBindHook) {                  /* vt9 del controllo rc_Pause_Sg_00 */
    static void Callback(uint8_t* ctl) {
        Orig(ctl);
        if (!view8::split || *(int32_t*)(ctl + 0x68) <= 5) return;
        uint8_t* g = ((uint8_t**)*(uintptr_t*)(ctl + 0x70))[5];                     /* G_SingleMultiChange_00 */
        mk8dx::Fn<void (*)(uint8_t*, float, int)>(0x8ad3c4)(g, view8::split == 8 ? 3.0f : 2.0f, 0);
    }
};
```
Il frame del gruppo 5 viene impostato una sola volta (unico `bl 0x8ad3c4` sul gruppo 5 nella classe) [VERIFICATO]. Il gruppo
è "statico" (0x8ae888(…,5,1)) [IPOTESI: frame fisso, non riprodotto].

**P4 – INLINE 0x53e404** (`orr w8,wzr,#0x3` 0x320007e8, primo dopo il ciclo 0..3, nessun salto entrante; x19 = pagina pausa):
```cpp
HOOK_DEFINE_INLINE(PauseAssignExtraHook) {                 /* vt75 0x53e0bc: ciclo 0..3 esteso a 4..n-1 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!view8::split) return;
        uint8_t* p = (uint8_t*)ctx->X[19];
        int pauser = p[0x51c], nb = *(int32_t*)(p + 0x1e8);
        uint8_t** btn = (uint8_t**)*(uintptr_t*)(p + 0x1f0);
        uint8_t* cur = *(uint8_t**)(p + 0x1b0);
        for (int i = 4; i < view8::nLocal && i < 8; i++) {
            uint32_t bit = 1u << i;
            if (i == pauser) {
                for (int k = 0; k < nb; k++) *(uint32_t*)(btn[k] + 0x88) |= bit;
                *(uint32_t*)(p + 0x1a8) |= bit;
                mk8dx::Fn<void (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(cur, i, *(int32_t*)(btn[0] + 0x98), 0, 1, 0);
            } else {
                for (int k = 0; k < nb; k++) *(uint32_t*)(btn[k] + 0x88) &= ~bit;
                if (p[0x51d]) *(uint32_t*)(p + 0x1a8) |= bit; else *(uint32_t*)(p + 0x1a8) &= ~bit;
                mk8dx::Fn<void (*)(uint8_t*, int)>(0x8afaf0)(cur, i);
            }
        }
    }
};
```
Ordine: il ciclo originale ha già chiamato 0x8afaf0(cur, 0); per un pauser ≥ 4 0x8af3f4 scrive lo stato del pad 0 (clamp):
il cursore funziona con quello stato [IPOTESI]. Bit 4 della maschera: con la mod il pad tocco è idx 8, il bit 4 è il giocatore 5.

**P5 – TRAMPOLINE 0x8b4038** (0xf81e0ff3; `void (uint8_t* scanner)`):
```cpp
HOOK_DEFINE_TRAMPOLINE(BtnScanHook) {                       /* scanner tasti: pad 0..4 srotolati -> anche 5..n-1 */
    static void Callback(uint8_t* s) {
        Orig(s);
        if (!padui::active8) return;
        uint8_t* page = *(uint8_t**)(s + 0x10); uint8_t* ctl = *(uint8_t**)(s + 0x18);
        for (int i = 5; i < 8; i++) {
            bool ok = page ? mk8dx::Fn<bool (*)(uint8_t*, int)>(0x8ba634)(page, i)
                           : (ctl && mk8dx::Fn<bool (*)(uint8_t*, int)>(0x8ae0d0)(ctl, i));
            if (ok) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b4174)(s, i);
        }
    }
};
```
Effetto globale (tutte le pagine con scanner): una pagina riceve eventi dal pad i solo se ha il bit i in +0x1a8 (o nel
controllo +0x58): in 1-4P `active8` è falso → identico. Rischio [IPOTESI]: pagine del menu con bit 5..7 accesi (selezione a
due turni: maschera per slot 0..3, quindi no) — loggare le pagine che ricevono eventi da pad 5..7 alla prima prova.

### 1.5 Rischi / da verificare in Eden
1. P1: layout trovato e pane `rc_Pause_Sg_00` risolto (log del puntatore pause+0x1e0 ≠ NULL); anteprima in 6P (split 6).
2. P2: 8 pannelli 8pl visibili e ciascuno con l'icona del controller giusto (L_SwitchBtn usa GetPad(i)); in 6P 06/07 nascosti.
3. P4/P5: pausa da P5..P8, continua/esci solo dal pauser, interruttori di P5..P8 funzionanti; regressione pausa 1P/4P.
4. La "foto" della gara dietro la pausa è la cattura dei layer (step 6C: bit 0x2 copiato sugli extra): già OK.

## 2. Risultati e classifica a 8

Analisi delegata a un sotto-agente (Opus); le parole dei siti di modifica sono state ricontrollate su main.bin
(0x541c80 = 0x39000408, 0x540e88 = 0x39000402, 0x520830 = 0x7100011f, 0x86a720 = 0x7100129f, 0x52dc48 = 0x940e4544,
0x8ba444 = 0xb901aa68). Estratti: `step7_data/r_*.asm`.

### 2.0 Risposta breve
- **Nessun crash o blocco trovato** nelle pagine Race_Result (0x70, ctor 0x540f5c), Race_Next (0x72), Award/TotalResult
  (0x78, ctor 0x52cad4) né nella scena Award [VERIFICATO]. Le uniche letture di `raceMgr+0x17c[i]` fino a 8 sono cicli
  contati su `raceMgr+0x178` (Race_Next 0x53b018, scena Award 0x501ecc): sicure solo grazie a `s32[8]` della mod (B1, da tenere).
- **Conferma**: Race_Result, Race_Next e Award accettano solo il **pad 0** (P1), come in vanilla (maschera page+0x1a8 =
  0x80000001 dal ctor base 0x8ba444, mai cambiata; Award 0x52dc48 `GetPad(0)`). A 8 non cambia nulla [VERIFICATO]. Il
  0x49c938 citato nello step 6 non è la conferma: è il controllo "pad pronto → applet" della pagina di gara (step6_plan_B §4).
- **Anteprime 3D**: le pagine Risultati/Race_Next/Award non creano modelli di kart (icona 2D `P_RankingIconChara_00`);
  la scena Award crea solo JugemAward/Trophy/VRAward [VERIFICATO]. Il "4" delle anteprime non è in queste pagine
  [IPOTESI: quello che si vede in Eden sono le icone; da riguardare dal vivo].
- **Cosmetico C1**: righe dei locali 5-8 non evidenziate (colore = frame 5 di ColorPtn = sfondo nero CPU).
- **Cosmetico C2**: spareggio a parità di punti `(4 - indiceLocale)*20` → P5 come una CPU, P6-P8 sotto le CPU.
- **Correzione a STRUCTS**: `settings+0x180` = numero di **partecipanti** (ricalcolato da 0x86f074), `+0x184` umani,
  `+0x188[tipo]` conteggi per tipo; locali = `0x86f154` = `+0x188[0]` [VERIFICATO].

### 2.1 Siti
| # | sito (parola) | cosa fa | con 8 locali | correzione | certezza |
|---|---|---|---|---|---|
| R1 | Race_Result: listener page+0x530 (0x5423f8, tasti 0x605), evento vt90 0x545370 → page+0x5a9 = 1; maschera 0x80000001 (0x8ba444) | conferma/accelera | solo P1, come vanilla; stati a timer avanzano da soli | nessuna | [VERIFICATO] |
| R2 | Award/TotalResult vt84 0x52db24: 0x52dc48 `bl 0x8bf158` con w0 = 0 (0x940e4544) | uscita dalla classifica GP | solo pad 0 | nessuna | [VERIFICATO] |
| R3 | Race_Next 0x53b018 `ldrsw x20,[x0,#0x178]`, ciclo +0x17c[i] | miglior locale → cfg+0x1a48 | dentro i limiti con B1 | nessuna | [VERIFICATO] |
| R4 | scena Award 0x501ef8/0x501f14 | miglior posizione fra i locali → trofeo | corretto (ciclo contato) | nessuna | [VERIFICATO] |
| R5 | onCreate Race_Result 0x5411e4, righe 0x541c2c..0x541d34 (n = partecipanti, entry 0x86e8e4(pos) clamp ≤ 0xb, capacità 12) | righe L_Result_00..11 | nessun overflow; riga[1] = entry+0x4c (0..7) a 0x541c80 | vedi R7 | [VERIFICATO] |
| R6 | colore riga 0x520600 `bl 0x420da0` → 0x86593c → 0x86a708 (`cmp w20,#4` a 0x86a720) | evidenziazione righe dei giocatori | indice ≥4 → colore 5 = frame 5 di `rc_L_Result_00` ColorPtn = (0,0,0,a180) come CPU; nessun frame per P5-P8 | **C1** | [VERIFICATO]; `split` durante Award [IPOTESI] |
| R7 | spareggio `(4 - riga[1])*20`: 0x542ad0, 0x542b24, 0x544114/0x544118, 0x544190, 0x540ec4/0x540ed8; chiave = punti*100 + bonus + (16 - entry) | ordine a pari punti | l=4 → 0 (come CPU), l=5..7 → -20/-40/-60; non scavalca mai un punto | **C2** | [VERIFICATO] |
| R8 | 0x541b44..0x541c28 (solo squadre, page+0x5a5): +0x17c..+0x188 srotolati → page+0x5a6 | ordine dei blocchi squadra | locali 4..7 ignorati | lasciare (0x541c2c bersaglio di 5 salti) | lettura [VERIFICATO], senso [IPOTESI] |
| R9 | 0x5205cc con arg4=1: 0x520830 `cmp w8,#0` (0x7100011f), frame = l+1 senza clamp; unico chiamante con arg4 variabile 0x53fdbc (classifica nella pausa `rc_Page_RacePause`/Cmn) | colore righe classifica in pausa | l=4..7 → frame 5..8 (nero/rosso/blu/rosso) | **C3** facoltativa | [VERIFICATO] |
| R10 | solo locale 0: 0x543cc0/0x543f68 (rating online), 0x52d940 (vittoria squadra) | online/squadre | inerte offline | nessuna | [VERIFICATO] |
| R11 | i vari `cmp #3/#4/#5/#8` (0x541198…0x544d58, Race_Next 0x5396f4…) | modo/stato pagina | non sono limiti sul numero di giocatori | nessuna | [VERIFICATO] |

Collegati (fuori da queste pagine): 0x551b28/0x551c58 (stato per locale a 4 voci con clamp, aliasing sul locale 0 a fine gara,
step6_plan_B §4: [DA VERIFICARE] con watch su obj+0x10); 0x534e04..0x534f08 (pagina Race, azzera entry+0x20c/+0x209 per i
locali 0..3 solo in un modo speciale 0x7c7fc).

### 2.2 Schizzi C++
**C1 – colore righe 5-8** (modifica dell'hook esistente `PlayerColorIdxHook`, 0x86a720, main.cpp:1973):
```cpp
static bool Local8() { auto* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)(); return rm && *(int32_t*)(rm + 0x178) > 4; }
HOOK_DEFINE_INLINE(PlayerColorIdxHook) {                   /* 0x86a720 `cmp w20,#4` (0x7100129f) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t l = ctx->W[20];
        if ((view8::split || Local8()) && l >= 4 && l < 8) ctx->W[20] = l - 4;   /* colori P1..P4 */
    }
};
```
0x86a708 ha un solo chiamante (0x86593c ← 0x420d18 HUD, 0x420da0 righe/icone 0x505460/0x50b008/0x520600): la condizione
`Local8()` copre le pagine che girano con `split` = 0 (Award) [IPOTESI sul valore di split lì].

**C2 – spareggio** (gli unici due produttori di riga[1]; consumatori 0x542ac8, 0x54410c, 0x540ed4; siti non PC-relativi, non bersagli):
```cpp
HOOK_DEFINE_INLINE(ResultRowLocalHook) {   /* 0x541c80 `strb w8,[x0,#1]` (0x39000408), onCreate Race_Result */
    static void Callback(exl::hook::InlineCtx* ctx) {
        int32_t l = (int32_t)ctx->W[8];
        if (Local8() && l >= 0 && l < 8) ctx->W[8] = (uint32_t)(l >> 1);        /* bonus sempre >= 20 per i locali */
    }
};
HOOK_DEFINE_INLINE(AwardRowLocalHook) {    /* 0x540e88 `strb w2,[x0,#1]` (0x39000402), init riga usata dall'Award */
    static void Callback(exl::hook::InlineCtx* ctx) {
        int32_t l = (int32_t)ctx->W[2];
        if (Local8() && l >= 0 && l < 8) ctx->W[2] = (uint32_t)(l >> 1);
    }
};
```
**C3 (facoltativa, classifica nella pausa)** INLINE 0x520830 (0x7100011f): `if (Local8() && W8 >= 4 && W8 < 8) W8 -= 4;`.

### 2.3 Priorità e rischi
1. Nulla di bloccante: tenere B1 (`raceMgr+0x17c` s32[8]).
2. C1 (una riga in un hook esistente), poi C2 (solo a pari punti), C3/R8 solo se servono.
3. Da vedere in Eden: valore di `split`/`nLocal` durante Race_Result e Award; nomi delle righe 5-8 (entry+0x1a0 via
   0x520818 → 0x423a0c, tabella per entry); podio = kart delle posizioni 1-3 [IPOTESI]; aliasing 0x551b28.

## 3. Helper DEBUG per i test: gara a 1 giro / arrivo forzato

### 3.1 Dove il gioco legge il numero di giri [VERIFICATO] — `d_lapcount_8730e8.asm`, `d_lapcheck_873ee0.asm`, `d_lapcoin_init_512b00.asm`, `d_courseparam_80052c.asm`
- Parametro del percorso: CourseParam (ctor 0x80052c, `+0x14 = 3` di default) letto da BYAML `"LapNumber"` (0xf0badb) in
  0x8005b4 (0x800660). Il valore usato in gara è una copia byte:
- **Controllore giri** (oggetto `[[raceSys(0x7e7434)+0x190+4*8]+0x218]+0x58`, metodo virtuale 0x8730e8, puntatore in vtable
  a 0x12cfeb0): `+0x38 = 0x7ff7b0()` (percorso/rotaia giri), `+0x64 = [+0x38]+0x32` (**giri totali**, u8) a 0x87311c,
  `+0x65 = 1`. Scritto una volta; nessun altro scrittore di quel campo nella classe (gli altri `strb …#0x64]` del main sono
  altre classi).
- Lettori di `+0x64` (stesso oggetto): 0x873ef8/0x873f44/0x873f64/0x873ffc (avanzamento giro per kart: voce 0x78 B, `+0x28`
  giro s8; `giro == +0x64` → `+0x12 |= 0x100` = **arrivato**, `+0x66 = 1`), 0x874c50/0x874cc0/0x874ddc, 0x8708c0 (arrivo
  forzato con tempi stimati), 0x871104, HUD LapCoin 0x512b64 (copia in LapCoin+0xd2, default 3), 0x7d2268 (oggetti per giro),
  0x10fadc (Lakitu: `== 7`/`== 3`), TA 0x5461d8/0x546444. Unico lettore diretto di `+0x32` del percorso: 0x841e4 (audio,
  tempo della musica) — cosmetico.

### 3.2 D1/D2 – giri forzati (consigliato)
```cpp
#define MK8DX_DEBUG_LAPS 1          /* 0 = disattivato; N = giri massimi in ogni gara (solo build di test) */
#if MK8DX_DEBUG_LAPS
/* D1: 0x873120 `orr w8,wzr,#0x1` (0x320003e8), subito dopo `strb w8,[x19,#0x64]` in 0x8730e8; x19 = controllore giri.
   Nessun salto entra in 0x873120. */
HOOK_DEFINE_INLINE(DebugLapsHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* lc = (uint8_t*)ctx->X[19];
        if (lc[0x64] > MK8DX_DEBUG_LAPS) {
            Logging.Log("[mk8dx-8p] DEBUG giri %d -> %d", lc[0x64], MK8DX_DEBUG_LAPS);
            lc[0x64] = MK8DX_DEBUG_LAPS;
        }
    }
};
/* D2: 0x512b74 `strb w8,[x19,#0xd2]` (0x39034a68) nell'init di LapCoin (HUD "giro x/N"): w8 = giri letti da +0x64.
   Serve solo se l'HUD viene inizializzato prima di 0x8730e8 (ordine da verificare); senza D2 l'HUD direbbe "1/3". */
HOOK_DEFINE_INLINE(DebugLapsHudHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (ctx->W[8] > MK8DX_DEBUG_LAPS) ctx->W[8] = MK8DX_DEBUG_LAPS;
    }
};
#endif
/* install (con verifica della parola come per gli altri):
   if (*(uint32_t*)(base + 0x873120) == 0x320003e8) DebugLapsHook::InstallAtOffset(0x873120);
   if (*(uint32_t*)(base + 0x512b74) == 0x39034a68) DebugLapsHudHook::InstallAtOffset(0x512b74); */
```
Effetto atteso [IPOTESI forte]: gara di 1 giro in tutti i modi (GP/VS) → ~5 minuti a 10 fps invece di ~15; il flusso
"arrivo → risultati → classifica GP → gara successiva" resta quello vero (stessa logica d'arrivo per giro, logo FINISH,
fine gara della pagina Race 0x534848 che confronta LapCoin `+0xd1 == +0xd2`). Percorsi a tratti (Monte Wario, giri = 3
sezioni) e Baby Park (7) finiscono dopo la prima sezione/il primo giro: accettabile per i test. Con `MK8DX_DEBUG_LAPS 0` il
codice non viene compilato.

### 3.3 D3 – arrivo forzato di tutti (alternativa, per saltare anche il giro)
- **0x86c5ac()** = "arrivo di tutti": per ogni kart `i < settingsA+0x180` di `[sub4+0x218]+0x68[i]` senza bit 2 di `+0x3c`
  chiama **0x870800(info, 1)**: `+0x3c |= 2`, tempo = tempo gara corrente, giri mancanti stimati (fino a `+0x64`), poi
  0x870adc / 0x870bcc (registrazione arrivo/classifica). È ciò che la sequenza di gara fa a fine gara per i kart non arrivati
  (chiamanti 0x39b8c0, 0x39ba3c, 0x39bffc, 0x39c1d0, 0x39da80; `d_goalall_callers.txt`) [VERIFICATO].
- Uso: in `proto::PerFrame` (già per frame in gara), con una combinazione sul pad UI 0 (es. tenuti L+R+Meno per 1 s) chiamare
  `mk8dx::Fn<void (*)()>(0x86c5ac)()` una volta per gara.
- Rischio [IPOTESI]: chiamata fuori dalla sequenza: i kart dei giocatori risultano "arrivati" (classifica/tempi) ma il kart
  non riceve l'evento di arrivo fisico (camera/logo FINISH, guida automatica): la pagina Race chiude la gara quando gli
  oggetti viewport risultano finiti (0x551294 / 0x5512c4), che dipendono dall'HUD e non solo da +0x3c → la gara potrebbe
  non chiudersi. Per questo D1 è preferibile; D3 solo come esperimento.
