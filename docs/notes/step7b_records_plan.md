# Step 7 B – Liste di modelli 12 → 16 record e anello Body/Wing 5 → 9: piano esatto

Data: 4 nov 2026. Solo analisi statica di `img/main.ann.asm` / `img/main.bin` (4.0.1). Eden e GDB non sono stati usati, `src/` non
è stato toccato. Mandato: `step7b_records.md` §4. Ogni parola, bersaglio e codifica citati qui è verificata da
`step7b_data/records_verify.py` → `step7b_data/records_verify.txt`: **525 OK, 0 FAIL** (385 parole con disassemblato, bersagli dei
salti, 30 qword di vtable/GOT, 12 stringhe, codifiche delle 5 parole nuove, layout e conti di memoria).
Etichette: **[V]** verificato nel disassemblato, **[D]** dato misurato (log `eden_log_cache_meas1.txt`), **[I]** ipotesi.

---

## 0. Sintesi
- **Le liste sono 8, non 7 [V].** C'è una lista "SmartCacheList" a **sys+0x6b88** (12 × 0x88, chiamata qui *TireSub*) fra Tire e
  0x71f0. Ogni buffer di caricamento L ha **una maniglia per ciascuna delle 8 liste** (vtable dal GOT 0x13105d8..0x1310610).
- Ogni lista è `[12 record][1 voce o anello][cursore]`, poi subito la lista seguente; Arm finisce esattamente a 0x9a50 = fine di sys.
  Niente spazio in loco [V].
- **Opzione scelta: i record 0..11 restano dove sono; i record 12..15 di ogni lista vanno in coda a sys, allargato da 0x9a50 a
  0xb4f0** (2 parole: i due `new`). Nessun sito che oggi legge i record 0..11 cambia. Gli accessi agli indici 12..15 passano per
  hook che calcolano `sys + EXT_L + (i−12)·passo`.
- I siti con offset assoluti sono tanti (ricerche e precarichi srotolati, 0x8845f4 con 144 accessi, 0x3dbff0, 0x881d80/0x881fa0 con
  72 scritture, ctor, init Stable): **rilocare i record 0..11 li toccherebbe tutti**. Con l'estensione in coda restano intatti e si
  aggiunge solo il pezzo 12..15.
- Le ricerche e i precarichi srotolati (14 funzioni) si **reimplementano con N = 16** in trampolini che chiamano le funzioni del gioco
  (acquire per record). Se l'estensione non è pronta, si chiama `Orig`. Il resto si risolve con hook inline sui punti esatti (19) e 4
  trampolini sull'anello.
- **Memoria [D+V]:** +56,5 MiB in BackgroundLoad (resta ~0,5 MiB libero dopo Stable, quindi va allargato), +13,2 MiB in DriverBodySE
  (= [sys+0x4ee0], ~16 KiB liberi, va allargato), +27,6 MiB in CourseResource per l'anello a 9 (~1 MiB libero). **Totale ≈ 97 MiB.**
- **Correzione a `step7b_cache.md` §3 [V]:** l'anello **non** sopravvive in gara. 0x881ae8 (uscita del KartDirector, `b` da
  0x13f610) chiama 0x881d80/0x881fa0, che distruggono le 5 voci e azzerano rec+0x98..+0xc0 dei 12 record.

---

## 1. Ricostruzione delle liste [V]

### 1.1 Layout in sys (ctor base 0x880c8c, ctor Arm 0x881770)
| lista | base | record | fine record | dopo i record | cursore record | nome heap (Stable) | heap/record | genitore |
|---|---|---|---|---|---|---|---|---|
| Driver | 0x5060 | 12 × 0xc0 | 0x5960 | 1 voce 0x90 (vt GOT 0x1310418) | +0x990 | `(Stable) Driver:%d` | 0x500000 | [sys+0x4ed0] BackgroundLoad |
| Emblem | 0x59f8 | 12 × 0xb0 | 0x6238 | 1 voce 0x80 | +0x8c0 | `Emblem` | 0x2147a | idem |
| Tire | 0x62c0 | 12 × 0xb0 | 0x6b00 | 1 voce 0x80 | +0x8c0 | `Tire` | 0x100000 | idem |
| **TireSub** | 0x6b88 | 12 × 0x88 | 0x71e8 | – | +0x660 | `SmartCacheList noname:%d` | 0x1e6666 | **[sys+0x4ee0] = DriverBodySE [D]** |
| 0x71f0 | 0x71f0 | 12 × 0x80 | 0x77f0 | – | +0x600 | `SmartCacheList noname:%d` | 0x166666 | [sys+0x4ee0] |
| Body | 0x77f8 | 12 × 0xc8 | 0x8158 | anello 5 × 0x98 | +0xc58 (anello +0xc5c) | `Body` | 0x533333 | BackgroundLoad |
| Wing | 0x8458 | 12 × 0xc8 | 0x8db8 | anello 5 × 0x98 | +0xc58 (+0xc5c) | `Wing` | 0x1b3333 | idem |
| Arm | 0x90b8 | 12 × 0xc0 | 0x99b8 | 1 voce 0x90 | +0x990 (= 0x9a48) | `Arm` | 0x119999 | idem |

Le voci singole dopo Driver/Emblem/Tire/Arm vengono solo costruite. Il puntatore del record che le indicherebbe (rec+0x90 / +0x80)
viene azzerato dall'init Stable e nessuno lo scrive [V: nessun altro accesso a +0x900/+0x840 nella regione del caricatore].

