# Step 8 – il riquadro di P5-P8 resta visibile dopo la chiusura della finestra varianti – analisi statica

Data: 6 nov 2026. Solo analisi statica: `src/` non modificato, nessuna build, niente Eden/gdb.

**Basi usate**
- `../img/main.bin`, `../img/main.ann.asm`;
- `src/program/main.cpp`: `cur8` (`V`, `Gfx`, `SetFoc`, `FocusExtra`, `CwSlot`, `AsWide`), `Cur8ActivateHook`, `Cur8DrawHook`,
  `CursorPollHook` (passo 7 b), `Cur8SetFocusHook`, `Cur8FocusFrameHook`, il blocco "p5fix D", `CwGfxCursorHook`/`CwCellGfxHook`;
- `step5b_plan_p7b.md` (posa del riquadro nella finestra, correzione 0x4553fc/0x455474 già applicata).

Etichette: [VERIFICATO] = letto nel disassemblato e parola controllata in main.bin; [IPOTESI] = da confermare in Eden.

## 0. Sintesi

**Il riquadro di P1-P4 non viene "nascosto" da una chiamata di hide alla chiusura.** Nel gioco originale l'ultimo atto sul cursore grafico
è il **decide** della finestra: `0x8affa4(cursore CharaList, p)`, chiamato a 0x4b3f34 quando si sceglie il costume. Riproduce sul grafico
`cm_L_BtnCursor_0p` l'animazione **Decide** e spegne il Loop (`g+0xc0 = 0`). Dopo, sia la CharaList (fino alla chiusura) sia la selezione
vedono lo slot nello stato 7 e non toccano più il grafico. [VERIFICATO]

**Con la mod, per p = 4..7:**
- il decide gira **fuori da `AsWide`**: il cursore della CharaList è a 4 slot, quindi il clamp F11 (`cur8::V` = 0 per un cursore non largo)
  dà `x21 = 0`;
- `0x8affa4` lavora sullo slot 0 (giusto: è il deposito dello stato di P5..P8 nella finestra);
- ma usa anche **gfx[0]**, cioè il riquadro di P1, invece di gfx[s+4]. [VERIFICATO]
- Il riquadro di P5..P8 (gfx 04..07), mostrato con "In + Loop" dal disegno `AsWide` sulla cella della finestra, non riceve mai il Decide e
  resta visibile dov'era. Il riquadro di P1 invece riceve un Decide spurio. [VERIFICATO il percorso; IPOTESI che sia l'animazione Decide a
  far sparire il riquadro, vedi §2.3]

**Correzione** (stesso passo del gioco, esteso a 4..7, nello stesso stile del passo 7 b):
- un hook TRAMPOLINE su `0x8affa4` (parola `f81d0ff5`);
- se il cursore è quello di una CharaList degli slot 4..7 (`cur8::CwSlot(c) == p`), il decide originale gira dentro
  `cur8::AsWide(c, p, …)`, esattamente come già succede per attivazione, lettura pad e disegno;
- in questo modo F11 dà V = p+4: slot V, che è la copia dello slot 0, e gfx V = cm_L_BtnCursor_04..07.

Con 1-4 giocatori non cambia nulla: `CwSlot` vale -1.

## 1. Percorso originale di apertura e chiusura della finestra varianti (P1-P4) [VERIFICATO]

