# Step 7b – La "moneta" 3D nelle celle P6-P8 della selezione a 8: analisi statica

Data: 4 nov 2026. Solo analisi statica (`../img/main.ann.asm`, `../img/main.bin`, `src/program/main.cpp`, log e screenshot
già in `step7b_data/`). Eden e GDB non usati, `src/` e `switch_env/` non modificati. Dalla RomFS 4.0.1 ho estratto in sola
lettura, nello scratchpad, solo `/UI3D/BaseTable/BaseTable.bfres` e `/RaceCommon/BattleCoin/BattleCoin.bfres`, con `tools/bktr.py`.
Verifica: `step7b_data/coin_verify.py` → `step7b_data/coin_verify.txt`, **294 OK, 0 FAIL**. Lo script controlla parole a 4 byte,
bersagli dei `b/bl`, qword di vtable, float, stringhe, i 18 lettori di X+0xac e i nomi dentro i due bfres.
Etichette: **[V]** = verificato nel disassemblato o nei dati, **[I]** = ipotesi, **[D]** = dato misurato (log/screenshot).

---

> **Aggiornamento (Misura 1, sotto §7):** le monete del corpo kart nel menu **non esistono** (corpo+0x430/+0x438 = NULL per le 8
> unità, [D] sonda coindiag) e il codice lo conferma: 0x11cd6c salta la creazione nei modi 5/7 (0x11cd90..0x11cd98). Le sezioni
> 1.1/2/3 restano valide come descrizione di X+0xac, ma l'ipotesi ItemCoin/BattleCoin e la correzione F1/F1′ **decadono**.
> Ipotesi aggiornate e misura decisiva nel §7.

## 0. Sintesi

1. **Monete 3D nel menu.** Nel binario e negli asset del menu ci sono solo tre modelli a forma di moneta [V]:
   - il **"Ticket"** del piedistallo `BaseTable`;
   - due modelli moneta attaccati al **corpo di ogni kart**, cioè al KartUnit:
     - **BattleCoin**, in corpo+0x430: è il contatore di monete;
     - **ItemCoin**, in corpo+0x438: sono le monete raccolte (`/Item/ItemCoin/ItemCoin.bfres`, creato solo per i kart con
       descrittore di tipo 0).

   Nella stanza del menu (LocalRoom, Earth, SpaceToad, Sun, VRNetwork) non c'è nessuna moneta. Nessun codice sostituisce un
   modello mancante con una moneta: l'ipotesi "segnaposto di caricamento" non ha appoggi nel codice.
2. **Il Ticket è escluso [V + ragionamento].** È un gettone con texture diversa per 6 personaggi DLC (Tanuki Mario, Peach
   gatto, Link, Abitante, Fuffi, Skelobowser). Si mostra solo se P1-P4 hanno scelto uno di quei 7 id (0x1e..0x24). La sua
   visibilità vale per tutto il modello: se fosse lui, si vedrebbe anche nelle celle P2-P4, perché nella variante A mostrano
   gli stessi piedistalli 1..3. In più `m3d8::MenuFrame` lo nasconde a ogni frame.
3. **Oggetto più probabile: le monete del corpo kart delle unità 4..7** [I, ~60 %]. Restano Ticket (~10 %) e un oggetto non
   ancora individuato (~30 %).
4. **Tabella a 4 voci [V].** Il gioco calcola la maschera di vista delle due monete del corpo **intera**:
   - funzioni 0x11e91c (reset del corpo) e 0x121b18 (visibilità per frame);
   - formula: `1 << X+0xac`, dove X+0xac è lo **slot di vista** del kart;
   - se X+0xac < 0 la maschera diventa **0xffff, cioè tutte le viste**.

   X+0xac viene assegnato solo nel ctor di X (0x16fdb0..0x16ff30). Il ciclo scorre i **4 slot** della scena (scena+0xb0..+0xbc,
   con indice di viewport limitato a 5: `cmp #5`). Le unità 4..7 non hanno uno slot, quindi restano a **-1**.

   Conseguenze:
   - le monete delle unità 4..7 sono abilitate in tutte le viste, comprese 4..7;
   - questa scrittura cancella la pulizia dei bit 4..7 che `ModelCreateHook` fa alla creazione;
   - `UnitViewBit` non la corregge: tocca solo i nodi della lista del KartUnit, mentre le monete sono modelli "nudi" spinti
     direttamente da 0x121ab8.

   Le monete delle unità 0..3 hanno invece `1 << slot`: solo la propria vista, mai le viste 4..7.
5. **P5 contro P6-P8.** La maschera spiega perché le monete compaiono solo nelle viste extra e solo per le unità 4..7. **Non
   spiega da sola perché la vista 4 (P5) ne resta senza**: l'unità 4 ha anch'essa -1.
   - **H-A [I], preferita:** lo stato per vista della moneta è elaborato solo per le prime 5 viste. Il "4+1" è quello dei 5
     viewport di scena (`0x16e184` bit 0..4, clamp `cmp #5`). Nelle viste 5..7 resta uno stato di default "visibile". Il sito
     esatto non è stato trovato.
   - **H-B [I]:** le maschere a runtime sono diverse da quelle previste.

   Le misure del §4 decidono fra le due in una sessione, solo con log.
6. **Piano.**
   - **0 hook nuovi per le misure:** un blocco di log in `m3d8::MenuFrame` e 4 prove one-shot dietro l'opzione
     `MK8DX_P7_COINTEST`.
   - **Correzione fedele al flusso, F1:** 1 hook inline a **0x16ff34** (`bd40f660`). Estende a 8 la tabella degli slot di vista
     del kart: unità k = 4..7 → X+0xac = k, con la stessa regola di 0..3. Così il gioco calcola da solo le maschere `1<<k`.
     - Prerequisito: audit dei 18 lettori di X+0xac. Almeno uno è a rischio: 0x147f34, camera del menu, dove l'indice di slot
       della camera è limitato a 4.
     - Se H-A è confermata, F1 non basta: serve estendere a 8 l'elaborazione per vista limitata a 5 (F2, sito da trovare con M3).

