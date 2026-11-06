# Step 7 B – anteprime 3D a 8: stato al 4 nov 2026 e piano "liste di modelli 12 → 16 record"

Documenti collegati: `step7b_plan.md` §7-7.4 (cronologia), `step7b_menu8.md` (flusso originale con N = 8), `step7b_loader.md`
(caricatore e scheduler), `step7b_cache.md` (liste di modelli, anello da 5, 12 record), `step7b_ghost.md` (fantasmi).
Build con le anteprime: `MK8DX_P7_MENU3D8=1 scripts/build.sh` (default 0 = mod senza anteprime, stabile).

## 1. Cosa funziona [VERIFICATO in Eden]
- **Flusso originale con N = 8** (patch tutto-o-niente in `src/program/main.cpp`, blocco `m3d8`):
  - giocatori locali di default 8: `0x869180(rm, N)` a 0x4f9e60 / 0x4fa254 (`321e03e1` → `321d03e1`);
  - limite del conteggio del modo menu 8: 0x86edd4 / 0x86f080 / 0x86f4ec (`321e03eb` → `321d03eb`);
  - heap della scena menu (TaskHeap della voce del task, creato a 0x603290) 0xd200000 → 0x10000000 (`HeapCreateHook`);
  - heap KartWork 0x1864000 → 0x30c8000 (`KartWorkSizeHook` 0x13da28).
  - Il gioco crea da solo 8 KartUnit, i gestori per kart a 8, 8 slot di risorse preparati e caricati all'avvio, lo scheduler a 8.
- Altri pezzi necessari (limiti del codice, non toppe): `MenuKartCfgHook` 0x13e59c (configurazione srotolata su 4),
  `MenuPresetHook` 0x147738 (preset 8pl), 6 guardie del viewer dei piedistalli, 4 viste extra con camera dal descrittore,
  layer+0xde4 bit 0x4 copiato da Sub0 (fantasmi), maschere di vista 4..7, ordine E1 nel menu, renderer "Main" a 8 viste.
