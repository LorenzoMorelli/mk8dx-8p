# Step 5b – passo 7b: il riquadro della finestra varianti di P5-P8 resta sulla griglia – piano statico

Data: 3 nov 2026. Solo analisi statica: `src/` non modificato, nessuna build.

**Basi usate**
- `../img/main.bin`, `../img/main.ann.asm`;
- osservazioni: `step5b_data/eden_5b8_riquadro_griglia.png`, `step5b_data/eden_log_5b8.txt` (righe `D:`);
- codice attuale: `src/program/main.cpp` (namespace `cur8`, `Cur8DrawHook`, `CursorPollHook`, `CwGfxCursorHook`, blocco "p5fix D").

**Verifica**: `step5b_data/p7b_verify.py` → `p7b_verify.txt`, **0 errori**.

Etichette: [VERIFICATO] = letto in main.bin; [IPOTESI] = da confermare in Eden.

## 0. Sintesi

**Causa [VERIFICATO].** Il riquadro di una finestra varianti **non lo posa `0x8ae628`**, la funzione degli item generici che ha il clamp
F5. Lo posa la finestra stessa:
- l'unico item del cursore della CharaList è la **parte finestra** (`parte+0x30`);
- il suo **vt+0x70 è `0x46c15c` → `0x46c0a4(parte, p)` → `0x4552b8(parte, p)`**;
- `0x4552b8` scrive la matrice globale del cursore grafico `cm_Page_Cursor+0x1d0[clamp4(p)]` con **due clamp `cmp w20,#4` non patchati**
  (0x4553f4 e 0x455468).

Per P5..P8 (p = 4..7) la posa finisce quindi sul **cm_L_BtnCursor_00 di P1**. Nello stesso frame `0x8b0238` **mostra** `cm_L_BtnCursor_04..07`
(gfx V, 0x8b083c → 0x442f5c), che però conserva l'ultima matrice ricevuta. Il flag 0x40 "matrice globale utente" fa sì che il layout non la
ricalcoli. Quella matrice è la cella della griglia su cui il giocatore aveva premuto A.

**Correzione.** Un nuovo hook inline, lo stesso callback su due indirizzi (0x4553fc e 0x455474): con p = 4..7 l'indice del grafico diventa p+4
(slot +0x210..). Non serve toccare nessun hook esistente. 1-4P identico: p < 4 non viene toccato e in vanilla `0x4552b8` non riceve mai p ≥ 4.

## 1. Catena [VERIFICATO]

### 1.1 Chi è l'item del cursore della CharaList
- onCreate CharaList (0x4b28a4):
  - `0x468ea0` costruisce la parte finestra (0x398 byte) e la salva in `CharaList+0x1d0` (0x4b2930);
  - `0x468ef4` usa `x25 = parte+0x30`, cioè l'item, e lo registra con `0x8af3c0(CharaList+0x1b0, x25)` a 0x469230 (`item+0x60` = cursore).
- Questo spiega `item=1` nel log D. I costumi **non** sono item del cursore: la finestra li gestisce internamente (cella `parte+0x168/+0x16c`).
- Vtable dell'item: GOT 0x1308ce0 = 0x126d8a8, +0xc8 (0x468ecc). Contiene:
  - **vt+0x60 = 0x46c1ec**, usato dal controllo "attivo?" a 0x8b0318;
  - **vt+0x70 = 0x46c15c**: `sub x0,x0,#0x30; b 0x46c0a4`.

  Per confronto, il pulsante generico (`L_ChangeBtn`, GOT 0x1308548) ha vt+0x70 = 0x8ae628.

### 1.2 Il disegno 0x8b0238 (cursore della CharaList, `AsWide` con V = s+4)
- `w21` = p **grezzo** (0x8b0268). Le patch F14 e `Cur8FocD` usano V solo per lo slot, il focus e il grafico `x20 = cp+0x1d0[V]`. Quindi
  `x20` = cm_L_BtnCursor_04..07 ed è corretto.
- Stato 4 → 0x8b0810..0x8b0820: `vt+0x70(item, w21)`, chiamato con **p grezzo = s** e non con V.
- Poi 0x8b083c: `0x442f5c(x20, …)` **mostra** il grafico V nel punto in cui si trova.

### 1.3 La posa della finestra: `0x46c0a4(parte, p)` → `0x4552b8(parte, p)`

