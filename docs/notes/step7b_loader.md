# Step 7 B – Caricatore delle risorse dei personaggi nel menu (sys = 0x8ace44())

Analisi statica di `img/main.ann.asm` (4.0.1). Parole verificate: `step7b_data/loader_verify.py` -> `loader_verify.txt`
(66/66 OK). Nessun avvio di Eden/GDB. [V] = VERIFICATO (disassemblato o log già esistenti), [I] = IPOTESI.

## 0. Sintesi
- Il cambio personaggio non passa da 0x148038: c'è uno **scheduler per frame** (0x3deee8) che serve **un solo slot per volta**,
  e solo quando **tutti** i KartUnit esistenti (fino a dir+0xc0 = 8) sono in stato 3. Il thread di caricamento carica nello slot i,
  la callback avvisa **unit[i]**, che scambia il buffer dello slot **unit+0x40** e si ricostruisce con **0x89fc4c(unit+0x40)**.
  Con 8 giocatori i quattro indici coincidono (i = sys+0x2d8[i] = unit+0x40 = entry+0x230; log: unit+0x40 = 0..7, entry+0x230 = 0..7).
  Quindi la callback non sbaglia KartUnit [V].
- **Correzione di lettura:** `sys+0x5040 = 0` è lo stato normale a riposo. Ogni 30 frame lo scheduler manda un precarico di tipo 10
  (0x8826a0), che azzera 0x5040 e non rimette i bit 2..13 (flag 0x200, niente ciclo per slot) [V]. L'indicatore giusto di "thread
  bloccato o morto" è **sys+0x5050 ≠ 0 che resta così** (tipo richiesta in corso). In quel caso lo scheduler esce subito (0x3def10)
  e tutte le 8 anteprime restano ferme.
- **Causa più probabile [I forte]: l'init crea una condivisione fra slot che il doppio buffer non prevede.** All'init a 8,
  KartResHook (kart 4..7 senza entry) fa costruire i KartUnit 4..7 con le risorse degli **slot 0..3** (0x16ec7c -> 0x16eddc ->
  0x89fc4c). Da quel momento il modello frontale di unit j+4 usa il buffer attivo dello slot j. Il doppio buffer dello slot j
  conosce un solo utente: al 2° cambio di Pj il thread ricarica proprio quel buffer (`0x892338(L[1-cur])`), mentre unit j+4 lo sta
  ancora disegnando o distruggendo. Il risultato è una risorsa incoerente, che spiega "va per qualche cambio, poi si blocca",
  i valori float letti come puntatori e i guasti anche nei modelli di unit 0..3.

## 1. Come si chiede il caricamento di uno slot [V]
**Scheduler 0x3deee8(this)**: chiamato da 0x3df438 solo se KartDirector+0xe9 vale 1 (menu). `this` è un gestore del menu;
this+0x8d4 è il prossimo slot del giro.
1. Esce se `sys+0x5050 ≠ 0` (occupato) o se `sys+0x4f19 == 0`.
2. Per i = 0..11 prende `u = 0x13d540(dir, i)` (NULL se i ≥ dir+0xc0). Se `u+0x350` vale 1 ("cambiato"), incrementa
   `u+0x354` (frame dal cambio, massimo 999) e mette il bit i in mask. Controlla anche che **tutti** gli u abbiano `+0x358 == 3`.
3. Se mask ≠ 0, tutti sono a riposo e il sistema è libero, scorre gli slot a partire da this+0x8d4. Per ogni slot i con il bit:
   `res = 0x89fc4c(i)` (KartResHook), `e = rm+0x58[rm+0x1cc[i]]` (**rm+0x1cc[i] letto senza controllo di -1**, 0x3df0f8),
   `desc = e+0x210` (0x1c byte). Poi `res->vt+0x18(desc)` / `vt+0x10(desc)` dicono se la risorsa è diversa
   (body 0x34/0x1d: anche e+0x2a8 bit 2).
   - Se è diversa e `u+0x354 ≥ 0x15` (21 frame di attesa): `u+0x358 = 4` (0x3df194), azzera e+0x2a8 bit 2, `u+0x350`, `u+0x354`,
     mette `u+0x361 = 1` e `this+0x8d4 = i+1`, poi chiama **`0x88259c(sys, 1<<i)`**.
   - Se è uguale: azzera +0x350/+0x354 e chiama 0x16f2a8(u) (rimostra il modello, niente caricamento).
