# Step 7d – censimento per il raceMgr a 20 entry (opzione A) – analisi statica, 5 nov 2026

Offset = main 4.0.1 decompresso (`../img/main.bin`, `../img/main.ann.asm`). [V] = letto nel disassemblato; [I] = ipotesi
(tipicamente: "in quale scena gira"). Nessun file modificato oltre a questo. Parte da `step7d_copie.md` (opzione A).

Premessa usata in tutto il censimento: fuori dalla scena finale (fra 0x869fd4 e il ripristino 0x86a2bc) le entry 16..19 sono
sempre tipo224 = 4 / tipo +0x44 = 2 come oggi le 12..15 (0x868a80 con il ciclo a 20) -> quasi tutto il codice che le salta
o non le vede e' innocuo. Contano: (a) codice che con id 16..19 scrive/legge array da 16 fuori limite; (b) codice che gira
nella scena finale, dove le entry 12..19 portano i kart registrati 4..11 (nell'originale N=4 erano 4..15, tutte <= 15).

## 1. Layout raceMgr [V]

- Allocazione 0x8ac370: `mov w0,#0x238` + 0x5faae0 -> **oggetto 0x238 B** [V]; unico ctor 0x8689b8 (chiamato solo da 0x8ac380).
- 0x5fb28c(arr, cap, buf) = `if (cap>=1 && buf) { arr+8 = buf; arr+0 = 0; arr+4 = cap }` [V].
  - 0x8689f8: arr = rm+0x50 {n +0x50, cap +0x54, ptr +0x58}, buffer inline **rm+0x60..+0xdf** (16 x 8);
  - 0x868a00 `str xzr,[x19,#0xe8]!` (x19 diventa rm+0xe8): arr = rm+0xe8 {+0xe8, +0xec, ptr +0xf0}, inline **rm+0xf8..+0x177**;
    poi le scritture "+0x90/+0x114/..." del ctor sono relative a rm+0xe8 (= rm+0x178, +0x1fc, +0x1fd, +0x200..+0x208,
    -1 su rm+0x17c..+0x19b, zeri fino a +0x234). rm+0xe0 (8 B) azzerato a parte.
- **Nessun altro uso del buffer inline** [V]: le sole chiamate a 0x5fb28c nell'area 0x860000..0x8b0000 sono le due del ctor
  (le altre 0x87eef8/0x87ef58/0x87efb8/0x87f388/0x8abd64 sono altri oggetti); nessun `add x,rm,#0x60/#0xf8` nei metodi del
  raceMgr; nei 319 xref a 0x8ace28 gli accessi diretti a rm sono solo +0x50, +0x58, +0x178/+0x17c/+0x180/+0x184/+0x188,
  +0x1fc/+0x1fd, +0x200..+0x214, +0x20c (indirizzo, 0x862008), +0xe0 (una str e una stp) – mai +0x54/+0x60..+0xdf/+0xec/+0xf0/+0xf8..+0x177.
  Le letture `[x,#0x54]`/`[x,#0xec]` nell'area (0x86e990, 0x86f6ac, 0x869b7c, 0x868434) sono di settings/entry, non di rm.
  -> Sostituire +0x58/+0xf0 con buffer esterni da 20 e cap +0x54/+0xec = 20 e' sicuro; l'inline resta inutilizzato.
- 0x868b0c (init) [V]: lista proprietaria +0x40 (alloc 0x80, allineamento 0x40) con cap +0x38 = 16, contatore +0x4c; se la
  lista e' piena l'entry non viene registrata (csel su slot 0, nessun overflow, solo leak) -> va estesa come da report.
  Il push in +0x50 (0x868c48) e in +0xe8 (0x868cb4) controlla `n < cap` -> con cap 20 dal ctor-hook entrano tutte e 20.
  L'id dell'entry e' salvato in **entry+0x48** (0x8647b0, da 0x86478c(e, rm, idx)).

## 2. Funzioni esterne del report (punto 1)

| funzione | cosa itera / indicizza [V] | array a 16 per entry? | con 20 entry |
|---|---|---|---|
| 0x44d018 | param w1 <= 0x10 = enum (non entry); 0x86e8e4(w1) = kart k (<= 11) -> rm+0x1cc[k] -> entry con controllo `n > id` | no | **OK** (il #0x10 non e' un id) |
| 0x489bf4 | `and #~3/#~7; cmp #0x10` su id percorso (+0xeac, +0x30), chiama 0x86a958 | no | **irrilevante** (non tocca entry) |
| 0x4a0ac0 (vt 0x1273c30) | ciclo **entry 0..15** (0x4a0e84), controllo n, salta tipo 2 / campi -1 / tipo224 4 / +0x50; carica risorse personaggio/kart (0x41ffd8, 0x415740 su e+0x210..0x21c), 0x852d1c(id) clampa a 12 | no (0x852d1c clamp `<0xc`) | **da estendere** 0x4a0e84 [I: serve se gira nella scena finale – allora 16..19 sono kart registrati da caricare] |
| 0x4b43b4 | una entry (`w21` con controllo n) e oggetto kart `[x20+0x728]+0x1f0[w19]` con controllo +0x1e8; il `cmp w8,#0x10` (0x4b4bd8) e' sul tipo personaggio | no | **OK** |
| 0x4f9ccc (avvio, da 0x400b94) | salva tipo224 delle entry **0..15 srotolate** nel globale 0x13426c0+0x2c..+0x68 (16 int; da +0x6c inizia un altro statico: 0x4fed4c), 0x869180(rm,4->8 mod,1,0), ciclo 0..15 0x864c5c(e,0,0); ripristino srotolato in 0x4f9eb8 (0x4fa014..) | **si'**: globale [16] | **NON estendere il salvataggio** (srotolato, nessun posto); 16..19 restano 4: irrilevante [V struttura, I uso]. Ciclo 0x4f9ea4 estendibile (innocuo) |
| 0x4fbd7c (da 0x499fd8) | ciclo 0..15 0x41ff50/0x42022c (tipo224 -> 4 tranne il primo umano e i tipo 3) poi +0x228 = 2 su 0..15 **srotolato** (0x4fc004..0x4fc0cc); idem 0x4fb290 e 0x4fba48 (+0x228 srotolati) e 0x4fbb28 (0x86c010(e+0x58) srotolato 16 con controllo n) | no (ma srotolati) | **irrilevante**: 16..19 restano tipo224 4 [I]. 0x4fbf00 estendibile (innocuo) |
| 0x501c78 (da 0x52e210) | ciclo 0..15: se tipo224 != 0 -> 0x42022c(id,&4) (nasconde) | no | **irrilevante** (16..19 gia' 4); 0x501cd4 estendibile |
| 0x53f9ac (UI risultati) | ciclo entry 0..15 (0x53feb8 `b.lt`) con controllo n; salta tipo 2 / -1 / tipo224 4 / +0x50 / +0x22c < 0; righe indicizzate con il rango (<= 11, `cmp #0xb`) e kart +0x22c clamp 12; 0x5205cc -> 0x420da0(id) con controllo n | no | **da estendere** 0x53feb8 [I: serve solo se gira nella scena finale] |
| 0x55211c | itera i **locali** rm+0x17c[x28] (`cmp x28,#4` csel); il `cmp w8,#0x10` (0x552438) e' sul personaggio (+0x21c) | no | **irrilevante** per le entry (limite 4 locali: questione separata) |
| 0x8845f4 | ciclo 0..15 con controllo n; carica le parti (vt+0x98, e+0x210) per id < 12 o per id >= 12 **solo se umano** (+0x44 == 0) | no | **OK** (16..19 mai umane); estendibile innocuo |
| 0x8da04c | switch su `nn::oe::PopNotificationMessage()-0xf <= 0x10`; scrive solo rm+0x200/+0x204/+0x208 | no | **irrilevante** |
| 0x86b19c (da 0x501b18 <- 0x501a70 <- 0x4e2454/0x4e93e0, dopo 0x854854) | sessione di rete ([0x12f9fb8]+0x28+0xf0, stazioni +0x38/+0x40 e +0xf8/+0x100, 0x853d80 = giocatore rete -> entry id). **Tre array su stack da 16**: `sp+0x8` byte[16] (scritto `strb [sp+8, id]` a 0x86b384/0x86b3d4 **senza limite**), `sp+0x18` int[16] (lista 0..15, scambi a 0x86b858), `sp+0x58` int[16] (-1, indicizzato con +0x22 della stazione). Cicli 0x86b41c/0x86b864/0x86b8bc indicizzano questi array; 16 chiamate srotolate 0x865298 (0x86b43c..0x86b61c, controllo n). Poi 0x86b8dc..0x86b94c itera **n** entry (riempie +0x17c / mappa slot, gia' patch B1e) e 0x86b980 cicla 16 su entry. | **si'** (stack) | **NON estendere 0x86b41c/0x86b864/0x86b8bc** (lettura/scrittura fuori dagli array su stack: sp+0x18.., sp+0x98..0xa7 = registri salvati). Id 16..19 non arrivano [I: in rete gli id sono <= 11]. Nessun hook necessario: percorso solo rete [I]. 0x86b980 e' estendibile |
| 0x3df5d4 (0x3df4f0, stessa classe di 0x3df6b8) | ciclo 0..15 con controllo n, solo tipo224 == 3 (rete) | no | **OK**, estendibile innocuo |

## 3. Altri posti (punto 2)

**Lettori di rm+0x50 esterni [V]:** tutte le 138 letture nei chiamanti di 0x8ace28 sono controlli di limite (`cmp n,id; b.ls`
-> NULL, o `cbz n`/`cmp n,#k; b.lo` per indici costanti). Nessun codice esterno usa n come limite di un ciclo -> nessun ciclo
esterno passa automaticamente a 20. I cicli esterni sulle entry sono tutti a costante 16 (elenco sopra) o per kart 0..11.
Dentro il raceMgr usano n: 0x868a9c (default), 0x86b8dc (0x86b19c), e i controlli inline di EntryById.

**Guardie "id <= 15" (cmp #0xf; b.hi/b.ls) davanti a rm+0x58[id] – non nel report [V]:**

| offset | funzione | uso | con 20 |
|---|---|---|---|
| 0x86ebcc | 0x86ebbc(id) -> e+0x22c (indice kart), altrimenti -1 | chiamata da 0x42022c, 0x420358..0x420ee8, 0x5373cc, 0x546324/0x54633c, 0x5472a8/0x5472c0 e **0x882468** (0x8823f8: per slot 0..11 -> mappa slot -> entry -> indice kart) | **da estendere**: nella scena finale gli slot 0..7 puntano a entry 12..19 (report 7d: slot nuovi 0..7); 16..19 darebbero -1 dove l'originale (12..15) dava l'indice [V codice, I che 0x8821c0 giri li'] |
| 0x86eae8 | 0x86eac4(k): kart k -> rm+0x1cc[k] -> entry -> GamePlayer +0x200 -> +0x768 | chiamata da 0x3a2a1c..0x3a4bf0 (sistema podio/premiazione del report 7d) e 0x7c92c0 | estendere per coerenza: per i kart registrati (CPU, +0x200 nullo [I]) il risultato e' -1 comunque |
| 0x4e4a20 | 0x4e4a04: oggetto -> 0x853d80 -> nome entry (+0x1a0) con controllo n | 0x3ea1e0/0x3eef9c/0x3f0114/0x3f14c0 | estendere (coerenza: nomi dei kart 16..19) |
| 0x50f258 | kart (clamp 12) -> rm+0x1cc -> entry con controllo n -> messaggio | – | estendere (coerenza) |
| 0x4278fc | 0x4278d8: oggetto -> 0x853d80 -> rm+0x58[id]+0x44 (senza controllo n) | – | estendere (coerenza; n = 20 sempre) |

**Guardie "id <= 11" [V]:** 0x4e440c / 0x4e4548 (0x853d80 -> `cmp #0xb`, evento oggetti in gara), 0x4e6cd4 / 0x4e6d8c
(e+0x48 <= 11 -> oggetto kart [x19+0x728]+0x1f0): in gara gli id sono <= 11; nella scena finale saltano gli id >= 12 anche
nell'originale -> **NON TOCCARE**.

**Array a 16 indicizzati con l'id [V]:** trovati solo il globale 0x13426ec[16] (0x4f9ccc/0x4f9eb8, srotolato) e i tre array su
stack di 0x86b19c. Nessuno nei cicli da estendere (verificato: nei corpi dei cicli del punto 4 gli unici accessi indicizzati
con la variabile del ciclo sono rm+0x58[i] / rm+0xf0[i]). Clamp `cmp #0x10; csel lo` nel binario (54 siti): nessuno
alimentato da un id entry fra quelli seguiti (0x3e1b28.. = tabella di nomi 0..15 di una scena, 0x8c96xx record da 0x58 B,
ecc.) [I: non tracciati tutti]. Tabelle per kart (settingsB 12 record, rm+0x1cc[12], mappa slot +0x19c[12], 0x86a7f4 bitmap
12, KartDirector, registrazione 12 flussi) usano indice kart/slot <= 11, invariati (0x869bd8..0x869bec clampa k a 12).

**Funzioni sostituite dalla mod:** 0x868f00 (CompactLocalsHook) contiene il ciclo a 16 0x868fd0 (non nel report) – non serve
patcharlo, ma nella mod `for (j < 16)` -> `j < *(u32*)(rm+0x50)`; 0x86a594 (AssignGamePlayerHook) idem (j < n);
0x86af68 (SlotSetHook) accetta qualunque id.

## 4. Verifica delle parole del report (punto 3) [V]

Tutte le parole originali del report corrispondono al binario. Codifica: `cmp` imm12 a bit 10 -> #0x10 = 0x4000, #0x14 =
0x5000 (+0x1000), #0xf -> #0x13 = +0x1000 (0x3c00 -> 0x4c00). Mov: `mov w0,#0xa0` = 0x52801400, `mov w8,#0x14` = 0x52800288
(MOVZ, imm16<<5) – corrette.

Errori/correzioni:
1. **0x86b41c, 0x86b864, 0x86b8bc vanno tolti dai cicli "per coerenza"**: indicizzano array da 16 su stack (sp+0x8 byte,
   sp+0x18 int, sp+0x58 int) -> con 20 leggono/scrivono fuori (0x86b858 scambia dentro sp+0x18 con indice fino a 19 ->
   sovrascrive sp+0x58; 0x86b880 legge sp+0x98..0xa4). 0x86b980 e 0x86b9f8 (0x86b9b0) invece sono sicuri.
2. 0x869448 (`cmp w22,#0x10; b.ge`) non e' un ciclo ma la guardia d'ingresso del ciclo 0x8694cc: va estesa **insieme** a
   0x8694cc (lo e' gia' nella lista).
3. 0x868adc: il `cmp` e' separato dal `b.ne` da uno `str` (0x868ae0) – nessun problema, la patch e' corretta.
4. Mancano nel report: le 5 guardie `#0xf` del punto 3 (soprattutto 0x86ebcc), e il ciclo 0x868fd0 (dentro 0x868f00, gia'
   sostituita dalla mod).
5. 0x869754 e' dentro 0x869180, hookato a trampolino: la patch di parola vale per l'originale chiamato da Orig [V mod: 4724].

## PATCH DEFINITIVE

| offset | parola originale | nuova / hook | motivo | |
|---|---|---|---|---|
| ctor 0x8689b8 (RaceMgrCtorHook, dopo Orig) | – | `rm+0x54=20, rm+0x58=bufE[20]; rm+0xec=20, rm+0xf0=bufB[20]` (n = 0 dopo il ctor) | capienza 20 fuori dall'oggetto (0x238 B, inline inutilizzato) | [V] |
| 0x868b30 | 321903e0 `orr w0,#0x80` | 52801400 `mov w0,#0xa0` | lista proprietaria 20 puntatori | [V] |
| 0x868b44 | 321c03e8 `orr w8,#0x10` | 52800288 `mov w8,#0x14` | cap +0x38 = 20 | [V] |
| 0x868cdc | 7100429f | 7100529f | crea 20 entry + 20 backup | [V] |
| 0x868adc | f100429f | f100529f | default: 16..19 tipo224 4 | [V] |
| 0x86a28c | f100431f | f100531f | 0x869fd4 registrazione su 20 | [V] |
| 0x86a418 | f100429f | f100529f | 0x86a2bc ripristino 20 | [V] |
| 0x869cd8 | f100431f | f100531f | 0x869a14 conta 12 valide | [V] |
| 0x86a834 | f100411f | f100511f | 0x86a7f4 slot occupati da 16..19 | [V] |
| 0x86ebcc | 71003e7f `cmp w19,#0xf` | 71004e7f `#0x13` | 0x86ebbc id->kart per id 16..19 (0x8823f8 per slot) | [V parola, I scena] |
| 0x86eae8 | 71003e7f | 71004e7f | 0x86eac4 kart->entry->GamePlayer (podio) | [V parola, I effetto] |
| 0x4e4a20 | 71003e7f | 71004e7f | nome entry 16..19 | [V parola, I effetto] |
| 0x50f258 | 71003d1f | 71004d1f | kart->entry 16..19 | [V parola, I effetto] |
| 0x4278fc | 71003c1f | 71004c1f | oggetto->entry 16..19 | [V parola, I effetto] |
| 0x86af50 | f100413f | f100513f | coerenza (n-esima per tipo) | [V] |
| 0x869174 | f100411f | f100511f | coerenza (mappa kart +0x1cc) | [V] |
| 0x86902c | f100429f | f100529f | coerenza (0x868fec) | [V] |
| 0x869448 + 0x8694cc | 710042df / 710042df | 710052df / 710052df | riempimento menu (guardia + ciclo, insieme) | [V] |
| 0x86953c | f10042df | f10052df | riempimento menu, slot | [V] |
| 0x8695fc | f100429f | f100529f | ripristino dentro 0x869180 (rm+0x1fc) | [V] |
| 0x869754 | f100411f | f100511f | 0x869180 (trampolino) | [V] |
| 0x869894 | f10042df | f10052df | 0x869760 | [V] |
| 0x86996c | f100429f | f100529f | 0x869760 | [V] |
| 0x869fb8 | f100429f | f100529f | 0x869dec modo 4 (#3/#0xb fissi restano) | [V] |
| 0x86a7d8 | f100429f | f100529f | 0x86a784 rilascio GamePlayer | [V] |
| 0x86b980 | f10042bf | f10052bf | 0x86b19c, ciclo solo su entry | [V] |
| 0x86b9f8 | f10042bf | f10052bf | 0x86b9b0 | [V] |
| 0x4a0e84 | f100429f | f100529f | caricamento risorse per entry (vt 0x1273c30) | [V parola, I scena] |
| 0x53feb8 | f100429f | f100529f | UI risultati per entry | [V parola, I scena] |
| 0x3df5d4 | f10042bf | f10052bf | coerenza (tipo224 3) | [V] |
| 0x884b3c | f100429f | f100529f | coerenza (innocuo: >=12 solo umani) | [V] |
| 0x4f9ea4 / 0x4fbf00 / 0x501cd4 | f100427f / 710042bf / f100427f | f100527f / 710052bf / f100527f | coerenza, innocui (16..19 gia' 4) – facoltativi | [V] |
| mod CompactLocalsHook / AssignGamePlayerHook | `j < 16` | `j < *(u32*)(rm+0x50)` | id 16..19 | [V] |

## NON TOCCARE

| offset | motivo |
|---|---|
| 0x86b41c, 0x86b864, 0x86b8bc (0x86b19c) | indicizzano array da 16 su stack: con 20 accessi fuori limite [V]; percorso solo rete [I] |
| 0x86b43c..0x86b61c (0x86b19c) | 16 chiamate srotolate 0x865298, non estendibili; solo rete, nessun hook necessario [I] |
| 0x86b384 / 0x86b3d4 (0x86b19c) | `strb [sp+8, id]` senza limite: sicuro solo perche' in rete id <= 11 [I]; non introdurre id >= 16 in quel percorso |
| 0x4f9ccc / 0x4f9eb8 (0x4f9d04.., 0x4fa014..) | salvataggio/ripristino tipo224 in globale [16] (0x13426ec, +0x6c gia' di un altro statico): 16..19 restano 4 [V/I] |
| 0x4fb290, 0x4fba48, 0x4fbd7c (0x4fc004..), 0x4fbb28, 0x82cb74 | scritture srotolate su 16 entry (+0x228, +0x224=4, 0x86c010): 16..19 restano come sono (4) [I] |
| 0x869a34..0x869af4 (0x869a14) | 16 `+0x22c = -1` srotolati: il ciclo esteso (0x869cd8) riscrive +0x22c di tutte le 20 [V] |
| 0x868fd0 | dentro 0x868f00, sostituita da CompactLocalsHook [V] |
| 0x86ac30 (0x86acdc), 0x55211c | iterano i locali (+0x17c), non le entry [V] |
| 0x4e440c, 0x4e4548, 0x4e6cd4, 0x4e6d8c | guardie id <= 11 su oggetti kart (12): corrette anche con 20 [V] |
| 0x869ecc `#3`, 0x869eec `#0xb` (0x869dec) | limiti di umani/kart del modo 4, non di entry [V] |
| 0x86e8e4/0x86e91c `cmp #0xb`, 0x852d1c `cmp #0xc`, 0x86a7f4 bitmap 12 | indici kart/slot (12) [V] |
| 0x44d018, 0x489bf4, 0x4b43b4, 0x8da04c | #0x10 non e' un id entry / nessun ciclo entry [V] |
| 0x5fb28c, buffer inline rm+0x60/+0xf8 | nessun altro utente: lasciarli inutilizzati [V] |
