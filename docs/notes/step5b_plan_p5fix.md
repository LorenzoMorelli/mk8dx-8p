# Step 5b – passo 5, correzioni dopo la prova in Eden (piano statico)

Data: 3 nov 2026. Solo analisi statica: `src/` non modificato, nessuna build. Base: `../img/main.bin`, `../img/main.ann.asm`,
RomFS `../romfs-tmp/menu4/mn_CharaKartSlct_{8pl,Multi}.sarc`. Screenshot: `step5b_data/eden_5b5_kart_vuota.png`,
`eden_5b5_kart_dopo_regole.png`, `eden_5b4_kart_p1vuoto.png`.
Verifica parole: `step5b_data/p5d_verify.py` → `p5d_verify.txt` (**0 errori**; nessun indirizzo già usato in `main.cpp`).
Etichette: [VERIFICATO] = letto in main.bin / RomFS; [IPOTESI] = da confermare in Eden.

## 0. Sintesi
| difetto | causa | correzione |
|---|---|---|
| **A** selettori L_KartSlot invisibili al primo ingresso | animazione **"Change"** (gruppo 0 della parte radice) avviata dalla selezione solo sulla **0x16** (`sel+0x728`), mai sulla **0x12** [VERIFICATO] | in `KartEnterHook` (set B, `fromSel`): `0x8ad300(Change(0x12), 0, 1.0f)`; nuovo trampolino **0x4c3360** (kart vt+0x280) per invertire la Change anche sulla 0x16 al ritorno alla selezione |
| **B** P5-P8 copiano le parti di P1 | il selettore legge la parte iniziale da `cfg+0x1fd4/+0x1fe4/+0x1ff4[clamp4(sel+0x1e8)]`: 3 clamp **mai patchati** in 0x474d54/0x476724/0x4774f0 [VERIFICATO] | 3 parole `cmp #4 → #8` + 3 `Cfg8AddB8I9` (0x476630, 0x477408, 0x477e40) |
| **C** icona carrozzeria vuota (P1 e chi copia P1) | si trasferisce con il **valore** `cfg+0x1fd4[0]`, non con lo slot. Ipotesi più probabile: valore assente dalla lista carrozzerie, quindi nessun match a 0x476660 e posizione/scroll del selettore non inizializzati [IPOTESI] | log diagnostico (§3.3); se confermato, si sanifica `cfg+0x1fd4[i]` prima di 0x4c0490 |
| **D** finestra varianti di Link da P8: niente movimento né conferma | non risolto. Il codice di decisione è **identico** per i tipi 3..9 (Yoshi tipo 3, Link tipo 8) [VERIFICATO]. Trovato invece un clamp sul cursore grafico (0x46c100): P8 muove il cursore di P1 [VERIFICATO] | patch cursore grafico (§4.2); prova discriminante Yoshi **con P8** e log (§4.4) |

Nuovi ingressi JIT: **+4 inline** (3 cfg8, 0x46c10c) e **+1 trampolino** (0x4c3360). Parole: **+4** (3 clamp B, 1 clamp D).

## 1. A – selettori L_KartSlot invisibili

### 1.1 Fatti [VERIFICATO]
- Le due pagine CharaParts hanno una sola animazione, **`Change`**, sul gruppo `G_Change_00` (20 frame; bflan dentro le sarc, un solo
  FLAN per file):
  - `mn_CharaKartSlct_8pl`: `L_KartSlot_00..07` con **alpha (FLVC 16) 0 → 255 ai frame 10..20** e scala 0.3 → 0.36;
    `N_Chara_00` alpha 255 → 0 e scala 1 → 0.8 (frame 0..10); `N_ConSuppo_00` alpha 255 → 0.
  - `mn_CharaKartSlct_Multi`: stessa animazione, con `L_KartSlot_00..03` (scala → 0.48).
  - Al frame 0 (stato di riposo) **i L_KartSlot hanno alpha 0**: sono invisibili finché la Change non viene riprodotta.