### 1.2 Campi del record (offset dal record) [V]
| lista | chiave | heap | parti/risorsa | ref | caricato | usato | nome (FixedSafeString: ptr, buffer) | anello |
|---|---|---|---|---|---|---|---|---|
| Driver | +8,+0xc | +0x10 | +0x18..+0x28 | +0x30 | +0x34 | +0x35 | +0x38 (ptr +0x40 → +0x4c) | +0x90 |
| Emblem, Tire | +8,+0xc | +0x10 | +0x18 | +0x20 | +0x24 | +0x25 | +0x28 (+0x30 → +0x3c) | +0x80 |
| TireSub | +8,+0xc, byte +0x10 | +0x18 | +0x20 | +0x28 | +0x2c | +0x2d | +0x30 (+0x38 → +0x44) | – |
| 0x71f0 | +8 | +0x10 | +0x18 | +0x20 | +0x24 | +0x25 | +0x28 (+0x30 → +0x3c) | – |
| Body, Wing | +8,+0xc,+0x10 | +0x18 | +0x20..+0x30 | +0x38 | +0x3c | +0x3d | +0x40 (+0x48 → +0x54) | +0x98..+0xb8 voci, +0xc0 &cursore |
| Arm | +8,+0xc,+0x10, byte +0x14 (u16 chiave: <0x100 → solo 2 parole) | +0x18 | +0x20,+0x28 | +0x30 | +0x34 | +0x35 | +0x38 (+0x40 → +0x4c) | +0x90 |

L'unico puntatore a sé stesso del record è il puntatore del nome. La voce dell'anello (0x98) ha lo stesso schema di Body fino a +0x94.

### 1.3 Tutti i siti che dipendono dai 12 record [V]
| gruppo | siti | accesso | lista |
|---|---|---|---|
| costruzione | 0x880c8c (inline, passi 0xc0/0xb0/0x88/0x80/0xc8), 0x881770 (Arm); reset 0x88340c/0x883570/0x883680/0x883790/0x8838f4/0x883a58, chiamati **solo** dal ctor | sys+assoluto / lista+assoluto srotolato | tutte |
| heap Stable | 0x3da428 → 0x3dac38, 0x3daf80, 0x3db2c0, 0x3db600, 0x3db958, 0x3dbcb0 (lista in x0, `cmp x23,#0xc`); TireSub e 0x71f0 **inline** in 0x3da428 (0x3da930..0x3daa84, 0x3daab4..0x3dabf4, sys+i·passo+assoluto) | | 6 + 2 |
| ricerca della maniglia | 0x88e6f4, 0x88ecfc, 0x88f0c0, 0x88f5b4, 0x88fb0c, 0x88fecc, 0x890374, 0x890944 (chiamante unico: il lookup) | lista+assoluto, 12 controlli srotolati, modulo 12 con smull 0x2aaaaaab | 8 |
| lookup indice→record | 0x88e63c, 0x88f4f4, 0x8902b4, 0x89087c (`cmp w8,#0xc` + csel + madd, offset sys-assoluto 0x5078/0x7818/0x8478/0x90d8) | sys+i·passo+assoluto | Driver, Body, Wing, Arm |
| rilascio per indice | 0x88ea40, 0x88f92c, 0x8906ec, 0x890d14 (refcount a sys+i·passo+0x5090/0x7830/0x8490/0x90e8) | idem | idem |
| rilascio per chiave | 0x88efc0..0x88f018, 0x88f3a4..0x88f3f0, 0x88fd6c..0x88fdc0, 0x890168..0x8901b4 (giro di 12 da sys+assoluto) | sys+assoluto | TireSub, Emblem, 0x71f0, Tire |
| precarico (vt+0xa0 0x3dc340) | 0x3dc4f0, 0x3dc6ec, 0x3dc8e8, 0x3dcb0c, 0x3dcd08, 0x3dcf2c (srotolati, scansione lineare `cmp x9,#0xb`, filtro "usato") | lista+assoluto | 6 (non TireSub/0x71f0) |
| azzera "usato" | 0x8845f4 (unico chiamante 0x3df244): **144 accessi srotolati** sys+0x5070..0x8d2d | sys+assoluto | 6 |
| segna "usato" (vt+0x98 0x3dbff0) | 6 giri da 12 con base sys+assoluta (0x3dc01c..0x3dc304) | sys+assoluto | 6 |
| anello | 0x884f2c/0x8852b8 (ciclo 0x885284/0x885610, `cmp x8,#0x960`: rec+0x98..+0xc0); smontaggio **0x881d80/0x881fa0** (72 `str xzr` srotolate su rec+0x98..+0xc0 + 5 voci) | lista+assoluto | Body, Wing |
| parte 0 | 0x89d668/0x89dae4 (vt+0x10 del record): legge solo rec+0x98..+0xc0 | relativo al record | Body, Wing |
| acquire | 0x3dd1d4, 0x3dd350, 0x3dd478, 0x3dd60c, 0x3dd734, 0x3dd8c8, 0x89333c, 0x89353c, 0x893218, 0x893448 | relativo al record | – |
| distruzione | il dtor di sys 0x8818d4 non tocca i record. 0x881ae8 smonta solo l'anello | – | – |

**Ricerca di siti mancanti [V]:**
- immediati 0x5060..0x9a50 in tutte le funzioni che chiamano 0x8ace44 o che stanno in 0x3d0000..0x3e0000 / 0x880000..0x8a8000;
- forme `add #N, lsl #12`: nessuna;
- tabelle con puntatori alle maniglie.

Gli altri riscontri (0x3c5390, 0x3d0d20.., 0x8a6580, 0x8ddab0, 0x89fff8..) riguardano altri oggetti.

**Offset assoluti difficili da spostare: sì, molti** (le righe "sys+assoluto" della tabella). È il motivo della scelta del §2: non si
spostano.

---

## 2. Dove mettere 16 record

| opzione | cosa cambia | siti da toccare | rischio | durata |
|---|---|---|---|---|
| A. Liste intere (16 record contigui) in memoria della mod | tutti gli accessi ai record 0..11 rediretti | tutto il §1.3 (anche 0x8845f4 da 144 accessi, 0x881d80/0x881fa0 da 72, 6+2 loop Stable, ctor con 16 costruzioni) | un sito mancato rompe anche il 4P | statica |
| B. Liste intere in coda a sys allargato | come A | come A | come A | quella di sys |
| **C. Record 12..15 in coda a sys allargato (scelta)** | +0x1aa0 B in coda; 0..11 invariati | solo i punti in cui nascono, si cercano, si leggono per indice o si scorrono 12 record | un sito mancato tocca solo gli indici 12..15 (usati solo sotto pressione) | quella di sys |
| D. Record 12..15 in memoria statica della mod | come C, senza le 2 parole del `new` | come C | va legata all'istanza di sys | statica |
| E. Record 12..15 con un `new` separato | come C | come C + allocazione | nessun vantaggio | da gestire |

