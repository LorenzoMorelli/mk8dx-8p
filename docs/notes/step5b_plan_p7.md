# Step 5b – passo 7: difetti delle finestre varianti a 8 e layout "come Switch 2" – piano statico

Data: 3 nov 2026. Solo analisi statica: `src/` non modificato, nessuna build.

**Basi usate**
- `../img/main.bin`, `../img/main.ann.asm`;
- RomFS 3.0.5/4.0.1 in `../romfs-tmp` (`v3/menu.sarc`, `v4/menu.sarc`, `menu4/mn_CharaKartSlct_{8pl,6pl,Multi}`, `menu4/mn_L_CharaWin_00.szs`,
  `v4/common.sarc` → `cm_L_BtnCursor_00`);
- osservazioni: `step5b_data/eden_5b7_charsel_varianti.png`, `eden_log_5b7.txt`;
- riferimenti Switch 2: `step5_data/ref_switch2_charsel_8p.jpg`, `ref_switch2_kart_8p.jpg`.

**Verifica**: `step5b_data/p7_verify.py` → `p7_verify.txt`, **0 errori**. Lo script controlla le parole citate, le stringhe, i fotogrammi
`G_Ptn_00` 3.0.5/4.0.1 e le posizioni N_Player/N_Window 8pl/Multi. I parser bflyt/bflan sono in `p7_lyt.py` e `p7_flan.py`.

Etichette: [VERIFICATO] = letto in main.bin/RomFS o misurato sullo screenshot; [IPOTESI] = da confermare in Eden.

## 0. Sintesi

| difetto | causa | correzione |
|---|---|---|
| **(a)** finestre grandi "agli angoli" che coprono i nomi | Il `CharaWinPosHook` **scatta**: la posizione è già quella 8pl (N_Player_0s + N_Window_0s). Il problema è la **scala**. Per la finestra 0x4694cc imposta il fotogramma 0 o 1 di `G_Ptn_00` (scala di `N_SC_00` 1.0 / 0.65) e oggi esce 1.0. La RomFS 4.0.1 ha **due fotogrammi nuovi, 2 = 0.4 e 3 = 0.3**, assenti nella 3.0.5 e mai impostati dal main Switch 1 [VERIFICATO] | Nel callback di `CharaWinPosHook` (stesso indirizzo, nessun nuovo hook): con 5-8 giocatori si mette `Ptn` al fotogramma **3** (8P) o **2** (6P), e lo si ripristina con 0x4694cc nelle sessioni 1-4P |
| **(b)** P5-P8: nessun riquadro attorno al costume | Il cursore a 4 slot della CharaList 0x01/0x02/0x5e/0x64 lascia il giocatore 4+j nello **slot 0 con stato 2**: lo stato 2 vuol dire "attivo, nessun item scelto" (log D: `slot0 2/0` costante) [VERIFICATO]. Le cause sono due. **(1)** L'attivazione 0x8afdd0 seleziona lo slot 0 con p = 0, e 0x8af3f4 esce subito perché il bit 0 manca in `pagina+0x1a8` = 0x80000080. **(2)** Nessuno legge il pad 4+j su quel cursore: 0x8b0084 legge solo p = 0..3, il ciclo 4..7 di `CursorPollHook` gira solo se il cursore è largo, e con p = 0 il focus è quello di P1. Il disegno 0x8b0238, con stato < 4, va a 0x44316c, che nasconde il riquadro. Quindi la correzione p5fix2, che copia lo slot nel disegno, è giusta ma riceve uno stato 2 | la stessa copia temporanea "larga" (slot 0 → slot s+4, `+0xbc = 8`) si applica anche in **attivazione** (`Cur8ActivateHook`) e in **lettura pad** (`CursorPollHook`). Nessun nuovo indirizzo |
| **(c)** "tutti i cursori viola" a finestre aperte | Il colore dei cursori grafici (ColorChange, anim[3]) si imposta **solo** in 0x49a9c4 (onCreate di cm_Page_Cursor, unico `bl 0x442ca0` del main) e nella pausa della mod (`TintCursor0`). Nessun codice del menu lo cambia quando si apre una finestra [VERIFICATO]. Viola = fotogramma 9 = colore di P7 | causa **non trovata staticamente**. Diagnostica mirata in §3.3: frame ColorChange letto con 0x8ad4cc e visibilità dei pane 00..07. Serve anche uno screenshot con i cursori viola |
| **(d)** sfondo scuro a trama carbonio | vanilla: `W_WinBase_Mt` (o `_B`) di `mn_L_CharaWin_00` con `tc_MtCarbon_16x16` + `tc_MtMarioKartLogo_00` [VERIFICATO] | nessuna |

