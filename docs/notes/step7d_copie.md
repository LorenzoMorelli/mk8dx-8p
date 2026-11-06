# Step 7d – chi scrive le "copie" dei kart nella scena finale (analisi statica, 5 nov 2026)

Offset = main 4.0.1 decompresso (`../img/main.bin`, `../img/main.ann.asm`). [V] = letto nel disassemblato (o nel log
`step7d_data/eden_log_gp8_crash.txt`); [I] = ipotesi. Parte da `step7d_premiazione.md` e dal watchpoint misurato
(scrittura di entry8+0x21c 8 -> 10 a 0x86a244). Nessuna patch scritta, nessun file modificato oltre a questo.

## 1. La funzione: 0x869fd4(raceMgr, replay) = "carica nelle entry i kart della registrazione"

**Firma e chiamante [V].** 0x869fd4 (prologo `d101c3ff` a 0x869fd4, `ret` a 0x86a2b8). Unico chiamante diretto: 0x3df7a4,
dentro 0x3df6b8 (metodo virtuale: puntatore a 0x1264f48 nella vtable di una classe di scena/sequenza; i metodi vicini sono
0x3df4f0 e 0x3df600). 0x3df6b8 e' una piccola macchina a stati su `this+0x930`:
- stato 0 (scelto a 0x3df680..0x3df6a0 quando `[0x8aceb4()]+0x4e8` e `+0x4e9` sono entrambi != 0): `x20 = 0x3bc5ac([this+0x9b0])` =
  oggetto **registrazione** (replay); `0x7e6fd4(holder, x20->vt+0x38())` copia le impostazioni registrate in settingsB;
  `0x7e70b8(holder, x20->vt+0x48())`; poi **`0x869fd4(rm = 0x8ace28(), x20)`**; poi settingsB[+4] = 1, 0x86ef2c, stato = 2;
- stato 1: niente registrazione (0x86ec8c, settingsB+0x1a0 = 0x10).
Il watchpoint lo colloca nella transizione verso la scena finale (SceneInit tipo 3 -> tipo 5) [V misurato].

**Corpo [V].** x19 = rm, x20 = registrazione, `x21 = x20->vt+0x38()` = impostazioni registrate (stesso formato di settingsB:
12 record da 0x1c B a +0x30, conteggio a +0x180; cfr. lo scrittore 0x86f474). Ciclo `x24 = 0..15` (0x86a28c `cmp x24,#0x10`),
contatore `w22` = kart della registrazione gia' sistemati, `x27 = 0x1c`:
1. backup dell'entry x24 in `rm+0xf0[x24]` (+0x44, +0x210..+0x228, e 0x8682c4 su +0xd0) (0x86a064..0x86a108);
2. se `entry+0x44 == 0` (tipo 0 = locale umano con GamePlayer) -> **nascosta**: `+0x224 = 4` (0x86a1b0, w25 = 4) e
   `0x865238(e, 1)` (0x86a1e8): +0x50 = 1 (0x86524c), +0x230 = -1 (0x865280), mappa slot pulita (0x86af68 -> SlotSetHook della mod);
3. altrimenti (tipo 1 CPU o 2 vuota): se `w22 >= x21+0x180` -> 0x864fcc(e, 2) (resta vuota) e basta; se no
   (se tipo 2 -> 0x864fcc(e, 1) = diventa CPU), `x28 = w22 < 12 ? w22 : 0`, `x26 = x21 + x28*0x1c + 0x3c`;
   0x86565c(e, vt+0x40(w22), mii?, 0) (ramo Mii se personaggio != 0x1d/0x34 e bit 5); poi copia del record:
   `+0x210/+0x214/+0x218 <- rec+0x30/0x34/0x38`, **`+0x21c <- rec+0x3c` (0x86a244, misurato)**, `+0x220 <- rec+0x40` (h),
   `+0x224 <- rec+0x44`, `+0x228 <- rec+0x48`; `0x865238(e, 0)` (0x86a280) -> slot; `w22++`.
4. fine: `rm+0x1fc = 1` (0x86a298) = "entry sostituite, da ripristinare".