| passo | dove | cosa fa sul cursore/grafico |
|---|---|---|
| A su un personaggio con varianti | cursore della selezione → `0x8affa4(selCur, p)` (F11) | slot p: 4→7 (5→6). gfx[p]: `0x442f5c(g,1)` (In+Loop) + `0x443320(g)` (Decide, `g+0xc0=0`) |
| evento tipo 2 alla selezione | 0x4bc014..0x4bc088 | `sel+0x904[p] = 5`, `sel+0x1a8 &= ~(1<<p)`, apre la CharaList 0x28+p (link `+0x660+p*0x28`, 0x4bc640/0x4bc648) |
| onEnter CharaList | 0x4b3160..0x4b3194 | `0x8bd5f8(mgr, p, CL)`: la selezione perde il focus (`0x8ba624`→`0x8aff64`: 6→7, 5→4, nessun grafico), la CL lo prende (`0x8ba614`→`0x8aff4c`: `cur+0xb0 \|= 1<<p`); `CL+0x1a8 \|= 1<<p` |
| finestra aperta | disegno `0x8b0238(clCur, p)` stato 4 | `vt+0x70` (0x46c15c→0x46c0a4→0x4552b8) posa gfx[p] sulla cella; `0x442f5c` (0x8b083c) lo mostra |
| **costume scelto** | **0x4b3f2c..0x4b3f34**: `0x8affa4([CL+0x1b0], [CL+0x1e8])` | **slot p: 4→7. gfx[p]: `0x442f5c(g,1)` (0x8b0000) + `0x443320(g)` (0x8b0008)**. Poi `CL+0x2d0 = 0`, `+0x184 = 0x19`, link di ritorno `0x8ba5fc(CL, CL+0x2a8)` (0x4b3f50..0x4b3f64) |
| uscita CL | 0x8bb8b8..0x8bb90c | attende che gli slot 0..3 non siano in 5..6, poi `vt+0x268` |
| disegno durante l'uscita | `0x8b0238`, stato 7 | 0x8b034c→0x8b078c: solo dimensione (0x443404) e 0x442c7c; a 0x8b0800 `stato\|1 == 7` → salto a 0x8b0840: **né posa, né mostra, né nasconde** |
| CL tolta dalla lista | `0x8bd6d8` 0x8bd890..0x8bd960 | focus p → pagina più alta con bit p in `+0x190` (selezione): `0x8ba624(CL,p)` (no-op su 7), `0x8ba614(sel,p)` |
| disegno della selezione | `0x8b0238(selCur, p)`, slot p = 7 | stesso ramo: nessuna azione sul grafico |

**Chi nasconde il riquadro in generale.** Le uniche chiamate alle funzioni di hide sono queste [VERIFICATO, grep `bl/b 0x44316c|0x443290`]:
- `0x8b0490` / `0x8b04c4` in `0x8b0238`: cursore senza item, oppure stato < 4, oppure item non attivo con stato ≠ 6/7;
- `0x8bd888` in `0x8bd6d8`: pad senza nessuna pagina col bit p e senza focus precedente.

Alla chiusura dopo un decide nessuna delle tre scatta, perché lo stato è 7 e il focus passa alla selezione. Quindi nel gioco originale
**l'unico intervento sul grafico alla conferma del costume è `0x443320` dentro `0x8affa4`**. L'indice è `clamp4(p)` = p per P1-P4.

Le animazioni del grafico sono definite in `0x442ae4` (stringhe in main.bin) [VERIFICATO]:
- gruppo 0 = "In"(0) / "Out"(1);
- gruppo 1 = "Decide"(0) / "Loop"(1);
- poi "SizeChange" e "ColorChange".

Le funzioni che le usano:
- `0x442f5c(g,1)` = In + Loop, `c0 = 1`;
- `0x443290` = Out + Loop fermo, `c0 = 0`;
- `0x443320` = se il gruppo 0 è su In: gruppo 0 = In, gruppo 1 = Loop al frame 0 e poi **Decide da 1.0** (0x8ad300), `c0 = 0`.

## 2. Perché per P5-P8 il riquadro resta

### 2.1 Il decide della finestra 4..7 usa gfx[0] [VERIFICATO]
- Con la mod, `CL+0x1e8 = s` (4..7) (`CharaListSlotHook` 0x4b28d8). Il cursore della CL è a 4 slot (`+0xbc = 4`, magic 'CUR8').
- `0x8affa4(c, s)`:
  - 0x8affcc `cmp w20,#4` è patchato a `#8`, con il PRE `Cur8ClampF11`: `x8 = cur8::V(c, s)`;
  - il cursore non è largo, quindi **V = 0**;
  - 0x8affd0 `csel x21, x8, xzr, lo`: `x21 = 0`.