**Parte B.**
- Il layout 8pl coincide **al pixel** con Switch 2 per nomi, griglia e celle kart [VERIFICATO, misura §5.1]. Manca solo l'anteprima 3D sopra ogni
  targhetta, centrata su N_Window_0i.
- I 4 modelli del menu sono i **KartUnit del KartDirector**, uno per voce di `settings+0x180`. Il limite 4 viene da:
  - la tabella dei preset di posa/camera `{3,4,5,6}[slot]`, letta **senza limite**;
  - il viewer 3D a 4 voci, con i piedistalli `BaseTable`.
- 8 anteprime 3D vere richiedono 4-6 sessioni, con rischio medio-alto (§5.4). Il ripiego 2D con icone del personaggio sui pannelli richiede
  1-2 sessioni (§5.5).

## 1. (a) Posizione e dimensione della finestra varianti

### 1.1 Fatti [VERIFICATO]
- **0x4694fc(win, &tipo, count = sel+0x8c0, slot = CharaList+0x1e8).** Chiamata da 0x4b2cc4 e 0x4b2e74 (macchina a stati della CharaList,
  all'apertura). Fa questo:
  - imposta i fotogrammi di `Kind` (anim[3]) e anim[4] in base al tipo;
  - calcola `id = {0x14,0x15,−1,0x16}[count−1]` (0x469740..0x469758) e cerca nel layout di quella pagina CharaParts i pane
    `"N_Player" + "_0"+slot` e `"N_Window" + "_0"+slot` (stringhe 0xee987e/0xee8fed, carattere a sp+0x4a/0x3a);
  - **somma le due traslazioni locali** (0x8addd4 = pane+0x30) e chiama `0x8adff0(win+0x30, &pos)` (SetTranslate del pane radice, 0x46985c).
- **Scala.** 0x4694fc non imposta alcuna scala. La scala della finestra viene da anim[2] = **`Ptn`** (ctor 0x469300.., stringa 0xee8cfa), il gruppo
  `G_Ptn_00` del layout `mn_L_CharaWin_00`:

  | fotogramma Ptn | 0 | 1 | 2 | 3 |
  |---|---|---|---|---|
  | `N_SC_00` scala | 1.0 | 0.65 | **0.4** | **0.3** |
  | 3.0.5 | sì | sì | — | — |
  | 4.0.1 | sì | sì | **nuovo** | **nuovo** |

  - 4.0.1: frame max 3; con i fotogrammi 1..3 cambiano anche `W_WinBase_*` a 400×370 e l'offset y da 32 a 0.
  - Unico scrittore di Ptn: **0x4694cc(win, b)**, che fa `frame = b ? 0 : 1` (fcsel 0x4694f4). Unico chiamante: 0x4b2b2c, con `b = 0x3fe658() & 1`.
  - I fotogrammi 2 e 3 non sono mai usati dal main Switch 1. È un'altra parte "5-8 giocatori" presente solo nella RomFS 4.0.1.
- **Il cursore grafico segue la scala.** 0x46c0a4 scala il cursore grafico della finestra con `scala(N_All_00) × scala(N_SC_00)` (+0x2f8/+0x300,
  0x46c0c4..0x46c0f4) e poi chiama 0x49abc4. Scalare `N_SC_00` tramite Ptn scala quindi anche il riquadro; scalare il pane radice no.
- **Geometria.**

  | layout | finestra = N_Player + N_Window | N_Window relativo | distanza tra le righe |
  |---|---|---|---|
  | 8pl | (±480, 294 / 136 / −22 / −180) | (∓140, −50) | 158 |
  | 6pl | — | (∓135, −30) | 208 |
  | Multi | (±460, 195 / −105) | (∓170, +10) | 300 |

  - 8pl: la targhetta `L_Name` sta a (∓140, −130) rispetto a N_Player, cioè **80 sotto** il centro della finestra.

### 1.2 Diagnosi
- **L'hook funziona** [VERIFICATO sullo screenshot]. La finestra di P1 ha il centro a y ≈ 294 ed esce dal bordo alto. Quelle di P7 e P8 sono
  centrate a y ≈ −170: 8pl prevede −180, Multi prevede −105. Con il layout Multi P7 e P8 non avrebbero proprio un N_Window_06/07.
- **Dimensione.**
  - Misura sullo screenshot: circa 251×230 unità, cioè `W_FrameSh_Frame` 333×325 × `N_All` 0.8 × `N_SC` **1.0**. Ptn è quindi al fotogramma 0.
  - Con un passo di 158 tra le righe la finestra copre la targhetta del proprio giocatore (bordo inferiore −165 contro targhetta −130) e quella
    della riga sotto.
  - "Angoli in stile Multi" è solo un'impressione: le colonne a ±480 sono vicine a ±460 della Multi.
- **Come dovrebbe apparire (Switch 2)** [IPOTESI forte]. Lo dicono la geometria 8pl e i fotogrammi Ptn nuovi:
  - finestra centrata su **N_Window_0i**, cioè dove Switch 2 disegna l'anteprima 3D (§5.1);
  - scala `N_SC` **0.3** (8P) o **0.4** (6P), quindi circa 80×78 o 107×104 unità;
  - entra nella riga sopra la targhetta. Con 0.4: da y−102 a y+2 rispetto a N_Player, con la targhetta da −118 a −142. P1 arriva al
    massimo a 356 < 360.

  Nelle immagini di riferimento non ci sono finestre aperte, quindi il fotogramma esatto per l'8P (2 o 3) va scelto a vista.

### 1.3 Correzione (corpo di `CharaWinPosHook`, inline esistente su 0x469760 `b90013e8`)
Registri in quel punto [VERIFICATO]:
- `x19` = finestra (0x469524);
- `w20` = count (0x469534);
- `w22` = slot (0x469530), ancora valido: viene riscritto solo a 0x469774.
```cpp
#define MK8DX_P7_PTN8 3.0f   /* fotogramma G_Ptn con 7-8 giocatori: 3 = N_SC 0.3, 2 = 0.4 */
namespace cw8 { static uint8_t ptnSet[8]; }                 /* finestra dello slot s con Ptn forzato */
HOOK_DEFINE_INLINE(CharaWinPosHook) {              /* 0x469760 str w8,[sp,#0x10] in 0x4694fc */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t c = ctx->W[20], s = ctx->W[22]; uint8_t* win = (uint8_t*)ctx->X[19];
        int32_t na = *(int32_t*)(win + 0x68); uint8_t** an = *(uint8_t***)(win + 0x70);
        if (c > 4 && c <= 8 && p4::Names()) {
            ctx->W[8] = p4::kNamesId;
            if (an && na > 2) {                            /* anim[2] = Ptn (come 0x4694cc) */
                mk8dx::Fn<void (*)(uint8_t*, int, float)>(0x8ad3c4)(an[2], 0, c > 6 ? MK8DX_P7_PTN8 : 2.0f);
                if (s < 8) cw8::ptnSet[s] = 1;
            }
        } else if (s < 8 && cw8::ptnSet[s]) {             /* sessione 1-4P dopo una a 8: Ptn come il vanilla */
            cw8::ptnSet[s] = 0;
            mk8dx::Fn<void (*)(uint8_t*, bool)>(0x4694cc)(win, mk8dx::Fn<bool (*)()>(0x3fe658)());
        }
    }
};
```
- 1-4P: identico, perché il ramo di ripristino scatta solo dopo una sessione a 8 e ripete esattamente 0x4b2b20..0x4b2b2c.
- [IPOTESI forte] 0x4b2b2c (che imposta Ptn 0/1) gira all'ingresso della CharaList, **prima** della macchina a stati che chiama 0x4694fc.
  Se in Eden la finestra restasse grande, serve un trampolino di riserva su **0x4694cc** (prima parola `b9406808`, nessuna istruzione relativa
  al PC) con la stessa condizione.
- [IPOTESI] Se il cursore dentro la finestra risultasse spostato: la sua scala la gestisce già 0x46c0a4 (N_SC), quindi controllare solo che
  `win+0x2d0 == 2` (ramo 0x46c124).

## 2. (b) Riquadro del costume per P5-P8

### 2.1 Catena [VERIFICATO]
1. **Attivazione 0x8afdd0** (per s = 0..3: stato 1 → `0x8af3f4(c, s, idx, 0, 1, 0)` → se resta 1, stato 2):
   - `0x8af3f4` richiede `(pagina+0x1a8 >> 31) && (pagina+0x1a8 & 1<<p)` (0x8af430..0x8af468). Per la 0x64, +0x1a8 = 0x80000080 e p = 0:
     **fallisce**, e lo slot 0 resta a 2.
2. **Lettura pad**:
   - `0x8b0084` chiama `0x8b00e0(c, 0..3)`;
   - `CursorPollHook` aggiunge 4..7 solo se `Wide(c)`;
   - con p = 0 il focus `mgr+0xc68[0]` è quello di P1 (≠ 0x64) e si esce a 0x8b011c;
   - lo stato 2 passerebbe a 3/4 solo con `+0xac & tasti` del pad giusto (0x8b015c..0x8b0174).
3. **Disegno 0x8b0238**: con stato < 4 (0x8b0310/0x8b0314) va a 0x8b0498 → `0x44316c(gfx, …)`, che nasconde il riquadro.
   - Il blocco p5fix2 di `Cur8DrawHook` (copia slot 0 → s+4, `+0xbc = 8`, `0x8b0238(c, s)`) è corretto: il focus `foc[s-4]` vale la pagina,
     come dice il log D `foc=1`. Ma copia uno stato 2, quindi non disegna nulla.
- La decisione già funziona (`7/0` nei log p5fix): la pagina la gestisce sullo slot 0 con il pad reale.

### 2.2 Correzione (corpi di hook esistenti)
Helper nel namespace `cur8` (dopo `Reset`). `kIds` è una copia di `cw8::kIds`, come in p5fix2:
```cpp
    static int CwSlot(uint8_t* c) {                         /* cursore a 4 slot di una CharaList 0x01/0x02/0x5e/0x64 -> 4..7, altrimenti -1 */
        if (!p4::one || !c || *(uint32_t*)(c + 0xb8) != kMagic || Wide(c)) return -1;
        uint8_t* pg = *(uint8_t**)(c + 8); if (!pg) return -1;
        static constexpr int32_t kIds[4] = { 0x01, 0x02, 0x5e, 0x64 };
        int32_t id = *(int32_t*)(pg + 0xe8);
        for (int j = 0; j < 4; j++) if (kIds[j] == id) return 4 + j;
        return -1;
    }
    template <class F> static void AsWide(uint8_t* c, int s, F f) { /* slot 0 vissuto come slot V = s+4 del cursore largo */
        uint8_t* s0 = Slot(c, 0); uint8_t* sv = Slot(c, s); uint8_t keep[0x20];
        memcpy(keep, sv, 0x20); memcpy(sv, s0, 0x20);
        *(int32_t*)(c + 0xbc) = 8; f(); *(int32_t*)(c + 0xbc) = 4;
        memcpy(s0, sv, 0x20); memcpy(sv, keep, 0x20);
    }
```
1. **`Cur8ActivateHook` (T 0x8afdd0, `a9be4ff4`).** Subito dopo `Orig(c)`, **prima** di `if (!cur8::Wide(c) …) return;`:
   ```cpp
        if (int s = cur8::CwSlot(c); s >= 4 && *(int32_t*)(c + 0x10) && *(int32_t*)cur8::Slot(c, 0) == 2)
            cur8::AsWide(c, s, [&] { uint8_t* sv = cur8::Slot(c, s); *(int32_t*)sv = 1;
                mk8dx::Fn<bool (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(c, s, *(int32_t*)(sv + 8), 0, 1, 0);
                if (*(int32_t*)sv == 1) *(int32_t*)sv = 2; });
   ```
   Con `Wide(c)`, F6/F7 danno V = s+4 → la copia. Il bit s è in +0x1a8, il bit s negli item +0x58 lo mette il blocco D di `CharselFrameHook`,
   e la scrittura memtab con V = 8..11 è rediretta a `mem8` (CUR8_MEM).
2. **`CursorPollHook` (T 0x8b0084, `f81e0ff3`).** Nel ramo `p < 0`, dopo `Orig(cur)`:
   ```cpp
            if (int s = cur8::CwSlot(cur); s >= 4) cur8::AsWide(cur, s, [&] { mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b00e0)(cur, s); });
   ```
   - F13 dà V = s+4 → focus `foc[s-4]` (Cur8FocP) = pagina.
   - `0x8ba634(pagina, s)` fa passare il pad perché il bit s è in +0x1a8, e `0x8bf158(s)` restituisce il pad extra.
   - 0x8af3f4 gira mentre il cursore è ancora largo, perché è una chiamata in coda dentro f().
   - Verificare che `CursorPollHook` sia installato ogni volta che `cur8::on` è vero: oggi l'installazione a 3401 è nel blocco pausa
     (`pause8::pollHooked`).
3. **`Cur8DrawHook` (R 0x8b01ec).** Il blocco p5fix2 si riduce a:
   ```cpp
        if (int s = cur8::CwSlot(c); s >= 4 && cur8::foc[s - 4] == *(uint8_t**)(c + 8))
            cur8::AsWide(c, s, [&] { mk8dx::Fn<Draw>(0x8b0238)(c, s); });
   ```
   - Il grafico è `cp+0x1d0+(s+4)*8` = `cm_L_BtnCursor_0s`, con colore P5..P8 (frame 7..10).
   - Il ColorChange per giocatore dell'item lo dà vt+0x70(item, s) a 0x8b0810.
- **1-4P**: `CwSlot` restituisce −1 (`p4::one` falso, id 0x28..0x2b) → identico.
- **Attesa in Eden**:
  - all'apertura della finestra di P8 compare subito il riquadro arancio sul costume corrente (indice da `mem8`);
  - si muove con il pad di P8;
  - alla conferma: "deciso" e il riquadro scompare;
  - log D: `slot0 4/idx` (o 3), non più 2.

## 3. (c) Cursori viola

### 3.1 Fatti [VERIFICATO]
- I cursori grafici (`cm_L_BtnCursor_00..07`, oggetti da 0x442a84) hanno 4 animazioni: [0] In/Out, [1] Decide/Loop, [2] **SizeChange**,
  [3] **ColorChange**.
- `0x442ca0(g, n)` imposta ColorChange. Il suo **unico** chiamante nel main è 0x49a9c4 (onCreate; la mod: frame 7..10 per 04..07). La mod lo
  usa anche in `pause8::TintCursor0`, solo nella pausa e solo sul cursore 0.
- `0x442c7c` (unico chiamante 0x8b07bc) usa anim[2] = SizeChange con il valore `CursorSize` dell'item: non è un colore.
- Gli altri metodi toccano solo In/Out, Loop, alpha (0x8ae048 da 0x46c0a4) e matrice o scala (0x49abc4, 0x443404):
  0x442f5c, 0x44316c, 0x44309c, 0x443008, 0x442ef0, 0x442f10.
- ColorChange di `cm_L_BtnCursor_00` (10 fotogrammi, colore base):

  | frame | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
  |---|---|---|---|---|---|---|---|---|---|---|
  | colore | giallo | ciano | rosa | verde | rosso | blu | rosso | blu | **viola** | arancio |

### 3.2 Conclusione
- Nessun codice ricolora i cursori all'apertura delle finestre. Il viola è il colore di P7 (frame 9, cursore grafico 06).
- Ipotesi, in ordine di probabilità [IPOTESI]:
  1. **Non è un cambio di colore ma di oggetto**: si vede il cursore 06 al posto di altri. Candidati:
     - il blocco p5fix2 quando la finestra di P7 (0x5e) è aperta: `0x8b0238(c, 6)` con V = 10 → gfx 06 → 0x44316c;
     - `0x49abc4(cp, p, s)` (G5, `Vg`) chiamato da 0x46c148 per le finestre.
     Va escluso con la diagnostica.
  2. Il "riquadro" percepito è l'evidenziazione dei pulsanti costume (`mn_L_CharaWinSlctBtn`, vt+0x70(item, p) → `BtnPlayerHook`, frame
     p+1): con p = 6 la tabella a 8 colori dà viola. Non spiega "tutti".
