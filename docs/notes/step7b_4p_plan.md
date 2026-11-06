# Step 7b – 4P bloccato dopo la selezione personaggi con il "flusso a 8": causa e piano

Data: 4 nov 2026. Lavoro solo statico (`img/main.ann.asm`, `img/main.bin`) più i log Eden già presenti in `step7b_data/`.
Eden e GDB non sono stati avviati e `src/` non è stato modificato.
Verifica: `step7b_data/fourp_verify.py` → `step7b_data/fourp_verify.txt` (**197 OK, 0 FAIL**). Lo script controlla:
- le parole a 4 byte;
- i bersagli di `bl`, `b`, `b.cond`, `tbz` e `cbz`;
- le 5 tabelle di salto;
- la codifica delle 2 parole nuove.

Etichette: **[V]** verificato nel disassemblato, **[D]** dato dai log, **[I]** ipotesi.

---

## 0. Sintesi
- **Causa esatta.** Il blocco non è nell'avanzamento della selezione (0x4ba938). È nell'**uscita** dalla pagina (vt87 0x4badb0), allo
  **stadio 4**: `0x14b5a4()` deve restituire vero, e lo fa solo se **tutti i modelli visibili del KartDirector** sono nello stato
  "fermo" (+0x191 = 0) oppure "deciso" (+0x191 = 4/0xb con +0xd5 ≠ 0). Il ciclo scorre `dir+0xc0` = **8** unità, non le +0x8c0 = 4 della
  selezione [V].
  - In 4P le unità 4..7 sono **visibili** (`obj+0xd4 = 1`, log "unit4..7 … vis=1" [D]) ma nessun giocatore le conferma, quindi
    0x14b5a4 resta falso per sempre: niente pagina kart, nessun crash.
  - Le unità 4..7 sono visibili perché l'originale nasconde le unità in eccesso solo nell'intervallo `[+0x8c0, 4)`
    (0x4b8658..0x4b8678, limite **4 fisso**). Intanto il nostro `MenuKartCfgHook` le configura con il tipo 2 ("tutte visibili")
    come le 0..3.
- **Nell'originale** il numero di KartUnit del menu **non** è N. È la **capacità** (4): la decide la chiamata di default
  `0x869180(rm, 4)` a 0x4f9e60, seguita da `0x869a14` al cambio di scena. Il KartDirector nasce **prima** che si scelga il modo.
  Il pulsante Ng (0x4d99bc) cambia solo le entry e i locali.
  - In 1-3P le unità N..3 esistono ma vengono **nascoste** all'ingresso nella selezione. Anche i descrittori B/A 2..3 restano quelli
    dell'avvio, quindi in 2P si ha +0x180 = 4.
  - Il default 4 vale quindi anche in 2P.
- **Piano (fedele al flusso, 0 hook nuovi).**
  - Portare il ciclo "nascondi le unità oltre N" da 4 a 8, con **2 parole**:
    - 0x4b865c `71000edf` → `71001edf`;
    - 0x4b8674 `710012df` → `710022df`.
  - Facoltativo ma consigliato: **togliere** la parola 0x4fa254 (ingresso nel menu) da kFlow8. Non influisce sul KartDirector, che
    viene inizializzato prima [D, 10 casi], e così fuori da 5-8g i locali tornano 4 come nell'originale.
  - kFlow8 passa da 5 a 6 parole: 0x4f9e60, i 3 limiti e le 2 nuove.
  - Con 1-4P il comportamento diventa quello dell'originale in 1-3P: le unità oltre N esistono ma sono nascoste.
- **Previsione verificabile:** con la build attuale anche il **3P** si blocca (tipo 2), mentre 1P e 2P passano (tipo 0/1 → unità 4..7
  già invisibili) [I].

---

## 1. Q1 – Come l'originale determina i giocatori del menu (1P..4P)

