# Step 7 – cursore della pausa per i giocatori 5-8 e colore dell'evidenziazione (analisi statica, main 4.0.1)

Base: `../img/main.bin` (offset = indirizzo), `../img/main.ann.asm`; confronto `../v305/main.ann.asm`; RomFS `../romfs-tmp/v4|v3/common.sarc`.
Estratti in `step7_data (dati non pubblicati)/c_*.asm` e `c_btncursor_colorchange.txt`. `src/` non modificato.
Etichette: [VERIFICATO] = letto nell'asm / parole lette da main.bin / dati RomFS; [IPOTESI] = da confermare in Eden.

Riferimento sintomi (log Eden): l'hook P4 (0x53e404) gira per pauser P5..P8, maschera 0x800000ff, 2 pulsanti; (a) solo con pauser
P1-P4 il cursore Continua/Abbandona si muove; (b) l'evidenziazione è "quasi sempre rossa".

## 0. Risposta breve

| | causa | certezza |
|---|---|---|
| (a) movimento | il cursore **non riceve eventi**: legge i pad da solo in `0x8b0084` (aggiornamento per frame della pagina), **srotolato sui pad 0..3** (`0x8b00e0(cur,0..3)`); i pad 4..7 non vengono mai letti. Anche la tabella del focus `mgr+0xc68` ha 4 voci (ciclo `cmp x20,#4` a 0x8bd96c) e lo stato per pad è `cur+0x28+min(pad,0→)·0x20` (clamp `pad<4 ? pad : 0`) | [VERIFICATO] |
| (a) conferma (A) | la conferma passa dallo scanner del pulsante (0x8b4038 → 0x8b4174) ma è condizionata da `0x8ae21c(item,pad)`: slot `clamp(pad)` in stato ≥3 **e** indice dello slot == indice del pulsante. Per pad ≥4 lo slot è lo **0**, che il nostro hook P4 rimette subito in stato 2 (chiama `0x8afaf0(cur,i)` per i non-pauser 4..7 → clamp → slot 0): con pauser P5..P7 A non fa nulla; con P8 A funziona solo su Continua (indice iniziale) | [VERIFICATO] |
| (a) effetto collaterale | lo stesso `0x8afaf0(cur,4..7)` dell'hook P4 azzera lo slot 0 anche quando il pauser è **P1**: in 5-8P P1 mette in pausa ma perde il cursore attivo (stato 2: serve un tasto per riattivarlo) | [VERIFICATO] codice, effetto a video [IPOTESI] |
| (b) colore | l'evidenziazione è il cursore grafico `cm_L_BtnCursor_0i` della pagina globale `cm_Page_Cursor` (componente 13 del UI manager, `[0x8bf228()]+0x1d0+i·8`), **uno per pad 0..3**; il colore è il frame `i+1` del gruppo `ColorChange`, impostato **una sola volta** alla creazione (0x49a9c4 `bl 0x442ca0`, unico chiamante). Ogni sito sceglie l'oggetto con `clamp(pad)` → per pad ≥4 si usa il cursore di P1 (giallo). Frame 4.0.1: 1 giallo, 2 ciano, 3 **rosso** (255,65,65), 4 verde, 5 rosso, 6 blu, 7 rosso, 8 blu, 9 viola, 10 arancio | meccanismo [VERIFICATO] |
| (b) "rosso" | nessun percorso del main produce un frame >4: un cursore rosso visibile può essere solo l'oggetto 2 (P3). Per pauser P5..P8 il main mostra il giallo di P1 (o nessun cursore attivo, vedi sopra), mai il colore del giocatore. Il "quasi sempre rosso" non si spiega staticamente → diagnostica §5 | [IPOTESI] |
| novità 4.0.1 | `cm_Page_Cursor` 4.0.1 ha **8** parti `cm_L_BtnCursor_00..07` (3.0.5: 4) e `ColorChange` arriva al frame **10** (3.0.5: 6). Il main 4.0.1 cicla sui nomi 07..00 (0x49a958) ma crea solo 0..3 e **nasconde 04..07** (0x49a9cc); il 3.0.5 crea 4 cursori srotolati senza ciclo. Frame 7..10 = rosso, blu, viola, arancio = i colori P5..P8 dei pannelli `rc_Pause_SgOunce_00` | [VERIFICATO] dati e codice; corrispondenza frame↔P5..P8 [IPOTESI forte] |