- 0x8affd8/0x8affdc: `x20 = [cp + 0x1d0 + 0*8]` = **cm_L_BtnCursor_00 (P1)**.
- 0x8affe4..0x8b0008: lo slot 0 è nello stato 4. Il codice mostra **gfx 00** e gli fa il Decide; slot 0: 4→7 (0x8b000c..0x8b0020).
- Lo **slot** 0 è giusto: è dove `AsWide` riporta lo stato di P5..P8 (`memcpy(s0, sv)` all'uscita). Il **grafico** no: il disegno `AsWide`
  (`Cur8DrawHook`, F14 con V = s+4) ha mostrato gfx[s+4] (0x8b083c), che resta "In + Loop" (`g+0xc0 = 1`, Loop guidato da
  `Cur8GfxLoopHook`) nell'ultima posizione: la cella del costume nella finestra.
- Dopo il decide nessuno lo tocca più:
  - disegno della CL in stato 7: nessuna azione (§1);
  - `SetFoc(p, selezione)` da `FocusExtra`: `OwnsSlots(CL)` è falso, quindi niente `0x8ba624`; `0x8ba614(sel, p)` agisce solo su `+0xb0`;
  - disegno della selezione, slot V = 7: nessuna azione.

  Il riquadro rimane quindi sullo schermo finché lo slot non viene riattivato (B / ritorno).

### 2.2 Gli altri passi della CL con p = s sono già a posto [VERIFICATO]

| chiamata nella classe CharaList | indice gfx con la mod | stato |
|---|---|---|
| 0x4b3058 `0x8b0888(cur, s)` (F15, ferma l'animazione) | `Vg(s)` = s+4 se `wideCur` | ok |
| 0x4b317c `0x8bd5f8(mgr, s, CL)` | `Cur8SetFocusHook` → `SetFoc` (CL non largo → `0x443290(Gfx(s))`, poi il disegno `AsWide` lo rimostra) | ok |
| attivazione / lettura pad / disegno | `Cur8ActivateHook` / `CursorPollHook` / `Cur8DrawHook` dentro `AsWide` | ok |
| posa nella finestra 0x4552b8 / alpha 0x46c10c / scala 0x49abc4 | `CwCellGfxHook` / `CwGfxCursorHook` / G5 | ok |
| **0x4b3f34 `0x8affa4(cur, s)` (decide)** | **F11 → V = 0 → gfx 00** | **sbagliato** |

Altri chiamanti di `0x8affa4`: solo 0x435bf0 (`9411e8ed`, altra classe) [VERIFICATO]. In 0x8affa4..0x8b0030 nessun altro clamp.

### 2.3 Riserve [IPOTESI]
1. **Che il Decide faccia sparire il riquadro.**
   - Si deduce dal fatto che per P1-P4 sparisce e che nel codice non c'è nessun altro hide su questo percorso (§1).
   - Il bflan "Decide" di `cm_L_BtnCursor_00` non è stato aperto. Se finisse visibile, il riquadro di P1 resterebbe anch'esso sulla
     finestra, il che contraddice l'osservazione dell'utente.
2. **Il decide va chiamato fuori da `AsWide`.**
   - 0x4b3f34 sta nel gestore eventi della pagina CL (`0x4b31fc` = vt+0x2e0, vtable a 0x1278048, puntatore a 0x1278328);
   - quel gestore è chiamato da `0x8bbef4`, che a sua volta è chiamato dalla consegna messaggi/link 0x8b2020..0x8b2fbc del gestore pagine;
   - non è chiamato dalla lettura pad 0x8b00e0 (che finisce in `0x8af3f4`).
   - Se invece fosse dentro `AsWide`, `CwSlot` varrebbe -1 (cursore temporaneamente largo) e F11 darebbe già V: il difetto non verrebbe
     da qui. La riga di log del §4 distingue i due casi.
3. **Effetto collaterale attuale.** A ogni costume scelto da P5..P8, il riquadro di P1 (gfx 00) riceve `0x442f5c` + Decide nella sua ultima
   posizione: un lampo del riquadro di P1. Sparisce con la correzione.

## 3. Correzione proposta (flusso originale con N = 8)

### 3.1 Hook
```cpp
HOOK_DEFINE_TRAMPOLINE(CwDecideHook) {      /* 0x8affa4(cur, p) decide; dalla CharaList a 0x4b3f34 con p = CL+0x1e8 = 4..7 (slot 0 come V = p+4) */
    static void Callback(uint8_t* c, int32_t p) {
        int s = cur8::CwSlot(c);
        if (s >= 4 && p == s) {                                   /* finestra varianti degli slot 4..7: stesso decide sullo slot/gfx V */
            int32_t before = *(int32_t*)cur8::Slot(c, 0);
            cur8::AsWide(c, s, [&] { Orig(c, p); });
            cur8::cwDec++;
            uint8_t* g = cur8::Gfx(s);
            Logging.Log("[mk8dx-8p] p8v: decide finestra P%d slot0 %d->%d gfx%d=%p c0=%d", s + 1, before,
                        *(int32_t*)cur8::Slot(c, 0), s, g, g ? g[0xc0] : -1);
            return;
        }
        if (*(uint32_t*)(c + 0xb8) == cur8::kMagic && *(int32_t*)(c + 0xbc) == 8 && cw8::SlotOfId(*(int32_t*)(*(uint8_t**)(c + 8) + 0xe8)) >= 4)
            Logging.Log("[mk8dx-8p] p8v: decide finestra P%d DENTRO AsWide (riserva 2)", p + 1);   /* solo diagnostica */
        Orig(c, p);
    }
};
```
- `cur8::cwDec` è un contatore diagnostico: va aggiunto a `static int cwAct, cwDraw;` (riga 2227) come `cwAct, cwDraw, cwDec;`.
- La seconda riga di log (cursore della CL temporaneamente largo) serve solo a chiudere la riserva 2 e si può togliere dopo la prova.
- `cw8` è definito dopo `cur8`: l'hook va messo dopo `namespace cw8`, per esempio accanto a `CwGfxCursorHook` (riga ~4155).

**Cosa fa dentro `AsWide`** [VERIFICATO sul codice di 0x8affa4 e di `AsWide`]:
- `c+0xbc = 8`, quindi `Wide(c)` è vero e F11 dà `x8 = V = s+4`;
- `cmp w20,#8` con p = 4..7 → `lo` → `x21 = V`;
- 0x8affd8: `cp + 0x1d0 + V*8` = `cp + 0x210 + (s-4)*8` = **cm_L_BtnCursor_04..07**;
- 0x8affe4 / 0x8b000c: `c + 0x28 + V*0x20` = `Slot(c, s)`, che contiene la copia dello slot 0 → stato 4→7 (5→6);
- all'uscita `AsWide` ricopia lo slot V nello slot 0 e ripristina V e `+0xbc = 4`.

**Registri / ABI.** È un trampolino a inizio funzione, non un inline:
- legge `x0` (cursore) e `w1` (p);
- non scrive registri del chiamante: `0x8affa4` è `void` (il chiamante a 0x4b3f38 ricarica `w2` da `[x19+0x1e8]`);
- prima istruzione `f81d0ff5` `str x21,[sp,#-0x30]!`, non relativa al PC: si può spostare nel trampolino;
- il PRE inline F11 a 0x8affcc resta dov'è e lavora dentro `Orig`.

### 3.2 Installazione
Nel blocco del passo 3 (`cur8`), dove sono installati `Cur8ActivateHook`/`Cur8DrawHook` (righe ~5270-5277):
- aggiungere la parola a `cur8::kHookWords`: `{0x8affa4, 0xf81d0ff5},   /* T decide (finestra varianti 4..7, step 8) */`;
- installare: `CwDecideHook::InstallAtOffset(0x8affa4);`.

In alternativa va nel blocco p5, accanto a `CwGfxCursorHook::InstallAtOffset(0x46c10c)` (riga 5383), con un controllo
`*(uint32_t*)(base + 0x8affa4) == 0xf81d0ff5`.

Costo: +1 trampolino (capienza JIT già portata a 0x8000).

### 3.3 Tabella delle parole (verificate in main.bin)
| tipo | indirizzo | parola | ruolo |
|---|---|---|---|
| nuovo (T) | 0x8affa4 | f81d0ff5 | `CwDecideHook` |
| riferimento | 0x8affcc / 0x8affd0 | 7100129f / 9a9f3115 | F11 (`cmp` patchato a `#8` + PRE) e `csel x21` |
| riferimento | 0x8affd8 / 0x8affdc | 8b150c08 / f940e914 | gfx = `[cp+0x1d0+x21*8]` |
| riferimento | 0x8affe4 / 0x8b0020 | 8b151668 / b9002909 | slot x21 e scrittura dello stato |
| riferimento | 0x8b0000 / 0x8b0008 | 97ee4bd7 / 97ee4cc6 | `bl 0x442f5c` / `bl 0x443320` (Decide) |
| riferimento | 0x4b3f2c / 0x4b3f30 / 0x4b3f34 | f940da60 / b941ea61 / 940ff01c | `x0 = CL+0x1b0`, `w1 = CL+0x1e8`, `bl 0x8affa4` |
| riferimento | 0x435bf0 | 9411e8ed | unico altro chiamante (altra classe, `CwSlot` = -1) |

### 3.4 Cosa non cambia
- **1-4 giocatori e P1-P4 in 5-8P:**
  - `CwSlot` vale -1 per i cursori delle CharaList 0x28..0x2b, dei cursori larghi e di ogni altra pagina, e se `p4::one` è falso;
  - anche con `CwSlot ≥ 4` serve `p == s`.

  Quindi `Orig(c, p)` gira identico.
- Uscita con **B** dalla finestra (0x4b3254, senza decide): non toccata. La selezione riattiva lo slot V (0x4b5610 → `0x8ba814` G2) e
  il riquadro torna sulla griglia come oggi.
- Nessuna parola patchata, nessun clamp in più: si estende a 4..7 il passo che il gioco fa per 0..3, nello stesso modo
  (`AsWide`) già usato per attivazione, lettura pad e disegno della stessa finestra.

## 4. Verifica in Eden (build `MK8DX_LOG=1`)
Prova: 5 giocatori, P5 sceglie Yoshi → finestra varianti → sceglie un colore.

Atteso nel log, una riga per costume scelto:
`p8v: decide finestra P5 slot0 4->7 gfx4=0x... c0=0`

- `c0=0` = il Decide è partito sul riquadro di P5;
- `gfx4` deve essere il puntatore di cm_L_BtnCursor_04 stampato da `cur8: cursori grafici …` (`Cur8GfxCreateHook`).

Atteso a video:
- alla conferma il riquadro rosso di P5 fa l'animazione di conferma sulla cella della finestra e sparisce come quello di P1;
- il riquadro di P1 non lampeggia più.

Casi diagnostici:
- **Nessuna riga `p8v`** → il decide non passa da 0x8affa4 con il cursore della CL, oppure `CwSlot` fallisce: controllare `p4::one` e
  `cw8::ready`.
- **Riga "DENTRO AsWide"** → riserva 2: il decide era già sul grafico giusto e la causa è un'altra.
- **Riga presente, `c0=0`, ma il riquadro resta** → riserva 1: il Decide non nasconde. In questo caso confrontare con P1 aggiungendo una
  riga sullo stato dei gruppi animazione del grafico (`[[g+0x70]]+0x28`, con 0 = In) nel blocco D di `CharselFrameHook`, prima di
  cercare altrove.

Regressione minima: 4 giocatori, P1 con Yoshi → nessuna riga `p8v`, comportamento identico a prima.