**Scelta C.**
- Tocca il minimo e lascia invariati tutti i percorsi esistenti.
- I record 12..15 hanno la stessa durata dell'oggetto sys e sono indirizzati come quelli originali (sys + costante).
- Restano nell'oggetto del gioco: 2 parole al `new`, +6,8 KB nell'heap di avvio.
- **D è equivalente** (stessi hook, niente parole al `new`) se si preferisce non toccare la dimensione.

Layout dell'estensione [V, script §5]:

| area | sys+ | delta nel lookup/rilascio (indice azzerato dal csel) |
|---|---|---|
| Driver 12..15 | 0x9a50..0x9d50 | 0x49f0 + (i−12)·0xc0 |
| Emblem | 0x9d50..0xa010 | 0x4358 + (i−12)·0xb0 |
| Tire | 0xa010..0xa2d0 | 0x3d50 + (i−12)·0xb0 |
| TireSub | 0xa2d0..0xa4f0 | 0x3748 + (i−12)·0x88 |
| 0x71f0 | 0xa4f0..0xa6f0 | 0x3300 + (i−12)·0x80 |
| Body | 0xa6f0..0xaa10 | 0x2ef8 + (i−12)·0xc8 |
| Wing | 0xaa10..0xad30 | 0x25b8 + (i−12)·0xc8 |
| Arm | 0xad30..0xb030 | 0x1c78 + (i−12)·0xc0 |
| voci 5..8 anello Body / Wing (S4) | 0xb030 / 0xb290 (4 × 0x98 ciascuna) | – |
| **nuova dimensione** | **0xb4f0** | |

`Rec(L,i) = i < 12 ? sys+BASE_L+i·S_L : sys+EXT_L+(i−12)·S_L`.

**Ricerche: reimplementazione o patch?**
- Ogni ricerca ha 12 controlli srotolati con offset fissi, il modulo 12 in 3 istruzioni (smull/asr/msub) e `cmp #0xc` in 2-3 punti.
  I record 12..15 non sono contigui e manca lo spazio per 4 controlli in più.
- Le patch di parola non bastano. Un trampolino con la stessa sequenza a N = 16 chiama le stesse acquire del gioco e conserva cursore
  e ordine. Sono 14 funzioni piccole e regolari, con un solo modello parametrizzato (tabella `LD` sotto).

---

## 3. Patch e hook esatti per sotto-obiettivo

### Descrittore comune (C++)
```cpp
struct LD { uint32_t base, S, ext, cur, heap, loaded, ref, used, name, nameBuf; int key; uintptr_t acq, pre; int w3; bool idx; };
//              base    S     ext     cur   heap ld   ref  used name buf  chiave   acq(ricerca) acq(precarico) w3 indice
LD Driver  = {0x5060,0xc0,0x9a50,0x990, 0x10,0x34,0x30,0x35,0x38,0x4c, K2,     0x3dd1d4, 0x3dd1d4, 0, true };
LD Emblem  = {0x59f8,0xb0,0x9d50,0x8c0, 0x10,0x24,0x20,0x25,0x28,0x3c, K2,     0x89333c, 0x3dd350, 1, false};
LD Tire    = {0x62c0,0xb0,0xa010,0x8c0, 0x10,0x24,0x20,0x25,0x28,0x3c, K2,     0x89353c, 0x3dd60c, 1, false};
LD TireSub = {0x6b88,0x88,0xa2d0,0x660, 0x18,0x2c,0x28,0x2d,0x30,0x44, K2B,    0x893218, 0,        0, false};
LD L71f0   = {0x71f0,0x80,0xa4f0,0x600, 0x10,0x24,0x20,0x25,0x28,0x3c, K1,     0x893448, 0,        0, false};
LD Body    = {0x77f8,0xc8,0xa6f0,0xc58, 0x18,0x3c,0x38,0x3d,0x40,0x54, K3,     0x3dd478, 0x3dd478, 0, true };
LD Wing    = {0x8458,0xc8,0xaa10,0xc58, 0x18,0x3c,0x38,0x3d,0x40,0x54, K3,     0x3dd734, 0x3dd734, 0, true };
LD Arm     = {0x90b8,0xc0,0xad30,0x990, 0x18,0x34,0x30,0x35,0x38,0x4c, KARM,   0x3dd8c8, 0x3dd8c8, 0, true };
// Match: K1 k0==r+8; K2 +k1==r+0xc; K2B +byte k[8]==byte r+0x10; K3 +k2==r+0x10;
//        KARM: u16 k[0xc] < 0x100 ? K2 : (K3 && (k[0xc]&0xff)==byte r+0x14)   (0x89098c..0x8909e8, 0x3dcf70..)
```
Flag `ext::ready` = le parole del `new` sono state verificate e scritte **e** CtorExt è passato da questa istanza di sys. Tutti gli
hook seguenti chiamano `Orig` (o non fanno nulla, se inline) quando `!ready`.