| punto | cosa fa | stato con p = 4..7 |
|---|---|---|
| 0x46c0f8 | `bl 0x4552b8(parte, p)`: **posa** | **sbagliato** |
| 0x46c100/0x46c10c | grafico per `0x8ae048` (solo **alpha**) | già corretto (p5fix + `CwGfxCursorHook`) |
| 0x46c148 | `0x49abc4(cp, p, s)` (solo **scala**, se `parte+0x2d0 == 2`) | già corretto (G5, `Vg`) |
| **0x4552b8** | `x19 = cp` (0x4552e8), `w20 = p` (0x4552d4, mai riscritto). Cella scelta → posizione | — |
| 0x4553f0..0x4553fc | `sxtw x8,w20; cmp w20,#4; csel x8,x8,xzr,lo; add x19,x19,x8,lsl #3` | **clamp a 0** |
| 0x45540c/0x455418 | `0x8adfb0([x19+0x1d0]+0x30, mtx)`: copia la matrice globale e mette il flag 0x40 (0x8adfdc) | scrive su **gfx 00** |
| 0x45542c, 0x455444 | `0x443404` (dimensione) e `0x8ae048` (alpha) sullo stesso grafico | gfx 00 |
| 0x455464..0x455474 | ramo "nessuna cella" (`cbz x22` a 0x45533c): stesso clamp, poi solo alpha (0x455498) | gfx 00 |

Chiamanti di `0x4552b8`: solo 0x46c0f8 (finestra varianti) e 0x4554a0, il thunk vt+0x70 della parte base [VERIFICATO].

### 1.4 Perché in vanilla funziona e per P5-P8 no
- **P1..P4**: il cursore della CharaList è nello slot p, `vt+0x70(item, p)` → `0x4552b8(parte, p)` → `gfx[p]`. Lo stesso oggetto che il
  disegno mostra viene anche posato sul costume.
- **P5..P8**:
  - `0x8b0238` mostra `gfx[s+4]`;
  - `0x4552b8` posa `gfx[0]`, cioè quello di P1;
  - la matrice di `gfx[s+4]` resta quella scritta l'ultima volta da `0x8ae628` del cursore largo della selezione, cioè la cella della griglia.
- Tutto torna con il log D:
  - `g+c0=1`, perché 0x442f5c lo mette a 1;
  - `pane+58=d3`;
  - `draw` cresce di 2 per frame (P5 e P8);
  - `foc=1`.
- Tornano anche i colori delle due cornici nello screenshot: Tipo Timido rosso (frame 7 = P5) e Link arancio (frame 10 = P8).
- Effetto collaterale (prima della correzione): due finestre aperte (P5 e P8) si contendono `gfx 00` ogni frame (matrice e alpha). Non si vede
  perché P1 ha già deciso, ma è sbagliato.

### 1.5 Escluso: il cursore largo della selezione non ridisegna P5-P8 [VERIFICATO]
- Ordine dei disegni (0x8bda78..0x8bdad0): le pagine si scorrono dall'alto. Prima la CharaList (sopra), poi la selezione. Il ciclo
  0x8bdb5c..0x8bdbb4 vale solo per le pagine con `+0x194 ≠ 0`.
- Per la selezione, `0x8b0238(sel, 4)` con F14 dà V = 8 e `Cur8FocD` dà `foc[0]`, che è la CharaList (`foc=1`). La funzione **esce a
  0x8b0294** senza toccare il grafico.
- Tra i due disegni nessuno cambia `foc`: `FocusExtra` gira in 0x8bd6d8, non nel ciclo dei disegni. La condizione `foc[s-4] == CharaList` vale
  a ogni disegno della CharaList, come mostra il contatore `draw`.
- Nel codice del cursore gli unici scrittori di posizione del grafico sono:
  - vt+0x70 (0x8b07dc/0x8b0814; 0x8be33c è codice pad);
  - 0x8b0880 (animazione di decisione, stati 5/6).

  `0x8b0888` (F15) e `0x4433ac` fermano solo le animazioni. `CursorPollHook`, `Cur8DecidedHook`, `0x8af3f4` e `FocusExtra` non posano nulla.
- Scansione (sezione 6 dello script): tutti i `cmp #4 / csel xzr` che indicizzano `cm_Page_Cursor+0x1d0` dopo `bl 0x8bf228` sono coperti
  (F5, F10..F15, p5fix 0x46c100). Gli unici scoperti sono **0x4553f4 e 0x455468**.

## 2. Correzione

### 2.1 Nuovo hook (accanto a `CwGfxCursorHook`, in `main.cpp`)
```cpp
HOOK_DEFINE_INLINE(CwCellGfxHook) {   /* 0x4553fc add x19,x19,x8,lsl #3 e 0x455474 add x8,x19,x8,lsl #3 in 0x4552b8(parte, p):
                                         cursore grafico posato dalla finestra varianti, p 4..7 -> slot +0x210.. (il csel ha gia' azzerato x8) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t p = ctx->W[20];                         /* w20 = p (0x4552d4), invariato fino a entrambi i punti */
        if (p - 4u < 4u) { ctx->X[8] = p + 4; cur8::cwCell++; }
    }
};
```
- `cur8::cwCell`: contatore diagnostico, da dichiarare accanto a `cwAct` e `cwDraw` (`static int cwAct, cwDraw, cwCell;`).
- Le prime parole sono istruzioni non relative al PC [VERIFICATO]:
  - 0x4553fc `8b080e73`;
  - 0x455474 `8b080e68`.

  Il callback gira prima dell'istruzione originale, come `CwGfxCursorHook` su 0x46c10c.