4. Se non ci sono richieste: un precarico ogni 30 frame, **0x8826a0(sys, entry)** (tipo 10, entry a giro 0..15).

**0x88259c (tipo 5)**: rifiuta se è occupato. Altrimenti scrive `sys+0x5050 = 5`, sveglia il thread (vt+0x18), azzera
`sys+0x5040` e chiama **0x8821c0(sys, 0x86f7d4(), mask)**.

**0x8821c0** (chi scrive sys+0x2d8 e sys+0x4d1c):
- attiva le callback: `sys+0x4ee8/0x4ef8/0x4f08 = sys+0x4ef0/0x4f00/0x4f10`. Questi tre delegati li scrive l'init del
  KartDirector (0x13d998..0x13d9b8, solo con e9 = 1);
- `sys+0x4d1c+i*0x1c` = settings(0x86f7d4)+0x30+i*0x1c per i = 0..11;
- per i = 0..11 mette `sys+0x2d8[i] = -1`. Se il bit i è in mask e `rm+0x18c[i] ≠ -1`: `desc[i] = entry[rm+0x18c[i]]+0x210`
  (0x86f074) e `sys+0x2d8[i] = i`. Questo è il ramo in cui lo slot 0 è preparato, cioè sempre nel menu. Nell'altro ramo
  `sys+0x2d8[i] = entry+0x22c`.
- Le mappe sono costruite così: `rm+0x18c[entry+0x230] = entry+0x48` (0x868e30). 0x869040 assegna k = 0,1,.. alle entry con
  +0x230 ≠ -1: `entry+0x22c = k`, `rm+0x1cc[k] = indice entry`. Con 8 giocatori e entry 0..7 → slot 0..7 sono tutte identità [V
  per le entry dai log; mappe da misurare].

Risposta (1): l'indice è **lo slot del KartUnit i**, che coincide con kart = entry+0x22c e con lo slot di risorse entry+0x230.
La richiesta riguarda un solo slot. Non partono mai 8 caricamenti insieme: lo scheduler li mette in fila uno dopo l'altro,
ognuno dopo 21 frame di attesa.

## 2. Thread (0x882a40..0x883318) e callback [V]
Flag per tipo a 0xf73d50: tipo 5 = 0xc (bit 2 = ciclo per slot, bit 3 = 0x8920ec), tipo 10 = 0x200, tipo 2 = 0x10, tipo 6 = 0x2.
Ciclo 0x882fd4, per i = 0..11 con `w23 = sys+0x2d8[i] ≠ -1`:
- predicato `sys+0x4ee8` = 0x13e2ac(dir, w23): **unit[w23]+0x358 == 4**. Se è falso salta il caricamento e mette solo il bit;
- `L = sys+i*0x20+0x160+cur*8`, con cur = +0x170. **`L+0xd38` è il buffer preparato da 0x891984** (vt+0x20(L, heap), 0x8919e0),
  non "risorsa caricata". Se è preparato: `0x892338(L[1-cur], desc_i)` (+0x8920ec). Altrimenti carica nel default
  `sys+0xf8[i]` (0x892338 o 0x892080);
- se c'è la callback `sys+0x4ef8`: **callback(w23) e niente scambio qui** (0x8830d8). Senza callback scambia +0x170 dello slot w23;
- poi `sys+0x5040 |= 0xf73d14[w23]`; a fine ciclo `|= 0x3ffc`; secondo ciclo (bit 3) con callback 0x4f08 -> 0x16f12c(unit)
  (`+0x361 = 1`); `|= 0x200000`; alla fine `sys+0x5050 = 0`.

