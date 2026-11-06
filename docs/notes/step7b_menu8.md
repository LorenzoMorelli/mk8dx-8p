# Step 7b – Menu a 8 "nativo": conteggio dei descrittori, crash Shutter/LocalRoom, piano N = 8

Data: 4 nov 2026. Solo analisi statica (`img/main.ann.asm`, `img/main.bin`) più i log Eden già presenti in `step7b_data/`.
Eden e GDB non sono stati usati e `src/` non è stato modificato.
Verifica: `step7b_data/menu8_verify.py` -> `step7b_data/menu8_verify.txt` (**128 OK, 0 FAIL**). Lo script controlla le parole a 4 byte,
ricalcola i bersagli di `bl`/`b`, verifica i qword della vtable e le stringhe, e codifica le parole nuove proposte.
Etichette: **[V]** = verificato (disassemblato o log esistente), **[I]** = ipotesi.

---

## 0. Sintesi
- **Q1.** In modo 5, `0x86f074` conta i descrittori validi consecutivi fino a un massimo di 4 (altrimenti 12). Nel menu però a fermare
  il conteggio non è quel 4: è il **tipo 4 ("fine lista") nei descrittori 4..7**. Questi descrittori nascono da `0x869a14`, che percorre
  le 16 entry di raceMgr e assegna k = 0,1,..
  (`rm+0x1cc[k]`, `entry+0x22c`, `settingsB.desc[k]`). Il risultato viene poi copiato pari pari in settings A (`0x7e6e54`, conteggio
  compreso). Per avere 8 descrittori "nativi" servono quindi due cose: (1) il limite del modo 5 portato a 8 in **tre** copie inline
  (0x86edd4, 0x86f080, 0x86f4ec); (2) **entry 4..7 valide** nel momento in cui `0x869a14` costruisce le settings del menu.
- **Q2.** La resfile NULL **non** viene dal caricatore dei personaggi. L'attore che crasha è lo sfondo della stanza locale
  (`UI3D/LocalRoom/LocalRoom.szs`, animazione `Shutter`/`LocalRoom_ftp`), voce +0xc18 del viewer. La carica il gestore di risorse
  generico (`0x8aba80()+0x220`) nell'heap **HeapResident**, un sotto-heap di **100 MiB (0x6400000)** che il proprietario del viewer
  (0x425c70) ritaglia dall'**heap corrente**, lo stesso heap da cui `0x891984` prende i caricatori, **dopo** l'init del
  KartDirector. I log esistenti mostrano la soglia:
  - con 8 slot preparati all'init restano **0x5f40dc8 = 95,25 MiB** (eden_log_7b1_crash_viewer) e si crasha;
  - con 4 slot restano **0x696fb88 = 105,4 MiB** (7b15/7b16/7b17) e non si crasha.

  Quindi HeapResident non entra (0x605130 restituisce NULL a 0x6051ec). Ipotesi vincente: **(c) bilancio di memoria** dell'heap di
  scena. Sono escluse (a) (nessun accesso a sys/slot) e (b) (archivio id 0, nessuna tabella a capienza fissa sul percorso).
- **Q3.** Flusso nativo N = 8:
  - cap 4 -> 8 (3 parole);
  - entry 4..7 valide prima di `0x869a14` del menu (1 trampolino);
  - **bilancio di memoria**: HeapResident ridotto, se la misura mostra margine, oppure heap di scena più grande.

  Con questo si possono togliere KartResHook, ResReqHook, ResWaitHook, PrepareLoaders/kickSched, la limitazione dello scheduler,
  i tre Force8 e DirGroupInitHook, oltre alla copia dei descrittori in KartDirInitHook.
- **Q4.** Un avvio di solo log con 4 trampolini: 0x605130, 0x425c70, 0x8805f4, 0x891984. Più una prova decisiva che non crasha:
  un blocco fittizio di 10,2 MiB, liberato e seguito da un nuovo tentativo se HeapResident fallisce.

---

## 1. Q1 – `0x86f074`, chi compila i descrittori, consumatori