- Le funzioni di animazione (gruppo `g = **(part+0x70)`, part = `[page+0x1d0]`):
  - `0x8ad300(g, idx, float velocità)` → vt+0xb0, avvia con quella velocità;
  - `0x8ad474(g, idx)` → vt+0xd8, porta l'animazione alla fine.
- Chi la usa:

| sito | funzione | pagina | effetto |
|---|---|---|---|
| 0x4bb538..0x4bb550 | selezione vt+0x280 (0x4bb508, inizio uscita), destinazione 0x2f..0x31 | **`[sel+0x728]` = 0x16** | `0x8ad300(g,0,+1.0)`: N_Chara sparisce, L_KartSlot compaiono |
| 0x4c0e38..0x4c0e4c | kart onEnter, ramo "non dalla selezione" (`w26 = prec−0x2c ≥ 3`, es. dalle regole) | `[kart+0x3a8]` | `0x8ad474(g,0)`: subito alla fine |
| 0x4c3390..0x4c33a8 | kart vt+0x280 (0x4c3360, inizio uscita), destinazione 0x2c..0x2e | `[kart+0x3a8]` | `0x8ad300(g,0,−1.0)`: indietro |

  vtable: 0x4c3360 è alla stessa distanza da 0x4c0898 (vt+0x258) in tutte le vtable kart (0x1279a70 → 0x1279a98, +0x28 = vt+0x280);
  0x4bb508 è a +0x28 da 0x4bb6a0 − 0x10 nelle vtable selezione (0x12785d0).
- Il ramo dalla selezione dell'onEnter kart (0x4c0da8..0x4c0e34) **non** tocca la Change: presuppone che l'abbia già avviata la selezione.

### 1.2 Causa
Con il set B la selezione (0x4bb550) avvia la Change sulla **0x16**, che in B ha `N_Player_00..03` nascosti (`p4::MultiNames(false)`).
Dei suoi effetti resta visibile solo la scomparsa della griglia (screenshot: nessuna griglia, quindi corretto). La **0x12** resta al frame 0:
L_KartSlot_00..07 hanno alpha 0, mentre L_Name, piedistalli e OK si vedono (`eden_5b5_kart_vuota.png`).

Dalle regole l'onEnter fa `0x8ad474` su `[+0x3a8]` = 0x12, e i selettori compaiono (`eden_5b5_kart_dopo_regole.png`). Il fenomeno è spiegato
interamente [VERIFICATO asm + bflan].

Difetto collegato, previsto e non ancora osservato [IPOTESI forte]:
- tornando dalla kart B alla selezione (B), 0x4c33a8 inverte la Change sulla **0x12** (`+0x3a8`) e non sulla 0x16;
- la 0x16 resta alla fine della Change, con `N_Chara_00` (la griglia dei personaggi) ad alpha 0;
- la selezione non resetta l'animazione radice della 0x16: nessuna chiamata 0x8ad3xx/0x8ad4xx su `+0x728` nella classe, solo sulle L_Name
  (0x4be3b0..0x4be5ac).

Risultato atteso: **griglia invisibile dopo B dalla kart a 8**.

### 1.3 Correzione
Helper (namespace p5):
```cpp
    static uint8_t* ChangeGrp(uint8_t* pg) {                 /* gruppo 0 della parte radice: come 0x4bb538..0x4bb544 */
        uint8_t* part = pg ? *(uint8_t**)(pg + 0x1d0) : nullptr; uint8_t** a = part ? *(uint8_t***)(part + 0x70) : nullptr;
        return a ? a[0] : nullptr;
    }
    static void Change(uint8_t* pg, float v) { if (uint8_t* g = ChangeGrp(pg)) mk8dx::Fn<void (*)(uint8_t*, int, float)>(0x8ad300)(g, 0, v); }
```
**KartEnterHook** (0x4c0898), nel blocco `if (one)` prima di `Orig`:
```cpp
            if (p5::fromSel) { mk8dx::Fn<void (*)(uint8_t*, bool)>(0x4c0490)(page, true);
                               p5::Change(*(uint8_t**)(page + 0x3a8), 1.0f); }          /* = 0x4bb550, ma sulla 0x12 (set B) */
```
- In alternativa, senza dissolvenza: `0x8ad474(g, 0)`, come 0x4c0e4c.
- Il ritorno dalle regole resta invariato: lo gestisce già l'onEnter.