- Lo screenshot 5b7 mostra il riquadro di P1 **giallo**. Serve uno screenshot del momento in cui i cursori sono viola, con l'indicazione di
  quali finestre erano aperte.

### 3.3 Diagnostica (solo log, nel blocco D di `CharselFrameHook`, ogni 60 frame)
```cpp
uint8_t* cp = cur8::GfxPage();
for (int i = 0; cp && i < 8; i++) { uint8_t* g = *(uint8_t**)(cp + 0x1d0 + (i < 4 ? i : i + 4) * 8); if (!g) continue;
    uint8_t** an = *(uint8_t***)(g + 0x70); float fc = *(int32_t*)(g + 0x68) > 3 ? mk8dx::Fn<float (*)(uint8_t*)>(0x8ad4cc)(an[3]) : -1;
    Logging.Log("[mk8dx-8p] C7: gfx%d=%p frameColor=%.0f vis=%d act=%d", i, g, fc, mk8dx::Fn<bool (*)(uint8_t*)>(0x8adf84)(g + 0x30), g[0xc0]); }
```
- `0x8ad4cc(anim)` restituisce il fotogramma corrente, come lo usa 0x442ea4.
- Se tutti valgono 9 → qualcuno chiama SetFrame su ColorChange: trampolino di solo log su 0x8ad3c4 filtrato su `x0 == an[3]` per stampare LR.
- Se i frame sono giusti → è il caso 3.2.1.