### 1.1 Cosa conta `0x86f074(settings)` [V]
Lo pseudocodice, con desc i = `settings+0x30+i*0x1c` e i campi `+0..+0xc` (personaggio/kart/ruote/aliante), `+0x10` h, `+0x14` tipo, `+0x18`:
```
lim = (settings+8 == 5) ? 4 : 12          // 0x86f078 cmp w10,#5 / 0x86f080 orr w11,#4 / 0x86f098 csel
+0x180 = +0x184 = +0x188[0..4] = 0         // 0x86f08c..0x86f094
for i < lim:                              // 0x86f138 cmp x9,x10
   if desc[i].tipo == 4: return           // 0x86f0ac/0x86f0b0/0x86f0b4: fine lista
   if uno dei 4 campi == -1: +0x180 = 0; return   // 0x86f0c0.. / 0x86f14c (conteggio azzerato!)
   +0x180++ ; if tipo in {0,2}: +0x184++ ; +0x188[tipo<5?tipo:0]++
```
- Lo stesso ciclo compare **inline in altre due funzioni**:
  - 0x86edb4 (0x86edcc/0x86edd4/0x86edec, chiamata da 0x7e6e18);
  - 0x86f474 (0x86f4e8/0x86f4ec/0x86f4f4, la funzione che scrive desc[k]).

  Le tre parole del limite sono `321e03eb` (`orr w11,wzr,#4`) a **0x86edd4, 0x86f080, 0x86f4ec** [V].
- **Chiamanti di 0x86f074** [V, grep]:
  - 0x869a14 (12 chiamate srotolate di reset);
  - 0x3bc010 (gara: tipo 3 -> 4);
  - i setter per entry 0x42022c/0x4202f4/0x420ac0/0x420b88/0x420c50 (tail call), usati dalla selezione;
  - 0x8821c0/0x8823e0 (sys).
- **Effetto di portare a 8 il limite del modo 5:**
  - Da solo **non cambia nulla nel menu**, perché i descrittori 4..7 sono di tipo 4 e il ciclo si ferma lì. Il vanilla a 4 resta
    identico, a patto che le entry 4..7 non siano valide quando il menu costruisce le sue settings (log 7b17_4p: `init desc4..7 =
    -1 -1 -1 -1 0 4 2`).
  - Conta solo se i descrittori 4..7 sono validi: allora +0x180 = 8 in settings B e, dopo la copia, in A.
- **Attenzione:** a 0x86f14c basta un campo -1 in un descrittore con tipo ≠ 4 per azzerare **tutto** il conteggio. Per questo le entry
  4..7 vanno rese valide con parti ≥ 0, non solo con tipo ≠ 4.

### 1.2 Chi compila i descrittori [V salvo indicazione]
1. **Settings B** (`0x86f7d4` = `[0x7e741c()+0x10]`). Le scrive **`0x869a14(rm)`**, l'unico chiamante di 0x86f474 (unico xref
   0x869c14):
   - azzera entry+0x22c, `rm+0x1cc[0..11] = -1` e i 12 tipi = 4 (0x869a34..0x869bbc);
   - poi, per le 16 entry (0x869cd8 `cmp x24,#0x10`), accetta quelle con `+0x44 ≠ 2`, parti `+0x210..+0x21c ≠ -1`,
     `+0x224 (tipo) ≠ 4` e `+0x50 == 0` (0x869c68..0x869cb4);
   - per ogni entry accettata scrive `entry+0x22c = k`, `rm+0x1cc[k] = indice` (clamp 12, 0x869be0) e `desc[k] = entry+0x210`
     (0x86f474, che rifà il conteggio con il limite 4/12).

   **Questo ciclo non ha un limite a 4**: con 8 entry valide produce 8 descrittori e `rm+0x1cc[0..7]` coerenti. Il 4 è solo in
   0x86f4ec.
   - Chiamata dalla sequenza `0x8ac6a4` (`b 0x869a14` a 0x8ac704, x0 = this+0x800 = raceMgr), che a sua volta è chiamata da
     0x7e6100 [V].
   - Che sia **questa** la chiamata che prepara le settings del menu è [I forte], da confermare (§4).