**Callback**: delegato dir+0x100 → ptmf 0x13e2e8(dir, w23) → se e9 e w23 < dir+0xc0: **0x16f08c(unit[w23])**:
- se `unit+0x350` vale 1 (cambiato di nuovo durante il caricamento): stato 7, nessuno scambio e nessuna costruzione;
- altrimenti scambia +0x170 dello slot **unit+0x40** (se L[cur]+0xd38 ≠ 0), poi **0x16eddc(unit)**: `sel' = 1-sel` (+0x34c),
  `res = 0x89fc4c(unit+0x40)`, 0x16def4 (kart 0x370 da res, pilota 0x1620/0x1548), **0x16e0cc(lista, res+0xd40)** → 0x1724c0 →
  0x122cd4. Quest'ultima rilegge `0x89fc4c([obj+0x10]+0xa8)`, usa res+0x528/+0x8d8 e 0x68f2dc, poi cerca "m_Body" /
  "emission_intensity" nel modello del **kart** (obj+0x18). Infine stato 5.
- Per frame (0x16e8d4, tabella 0xf24198): 5 → 0x16eb20 (scambia sel, distrugge il vecchio modello) → 6 → 7 → 8 → 9 → 10 → 3.

Risposta (2): notifica **solo il KartUnit dello slot richiesto**. Usa la risorsa **attiva dopo lo scambio** (= il buffer appena
caricato, tramite 0x884b50: attivo se preparato, altrimenti default), mai la default se lo slot è preparato. Tutto succede nel
thread di caricamento, compresa la costruzione del modello.

## 3. Perché un modello senza m_Body o una risorsa incoerente
Esclusi o ridimensionati:
- callback al KartUnit sbagliato: no, gli indici sono identità [V];
- stato mancante negli slot 4..7 preparati solo con 0x891984: 0x884bd8 fa per i < n **solo** 0x891984 (s0 = 1.5) sui due
  caricatori. 0x892018 sui 12 default e le altre init (0x884f2c, 0x8852b8) non dipendono da i. Le callback sono globali
  (sys+0x4ef0..), predicato e callback controllano dir+0xc0 = 8. Non manca altro stato [V]. Dai log gli slot 4..7 usano già il
  doppio buffer (cur = 1, entrambi d38 ≠ 0);
- KartResHook con 16 entry: scatta **solo se rm+0x1cc[kart] = -1** (0x86e8e4 → rm+0x1cc[k]; confronto unsigned con rm+0x50 = 16) [V].

Ipotesi in ordine:
- **H1 [I forte] – condivisione creata all'init.** All'init rm+0x1cc[4..7] = -1, quindi KartResHook → Orig(kart-4) e i modelli
  di unit 4..7 sono costruiti dal buffer attivo degli slot 0..3. Il redirect ha davvero agito all'init (§7, tappa 1 #1).
  Conseguenze:
  - (a) al secondo cambio di Pj (j = 0..3) il thread ricarica il buffer ancora usato da unit j+4. È uso dopo la liberazione nel
    rendering/animazione del modello di unit j+4: letture "float come puntatore" 0xBF0BDEDD/0xBF26B0FA. Negli slot 1..3 cur = 1
    all'ingresso, slot 0 cur = 0 (log 7b13);
  - (b) quando unit j+4 viene ricostruito (P5..P8 cambiano), 0x16eb20 distrugge il suo vecchio modello. Così rilascia binding e
    refcount (0x68f2dc / 0x6a8230) dentro una risorsa dello slot j che nel frattempo può essere stata ricaricata per unit j. La
    prossima costruzione di unit j trova una risorsa danneggiata: "m_Body" assente (modelli di unit0/unit1 nel log 7b13) e salto
    a 0 in 0xd4ffc → ... → 0x6dd30c;
  - (c) se il thread muore, sys+0x5050 resta ≠ 0 e lo scheduler non manda più nulla: **tutte le 8 anteprime ferme**.
- **H2 [I] – rm+0x1cc[4..7] resta -1 anche nella selezione a 8** (mappa 0x869040 non ricalcolata). In quel caso la condivisione è
  permanente: lo scheduler legge `rm+0x58[-1]` (0x3df0f8, puntatore spazzatura come entry → desc casuale → ricarichi continui)
  e i modelli 4..7 sono ricostruiti dagli slot 0..3 mentre i caricamenti vanno negli slot 4..7. P5 resta con il personaggio
  vecchio, le monete sono un ripiego del body [I].
