# Step 6 – piano A: crash "PC=0" in gara con 8 locali (gestore M, array dei locali a 4)

Main 4.0.1 (BID 158C0C73...), offset = `../img/main.bin`. Convenzione: **[VERIFICATO]** = letto nel disassemblato
(indirizzo citato) o nel log del debugger; **[IPOTESI]**; **[DA VERIFICARE A RUNTIME]**.
Estratti commentati in `step6_data/`:

| file | contenuto |
|---|---|
| `a_m_class.asm` | new M (0x9c190), ctor 0xa8c00, inizializzazione +0xa0/+0xe0, vt+0xa8, vt+0xc0 |
| `a_reg_ac290.asm` | registrazione del kart 0xac290 (scrittura senza limite in +0xa0, clamp di +0xe0) |
| `a_loops_locals.asm` | tutti i lettori di +0xa0[] e +0xe0[] (0xaa16c, 0xaa888, 0xab4a8, 0xb1f54), pan audio 0x7586c |
| `a_clobber_c0_d8.asm` | scrittori di +0xc0/+0xc8/+0xd0/+0xd8 (puntatori scalari subito dopo l'array) |
| `a_f0_views.asm` | +0xf0 vista→kart, 0xaa068, 0x84278, 0x76bf0 (4 viewport srotolati) |
| `a_descr.txt` | descrittore d = K+0x258: vtable, metodi vt[0]/vt[0x58]/vt[0x98]/vt[0xa0] |
| `a_patches.txt` | 15 patch dell'opzione A: parola originale (letta da main.bin) → nuova (verificata con llvm-objdump) |

## 0. Riassunto

Il crash si spiega interamente così [VERIFICATO]: `M+0xa0` è un array **K\*[4]** riempito senza limite da
0xac364 (`str x19,[x9,x8,lsl #3]`, x8 = M+0x104). Con 8 locali i locali 4..7 finiscono in +0xc0, +0xc8, +0xd0,
+0xd8, che sono **altri membri** (puntatori K scalari) riscritti ogni frame; +0xd8 viene azzerato a 0xaa3dc
(in 0xaa38c, chiamata da 0xaa16c *prima* del ciclo). Il ciclo 0xaa1ac-0xaa1f4 (`i < M+0x104` = 8) legge
K = [M+0xd8] = 0 per i = 7: Eden restituisce 0 sulle letture non mappate (gdb: "Cannot access memory at 0x250",
x20 = 0, x8 = 0, x21 = 7), quindi `d = [0+0x258] = 0`, `vt = 0`, `blr [0+0xa0] = 0` → PC = 0. Su hardware sarebbe
un data abort a 0xaa1b8. Lo stesso array è letto, sempre con `i < +0x104`, anche da 0xaa888, 0xab4a8 (M vt+0xd0)
e 0xb1f54 (kart CPU): tutti avrebbero dato lo stesso problema subito dopo.

Correzione raccomandata: **opzione A** (rilocazione di +0xa0 e +0xe0 in coda a M ingrandendo l'allocazione a
0x3080): 15 patch di una parola + 2 trampolini piccoli (ctor, registrazione). L'array +0xf0 (vista→kart) **non**
va toccato ora: è limitato a monte da 0x76bf0, che conosce solo 4 viewport (§1.4).

## 1. Classi M e K

### 1.1 Chi crea chi [VERIFICATO]
- **G** = oggetto "gara" con lista di sottosistemi (vtable 0x11b6768, GOT 0x12fb760; vptr 0x11b6778).
  0x9c044 (G vt+0xa8, riferisce `BGM_COURSE_IN_Cnsw18`) crea i sottosistemi con `0x5faae0(size, 8)` e li mette in
  G+0x58[G+0x64]: G+0x848 (0x70 B, ctor 0xa8584), **G+0x850 = M** (0x2978 B, ctor 0xa8c00, a 0x9c190-0x9c1e8),
  G+0x858 (0x550, 0xb80ac), G+0x860 (0x550, 0xb7504), G+0x868 (0x70, 0x95d34), G+0x870 (0x590, 0x8db94),
  G+0x878 (0x1080, 0x7eaa4), G+0x880. Ogni sottosistema ha +0x28 = G.
- **M**: vtable 0x11b7610 (GOT 0x12fb970), vptr 0x11b7620 = main+0x11b7620 (= 0x8129e620 nel dump con base
  0x800e7000). **Unica** allocazione: 0x9c190 `mov w0,#0x2978` (52852f00) + 0x9c19c `bl 0x5faae0` + 0x9c1a4
  `bl 0xa8c00` (unico chiamante del ctor). sizeof(M) = **0x2978**.
- **K**: vtable 0x11b7748 (GOT 0x12fb980), vptr 0x11b7758. Ctor 0xabb8c(K, kart) chiamato solo da 0x16cef8 in
  0x16ce98, con `0x5faae0(0x1700, 8)` (sizeof(K) = 0x1700); 0x16ce98 è chiamata da 0x16cc04 in 0x16cacc con
  x0 = unità+0x40 (indice kart di KartUnit, STRUCTS) e il risultato va in unità+0x28. K+0x28 = M (0x16cf00),
  K viene inserito nella lista M+0x50 (0x16cf10-0x16cf4c, M+0x60++). Quindi **un K per kart (12)**, componente di
  KartUnit.
- Significato [IPOTESI forte]: M = gestore per-gara dei kart lato "presentazione" (LOD/aggiornamento: divide i K in
  due liste +0x70/+0x88 con budget min(+0x110, +0x60) contro +0x10c viste in 0xaa38c; calcola le distanze dai
  locali in 0xaa888/0xb1f54 e imposta timer K+0x208 = 0x78). K = controller per kart (K+0x260 oggetto 0x2438 B
  ctor 0xbcb5c, K+0x50 oggetto 0x128 B 0xa37c4).

### 1.2 Catena del crash [VERIFICATO]
`0x6263c0 → 0x621b34 → 0x9abb0` (base "Director", vt+0x118 a 0x9ac58) `→ 0x9e148` (G vt+0x118; ciclo
0x9e270 sui sottosistemi G+0x58 con 0x7a9140 = vt+0x30) `→ 0xa8720` (**M vt+0x30 = M::calc**; 0xa874c è a
metà funzione) `→ vt+0xc8 = 0xaa16c` → `blr` a 0xaa1c8.
M::calc 0xa8720: se !M+0x6c → vt+0xc8 (0xaa16c), poi per ogni K della lista +0x50 `vt+0x30`, poi `vt+0xd0`
(0xab4a8). 0xaa16c chiama in ordine 0xaa208 (viste, +0xf0), 0xaa38c (liste LOD, azzera +0xd8), 0xaa888
(locali vs altri), 0xaae94 (+0xd0), poi il ciclo del crash (conta in +0x108 i locali con `!d->vt[0xa0]` e
0x1422ac(d)).

### 1.3 Chi chiama 0x9d4a0 e 0xac290 – correzione al brief
- **0x9d4a0 non è di M**: è nella vtable di G (slot 0x11b6830 = G vt+0xb8). Scrive **G+0x104** = settingsA+0x180
  (0x9d4bc-0x9d4c0), G+0x115, G+0x114, chiama G+0x878 (0x94be0, 0x95318) e libera G+0x848/+0x860/+0x870 se non
  servono [VERIFICATO]; momento: inizio gara [IPOTESI]. M+0x104 invece è solo il contatore dei locali registrati:
  scritto da 0xac3a4 (++), azzerato da 0xa8d44 (M vt+0xa8) [VERIFICATO]. Nel dump M+0x104 = 8 → 8 registrazioni.
- **0xac290 = K vt+0x20** (slot 0x11b7778). Il chiamante diretto non è stato individuato (dispatch virtuale da
  framework); è eseguita una volta per K all'init degli attori [IPOTESI], confermato indirettamente dal dump
  (8 locali registrati su 12 K, K7 con +0x1640 = 1).

### 1.4 Membri di M dimensionati sui locali / viste

| offset | tipo | contenuto | scrittori | lettori | estendibile in loco? |
|---|---|---|---|---|---|
| +0xa0 | K\*[4] | K dei locali in ordine di registrazione | ctor 0xa8d14/0xa8d24 (=0); 0xac360/0xac364 (indice = +0x104, **senza limite**) | 0xac36c; 0xaa1b0/0xaa1b4 (ciclo crash); 0xaaa20/0xaaa24 (x25) + 0xaaa2c/0xaaa3c/0xaaa88/0xaab0c/0xaabb0; 0xaad58/0xaad7c/0xaae30 ([0], solo se +0x104==1); 0xab4d0/0xab4f0.. (vt+0xd0); 0xb2070/0xb2074 (K 0xb1f54, `mov w14,#0x14` = 0xa0/8) | **no**: +0xc0..+0xd8 sono 4 puntatori K diversi |
| +0xc0 | K\* | locale "migliore" (+0x1644 minimo) | 0xac39c (se count==0), 0xaaa30 | 0xaaa10, 0xa09fc (G) | – |
| +0xc8 | K\* | ultimo K con +0x1644 == 0 | 0xa8ed8, 0xaac58, 0xaacb8 | – | – |
| +0xd0 | K\* | scelto in 0xaae94 | 0xab050 | 0xaa0c8 (0xaa0bc, chiamato da 0x84140/0x84150) | – |
| +0xd8 | K\* | scelto per distanza | 0xaa3dc (=0 ogni frame), 0xab230, 0xab39c, 0xab464 | 0xab118, 0xab1a4, 0xab2fc, 0xab388 | – |
| +0xe0 | s32[4] | locale i → id kart (K+0x1624) | ctor 0xa8d18/0xa8d30 (=-1); 0xac374-0xac380 (`cmp w8,#4; csel` → i≥4 scrive [0]) | 0xaa9e4-0xaa9f0 (`cmp x26,#4`), 0xaad9c ([0]) | no (+0xf0 subito dopo) |
| +0xf0 | s32[4] | vista → id kart | 0xaa2c8 (`stp x8,x8` = -1 ×4 ogni frame), 0xaa2fc ([0]), 0xaa350-0xaa35c (`cmp w0,#4`) | 0xaa06c-0xaa078 (0xaa068, `cmp w1,#4`) | no (+0x100 n kart, +0x104 n locali) |
| +0x100 | s32 | n K in lista (copia di +0x60) | 0xa8ef0 | 0x76ecc | – |
| +0x104 | s32 | n locali registrati | 0xac3a4, 0xa8d44 | 0xaa19c, 0xaa1e8, 0xaa9b0, 0xaabf8, 0xaace0, 0xab4b0, 0xac350, 0xac35c, 0xac368, 0xac394, 0xb1898, 0xb2054, 0xb48c4, 0xb4bec | – |
| +0x108 / +0x10c | s32 | n locali attivi (0xaa16c) / n viste con kart (0xaa208) | 0xaa1a4.., 0xaa2d0.. | 0xaa3c4, 0x8427c (`1..4`, `cmp w8,#3`) | – |

Note [VERIFICATO]:
- Non esistono accessi a indice fisso 1..3 (+0xa8/+0xb0/+0xb8) su M: le occorrenze in 0xa7cec/0xa7d2c/0xa7d60 sono
  dispatch di vtable (x8 = vtable).
- Ricerche fatte: tutte le `ldr xN,[xM,#0x850]` del binario (105, solo 5 toccano campi di M: 0x76ecc, 0x8427c,
  0xa09fc, 0xac350, 0xac35c), le `ldr [K+0x28]` in 0x60000-0x200000 (lettori di +0x104: 0xb1898, 0xb2054, 0xb48c4,
  0xb4bec), i chiamanti esterni dei metodi di M (0xa9ff8 ← 0x7733c/0x773bc/0x8d174/0xa09b8; 0xaa034 ← 0x76ee8/
  0x966f8; 0xaa068 ← 0x84308; 0xaa0bc ← 0x84140/0x84150), l'idioma `add xN,xM,#0xa0` + `[xN,x,lsl #3]` su tutto il
  binario (solo 0xaa1b0, 0xac360) e `mov wN,#0x14` + `[x,xN,lsl #3]` (solo 0xb2070), e `cmp #4` seguito da
  `#0xe0]/#0xf0]` (solo i siti in tabella; 0x76f20/0x926e0/0x94168 sono altre classi). I campi +0xe0/+0x100/+0x108
  letti via `[x+0x28]` in 0x83c84/0x841b4/0x841cc/0xaa8c8/0xa8d74 sono di **G**, non di M.
- K+0x162c = indice locale (0xac354), letto solo da 0xb48d0/0xb4bf8 per 0x7586c (pan audio: idx > 3 o n fuori
  2..4 → 0.0, innocuo).
- +0xf0: il valore scritto è l'indice di vista restituito da 0x76bf0(obj, kart), che confronta il kart solo con i
  viewport 0..3 della Scene (Scene+0x40/+0x4c, +0x58/+0x64, +0x70/+0x7c, +0x88/+0x94; altrimenti -1). Quindi
  w0 ≤ 3 e la scrittura a 0xaa35c non esce mai dall'array finché le viste 4..7 non vengono date alla logica di gioco.
  Nel dump +0xf0 = {0,-1,-1,-1}, +0x10c = 1: ramo "una vista" (0x878e40 vero, 0xc8f48 → kart 0) [VERIFICATO log].

## 2. Correzione per 8 locali

### 2.1 Opzione A – rilocazione in coda a M (RACCOMANDATA)
Nuovo layout: **M+0x3000 = K\*[8]** (ex +0xa0), **M+0x3040 = s32[8]** (ex +0xe0), sizeof(M) = **0x3080**.
0x3000 è scelto perché `add xD,xN,#0x3,lsl #12` è codificabile in una parola (0x2978 non lo è); `ldr x,[#0x3000]`
(imm 0x600×8) e `ldrsw/str w,[#0x3040]` (imm 0xc10×4) pure. Tabella completa in `step6_data/a_patches.txt`:

| # | offset | orig | nuova | istruzione nuova |
|---|---|---|---|---|
| SZ | 0x9c190 | 52852f00 | 52861000 | `mov w0,#0x3080` (new M) |
| A1 | 0xac360 | 91028289 | 91400e89 | `add x9,x20,#0x3000` (registrazione) |
| A2 | 0xaa1b0 | 91028276 | 91400e76 | `add x22,x19,#0x3000` (ciclo del crash) |
| A3 | 0xaaa24 | 91028139 | 91400d39 | `add x25,x9,#0x3000` (0xaa888) |
| A4 | 0xab4d0 | 9102800e | 91400c0e | `add x14,x0,#0x3000` (vt+0xd0) |
| A5-7 | 0xaad58, 0xaad7c, 0xaae30 | f94052c8 | f95802c8 | `ldr x8,[x22,#0x3000]` |
| A8 | 0xb2070 | 5280028e | 5280c00e | `mov w14,#0x600` (K 0xb1f54) |
| A9 | 0xb2094 | d1004dcf | d117fdcf | `sub x15,x14,#0x5ff` (stesso ciclo, i+1) |
| E1 | 0xac374 | 7100111f | 7100211f | `cmp w8,#8` |
| E2 | 0xac380 | b900e109 | b9304109 | `str w9,[x8,#0x3040]` |
| E3 | 0xaa9e4 | f100135f | f100235f | `cmp x26,#8` |
| E4 | 0xaa9f0 | b980e108 | b9b04108 | `ldrsw x8,[x8,#0x3040]` |
| E5 | 0xaad9c | b980e2ca | b9b042ca | `ldrsw x10,[x22,#0x3040]` |

Più due trampolini (lo heap di 0x5faae0 non è garantito azzerato; il ctor inizializza solo i vecchi offset):

```cpp
namespace race8 {                                  /* gestore kart M (G+0x850), step6_plan_A */
    constexpr uintptr_t kA0 = 0x3000, kE0 = 0x3040; /* K*[8], s32[8]; sizeof(M) 0x2978 -> 0x3080 (patch SZ) */

    HOOK_DEFINE_TRAMPOLINE(KartMgrCtorHook) {      /* 0xa8c00 M::M(M*) */
        static void Callback(uint8_t* m) {
            Orig(m);
            memset(m + kA0, 0, 8 * sizeof(void*));
            memset(m + kE0, 0xff, 8 * sizeof(int32_t));   /* -1 come il ctor a 0xa8d18 */
            Logging.Log("[mk8dx-8p] R0: M=%p creato, size 0x3080", m);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(KartRegisterHook) {     /* 0xac290 = K vt+0x20 (K*) */
        static void Callback(uint8_t* k) {
            Orig(k);
            if (!k[0x1640]) return;                                  /* non locale: niente registrazione */
            uint8_t* g = *(uint8_t**)(k + 0x60);   if (!g) return;   /* come 0xac348 */
            uint8_t* m = *(uint8_t**)(g + 0x850);  if (!m) return;
            int n = *(int32_t*)(m + 0x104), i = n - 1;
            /* specchio nel vecchio array per i lettori eventualmente sfuggiti (semantica vanilla, max 4) */
            if (i >= 0 && i < 4) {
                ((uint8_t**)(m + 0xa0))[i] = ((uint8_t**)(m + kA0))[i];
                ((int32_t*)(m + 0xe0))[i]  = ((int32_t*)(m + kE0))[i];
            }
            Logging.Log("[mk8dx-8p] R1: M=%p locale %d/%d K=%p kart=%d id=%d", m, i, n, k,
                        *(int32_t*)(k + 0x1620), *(int32_t*)(k + 0x1624));
            if (n > 8) Logging.Log("[mk8dx-8p] R1: ERRORE %d locali > 8 (array +0x3000 sfora)", n);
        }
    };

    static void Install() {
        struct P { uintptr_t off; uint32_t orig, repl; const char* what; };
        static const P kM[] = {
            {0x9c190, 0x52852f00, 0x52861000, "R SZ new M 0x2978 -> 0x3080"},
            {0xac360, 0x91028289, 0x91400e89, "R A1 reg +0xa0 -> +0x3000"},
            {0xaa1b0, 0x91028276, 0x91400e76, "R A2 0xaa16c"},   {0xaaa24, 0x91028139, 0x91400d39, "R A3 0xaa888"},
            {0xab4d0, 0x9102800e, 0x91400c0e, "R A4 vt+0xd0"},   {0xaad58, 0xf94052c8, 0xf95802c8, "R A5"},
            {0xaad7c, 0xf94052c8, 0xf95802c8, "R A6"},           {0xaae30, 0xf94052c8, 0xf95802c8, "R A7"},
            {0xb2070, 0x5280028e, 0x5280c00e, "R A8 0xb1f54 base"}, {0xb2094, 0xd1004dcf, 0xd117fdcf, "R A9"},
            {0xac374, 0x7100111f, 0x7100211f, "R E1 cmp #8"},    {0xac380, 0xb900e109, 0xb9304109, "R E2 +0x3040"},
            {0xaa9e4, 0xf100135f, 0xf100235f, "R E3 cmp #8"},    {0xaa9f0, 0xb980e108, 0xb9b04108, "R E4"},
            {0xaad9c, 0xb980e2ca, 0xb9b042ca, "R E5"},
        };
        exl::patch::RandomAccessPatcher rp;                 /* input::Patch scrive subito: prima verifica tutto */
        for (const P& p : kM)
            if (rp.Read<uint32_t>(p.off) != p.orig) { Logging.Log("[mk8dx-8p] R: %s non corrisponde, M non rilocato", p.what); return; }
        for (const P& p : kM) input::Patch(p.off, p.orig, p.repl, p.what);
        KartMgrCtorHook::InstallAtOffset(0xa8c00);
        KartRegisterHook::InstallAtOffset(0xac290);
    }
}
```
Nota: `input::Patch` (main.cpp:540) scrive subito la parola; per questo l'Install verifica prima tutte le parole
originali e poi scrive (tutto o niente): una rilocazione parziale è peggio di nessuna.
Le patch vanno installate all'avvio (prima che G crei M), come le altre del modulo.

### 2.2 Opzione B – tabella ombra con hook (sconsigliata)
Servirebbero: inline a 0xac360 (x9 → tabella ombra per count ≥ 4) e inline a 0xac374/0xac380; REPLACE di 0xaa16c
(piccola, fattibile) e di 0xab4a8 (piccola); ma anche la riscrittura di **0xaa888** (0x60c B, con ramo 1 locale,
distanze, timer +0x208, +0xc0) e del ciclo in **0xb1f54** (metodo K, distanza minima dai locali), più gestire la
corruzione di +0xc0..+0xd8 che resterebbe se 0xac364 non fosse deviata. Più codice C++ che replica logica di gioco,
più punti di divergenza. L'opzione A lascia il codice originale intatto (solo base e limiti spostati).

### 2.3 Confronto
| | A (rilocazione) | B (ombra) |
|---|---|---|
| siti | 15 parole + 2 trampolini banali | 3 inline + 2 REPLACE + 2 riscritture complesse |
| codice di gioco reimplementato | nessuno | 0xaa888, 0xb1f54, 0xaa16c, 0xab4a8 |
| 1-4P | identico (clamp 4→8 non cambia i<4; specchio nel vecchio array) | identico se le riscritture sono esatte |
| rischio residuo | lettore sfuggito del vecchio array (coperto dallo specchio fino a 4) | errori di riscrittura |

## 3. Descrittore d = K+0x258 [VERIFICATO]
- Origine (0xac2a4-0xac2f8): sys4 = `0x7e7434()+0x190+4*8` (sottosistema 4, lo stesso con +0x218 = G usato a
  0xa8e90/0xaa910); O = [sys4+0x238]; **d = O+0x50[kart]** (kart = K+0x1620, clamp 12 a 0xac2e8). Non è "tabella+0x50
  di ciascuno": è l'array di 12 puntatori a O+0x50.
- d: vtable 0x11ba508 (GOT 0x12fc3f0), ctor 0x141a5c(d, kart): d+0x48 = `0x13d6f8(O, kart)+0x8` =
  [[O+0xc8]+kart*8]+0x8 (oggetto info del kart). Metodi: **vt[0] 0x141e3c** = [d+0x48]+0xa8 (id kart → K+0x1624);
  **vt[0x58] 0x141ef0** = u8 [d+0x48]+0xd0 (locale umano → K+0x1640); **vt[0xa0] 0x141f80** = bit0 di
  [d+0x48]+0x1cf (flag di stato, [IPOTESI] arrivato/fuori gara); vt[0x98] 0x141f70 = bit6 di +0x1cd; 0x1422ac =
  bit1 di +0x1cd.
- Esistenza per i locali 4..7: dal dump il d di K7 (0x2165f5c280) ha vptr 0x812a1518 = main+0x11ba518 =
  vtable 0x11ba508+0x10 → valido; K11 (CPU) idem. Tutti i K sono stati registrati con il proprio d prima del crash
  [VERIFICATO log]. Nel percorso di registrazione non ci sono altri array a 4: 0x8e414([G+0x878], K) è un `ret`
  (funzione vuota) [VERIFICATO]; 0xbcb5c (oggetto 0x2438 per K) e 0xa37c4/0xa380c non dipendono dal conteggio locale
  [IPOTESI, non letti a fondo].

## 4. Dopo il crash: altri limiti a 4 sul percorso della gara
Con l'opzione A il resto di M è a posto: 0xaa1ac-0xaa1f4 (conteggio +0x108), 0xaa888 (8 locali vs CPU, +0xc0),
0xab4a8 (OR dei flag K+0x164b/0x164c/0x164f/0x1650 → M+0x121..+0x124), 0xb1f54 (CPU: distanza minima dai locali)
iterano tutti su +0x3000[0..7]. Limiti successivi (in ordine di probabilità di comparsa):

1. **Viste 4..7 sconosciute alla logica di gioco** [VERIFICATO statico]: 0x76bf0 conosce solo i viewport 0..3 della
   Scene → per i kart dei locali 4..7 K+0x1628 = -1, M+0x10c ≤ 4, liste LOD (0xaa38c) calcolate come in 4P. Effetto
   atteso cosmetico (dettaglio/aggiornamento dei kart visti solo dalle viste extra). Da affrontare con le viste/HUD:
   allora vanno rilocati anche +0xf0 (0xaa2c8 reset ×8 con hook pre-0xaa208, 0xaa2fc, 0xaa350/0xaa35c, 0xaa06c/
   0xaa078) e 0x84278 (`cmp w8,#3` su 1..+0x10c, solo con G+0x108 == 0x27).
2. **0x76dfc** (oggetto G+0x888, da 0x9e260): blocchi per vista 4 × 0x140 con `cmp x8,#4; csel` a 0x76f20, ciclo
   `i < obj+0x6c` [VERIFICATO]. Se obj+0x6c resta 4 nessun problema; se > 4, i blocchi oltre il 4° sovrascrivono il 0
   [DA VERIFICARE A RUNTIME: valore di [G+0x888]+0x6c].
3. **0x551b28** (pagina di gara): +0x10 + i*8 con `cmp x21,#4; csel` (0x551bc4/0x551bec/0x551c0c), count +0x30 =
   raceMgr+0x178 (= 8) [VERIFICATO]: i locali 4..7 sovrascrivono la voce 0 (niente overflow: +0x30 viene subito dopo).
4. **0x49c938** (pronto/applet in gara): controlla solo i pad 0..3 srotolati (0x49c9a4-0x49ca14) e poi
   `0x49ea60(n, n)` con n = +0x1628 [VERIFICATO]: niente crash, i locali 4..7 non vengono controllati.
5. Audio per giocatore 0x13e59c (tabella 0x131f32c, +0xc0 oggetti ≤ 4) e pan 0x7586c (innocuo, §1.4)
   [0x7586c VERIFICATO; 0x13e59c letto solo l'inizio, limite dal catalogo step 4].
6. Dal catalogo step 1/5, **non riletti qui**: pagine HUD fisse RaceView0..3 (enum 103-106) e RaceViewFront0..3
   (108-111); raceMgr +0x214/+0x224 s32[4] (getter con clamp); lettori srotolati di raceMgr+0x17c (§1.3 di
   step5_plan_B). Le pagine HUD per locale sono il candidato principale per il prossimo crash (un lettore che cerca
   la pagina 103+i per i ≥ 4) [IPOTESI].

Lista ordinata degli interventi per far partire una gara con 8 umani: (1) opzione A su M; (2) rilancio con GDB stub
agganciato per trovare il prossimo arresto; (3) a seconda di dove si ferma: HUD RaceView (rimandato), 0x76dfc se
obj+0x6c > 4, 0x551b28 (rilocazione dell'array +0x10 a 8, stessa tecnica); (4) solo dopo, +0xf0/0x76bf0 insieme
alle viste extra (step 2) passate alla logica di gioco.

## 5. Sintesi

Modifiche (ordine di installazione, all'avvio del modulo):
1. 15 patch `input::Patch` della tabella §2.1 (SZ, A1-A9, E1-E5), tutto o niente.
2. `HOOK_DEFINE_TRAMPOLINE` su **0xa8c00** `void(M*)`: dopo Orig azzera M+0x3000[8], M+0x3040[8] = -1.
3. `HOOK_DEFINE_TRAMPOLINE` su **0xac290** `void(K*)`: dopo Orig, se K+0x1640, specchia new[i] → vecchio
   +0xa0[i]/+0xe0[i] per i < 4 e logga (M, i, n, K, kart, id).
4. Nessuna modifica a +0xf0 per ora (limite a monte in 0x76bf0).

Rischi 1-4P: con ≤ 4 locali il comportamento è identico (stessi indici, clamp 4→8 inattivi, vecchio array tenuto
uguale dallo specchio). Rischi residui: (a) lettore del vecchio array non trovato staticamente → vede gli stessi
valori vanilla fino a 4 locali, ma per i locali 4..7 nulla; (b) +0x708 byte nello heap di gara [DA VERIFICARE: nessun
errore di allocazione]; (c) altri oggetti con dimensione di M copiata/assunta – nessuna trovata (un solo `new`).

Verifiche a runtime:
- Log `R0` (una volta per gara) e `R1` (8 righe, i = 0..7, kart/id coerenti con raceMgr+0x17c) nel log di Eden.
- GDB: break dopo l'init (o un trampolino di log su 0xaa16c ogni 600 frame) e dump di M+0x3000..+0x303f (8 K
  validi, vptr main+0x11b7758), M+0xc0..+0xd8 (puntatori K o 0 su +0xd8, non più i locali), M+0x104 = 8,
  M+0x3040[0..7] = id dei locali (nel dump attuale +0xe0[0] = 7 era sbagliato).
- GDB `rwatch` sui 4 qword del vecchio M+0xa0 durante una gara 8P: ogni hit = lettore sfuggito (indirizzo da
  aggiungere alla tabella). Ricordare `set breakpoint always-inserted on` (bug dello stub, CLAUDE.md).
- Gara 8P fino al traguardo; se si ferma, catena con `fpchain` come in gdb_crash1_race8.txt.
- Regressione: gara 1P, 2P, 4P completa con risultati (con la mappa slot +0x19c dello step 5 sempre attiva).