- Nessuna patch di parola: i `cmp #4` restano, perché il callback riscrive `x8` dopo il `csel`.

### 2.2 Installazione (blocco `if (p4::on)` del passo 5, nel controllo `ok` e nell'installazione)
```cpp
        for (auto [o, v] : {std::pair<uint32_t, uint32_t>{0x4553f4, 0x7100129f}, {0x4553fc, 0x8b080e73},
                            {0x455468, 0x7100129f}, {0x455474, 0x8b080e68}})
            if (*reinterpret_cast<uint32_t*>(base + o) != v) { ok = false; Logging.Log("[mk8dx-8p] p7b: parola inattesa a %x", o); }
        ...
            CwGfxCursorHook::InstallAtOffset(0x46c10c); CwCellGfxHook::InstallAtOffset(0x4553fc); CwCellGfxHook::InstallAtOffset(0x455474);
```
- Servono gli slot grafici 04..07 a +0x210, creati da cur8 (0x49a968 + `Cur8GfxStoreHook`). È la stessa dipendenza di `CwGfxCursorHook`, che
  sta già in questo blocco.
- Si aggiungono due hook inline. Va considerata la capienza JIT (+2 trampolini).

### 2.3 1-4P identico
- Con p < 4 il callback non fa nulla.
- In vanilla `0x4552b8` riceve p da `0x8b0238` (sempre 0..3) e dal thunk 0x4554a0, quindi la mod cambia solo i casi p ≥ 4:
  - l'`AsWide` della finestra;
  - un eventuale cursore largo su una parte base. Oggi non esiste: gli item della griglia usano 0x8ae628 e funzionano.
- La pausa (`pause8`) disegna solo p < 4.

### 2.4 Risultato atteso in Eden
- All'apertura della finestra di P5 (P8) la cornice rossa (arancio) **sparisce dalla griglia** e compare sulla cella del costume corrente
  nella finestra.
- Si muove con il pad di P5 (P8) e alla conferma si comporta come per P1.
- Con P5 e P8 aperti insieme le due cornici sono indipendenti.
- Il cursore di P1 (gfx 00) non viene più spostato né sfumato dalle finestre 5-8.

## 3. Log diagnostico (solo se in Eden il riquadro resta fuori posto)
Nel blocco D di `CharselFrameHook` (ogni 60 frame, finestra aperta), dopo le due righe attuali:
```cpp
auto Tr = [](uint8_t* g, int k) -> float {            /* traslazione globale del pane radice del grafico: [[g+0x40]+0x18]+0x7c/+0x8c */
    uint8_t* l = g ? *(uint8_t**)(g + 0x40) : nullptr; uint8_t* pn = l ? *(uint8_t**)(l + 0x18) : nullptr;
    return pn ? *(float*)(pn + 0x7c + 0x10 * k) : 0.0f; };
uint8_t* g0 = cur8::Gfx(0); uint8_t* gw = cur8::Gfx(4 + j);
Logging.Log("[mk8dx-8p] D7b: P%d cell=%d gfx0=(%.0f,%.0f) gfx%d=(%.0f,%.0f) win=(%.0f,%.0f) cella=%d,%d", 5 + j, cur8::cwCell,
            Tr(g0, 0), Tr(g0, 1), 4 + j, Tr(gw, 0), Tr(gw, 1), Tr(part, 0), Tr(part, 1),
            part ? *(int32_t*)(part + 0x168) : -1, part ? *(int32_t*)(part + 0x16c) : -1);
```
- `Tr(part)` legge `[part+0x40]+0x18` = `[item+0x10]+0x18` (item = `part+0x30`), il pane radice della finestra, come 0x8adf40. Per i
  grafici `g+0x40` = `(g+0x30)+0x10`, stessa struttura.
- Atteso dopo la correzione:
  - `cell` cresce di 1 per finestra aperta a ogni frame;
  - `gfx{4+j}` vicino a `win` (entro mezza finestra), cambia con la cella.

  Se `cell` resta 0, il ramo stato 4 → vt+0x70 non viene raggiunto: controllare `vt+0x60` = 0x46c1ec e `0x8adf40` dell'item.
- Prima della correzione: `gfx0` segue la finestra e `gfx{4+j}` resta fermo sulla cella della griglia.

## 4. Tabella indirizzi (verificata, `p7b_verify.txt`)
| tipo | indirizzo | parola | azione |
|---|---|---|---|
| nuovo (I) | 0x4553fc `CwCellGfxHook` | 8b080e73 | `x8 = p+4` per p 4..7 (ramo con cella) |
| nuovo (I) | 0x455474 `CwCellGfxHook` | 8b080e68 | idem (ramo senza cella, solo alpha) |
| controllo | 0x4553f4, 0x455468 | 7100129f | clamp lasciati invariati |
| riferimento | 0x46c15c (vt+0x70 item finestra), 0x46c0a4, 0x4552b8, 0x8b0814/0x8b081c (vt+0x70 con w21 grezzo) | — | — |