2. **Valori "0 0 0 0 0 tipo 0 2".** Sono le parti a zero delle entry 0..3:
   - `0x490828` le azzera **srotolato su 4** (0x490850..0x490958: 0x4202f4/0x420ac0/0x420b88/0x420c50 con entry 0..3);
   - `+0x18 = 2` è il default del ctor (0x86ecc8) oppure viene da 0x86f4c0 (`settings+0x27 == 0 -> 2`).

   Le entry 4..7 a quel punto hanno tipo 4 (scartate). Il tipo delle entry si decide nella pagina della selezione (0x4bb000..:
   16 entry a tipo 4, poi tipo 0 per i pad collegati, con clamp `cmp x21,#4` a 0x4bb108, già toppato dalla mod a 8).
3. **Settings A** (`0x86f7bc` = `[0x7e7404()+0x10]`, quella del KartDirector). È una **copia integrale** fatta da `0x7e6e54(dst, src)`:
   12 descrittori (0x7e6ee4 `cmp x10,#0x150`) e `+0x180..+0x19c` copiati, **non ricalcolati** (0x7e6f38/0x7e6f3c). Viene chiamata
   da 0x3d6c38 (`+0x708 <- +0x710`) [V]. Che sia "A <- B" al cambio di scena è [I forte]: dei 1413 chiamanti di 0x86f7bc nessuno
   scrive i descrittori.
   - **Conseguenza:** il ciclo da portare "a 8" non è un ciclo `i < 4` del menu. Sono (a) il cap 0x86f4ec e le sue due copie, e
     (b) la validità delle entry 4..7 quando gira 0x869a14 per il menu.

### 1.3 Consumatori di settings A +0x180 / descrittori nel menu [V]
Ci sono **119 letture di `A+0x180` in 116 funzioni**. Le ha trovate una scansione `bl 0x86f7bc` -> `ldr [x0,#0x180]` entro 25 istruzioni. Non è nello script di verifica, si rifà con un
grep. Quelle rilevanti per il menu:

| consumatore | sito | uso | con A+0x180 = 8 |
|---|---|---|---|
| KartDirector init 0x13d778 | 0x13d7b8/0x13d7bc | `dir+0xc0` = n KartUnit, array `+0xc8` | 8 KartUnit senza hook (resta l'heap KartWork 0x13da24/0x13da28) |
| 0x884bd8(sys,e9,A,n) | 0x13d98c, 0x884c90 `cmp x22,#0xc` | prepara i caricatori degli slot i < n con tipo 0 | prepara gli slot 0..7 (+10,2 MiB, vedi Q2) |
| 0x8843a8(sys,i) | ciclo 0x13dab4.. | attende il bit dello slot i | slot 4..7 richiesti, attesa soddisfatta |
| gestore dg+0x218 (0x86d918) | 0x86d938 | PtrArray<n> (+0x60/+0x68, letto a 0x182cf0) | 8: niente `KartMgrInitHook` |
| gestore dg+0x240 (0x2a9c) | 0x2b08 | array +0xe0/+0xe8 (letto a 0x150f04) | 8: niente `KartMgr240InitHook` |
| gestore dg+0x228 (0xcefb4) | 0xcefd0 | array | 8: niente `KartMgr228InitHook` |
| scheduler 0x3deee8 | 0x13d540 (dir+0xc0), 0x3df0f8 `rm+0x58[rm+0x1cc[i]]` | un caricamento per volta | con `rm+0x1cc[4..7]` validi non serve più la limitazione |
| 0x8821c0 (richiesta) | copia `sys+0x4d1c` <- **B**.desc[0..11] | descrittore richiesto | invariato (B) |
| config oggetti 0x13e59c | 0x13e634..0x13e6f4 srotolato su 4 | preset/tipo degli oggetti menu-kart | **resta** il trampolino `MenuKartCfgHook` |
| viewer piedistalli | 4 voci a +0x568, accessori senza controllo | — | restano le guardie (non dipendono da +0x180) |

Altri lettori nelle zone del menu da controllare quando A+0x180 diventa 8:
- 0x39d8b8/0x39d92c/0x39d988/0x39e90c/0x39e980/0x39f868;
- 0x3a60a4/0x3a6620/0x3a70e8/0x3a72a8/0x3a7490/0x3a7838;
- 0x3b1ff8/0x3b4024/0x3b40b0;
- 0x7d8b58/0x7d8cb8/0x7d905c, 0x7f3c3c/0x7f3dc8, 0x86c308..0x86d1e0.

Oggi vedono 4. Con il flusso nativo vedranno 8, che è il comportamento desiderato. Va però controllato che nessuno indicizzi un array
fisso di 4: rischio R2, §3.3.

---

## 2. Q2 – Perché preparare gli slot 4..7 all'init rompe LocalRoom/Shutter

### 2.1 Da dove l'attore prende la resfile [V]
1. Il proprietario del viewer, `[0x398898()+0x270]`, ha il ctor **0x425c70**:
   - `owner+0x50 = 0x605128(0x6400000, "HeapResident", genitore = NULL -> heap corrente, align 8)` (0x425c9c/0x425cac/0x425cc0/0x425cc4);
     `owner+0x58 = 0`;
   - subito dopo crea il viewer (0x425cf0 -> 0x42fc84) e lo inizializza (0x425cfc -> 0x430154).
2. `0x430154` chiama vt+0x10/vt+0x48 delle voci; l'ultima è **viewer+0xc18** (0x430218/0x430220). La sua vtable è 0x1267518:
   - +0x00 = 0x430fa0 (ritorna 1);
   - +0x30 = 0x430fb4 -> **"UI3D/LocalRoom/LocalRoom.szs"**;
   - +0x38 -> "LocalRoom";
   - +0x48 = 0x42f20c (carica);
   - +0x50 = 0x42f38c (animazioni "Shutter" 0xee89b2, "LocalRoom_ftp" 0xee89ba).

   È lo sfondo della lobby locale, non un attore dei personaggi.
3. Caricatore generico `0x4298f8`:
   - heap = `vt+0 ? owner+0x50 : owner+0x58` (0x429a14/0x429a1c): **HeapResident**;
   - `params+0x18 = heap` (0x429a2c);
   - `model = 0x8806c4(params, &resfile)` (0x429a30); poi `vt+0x50(this, resfile)` (0x429a3c/0x429a48).
4. `0x8806c4`:
   - `resfile = 0x8805f4(params)`; `*out = resfile` anche se NULL (0x8806ec);
   - **se è NULL anche il modello è NULL** (0x8806f0).
5. `0x8805f4`:
   - usa il gestore di risorse **generico** `0x8acdd0() = 0x8aba80()+0x220` (0x88060c);
   - richiesta con `heap = params+0x18` (0x880620), poi `0x13ff50` (0x880668); NULL -> NULL (0x88066c);
   - in `0x13ff50`, con heap NULL si usa l'heap corrente (0x13fffc);
   - l'archivio id 0 indica la ROM. `0x894d78` ha solo 9 archivi montati (clamp 0x894e5c), ma l'id 0 non passa da lì.
6. `0x42f38c` chiama `0x68f2dc(modello, &resfile, 1, ..., heap = owner+0x50)` (0x42f3d4/0x42f3e8) -> 0x6920e8 -> 0x6921d4:
   `0x6a8230(resfile[0])` (0x692258/0x69225c) = `[NULL+0x180]`. Poi 0x42f454 `blr` NULL.

   Il modello stesso è NULL (punto 4), coerente con il crash prima di 0x68f364.

### 2.2 Le quattro ipotesi
- **(a) risorsa del caricatore dei personaggi / slot ≥ 4: esclusa [V].** Il percorso (0x4298f8 -> 0x8806c4 -> 0x8805f4 -> 0x13ff50)
  usa `0x8aba80()+0x220`, non `+0x808` (sys) né `+0x800` (rm). Nessun indice di slot. La frase "nel menu gli slot 4..7 servono ad
  altri attori" (STRUCTS.md, step7b_plan §7 #3) **è sbagliata**: va corretta.
- **(b) pool a dimensione fissa: non trovato [V per il percorso, I per gli interni].**
  - archivio id 0 (ROM), nessuna tabella di handle;
  - `0x891984` crea sotto-heap: vt+0x20 a 0x8919c8 e i "DriverModelResource%d"/"EmblemTexResource%d" con 0x8958b8 -> 0x605130;
  - non registra voci nel gestore generico.
- **(c) heap: VINCENTE [V per i numeri, I forte per la catena esatta].**
  - HeapResident = **100 MiB presi dall'heap corrente** (lo stesso heap `0x21626339e8` da cui `0x884bd8`/`0x891984` prendono i
    caricatori e `0x13da50` il KartWork). Viene creato **dopo** l'init del KartDirector.
  - `0x605130` restituisce NULL se il genitore non riesce ad allocare la dimensione (vt+0x30 a 0x6051e4, `cbz` a 0x6051ec).
  - Log già esistenti (max allocabile dell'heap corrente subito dopo l'init del KartDirector):

    | prova | slot preparati all'init | libero dopo l'init | ≥ 0x6400000? | esito |
    |---|---|---|---|---|
    | 7b1_crash_viewer (n = 8, tipo 0) | 8 | **0x5f40dc8** (95,25 MiB) | no (manca 0x4bf238 = 4,75 MiB) | crash LocalRoom |
    | 7b15/7b16/7b17 (ResReqHook n -> 4) | 4 | **0x696fb88** (105,4 MiB) | sì (+5,4 MiB) | ok |
    | vanilla (KartWork 0x1864000) | 4 | 0x696fb88 + 0x1864000 = 0x7fd3b88 ≈ 129,8 MiB [calcolato] | sì (+29,8) | ok |

  - Conti dell'heap corrente (max allocabile 0xa484410 = 164,5 MiB all'ingresso):
    - 4 slot = 10,2 MiB (0xa484178 -> 0x9a513b8);
    - KartWork mod = 48,8 MiB;
    - HeapResident = 100 MiB.

    **A 8 slot: 20,4 + 48,8 + 100 = 169,2 MiB > 164,5.** Il caso "solo 0x891984, senza caricare nulla" crasha allo stesso modo perché
    il costo sono proprio i sotto-heap creati da 0x891984. Preparare gli slot al primo frame della selezione a 8 funziona perché
    HeapResident esiste già.
  - [I] Resta da chiarire come falliscono le allocazioni con HeapResident NULL. 0x5faba8 e 0x13ff50 ripiegano sull'heap corrente, che
    però è quasi pieno. E con soli ~5,4 MiB liberi dopo HeapResident, i 10,2 MiB di PrepareLoaders in sel8 dovrebbero mancare,
    mentre i log mostrano d38 ≠ 0. Probabilmente nel frattempo si libera memoria (risorse del titolo). Si misura in §4.