### S0 – solo misure (nessuna patch)
| offset | parola | tipo | scopo |
|---|---|---|---|
| 0x881ce8 | d100c3ff | trampolino | `MaxAlloc(CurHeap())` all'ingresso: libero nel genitore di BackgroundLoad (x2 = 0 a 0x881d20) |
| 0x3da428 | d10383ff | trampolino | dopo `Orig`: `MaxAlloc([sys+0x4ed0])`, `MaxAlloc([sys+0x4ee0])` (atteso ~0,5 MiB e ~16 KiB [D]) |
| 0x3d5794 | d10103ff | trampolino | `MaxAlloc(x1)` = genitore di DriverBodySE/CourseResource (2,7 GiB all'avvio [D]); ripetere in gara |
| 0x88ecfc, 0x88f0c0, 0x88fb0c, 0x88fecc | d10103ff, d100c3ff ×3 | trampolino (log) | ritorni NULL delle 4 liste "a puntatore" (oggi non misurate) |

Inoltre:
- correggere `cachediag::Report`: oggi legge +0x3c/+0x24 con il passo della lista anche per Driver e Arm. Va usato `LD.loaded`/`LD.ref`.
  La riga "Arm −1 … 0/0" del log 7b è quindi illeggibile;
- un contatore per frame del menu: per lista, numero di record con ref > 0 (massimo) e numero di maniglie;
- `MaxAlloc([sys+0x320])` dopo 0x884bd8.

Domande a cui risponde S0:
1. Quali liste arrivano a 12 bloccati (TireSub e 0x71f0 comprese)?
2. Il genitore di BackgroundLoad regge +58 MiB?

### S1 – sys allargato e costruzione dei record 12..15 (2 parole + 1 trampolino)
| offset | prima | dopo |
|---|---|---|
| 0x3de9f4 | `52934a00 mov w0,#0x9a50` | `52969e00 mov w0,#0xb4f0` |
| 0x8ac240 | `52934a00 mov w0,#0x9a50` | `52969e00 mov w0,#0xb4f0` |

Sono gli unici due `new` di sys [V, script §4]. Tutto o niente.

**CtorExt**, trampolino su **0x880c8c** (`d10203ff`), firma `void(uint8_t* sys)` (chiamanti 0x3da08c e 0x8ac24c):
```cpp
Orig(sys);
if (!sizeOk) return;                                   // parole del new non applicate: non scrivere oltre 0x9a50
for (LD& L : all8) for (int e = 0; e < 4; e++) {
    uint8_t* r = sys + L.ext + e * L.S;
    memcpy(r, sys + L.base + 11 * L.S, L.S);           // record 11 appena costruito (i reset del ctor sono no-op: heap = 0)
    *(uint8_t**)(r + L.name + 8) = r + L.nameBuf;      // unico puntatore a sé stesso (ctor: 0x881164/0x881180, 0x881280/0x8812a0, ...)
}
// S4: voci 5..8 come la voce 4: memcpy(sys+0xb030+e*0x98, sys+0x77f8+0x960+4*0x98, 0x98); *(+0x48) = self+0x54  (idem Wing 0xb290/0x8458)
ext::sys = sys; ext::ready = true;
```
Prova: avvio normale, log delle 8×4 copie, 4P invariato (le estensioni non sono ancora usate).

### S2 – heap dei record 12..15 (1 trampolino + 1-2 parole)
**StableExt** = lo stesso trampolino di S0 su **0x3da428** (vt+0x90 di sys, chiamato da 0x881d48). Dopo `Orig`, ripete il corpo del
ciclo originale per i = 12..15 (0x3db670..0x3db7c0 per Body; analoghi 0x3daca8.., 0x3daff0.., 0x3db330.., 0x3db9c8.., 0x3dbd20..;
inline 0x3da930.. e 0x3daab4..):
```cpp
for L in {Driver,Emblem,Tire,Body,Wing,Arm}: parent=[sys+0x4ed0], size = 0x500000/0x2147a/0x100000/0x533333/0x1b3333/0x119999,
                                             fmt "ModelSmartCacheList(Stable) %s:%d" (0xee6646), %s = "Driver".. (x4 di 0x3da7c4..0x3da87c)
for L in {TireSub, L71f0}:                   parent=[sys+0x4ee0], size = 0x1e6666 / 0x166666, fmt "SmartCacheList noname:%d" (0xee668e)
  for i in 12..15: r = Rec(L,i);
    azzera anello: Driver/Arm r+0x90; Emblem/Tire r+0x80; Body/Wing r+0x98..+0xb8  (0x3dacac, 0x3daff4, 0x3db334, 0x3db674/67c, 0x3db9cc, 0x3dbd24)
    nome: formatta fmt(%s, i) nel buffer del nome di r (cap 0x40, come la copia 0x3db6bc..0x3db76c)
    h = 0x605128(size, r+L.name, parent, 8, 1, 0)        // 0x3db77c..0x3db78c
    *(void**)(r+L.heap) = h;
    if (h) { 0x8ad040(h); r[L.loaded] = 0; parti/risorsa = 0; }   // 0x3db798..0x3db7a8
    *(int*)(r+L.ref) = 0; r[L.used] = 0;                           // 0x3db7b8/0x3db7bc
  log MaxAlloc(parent) prima/dopo; se h == NULL il record resta con heap 0 e tutte le ricerche lo saltano (come l'originale)
```

Parole di memoria (scelte con S0):
| offset | prima | dopo | quando |
|---|---|---|---|
| 0x3d57b8 | `52a04f20 mov w0,#0x2790000` (+ movk 0xe000 a 0x3d57bc) | `52a06a00 mov w0,#0x3500000` → DriverBodySE **0x350e000** (+0xd70000, margine 0x3ccd0) | sempre con S2 (libero ~16 KiB [D]) |
| 0x881d10 | `52a27600 mov w0,#0x13b00000` | `52a2ea00 mov w0,#0x17500000` (+0x3a00000, margine 0x17ae1c) | se `MaxAlloc([sys+0x4ed0])` dopo Stable < 0x38851e4 + 24 intestazioni (atteso: sì, ~0,5 MiB [D]) **e** il genitore misurato a 0x881ce8 ha ≥ 58 MiB |

### S3 – ricerche e accessi a N = 16 (18 trampolini, 19 inline, 0 parole)
**Search16** – 8 trampolini, firma `R(uint8_t* list, const void* key, void* x2)`, R = `int32_t` (Driver, Body, Wing, Arm) o puntatore:

| offset | parola | lista | note |
|---|---|---|---|
| 0x88e6f4 | d10103ff | Driver | sostituisce `CacheDriverHook` |
| 0x88ecfc | d10103ff | TireSub | |
| 0x88f0c0 | d100c3ff | Emblem | |
| 0x88f5b4 | d10183ff | Body | sostituisce `CacheBodyHook` |
| 0x88fb0c | d100c3ff | 0x71f0 | |
| 0x88fecc | d100c3ff | Tire | |
| 0x890374 | d10183ff | Wing | sostituisce `CacheWingHook` |
| 0x890944 | d10183ff | Arm | sostituisce `CacheArmHook` |

```cpp
if (!ready) return Orig(list, key, x2);
K kc = *key;                                                      // copia sullo stack come l'originale (8/12/16 byte)
for i<16: r=Rec(i); if (heap(r) && loaded(r) && Match(r,key))      // 1. stessa chiave
              return idx ? (ACQ(r,&kc,x2,0), i) : ACQ(r,&kc,x2,W3);   //    indice restituito senza guardare l'esito (0x88f7a0)
for i<16: r=Rec(i); if (heap(r) && !loaded(r)) {                   // 2. primo libero (heap == 0 → salta, 0x88f630)
              res=ACQ(r,&kc,x2,W3); return idx ? ((res&1) ? i : -1) : res; }
int c = *(int*)(list+CUR);                                        // 3. giro dal cursore
for k<16: j=(c+k)%16; r=Rec(j); if (heap(r) && ref(r) <= 0) {
              *(int*)(list+CUR) = (c+1)%16;                       //    c+1, non j+1 (0x88f7ec..0x88f808)
              res=ACQ(r,&kc,x2,W3); return idx ? ((res&1) ? j : -1) : res; }
log(lista, 16 ref); return idx ? -1 : nullptr;
```

**Pre16** – 6 trampolini, firma `void(uint8_t* list, const void* key, void* x2)`:
| offset | parola | lista |
|---|---|---|
| 0x3dc4f0 | d100c3ff | Driver |
| 0x3dc6ec | d100c3ff | Emblem |
| 0x3dc8e8 | d10103ff | Body |
| 0x3dcb0c | d100c3ff | Tire |
| 0x3dcd08 | d10103ff | Wing |
| 0x3dcf2c | d10103ff | Arm |

```cpp
if (!ready) return Orig(...);
for i<16: if (heap&&loaded&&Match) return;                         // già presente
for i<16: if (heap&&!loaded) { PRE(r,&kc,x2,1); return; }
for i<16: if (heap && ref<=0 && !used) { PRE(r,&kc,x2,1); return; }    // scansione lineare, nessun cursore (0x3dc60c..0x3dc640)
```

**IdxL/IdxR** – 8 hook inline, eseguiti prima dell'istruzione. Con i ≥ 12 il `csel` ha già azzerato l'indice, quindi basta aggiungere il
delta del §2: `if (12 <= i && i < 16) X8 += (EXT_L − BASE_L) + (i−12)·S_L`.
| offset | parola | istruzione | maniglia / indice |
|---|---|---|---|
| 0x88e63c | f9683d08 | `ldr x8,[x8,#0x5078]` | X20, [+0x90] (Driver lookup) |
| 0x88f4f4 | f97c0d08 | `ldr x8,[x8,#0x7818]` | X20, [+0x98] (Body lookup) |
| 0x8902b4 | f8696908 | `ldr x8,[x8,x9]` (x9 = 0x8478) | X20, [+0x98] (Wing lookup) |
| 0x89087c | f8696908 | `ldr x8,[x8,x9]` (x9 = 0x90d8) | X20, [+0x98] (Arm lookup) |
| 0x88ea40 | b9400109 | `ldr w9,[x8]` refcount | X19, [+0x90] (Driver rilascio) |
| 0x88f92c | b9400109 | idem | X19, [+0x98] (Body) |
| 0x8906ec | b9400109 | idem | X19, [+0x98] (Wing) |
| 0x890d14 | b9400109 | idem | X19, [+0x98] (Arm) |

I `cmp w8,#0xc` (0x88e61c, 0x88f4d4, 0x890290, 0x890858, 0x88ea28, 0x88f914, 0x8906d4, 0x890cfc) restano: nessuna patch di parola.

**PtrR** – 4 inline all'uscita del rilascio per chiave. La fine del ciclo si riconosce dal puntatore uguale all'ultimo + passo; con una
corrispondenza il puntatore resta sul record:
| offset | parola | lista | registro / valore a fine ciclo | chiave nella maniglia (X19) |
|---|---|---|---|---|
| 0x88f018 | f9002e7f `str xzr,[x19,#0x58]` | TireSub | X9 == X0+0x6bb4+0x660 | +8, +0xc, byte +0x10 |
| 0x88f3f0 | f900527f `str xzr,[x19,#0xa0]` | Emblem | X8 == X0+0x5a1c+0x840 | +8, +0xc |
| 0x88fdc0 | f9002a7f `str xzr,[x19,#0x50]` | 0x71f0 | X8 == X0+0x7214+0x600 | +8 |
| 0x8901b4 | f900527f `str xzr,[x19,#0xa0]` | Tire | X8 == X0+0x62e4+0x840 | +8, +0xc |

Callback: se il ciclo è finito senza trovare nulla, cerca nei record 12..15 un `heap && loaded && Match`. Al primo trovato:
`if (ref−1 >= 0) ref−−` (come 0x88f408..0x88f414), poi stop.

**UsedClr** – inline a **0x884ae8** (`aa1f03f4`, dopo i 144 azzeramenti di 0x8845f4 e prima del giro vt+0x98). X19 = sys. Per le 6
liste e i record 12..15: `if (heap) used = 0`.

**UsedSet** – 6 inline all'uscita dei giri di 0x3dbff0. `X8 == 12` vuol dire "non trovato": la corrispondenza mette `w8 = 1`
(0x3dc2d0..), Arm mette `x8 = sys+i·0xc0`. Callback: primo record 12..15 con `heap && loaded && Match(registri)` → `used = 1`.
| offset | parola | lista | chiave nei registri (ancora vivi) |
|---|---|---|---|
| 0x3dc068 | 294257f4 | Driver | W20 (rec+8), W21 (+0xc) |
| 0x3dc0bc | b9400274 | Emblem | W20, W21 |
| 0x3dc128 | b9400674 | Body | W20, W21, W22 (+0x10) |
| 0x3dc180 | 29415674 | Tire | W20, W21 |
| 0x3dc1e8 | 29405674 | Wing | W20, W21, W22 |
| 0x3dc308 | 910023e0 | Arm | W23 == 1 ? (W20, W21, W22, byte W19 vs +0x14) : (W20, W21) |

**RingFill / RingDown** – 4 trampolini:
| offset | parola | firma | dopo `Orig` |
|---|---|---|---|
| 0x884f2c | d10383ff | `void(list, int size, void* parent, x3, const char* name)` (Body) | per i = 12..15: `r+0x98+8k = list+0x960+k·0x98` (k = 0..4), `r+0xc0 = list+0xc5c` (come 0x885268..0x885290) |
| 0x8852b8 | d10383ff | idem (Wing) | idem |
| 0x881d80 | a9be4ff4 | `void(list)` (Body, da 0x881c90) | per i = 12..15: azzera r+0x98..+0xc0 (come 0x881e8c..0x881f84) |
| 0x881fa0 | a9be4ff4 | idem (Wing, da 0x881c9c) | idem |

Prova di accettazione S3, stress con 8 cursori:
- 0 ritorni −1/NULL in tutte le 8 liste;
- 0 guardie 0x68fffc;
- sys+0x5050 mai fermo per più di 120 frame;
- massimo bloccati per lista ≤ 15;
- 4P: anteprime e tempi uguali.

### S4 – anello Body/Wing 5 → 9 (2 trampolini + estensione di 4 + 1 parola)
Ripresa di `step7b_cache.md` §4.1 con la rilocazione: **le voci 5..8 non stanno nel record.** rec+0xc8 è già il record seguente e i
record 12..15 hanno lo stesso schema. Stanno quindi in una tabella per lista in coda a sys: Body 0xb030, Wing 0xb290 (4 × 0x98),
costruite da CtorExt.
- **RingFill** (0x884f2c/0x8852b8), in più dopo `Orig`: per k = 5..8, voce `v`:
  - nome `"ModelSmartCacheList(Menu) noname:%d"` (0xf0cf7e, k; con x4 ≠ NULL il ramo con nome, non usato da 0x884d24/0x884d40);
  - `h = 0x605128(size, v+0x40, parent, 8, 1, 0)` (0x885214..0x885224) → v+0x18;
  - se h: `0x8ad040(h)`, v+0x3c = 0, v+0x20/+0x28/+0x30 = 0;
  - v+0x38 = 0, v+0x3d = 0.
- **RingDown** (0x881d80/0x881fa0), in più: per le voci 5..8:
  - `if (ref−1 >= 0) ref−−`;
  - `if (v+0x18) { vt+0x20(heap); v+0x18 = 0; *(char*)[v+0x48] = 0; }` (come 0x881d98..0x881dc8).
- **Part0x9**, trampolini su **0x89d668** (Body) e **0x89dae4** (Wing) (`d101c3ff`, vt+0x10 del record, qword 0x12d22f8 / 0x12d2358).
  Lo pseudocodice è quello di `step7b_cache.md` §4.1, con `E[0..4] = rec+0x98..+0xb8` e `E[5..8] = sys+0xb030/0xb290 + k·0x98`
  (la lista è nota dall'hook). Il cursore resta `*(int*)rec[+0xc0]` e si usa `Orig` se `part ≠ 0 || rec+0x98 == 0 || !ready`.
  Il modulo dell'originale è `c > 3 ? 0 : c+1` (0x89d97c, `cmp #5` a 0x89d988); a 9 diventa `c > 7 ? 0 : c+1`.
- **Memoria:** 0x3d57f0 `52a1c800 mov w0,#0xe400000` → `52a20000 mov w0,#0x10000000` (con movk 0x4000 a 0x3d57f8: CourseResource
  0xe404000 → **0x10004000**, +28 MiB, margine 0x66668).
  - [sys+0x320] = CourseResource (0x210720b020 nel log), ~1 MiB libero dopo le 10 voci: 0x2b6e88 prima di Wing:4 − 0x1b3333 [D].
  - Il genitore (0x2104a00000) aveva 2,7 GiB liberi all'avvio [D]; va rimisurato in gara.
- **Durata [V].** Le voci nascono in 0x884bd8 (KartDirector del menu, e9 = 1, che mette sys+0x4f1a = 1 a 0x884c04) e muoiono in
  0x881ae8 (`b` da 0x13f610 all'uscita del KartDirector, solo se 0x4f1a ≠ 0), che azzera anche i puntatori nei record.
  - In gara rec+0x98 = 0, quindi la parte 0 usa vt[0] (heap del record) e Part0x9 chiama sempre `Orig`.
  - Le voci 5..8 seguono lo stesso ciclo grazie a RingFill/RingDown. Niente puntatori pendenti fra due menu.

Accettazione S4:
- con più di 5 body distinti fra gli 8 giocatori, nessuno sfratto di una voce usata da un record visibile (log epoca/STALE di
  `step7b_cache.md` §4.3);
- cursore su 0..8;
- `MaxAlloc([sys+0x320])` dopo RingFill > 0.

### Conteggio
| sotto-obiettivo | patch di parola | trampolini | inline |
|---|---|---|---|
| S0 misure | 0 | 7 (3 + 4 ricerche a puntatore, solo log) | 0 |
| S1 struttura | 2 | 1 | 0 |
| S2 heap | 1 (+1 condizionale) | 0 (riusa 0x3da428 di S0) | 0 |
| S3 N = 16 | 0 | 18 (8 Search16, 6 Pre16, 4 Ring) | 19 (8 Idx, 4 PtrR, 1 UsedClr, 6 UsedSet) |
| S4 anello 9 | 1 | 2 (+ estensione dei 4 Ring) | 0 |

---

## 4. Memoria [V conti, D liberi]
| destinazione | serve | libero oggi | azione |
|---|---|---|---|
| BackgroundLoad [sys+0x4ed0] (0x13b00000, 0x607aa8, genitore = heap corrente a 0x881d20) | 0x38851e4 = 56,5 MiB (6 liste × 4) | ~0,5 MiB dopo Arm:11 (0x1a1a20 − 0x119999) | 0x881d10 → 0x17500000 (+58 MiB) se il genitore li ha (misura S0 a 0x881ce8) |
| DriverBodySE [sys+0x4ee0] (0x279e000, 0x3d57b8) | 0xd33330 = 13,2 MiB (TireSub + 0x71f0) | ~16 KiB (0x16a8a0 − 0x166666) | 0x3d57b8 → 0x350e000; genitore 0x2104a00000 |
| CourseResource [sys+0x320] (0xe404000) | 0x1b99998 = 27,6 MiB (anello) | ~1 MiB | 0x3d57f0 → 0x10004000 (S4) |
| heap di avvio di sys | +0x1aa0 B | – | S1 |

**Totale ≈ 97,3 MiB**, sempre occupati (avvio), anche in gara. Misure prima:
1. `MaxAlloc(CurHeap)` a 0x881ce8 (genitore di BackgroundLoad);
2. `MaxAlloc` di BackgroundLoad e di [sys+0x4ee0] dopo 0x3da428;
3. `MaxAlloc(x1)` a 0x3d5794;
4. `MaxAlloc([sys+0x320])` nel menu;
5. ripetere 1-4 nella prima gara a 8 dopo S2/S4.

Come `m3d8::MaxAlloc` (vt+0x80(h, 8)).

---

## 5. Rischi, ordine e accettazione
**Ordine.** Un commit per passo, ognuno dietro il flag `MK8DX_P7_REC16` (default 0), tutto o niente con il controllo delle parole:
1. **S0** misure;
2. **S1** struttura (la prova: avvio e 4P identici);
3. **S2** heap (la prova: log di 32 heap creati, `MaxAlloc` positivi, 4P identico);
4. **S3** N = 16 (stress a 8, accettazione §3);
5. **S4** anello 9.

S3 senza S4 è già testabile: separa il problema "−1" da quello "sfratto". I trampolini CACHE_DIAG su 0x88e6f4/0x88f5b4/0x890374/0x890944
vanno tolti in S3, perché gli stessi indirizzi diventano Search16 (che logga).

**Rischi:**
1. **Margine zero a 16 [I].** 8 slot × 2 buffer = 16 maniglie. Durante una ricerca la propria è già rilasciata, quindi 15 bloccate +
   1 libera. Se qualcos'altro tiene un ref (default sys+0xf8, precarico), torna −1. La misura S0 ("massimo bloccati") lo dice.
   *Opzione da far scegliere all'utente:* N = 20 (stesso schema, 8 record in coda, +0x1aa0 B e altri ~97 MiB).
2. **Memoria:** 97 MiB in più sempre allocati; il genitore di BackgroundLoad è ignoto finché non si misura.
   *Opzione:* creare gli heap 12..15 in un altro genitore. È uno scostamento dal flusso, da proporre solo se la misura lo impone.
3. **Comportamento in 4P:** con N = 16 anche il 4P usa i record 12..15 quando 0..11 sono tutti caricati (più cache, stessa
   immagine). *Opzione:* N = 12 quando il menu non è a 8. Sconsigliata: i record 12..15 caricati resterebbero invisibili alle
   ricerche finché non si torna a 8.
4. **TireSub/0x71f0:** significato ignoto [I] (chiave a 1 o 3 campi; maniglie in ogni L). Se S0 mostra che non superano 12 bloccati,
   *opzione:* lasciarle a 12 e risparmiare 13,2 MiB e la parola 0x3d57b8.
5. **Coerenza del ctor:** CtorExt scrive oltre 0x9a50. Va eseguito **solo** se le due parole del `new` sono scritte (flag `sizeOk`
   impostato nell'installazione), altrimenti corrompe l'heap.
6. **Siti mancati:** un lettore sconosciuto dei record per indice vedrebbe il record 0 (csel) per gli indici 12..15. La ricerca del §1.3
   non ne ha trovati. I log di Search16 (indice restituito ≥ 12) e delle guardie esistenti lo segnalerebbero.
7. **Thread:** Search16 gira nel thread di caricamento (0x618610), UsedClr/UsedSet nel thread principale, come l'originale. Niente
   allocazioni né log pesanti nei callback (log con contatore, come cachediag).

**Misure di accettazione finali (8P, 8 cursori, almeno 5 minuti):**
- 0 ricerche −1/NULL (8 liste);
- 0 `guardia: 0x68fffc` e 0x11dc0c;
- sys+0x5050 mai ≠ 0 per più di 120 frame;
- anteprime corrette, nessuna moneta;
- dopo S4, 0 eventi STALE;
- massimi bloccati per lista e `MaxAlloc` nei log;
- 4P invariato (stessa prova di controllo della build di default).

## 6. Punti incerti
- Genitore di BackgroundLoad e suo libero (x2 = 0 → heap corrente a 0x881d20) [I], si misura in S0.
- Uso reale delle liste TireSub/0x71f0 da parte delle maniglie di L [I].
- Margine 16 contro i ref non di maniglia (precarico, default sys+0xf8) [I].
- Il −1 di Arm nel log 7b (sys+0x5050 = 1, "0/0") va riletto con gli offset giusti: diag con offset di Body.
- `0x60c6c4` (formattazione del nome) non è stata analizzata: per il nome basta `snprintf` nel buffer (cap 0x40); il nome serve solo
  all'heap.
- Lo pseudocodice di Part0x9 riusa l'analisi verificata di `step7b_cache.md` §2.2 (prologhi e punti chiave riverificati qui).

---

## 7. Esito S0 in Eden (4 nov 2026, log `step7b_data/eden_log_rec_s0.txt`, solo righe della mod)
Build `MK8DX_P7_MENU3D8=1` con le sole misure S0 (cachediag a 8 liste con campi corretti, 4 ricerche a puntatore, memoria a 0x881ce8,
0x3da428, 0x3d5794, 0x884bd8). Stress: selezione personaggi a 8 con 8 cursori (kart non provati).

**Record bloccati (ref > 0) per lista [D]:**
| lista | max bloccati | ricerche fallite |
|---|---|---|
| Driver, Emblem, TireSub, Body, Wing | **12/12** (f≈2894) | 1-2 ciascuna, subito dopo |
| Tire | 3 (solo personaggi cambiati) | 0 |
| L71f0 | 0 (mai usata nel menu) | 0 |
| Arm | 5, caricati max 8 | 2, **con 4 record liberi** |
- Al momento del -1 la somma dei refcount e' **15** (es. Driver `4 1 1 1 1 1 1 1 1 1 1 1`) = 16 maniglie (8 slot × 2) − quella appena
  rilasciata: con 16 record resta sempre almeno 1 record libero (N = 16 basta, con margine 1).
- Subito dopo: guardia 0x68fffc (m_Body) e `sys+0x5050 = 5 FERMO` per sempre (thread di caricamento morto): causa confermata.
- **Arm -1 non e' esaurimento:** nel ramo "record libero" (0x8909fc..0x890c64) -1 = acquire 0x3dd8c8 fallito (`tst w0,#1; csinv`), con i
  record 8..11 liberi e non caricati. Probabilmente combinazione senza modello Arm (normale anche nell'originale) [I]; non dipende dai 12.
- Effetto visto dall'utente: anteprime non corrispondenti ai nomi (Toadette sopra "Diddy Kong", monete sotto Mario/Ludwig) e, dopo il
  blocco del thread, finestra varianti di P7 senza riquadro e scelta bloccata (probabilmente attesa del modello [I]).

**Memoria [D]:**
| heap | libero misurato | serve | conclusione |
|---|---|---|---|
| genitore di BackgroundLoad = **TaskHeap d'avvio 0x15b00000** (0x211852aad0, creato a 0x603290 come quello del menu) | 0x14b2d190 prima, **0x102d170 (16 MiB) dopo BackgroundLoad** | +58 MiB | non basta: allargare anche il TaskHeap d'avvio |
| BackgroundLoad [sys+0x4ed0] dopo 0x3da428 | 0x88080 (0,5 MiB) | +56,5 MiB | 0x881d10 → 0x17500000 |
| DriverBodySE [sys+0x4ee0] | 0x4218 | +13,2 MiB | 0x3d57b8 → 0x350e000 |
| genitore di DriverBodySE/CourseResource (`sead::SystemManagers` 0xb04ffa78) | 0x9f8f0fc0 (2,5 GiB) dopo 0x3d5794 | +41,6 MiB | regge |
| genitore del TaskHeap d'avvio (0x2118211948) | 0x9c7d5530 (2,4 GiB) | +58 MiB | regge |
| [sys+0x320] CourseResource nel menu | 0x2383ca0 → 0x103b30 dopo l'anello | +27,6 MiB | 0x3d57f0 → 0x10004000 |
Eden con `memory_layout_mode=0` (4 GB, come Switch 1): i liberi in GiB sono quelli della console [I].

## 8. Esito S1-S3 in Eden (4 nov 2026)
Build `MK8DX_P7_MENU3D8=1` (REC16 = 1 di default con le anteprime; `MK8DX_P7_REC16=0` la spegne). Log: `step7b_data/eden_log_rec_s3_stress8.txt`.
- Avvio: 4 parole + 30 siti verificati, sys 0x9a50 -> 0xb4f0, record 12..15 costruiti in 8 liste, **32 heap creati, 0 falliti**
  (TaskHeap d'avvio 0x15b00000 -> 0x19500000 in HeapCreateHook, BackgroundLoad 0x17500000, DriverBodySE 0x350e000). Dopo: BackgroundLoad
  0x202e40 liberi, DriverBodySE 0x40dd8, TaskHeap d'avvio 0x1647170 (prima 0x102d170).
- Stress a 8 (personaggi e kart): **0 ricerche fallite** in Driver/Emblem/Tire/TireSub/L71f0/Body/Wing, **0 guardie m_Body**, sys+0x5050
  mai fermo (max 42 frame), Driver/Emblem/TireSub/Body/Wing fino a **16/16 bloccati** senza -1. Anteprime che seguono le scelte.
- Arm: -1 solo come acquire fallito con record liberi (vedi §7), invariato.
- Restano le **monete** nelle celle P6-P8 (non dipendono dai record): da analizzare a parte (anello S4? modello di ripiego).
- **4P bloccato dopo la scelta dei personaggi (non va alla selezione kart)**: NON dipende da REC16 (uguale con `MK8DX_P7_REC16=0`);
  assente con `MK8DX_P7_MENU3D8=0` e con le 5 parole del flusso a 8 spente (`MK8DX_P7_FLOW8=0`). Con il flusso a 8 il menu in 4P ha
  settings+0x180 = 8, KartDirector n = 8 e entry 4..7 valide (tipo 0, personaggi casuali); senza, 4/4 e descrittori 4..7 tipo 4.
  Log: `eden_log_flow8_4p_blocco.txt` / `eden_log_flow0_4p_ok.txt`. Regressione di ada67de (allora 4P verificato solo sulle anteprime).

## 9. Esito S4 in Eden (4 nov 2026)
Anello Body/Wing a 9 voci (`MK8DX_P7_RING9`, default 1 con REC16): voci 5..8 a sys+0xb030 / 0xb290 (CtorExt), create e smontate come le
originali (RingFill/RingDown), parte 0 reimplementata a N = 9 (0x89d668 / 0x89dae4), CourseResource 0x10004000 (0x3d57f0 `52a20000`).
- **Correzione al piano:** la parte 0 di Wing NON e' identica a quella di Body: percorso `x2+0x138` (0x89dc58 `9104e054`, Body 0x89d7dc
  `add x20,x2,#0x88`) e caricamento della voce **0x89bc70** (0x89dd00/0x89dd34/0x89dd70/0x89dda4/0x89ddd8/0x89ded0; Body 0x89b290).
  Con i valori di Body Wing finiva sempre nel ripiego vt[0] (log: 0/0/46/46).
- Avvio: 4+4 voci create, 0 fallite; CourseResource libero dopo l'anello 0x10b6e88 (Body) / 0x16a070 (dopo Wing).
- Stress a 8 (kart, alianti): Body riuso/libera/sfratto/ripiego 4/5/0/0, Wing 7/2/0/0 (dopo la correzione), 0 ricerche -1 (Arm a parte),
  0 guardie, 0 crash. Le **monete in P6-P8 restano**: non dipendono da liste ne' anello (analisi in step7b_coin.md).