## 4. (d) Sfondo a trama carbonio [VERIFICATO]
`mn_L_CharaWin_00`, `N_WinBlack_00`:
- `W_WinBase_B` e `W_WinBase_Mt` (475×490, scalati da Ptn) con le texture `tc_MtCarbon_16x16^s`, `tc_MtMarioKartLogo_00^s`, `tc_MtBlock_8x8^r`;
- versioni a tema con `N_WinBase_Mt_{Yoshi,Heyho,Suits,Mario,Catherine,Animal}`, `Splatoon`, `Link`;
- `G_Kind` sceglie la versione.

È lo sfondo vanilla della finestra varianti (Link → `W_WinBase_Link_00`). Non c'è niente da correggere.

## 5. Parte B – layout come Switch 2

### 5.1 Confronto con il riferimento [VERIFICATO, misure]
Scala del riferimento: area di gioco da x = 180 a 1820 su 2000 px, cioè 1.281 px per unità.

| elemento | Switch 2 | layout 8pl |
|---|---|---|
| targhetta P1 | (−480, 214) | N_Player_00 + L_Name = (−340−140, 344−130) = (−480, 214) ✓ |
| righe | 214 / 56 / −102 / −260 | 344/186/28/−130 −130 ✓ |
| colori | P1 giallo, P2 ciano, P3 rosa, P4 verde, P5 rosso, P6 blu, P7 viola, P8 arancio (sinistra = pari, destra = dispari) | ✓ |
| griglia | centrata (x = 0, larghezza ~640) | `N_Chara_00` (0, 25) + `W_GlassBoard` 644×495 ✓ (uguale alla Multi) |
| kart: celle | L_KartSlot a x = ±163 | N_Player ± 177 = ±163 ✓ |
| anteprima 3D | circa 117 unità di altezza, centro circa 95 sopra la targhetta, cioè ≈ N_Player + (∓140, −35) | ≈ **N_Window_0i** (∓140, −50) |