**Correzione proposta (minima, solo pausa, solo con `view8::split`):**
1. **Modifica dell'hook P4** (0x53e404): per i non-pauser 4..7 aggiornare solo le maschere, **niente `0x8afaf0`**; per pauser ≥4 chiamare
   `0x8af3f4(cur, p, …)` (usa lo slot 0, libero perché P1 non è il pauser) e **ricolorare il cursore grafico 0** con il frame `7+(p-4)`.
2. **TRAMPOLINE 0x8b0084** (`f81e0ff3`): se la pagina del cursore è Race_Pause (0x6f) aperta da un pauser p ≥4: durante `Orig` spegnere
   temporaneamente il bit 0 di `page+0x1a8` (P1 non deve pilotare lo slot 0 condiviso), poi chiamare `0x8b00e0(cur, p)`.
3. **Ripristino del colore** (frame 1) all'uscita dalla pausa: nel `PageLinkHook` esistente (0x8ba5fc) quando `page+0xe8 == 0x6f`.

Nessun sito di clamp viene toccato; in 1-4P e in tutti gli altri menu il comportamento è identico (vedi §4).

## 1. La classe cursore (pagina+0x1b0) [VERIFICATO] — `c_cursor_*.asm`

Layout dell'oggetto (dai siti di 0x8af3f4, 0x8afc54 init, 0x8b00e0):

| offset | significato |
|---|---|
| +0x8 | pagina proprietaria |
| +0x10 / +0x18 | numero di voci / array di item (item = pulsante+0x30; item+0x58 = maschera pad del pulsante = pulsante+0x88, bit31 attivo; item+0x68 = indice; item+0x78 visibile; item+0x60 = cursore) |
| +0x20 | navigatore (vt+0: `next(idx, tasti)`) |
| +0x28 + s·0x20, s = 0..3 | **slot per pad**: +0 stato (0 nessuno, 1/2 inattivo, 3/4 attivo, 5..7 deciso), +4 flag, +8 indice corrente, +0xc, +0x10 indice precedente, +0x14..+0x1c posizione |
| +0xa8 / +0xac / +0xb0 | maschera tasti di movimento / di riattivazione / bit per pad (azzerato da 0x8b01ec) |

Lo slot 3 finisce a +0xa8 e +0xa8.. sono le maschere: **l'array non si può estendere sul posto** (servirebbe riallocare il cursore e
toccare 15+ siti, più la tabella globale dell'ultimo indice per pagina `[[0x1306e70]+0x60] + idPagina·16`, 4 × s32 per pagina, indice
`clamp(pad)` a 0x8af55c..0x8af5d0 / 0x8afbec / 0x8afc54). Scartato.

Funzioni:

| funzione | ruolo | per pad ≥4 |
|---|---|---|
| **0x8b0084** (`f81e0ff3`) | per frame, chiamata da 0x8bc45c con `page+0x1b0`: `0x8b00e0(cur, 0..3)` **srotolato** | **mai chiamata con 4..7** ← causa (a) |
| 0x8b00e0 (`f81c0ff7`) | `mgr=0x8bdc8c(); if (mgr+0xc68[clamp(p)] != page) return; if (!0x8ba634(page,p)) return; s=slot[clamp(p)]; if (s.stato ∈ 5..7) return; pad=0x8bf158(p); t=pad+0x14;` stato 2: `t & cur+0xac` → `0x8af3f4(cur,p,s.idx,t,1,0)`; stato 3/4: `t & cur+0xa8` → `n = nav->next(s.idx,t)` → `0x8af3f4(cur,p,n,t,1,0)` | funzionerebbe (clamp → slot 0, `0x8bf158(p)` → pad extra con la mod) |
| 0x8af3f4 (`d10303ff`) | seleziona: controlla bit `1<<p` (non clampato) in page+0x1a8, slot `clamp(p)` (0x8af474/0x8af490), `0x8af87c` verifica il bit p nella maschera dell'item, imposta stato 4, evento focus-in all'item (vt+0x58, ev+0x2c = p), `[0x8bf09c()]+0x30 |= 1<<p` (azzerato ogni frame a 0x8bf06c), animazione `In` del cursore grafico `clamp(p)` | slot 0 |
| 0x8afaf0 (`d101c3ff`) | "rilascia": se bit p in page+0x1a8, slot `clamp(p)` (0x8afb48): stato → 2 + evento focus-out | **slot 0** ← colpisce P1 / il pauser |
| 0x8b01ec (`f81e0ff3`) | per frame (0x8bdac0/0x8bdba4): `0x8b0238(cur, 0..3)` srotolato, poi `cur+0xb0 = 0` | lo slot 0 viene disegnato come pad 0 |
| 0x8b0238 (`d10403ff`) | disegno: se `mgr+0xc68[clamp(p)] == page` e stato slot ≥4 (0x8b0310): cursore grafico `clamp(p)` posizionato sull'item (0x8ae628 = item vt+0x70), animazioni | — |
| 0x8ae21c (`f9403009`) | `item è selezionato dal pad p` = slot `clamp(p)` stato ≥3 && slot.idx == item+0x68 | slot 0 |
| 0x8affa4 / 0x8b0034 / 0x8aff64 / 0x8aff4c / 0x8afbec | animazione decide (clamp 0x8affcc) / stato 6,7 → 5,3/4 (0x8b0040) / perdita focus (0x8aff70) / bit +0xb0 / ultimo indice (0x8afc1c) | slot 0 |

Gli altri `cmp #4` citati non sono clamp: 0x8af620 (`stato == 4`), 0x8afff0 (`(stato & ~1) == 4`), 0x8b0310 (`stato >= 4`);
`sub #5; cmp #2/#3` = "slot in stato deciso 5..6 / 5..7".

**Focus per pad** [VERIFICATO] — `c_focus_mgr_8bd890.asm`: UI manager (0x8bdc8c, oggetto 0xda0 B ctor 0x8bcd80) `+0xc68` = 4 puntatori
(memset 0x70 a 0x8bce10). Ogni frame 0x8bd890..0x8bd970, per pad 0..3 (`cmp x20,#4` 0x8bd96c, `f100129f`), assegna la pagina più in alto
della lista con bit pad in `page+0x190` (0x8bd920), chiamando 0x8ba624/0x8ba614 (perdita/acquisto focus). `page+0x190` = `0xffffffff` di
default (ctor base 0x8ba3b8 `mov w9,#-1` → 0x8ba43c). Quindi in pausa `mgr+0xc68[0]` = pagina pausa [IPOTESI forte: è in cima alla lista].