---

## 1. Q1 – Che oggetto è

### 1.1 Inventario delle monete 3D [V]
| oggetto | risorsa | dove nasce | chi lo spinge nella lista di disegno | maschera di vista |
|---|---|---|---|---|
| **Ticket** del piedistallo | parte di `UI3D/BaseTable/BaseTable.bfres` (0xee8b47) | 4 voci del viewer `v+0x568+i*0x138`, ctor 0x42fc84 (`cmp x24,#0x3a8`) | il modello del piedistallo | quella del piedistallo (0x430cc4 → 0x68fa38, per vista) |
| **BattleCoin** | `/BattleCoin/BattleCoin.bfres` (0xedbff0; RomFS `/RaceCommon/BattleCoin/`), shape `BattleCoin__m_Coin`, `__m_Coin_Side` | corpo kart, 0x11d508: `bl 0x8807a8` a 0x11d69c, `str [x19,#0x430]` a 0x11d6a4, **sempre** | 0x121ab8: solo se corpo+0x48c ≥ 1 (0x121adc..0x121ae8) | intera: 0x11e91c (0x11e9bc..0x11ea1c), 0x121b18 |
| **ItemCoin** | `"ItemCoin"` (0xed6b08) + `"/Item/%s/%s.bfres"` (0xedc030), oppure `TS_ItemGesoCoin` nel modo 3 | corpo kart, 0x11d508: solo se **X+0xd0** (0x11d6b0/0x11d6b4), `str [x19,#0x438]` a 0x11d884 | 0x121ab8: **a ogni chiamata**, senza condizioni (0x121aec → `b 0x691dd0`) | intera: 0x11e91c (0x11ea20..), stessa formula |

- **X+0xd0 = (descrittore.tipo == 0)** [V]: 0x16faf0 `ldr w8,[L,#0xd54]`, 0x16fafc `cset eq`, 0x16fb00 `strb [X,#0xd0]`. L+0xd54 è il
  campo +0x14 del descrittore, cioè il "tipo" (`LogDesc` q[5]).
  - Nella selezione a 8 i descrittori 0..7 sono tutti di **tipo 0** [D: `eden_log_rec_s3_stress8.txt` / `flow8_*`, righe
    `slotN ... tipo=0`].
  - Quindi tutte le 8 unità hanno l'ItemCoin.
- **Corpo** = X+0x50 [V: 0x16d810, 0x174e30]. X è il gruppo modelli del kart (ctor 0x16fa58). Sta in voce+0x18, con voce =
  `unit+0x248 + sel*0x78` [V: 0x16ee14..0x16ee20, 0x16dfe4]. X+0xa8 è l'indice del kart, cioè unit+0x40 [V: 0x16ee24 → 0x16df1c →
  0x16dfd0 → 0x16fab4].
- **Spinta per frame** (0x691dd0) [V]:
  - il modello va nella lista dei modelli del renderer: `R+0x55c8` (= DLM+0x110), contatore `R+0x55c0`;
  - la catena è 0x16cd14 → `0x174e18(X)` (solo se unit+0x44 e unit+0x45; nel menu serve anche unit+0x4c) → `0x121ab8(corpo)`.
  - [I forte] Un modello non spinto non viene disegnato: il job di disegno scorre le liste del DLM (step2_crash8v §1).
- **Il conteggio del BattleCoin** (corpo+0x48c) è `min(entry[X+0xa8]+0x50, termine di distanza)` (0x120d84..0x120edc).
  - `entry` sono le voci del gestore `dg+0x218`. Nel menu ce ne sono 8 [D: `diag218 +60 = 0x800000008`].
  - Il ctor 0x86fabc azzera +0x50 (0x86fb40).
  - Nel menu il conteggio vale quindi 0 e il BattleCoin non viene spinto [I forte]. Per questo **ItemCoin è il candidato
    migliore**: è spinto a ogni frame, ed è la moneta standard con la fessura, come negli screenshot.
- **Posizione [I].** Le ossa dell'ItemCoin vengono posizionate solo quando arrivano monete (0x1211a4 → `bl 0x122578` a 0x121240 → … → `b 0x122670` a 0x12261c). Senza monete il
  modello resta nella posa di legame sull'origine del mondo. Nel menu l'origine coincide con il centro del piedistallo: tutti i
  modelli del menu stanno lì (`pos=0,0,0` nei log [D]). Così si spiega la moneta in piedi "dietro al kart, sopra il piedistallo",
  uguale in P6, P7 e P8 e indipendente dal personaggio.

### 1.2 Perché non è il Ticket del piedistallo [V + ragionamento]
- Cos'è il Ticket:
  - il reset del BaseTable (0x42d04c) cerca "Ticket" (0x42d090 → 0x68f8fc), salva l'indice in voce+0xc8 e lo nasconde
    (0x42d0e8, w2 = 0);
  - lo mostra solo 0x42cf6c, chiamata dal cambio personaggio 0x148038 → 0x430e24, per gli id 0x1e..0x24 (`sub #0x1e`,
    `cmp #6`); i frame di `Ticket_Change_ftp` sono {0,2,3,4,1,5,3} (0xf26f10);
  - nel bfres le texture sono `Ticket_{TanukiMario,CatPeach,Link,AnimalBoy,Shizue,HoneKoopa}_Alb` [V, RomFS]. È un gettone
    con l'immagine di uno dei 7 personaggi DLC, non una moneta liscia.
- La visibilità del Ticket è un flag dell'intero modello (vt+0x88 sull'oggetto `[model+0x38][idx]`), non per vista. Nella variante A:
  - la vista 4+j mostra il piedistallo j;
  - lo stesso piedistallo j si vede anche nella vista j (P1..P4).

  Un Ticket visibile in P6-P8 comparirebbe quindi anche in P2-P4. Negli screenshot non succede [D: eden_7b8/7b10].