- **H3 [I] – blocco senza crash:** se una richiesta per lo slot k trova `rm+0x18c[k] = -1`, allora sys+0x2d8[k] = -1. Niente
  caricamento e niente callback: unit k resta per sempre in stato 4 e lo scheduler non trova mai "tutti in 3", quindi tutto si
  ferma con sys+0x5050 = 0.
- Monete 3D in P6-P8 [I]: modello di ripiego del kart quando la risorsa di quel kart nel buffer non corrisponde al descrittore
  (vedi H1b/H2). Non c'è prova statica di quale ripiego sia.

## 4. Correzione minima proposta
Tutte le parole sono verificate in `loader_verify.txt`.
1. **Spezzare la condivisione dell'init (per H1).** In `m3d8::PrepareLoaders()`, dopo i 0x891984, per k = 4..7 imposta
   `unit[k]+0x350 = 1` e `unit[k]+0x354 = 0x15`: lo scheduler ricostruisce subito 4..7 dai propri slot. Con `KartSchedHook`
   (trampolino 0x3deee8 `d101c3ff`) salva `this` e, una sola volta, scrivi `this+0x8d4 = 4` così 4..7 passano per primi.
2. **Nessuna ricarica degli slot 0..3 finché 4..7 usano ancora le loro risorse.** Hook su 0x88259c (`a9bc5ff8`, chiamante unico
   0x3df2dc). Se `mask = 1<<j` con j < 4 e un unit k ∈ 4..7 non è ancora stato ricostruito dal proprio slot (flag mod), scambia
   la richiesta:
   - unit j: `+0x358 = 3`, `+0x350 = 1`, `+0x354 = 0x15`;
   - unit k: `+0x358 = 4`, `+0x350 = 0`, `+0x354 = 0`, `+0x361 = 1`;
   - poi `Orig(sys, 1<<k)`.

   Il flag "ricostruito" per k si mette in un hook su 0x16eddc (`a9bd57f6`) quando `0x89fc4c(k) == 0x884b50(sys, k)` e lo slot
   restituito è k.
3. **H2:** in KartResHook, fuori da `init8`, **non** reindirizzare. Se `rm+0x1cc[kart] = -1` fuori dall'init, logga e restituisci
   `Orig(kart)` solo dopo aver corretto la mappa: mod `rm+0x1cc[k] = k` e `entry[k]+0x22c = k` per le entry con +0x230 = k,
   come fa 0x869040. Meglio ancora chiamare 0x869040(rm) (inizio da confermare: la funzione termina a 0x86917c, `fstart`
   la fa partire da 0x869040) all'ingresso nella selezione a 8.
4. Tenere MatFindHook/MatUseHook come rete di sicurezza. Con 1-3 attivi devono restare a 0 eventi.

## 5. Cosa misurare a runtime (solo log)
- Ogni 30 frame nella selezione a 8 e a ogni cambio di valore:
  - `sys+0x5050`, `sys+0x5040`;
  - per unit 0..7: `+0x358 +0x350 +0x354 +0x361 +0x348`;
  - per slot 0..7: `cur`, L[cur];
  - `rm+0x1cc[0..11]`, `rm+0x18c[0..15]`, entry[0..7] `+0x22c +0x230 +0x48`.
- Hook di solo log:
  - 0x88259c (mask, chiamante);
  - 0x16f08c (unit, unit+0x40, +0x350, cur prima e dopo);
  - 0x16eddc (unit+0x40 e **res = 0x89fc4c(unit+0x40)**, con lo slot e il buffer a cui appartiene res: confronto con
    sys+i*0x20+0x160/+0x168);
  - KartResHook con contatore quando reindirizza (kart, idx, init8).
- Prove: H1 è confermata se, in sel8 prima di ogni cambio, res di unit k (4..7) = buffer dello slot k-4 e il guasto compare al
  2° cambio dello stesso Pj. H2 è confermata se rm+0x1cc[4..7] = -1 in sel8. H3 è confermata se un unit resta in stato 4 con
  sys+0x5050 = 0. Thread morto se sys+0x5050 resta ≠ 0 per più di 120 frame.