- **4P**: identico all'originale, anche sotto stress. **8P**: 8 anteprime piccole nelle posizioni 8pl, un cambio alla volta corretto per
  tutti gli 8 (log `step7b_data/eden_log_flow8_rebuild.txt`: ogni unita' dal proprio slot e buffer, 0 letture non mappate).
- Risolti lungo la strada (gia' in STATE): stub degli hook inline senza NZCV (P8), letture nulle @0xc0 (guardia Model3DSlot).

## 2. Il problema aperto: cambi simultanei a 8 [VERIFICATO con log]
- Con 8 cursori mossi insieme, dopo un po' la ricerca di un record libero nella lista **Body** fallisce:
  `cache: Body lista sys+77f8 ricerca = -1 ... record: 1/1 x10, 257/257 x2` (log `step7b_data/eden_log_cache_meas1.txt`).
  Subito dopo la ricostruzione del KartUnit non trova il materiale `m_Body` (guardia 0x68fffc), il thread di caricamento puo' morire
  e le anteprime si bloccano per tutti (anche la 4P successiva).
- Causa: **ogni lista di modelli del caricatore ha 12 record**. Nell'originale bastano (4 slot × 2 buffer = 8 maniglie); a 8 giocatori
  le maniglie sono 16. Il menu originale a 4 regge lo stesso stress (prova di controllo con la build di default).
- Secondo limite collegato: **anello di 5 voci** per Body/Wing nel menu (0x884f2c / 0x8852b8, `cmp #5` a 0x885250 / 0x8855dc), che
  sfratta una voce in uso quando le carrozzerie diverse sono piu' di 5 (4 + 1 nell'originale → 8 + 1 a 8 giocatori).
- Le monete nelle celle P6-P8 sono probabilmente il modello di ripiego quando un pezzo resta NULL: da ricontrollare dopo la correzione.

## 3. Strutture [VERIFICATO statico, step7b_cache.md]
- sys = 0x8ace44(), oggetto da 0x9a50 (new a 0x3de9f4). Sette liste "ModelSmartCacheList", **12 record** ciascuna, chiave a 3 parole,
  refcount per maniglia, ricerca srotolata (`cmp #0xc` + 12 controlli):

  | lista | offset in sys | record | heap per record (Stable) | init | ricerca |
  |---|---|---|---|---|---|
  | Driver | 0x5060 | 12 × 0xc0 | 0x500000 | 0x3dac38 | 0x88e6f4 |
  | Emblem | 0x59f8 | | | | |
  | Tire | 0x62c0 | | 0x100000 | | |
  | (altra) | 0x71f0 | | | | |
  | Body | 0x77f8 | 12 × 0xc8 | 0x533333 | 0x3db600 | 0x88f5b4 |
  | Wing | 0x8458 | 12 × 0xc8 | 0x1b3333 | 0x3db958 | 0x890374 |
  | Arm | 0x90b8 | 12 × 0xc0 | 0x119999 | 0x3dbcb0 | 0x890944 |

- Gli heap dei record "Stable" li crea l'avvio (0x3da428) nell'heap BackgroundLoad (0x13b00000, sys+0x4ed0).
- Maniglia nel buffer di caricamento L (ctor 0x890ddc, sys+i*0x20+0x160/+0x168): Body a L+0x3e0 (vt 0x12d2a20: +0x28 lookup 0x88f44c,
  +0x30 rilascio 0x88f898). 0x892338(L, desc) → 0x895a44: rilascia poi cerca. Ricerca 0x88f5b4: record caricato con chiave uguale,
  altrimenti il primo con +0x3c = 0, altrimenti giro modulo 12 dal cursore +0xc58 sui record con refcount ≤ 0; se niente → -1.
- 0x3dd478(record, chiave): riusa senza ricostruire se refcount ≠ 0 o chiave uguale e caricato.
- Anello del menu: 5 voci da 0x98 B a lista+0x960 (heap 0x533333 Body / 0x1b3333 Wing, genitore [sys+0x320] = CourseResource
  0xe404000 creato in 0x3d5794: dopo le 10 voci ~2,8 MiB liberi); il ciclo a 0x885284 (`cmp #0x960`) scrive nei 12 record
  rec+0x98..+0xb8 = voci 0..4, rec+0xc0 = &cursore (lista+0xc5c). La lista Wing segue la Body (+0xc60): niente estensione in loco.
  Parte 0 Body/Wing 0x89d668 / 0x89dae4 (sfratto (c+1) mod 5, refcount decrementato alla cieca 0x89da20, 0x89b290 freeAll+memset).

## 4. Piano per la prossima sessione: liste 12 → 16 record (e anello 5 → 9)
Stesso metodo dei passi precedenti: prima analisi statica completa con parole verificate da script, piano esatto all'utente, poi
implementazione e prova di stress; nessuna toppa, si estendono i limiti del gioco.
1. **Ricostruire ogni lista e tutti i punti che usano i 12 record**: ricerca (0x88e6f4, 0x88f5b4, 0x890374, 0x890944 e quelle di
   Emblem/Tire/0x71f0), assegnazione, rilascio (vt+0x30), refcount, cursore +0xc58, il ciclo dell'anello 0x885284, distruzione,
   lettori per indice (maniglia +0x98), e i campi dopo l'array (cosa c'e' fra un record e la lista successiva).
2. **Rilocare i record** in un'area piu' grande (16 record per lista): le liste sono contigue nell'oggetto sys, quindi niente
   estensione in loco; definire dove (memoria della mod o heap del gioco) e come aggiornare tutti gli accessi (immediati `cmp #0xc`,
   ricerche srotolate, basi `add xN, xM, #off`).
3. **Creare gli heap dei record 12..15** con lo stesso schema dell'avvio (0x3da428, BackgroundLoad sys+0x4ed0), misurando prima la
   memoria disponibile (`MaxAlloc`) in BackgroundLoad e nei genitori.
4. **Anello del menu 5 → 9** (Body e Wing) insieme alle liste: voci extra con lo stesso costruttore (vt da GOT 0x1310528, heap con
   0x605128 e 0x8ad040, genitore [sys+0x320]) e memoria (+27,6 MiB: CourseResource o altro genitore da misurare).
Accettazione: stress con 8 cursori senza ricerche -1, senza guardie m_Body, sys+0x5050 mai fermo, anteprime sempre corrette;
4P invariato; nessuna moneta.

## 5. Esito (4 nov 2026, pomeriggio)
Piano eseguito: step7b_records_plan.md (S0 misure, S1-S3 liste a 16, S4 anello a 9; §7-9 esiti in Eden). Accettazione raggiunta:
stress a 8 con 0 ricerche -1 (Arm escluso, fallimento di acquire presente anche nell'originale), 0 guardie m_Body, sys+0x5050 mai fermo
(max 42 frame), anteprime corrette per tutti gli 8, nessuna moneta (causa diversa: step7b_coin.md), 4P fino ai kart
(step7b_4p_plan.md). Commit 4e3e46b, 1373c2b, 7f8d5cb, 98e6f90, 125c637.