### 1.1 Sequenza [V salvo indicazione]
| momento | sito | cosa scrive |
|---|---|---|
| cambio di scena (avvio, ritorno da gara/demo) | 0x4f9e60 `orr w1,#4` → `bl 0x869180` (0x4f9e6c) | `0x869180(rm, 4, 1, 0)`: entry 0..3 umane, 4..15 CPU; locali rm+0x178/+0x17c; mappe rm+0x18c e rm+0x1cc (vedi 1.2) |
| subito dopo, stessa sequenza | 0x7e6100 → 0x8ac6a4 → `b 0x869a14` (0x8ac704) | **settings B** dalle entry valide: desc[k] = entry+0x210, conteggio con il limite del modo 5 (0x86f4ec = 4) → **B+0x180 = 4** |
| creazione della scena menu | 0x3d6c38 → `b 0x7e6e54` | **A ← B**, copia integrale con +0x180 |
| init KartDirector del menu | 0x13d7b8/0x13d7bc | `dir+0xc0 = A+0x180` = **4** (n KartUnit), slot di risorse 0..3 preparati e caricati |
| ingresso nel menu (dopo l'init, log [D]) | 0x4fa254 `orr w1,#4` → 0x4fa260 | `0x869180(rm, 4)`: di nuovo 4 locali di default (settings e KartDirector **non** ricalcolati) |
| pulsante Ng (eventi 0x13..0x1a) | 0x4d9774 `sub w21,w8,#0x12` (N = id−0x12); 0x4d99a8 cfg+0x1fa4 = N; 0x4d99bc `0x869180(rm, N, 1, 0)` | entry 0..N−1 umane, N..15 CPU; rm+0x178 = N; **niente** 0x869a14, **niente** KartDirector |
| configurazione dei modelli | 0x13e59c(dir, n, …) | stesso tipo per tutte le unità: n=1 → 0, n=2 → 1, n=3/4 → 2 (tabella a 0x131f32c); 0x147678: tipo 0 → visibile solo lo slot 0, tipo 1 → slot < 2, tipo 2 → **tutte visibili** |
| ingresso nella selezione (vt77) | 0x4b8308..0x4b862c, poi 0x4b8658..0x4b8678 | per i < +0x8c0: stato 3 → mostra, altrimenti nascondi (0x14a7ac); poi **nasconde i ∈ [+0x8c0, 4)** se +0x8c0 ≤ 3 |
| uscita dalla selezione (vt87, stadio 1) | 0x4bb000..0x4bb134 | `0x42022c(i, 4)` su 16 entry, poi tipo 0 per gli slot in stato 7. B.desc[k] viene aggiornato solo per le entry con k = +0x22c ≠ −1, poi il conteggio è rifatto (b 0x86f074) |

### 1.2 Cosa fa `0x869180(rm, N, a, b)` [V]
- Ciclo da N a 16:
  - i < N → `0x864fcc(e, &1)` (+0x44 = 1, umano) e `0x8657dc(e)`;
  - i ≥ N → `0x864fcc(e, &2)` (CPU).
- `0x865238(e, 0)` su tutte: per le CPU **e+0x230 = −1**. Per i < N, `0x8651c4(e, k++)` mette e+0x230 = k e **e+0x224 = 0**.
- `0x869040`:
  - e+0x22c = −1 e rm+0x1cc[0..11] = −1;
  - per le entry con +0x230 ≠ −1: e+0x22c = k, **rm+0x1cc[k] = indice**.
- Ricalcola i locali: rm+0x178/+0x17c (0x869618..0x869684).

### 1.3 Chi decide cosa (originale)
- **Entry/locali**: sempre `0x869180`. 4 di default al cambio di scena e all'ingresso nel menu, N al pulsante.
- **settings+0x180 e descrittori**: `0x869a14` **solo al cambio di scena**, con le entry di default (4), poi copiati in A. Nel menu cambiano
  solo i tipi dei descrittori delle entry che hanno ancora k. Quelli delle entry passate a CPU (2P: desc 2..3) restano tipo 0 dall'avvio,
  quindi **B+0x180 resta 4 anche in 1-3P** [I forte, ricavato dal codice di 0x42022c/0x86ebbc].
- **Tipo 4 nei descrittori 4..7** ("fine lista"): viene dalla pulizia di 0x869a14 (12 tipi = 4) per gli indici k ≥ numero di entry valide.
- **KartDirector n** = A+0x180 all'init della scena menu = **capacità** (4), non N. Il default 4 di 0x869180 vale anche in 2P: in 2P
  esistono 4 unità e le 2..3 vengono nascoste.
- **Con kFlow8**:
  - 0x4f9e60 = 8 → 8 entry valide in 0x869a14 → B+0x180 = 8 (limite 8) → **n = 8**. Il KartDirector a 8 nasce dal codice del gioco [D: log
    blocco righe 313-332, 383-467].
  - Il pulsante 4g rimette 4 locali (rm+0x1cc = {0,1,2,3,−1…}) ma non tocca KartDirector né descrittori. B.desc[4..7] resta tipo 0
    (log "slot4..7 desc 0 0 0 0 tipo=0" [D]).

---

## 2. Q2 – Dalla selezione personaggi alla selezione kart, e dove si ferma

### 2.1 Selezione (per frame, 0x4ba77c → ciclo 0x4ba3c0) [V]
Stati dello slot i (i < +0x8c0, array +0x904, rilocato dalla mod a +0xa28), tabelle 0xf29164/0xf29150:
- **2** → aspetta `obj+0x191 == 3` (0x14a858) → 3;
- **3** = sta scegliendo;
- **4** → aspetta `obj+0x191 == 0` → 1;
- **5** (confermato) → aspetta che la CharaList 0x28+i sia chiusa (+0x120 ∉ [3,7]) → 6 e `0x14b46c(i)`. Quest'ultima, se il modello è
  visibile, chiama 0x149b28: richiesta di stato 4 o 0xb in +0x1b8/+0x193, l'animazione "deciso";
- **6** → contatore +0x908 fino a 10 → **7**.

**Avanzamento** (0x4ba81c..0x4ba948): conta gli slot i < +0x8c0 in stato 7 e li confronta con `cfg+0x1fa4`. Se sono uguali chiama
`0x8ba5fc(this, this+0x1d0)` verso la pagina 0x31. Usa solo +0x8c0 e N, quindi in 4P funziona anche con 8 unità.

### 2.2 Uscita dalla pagina (vt87 0x4badb0, stadio +0x124, tabella 0xf2917c) [V]
| stadio | sito | condizione per avanzare (0x8bbec4) |
|---|---|---|
| 1 | 0x4baf64 | tipi delle entry (16 × tipo 4, poi tipo 0 per gli slot in stato 7), cfg+0x1434 |
| 2 | 0x4bb1bc | CharaList degli slot < +0x8c0 chiuse |
| 3 | 0x4bb278 | +0x938[i] ≤ 0 |
| **4** | **0x4bb2b4** | destinazione 0x2f..0x31 (kart = 0x31): **`0x14b5a4()` vero** (0x4bb2bc/0x4bb2c0); altrimenti `b 0x4bb4e8` = resta |
| 5 | 0x4bb2c8 | flag +0x220 della pagina 3 |
| 6 | 0x4bb350 | per il kart (w21 < 3) passa subito |
| 7 | 0x4bb36c | ≥ 25 frame (+0x12c); per il kart salta 0x14ad4c; poi 0x8bbc64 = apre la pagina successiva (onEnter kart 0x4c0898) |

**`0x14b5a4()`** [V] (0x14b5f4..0x14b6d8) scorre `i < dir+0xc0`, con `obj = 0x13d47c(dir, i)`:
- unità ignorata se `obj` è NULL o `obj+0xd4 == 0` (nascosta);
- ok se `+0x191 == 0`, oppure se `+0x191 ∈ {4, 0xb}` e `+0xd5 ≠ 0`;
- **qualunque altro stato → ritorna falso.**

È l'unico chiamante (0x4bb2bc). Gli altri controlli sulle unità (0x14ad4c stadio 7, 0x14ac5c stadio 6) contano anch'essi su
`dir+0xc0` e solo sulle unità visibili [V], ma per la pagina kart non vengono eseguiti.

### 2.3 Perché il 4P si ferma (sito, registri, campo)
- **Sito:** 0x4bb2bc `bl 0x14b5a4` → `w0 = 0` → 0x4bb2c0 `tbnz w0` non salta → 0x4bb2c4 `b 0x4bb4e8`. Lo stadio resta 4 per sempre.
- **Dentro 0x14b5a4:**
  - `w20 = dir+0xc0 = 8` (0x14b5f4) [D: "KartDirInit -> n=8", "m3d8 sel: … n=8"];
  - per i = 4: `obj+0xd4 = 1` (0x14b670) [D: "unit4 … vis=1", e così 5..7];
  - `obj+0x191` non è né 0 né 4/0xb [I forte: le unità 4..7 non ricevono mai la richiesta 4/0xb, perché 0x14b46c è chiamata solo per
    gli slot < +0x8c0, né la richiesta d'uscita 2, che arriva solo allo stadio 6];
  - quindi `w8 = 1` (0x14b6a8), `(w8|4)&7 = 5 ≠ 4` (0x14b6b4) → ritorna `w8 == 2` = falso.
- **Perché sono visibili:**
  1. al pulsante 4g il nostro `MenuKartCfgHook` (0x13e59c) estende la configurazione alle unità 4..7 con `kTipo[n−1]`. Con n = 4 il tipo è
     2 → `+0xd4 = 1` (0x1478d4) [D: "oggetti menu-kart configurati n=4 modelli=8 locali=4"];
  2. all'ingresso nella selezione 0x4b865c salta il ciclo "nascondi", perché +0x8c0 = 4 > 3, e comunque il ciclo si ferma a 4 (0x4b8674).
     Le unità 4..7 non vengono mai nascoste.
  - Non si vedono a schermo perché le loro maschere di vista sono vuote (`+0x362 = 0100`, "maschere kart 01 02 04 08 00 00 00 00" [D]).
    Per il ciclo di uscita però sono "visibili".

### 2.4 Ipotesi escluse con i log [D]
| ipotesi | prova |
|---|---|
| avanzamento 0x4ba938 non raggiunto | la selezione è uscita dal per-frame: l'unica sonda "m3d8 sel" è a fr = 90 (riga 1083) e la successiva (fr = 600) non compare, anche se il log va avanti ~890 frame (cache f = 1800, 2400). Senza la pagina kart (nessuna riga "C: sel", contro 4 nel log ok) resta solo la fase di uscita vt87 |
| applet / pad / locali | identici: applet (4,4) ritorno 1, race+0x178 = 4, +0x17c = {0..3}, cfg+0x1fa4 = 4 in entrambi i log |
| thread di caricamento morto | sys+0x5050 = 5 alla sonda, ma dopo ci sono le ricostruzioni di kart 1 e kart 2 (righe 1142, 1153) e la cache non segnala errori (f = 1800/2400: "-0") |
| memoria / HeapResident | allocabile 0xad1d30 (blocco) e 0xad10c0 (ok), praticamente uguali; nessun crash |
| settings+0x180 = 8 usato dall'uscita | no: lo stadio 4 usa `dir+0xc0` (che deriva da +0x180 all'init), non +0x180 |
| entry 4..7 tipo 0 / personaggi casuali | sono CPU (+0x44 = 2 da 0x869180(4)) e il ciclo di uscita non le legge. Sono un effetto collaterale, non la causa |

### 2.5 Previsione (da verificare): 1P, 2P, 3P con la build attuale
| modo | tipo dell'unità (0x13e59c) | visibilità 4..7 | esito previsto |
|---|---|---|---|
| 1P | 0 | invisibili | ok |
| 2P | 1 | invisibili | ok |
| 3P | 2 | **visibili** | **bloccato come il 4P** |

[I]

---

## 3. Q3 – Piano fedele al flusso originale

### 3.1 Scelta: capacità 8, visibilità = N (lo schema che l'originale usa già in 1-3P)
Nell'originale le unità del menu sono sempre 4, cioè la capacità, e quelle oltre N vengono nascoste. Con il flusso a 8 la capacità è
8, quindi lo stesso flusso richiede di nascondere fino a 8. Il ciclo di coda dell'ingresso in selezione (vt77) diventa:
```
// originale                                  // con N = 8 (2 parole)
w22 = +0x8c0                                  w22 = +0x8c0
if (w22 <= 3)                                 if (w22 <= 7)              // 0x4b865c cmp w22,#3 -> #7
  do 0x14a7ac(w22, false) while (++w22 != 4)    do 0x14a7ac(w22, false) while (++w22 != 8)   // 0x4b8674 #4 -> #8
```

| # | sito | originale [V] | nuova [V codifica] | effetto |
|---|---|---|---|---|
| H1 | 0x4b865c | `71000edf` cmp w22,#3 | `71001edf` cmp w22,#7 | ciclo eseguito anche con +0x8c0 = 4..7 |
| H2 | 0x4b8674 | `710012df` cmp w22,#4 | `710022df` cmp w22,#8 | nasconde le unità +0x8c0..7 |
| (R) | 0x4fa254 | `321e03e1` (#4) | **nessuna patch** (togliere da kFlow8) | locali di default 4 all'ingresso nel menu, come l'originale |

- **Sicurezza di H1/H2:**
  - con un KartDirector a 4, per esempio nella build senza opzione, `0x13d47c(dir, i ≥ 4)` restituisce NULL (0x13d488) e 0x14a7ac
    ritorna senza fare nulla (0x14a82c/0x14a848). Le parole sono quindi innocue anche fuori dal flusso a 8;
  - in 8P (+0x8c0 = 8) il ciclo resta saltato;
  - in 5-7P (+0x8c0 = 8 dalla mod p4) gli slot vuoti li nasconde già il primo ciclo (stato ≠ 3 → 0x4b83f8) [V codice, I a runtime].
- **(R) togliere 0x4fa254:** la chiamata arriva **dopo** l'init del KartDirector e dopo le ricostruzioni delle unità 4..7 [D: in tutti i
  10 ingressi nel menu dei log flow8/cache/stress, "KartDirInit -> n=8" precede "0x869180 … da main+4fa264"; in stress8 le ricostruzioni
  kart 4..7 avvengono dentro l'init]. Quindi non serve al flusso a 8. Lasciarla a 8 dà "8 locali" anche nei menu prima della scelta
  (singolo, online, wireless), che non è l'originale.
- **kFlow8 risultante (6 parole):** 0x4f9e60 #8, 0x86edd4/0x86f080/0x86f4ec #8, 0x4b865c #7, 0x4b8674 #8. Va applicato tutto o niente,
  con la verifica delle parole originali come oggi. Nessun hook nuovo, nessuna toppa che corregge effetti.
- **Commento da correggere in `MenuPresetHook`** ("1-4P: modelli 4..7 invisibili"): oggi in 3P/4P non lo sono. Con H1/H2 lo diventano.

### 3.2 Effetti sull'init del KartDirector e sulla sessione
- **Init.** Invariato rispetto alla build attuale: il KartDirector del menu viene creato **una volta per scena**, al cambio di scena e
  prima della scelta del modo, con n = 8 (0x4f9e60 → 0x869a14 → A+0x180 = 8). Il pulsante Ng non lo ricrea [D: un solo KartDirInit per
  ingresso nel menu].
- **4P.** Unità 4..7 nascoste all'ingresso in selezione. Nessuno le segna come cambiate (0x13d424 è chiamata solo nei cicli < +0x8c0) e
  restano nello stato 3, quindi lo scheduler non le tocca. Lo stadio 4 le ignora, si passa ai kart. La pagina kart tocca la visibilità solo
  per i < +0x39c (0x4c0fd0..0x4c101c), quindi le 4..7 restano nascoste. La gara ricostruisce le settings da 0x869a14 al cambio di scena
  (modo 4, limite 12), identiche all'originale [D: "0x869a14 da main+7e6104 tipoScena=1" prima di ogni gara].
- **8P → 4P → 8P nella stessa scena menu:**
  - 8g: `0x869180(8)`, rm+0x1cc = {0..7}. `MenuKartCfgHook` con tipo 2 rende visibili 4..7 [D riga 731], il primo ciclo dell'ingresso
    (+0x8c0 = 8, stato 3) le mostra, la coda è saltata;
  - indietro e 4g: `0x869180(4)`, coda H1/H2 → 4..7 nascoste;
  - di nuovo 8g: tornano visibili per la configurazione e il primo ciclo. Il KartDirector resta lo stesso (8 unità) per tutta la scena;
  - dopo una gara (4P o 8P) il ritorno al menu rifà 0x4f9e60(8) → n = 8 [D: stress8 riga 760, "N=8 da main+4f9e70 tipoScena=3"].

### 3.3 Alternative valutate e scartate
| alternativa | perché no |
|---|---|
| `0x869180(rm, N del modo)` a 0x4f9e60 | al cambio di scena il modo non è ancora scelto. Con N salvato (cfg+0x1fa4), il passaggio 8P → 4P → 8P nella stessa scena darebbe un KartDirector della dimensione sbagliata (non viene ricreato alla scelta). Servirebbe comunque H1/H2 |
| `0x869180(rm, N)` a 0x4fa254 | arriva dopo l'init del KartDirector [D]: non cambia n. Il pulsante usa già N (0x4d99bc) |
| KartDirector n = 4 in 4P | richiede di distruggerlo e ricrearlo alla scelta del modo, cosa che l'originale non fa mai: non è "stesso flusso" |
| `MenuKartCfgHook` che nasconde i ≥ locali | rattoppa l'effetto in un hook nostro invece di estendere il limite 4 del ciclo originale di 0x4b8658 |
| forzare 0x14b5a4 / lo stadio 4 | toppa sull'effetto, vietata |

---

## 4. Q4 – Rischi e misure per l'accettazione

### 4.1 Rischi
- **R1 (medio).** Il 4P non è identico byte per byte all'originale: 8 KartUnit e 8 slot preparati all'init, KartWork e heap di scena
  allargati, B/A+0x180 = 8 nel menu. È lo stesso schema dell'originale in 2P (unità e descrittori = capacità). Se si vuole n = 4
  letterale in 4P, l'unica strada è ricreare il KartDirector, fuori dal flusso originale.
- **R2 (basso).** Altri cicli con 4 fisso sulle unità visibili non trovati. I tre controlli d'uscita contano su `dir+0xc0` e saltano le
  unità nascoste. 0x14a7ac ha solo 3 chiamanti (0x4b8620, 0x4b866c, 0x4c1010) [V].
- **R3 (basso).** Senza la parola 0x4fa254, tra l'ingresso nel menu e il pulsante si ha rm+0x1cc[4..7] = −1 con le unità 4..7 vive. È
  la stessa situazione delle unità 2..3 in 2P nell'originale. Le loro ricostruzioni dell'init sono già finite [D]. Resta da escludere un
  `0x89fc4c(4..7)` per frame (lettura `[NULL+0x230]`).
- **R4 (basso).** 5-7P: le unità degli slot vuoti devono essere nascoste dal primo ciclo (stato ≠ 3). Altrimenti anche lì lo stadio 4 si
  blocca. Da provare con 5P.

### 4.2 Misure (solo log, da aggiungere a hook esistenti)
1. **Diagnosi (facoltativa, prima della modifica).** In `CharselStageHook` (0x4badb0): a ogni cambio di `+0x124` loggare lo stadio. Poi,
   ogni 60 frame mentre lo stadio vale 4, per i < dir+0xc0 loggare `obj+0xd4 +0x191 +0xd5 +0x1b8 +0x193`.
   - Atteso con la build attuale in 4P: stadio fermo a 4, unità 4..7 con `d4=1` e `191 ∉ {0,4,0xb}`.
   - Nella stessa build il 3P deve bloccarsi e 1P/2P no.
2. **Accettazione 4P (con H1/H2 e senza 0x4fa254):**
   - sonda "m3d8 sel": unit4..7 `vis=0`;
   - stadio 1 → 7 senza fermate, poi le righe "C: sel0..3" (onEnter kart 0x4c0898);
   - corsa avviata: "KartDirInit … settings+8=4 +0x180=12" come nel log ok;
   - "0x869180(N=4…) da main+4fa264" dopo "KartDirInit -> n=8";
   - nessuna riga "ricostruzione … kart 4..7 … entry -1/-9" nel menu.
3. **1P, 2P, 3P:** stessa sequenza di stadi e pagina kart. Unità N..7 `vis=0`.
4. **8P (invariato):**
   - unit4..7 `vis=1`, stadio 4 superato, "C:" o "p5: kart a turno unico";
   - preset 8pl e anteprime come in `eden_log_flow8_rebuild.txt`.
5. **Sessione:**
   - 8g → indietro → 4g → kart → indietro → 8g → kart (visibilità 4..7: 1 → 0 → 1);
   - poi gara 8P → menu → gara 4P → menu → 8P.

   Su ogni ingresso: "KartDirInit -> n=8" una sola volta per scena.
6. **5P:** unità 5..7 `vis=0` all'ingresso in selezione, avanzamento con 5 conferme.

---

## 5. Verifica statica
`python3 -I step7b_data (dati non pubblicati)/fourp_verify.py > step7b_data (dati non pubblicati)/fourp_verify.txt` → **197 OK, 0 FAIL**. Contenuto:
- 130 parole del flusso: pulsante Ng, 0x869180, 0x869040, setter delle entry, limiti, KartDirector, 0x13e59c/0x147678/0x14a7ac,
  ingresso, per-frame e uscita della selezione, 0x14b5a4/0x14ad4c/0x14ac5c, richieste di stato, scheduler;
- 57 salti;
- 5 tabelle di salto (stadi 0xf2917c, stati slot 0xf29164/0xf29150, tipo → visibilità 0xf23c28, conferma 0xf23c38);
- 5 controlli sulle parole nuove: encoder `cmp` (2), parole originali di H1/H2 (2), codifica di `71001edf`/`710022df` (1).

## Esito in Eden (4 nov 2026)
Applicato: kFlow8 = 6 parole (0x4f9e60 #8, limiti 0x86edd4/0x86f080/0x86f4ec, H1 0x4b865c `cmp #7`, H2 0x4b8674 `cmp #8`), tolta 0x4fa254.
Prova dell'utente nella stessa sessione: 4P -> kart OK, 8P -> anteprime e kart OK, 4P -> kart OK, 3P -> kart OK (2P non provato).
Log: un solo `KartDirInit -> n=8`, righe "C: sel0" a ogni ingresso nei kart, 0 guardie, 0 crash; solo i -1 di Arm (acquire fallito, §7 del piano records).