- `m3d8::MenuFrame` chiama `VwVisHook::Orig(v, j, false, b)` per 3 viste su 4 a ogni frame, e 0x430cc4 con on = 0 nasconde anche il
  Ticket (0x430cf8 → 0x430d44). Il gioco lo rimostra (0x42d4e0..0x42d594) solo se voce+0x130 = 1, cioè dopo un personaggio
  DLC di P1-P4. Negli screenshot P2-P4 non ne hanno.

### 1.3 Legami chiesti
- **Slot del viewer (0..3 / 4..7):** no. Le monete del corpo non passano dal viewer, e il Ticket è escluso (§1.2).
- **KartUnit:** sì, corpo del gruppo X dell'unità. Non dipende dallo **stato di caricamento** (+0x358) né dal "modello non
  pronto": BattleCoin nasce sempre, ItemCoin per il tipo 0, entrambi nel ctor del corpo [V].
- **Elemento della stanza visto dalle camere extra:** no. Nessuna moneta nei modelli della stanza (stringhe 0x429ffc..0x431d6c [V]).

## 2. Q2 – Perché P6-P8 e non P5

### 2.1 La tabella a 4 voci: lo slot di vista X+0xac [V]
Ctor X (0x16fa58): `X+0xa8 = indice kart`, `X+0xac = -1` (0x16fab0/0x16fab4). Poi, solo se X+0xd0:
```
for s in 0..3:                                  // 4 slot di scena, srotolati: 0x16fdbc / 0x16fe18 / 0x16fe78 / 0x16fed8
    vp = scena[+0xb0 + 4*s]; if vp == -1: continue
    vp = (vp < 5) ? vp : 0                      // cmp #5 + csel (5 viewport di scena)
    if (scena.vp[vp].stato | 2) == 3 && scena.vp[vp].kart == X+0xa8 && X+0xac < 0:
        X+0xac = s                              // 0x16fe10 / 0x16fe70 / 0x16fed0 / 0x16ff30
// 0x16ff34: nessun altro slot
```
- Nel menu: slot→vp = {0,1,2,3}, kart dei vp = 0..3 [D: `maschere kart 01 02 04 08 00 00 00 00`]. Unità 0..3 → X+0xac = 0..3;
  **unità 4..7 → -1**.
- **Maschera intera delle monete** (0x11e91c reset; 0x121b18 per frame, solo BattleCoin):
  - `(corpo+0x508 && [+0x508]+8 == 2) ? 0xffff : (X+0xac < 0 ? 0xffffffff : 1 << X+0xac)`;
  - viene scritta con 0x68fb7c, che fa `strh` su ogni shape (0x68fbb0);
  - [V] 0x11ea0c..0x11ea1c, 0x121b2c..0x121b64.
- **La mod non raggiunge queste maschere:**
  - `ModelCreateHook` (0x69bb0c) azzera i bit 4..7 alla **creazione**, ma il reset del corpo li riscrive dopo (0x172760 →
    0x11e91c);
  - `UnitViewBit` scorre solo i nodi della lista (`0x113c88` → nodo+0x40). Il corpo vi entra come nodo corpo+0x60 (0x121ac8),
    le due monete no (spinte direttamente da 0x121ae8 / 0x121afc);
  - il gioco tocca solo le viste 0..3 dei nodi (0x16e2cc..0x16e3bc) e della mesh del corpo (0x173020 → 0x121b0c: corpo+0x10,
    non le monete).

**Risultato [V nella logica, da misurare]:**
| unità | X+0xac | maschera monete | viste in cui la moneta può comparire |
|---|---|---|---|
| 0..3 | 0..3 | `1 << slot` | solo la propria (0..3); nell'originale 4P non si vede nulla, quindi lì la moneta è nascosta da altro (§2.2) |
| 4..7 | -1 | 0xffff | **tutte**, comprese 4..7 |

### 2.2 Perché non P5 (vista 4) [I]
La tabella sopra spiega perché P1-P4 non mostrano mai le monete delle unità 4..7 nelle viste extra. Non spiega perché la vista 4
non le mostri e le viste 5..7 sì: l'unità 4 ha lo stesso -1. Nell'originale a 4 la moneta di ogni unità (maschera della propria
vista, spinta a ogni frame) non si vede. Esiste dunque uno stato che la rende invisibile nelle viste elaborate "bene".
- **H-A (preferita).**
  - Lo stato per vista della moneta viene calcolato solo per le prime **5** viste: 4 giocatori + 1, come i 5 viewport di scena
    (0x16e184 legge i bit 0..4, il ctor X limita il vp con `cmp #5`). Esempi di stato per vista: culling, LOD, visibilità o
    posa per vista.
  - Nelle viste 5..7 resterebbe il valore iniziale, "visibile". Le monete delle unità 4..7 (maschera 0xffff) si vedrebbero
    quindi **solo in 5..7**, sovrapposte all'origine. È quanto si osserva.
  - Le monete 0..3 (maschera 0..3) e le viste 0..4 (stato elaborato) restano pulite.
  - Il sito con il limite a 5 **non è stato trovato**: ci sono 30 `cmp #5` nel renderer 0x660000..0x740000, non analizzati uno
    per uno.
- **H-B.** Le maschere a runtime non sono quelle previste (un altro scrittore non trovato), per esempio `{5}`, `{6}`, `{7}` sulle
  monete delle unità 5..7 e `{4}` più "non visibile" sull'unità 4.
- **Escluse dal codice [V]:**
  - array per kart da 5 voci letti fuori limite: `dg+0x218` ha 8 voci [D];
  - slot del viewer: il Ticket è escluso;
  - ViewCount dei modelli moneta: 0x8807a8 crea con 4 (0x8807d4), `ModelCreateHook` porta a 8.

## 3. Q3 – Correzione fedele al flusso (dopo le misure del §4)