- **Griglia e pannelli della 0x12 vanno già bene così.**
- Mancano solo:
  - le 8 anteprime (personaggio su kart con piedistallo scuro) centrate su N_Window_0i;
  - "−/+🔍" accanto alla targhetta nella pagina kart.
- I piedistalli che si vedono oggi nello screenshot (2 a mezza altezza) sono i `BaseTable` del viewer Multi (§5.2). `Hide3D` nasconde i
  KartUnit ma non loro: vanno nascosti anch'essi, §5.4 tappa 0.

### 5.2 Origine dei modelli del menu [VERIFICATO salvo indicazione]
- **KartDirector (0x13d778, init).**
  - `x20 = 0x86f7bc()`; `+0xc0 = [x20+0x180]` (0x13d7b8/0x13d7bc);
  - heap **"KartWork"** di 0x1864000 o 0x2464000 (0x13da10, il secondo se `+0xe9 == 0`);
  - array `+0xc8 = new KartUnit*[+0xc0]` (0x13da98);
  - per i < +0xc0: `0x8843a8(i)`, `new 0x368`, `0x16e3dc(obj, i, +0xe9)` (stessa classe KartUnit della gara), poi 0x16ca28.
  - Nel menu +0x180 = 4 (log `modelli 4`). **Il 4 non è una costante nel codice**: è il numero di voci della configurazione del menu al momento
    dell'init della scena.
  - Le stringhe vicine (`KartDirector::CalcPosition`, ecc.) indicano che è lo stesso KartDirector della gara.