**Nuovo trampolino `KartPreExitHook`** su **0x4c3360** (prima parola `d10143ff` `sub sp,sp,#0x50`, verificata):
```cpp
HOOK_DEFINE_TRAMPOLINE(KartPreExitHook) {                /* 0x4c3360 kart vt+0x280: inizio uscita (Change -1 su +0x3a8) */
    static void Callback(uint8_t* pg) {
        Orig(pg);
        if (!p5::isB || p5::built != pg) return;
        uint8_t* l = *(uint8_t**)(pg + 0x168); int32_t dest = l ? *(int32_t*)(l + 0xc) : -1;   /* come 0x8ba5a0 */
        if (dest >= 0x2c && dest <= 0x2e) p5::Change(p5::A.p3a8, -1.0f);                     /* anche la 0x16 torna indietro */
    }
};
```
`p5::A.p3a8` è la 0x16 salvata in BuildB [VERIFICATO in main.cpp]. Se la 0x16 non è aperta, ad esempio dopo regole → kart → selezione,
quando KartExitHook l'aveva chiusa: [IPOTESI] 0x8ad300 su una pagina chiusa è innocuo, perché agisce solo sull'oggetto animazione.

Installazione: nel blocco p5, insieme a KartExitHook. È un nuovo indirizzo, senza collisioni (p5d_verify).

### 1.4 Prova
- 8P, selezione → kart: gli 8 selettori compaiono con la dissolvenza in 10 frame. Poi B → selezione: la griglia è visibile.
- Kart → regole → indietro: invariato. Kart → B → selezione → kart: i selettori compaiono di nuovo, perché la Change della 0x12 è
  tornata a 0.
- 1-4P: nessun cambiamento. Tutto è condizionato da `one`/`isB`.

## 2. B – parti di P5-P8 copiate da P1

### 2.1 Fatti [VERIFICATO]
- Il selettore di parte (classe 0x47446c, oggetto 0x330) riceve lo slot in **`sel+0x1e8`**: costruzione 0x4bfdd0, `str w23,[x27,#0x1e8]`
  a 0x4bfff4/0x4c0090/0x4c012c, con w23 = indice del ciclo, 0..`+0x39c`−1.
- `0x474bf0(sel, &pers, var, flag)` costruisce la lista in base al tipo `sel+0x274`:
  - 0 → 0x474d54 (carrozzeria), 1 → 0x476724 (gomme), 2 → 0x4774f0 (aliante);
  - se il regalo `sel+0x324` è −1, posiziona il cursore sulla parte salvata (`+0x110 = 0, +0x114 = idx, +0x118 = 0, +0x11c = idx,
    +0x120 = 0`):

| sito | lettura | clamp |
|---|---|---|
| 0x47661c..0x476634 | `ldrsw x9,[x19,#0x1e8]` … `cmp w9,#4; csel x9,x9,xzr,lo; add x8,x8,x9,lsl #2; ldr w8,[x8,#0x1fd4]` | **0x476628** |
| 0x4773f4..0x47740c | idem, `#0x1fe4` | **0x477400** |
| 0x477e2c..0x477e44 | idem, `#0x1ff4` | **0x477e38** |

- Con sel+0x1e8 = 4..7 l'indice diventa **0**: P5-P8 si posizionano sulla parte salvata di P1. Il valore va poi nelle entry e4..e7
  tramite 0x4c1bfc, che le scrive correttamente (cfg8 sugli slot 4..7). Questo spiega il log `carr=0 gomme=8 ali=5` per e0, e4..e7 e
  lo screenshot dopo le regole (gomme arancioni e parapendio di P1 in P5-P8).