- **(d) altro:** nulla che spieghi la soglia. In particolare il refcount `[NULL+0x180]` vale per qualunque resfile mancante.

### 2.3 Come lo farebbe un menu nativo a 8
Il codice del menu non ha alcun limite strutturale (i cicli dei descrittori arrivano a 12/16). Il vincolo è il **bilancio dell'heap di
scena**:
- 8 KartUnit = KartWork da ~48,8 MiB invece di 24,4;
- 8 coppie di caricatori = +10,2 MiB;
- HeapResident fisso a 100 MiB.

[I] Su Switch 2 la build avrà heap più grandi. Su Switch 1 servono **~5 MiB in più, con margine 8-10**, presi da una di queste fonti:
1. **HeapResident più piccolo** (0x425c9c `52a0c800` -> per esempio `52a0b400` = 90 MiB, oppure `52a0b800` = 92 MiB), **solo se**
   la misura del suo picco d'uso lascia ≥ 10 MiB liberi. È la toppa più piccola ed è globale (vale per tutti i menu);
2. **heap di scena più grande** alla sua creazione, se il genitore ha spazio. Eden riporta mem 3213552/3363840 KiB, cioè ~147 MiB
   liberi nel processo [V log]. Sito di creazione da trovare con il log §4. È la soluzione più fedele a "più RAM su Switch 2";
3. (sconsigliata) caricatori 4..7 o KartWork in un altro heap (HeapResident stesso): sposta il problema e ne cambia la durata di vita.

---

## 3. Q3 – Piano di patch "flusso originale N = 8"