- **Viewer 3D del menu** (`[0x398898()+0x270]+0x70`, classe 0x42fxxx..0x431xxx; risorse `UI3D/BaseTable/BaseTable.bfres` ecc.):
  - **4 voci fisse** da 0x138 a +0x568, ciclo del ctor fino a `cmp x24,#0x3a8` (0x42feac);
  - voce: modello `+0x570` (piedistallo), matrice +0x578..+0x5a4 (traslazione **+0x59c..+0x5a4**), +0x630/+0x632 animazione;
  - accessori per indice:
    - 0x430cc4: visibile, tramite 0x427648(v, i, b, 0);
    - 0x430d5c: posizione, tramite 0x427654;
    - 0x430d7c, 0x430d8c, 0x430e24, 0x430e58.
- **Oggetto "menu-kart" per slot** (0x147680.., `+0xc0` = slot = indice viewer, `+0xc4` = tipo di schermata):
  - tipo 0 → preset 0; tipo 1 → `(slot&1)+1`; **tipo 2 (Multi) → `{3,4,5,6}[slot]`** (0x131f478, scritta dall'init statico, letta a
    0x14771c **senza limite**); tipo 3 → 7;
  - preset da 0x74 byte a `0x131f450+0x38+k*0x74` (8 preset, costanti float scritte da 0x14a408), copiati in `this+0x8..+0x78`
    (posa/camera);
  - posizione da `arg+0x1c..+0x24` → `+0xec` → `0x427654(viewer, slot, +0xec)` (0x147850): piedistallo.
  - Con slot 4..7 la tabella legge i float del preset 0 come indice: oggi non succede solo grazie alle guardie (`Model3DSlotHook`, 0x14a038).
