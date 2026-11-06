# Step 7b – Cache dei modelli del menu ("ModelSmartCacheList"): limiti a 5 e a 12 con 8 KartUnit

Data: 4 nov 2026. Solo analisi statica (`img/main.ann.asm`, `img/main.bin`). Eden e GDB non sono stati usati e `src/` non è
stato modificato. Verifica: `step7b_data/cache_verify.py` -> `step7b_data/cache_verify.txt` (**189 OK, 0 FAIL**: parole a 4 byte,
bersagli dei `bl`, qword di GOT/vtable, stringhe e codifica della parola nuova proposta).
Etichette: **[V]** = verificato nel disassemblato, **[I]** = ipotesi, **[D]** = dato già misurato in Eden (fornito o nei log).

---

## 0. Sintesi
- **Le "5 cache" non sono 5 cache di modelli. Sono un anello di 5 resfile condivisi**, uno per lista, che si aggiunge a
  **12 record** con heap proprio [V].
  - Ogni lista "ModelSmartCacheList" ha **12 record da 0xc8**. Sono chiavati da 3 parole e hanno un refcount per maniglia.
    L'avvio (0x3da428, lista "Stable") li crea con un heap proprio preso da sys+0x4ed0.
  - Solo nel menu, 0x884bd8 aggiunge un **anello di 5 voci da 0x98**: heap "ModelSmartCacheList(Menu) noname:%d" con genitore
    [sys+0x320]. Liste interessate: **Body** (sys+0x77f8, voci da 0x533333) e **Wing** (sys+0x8458, voci da 0x1b3333).
  - Nella parte 0 del record (0x89d668 / 0x89dae4) il **resfile del modello sta nella voce dell'anello**. Il record ci
    costruisce sopra il modello nel proprio heap.