### 3.1 Patch minime
| # | sito | parola originale [V] | nuova | effetto |
|---|---|---|---|---|
| P1a | 0x86f080 | `321e03eb` orr w11,wzr,#4 | `321d03eb` orr w11,wzr,#8 | cap del modo 5 = 8 in 0x86f074 |
| P1b | 0x86f4ec | `321e03eb` | `321d03eb` | idem in 0x86f474 (scrittura di desc[k], usata da 0x869a14) |
| P1c | 0x86edd4 | `321e03eb` | `321d03eb` | idem in 0x86edb4 (0x7e6e18) |
| P2 | trampolino **0x869a14** (`f81b0ff9`) | — | prima di `Orig`, solo con opzione 8 attiva e `B+8 == 5`: per le entry 4..7 con tipo 4, imposta parti `+0x210..+0x21c = 0`, `+0x220/+0x221 = 0`, tipo `+0x224 = 0`, `+0x50 = 0`; controlla `+0x44 ≠ 2` e `+0x230 == indice` | 0x869a14 assegna k = 0..7: `rm+0x1cc[0..7]`, `entry+0x22c` e B.desc[0..7] validi e coerenti; con P1 B+0x180 = 8, poi A per copia |
| P3 | 0x425c9c | `52a0c800` (100 MiB) | dopo la misura: `52a0b400` (90 MiB) o `52a0b800` (92 MiB); in alternativa heap di scena +8..10 MiB | HeapResident creato con 8 slot preparati all'init |
| P4 | KartWorkSizeHook 0x13da28 | (esistente) | resta | KartWork per 8 KartUnit (0x13da24 sceglie solo in base a e9) |

Se P2 si rivela troppo invasivo (le entry 4..7 a tipo 0 nel menu cambiano +0x184 = umani), si può usare la variante **P2'**. Si aggancia
0x869a14 *dopo* `Orig` e si completa a mano quello che avrebbe fatto il ciclo per k = 4..7:
- `rm+0x1cc[k] = k`, `entry[k]+0x22c = k`;
- `0x86f474(B, k, entry[k]+0x210)` con le parti del giocatore k, o 0 se vuote (desc tipo 0).

Ha lo stesso risultato, ma le entry restano intatte.

### 3.2 Cosa si toglie delle toppe attuali
- **KartResHook** (0x89fc4c): `rm+0x1cc[4..7]` è valido dall'init, quindi `0x89fc4c(4..7)` restituisce lo slot proprio. Sparisce la
  condivisione di H1 (step7b_loader §3).
- **ResReqHook** (0x884bd8) e **ResWaitHook** (0x8843a8): i descrittori 4..7 di tipo 0 vengono richiesti e attesi normalmente.
- **PrepareLoaders / kickSched / unit+0x350 forzati**: gli slot 4..7 vengono preparati dall'init originale.
- **MenuSchedHook**:
  - la limitazione a 4 unità non serve più (0x3df0f8 legge entry valide);
  - il reset `this+0x8d4` non serve più.
- **KartMgrInitHook, KartMgr240InitHook, KartMgr228InitHook, DirGroupInitHook (Force8)**: A+0x180 vale già 8.
- **KartDirInitHook**: via la copia dei descrittori 0..3 -> 4..7 e la forzatura di +0x180; resta solo il log.
- Restano invece:
  - KartWorkSizeHook;
  - MenuKartCfgHook (0x13e59c srotolato su 4) e MenuPresetHook;
  - le guardie del viewer (4 voci);
  - le viste/layer extra, i bit di vista, la correzione layer+0xde4;
  - MatFind/MatUse come rete di sicurezza;
  - Model3DSlotHook (0x13d424).

### 3.3 Rischi residui
- **R1 (alto)** – bilancio di memoria: senza P3 il menu crasha all'avvio (LocalRoom). Con P3 l'heap di scena, dopo HeapResident, resta
  comunque molto stretto: 5,4 MiB o meno, contro i 29,8 del vanilla. Altri pannelli/attori del menu caricati in seguito (online,
  negozio, album) potrebbero esaurirlo. Va misurato il minimo libero su tutte le pagine.
- **R2 (medio)** – A+0x180 = 8 nel menu raggiunge tutti i 116 lettori. Quelli elencati in §1.3 possono avere array fissi a 4. Oggi solo 3
  sono stati esercitati (dg). Prova: un avvio con log degli ingressi in quelle funzioni (solo prime parole) e una scansione statica dei
  loro `cmp #4`/`csel lo` prima di attivare P1.
- **R3 (medio)** – con P2 le entry 4..7 sono "umani" (+0x184 = 8) già nel menu. Questo influisce sulle regole che contano i giocatori
  locali (pulsanti, online). Si mitiga con P2', che lascia le entry intatte.
