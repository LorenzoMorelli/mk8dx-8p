# Step 7d – crash nella scena finale del Gran Premio a 8 (diagnosi statica, 4 nov 2026 notte)

Offset = main 4.0.1 decompresso (`../img/main.bin`, `../img/main.ann.asm`). [V] = letto nel disassemblato/log; [I] = ipotesi.
Log del crash: `step7d_data/eden_log_gp8_crash.txt`. Analisi: subagente Opus, riassunta qui. Nessuna patch scritta.

## Fatto
Gran Premio a 8 locali (4 gare) completato senza errori; alla fine della quarta gara, all'inizio del filmato finale / premiazione
(`SceneInit tipo=5`), crash: PC=4 (salto quasi nullo), R1=R19=7 (byte dell'evento, non l'indice), R20=0, LR=main+0xb158c.
Backtrace: 0xb158c <- 0x3a87ac <- 0x79ba24 <- 0x79cf80 <- 0x3aa9ac <- 0x7e3cdc <- 0x602104 <- 0x60211c x5 <- 0x6020bc <- 0xa847d4
<- 0xa84758 <- 0x624378 <- 0x7e35b4. Eden ignora gli abort di memoria (letture a 0 restituiscono 0), da qui il salto a 4.

## Causa
La scena 5 e' la **riproduzione del replay** dell'ultima gara (12 kart registrati), ma il KartDirector della scena ha solo 8 unita':
il gruppo eventi del kart 8 trova d[8] = NULL.
- `this` di 0x3a86f4 = ricevitore delle voci di un kart (ctor 0x3a8608, base 0x79ce08, +0x288 = kart k) [V]. Creato da 0x87c918(gruppo, k):
  gruppo 0x38 B {+0 k, +8 A = flusso di posa 0x879758, +0x10 B 0x3a7e44, +0x18 C = ricevitore}; 0x878b..-0x878d50 crea 12 gruppi in
  mgr+0x118..+0x170 (k = 0..11) [V].
- mgr = [sub4+0x230] = gestore del replay: 13 stazioni a +0x40..+0xa0 (1 globale + 12 per kart); stato +0x194 = 5 (riproduzione):
  0x3ab62c..0x3ab6b0 chiede il modo 6 a tutte (0x7a6c48) -> stazione in stato 6 = tipo 2 (ricevitore) (0x7a7174..0x7a71a8); subito dopo
  `SceneInit tipo=5` (bt 0x3aba3c) [V].
- Crasha il gruppo 8: LR 0x3aa9ac dopo `bl 0x87cadc` (0x3aa9a8) con x0 = [mgr+0x158] = gruppo 8; 0x87cadc aggiorna A, B, C (C in tail
  call); 0x79cf00 su C tipo 2 consegna gli eventi a 0x3a86f4 [V].
- 0x3a86f4: per ogni evento x0 = 0x142764([[sys+0x238] + clamp12(k)*8 + 0x50]) e poi 0xb154c(x0, ev0, ev1, 0) (0xb154c usa [x0+0x258]).
  sys+0x238 = KartDirector O: O+0x50[k] = d (ctor 0x141a5c) per k < O+0xc0, NULL per k >= n (0x13e100..0x13e13c); 0x142764(d) =
  [[[d+0x48]+8]+0x28] = kart, NULL se d NULL [V].
- Dal log: nella scena finale settingsB+0x180 = 8 e KartDirector n = 8 (in gara 12) -> d[8..11] = NULL [V]. Gli id dei kart della scena
  sono 0..7: i dati del gruppo 8 sono la registrazione della gara (12 kart) [I forte].

## Limite originale [I, da verificare]
Prima di 0x869a14 della scena finale (log): le entry 0..7 (umani) vengono nascoste (+0x50 = 1, tipo +0x224 = 4, slot +0x230 = -1) e
si accettano le entry 8..15 con i personaggi di P1..P8 nell'ordine (10, 0, 12, 3, 18, 20, 15, 33), slot +0x230 = 8, 9, 10, 11, 0, 1, 2, 3.
Ipotesi: per il replay il gioco nasconde le entry dei locali e ne crea **copie in coda** all'array di 16 entry del raceMgr (oggetto
0x238 B, entry 0x2b0 B). Originale (N <= 4): 8 CPU (4..11) + 4 copie in 12..15 = 12 kart, come la registrazione. Con N = 8 le copie
occupano 8..15 e **sovrascrivono le CPU 8..11**: 8 kart, e i flussi registrati 8..11 non hanno unita'. Il limite e' la capienza fissa
di 16 entry, non un ciclo a 4. La funzione che scrive le copie NON e' ancora trovata.

## Verifiche da fare per prime (GDB, stub di Eden)
1. bp 0x3a8794: [x19+0x288] == 8 e [[sub4+0x238]+0xc0] == 8.
2. Watchpoint su entry8+0x21c durante la transizione finale: trova la funzione che crea le copie e la formula (16 - N o 12 + i).
3. All'ingresso nella scena 5: [mgr+0x194] == 5 e [[mgr+0x158]+8]+8 == 2.

## Opzioni (da decidere con l'utente)
- **Strutturale (flusso originale)**, dopo la verifica 2: le copie non devono sovrascrivere le CPU. Servono 20 entry (8 umani + 4 CPU
  + 8 copie) e n = 12 (4 CPU + 8 copie): rm+0x1cc[12] e KartDirector restano a 12. Hook sul ctor 0x8689b8: rm+0x50 = 20 e array
  rm+0x58 proprio con 4 entry in piu' (ctor delle entry come le altre); censire i cicli su 16 (`cmp x24,#0x10` a 0x869cd8 in 0x869a14 e
  altri). Invasiva.
- **Prudente**: trampolino su **0x87cb60** (prima parola `f9400408`, "gruppo attivo?"; chiamata solo dai 12 siti 0x3aa8fc..0x3aa9d8):
  se k = [x0] >= [[sub4+0x238]+0xc0] -> false, altrimenti Orig. Esclude A/B/C dei kart che nella scena non esistono (stessa regola del
  gioco per d = NULL per k >= n, 0x13e12c). Effetto: le CPU 8..11 assenti nel filmato finale (come oggi). Piu' stretta ma insufficiente:
  trampolino su 0x3a86f4 (`d10143ff`): copre solo la voce, non la posa (0x879998..) ne' i 20+ lettori di +0x238 in 0x3a2b88..0x3a46c8.
- Rischi: altri lettori del replay per k >= 8 (posa, camera); controllare che il filmato finale mostri gli 8 umani giusti (unita' 0..3
  legate alle entry 8..11 con gli slot di risorse delle CPU).