### F1 – Tabella degli slot di vista del kart a 8 (1 hook inline) [V siti, I effetto]
Stesso principio del flusso a N = 8. Nella selezione a 8 lo slot dell'unità k è la vista k: le celle 0..3 sono i Sub0..3, le
4..7 le viste extra. Basta estendere il ciclo dei 4 slot. Da lì il gioco ricava le maschere `1<<k` sia al reset (0x11e91c) sia
per frame (0x121b18), con le stesse istruzioni usate per 0..3.
```cpp
HOOK_DEFINE_INLINE(KartSlot8Hook) {      /* 0x16ff34 bd40f660 ldr s0,[x19,#0xf4]: dopo i 4 slot del ctor X 0x16fa58 (anche bersaglio del cbz 0x16fdb4) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* X = (uint8_t*)ctx->X[19];
        int32_t k = *(int32_t*)(X + 0xa8), &slot = *(int32_t*)(X + 0xac);
        if (!X[0xd0] || slot >= 0 || k < 4 || k >= 8) return;
        if (!m3d8::N8() || menu8::LocalCount() <= 4) return;       /* solo menu a 8 (KartDirector n = 8, 5-8 locali) */
        slot = k;                                                  /* come 0x16fe10..0x16ff30 per s = k */
    }
};
// installazione dentro il blocco MK8DX_P7_MENU3D8: KartSlot8Hook::InstallAtOffset(0x16ff34);
```
- **Prerequisito: audit dei lettori di X+0xac** (18 siti verificati in coin_verify: 0x113280, 0x11e9e8, 0x11ea2c, 0x121b2c,
  0x147f34, 0x16d51c, 0x16fe08/64/c4, 0x16ff24, 0x1708b4, 0x172a58/aa0/af8/b50, 0x175c28, 0x177c94, 0x17a90c).
  - **0x147f34 (rischio alto).** Il menu-kart confronta X+0xac con obj+0xc0 e, se coincidono, assegna la camera in
    `0xc9074(CameraDirector+0x1f8, k, ...)`. Il gestore ha 4 slot (`cmp w22,#4; csel` a 0xc8e98). Con 4..7 potrebbe sovrascrivere
    la camera dello slot 0. Oggi questo ramo non scatta (la ricerca a 0x147f48 non trova un vp).
  - 0x16d51c (0x16d500, visibilità per vista 0..3: con k ≥ 4 nessun `k == slot`, innocuo).
  - 0x1708b4 (copiato in corpo+0x214).
  - 0x175c28 / 0x177c94 (passati a 0x4217dc e a una funzione con `cmn #-1`: probabilmente suono o voce per giocatore).
  - Se l'audit trova limiti a 4, F1 va ristretto alle due formule maschera (F1′, sotto).
- **F1′ (variante ristretta, 2 trampolini, meno fedele):** dopo `0x11e91c` (prologo `d101c3ff`) e dopo `0x121b18` (prima parola
  `aa0003e8`, `b` da 0x16d828 → trampolino sul bersaglio):
  - condizione: menu a 8 e `X+0xac < 0` e `4 ≤ X+0xa8 < 8`;
  - azione: riapplicare `0x68fb7c(moneta, 1 << X+0xa8)` a corpo+0x430/+0x438. Per 0x121b18 solo con on = 1, altrimenti 0;
  - è la stessa formula con lo slot esteso, ma solo in quei due punti.
- **Effetto atteso:**
  - con H-B o "maschera sola": monete 4..7 limitate alla propria vista e invisibili come in P1-P4, cioè nessuna moneta;
  - **con H-A**: la moneta k resta visibile nella vista k (5..7), perché lì lo stato per vista non viene calcolato. Serve F2.

### F2 – (solo se H-A) estendere a 8 l'elaborazione per vista limitata a 5
Il sito si cerca con la misura M3, poi un'analisi statica mirata. Il candidato è il percorso "calc per vista" del modello,
ristretto ai `cmp #5` di 0x6a5204..0x6a53b0, 0x6b1590..0x6b1928, 0x6ad740, 0x6aff34. Non è parte di questo report.

### Conteggio
| fase | patch/hook |
|---|---|
| misure (§4) | 0 hook nuovi: log in `m3d8::MenuFrame` + opzione `MK8DX_P7_COINTEST` (prove T1..T4, scritture one-shot di maschera) |
| F1 | 1 hook inline (0x16ff34) + audit dei 18 lettori |
| F1′ (alternativa) | 2 trampolini (0x11e91c, 0x121b18) |
| F2 (se H-A) | da definire (1-2 siti attesi) |

## 4. Misure prima della correzione (solo log, niente GDB, niente hook nei percorsi caldi)
Tutto in `m3d8::MenuFrame` (già per frame), ogni 120 frame mentre `On8()`. Per k = 0..7: `u = Unit(d,k)`. Per sel = 0,1:
`X = *(u + 0x248 + sel*0x78 + 0x18)`, `corpo = *(X+0x50)`.

- **M1 – stato per unità.** Loggare:
  - `u+0x348`, `u+0x34c` (sel attivo/ricostruzione), `u+0x44/0x45/0x4c`;
  - `X+0xa8`, `X+0xac`, `X+0xd0`, `X+0x1ce`;
  - `corpo+0x48c`.
- **M2 – modelli moneta.** Per m in {corpo+0x430, corpo+0x438} loggare:
  - `m`, `m+0xe0` (ViewCount), `m+0x99` (bit 2 = in lista questo frame), `m+0x20`/`m+0x9c` (shape);
  - l'OR e l'AND delle maschere `[[m+0x28]+i*0x38]+0xc` per i < min(+0x20, +0x9c), come 0x68fb20.

  Atteso con H-A: unità 0..3 → `1<<k`; unità 4..7 → `ffff`; ItemCoin con `+0x99` bit 2 a ogni frame.