- **R4 (basso)** – P1 vale per **ogni** modo 5. Con meno di 8 entry valide il conteggio resta il numero di entry valide (tipo 4 = fine),
  quindi il 1-4P vanilla non cambia, purché le entry 4..7 non siano valide da una gara precedente quando il menu viene costruito
  (log 7b17_4p: valide no).
- **R5** – il KartUnit k di 4..7 usa entry k = giocatore k. In sel8 `entry+0x230 = 4..7` [V log]; all'init del menu va verificato
  (`rm+0x18c[k]`, `entry+0x230`), altrimenti 0x8821c0 mette `sys+0x2d8[k] = -1` (H3 di step7b_loader).

---

## 4. Q4 – Un avvio di solo log (nessuna navigazione)
Build con l'opzione 8 attiva **così com'è oggi** (4 slot all'init, nessun crash), più i trampolini di solo log qui sotto. Ognuno fa
`Orig` e poi logga; si limita ai primi ~200 eventi.
1. **0x605130** (`f81b0ff9`), creazione di heap:
   - argomenti: size x0, nome `*(char**)(x1+8)`, genitore x2 (o heap corrente se 0);
   - `MaxAlloc(genitore)` prima e dopo, ritorno x0, LR;
   - loggare tutti i casi con size ≥ 1 MiB o size = 0. Serve a trovare chi crea l'heap di scena `…339e8` e il suo genitore, e a
     confermare che "HeapResident" esca ≠ NULL con 4 slot.
2. **0x425c70** (`d10103ff`), ctor del proprietario del viewer:
   - prima: `CurHeap()` e `MaxAlloc(CurHeap())`. È lo stesso heap del KartDirector? Margine rispetto a 0x6400000;
   - dopo: `owner+0x50`, `MaxAlloc(owner+0x50)`.
3. **0x8805f4** (`f81d0ffc`), caricamento della resfile:
   - richiesta: percorso `*(char**)(x0+0x8)` (SafeString archivio a params+0x0/+0x8), `params+0x18` heap, `params+0x10` id
     archivio;
   - esito: ritorno; con ritorno NULL loggare anche `MaxAlloc(heap)` e `MaxAlloc(CurHeap())`.
4. **0x891984** (`a9ba6ffc`):
   - argomenti: L, heap x1, `MaxAlloc(heap)` prima e dopo;
   - a fine chiamata: `L+0xd38`, LR (0x884bd8 oppure PrepareLoaders).
5. **0x869a14** (`f81b0ff9`), prima e dopo `Orig`:
   - B (`0x86f7d4()`): `+8`, `+0x180`, `+0x184`;
   - per le entry 0..7: `+0x44 +0x50 +0x210..+0x21c +0x224 +0x22c +0x230`;
   - `rm+0x1cc[0..7]`, LR.

   Conferma che sia lei a costruire le settings del menu e mostra lo stato delle entry 4..7 in quel momento.
6. **0x7e6e54** (`a9be4ff4`): dst/src, `src.settings+8`, `+0x180` e i puntatori A/B (`0x86f7bc()`, `0x86f7d4()`). Conferma "A <- B".
7. Una sonda dopo 600 frame dall'ingresso nel menu (MenuFrame):
   - `MaxAlloc(owner+0x50)`, cioè il margine di HeapResident: decide P3;
   - `MaxAlloc(CurHeap del KartDirector)`.

**Prova decisiva senza crash, nello stesso avvio:** nel KartDirInitHook, dopo `Orig`, allocare dall'heap corrente un blocco fittizio di
**0xA40000** (10,25 MiB = i 4 slot in più; vt+0x30 dell'heap, allineamento 8). Poi, nel trampolino 0x605130:
- se il nome è "HeapResident" e il ritorno è NULL: loggare `HEAPRESIDENT_FAIL`, liberare il blocco fittizio (free dello stesso heap tramite la sua vtable; l'offset è
  da ricavare prima della build, non è verificato qui) e richiamare `Orig`;
- loggare il secondo esito.

Lettura dei risultati:
- `FAIL` al primo tentativo, `OK` al secondo, nessun crash: causa (c) confermata, è una questione di sola memoria e non di stato dei
  caricatori;
- `OK` già al primo tentativo e LocalRoom caricato: (c) smentita, si torna a (b)/(d) usando i log di 0x8805f4.