- **Scansione completa** degli accessi `cfg+0x1fb4..+0x208f` nel selettore (0x46c000..0x47c000) e nella classe kart (0x4bfdd0..0x4c4ee0):
  - selettore: solo i 3 siti sopra;
  - kart: 0x4c0a08/0x4c0a78/0x4c0adc (onEnter, ciclo `i ≤ 1` a 0x4c0998, copia in +0x480), 0x4c1df0/0x4c1ea4/0x4c1f58 (scrittura, già cfg8),
    0x4c37f0..0x4c3b8c (uscita, `i ≤ 1` a 0x4c3778: statistiche di P1/P2), 0x4c113c/0x4c11b4 (`+0x1fb0`, scalare). Nessun altro.
  - I campi `+0x1e8` nel selettore sono letti solo in questi 3 punti.
- **Scansione dei `cmp #4` + `csel …xzr`** non patchati:
  - selettore: solo 0x476628/0x477400/0x477e38;
  - kart: solo 0x4c09f0 (i ≤ 1), 0x4c2658/0x4c26cc/0x4c2748 (PartsPresent, lasciati a 4 di proposito, §4 di p5), 0x4c37e0 (i ≤ 1),
    0x4c1594 (`b.hs` nell'azzeramento 0x4c157c, non eseguito con +0x39c = 8).
- Il layout dell'ombra è compatibile: `cfg8::Base() = sh − 0x1fb4 − 16`. Con xI = 4..7 e imm 0x1fd4/0x1fe4/0x1ff4 si ottiene
  `sh[8/12/16 + xI−4]`, le stesse celle scritte da 0x4c1df0/0x4c1f58/0x4c1ea4.

### 2.2 Correzione (tabella verificata, p5d_verify.txt)
| off | originale | nuova | |
|---|---|---|---|
| 0x476628 | 7100113f `cmp w9,#0x4` | **7100213f** `cmp w9,#0x8` | carrozzeria |
| 0x477400 | 7100113f `cmp w9,#0x4` | **7100213f** `cmp w9,#0x8` | gomme |
| 0x477e38 | 7100113f `cmp w9,#0x4` | **7100213f** `cmp w9,#0x8` | aliante |

Hook inline `Cfg8AddB8I9` (esistente, `X[9]−4 < 4 → X[8] = cfg8::Base()`) su:
- **0x476630**, **0x477408**, **0x477e40**, tutti con parola `8b090908` `add x8,x8,x9,lsl #2`.

In `main.cpp`: aggiungere le 3 voci a `p5::kClampKart2` (o a una nuova `kClampSel` controllata dallo stesso `chk`). Aggiungere poi i 3
`InstallAtOffset` accanto a quelli di 0x4c1dec/0x4c1ea0/0x4c1f54, e le 3 parole a `p5::kHookWords`.

1-4P identico: sel+0x1e8 ≤ 3, quindi il clamp è inerte e l'hook non scatta. I valori iniziali di P5-P8 sono quelli dell'ombra: 0 la prima
volta (Kart standard / Standard / Superdeltaplano), poi l'ultima scelta della sessione (solo RAM).

### 2.3 Prova
- 8P: entrando nella kart, P5-P8 mostrano 0/0/0 (o l'ultima scelta), non le parti di P1. `cfg8::hits` cresce di 3×4 all'ingresso.
- `LogAssigned("kart Multi -> avanti")`: e4..e7 con i valori scelti da ciascuno.
- Uscire e rientrare: P5-P8 ritrovano la loro scelta, letta dall'ombra.

## 3. C – icona carrozzeria vuota

### 3.1 Fatti
- [VERIFICATO screenshot `eden_5b4_kart_p1vuoto.png`] Nella colonna carrozzeria di P1:
  - sopra c'è "?" (segnaposto di fine lista), sotto la stessa moto di P2;
  - quindi la **posizione visualizzata è l'indice 0**, come per P2, ma la **texture centrale manca**;
  - la targhetta dice "Kart standard", il modello 3D di P1 ha il Kart standard, e l'entry riceve carr = 0.
- [VERIFICATO log/screenshot 5b5] Il difetto compare in P1 e negli slot che leggono `cfg+0x1fd4[0]` (P5-P8, a causa del clamp di §2), non in
  P2-P4 con la stessa carrozzeria 0. Tutti i selettori B passano dallo **stesso** 0x4c0490(page,1) → 0x474bf0(flag=1), con regalo e lista
  calcolati allo stesso modo. Quindi **la variabile è il valore `cfg+0x1fd4[0]`**, non lo slot 0 come tale.
- [VERIFICATO 0x474d54] La lista carrozzerie contiene:
  - gli id 0..0x27 posseduti (`0x865f60(0x8ad024()+0x190, &id)`), più l'eventuale regalo `+0x324`;
  - un **segnaposto id −1 con flag byte+8 = 1** se manca almeno una carrozzeria (0x476508..0x47652c, `+0x320 = 0`).

  Il posizionamento (0x476660..0x4766fc) scrive +0x110..+0x124 **solo se trova `item.id == cfg[slot]`**. Senza match restano i valori
  precedenti: ctor 0 o stato residuo dello scroll (`+0x120/+0x124`).
- [VERIFICATO] L'unico scrittore di `cfg+0x1fd4` nel main è 0x4c1df0, che salta −1 (0x4c1d4c). Il valore arriva quindi da qui o dal
  caricamento del salvataggio (memcpy).

### 3.2 Ipotesi, in ordine
- **C1** [IPOTESI, la più probabile]: `cfg+0x1fd4[0]` contiene un id che non è nella lista (non posseduto, ≥ 0x28 o −1/sporco).
  - Nessun match, quindi +0x110..+0x124 non vengono inizializzati.
  - La riga visualizzata vale 0 per ctor (vicini = quelli dell'indice 0), ma lo stato di scroll/caricamento della cella centrale non viene
    reimpostato e l'icona resta vuota.
  - L'entry riceve list[0] = 0: "Kart standard".
  - Spiega anche la "copia" su P5-P8 (stesso valore) e la presenza del difetto già con i due turni.
  - Origine plausibile [IPOTESI]: una sessione precedente a due turni che ha scritto in `cfg+0x1fd4[0]` la parte di P5, oppure un
    valore del salvataggio.
- **C2** [IPOTESI, meno probabile]: valore nella lista che coincide con il **segnaposto −1** (flag+8 = 1, icona "bloccata"). Lo
  contraddicono i vicini visti nello screenshot: con l'indice finale, sotto ci sarebbe l'indice 0 (Kart standard), non la moto.
- **C3** escluso: regalo PartsPresent (`[+0x3d8][0]+0x324`). Varrebbe per tutti i selettori (stesso flag) e i nomi sarebbero "?".

### 3.3 Verifica (una build, nessun effetto sul gioco)
In `KartEnterHook`, dopo `0x4c0490(page,true)` (e anche in 1-4P dopo `Orig`), loggare per i < +0x39c il selettore carrozzeria
`s = [+0x3d8][i]`:
```cpp
uint8_t* cfg = menu8::Cfg(); uint8_t** ss = *(uint8_t***)(page + 0x3d8);
for (int i = 0; i < *(int32_t*)(page + 0x39c); i++) { uint8_t* s = ss[i]; if (!s) continue;
    int32_t want = i < 4 ? *(int32_t*)(cfg + 0x1fd4 + 4 * i) : cfg8::sh[8 + i - 4];
    int cnt = *(int32_t*)(s + 0xcc), hit = -1; uint8_t* it = *(uint8_t**)(s + 0xc0);
    for (int k = 0; k < cnt && it; k++) if (*(int32_t*)(it + 16 * k) == want) hit = k;
    Logging.Log("[mk8dx-8p] C: sel%d slot=%d pers=%d cfg=%d match=%d n=%d +324=%d +110=%d +114=%d +118=%d +11c=%d +120=%d,%d", i,
        *(int32_t*)(s + 0x1e8), *(int32_t*)(s + 0x278), want, hit, cnt, *(int32_t*)(s + 0x324), *(int32_t*)(s + 0x110),
        *(int32_t*)(s + 0x114), *(int32_t*)(s + 0x118), *(int32_t*)(s + 0x11c), *(int32_t*)(s + 0x120), *(int32_t*)(s + 0x124)); }
```
Lettura dei risultati:
- `match = -1` solo per sel0 → **C1 confermata**.
- `match = n−1` (segnaposto) → C2.
- `match = 0` come P2 → la causa è nel caricamento delle texture: watchpoint su `s+0x29c` (flag di ricarica della cella 0, stride 0x18
  da +0x290).

Prova indipendente: 4P **senza mod** con lo stesso salvataggio. Con C1 il difetto c'è anche nel vanilla, perché il valore viene dal
salvataggio.

### 3.4 Correzione (se C1)
Sanificare prima di 0x4c0490, in `KartEnterHook` (tutti i casi) e/o in `CharselExitHook`. Per i < N, se `v = cfg+0x1fd4[i]` (ombra per
4..7) è < 0, ≥ 0x28 o `!0x865f60(0x8ad024()+0x190, &v)` [firma VERIFICATA da 0x475010..0x475024: `bool (void* save190, int32_t* id)`]:
scrivere 0. Lo stesso vale per gomme e aliante, se il log mostra casi analoghi. Gli id massimi si ricavano dal ciclo srotolato di
0x476724/0x4774f0, da verificare prima di usarli.

## 4. D – finestra varianti di Link aperta da P8 (slot 7, CharaList 0x64)

### 4.1 Fatti [VERIFICATO]
- **Tipo** di finestra (vt92 0x4bbd18, tabella 0xf2919c per personaggio 4..0x34, in `CharaList+0x2a0`):
  - personaggio 4 → 3; 0x10 → 4; 0xd/0x25 → 5; 0x29/0x2a → 6; 0x1f/0x2b → 7; **0x2c → 8**, solo se `0x8636bc(&0x2c) == 0`; 0x20/0x24 → 9;
  - personaggi con `0x8636bc ≠ 0` → tipo 1 (2 per 0x30..0x33, tabella 0xf23780). Questi passano dal flag **unico per pagina** `sel+0x9c9`
    (`cbnz` → nessuna finestra; poi `strh 1`) e dalla decisione pesante 0x4b3380 (array cfg +0x2004..+0x2074, già cfg8).
  - Corrispondenza nomi [IPOTESI]: Yoshi = 4 (tipo 3), Tipo Timido = 0x10 (tipo 4), Link = 0x2c (tipo 8, se Link BotW è sbloccato).
- **Decisione** (evento 7, vt92 0x4b31fc → tabella 0xf28cd4): i tipi 3..9 eseguono **lo stesso codice** (0x4b40a0..0x4b4258):
  `0x4b43b4(madre, slot, &idPers, item+0x10)`, con l'id personaggio come unica differenza. Nessun indice per slot diverso tra Yoshi e Link.
- **Ingresso** 0x4b29e0:
  - con tipo 1 e +0x2d1 esce subito (tipo := 2);
  - altrimenti la maschera +0x1a8 è srotolata su 0..3 e per lo slot 7 azzera solo i bit 0..3;
  - il bit 7 arriva da **vt93 0x4b3160** (`+0x1a8 |= 1<<slot`, con `0x8bd5f8(mgr, slot, page)` per il focus, già esteso da cur8).
- **Cursore** della CharaList: vt+0x240 = 0x409f90 → `0x8bafb8(page, 10)`, cioè 4 slot, **non largo**. `cur8::V(c,7) = 0`: P8 usa lo stato
  dello slot 0 del cursore della **propria** pagina. È accettabile, perché la pagina è per giocatore, ma richiede il bit 7 nelle maschere
  degli item (+0x58) e il focus di P8.
- **Cursore grafico**: la parte finestra 0x46c0a4(part, slot) prende `cm_Page_Cursor+0x1d0[clamp4(slot)]` (**0x46c100 `cmp w19,#4` non
  patchato**) e vi applica `0x8ae048(gfx+0x30, …)`. Con slot 7 manipola **cm_L_BtnCursor_00 di P1**. Se +0x2d0 == 2 chiama anche
  `0x49abc4(cp, slot)` (G5, già cur8).
- 0x4b2e4c `0x14a990(slot, 1)` è null-safe (0x13d47c restituisce NULL e c'è il `cbz` a 0x14aa10).

### 4.2 Correzione certa (cosmetica, ma tocca il cursore di P1)
| off | originale | nuova |
|---|---|---|
| 0x46c100 | 7100127f `cmp w19,#0x4` | **7100227f** `cmp w19,#0x8` |

Hook inline su **0x46c10c** (`8b090e89` `add x9,x20,x9,lsl #3`, x20 = cm_Page_Cursor 0x230, x9 = slot):
```cpp
HOOK_DEFINE_INLINE(CwGfxCursorHook) { static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->X[9] - 4 < 4) ctx->X[9] += 4; } };  /* 4..7 -> +0x210.. */
```
In 1-4P la slot è < 4 e non cambia nulla. I controlli 04..07 esistono già (cur8, onCreate 0x49a8fc).

### 4.3 Ipotesi sul blocco di P8 [IPOTESI]
- **D1**: il blocco non dipende da Link ma dallo slot 7. In questo caso Yoshi e Tipo Timido sono stati provati con P1-P4.
  - Cause candidate, tutte per slot ≥ 4: maschere degli item della finestra (+0x58 = 0x8000000f di default, senza bit 7, in nessun punto
    della classe 0x468000..0x46e000 si trova `1<<slot` sulle maschere: i `lsl` trovati sono flag di sblocco del salvataggio);
    focus/attivazione dello slot V=0 del cursore non largo.
- **D2**: è davvero legato al tipo 8 (Link, 2 voci). La differenza sarebbe nella costruzione degli item per tipo dentro la parte
  (`part+0x2d0` = tipo, scritto da 0x4694fc a 0x469528; consumatori 0x46a160, 0x46b058, 0x46b2a0, 0x46b47c, 0x46bf74, 0x46c024, 0x46c2b4…),
  non nella decisione.
- **D3**: il personaggio rientra nei tipi 1/2 (`0x8636bc ≠ 0`). In questo caso la finestra è quella "pesante":
  - flag unico `sel+0x9c9`;
  - onEnter che con +0x2d1 esce prima di maschera e focus;
  - pagina in due fasi (tipo 2, vt85 0x4b2ca8, con `0x4694fc(…, +0x8c0, slot)`).

### 4.4 Prova discriminante
1. Yoshi **con P8** (stesso slot 7). Se si blocca anche Yoshi → D1. Se Yoshi funziona → D2/D3.
2. Log una volta al secondo in `CharselFrameHook`, per la CharaList dello slot 7:
   - `p = 0x8bf0ec(&0x64)`: `+0x2a0` (tipo), `+0x120`, `+0x124`, `+0x1a8`, `+0x190`, `+0x2d1`;
   - `[p+0x1d0]+0x2d0` (tipo nella parte);
   - `c = [p+0x1b0]`: slot 0 `{+0x28 stato, +0x30 indice}`;
   - `cur8::foc[3]`;
   - `sel+0x9c9`.

   Con D1 ci si aspetta `+0x1a8` con il bit 7 e il cursore fermo. Con D3 `+0x2a0` = 1/2.
3. GDB: breakpoint a 0x4b327c (vt92, evento ricevuto) con x19 = pagina 0x64. Se non scatta mai quando P8 preme A, l'input non arriva alla
   pagina (maschera/focus/item). Se scatta con w8 ≠ 7, l'evento arriva ma non come decisione.

## 5. Tabella riassuntiva da integrare (verificata, `p5d_verify.txt`)
- Parole: 0x476628, 0x477400, 0x477e38 `7100113f → 7100213f`; 0x46c100 `7100127f → 7100227f`.
- Hook inline: 0x476630, 0x477408, 0x477e40 (`8b090908`, `Cfg8AddB8I9`); 0x46c10c (`8b090e89`, `CwGfxCursorHook`).
- Trampolino: 0x4c3360 (`d10143ff`, `KartPreExitHook`).
- Modifica: `KartEnterHook`, con la Change sulla 0x12 dopo 0x4c0490 quando `fromSel`.
- Facoltativo: log C (§3.3) e log D (§4.4).
- Capienza JIT: circa 204 + 5 = **209 su 327**.