- **M3 – stato per vista (discrimina H-A/H-B).** Per l'ItemCoin dell'unità 5 e dell'unità 4 loggare per v = 0..7 i primi 16 byte di
  `[m+0x60 + v*0x58]` (voce per vista) e `m+0x58 + (v*0xc)`. Lo stesso per il modello corpo+0x10 come riferimento. Se le voci
  5..7 differiscono sistematicamente dalle 0..4 (valori iniziali contro calcolati), H-A è confermata e si ha il punto di partenza
  per F2.
- **Prove one-shot** (`MK8DX_P7_COINTEST`, una scrittura al primo frame a 8 e poi ogni frame, solo nel menu):
  - **T1:** `0x68fb7c(m, 0)` per entrambe le monete delle unità 4..7. Le monete spariscono → oggetto identificato.
  - **T2:** solo ItemCoin. **T3:** solo BattleCoin. Dicono quale dei due.
  - **T4:** `0x68fb7c(m, 1<<k)` per le unità 4..7, cioè l'effetto di F1′/F1. Se sparisce tutto → basta F1. Se la moneta resta
    in P6-P8 (dalla propria unità) → H-A, serve F2.
  - **Controllo Ticket:** per i = 0..3 loggare `v+0x568+i*0x138+0x130`. Loggare anche la visibilità del Ticket
    (`[[mdl+0x38][+0xc8]]->vt+0x90(+0xca)`, come 0x42d544..0x42d560). Atteso: 0, falso.
- **Accettazione della correzione:**
  - nessuna moneta in P1-P8 con 8 cursori in movimento;
  - 4P invariato;
  - maschere M2 delle unità 4..7 = `1<<k`;
  - nessuna regressione di camera nel menu a 8 (verifica di 0x147f34).

## 5. Punti incerti
1. **Quale moneta.** ItemCoin (preferita: spinta a ogni frame, moneta liscia) o BattleCoin (spinta solo con conteggio ≥ 1, che
   nel menu vale 0). Lo dicono T2/T3.
2. **P5.** Il meccanismo che separa la vista 4 dalle 5..7 non è stato trovato nel codice (H-A). È la parte meno solida:
   confidenza ~50 % su H-A, ~30 % H-B, ~20 % altro.
3. **Visibilità originale.** Nell'originale non si sa perché la moneta delle unità 0..3, spinta e mascherata sulla propria vista,
   non si veda (ossa nascoste per default? stato per vista?). M2/M3 lo mostrano.
4. **Rischio F1.** Lettori di X+0xac con limiti a 4, in particolare 0x147f34 / CameraDirector a 4 slot. Va fatto l'audit statico
   prima di installarlo.
5. Il "dietro al kart, sopra il piedistallo" è dedotto dalla posizione all'origine (`pos=0,0,0` di tutti i modelli del menu) e
   dagli screenshot; la posa delle ossa dell'ItemCoin senza monete non è stata letta dal bfres.

## 6. Dati
- `step7b_data/coin_verify.py` / `.txt`: 294 OK, 0 FAIL. I nomi RomFS sono controllati con
  `--romfs <dir con BaseTable.bfres e BattleCoin.bfres>`, estratti con
  `tools/bktr.py keys <xci>@0x17018600 <nsp>@0x1a23d0 5ac982f853e174d8bb8b1f67e022af02 get <path> <out>`.
- Screenshot di riferimento: `eden_7b8_sel8.png` (monete in P6, P7, P8; P5 senza), `eden_7b10_sel8.png`, `eden_7b6_sel8.png`.
  In `eden_7b3_sel8.png` le monete non si vedono perché le celle da 180 tagliavano la parte alta del modello.

---

## 7. Misura 1 (4 nov, sonda coindiag in Eden) e rianalisi

### 7.1 Dati [D]
- Per k = 0..7, sel attivo: X = voce+0x18 valido, X+0xa8 = k, **X+0xac = 0..3 per 0..3 e -1 per 4..7** (come previsto dal §2.1),
  X+0xd0 = 1, X+0x1ce = 0, corpo valido, corpo+0x48c = 0; **corpo+0x430 = corpo+0x438 = NULL per tutte le unità**.
  u+0x348 = 0/1, u+0x34c = -1, u+0x44 = u+0x45 = u+0x4c = 1. T1 (0x68fb7c(m,0)) quindi senza effetto.
- L'utente: moneta liscia con fessura, in piedi dietro il kart sopra il piedistallo, in P6/P7/P8 (mai P5), sia nella pagina
  personaggi sia nella pagina kart.

### 7.2 Perché le monete del corpo non ci sono [V]
0x11cd6c, prima della `bl 0x11d508` (creazione BattleCoin/ItemCoin):
*(estratto di disassemblato del gioco rimosso)*
Nel menu (A+8 = 5) le due monete non vengono mai create: errore mio nel §1 (avevo visto la creazione ma non la guardia del chiamante).

### 7.3 Altri modelli moneta controllati
- **RomFS del menu**: `LocalRoom.szs` (40,9 MB decompresso) ed `Earth.szs` non contengono nomi con coin/ticket/medal/gold: LocalRoom
  ha solo pezzi di kart di scena (Body/Wing/Arm/Tire `_Std`), saracinesca, cartelli [V, strings sui file decompressi con yaz0.py].
- **BaseTable**: unico modello a moneta del menu resta il **Ticket** (gettone DLC, §1.2).
- Modelli kart/pilota: la ricerca per nomi nel main non trova parti "coin" fuori da oggetti/gara; le parti kart caricate per
  nome a runtime non sono enumerabili staticamente [non verificato].