**Conferma (A)** [VERIFICATO] — `c_btn_events_435a5c.asm`, `c_scanner_event_8b4174.asm`: pulsante (0x3d0 B, ctor 0x434d90, vtable
[0x13084e8]+0x10, item a +0x30) con scanner proprio a +0xd8 (classe 0x8b4010, registrato sull'item con 0x8ae9cc). Scanner 0x8b4038:
`0x8ae0d0(item,p)` (bit p nella maschera +0x88, bit p in page+0x1a8) → 0x8b4174: `+0x24 & trig` (=1, scritto a 0x43552c) →
**`0x8ae21c(item,p)`** → evento → item vt+0x58 = 0x435a5c → vt+0x90 = 0x435ba0 (decide: `0x8affa4(cur,p)`, link +0x158 con 0x8ba5fc).
Con la mod lo scanner copre già i pad 4 (nativo) e 5..7 (BtnScanHook): manca solo lo slot del cursore in stato ≥3 sul pulsante giusto.

## 2. Perché oggi con pauser P5..P8 il cursore è fermo [VERIFICATO]

Sequenza in vt75 0x53e0bc + hook P4 con pauser p (es. 4 = P5), multi (`pause+0x51d = 1`):
1. ciclo originale i = 0..3 (non pauser): `0x8afaf0(cur,i)` → slot i stato 2; il bit i resta in `page+0x1a8` (pannelli).
2. hook, i = 4 (pauser): maschere +0x88 |= bit 4, `0x8af3f4(cur,4,btn0.idx,0,1,0)` → **slot 0** stato 4, focus-in, cursore grafico 0 (giallo).
3. hook, i = 5,6,7 (non pauser): `0x8afaf0(cur,i)` → clamp → **slot 0** stato 2 + focus-out. Il cursore del pauser è spento.
4. ogni frame `0x8b0084` legge solo i pad 0..3: P5 non può né muovere (pad 4 mai letto) né riattivare; A passa lo scanner ma
   `0x8ae21c(item,4)` vede lo slot 0 in stato 2 → nessuna conferma. Con pauser P8 il passo 3 non avviene dopo il 2 (i = 7 è l'ultimo):
   slot 0 in stato 4 su Continua → A conferma Continua, il movimento resta impossibile.
5. con pauser **P1** (p = 0) il passo 3 vale per i = 4..7 → lo slot 0 di P1 torna a 2 subito dopo l'assegnazione (regressione dell'hook P4).

Il ritorno con B (annulla, vt90 0x53ea20, evento dello scanner della pagina con `ev+0x2c == pauser`) non dipende dal cursore: dovrebbe
già funzionare per P5..P8 [IPOTESI forte, da provare].

## 3. Colore dell'evidenziazione [VERIFICATO salvo dove indicato] — `c_page_cursor_49a8b8.asm`, `c_btncursor_442a84.asm`, `c_btncursor_colorchange.txt`

- Pagina `cm_Page_Cursor` (alloc 0x1f8 a 0x408efc, ctor 0x49a8c4, nome 0x49a8b8), onCreate 0x49a8fc: ciclo `w22 = 0x41..0x3a`, nome
  `"cm_L_BtnCursor_0" + ('0'+i)`, i = w22-0x3a = 7..0; **i ≤ 3** (0x49a968 `cmp x25,#3; b.hi`): alloc 0xc8, ctor 0x442a84(obj,i),
  0x8bbf04, `page[0x1d0+i·8] = obj`, **`0x442ca0(obj, i+1)`** (0x49a9c4, unico chiamante in tutto il main); **i ≥ 4**: pane trovato con
  0x725ad0 e reso invisibile (`+0x58 &= ~1`). La pagina ha posto solo per 4 puntatori (+0x1d0..+0x1e8; +0x1f0 float, +0x1f4 byte).
- 3.0.5 (0x49740c): 4 creazioni srotolate `cm_L_BtnCursor_03..00`, nessun ciclo né ramo "nascondi" → il ciclo a 8 nomi è codice nuovo del 4.0.1.
- Oggetto cursore (0x442ae4 = bind): gruppi 0 `In/Out`, 1 `Decide/Loop`, 2 `SizeChange` (0x442c7c), 3 **`ColorChange`** (0x442ca0:
  `0x8ad3c4(gruppo3, (float)n, 0)`); solo 0x442ca0 e il bind (frame 0) toccano il gruppo 3.
- Il pulsante della pausa (`cm_L_CmnBtn_00`) non ha animazioni di colore (solo InOut, Decide, Loop, OnOff): l'evidenziazione colorata è il
  cursore grafico sovrapposto.
- Scelta dell'oggetto: `[0x8bf228()]+0x1d0 + clamp(pad)·8` in 0x8ae628 (0x8ae64c), 0x8af690, 0x8affd8 (0x8affcc), 0x8b02cc (0x8b0278),
  0x8b08ac (0x8b08a4), 0x8bd87c → pad ≥4 = **oggetto 0 = giallo**.
- Colori `ColorChange` (P_BtnCursorBase_00, RGB al frame intero):

| frame | 4.0.1 | 3.0.5 |
|---|---|---|
| 1 | (255,200,0) giallo – P1 | uguale |
| 2 | (0,255,255) ciano – P2 | (0,120,255) |
| 3 | **(255,65,65) rosso – P3** | uguale |
| 4 | (60,230,10) verde – P4 | uguale |
| 5 | (240,20,10) rosso | uguale (squadra rossa) |
| 6 | (0,120,255) blu | (10,50,220) (squadra blu) |
| 7..10 | (240,20,10) rosso, (0,60,255) blu, (120,0,255) viola, (245,28,0) arancio | – (frameSize 6) |

  I pannelli 8pl della pausa mostrano per P5..P8 rosso, blu, viola, arancio = frame 7..10 → colore del cursore per il giocatore k (0-based)
  = `k < 4 ? k+1 : k+3` [IPOTESI forte].
- `PlayerColorIdxHook` (0x86a720 in 0x86a708) non c'entra: 0x86a708 non è usato dal cursore (il colore del cursore non passa da lì).

**Perché "rosso"**: staticamente il main non arriva mai ai frame 5..10; un cursore rosso può essere solo l'oggetto 2 (frame 3, P3).
Per pauser P1..P4 il colore atteso è quello del pauser (con la regressione su P1, §2.5); per P5..P8 giallo o nessun cursore attivo.
Se in Eden si vede rosso anche con pauser P1/P2/P4, allora lo slot disegnato è il 2 (o un altro cursore): verificare con la diagnostica §5.

## 4. Correzione

### 4.1 Hook P4 corretto (sostituisce il corpo di `PauseAssignExtraHook`, stesso sito 0x53e404 `320007e8`)
```cpp
namespace pause8 {
    static int tint = 1;                                   /* frame ColorChange attuale del cursore grafico 0 */
    static void TintCursor0(int frame) {                   /* 0x442ca0(obj, n): gruppo 3 ColorChange, frame n */
        if (frame == tint) return;
        uint8_t* cp = mk8dx::Fn<uint8_t* (*)()>(0x8bf228)();             /* pagina cm_Page_Cursor (componente 13) */
        uint8_t* g0 = cp ? *(uint8_t**)(cp + 0x1d0) : nullptr;
        if (!g0) return;
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x442ca0)(g0, frame);
        tint = frame;
    }
    static int Owner(uint8_t* cur) {                       /* pauser >= 4 della pausa attiva che possiede cur, altrimenti -1 */
        if (!view8::split || !cur) return -1;
        uint8_t* pg = *(uint8_t**)(cur + 0x8);
        if (!pg || *(int32_t*)(pg + 0xe8) != 0x6f || !(*(uint32_t*)(pg + 0x1a8) >> 31)) return -1;
        int p = pg[0x51c];
        return (p >= 4 && p < 8 && p < view8::nLocal) ? p : -1;
    }
}
HOOK_DEFINE_INLINE(PauseAssignExtraHook) {                 /* 0x53e404, dopo il ciclo 0..3 di vt75 0x53e0bc; x19 = pagina pausa */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!view8::split) return;
        uint8_t* p = (uint8_t*)ctx->X[19];
        int pauser = p[0x51c], nb = *(int32_t*)(p + 0x1e8);
        uint8_t** btn = (uint8_t**)*(uintptr_t*)(p + 0x1f0);
        uint8_t* cur = *(uint8_t**)(p + 0x1b0);
        if (!cur) return;
        for (int i = 4; i < view8::nLocal && i < 8; i++) {
            uint32_t bit = 1u << i;
            if (i == pauser) {
                for (int k = 0; k < nb; k++) *(uint32_t*)(btn[k] + 0x88) |= bit;
                *(uint32_t*)(p + 0x1a8) |= bit;
            } else {
                for (int k = 0; k < nb; k++) *(uint32_t*)(btn[k] + 0x88) &= ~bit;
                if (p[0x51d]) *(uint32_t*)(p + 0x1a8) |= bit; else *(uint32_t*)(p + 0x1a8) &= ~bit;
                /* NIENTE 0x8afaf0(cur,i): con i >= 4 colpirebbe lo slot 0 (clamp 0x8afb48) = P1 o il pauser */
            }
        }
        /* dopo le maschere (anche dei non pauser): lo slot 0 (P1, gia' rilasciato dal ciclo originale) diventa del pauser */
        if (pauser >= 4 && pauser < 8 && nb > 0)
            mk8dx::Fn<void (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(cur, pauser, *(int32_t*)(btn[0] + 0x98), 0, 1, 0);
        pause8::TintCursor0(pauser >= 4 && pauser < 8 ? 7 + (pauser - 4) : 1);
        Logging.Log("[mk8dx-8p] P4: pausa di P%d, maschera 0x%x, pulsanti %d, colore cursore %d", pauser + 1,
                    *(uint32_t*)(p + 0x1a8), nb, pause8::tint);
    }
};
```
- `0x8af3f4` dopo il ciclo: il bit del pauser in page+0x1a8 è già acceso (controllo 0x8af45c), lo slot 0 è in stato 2 (o 0 con flag, primo
  ingresso) come per un pauser P1 nativo [VERIFICATO percorso]. Con P1 pauser lo slot 0 non viene più toccato → regressione §2.5 risolta.
- `TintCursor0(1)` anche per pauser 1-4: rimette il giallo se un'uscita non è passata dal ripristino.

### 4.2 TRAMPOLINE 0x8b0084 – lettura del pad del pauser (`f81e0ff3`, `void (uint8_t* cur)`)
```cpp
HOOK_DEFINE_TRAMPOLINE(CursorPollHook) {                   /* 0x8b0084: 0x8b00e0(cur, 0..3) srotolato */
    static void Callback(uint8_t* cur) {
        int p = pause8::Owner(cur);
        if (p < 0) { Orig(cur); return; }
        uint32_t* m = (uint32_t*)(*(uint8_t**)(cur + 0x8) + 0x1a8);
        uint32_t save = *m;
        *m = save & ~1u;                                   /* P1 non pilota lo slot 0 condiviso (0x8b00e0 -> 0x8ba634(page,0) falso) */
        Orig(cur);
        *m = save;
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b00e0)(cur, p);   /* slot clamp -> 0, mgr+0xc68[0], GetPad(p) = pad extra */
    }
};
/* exl_main, accanto a BtnScanHook: */
CursorPollHook::InstallAtOffset(0x8b0084);                 /* parola attesa 0xf81e0ff3 */
```
- Il bit 0 viene spento solo durante `Orig` di 0x8b0084, che tocca solo il cursore: i pannelli di P1 (scanner propri) non ne risentono
  [VERIFICATO: 0x8b0084 chiama solo 0x8b00e0]. Senza questo, con lo slot 0 attivo una direzione di P1 passerebbe da `nav->next` a
  `0x8af3f4(cur,0,…)`→`0x8af87c`, che nel vanilla non viene mai esercitato con un pad senza bit nei pulsanti [rischio evitato].
- Disegno: nessun hook. 0x8b01ec disegna lo slot 0 come pad 0 col cursore grafico 0 (ricolorato) sulla voce del pauser.
- Conferma: con lo slot 0 in stato 3/4 sull'indice giusto `0x8ae21c(item,p)` è vero → A del pauser decide (scanner già esteso).

### 4.3 Ripristino del colore all'uscita (nel `PageLinkHook` esistente, 0x8ba5fc `b940e808`)
```cpp
/* in testa a PageLinkHook::Callback(page, link): */
if (page && *(int32_t*)(page + 0xe8) == 0x6f) pause8::TintCursor0(1);   /* Continua (+0x220) e pulsanti (+0x158) passano da qui */
```
Le uscite dalla pausa usano 0x8ba5fc: annulla (vt90 0x53ea64 `b 0x8ba5fc` con `pause+0x220`) e decide del pulsante (0x435c20 con
link +0x158) [VERIFICATO]; altre uscite (fine gara forzata, disconnessione) [IPOTESI] → coperte dal `TintCursor0(1)` al successivo
ingresso in pausa e, se serve, da un `TintCursor0(1)` anche in SceneInitRace.

### 4.4 Alternative scartate
- **Estendere l'array degli slot**: impossibile sul posto (+0xa8.. occupati, §1) e 15+ siti + tabella globale 4 × s32 per pagina.
- **Hook di tutti i clamp** (0x8af474, 0x8af490, 0x8afb48, 0x8afc1c, 0x8aff70, 0x8affcc, 0x8b0040, 0x8b0108, 0x8b0278, 0x8ae1e4,
  0x8ae228, 0x8ae274, 0x8ae64c, 0x8b08a4): servirebbe comunque una memoria per gli slot 4..7 → stesso problema.
- **Cursori grafici 4..7 veri** (dalle parti nascoste `cm_L_BtnCursor_04..07` del 4.0.1, frame 7..10): creazione in un array della mod
  (la pagina ha posto per 4), hook dei 6 siti `+0x1d0[clamp]` e dei cicli di aggiornamento della pagina cursore (0x49aa1c, 0x49ab14,
  0x49ab78, 0x49abdc `cmp w1,#4`). Utile solo se più giocatori ≥5 devono avere un cursore contemporaneamente (menu a 8): non per la pausa.
- **Rimappare il pauser su uno slot 0..3 cambiando GetPad**: GetPad(s) serve anche ai pannelli L_SwitchBtn dei giocatori 1-4 → invasivo.

## 5. Rischi e prove in Eden
1. [IPOTESI forte] `mgr+0xc68[0]` = pagina pausa (focus di P1): se no, 0x8b00e0/0x8b0238 escono subito. Diagnostica in CursorPollHook
   (solo pagina 0x6f, una riga ogni ~60 frame):
   ```cpp
   uint8_t* mgr = mk8dx::Fn<uint8_t* (*)()>(0x8bdc8c)(); uint8_t* pg = *(uint8_t**)(cur + 8);
   uint8_t* cp = mk8dx::Fn<uint8_t* (*)()>(0x8bf228)();
   for (int s = 0; s < 4; s++) Logging.Log("[mk8dx-8p] C: slot %d stato %d idx %d focus=%d gfx=%p", s, *(int32_t*)(cur + 0x28 + s*0x20),
       *(int32_t*)(cur + 0x30 + s*0x20), *(uint8_t**)(mgr + 0xc68 + s*8) == pg, cp ? *(uint8_t**)(cp + 0x1d0 + s*8) : nullptr);
   ```
   Questa riga serve anche per (b): lo slot con stato ≥4 è quello disegnato; se con pauser P1/P2/P4 risulta attivo lo slot 2, il "rosso" è
   il cursore di P3 e la causa è a monte (pauser/indice pad), non nel colore.
2. Pausa da P1..P4 in 8P: cursore e colore del pauser (P1 deve tornare a funzionare); pausa 1P/4P: identica (hook inerti senza split).
3. Pausa da P5..P8: muove con le direzioni, A su Continua/Abbandona, B torna in gara; colore rosso/blu/viola/arancio (frame 7..10).
4. Dopo la pausa di P5..P8: nei menu il cursore di P1 di nuovo giallo (ripristino §4.3).
5. P1..P4 non devono poter muovere né confermare la pausa di P5..P8 (maschere +0x88 e bit 0 spento durante la lettura).
6. La memoria dell'ultimo indice della pagina 0x6f (slot 0) viene condivisa tra P1 e il pauser ≥5: al massimo la pausa successiva di P1
   parte da "Abbandona" se l'ultimo pauser vi era sopra [VERIFICATO meccanismo 0x8afbec; impatto cosmetico].
7. `0x442ca0` cambia solo il frame del gruppo statico: si applica al prossimo calcolo del layout [IPOTESI forte: è anche il modo in cui il
   colore viene impostato alla creazione].