**Formula [V].**
- sorgente: record `i = w22` della registrazione, indirizzo `x21 + 0x30 + i*0x1c` (x21 = vt+0x38 della registrazione,
  x27 = 0x1c, x28 = i con clamp 12); i = 0 .. min(rec+0x180, numero di entry non umane) - 1;
- destinazione: **la i-esima entry con `+0x44 != 0`, in ordine di indice**. Con gli umani in 0..N-1: **dest = N + i**.
  Smentite sia "16 - N + i" sia "12 + i": coincidono con N + i solo per N = 4 (16-4 = 4) e N = 12.
- Ordine dei record = indice kart della gara registrata (0x869a14 numera le entry valide in ordine: umani 0..7, CPU 8..11);
  osservato: personaggi copiati 10, 0, 12, 3, 18, 20, 15, 33 = P1..P8 = record 0..7 [V log + misura].
- Origine dei dati: **la registrazione della gara** (non la lista locali ne' la classifica): `[this+0x9b0]` -> 0x3bc5ac ->
  vt+0x38. Che sia l'ultima gara del GP e' [I forte] (report 7d: 12 flussi per kart).

**Slot +0x230 = 8,9,10,11,0,1,2,3 [V].** In 0x865238(e, 0): se +0x230 != -1 lo tiene, altrimenti `0x86a7f4(rm, id)` = primo slot
libero fra 0..11 (bitmap di 12 byte, 0x86a808..0x86a8a0). Entry 8..11 (CPU in gara) tengono 8..11; entry 12..15 (vuote)
prendono 0..3, liberati poco prima perche' gli umani 0..7 vengono nascosti prima nello stesso ciclo.

**Dopo [V].** 0x8ac6a4 (init della scena successiva): se `scene+0x8c9` -> `0x86a2bc(rm)` ripristina dal backup (ciclo 16,
0x86a418); poi 0x869dec (solo settingsB+8 == 4), `scene+0x8c9 = rm+0x1fc`, tail call **0x869a14** = settingsB dai validi:
`+0x22c` = indice kart, `rm+0x1cc[k]` = entry, record k via 0x86f474, **+0x180 = numero di entry valide**.
Log: `dopo 0x869a14 settingsB+8=0 +0x180=8` e `KartDirInit ... settings+8=0 +0x180=8 -> n=8` (scena 3 -> 5) [V].
KartDirector: `O+0xc0 = [0x86f7bc()+0x180]` (0x13d7ac..0x13d7bc, settings A) [V]; A = B al cambio di scena [I, il log li da' uguali].

## 2. Il limite originale

[V] raceMgr (ctor 0x8689b8, oggetto 0x238 B): array di entry `+0x50 {n, cap}`/`+0x58 ptr` con **buffer inline di 16** a +0x60
(0x5fb28c(rm+0x50, 16, rm+0x60) a 0x8689f8) e array di backup `+0xe8/+0xf0` con buffer inline di 16 a +0xf8; lista
proprietaria +0x38/+0x40 da 16 (alloc 0x80 B a 0x868b30, cap 0x10 a 0x868b44); 0x868b0c crea 16 entry (0x868cdc).
Default 0x868a80: entry 0 tipo224 0, 1..11 = 1, **12..15 = 4 (inutilizzate)**.

**16 = 4 locali massimi + 12 kart.** In gara servono al piu' 12 entry; le 4 di riserva esistono per la registrazione: gli N
umani vengono nascosti (non possono guidare un kart registrato: hanno GamePlayer/controller) e i 12 kart registrati vanno
nelle 16 - N entry non umane. Condizione: `16 - N >= 12` <=> **N <= 4**. Lo conferma 0x869dec (modo 4): umani fissi 0..3
(`cmp x20,#3` a 0x869ecc) e al piu' 12 kart (`cmp w21,#0xb` a 0x869eec) [V].

Con N = 8: entry non umane = 8 (8..15), record 0..7 (gli 8 umani) -> entry 8..15, **i record 8..11 (le 4 CPU) non hanno
entry** e le CPU 8..11 della gara sono sovrascritte (non "copie in coda": e' lo stesso riempimento in ordine). 0x869a14 conta
8 entry valide -> settingsB+0x180 = 8 -> KartDirector n = 8 -> d[8..11] = NULL; la registrazione ha 12 flussi (gruppi
k = 0..11 creati fissi a 0x878b..0x878d50) -> il gruppo 8 consegna gli eventi a d[8] = NULL -> salto a 4 (crash).
Kart della scena k = record k (entry N+k): con 12 entry la corrispondenza flusso k <-> d[k] e' esatta, come nell'originale.

Non esiste una "costante sola": la capienza e' l'array inline (cambiare il cap a 20 sovrascriverebbe +0xe8..). Il numero
che conta e' `16 - N`.

## 3. Opzioni

### (A) Strutturale, flusso originale con N = 8: raceMgr a 20 entry (raccomandata)
Entry necessarie: **8 umani + 12 kart registrati = 20** (estensione di "4 + 12 = 16"). In gara restano usate 12 entry
(0..7 umani, 8..11 CPU, 12..19 tipo224 = 4). Nella scena finale: umani 0..7 nascosti, record 0..11 -> entry 8..19, slot
8..11 tenuti + 0..7 nuovi (12 slot, il limite 12 di 0x86a7f4 basta), 0x869a14 -> 12 valide, **settingsB+0x180 = 12,
KartDirector n = 12** (come in gara: capacita' gia' provata), `rm+0x1cc[12]`, mappa slot +0x19c[12] invariati.

Patch (tutte [V] come parola originale; la nuova parola e' calcolata):

| offset | funzione | originale | nuova | scopo |
|---|---|---|---|---|
| hook ctor 0x8689b8 (gia' RaceMgrCtorHook) | ctor | – | dopo Orig: `rm+0x54 = 20, rm+0x58 = bufE[20]; rm+0xec = 20, rm+0xf0 = bufB[20]` (n e' 0 dopo 0x5fb28c [V]) | capienza 20 fuori dall'oggetto |
| 0x868b30 | 0x868b0c init | `321903e0` orr w0,#0x80 | `52801400` mov w0,#0xa0 | lista +0x40: 20 puntatori |
| 0x868b44 | 0x868b0c | `321c03e8` orr w8,#0x10 | `52800288` mov w8,#0x14 | cap +0x38 = 20 |
| 0x868cdc | 0x868b0c | `7100429f` cmp w20,#0x10 | `7100529f` | crea 20 entry (+ 20 backup da 0x120 B) |
| 0x868adc | 0x868a80 default | `f100429f` | `f100529f` | 16..19 -> tipo224 4 |
| 0x86a28c | **0x869fd4** | `f100431f` cmp x24,#0x10 | `f100531f` | registrazione su 20 |
| 0x86a418 | 0x86a2bc ripristino | `f100429f` | `f100529f` | ripristino di 20 |
| 0x869cd8 | 0x869a14 settingsB | `f100431f` | `f100531f` | conta 12 valide |
| 0x86a834 | 0x86a7f4 slot libero | `f100411f` | `f100511f` | slot occupati da 16..19 visti |
| 0x86af50 | 0x86aee4 (n-esima per tipo) | `f100413f` | `f100513f` | coerenza |
| 0x869174 | 0x869040 (mappa kart, +0x1cc) | `f100411f` | `f100511f` | coerenza |

Altri cicli a 16 sulle entry nel raceMgr (fuori dal percorso della scena finale [I], da estendere per coerenza, stessa
trasformazione `#0x10 -> #0x14`, imm<<10: +0x1000 alla parola): 0x86902c `f100429f` (0x868fec, solo +0x4c<=3), 0x869448
`710042df`, 0x8694cc `710042df`, 0x86953c `f10042df`, 0x8695fc `f100429f`, 0x869754 `f100411f` (0x869180 riempimento menu,
hookato in trampolino), 0x869894 `f10042df`, 0x86996c `f100429f` (0x869760), 0x869fb8 `f100429f` (0x869dec, modo 4: ha
anche #3/#0xb fissi), 0x86a7d8 `f100429f` (0x86a784 rilascio GamePlayer), 0x86b41c/0x86b864/0x86b8bc/0x86b980 `f10042bf`,
0x86b9f8 `f10042bf`, 0x3df5d4 `f10042bf` (0x3df4f0, stessa classe di 0x3df6b8, solo tipo224 == 3).
Non estendibili con una costante: **0x86b19c** (chiamato da 0x501b18) ha 16 chiamate srotolate a 0x865298 (0x86b43c..0x86b61c,
`cmp w8,#0xe/#0xf/#0x10` sul conteggio) -> se serve, hook dopo con 0x865298 sulle entry 16..19 [I: non nel flusso replay];
0x869a14 ha 16 `+0x22c = -1` srotolati (0x869a34..0x869af4) ma il ciclo li riscrive tutti [V]. Esclusi: 0x86ac30 (passo 4 su
+0x17c = locali, non entry), 0x86a594 (sostituito dalla mod). Funzioni fuori dal raceMgr con 0x8ace28 e `cmp #0x10`
da controllare [elenco V, pertinenza I]: 0x44d018, 0x489bf4, 0x4a0ac0, 0x4b43b4, 0x4f9ccc, 0x4fbd7c, 0x501c78, 0x53f9ac,
0x55211c, 0x8845f4, 0x8da04c.
Mod: `CompactLocalsHook` (j < 16) e `AssignGamePlayerHook` (j < 16) -> usare `*(u32*)(rm+0x50)`; `EntryById` usa gia' +0x50.
Effetti collaterali [I]: +4 entry (4 x 0x2b0 + 4 x 0x120 B di heap), ids 16..19 mai visti prima dal codice (SlotSetHook li
accetta: mappa per slot); in gara/menu restano tipo 2/tipo224 4 come 12..15 oggi.
**Premiazione [I]:** identica al caso originale: tutti i 12 kart registrati (8 umani con i personaggi giusti + 4 CPU),
podio con i primi 3 della classifica chiunque siano.

### (B) Prudente: trampolino su 0x87cb60
0x87cb60 [V]: `ldr x8,[x0,#8]; ldr w8,[x8,#8]; cmp w8,#2; cset w0,eq; ret` (prima parola `f9400408`), 12 chiamate.
Callback: `k = *(int32_t*)g; if (kartDir && k >= *(int32_t*)(kartDir+0xc0)) return false; return Orig(g);` con `kartDir`
salvato in KartDirInitHook (0x13d778, gia' presente). Nessun cambio di strutture.
**Premiazione [I]:** solo gli 8 umani (record 0..7 -> entry 8..15, personaggi giusti); le 4 CPU spariscono. Se una CPU e'
nei primi 3, il podio ha un posto senza kart o un altro accesso a d[k] = NULL (posa 0x879998.., camera, lettori di
sys+0x238 in 0x3a2b88..0x3a46c8): rischio concreto (con 4 CPU forti e' frequente).

### (C) Alternativa minima: riusare le entry dei locali 5-8 come ospiti della registrazione
Trampolino su 0x869fd4: prima di Orig, per le entry degli umani 4..7 `0x864fcc(e, &1)` (diventano CPU, GamePlayer
rilasciato); dopo Orig, nel backup `rm+0xf0[id]+0 = 0` cosi' 0x86a2bc li riporta a tipo 0 (0x864fcc -> 0x86a594/B3 riassegna
il GamePlayer). Risultato: non umane = 4..15 = 12, record 0..11 -> entry 4..15, n = 12, slot 4..7 tenuti + 0..3 nuovi.
Nessuna estensione di array. Contro [I]: tocca lo stato dei locali (riassegnazione GamePlayer/npad al ripristino, binding
0x8652f8 non rifatto), ed e' una deviazione dal flusso; premiazione come (A).

## Raccomandazione
(A): e' esattamente il flusso del gioco con la costante di progetto "max locali + 12" portata a 8 + 12; le parole essenziali
sono 8 (ctor + 0x868b30/b44/cdc/adc + 0x86a28c/0x86a418/0x869cd8/0x86a834) piu' i cicli di coerenza. (B) solo come rete di
sicurezza (puo' restare installata anche con (A): con n = 12 non scatta mai).
Verifiche dal vivo: dopo 0x869fd4 entry 8..19 con personaggi dei record 0..11 e +0x230 = 8..11,0..7; dopo 0x869a14
settingsB+0x180 = 12; KartDirInit n = 12 nella scena 3/5; al ritorno al menu entry 16..19 tipo 2 e umani 0..7 ripristinati.