- **Le posizioni 2D non vengono dal layout.** Nel layout Multi `L_CameraPlay_0i` è solo l'icona "ruota camera": l'onCreate CharaParts la
  nasconde a 0x4bee84..0x4beeb0. La posizione a schermo viene dai **preset 3..6** (posa/camera per giocatore). Il dettaglio (offset di
  proiezione per quadrante o viewport per modello) è [IPOTESI] e va letto a runtime (tappa 0).

### 5.3 Memoria e costo
- [IPOTESI forte] KartWork nel menu è di 36 MB, la stessa classe di heap che in gara regge i KartUnit di tutti i kart. 8 KartUnit nel menu
  (personaggio + kart + gomme + aliante) dovrebbero starci.
- Ogni KartUnit costa 0x368 byte più le sue risorse.
- Il costo GPU è circa il doppio dei modelli del menu Multi.
- [IPOTESI] Se il menu disegna ogni modello in una viewport propria (preset = camera), servono 8 viewport anche nel menu. In quel caso si
  riusa l'infrastruttura dello step 2 (renderer "Main" a N viste 0x6a8f10, 0x69bb0c). Si verifica nella tappa 0 con i log viewport già
  presenti (hook 0x582c94 e SetViewport).

### 5.4 Piano a tappe (8 anteprime 3D)
| tappa | contenuto | stima |
|---|---|---|
| **0** ricognizione (Eden + gdb, solo log) | (1) trampolino di log su 0x13d778: `+0x180`, scena; (2) dump di `0x131f450` (+0x38, 8×0x74) e delle 4 voci viewer (+0x568..); (3) numero di viewport/camere attive nel menu Multi (log 0x582c94/SetViewport); (4) KartWork: spazio libero dopo 4 modelli. In più, subito: nascondere i 4 piedistalli a turno unico con `0x427648([0x398898()+0x270], i, false, 0)` in `p5::Hide3D` [IPOTESI: semantica da 0x14860c] | 0.5 sessione |
| **1** 8 KartUnit | inline su **0x13d7bc** (`b900c268`): con scena menu e opzione `MK8DX_P7_MENU3D8`, `w8 = max(w8, 8)`. Il KartDirector nasce prima della scelta del numero di giocatori, quindi l'opzione vale anche in 1-4P: nascondere i KartUnit 4..7 con `0x14a7ac(i,false)` (visivamente identico, +memoria). In alternativa ricreare la scena menu dopo la scelta 5-8 (più invasivo) | 0.5-1 |
| **2** viewer a 8 voci | le voci 4..7 in un buffer della mod. Si fanno REPLACE dei 6 accessori `(viewer, i)` (0x430cc4, 0x430d5c, 0x430d7c, 0x430d8c, 0x430e24, 0x430e58): con i ≥ 4 operano sul buffer. Init delle voci copiando lo schema del ctor (0x42fde8..0x42feb4) e creando 4 istanze `BaseTable` in più. Il punto di creazione dei modelli +0x570 si trova in tappa 0 | 1-2 |
| **3** posa/camera 8pl | inline su **0x14771c**/0x147734 (tipo 2, slot ≥ 4 o modo 8P): indice preset da una tabella della mod con 8 preset nuovi, derivati dai preset 3..6. Si ricalcolano offset e scala per (±480, 294/136/−22/−180) a circa 0.5 della Multi. Taratura a vista in Eden | 1-2 |
| **4** integrazione | tolte guardie e `Hide3D` quando i KartUnit sono 8. Restano: 0x13d4ac (personaggio), 0x14a038/0x13d47c (kart), PartsPresent `+0x238[4]` (p5 §, clamp4 → 8), 0x14a990 (finestra varianti → posa) | 0.5-1 |