### 7.4 Ipotesi aggiornate
Vincolo dai dati: l'oggetto è visibile nelle viste 5, 6, 7 e **non** nelle 0..4, per entrambe le pagine; quindi o ha bit di
maschera 5..7 senza 0..4, oppure ha bit più larghi ma uno stato per vista che lo scarta nelle 0..4.
| # | ipotesi | prob. | per | contro |
|---|---|---|---|---|
| H-C | **modello del menu nato con ViewCount ≠ 4** (contenitori di R o altri, ViewCount = viste di R = 8 nel menu): `ModelCreateHook` pulisce i bit 4..7 **solo se `before == 4`** (main.cpp, condizione `before == 4 && model[0xe0] >= 8`), quindi la sua maschera resta quella di default in 4..7; il gioco lo nasconde solo nelle viste che conosce (0..3, + la 4 se gestisce 5 viewport di scena) | 40 % | spiega P6-P8 su entrambe le pagine e l'indipendenza dal personaggio; la mod non ha mai toccato questi modelli | il bit 4 spento va ancora spiegato (gestione a 5 viewport) |
| H-T | **Ticket** del piedistallo 1..3 (visibile in 4+j) con la shape Ticket fuori dal ciclo di 0x68fa38 (`n = min(+0x20, +0x9c)`, 0x68fa44): la sua maschera non viene mai aggiornata né dal gioco né dalla mod | 15 % | si trova esattamente "sopra il piedistallo" | le texture sono gettoni DLC (non moneta liscia); il piedistallo 0 (P5) dovrebbe averlo uguale |
| H-K | parte nascosta dei modelli kart/pilota/vetro delle unità 5..7 (nodo non visitato da `UnitViewBit`, che scorre solo la lista del sel attivo; nodi del sel inattivo mantengono i bit della sessione precedente) | 15 % | unità 5..7 = celle 6..8 | l'unità 4 andrebbe uguale; forma "moneta" non attesa |
| H-? | altro (modello UI3D creato da un percorso non visto) | 30 % | — | — |

### 7.5 Misura decisiva proposta (log + un inline/trampolino già esistente, niente GDB)
Due pezzi, entrambi nel menu e fuori dai percorsi per frame del renderer.

**A. Registro delle creazioni** – dentro il `ModelCreateHook` esistente (trampolino a 0x69bb0c, prima parola `a9ba6ffc`,
`(model, arg, heap)`): dopo `Orig`, nel menu (`MK8DX_P7_MENU3D8 && rlm::sceneType != 3`) annotare modello, `before`
(= arg+8 originale), ViewCount, e 6 indirizzi di ritorno dalla catena x29 (la creazione non è un percorso caldo).
```cpp
namespace coinreg {
    struct Rec { uint8_t* m; int8_t before; uint8_t vc; uint32_t lr[6]; };
    static Rec tab[2048]; static int n;
    static void Note(uint8_t* m, int before) {
        if (!m || n >= 2048) return;
        Rec& r = tab[n++]; r.m = m; r.before = (int8_t)before; r.vc = m[0xe0];
        uintptr_t base = mk8dx::Base();
        r.lr[0] = (uint32_t)((uintptr_t)__builtin_return_address(0) - base);
        uintptr_t* fp = (uintptr_t*)__builtin_frame_address(0);
        for (int i = 1; i < 6; i++) {                       /* catena x29: [fp] = fp precedente, [fp+8] = lr */
            if (!fp || ((uintptr_t)fp & 0xf)) break;
            fp = (uintptr_t*)fp[0]; if (!fp || ((uintptr_t)fp & 0xf)) break;
            r.lr[i] = (uint32_t)(fp[1] - base);
        }
    }
}
// in ModelCreateHook::Callback, dopo `uint64_t r = Orig(...)`:
//   if (MK8DX_P7_MENU3D8 && rlm::sceneType != 3) coinreg::Note(model, before);
```
**B. Scansione delle maschere** – in `m3d8::MenuFrame` (già per frame), quando `on`, al frame 60 e poi ogni 600: per ogni
record calcolare OR e AND delle maschere di shape come 0x68fa38/0x68fb20 (shape i = `*(uint8_t**)(*(uint8_t**)(m+0x28) + i*0x38)`,
maschera `u16` a shape+0xc, `n = min(u32 m+0x20, u16 m+0x9c)`, saltare se n = 0 o n > 512 o m+0x28 nullo) ed escludere i modelli
noti (piedistalli `*(v+0x570+i*0x138)`, i = 0..3; modelli dei nodi `*(w+0x40)` delle liste `u+0x248+sel*0x78`, sel = 0,1, per le 8
unità). Loggare i restanti con `(or & 0xe0) != 0`:
```
[mk8dx-8p] coinreg #idx m=%p before=%d vc=%u n=%u or=%04x and=%04x q99=%02x R=%d lr=%x<%x<%x<%x<%x<%x
```
(`q99` = m+0x99, bit 2 = spinto nella lista del frame da 0x691dd0; `R` = `*(uint8_t**)(m+0xf0) == proto::R`).
- **Prova di conferma one-shot** (`MK8DX_P7_COINTEST2=1`): sui candidati loggati applicare una volta
  `0x68fb7c(m, or & ~0xe0)` (ripristinabile, si logga il valore precedente) e chiedere all'utente se la moneta sparisce.
- **Lettura**: la catena lr identifica il creatore (es. 0x429a34 = caricatore generico UI3D 0x4298f8 → `bl 0x8806c4` a 0x429a30,
  usato da BaseTable/Earth/...; 0x11d6a0-like = corpo kart; 0x6aa8xx = contenitori di R). `before` ≠ 4 conferma H-C; un modello
  piedistallo con or ⊇ 0xe0 e la shape Ticket fuori dal ciclo conferma H-T (per questo nel log aggiungere, per i 4 piedistalli,
  `u16 m+0x9c`, `u32 m+0x20` e le maschere di **tutte** le `m+0x20` shape, non solo delle prime n).
- **Ticket (in parallelo, per i = 0..3)**: `e = v+0x568+i*0x138`; loggare `e[0x130]`, `int16 e+0xc8 / e+0xca` e la visibilità come
  0x42d544..0x42d560: `o = *(void**)(*(uint8_t***)(mdl+0x38))[lo]`; `vt+0x90(o, hi)`.