- **L'anello non protegge le voci in uso [V].**
  - Il confronto avviene solo sulla chiave0 (= id del body [I forte]).
  - Il refcount della voce vale in pratica 0/1: sale solo in 0x89b290 e scende solo quando la voce viene sfrattata.
  - Quando le 5 voci sono piene, la vittima è `(cursore+1) mod 5`, scelta a turno. Il refcount viene decrementato alla cieca e
    0x89b290 fa **freeAll + memset 0** dell'heap (0x8ad040) prima di caricare il nuovo body.
  - Ogni modello costruito su quel resfile resta così senza dati validi. In più, **i record già costruiti si riusano per chiave
    senza ricostruirli** (0x3dd478). Un KartUnit che torna su una combinazione già vista riceve un modello il cui resfile è stato
    riscritto, e la ricerca di "m_Body" restituisce 0xFFFF (0x68fffc). È **esattamente il guasto osservato** [I forte: il
    percorso è V, l'innesco va misurato].
  - Dimensionamento: i caricamenti sono serializzati (uno slot per volta), quindi 5 = 4 body visibili + 1 in caricamento,
    cioè "4+1". Con 8 unità serve **9 = 8+1**.
- **Condizione d'innesco [I]:** l'anello lavora solo se gli 8 giocatori mostrano **più di 5 body diversi** (chiave0 diversa).
  Se tutti hanno lo stesso kart, l'anello non sfratta mai e la causa è un'altra. La prima misura (§4.3) è proprio questa.
- **Secondo limite, solo a 8P [V struttura, I effetto]: 12 record per lista contro 16 buffer.** Le liste sono 7 (Driver,
  Emblem, Tire, sys+0x71f0, Body, Wing, Arm) e ognuna ha 12 record. Ogni buffer di caricamento L tiene la sua maniglia fino al
  ricaricamento successivo. Il conto:
  - in 4P: 4 slot × 2 buffer = 8 ≤ 12;
  - in 8P: 8 × 2 = **16 > 12**.

  Con più di 12 chiavi distinte bloccate (refcount > 0), la ricerca restituisce **-1** e la maniglia resta senza risorsa
  (NULL). Non dà resfile corrotti, ma probabilmente porta a "kart senza pilota" o a un modello di ripiego (le monete?) [I].
- **Memoria:** portare i due anelli a 9 voci costa **+0x1b99998 (27,6 MiB)**. In [sys+0x320] (CourseResource) restano circa
  2,8 MiB [D], quindi serve allargare CourseResource (0x3d57f0: 0xe404000 → 0x10004000) o prendere un altro genitore, dopo
  averlo misurato (§3).

---

## 1. Q1 – Struttura delle liste e limiti

### 1.1 Oggetto sys [V]
- `new(0x9a50)` a 0x3de9f4. Il ctor derivato è 0x3da070 (vt 0x1264980, GOT 0x1307280; vt base 0x12d1d90, ctor base 0x880c8c).
  Il puntatore viene salvato in mgr+0x808: `0x8ace44()` = `[0x8aba80()+0x808]`.
- All'avvio 0x881ce8 crea l'heap **BackgroundLoad** da **0x13b00000** in sys+0x4ed0 (0x881d10/0x881d3c), poi chiama vt+0x90 =
  **0x3da428**, che crea tutte le liste "Stable": 12 record per lista, ciascuno con heap proprio preso da sys+0x4ed0.

| lista | offset sys | record | heap per record | creazione | ricerca |
|---|---|---|---|---|---|
| Driver | 0x5060 | 12 × 0xc0 | 0x500000 | 0x3dac38 | 0x88e6f4 |
| Emblem | 0x59f8 | 12 × 0xb0 | 0x2147a | 0x3daf80 | 0x88f0c0 |
| Tire | 0x62c0 | 12 × 0xb0 | 0x100000 | 0x3db2c0 | 0x88fecc |
| ? | 0x71f0 | – | – | ctor 0x881524 | 0x88fb0c |
| **Body** | **0x77f8** | 12 × 0xc8 | 0x533333 | 0x3db600 | **0x88f5b4** |
| **Wing** | **0x8458** | 12 × 0xc8 | 0x1b3333 | 0x3db958 | 0x890374 |
| Arm | 0x90b8 | 12 × 0xc0 | 0x119999 | 0x3dbcb0 | 0x890944 |

Totale Stable ≈ 0xa98f5ac (169,6 MiB) dentro BackgroundLoad. Arm termina a 0x90b8+0x998 = **0x9a50 = fine di sys**.

### 1.2 Lista Body/Wing (0xc60 byte) [V]
| offset | contenuto |
|---|---|
| +0x000..+0x960 | **12 record × 0xc8**. Ctor a 0x8815f0..0x881640, passo 0xc8, fine 0x8158 = 0x77f8+0x960 |
| +0x960..+0xc58 | **5 voci × 0x98** (anello), ctor 0x88629c / 0x886460 |
| +0xc58 | cursore dei record (giro modulo 12, 0x88f6ec/0x88f808); il ctor lo azzera (0x881654: sys+0x8450) |
| +0xc5c | cursore dell'anello (modulo 5); 0x884f2c lo azzera (0x884f60) |
| +0xc60 | inizio della lista successiva (Body+0xc60 = 0x8458 = Wing, Wing+0xc60 = 0x90b8 = Arm) |

**Record (0xc8):**
- +0 vt: Body 0x12d22e8 (GOT 0x1310468), Wing 0x12d2348;
- +0x8/+0xc/+0x10 chiave. Per il Body [I forte, dalla stessa chiave nel precarico di gara 0x3dc3c8] è (desc+0 = body,
  desc+0xc = personaggio, byte desc+0x10 = variante);
- +0x18 heap proprio (Stable);
- +0x20/+0x28/+0x30 risorse delle parti 0..2;
- +0x38 refcount (numero di maniglie);
- +0x3c caricato, +0x3d usato;
- +0x40 FixedSafeString<64> (nome);
- **+0x98..+0xb8 = 5 puntatori alle voci dell'anello**, **+0xc0 = &cursore dell'anello**. Li scrive 0x884f2c (ciclo 0x885260,
  `cmp x8,#0x960`, 12 record); 0x3db600 li azzera all'avvio (0x3db674/0x3db67c).

**Voce (0x98, classe base del record):** vt 0x12d2330 (GOT 0x1310528, vt[0] = 0x89b3b8), chiave +0x8..+0x10 (ctor -1), +0x18 heap,
+0x20.. resfile, +0x38 refcount, +0x3c caricato, +0x40 nome.

**Si può estendere in loco a 9? No [V].**
- Subito dopo le 5 voci ci sono i cursori e poi la lista successiva.
- Nel record, dopo +0xc0 inizia il record seguente.
- Il codice è srotolato per 5: 0x89d668 legge rec+0x98, +0xa0, +0xa8, +0xb0 e +0xb8 a uno a uno, e il modulo è `cmp #3 / csinc` +
  `cmp #5` (0x89d97c..0x89d988, Wing 0x89ddf8..0x89de04).
- Le 4 voci in più vanno quindi **fuori dall'oggetto** (memoria del mod), e la parte 0 dei record va reimplementata con N = 9.
  Rilocare sys non serve.

### 1.3 Tutti i cicli e i limiti [V]
| sito | limite | ruolo |
|---|---|---|
| 0x884f2c: 0x8850e4 / **0x885250** | `cmp #5` | crea le 5 voci (ramo con nome / ramo noname) |
| 0x8852b8: 0x885470 / 0x8855dc | `cmp #5` | uguale per Wing |
| 0x885284 / 0x885610 | `cmp x8,#0x960` | 12 record: rec+0x98..+0xc0 = voci 0..4 + &cursore |
| 0x89d6a0..0x89d7d0 (Wing 0x89db1c..) | 5 letture srotolate | cerca la voce con chiave0 uguale e refcount ≥ 1 |
| 0x89d814..0x89d968 | 5 rami srotolati | prima voce con +0x3c = 0 → 0x89b290 |
| 0x89d970..0x89da18 (Wing 0x89ddec..) | modulo 5 | vittima = (c+1) mod 5, refcount−−, 0x89b290 |
| 0x88f5b4 (e 0x890374, 0x890944, 0x88e6f4..) | `cmp #0xc` + 12 controlli srotolati | ricerca, poi record libero, poi giro su refcount ≤ 0 |
| 0x88f4d4, 0x890290, 0x890858, 0x88f914 | `cmp #0xc` + csel | indice del record nella maniglia (≥ 12 → 0) |
| 0x3db600 / 0x3db958 / 0x3dbcb0 | `cmp #0xc` | creazione degli heap dei record (avvio) |
| 0x883790 / 0x8838f4 | 12 srotolati | azzeramento dei record nel ctor |
| 0x3dbff0 / 0x3dc340 | `cmp #0xc` | precarico e segni di gara |

Nessun ciclo a 4 nelle liste. La distruzione delle voci non esiste come funzione separata: 0x884f2c le **ricrea a ogni init del menu**
sovrascrivendo voce+0x18. Il rilascio passa per la distruzione del genitore [I].

---

## 2. Q2 – Assegnazione, riuso e percorso fino al modello senza m_Body

### 2.1 Dalla maniglia al record [V]
- Il buffer di caricamento L (ctor 0x890ddc, uno per slot e per buffer: sys+0x160/+0x168) contiene una maniglia per lista. La
  maniglia Body sta in L+0x3e0, vt 0x12d2a20: +0x28 = lookup 0x88f44c, +0x30 = rilascio 0x88f898.
- 0x892338(L, desc) → 0x895a44: **prima rilascia** (vt+0x30, che decrementa il refcount del record e azzera +0xa8), **poi cerca**
  (vt+0x28).
- 0x88f5b4(lista, chiave) segue quest'ordine:
  1. record con +0x18 ≠ 0, caricato e chiave uguale;
  2. altrimenti il primo con +0x3c = 0;
  3. altrimenti un giro modulo 12 dal cursore +0xc58, che sceglie solo record con **refcount ≤ 0** (0x88f744).

  Se non trova nulla restituisce **-1** (0x88f840). La maniglia salva l'indice in +0x98 e prende `rec+0x20+parte*8`
  (0x88f4f4); con -1 resta NULL.
- 0x3dd478(record, chiave): **se refcount ≠ 0 o se la chiave è uguale e il record è caricato, non ricostruisce** e fa solo
  refcount++. Altrimenti esegue 0x8ad040 sull'heap del record e chiama vt+0x10 per le parti 0..2.

### 2.2 Parte 0 di Body/Wing: anello condiviso (0x89d668 / 0x89dae4, solo se rec+0x98 ≠ 0, cioè nel menu) [V]
1. **Ricerca:** prende la voce con `ref ≥ 1 && voce.chiave0 == rec.chiave0` (priorità e4 > e3 > … > e0). La usa senza
   incrementare nulla: `vt+0x18(rec, x2+0x88, voce+0x20)`, cioè costruisce il modello del record dal **resfile della voce**.
2. **Voce libera** (+0x3c = 0): `0x89b290(voce, chiave, path, 1)`: carica e fa ref++.
3. **Sfratto:** `c = cursore; c = (c > 3) ? 0 : c+1` (fino a 5 tentativi per una voce non nulla). Se `ref ≥ 1` allora `ref−−`
   (0x89da20), poi `0x89b290`, poi `cursore = c`. In 0x89b290:
   - se ref (dopo il decremento) è ≠ 0, non ricarica e restituisce il resfile **dell'altro body**;
   - se è 0, fa **0x8ad040 (freeAll + memset 0 di tutto l'heap della voce)** e poi carica il nuovo body.
4. Se tutto fallisce: vt[0] carica nell'heap del record (ripiego).

Il refcount della voce sale solo in 0x89b290 e scende solo allo sfratto. Il riuso del passo 1 non lo tocca e nessun rilascio di
record lo decrementa (vt del record: +0x8 = 0x88645c = `ret`). Vale quindi 0 o 1 e **non conta gli utenti** [V sui percorsi
trovati]. Lo sfratto non guarda chi usa la voce.

### 2.3 Percorso del guasto [percorso V, innesco I]
Situazione: P1..P8 hanno almeno 6 body diversi (chiave0), e uno solo viene caricato alla volta.
1. Il giocatore a mostra la combinazione Ka = (body Ba, personaggio, variante). Il record Ra ha la parte 0 costruita sul resfile
   della voce ej (Ba).
2. Altri giocatori cambiano e caricano body nuovi. Dopo 5 sfratti a giro tocca a ej: `ref 1→0`, poi 0x8ad040 (memset), poi Bnew.
   Il resfile sotto il modello di Ra (in uso o solo in cache con refcount 0 ma "caricato") ora è un altro body o è azzerato.
3. Il giocatore a torna su Ka, o un altro giocatore sceglie Ka. 0x88f5b4 trova Ra per chiave e 0x3dd478 **non ricostruisce**.
   La maniglia riceve la parte di Ra: la callback 0x16f08c → 0x16eddc → 0x16e0cc → 0x1724c0 → **0x122cd4 cerca "m_Body"** in
   un modello il cui dizionario dei materiali punta a dati riscritti → **0x68ff80 = 0xFFFF → 0x68fffc salta a 0**. Il
   thread 0x618610 muore e sys+0x5050 resta a 5 [D]. Può capitare a **qualsiasi** unità, anche la 1 o la 2 [D].
   - Con le guardie successive: letture "float come puntatore" e crash più avanti (0x16e13c → 0xd6318 → 0x690ea8 → 0x6dd30c) [D],
     coerenti con un resfile che appartiene a un altro body.
4. **Perché 4P regge e 8P no:**
   - in 4P i body visibili sono al più 4 e i caricamenti sono serializzati: 4 + 1 = 5 voci, quindi in selezione personaggi
     l'anello non sfratta un body in uso (la quinta voce serve a chi cambia);
   - in 8P servono 8 + 1. Con 5 voci, ogni giro di 8 caricamenti sfratta body ancora visibili o in cache.

   Se i body sono tutti uguali, l'anello usa una voce sola e **non sfratta mai**: va misurato (§4.3).

### 2.4 Altra struttura con lo stesso effetto "8 > 4": 12 record per lista [V struttura, I effetto]
- I buffer con maniglie bloccate nel menu sono slot × 2:
  - in 4P sono 8;
  - in 8P sono 16, tutti con doppio buffer a cur = 1 e entrambi d38 ≠ 0 [D, log 7b13].
- Nel menu 0x884bd8 chiama 0x892018 sui 12 default sys+0xf8.. (0x884c10..0x884c6c) [V]: probabilmente li rilascia [I], quindi non bloccano record.
- Quando L[1-cur] dello slot k ricarica, prima rilascia la propria maniglia e poi cerca. Le altre 15 restano bloccate: se le chiavi
  distinte bloccate sono ≥ 12, 0x88f5b4 restituisce -1 e la parte risulta NULL.
- Vale per **tutte e 7 le liste**: con 8 personaggi diversi Driver si esaurisce prima di Body.
- Effetto atteso: niente corruzione, ma kart senza pilota o senza body, poi un probabile ripiego (monete viste in P6-P8) [I]. Se
  il kart prende un modello di ripiego senza materiale m_Body, si arriva **allo stesso crash di 0x122cd4**. Anche questo va
  distinto con i log (§4.3).
- Rimedio nel flusso originale: record da 12 a **16** per ogni lista. Sono array incorporati seguiti dalla lista successiva e
  l'indice è limitato a 12 in circa 15 siti. Il lavoro è molto più grande e conviene affrontarlo solo se la misura lo richiede.

---

## 3. Q3 – Memoria
- **Genitore delle voci del menu = [sys+0x320]** (0x884d00) [V]. Nessuno scrive in sys+0x320 con uno `str` diretto (ricerca su
  tutto il main), quindi il valore viene da un percorso indiretto [I]. Che sia l'heap **"CourseResource"** lo dice la misura già
  fatta [D].
  - CourseResource nasce in **0x3d5794** (`obj+0x10`, 0x3d5814/0x3d5818, dimensione **0xe404000** da 0x3d57f0/0x3d57f8), dopo
    "DriverBodySE" 0x279e000 (0x3d57e4) e con lo stesso genitore x1. 0x3d5794 è un metodo virtuale (vt 0x1264050, d50758) senza
    `bl` diretti.
- **Costo dell'anello:**
  - originale: 5×0x533333 + 5×0x1b3333 = **0x227fffe (34,5 MiB)**;
  - a 9 voci: 0x3e19996 (62,1 MiB);
  - **differenza: +0x1b99998 (27,6 MiB)** più le intestazioni degli heap (pochi KiB).
- **Dove prenderla** (da misurare prima con `MaxAlloc`, cioè vt+0x80(h, 8) come in `m3d8::MaxAlloc`):
  1. **Allargare CourseResource**: 0x3d57f0 `52a1c800` → **`52a20000`** (movz w0,#0x1000,lsl#16; il movk 0x3d57f8 `72880000` resta)
     = **0x10004000** (+0x1c00000 = 28 MiB). Serve un margine di 28 MiB nel genitore di 0x3d5794, oggi ignoto. Effetto anche in
     gara: CourseResource più grande è innocuo se il genitore lo regge [I].
  2. Creare le 4 voci extra con genitore **sys+0x4ed0 (BackgroundLoad, 0x13b00000)**. Contiene circa 169,6 MiB di record Stable
     più l'oggetto sys+0x4ca8 (0x703333); il libero è da misurare. Gli heap restano vivi per tutta la partita, come i record
     Stable: niente problemi di durata, ma 27,6 MiB restano occupati anche in gara.
  3. Heap di scena del menu (già portato a 0x10000000): durata giusta, libero da misurare.
- **Altri utenti di CourseResource nel menu:** nel menu lo usano le 10 voci più quello che c'era prima (circa 190 MiB già occupati
  quando restano 2,8 MiB) [D/I]. Non ho trovato l'elenco statico degli altri utenti; serve un log del libero prima e dopo 0x884bd8.
  In gara lo usano il percorso e forse le stesse voci: i puntatori rec+0x98 non vengono azzerati fuori dal menu [V: nessun altro
  scrittore], quindi in gara 0x89d668 continua a usare l'anello del menu. Le voci extra devono avere la **stessa durata** delle 5
  originali (stesso genitore, create nello stesso punto).

---

## 4. Q4 – Piano minimo "flusso originale" (anello da 4+1 a 8+1)

### 4.1 Patch/hook (parole verificate in `cache_verify.txt`)
1. **`CacheRingCreate`**, trampolino su **0x884f2c** (`d10383ff`) e **0x8852b8** (`d10383ff`):
   - chiama `Orig(lista, size, parent, 0, 0)`;
   - se l'opzione 8P è attiva e `lista ∈ {sys+0x77f8, sys+0x8458}`, prepara `ext[l][0..3]`, cioè 4 voci da 0x98 nella memoria del
     mod, come fa 0x88629c:
     - vt = `[GOT 0x1310528]+0x10`, chiave = -1, +0x18..+0x3d = 0;
     - heap = `0x605128(size, &nome, parent, 8, 1, 0)` con nome "ModelSmartCacheList(Menu) noname:5..8", poi `0x8ad040(heap)`;
     - il genitore è lo stesso `parent` (§3, opzione 1) oppure il genitore alternativo scelto dopo la misura.
   - Se 0x605128 restituisce NULL, la voce resta con heap = 0 e la funzione di sostituzione la salta (log).
2. **`CacheRingPart0`**, trampolino su **0x89d668** (`d101c3ff`) e **0x89dae4** (`d101c3ff`), solo per la parte 0 nel menu.
   Si usano solo funzioni del gioco (0x89b290, vt+0x18, vt[0]); l'algoritmo è lo stesso, con N = 9:
   ```cpp
   // E[0..4] = rec+0x98..+0xb8 (originali), E[5..8] = ext[l][0..3]; cur = *(int*)rec[+0xc0]
   if (part != 0 || !*(void**)(rec+0x98) || !mod8) return Orig(rec, part, x2);
   void* path = x2 + 0x88; Entry* hit = nullptr;
   for (int i = 0; i < 9; i++) if (E[i] && E[i]->ref >= 1 && E[i]->key0 == rec->key0) hit = E[i];   // ultima vince
   if (hit) return rec->vt18(rec, path, hit->res);
   for (int i = 0; i < 9; i++) if (E[i] && E[i]->heap && !E[i]->loaded && Fn89b290(E[i], &rec->key, path, 1)) return rec->vt18(rec, path, E[i]->res);
   int c = *cur; Entry* v = nullptr;
   for (int t = 0; t < 9 && !v; t++) { c = (c > 7) ? 0 : c + 1; v = E[c]; }
   if (v) { if (v->ref >= 1) v->ref--; void* r = Fn89b290(v, &rec->key, path, 1); *cur = c; if (r && v->res) return rec->vt18(rec, path, v->res); }
   return rec->vt0(rec, path, 1);
   ```
   Nota: il confronto e il cursore restano quelli del gioco (chiave0, giro). Si estende solo il limite 5 → 9, come chiesto. Il
   cursore resta in lista+0xc5c (rec+0xc0).
3. **Memoria**: 0x3d57f0 `52a1c800` → `52a20000` (CourseResource 0x10004000), **solo se** il log mostra che il genitore di
   0x3d5794 ha almeno 28 MiB liberi. Altrimenti si usa il genitore alternativo nel punto 1.
4. Per ora niente su 12 → 16 record: si fa solo se il log del §4.3 mostra ricerche fallite.

### 4.2 Rischi
- **Durata e gara:**
  - se 0x884f2c viene richiamato (nuovo ingresso nel menu), le voci extra vanno ricreate come le originali. Con lo stesso
    genitore distrutto in blocco, i puntatori vecchi restano pendenti fino alla ricreazione;
  - in gara, rec+0x98 ≠ 0 resta [V], quindi la sostituzione lavora anche lì su 9 voci. È innocuo se gli heap sono vivi, ma con
    il genitore CourseResource va controllato all'ingresso in gara. Prudenza: applicare la sostituzione solo con
    KartDirector+0xe9 = 1 e usare Orig altrimenti. **Attenzione:** Orig con cursore > 4 resta valido, perché il gioco fa
    `c > 3 ? 0 : c+1`.
- **Thread:** 0x89d668 gira nel thread di caricamento (0x618610). Il mod non deve fare log pesanti né allocare lì.
- **Wing:** stessa logica, ma i suoi heap sono da 0x1b3333; usare la `size` passata.
- **Se la causa è il §2.4** (record), l'anello a 9 non basta: serve anche 12 → 16 record, oppure meno chiavi distinte bloccate.
- **Memoria in gara** con il genitore CourseResource allargato: da verificare sulla prima gara a 8.

### 4.3 Cosa misurare (solo log, niente GDB)
1. **Innesco dell'anello.** Log del trampolino 0x89d668/0x89dae4 (parte 0), stampato dopo Orig:
   - indice del record `(rec - lista)/0xc8` e chiave a 3 parole;
   - ramo preso: riuso / voce libera / **sfratto** / ripiego;
   - le 5 voci (chiave0, ref, caricata) e il cursore.

   Contatore di "epoca" per voce nel mod: +1 quando la chiave0 cambia.
2. **Uso di resfile scaduti.** Trampolino solo di log su 0x3dd478 (`f81c0ff7`). Quando restituisce senza ricostruire
   (refcount ≠ 0, oppure chiave uguale e caricato), confronta l'epoca della voce usata da quel record (salvata al punto 1) con
   quella attuale. **"STALE" prima del crash = causa confermata.**
3. **Esaurimento dei record.** Trampolino su 0x88f5b4, 0x890374, 0x890944 e 0x88e6f4 (`d10183ff`/`d10103ff`): log quando
   restituiscono -1, con la lista e i 12 refcount.
4. **Chiavi:** body (desc+0) e personaggio (desc+0xc) di ogni slot in 0x88259c (già agganciato). Se i body distinti sono ≤ 5,
   l'anello è escluso.
5. **Memoria:** `MaxAlloc([sys+0x320])` prima e dopo 0x884bd8. `MaxAlloc(x1)` all'ingresso di 0x3d5794. `MaxAlloc([sys+0x4ed0])`
   nel menu.
6. **Dopo la correzione:** stesso stress con 8 cursori. Ci si aspetta:
   - 0 eventi STALE, 0 ritorni -1, 0 MatFind 0xFFFF;
   - sys+0x5050 mai fermo a 5 per più di 120 frame;
   - cursore dell'anello che gira su 0..8.

---

## 5. Riassunto
1. Le "5 cache" sono un **anello di 5 resfile condivisi** (heap "Menu noname:%d" in [sys+0x320]) solo per **Body** (sys+0x77f8, voci
   0x533333) e **Wing** (sys+0x8458, 0x1b3333). Lo crea 0x884f2c/0x8852b8 (`cmp #5` a 0x885250/0x8855dc), a 0x960+i*0x98
   nella lista. I puntatori stanno nei **12 record** (rec+0x98..+0xb8, +0xc0 = &cursore a lista+0xc5c) [V].
2. Ogni lista ha anche **12 record da 0xc8** con heap proprio (Stable, avvio 0x3da428 da BackgroundLoad 0x13b00000), una chiave a
   3 parole e un refcount per maniglia. La ricerca 0x88f5b4 (12, giro sui record con refcount ≤ 0) restituisce -1 se tutti sono
   bloccati [V].
3. La parte 0 del record (0x89d668/0x89dae4) costruisce il modello sul **resfile della voce dell'anello**: chiave0 = body
   [I forte]. Lo sfratto è a giro (modulo 5, 0x89d97c..), con refcount decrementato alla cieca e **freeAll + memset** dell'heap
   (0x89b290 → 0x8ad040), senza controllo di chi usa la voce [V].
4. I record già costruiti si riusano senza ricostruirli (0x3dd478). Se la loro voce è stata sfrattata, il modello punta a un
   resfile riscritto e **0x122cd4 non trova m_Body** (0x68fffc, indice 0xFFFF): è esattamente il guasto osservato, su
   qualsiasi unità.
5. Dimensionamento: i caricamenti sono serializzati, quindi 5 = 4 visibili + 1 in caricamento. **A 8 servono 9.**
6. Innesco da misurare: l'anello sfratta solo con **più di 5 body distinti** fra gli 8 giocatori.
7. Secondo limite solo a 8P: 16 buffer con maniglie (8 slot × 2) contro **12 record** in **tutte** le 7 liste. Dà NULL, non
   corruzione, ma con un ripiego può portare allo stesso crash. Da distinguere con i log; si estende solo se serve (lavoro
   grande).
8. Non si può estendere in loco: la lista Wing segue subito la Body (+0xc60) e il codice è srotolato per 5. Servono 4 voci extra
   nel mod e la parte 0 reimplementata con N = 9 (trampolini su 0x884f2c, 0x8852b8, 0x89d668 e 0x89dae4, tutti `d10383ff`/`d101c3ff`).
9. Memoria: **+27,6 MiB** (0x1b99998). CourseResource ha circa 2,8 MiB liberi, quindi va allargato (0x3d57f0 `52a1c800` →
   `52a20000`, 0x10004000) se il genitore regge; in alternativa si usa BackgroundLoad o l'heap di scena, dopo una misura
   `MaxAlloc`.
10. Prossima sessione: build di solo log (§4.3, punti 1-5). Se compaiono eventi STALE si applica §4.1; se compaiono -1 si
    pianifica 12 → 16 record.