- **Totale: 4-6 sessioni, rischio medio-alto.** Il nodo è la tappa 2/3: modelli e camere per indice, con possibile viewport per modello.
- **1-4P identico** solo se l'opzione è spenta, oppure accettando +4 KartUnit nascosti in memoria.

### 5.5 Ripiego consigliato per ora: icone 2D nei pannelli (1-2 sessioni, rischio basso)
- **Layout (LayeredFS, `../tools/sarcpatch.py`).** In `mn_CharaKartSlct_8pl` aggiungere sotto ogni `N_Window_0i` un `prt1 L_CharaIcon_0i` (parte
  `mn_L_CharaIcon_00`, già nella RomFS menu) a circa 0.6. Si può fare anche con un `pic1` 96×96.
- **Codice.**
  - Nella pagina nomi 0x12, a ogni cambio di personaggio (log A2 = 0x4203e0 / `0x13d4ac(mgr, slot, &pers)`, già intercettato) si imposta la
    texture faccia del personaggio sul `P_Img` dell'icona dello slot, con `0x8ada44(P_Img, tex, 0)`, come le celle della griglia.
  - Il getter della texture faccia usato dai pulsanti `L_Chara_XX` va estratto: è il percorso onCreate della griglia 0x4bee.., in tappa 0.
- **Pagina kart.**
  - Stessa icona più le icone delle 3 parti, già in `L_KartSlot_0i`.
  - Per i colori della targhetta non serve niente: c'è già L_Name.
- Con il ripiego si nascondono anche i piedistalli (tappa 0) e si tengono `Hide3D` e le guardie.

## 6. Tabella indirizzi (verificata, `p7_verify.txt`)
| tipo | indirizzo | parola | azione |
|---|---|---|---|
| hook esistente (I) | 0x469760 `CharaWinPosHook` | b90013e8 | nuovo corpo §1.3 (Ptn 2/3; ripristino 0x4694cc) |
| riserva (T) | 0x4694cc | b9406808 | solo se §1.3 non basta |
| hook esistente (T) | 0x8afdd0 `Cur8ActivateHook` | a9be4ff4 | §2.2.1 |
| hook esistente (T) | 0x8b0084 `CursorPollHook` | f81e0ff3 | §2.2.2 (installarlo sempre con `cur8::on`) |
| hook esistente (R) | 0x8b01ec `Cur8DrawHook` | f81e0ff3 | §2.2.3 (blocco p5fix2 con `AsWide`) |
| funzioni chiamate | 0x8ad3c4 SetFrame, 0x4694cc, 0x3fe658, 0x8af3f4, 0x8b00e0, 0x8b0238, 0x8ad4cc (diag) | — | — |
| B (futuro) | 0x13d7bc `b900c268`, 0x14771c `b9402908`, 0x42feac `f10ea31f`, 0x430cc4..0x430e58 | — | tappe 1-3 |

Nessun nuovo indirizzo per la parte A: si cambiano solo i corpi di 4 hook già installati. La capienza JIT non cambia.