### 7.6 Correzione (dopo la misura)
- Se **H-C**: estendere la regola della mod nel punto in cui già opera – in `ModelCreateHook` applicare la pulizia dei bit 4..7
  a tutti i modelli del menu con ViewCount ≥ 8 (oggi solo `before == 4`), se il creatore loggato è un modello che il gioco nasconde
  per vista (non R-interno). 0 hook nuovi, 1 condizione cambiata. Da verificare che i modelli interni di R non ne dipendano.
- Se **H-T**: estendere il ciclo per-shape del piedistallo (ultima shape esclusa da `min(+0x20,+0x9c)`) applicando i bit 4..7
  a tutte le `+0x20` shape nella variante A (in `MenuFrame`, dove già si chiamano le guardie). 0 hook nuovi.
- Se **H-K**: `UnitViewBit` anche sulla lista del sel inattivo (spegnere 4..7). 0 hook nuovi.
- F1/F1′ (§3) **non vanno installati**.

---

## 8. Misura 2 – decisione di disegno per vista

### 8.1 Dati della Misura 1 [D]
685 modelli creati da 0x69bb0c fuori gara; a f=60 e f=600 con le anteprime a 8 nessuno ha bit 0xe0 nell'OR delle maschere
`[*(m+0x28)+i*0x38]+0xc`; le monete restano in P6-P8. Quindi l'oggetto disegnato in 5..7 non è un sotto-oggetto di quei modelli
come li legge 0x68fa38 (oppure non nasce da 0x69bb0c). Serve guardare il disegno.

### 8.2 Dove si decide se un oggetto va in una vista [V]
*(estratto di disassemblato del gioco rimosso)*
- **Indice di vista** [V]: le voci sono create per frame da 0x721524 solo per le viste attive (chiamata da 0x6abcac):
  `voce+0 = V+8`, `voce+2 = 1 << V+8` (0x721580/0x72158c); `V+8 = i` è scritto una sola volta alla creazione della vista
  (0x6bca74). Per i layer extra della mod V è scelto da `layer+0x1088` = 4..7 con clamp sulla vista 0 solo se fuori da R+0xf0
  (= 8). Quindi nelle viste extra l'indice usato **è 4..7**, non un altro: un oggetto compare nella vista k se e solo se
  `obj+0xc & R+0x5d14` ha il bit k. Una moneta in 5..7 e non in 4 ha dunque, al momento del disegno, i bit 5..7 (o alcuni) e non
  il bit 4 nella maschera **dell'oggetto passato a 0x6e2430**.
- **Lista dei nodi**: i nodi che arrivano al job sono quelli del DLM costruiti dagli oggetti spinti per frame (0x691dd0 mette il
  modello in `R+0x55c8` = DLM+0x110, contatore DLM+0x108) [I forte sul collegamento lista→nodi]. Gli oggetti con la maschera
  sono quelli a cui puntano i nodi; la Misura 1 ne leggeva un'altra struttura (gli elementi di `m+0x28` dei modelli registrati),
  per cui un oggetto diverso (o non registrato) poteva sfuggire.

### 8.3 Hook di misura (un solo frame, trampolino a 0x6e2430)
Sito: **0x6e2430**, prima parola `fc190fe8` (`str d8,[sp,#-0x70]!`, non relativa al PC). Funzione chiamata per ogni oggetto e
per frame dai 3 worker: il callback deve costare un confronto quando non armato.
```cpp
namespace drawreg {
    static volatile int armed;                       /* 0 spento, 1 = registra questo frame, 2 = da scaricare */
    struct Hit { uint8_t* o; uint16_t vs, mask; };
    static Hit hits[1024]; static int nh;
}
HOOK_DEFINE_TRAMPOLINE(DrawObjHook) {                /* 0x6e2430 fc190fe8: (obj, passi, voci, n voci, R, nodo+0x20, ctx, core) */
    static void Callback(uint8_t* o, uint32_t pass, uint8_t* ent, uint32_t cnt, uint8_t* R, void* l, void* c, uint32_t core) {
        if (drawreg::armed == 1 && R == proto::R && o) {
            uint16_t m = *(uint16_t*)(o + 0xc) & (uint16_t)*(uint32_t*)(R + 0x5d14), vs = 0;
            for (uint32_t i = 0; i < (cnt & 0xffff) && i < 16; i++) {
                uint8_t* e = ent + i * 0x3b8;
                if (m & *(uint16_t*)(e + 2)) vs |= (uint16_t)(1u << (*(uint16_t*)e & 15));
            }
            if (vs & 0xf0) { int k = __atomic_fetch_add(&drawreg::nh, 1, __ATOMIC_RELAXED);
                             if (k < 1024) drawreg::hits[k] = { o, vs, *(uint16_t*)(o + 0xc) }; }
        }
        Orig(o, pass, ent, cnt, R, l, c, core);
    }
};
/* installazione nel blocco MK8DX_P7_MENU3D8 (solo con MK8DX_P7_COINTEST): DrawObjHook::InstallAtOffset(0x6e2430); */
```
Armamento e scarico in `m3d8::MenuFrame` (dopo `if (!on) return;`):
```cpp
static unsigned long onF; onF++;
if (onF == 300) { drawreg::nh = 0; drawreg::armed = 1; }
else if (onF == 302 && drawreg::armed == 1) {               /* due frame: il job del frame armato e' finito */
    drawreg::armed = 2;
    /* oggetti noti: sotto-oggetti dei piedistalli e dei modelli nodo dei KartUnit, come li legge 0x68fa38 */
    auto known = [&](uint8_t* o) -> int {
        auto in = [&](uint8_t* m) { if (!m) return false; uint32_t n = *(uint32_t*)(m + 0x20); uint16_t n2 = *(uint16_t*)(m + 0x9c);
            if (n2 < n) n = n2; uint8_t* a = *(uint8_t**)(m + 0x28); if (!a || n > 512) return false;
            for (uint32_t i = 0; i < n; i++) if (*(uint8_t**)(a + i * 0x38) == o) return true; return m == o; };
        if (uint8_t* v = Viewer()) for (int i = 0; i < 4; i++) if (in(*(uint8_t**)(v + 0x570 + i * 0x138))) return 1;
        for (int k = 0; k < 8; k++) if (uint8_t* u = Unit(d, k)) for (int s = 0; s < 2; s++) {
            uint8_t* lst = u + 0x248 + s * 0x78; int g = 0;
            for (uint8_t* nd = *(uint8_t**)(lst + 8); nd && nd != lst && g < 64; nd = *(uint8_t**)(nd + 8), g++)
                if (uint8_t* w = *(uint8_t**)(nd + 0x10)) if (in(*(uint8_t**)(w + 0x40))) return 2 + k; }
        return 0; };
    int n = drawreg::nh < 1024 ? drawreg::nh : 1024;
    for (int i = 0; i < n; i++) { drawreg::Hit& h = drawreg::hits[i]; bool dup = false;
        for (int j = 0; j < i; j++) if (drawreg::hits[j].o == h.o) { dup = true; break; } if (dup) continue;
        int kn = known(h.o); uint64_t* q = (uint64_t*)h.o; uintptr_t b = mk8dx::Base();
        auto rel = [&](uint64_t x) -> uint64_t { return (x >= b && x < b + 0x2000000) ? x - b : x; };
        Logging.Log("[mk8dx-8p] drawreg %s%d o=%p viste=%04x mask=%04x vt=%lx +8=%02x +12=%04x +1e=%02x q10=%lx q18=%lx q20=%lx q28=%lx q30=%lx",
            kn == 0 ? "?" : kn == 1 ? "piedistallo" : "unit", kn >= 2 ? kn - 2 : 0, h.o, h.vs, h.mask, rel(q[0]), h.o[8],
            *(uint16_t*)(h.o + 0x12), h.o[0x1e], rel(q[2]), rel(q[3]), rel(q[4]), rel(q[5]), rel(q[6]));
    }
    Logging.Log("[mk8dx-8p] drawreg: %d oggetti con viste 4..7 (frame %lu)", n, frames);
}
```
Prova di conferma (opzionale, stesso scarico): per gli oggetti `?` con `viste & 0xe0` e senza `0x10`, una volta
`*(uint16_t*)(o + 0xc) &= ~0xe0` (loggando il valore precedente) e chiedere all'utente se la moneta sparisce.

### 8.4 Come leggere il risultato
- Atteso per i noti: `unit k` con viste = `1<<k` (k = 4..7, anche 0..3 senza filtro), `piedistallo` con `1<<(4+j)`.
- La moneta: una o più righe con viste ⊇ 0x20/0x40/0x80 e senza 0x10.
  - `?` → oggetto fuori dai modelli noti: `vt` (relativa al main) dà la classe, `q10..q30` puntatori padre da seguire
    staticamente (es. vt dei sotto-oggetti di gsys, ModelObj in +0xc8 del modello padre); poi si cerca chi lo crea.
  - `piedistallo` con viste 0x20..0x80 e bit 1..3 assenti → sotto-oggetto del BaseTable (Ticket) con maschera propria (H-T).
  - `unit k` con viste che non sono `1<<k` → nodo dell'altra lista o maschera sbagliata (H-K).
  - se le maschere `mask` hanno bit 5..7 ma nessuna riga `?`: i sotto-oggetti letti da 0x68fa38 non sono gli oggetti dei nodi
    di disegno; la Misura 1 andrebbe rifatta con gli `o` di questa misura.
- Costo: fuori dal frame armato un caricamento e un confronto per oggetto; nel frame armato ≤ 16 iterazioni e un atomico.

### 8.5 Rischi
- Firma: i registri x0..x7 sono quelli di 0x72144c [V]; `w7` è passato come `uint32_t`. Il trampolino conserva tutto.
- Gli oggetti di altri renderer (`R != proto::R`) sono ignorati; se la moneta fosse in un altro renderer non comparirebbe:
  in quel caso togliere il filtro su R e loggare anche `R`.
- [I] il nome della risorsa non è disponibile al disegno: si ricava da `vt`/padre in una seconda analisi statica.

## 9. Esito in Eden e correzione (4 nov 2026)
- Misura 1 (registro delle creazioni + maschere delle shape): 685 modelli, nessuno con bit 5..7 nelle maschere delle shape -> negativo.
- Misura 2 (DrawObjHook 0x6e2430, un frame): candidati = solo nodi dei KartUnit (vista propria) e piedistalli; 4 oggetti + R con
  maschera di disegno (+0xc) **0xffff** (tutte le viste).
- Bisezione per cella (COINTEST=7: oggetti 0xffff spenti per gruppo +0x1e) -> la moneta sparisce nella cella del gruppo +0x1e = 0x0c.
- Identificazione: quell'oggetto e' un **modello** (vtable 0x12b53f0 = classe modello, ctor 0x69b8e0, init 0x69bb0c), registrato come #4,
  creato **all'avvio** (before = 4, ViewCount portato a 8 da ModelCreateHook), catena 0x68eb98 < 0x68e9d0 < 0x880794 < 0x7f8aac
  (caricatore `%s/%s.bfres`) < 0x7f866c < 0x2744b8. Maschera +0xc = 0xffff mai ristretta: nel menu a 8 e' disegnato anche nelle viste
  extra; nelle viste 0..4 resta fuori inquadratura. La pulizia di ModelCreateHook agiva sulle shape, non sulla maschera del modello.
- **Correzione** (`DrawObjHook`, sempre installato con MK8DX_P7_MENU3D8): nel menu con anteprime a 8 (`drawreg::menu8`), per gli oggetti
  del renderer Main con maschera 0xffff la maschera e' ridotta a 0xff0f solo durante il disegno (viste extra 4..7 escluse), poi
  ripristinata. Non attiva in gara (tipo scena 3). Verificato dall'utente: nessuna moneta in nessuna cella, KartUnit e piedistalli
  presenti in tutte le 8 celle. Misure restano con `MK8DX_P7_COINTEST` (registro, coindiag, drawreg).
