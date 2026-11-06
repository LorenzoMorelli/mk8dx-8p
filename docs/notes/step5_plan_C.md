# Step 5 – sotto-obiettivo C: pad UI 4..7 veri, pad "speciale" spostato a 8 (piano statico)

Main 4.0.1 (BID 158C0C73…), offset nel main decompresso `../img/main.bin`. Solo analisi statica:
[VERIFICATO] = letto nel disassemblato in questa sessione (indirizzi citati), [IPOTESI] = dedotto,
[DA VERIFICARE A RUNTIME] = da confermare in Eden. Dati grezzi in `step5_data (dati non pubblicati)/`:
`c_padui_family.asm` (famiglia completa 0x8bde24-0x8bf188 con firme in testa), `c_fill_and_calc.asm`
(creazione/calc/allocatore), `c_8bf158_callers.txt` (185 chiamate a GetPad: argomento e uso),
`c_cmp3_sites.txt` (12 siti), `c_index4_sites.txt` (tutti gli usi dell'indice 4 trovati).

## 0. Sintesi (le correzioni importanti rispetto alle ipotesi di partenza)

1. **Il pad UI 4 non è un "any-controller": è il pad del solo TOCCO** [VERIFICATO]. Alla creazione ha pulsanti
   (+0x1c) e tocco (+0x1d) spenti (0x3ffb94-0x3ffbac); i 4 siti letterali `0x8bf158(4)` chiamano solo
   `0x8be478` = **SetTouchOn** (non "evidenzia"). Il suo idx 4 fa fallire tutti i `cmp #3`, quindi non ha mai
   input da GamePlayer. È il "proprietario" di default dei controlli menu (campo +0x118 = 4 nel ctor base
   0x8ba39c) e il bit 4 delle maschere di pagina +0x1a8.
2. **La fusione degli input (flag +0x2c5) non è del pad 4**: il flag viene acceso sul **pad 0** dalla pagina di
   gara (0x49d020-0x49d034, ripristinato a 0x49e764-0x49e770). Un pad con +0x2c5 fonde gli input dei
   GamePlayer di `ptr[0..3]` del contenitore (0x8beb70-0x8bec08) leggendo direttamente i GamePlayer, non i
   campi degli altri pad ⇒ l'ordine dei calc non conta.
3. Ogni pad UI legge l'input da **GamePlayer+0x3d8** (oggetto controller del GamePlayer, layout sead:
   +0x8 trig, +0x10 repeat, +0x114 hold) tramite idx → `0x86ebf8` → raceMgr+0x58[e] → +0x200. **0x86ebf8 ha
   esattamente 12 chiamanti in tutto il main, tutti nella famiglia pad UI** (grep `bl 0x86ebf8` = 12).
4. Proposta: hook REPLACE su 0x8bf158 **condizionato a N>4** (slot 4..7 = pad extra solo in modalità 5-8;
   indice 8 = pad tocco sempre; in 1-4P risposte identiche all'originale), trampolino su 0x8bf040 (idx del
   pad tocco 4 → 8 e creazione dei 4 extra in memoria statica del modulo), trampolino su 0x8bf058 (calc degli
   extra + estensione della fusione), 12 patch `cmp #3 → #7`, 4 patch `#4 → #8`.

## 1. Oggetto pad UI (0x2c8 B) e famiglia di funzioni

### 1.1 Layout [VERIFICATO salvo indicazione]
| off | tipo | significato | prova |
|---|---|---|---|
| +0x00 | ptr | vtable `[GOT 0x1310a40]+0x10` (= 0x12d45c0+0x10) | ctor 0x8bde34-0x8bde5c |
| +0x08 | s32 | **idx** = indice giocatore locale (scritto solo da 0x8bf040: `str w1,[x2,#8]`) | 0x8bf04c |
| +0x0c | u32 | pulsanti **hold** (da ctrl+0x114) | 0x8bec30-0x8bec34 |
| +0x10 | u32 | pulsanti **trig** (da ctrl+0x8) | 0x8bec38-0x8bec3c |
| +0x14 | u32 | **trig\|repeat** (ctrl+0x8 \| ctrl+0x10) | 0x8bec40-0x8bec4c |
| +0x18 | u32 | pulsanti **iniettati** (OR in +0xc/+0x10/+0x14, poi azzerato: `stp w8,wzr,[x19,#0x14]`); scritto da pagine (0x4329ec, 0x432a34, 0x434d30, 0x434d78, 0x47a410, 0x4bbd6c) | 0x8bec50-0x8bec64 |
| +0x1c | u8 | **pulsanti attivi** (setter 0x8be458) | 0x8be698, 0x8beb60 |
| +0x1d | u8 | **tocco attivo** (setter 0x8be478) | 0x8be6a0, 0x8be7d8 |
| +0x1e/+0x1f | u8 | tocco: valido / bit15 di [touch+8] | 0x8be80c, 0x8be8bc |
| +0x20..+0x27 | float2 | posizione tocco (filtrata se +0x1e) | 0x8be85c-0x8be8b8 |
| +0x28..+0x117 | 2 × 10 × float3 | storico tocco A (+0x28, +0xa0) | ctor 0x8be000-0x8be028 |
| +0x118 | u8 | tocco B valido | 0x8be8d8 |
| +0x11c..+0x213 | float2 + 2 × 10 × float3 | tocco B | ctor |
| +0x214 | u8[10] | zone tocco attive (helper 0x8bec98, scritte da 0x422d38) | 0x8bece8, 0x422e04 |
| +0x220 | 10 × 16 B | rect zone (init FLT_MAX/-FLT_MAX poi da [GOT 0x13082d8]) | ctor, 0x8bed04 |
| +0x2c0 | s32 | ultimo tipo giocatore (gp+0x73c): se cambia → reset stato tocco | 0x8be708-0x8be718 |
| +0x2c4 | u8 | "deve confermare" (letto in cicli srotolati 0..3: 0x43d2a8, 0x4523d4, 0x453324, 0x4588b4) | c_index4_sites §I |
| +0x2c5 | u8 | **fusione input dei pad 0..3** (acceso sul pad 0 da 0x49d034) | 0x8beb68 |

Valori iniziali (ctor 0x8bde24, unico argomento x0): +0x8..+0x17 = 0, +0x18 = 0, **+0x1c = +0x1d = 1**
(`mov w8,#0x101; stp wzr,w8,[x20,#0x18]`), +0x2c0 = 0, +0x2c4/+0x2c5 = 0 (`strh wzr,[x20,#0x2c4]`).

### 1.2 Famiglia (firme dedotte; tutte [VERIFICATO] nel corpo, nomi [IPOTESI])
| funzione | firma | comportamento |
|---|---|---|
| 0x8be03c | `s32 EntryIndex(pad)` | idx≤3 ? 0x86ebf8(idx) : -1 |
| 0x8be068 | `Entry* GetEntry(pad)` | raceMgr+0x58[e] (e < raceMgr+0x50) o NULL |
| 0x8be0bc | `Ctrl* GetCtrl(pad)` | gp = entry+0x200; gp ? gp+0x3d8 : NULL |
| 0x8be124 | `void GetPlayerType(pad, x8=out)` | *out = gp+0x73c (0 se manca) |
| 0x8be190 | `bool ()` | sempre 1 |
| 0x8be198 | `bool IsActive(pad, flag)` | gp && (flag && [[0x8ace7c()]+0x38]+0xf5 \|\| !gp->vt[0xa0]) && 0x7ce350(gp) (gp+0x760≠0 e gp+0x770 ≠ PIM+0xb0) && !gp->vt[0x88] |
| 0x8be248 | `bool IsReady(pad, flag)` | come sopra senza 0x7ce350 → gp->vt[0x80] (tail call) |
| 0x8be2e8 | `void ResetPlayer(pad, flag)` | gp->vt[0x70]; entry+0x40 = 0; flag → 0x865a14(entry, -1) |
| 0x8be36c | `void Rumble(pad, a, b)` | 0x7cdbac(gp+0x3d8, a, b): byte +0x35c/+0x35d [IPOTESI vibrazione] |
| 0x8be3f0 | `void RumbleStop(pad)` | 0x7cd990(gp+0x3d8) [IPOTESI] |
| 0x8be458 | `void SetButtonsOn(pad, on)` | +0x1c = on; spegnendo azzera +0xc/+0x10/+0x14 |
| 0x8be478 | `void SetTouchOn(pad, on)` | +0x1d = on; spegnendo azzera +0x1e..+0x210 |
| 0x8be674 | `void Calc(pad)` | §1.3 |
| 0x8beff4 | `ctor(container)` | 0x38 B a zero |
| 0x8bf008 | `void(container, heapArg*)` | +0x28 = 0x5faba8(0x218, [x1], 8) + ctor 0x7267e4 (controller di default) |
| 0x8bf040 | `void SetPad(container, i, pad)` | **pad+8 = i sempre**; ptr[i<5 ? i : 0] = pad |
| 0x8bf058 | `void CalcAll(container)` | +0x30 = 0; Calc(ptr[0..4]) srotolato (ultimo in tail call) |
| 0x8bf09c | `Container* Get()` | `[[GOT 0x1306e70]]+0x58` |
| 0x8bf158 | `Pad* GetPad(i)` | ptr[i<5 ? i : 0] (`cmp w19,#5; csel`, unsigned) |

I 12 siti `cmp w0,#3` sono tutti in queste funzioni (§4). Nessun altro `cmp #3/#4/#5` nella famiglia oltre a
`cmp x22,#0x20` (0x8bec04, fusione su 4 puntatori) e ai clamp 5 di 0x8bf044/0x8bf170 [VERIFICATO grep su
0x8bde24-0x8bf188].

### 1.3 Catena di input e disassemblato commentato di 0x8be674 [VERIFICATO]
Catena: `pad+8 (idx) →[cmp #3] 0x86ebf8(idx) = raceMgr+0x17c[idx] → e → raceMgr+0x58[e] → entry+0x200 = GamePlayer
→ ctrl = gp+0x3d8` (non gp+0x760: quello è il sead::Controller grezzo usato da 0x7ce350).
```
8be674  prologo; x19 = pad
8be698  if (!pad[0x1c] && !pad[0x1d]) return            ; pad spento: niente (stato vecchio resta)
8be6a8  if (0x86a958(0x8acdcc()->[+0x800])) return      ; raceMgr: [sys+0x9a8] 0x862120 || raceMgr+0x200==1
8be6b8  t = idx<=3 (cmp #3 @8be6bc) && e!=-1 && gp ? gp+0x73c : 0
8be70c  if (t != pad+0x2c0) { pad+0x2c0 = t; azzera +0xc,+0x10,+0x1e, ripristina tocco +0x20..+0x210 }
8be790  x25 = ctrl = idx<=3 (cmp #3 @8be794) ... gp ? gp+0x3d8 : 0
8be7d8  if (pad[0x1d]) {                                 ; TOCCO
8be7f0     t = 0x7c6360(PIM 0x8ace0c());  pad[0x1f] = ([t+8]>>15)&1
8be810     se tocco premuto (t+0x14 bit0): pos = t+0x118/0x11c (filtrata con 0xed5b24 se +0x1e) -> +0x20/+0x24
8be8e0     lista = 0x8bfcc0(); per 10 zone: 0x8bec98(...) aggiorna storici +0x28.. / +0x124..
        } else pad[0x1f] = 0
8beb60  if (!pad[0x1c]) { +0xc = +0x10 = +0x14 = 0; goto 8bec50 }   ; PULSANTI spenti
8beb68  if (pad[0x2c5]) {                                ; FUSIONE
8beb70     c = 0x8bf09c(); +0xc = +0x10 = +0x14 = 0
8beb84     for (x22 = 0; x22 != 0x20; x22 += 8) {        ; SOLO ptr[0..3] del contenitore
             q = c[x22/8]; if (!q || q+8 > 3 (cmp #3 @8beb90)) continue
             gp = ...0x86ebf8(q+8)...+0x200; if (!gp) continue
             +0xc |= gp+0x4ec ; +0x10 |= gp+0x3e0 ; +0x14 |= gp+0x3e0 | gp+0x3e8   ; = ctrl+0x114/+0x8/+0x10
           }
        } else if (ctrl) { +0xc = ctrl+0x114; +0x10 = ctrl+0x8; +0x14 = ctrl+0x8 | ctrl+0x10 }
          else { 0 }
8bec50  +0xc |= +0x18; +0x10 |= +0x18; +0x14 |= +0x18; +0x18 = 0
```

## 2. Creazione dei pad e proposta per gli extra 4..7

### 2.1 Contesto [VERIFICATO] (`c_fill_and_calc.asm`)
- Oggetto sistema menu 0xd0 B: `0x5faba8(0xd0, heap, 8)` a 0x3fecb8, vtable 0x1265898+0x10, poi
  0x8b1164(this, arg): `[GOT 0x1306e70] = this` (singleton) e, se +0x58 è NULL, `0x5fa960(0x38)` + ctor
  0x8beff4 + 0x8bf008 (0x8b1208-0x8b122c).
- Riempimento in 0x3ff5d0 (slot vtable a 0x12658e0 dello stesso oggetto): 0x3ffaec-0x3ffb90, 5 volte
  `p = 0x5fa960(0x2c8); 0x8bde24(p); 0x8bf040(this+0x58, i, p)` per i = 0..4 in ordine; poi
  0x3ffb94-0x3ffbac `p4 = cont[4]; 0x8be458(p4, 0); 0x8be478(p4, 0)` e 0x3ffbb8 `0x422efc(1, 0)`
  (tocco: pad 0 acceso, 1..3 spenti; pulsanti di 0..3 restano accesi dal ctor).
- 0x5fa960(size) = new sull'heap corrente (`0x609f64` → vt+0x30 alloc(size, 8); senza HeapMgr 0x58294c).
- Calc: 0x8b1074 (vtable 0x12d3558, calc del sistema menu) → 0x8bf058(this+0x58) a 0x8b10f4, ogni frame salvo
  il ramo `this+0xa8 && sottosistema[2]+0x415`.

### 2.2 Stato iniziale da replicare
| pad | +0x8 | +0x1c pulsanti | +0x1d tocco | origine |
|---|---|---|---|---|
| 0 | 0 | 1 | 1 | ctor + 0x422efc(1,0) |
| 1..3 | 1..3 | 1 | 0 | ctor + 0x422efc(1,0) |
| 4 (tocco) | 4 → **8** | 0 | 0 | 0x3ffba0 / 0x3ffbac |
| extra 4..7 (nuovi) | 4..7 | 1 | 0 | come 1..3 |

### 2.3 Creazione proposta
- Memoria: **statica nel modulo** (`alignas(16) uint8_t mem[4][0x2c8]`) invece di 0x5fa960: l'heap "corrente"
  dipende dal contesto del chiamante e i pad extra non sono nel contenitore (nessuno li distrugge). Il ctor
  0x8bde24 prende solo `x0`.
- Momento: nel trampolino di **0x8bf040** quando `i == 4` (ultimo dei 5 set, stesso istante dell'originale).
  Lì si porta anche `pad4+8 = 8` (il pad tocco esce dall'intervallo 0..7 dei `cmp #7`: senza questo, con la
  patch #7 il pad tocco leggerebbe il GamePlayer del giocatore 5).
- Nessun altro scrittore di pad+8 oltre a 0x8bf040 [VERIFICATO: nessun `[x0,#0x8]` negli usi del risultato di
  0x8bf158, `c_8bf158_callers.txt`]; 0x8bf040 è chiamata solo dai 5 siti 0x3ffb10..0x3ffb90.

### 2.4 Hook REPLACE su 0x8bf158 (condizionato)
Mappa: `i<4 → ptr[i]`; `i==8 → ptr[4]` (tocco, sempre); `4≤i≤7 → extra[i-4]` **solo se active8**; altrimenti
`i==4 → ptr[4]`, resto `→ ptr[0]` (clamp originale, unsigned). In 1-4P ogni risposta è identica all'originale,
compresi i siti non patchati che passano 4 con senso "tocco" (campo +0x118 = 4, §2.5).

### 2.5 Altri usi dell'indice 4 (non coperti dalle 4 patch) [VERIFICATO] (`c_index4_sites.txt`)
| sito | uso | effetto in 5-8 con l'hook condizionato |
|---|---|---|
| 0x4b82a4, 0x4bbcdc, 0x4c17c8, 0x4c3c98 | `orr w0,wzr,#4` → GetPad → SetTouchOn | patch `321e03e0 → 321d03e0`: restano sul pad tocco |
| 0x8ba3bc (ctor base controlli 0x8ba39c) | +0x118 = 4 (proprietario di default) | indice 4 = giocatore 5: tocco su quei controlli letto dal pad extra (tocco spento) |
| 0x53b6f8, 0x46249c | +0x118 = 4 (0x462488: 6 se w2&1) | idem; 6 → in 1-4P resta ptr[0] (clamp) |
| 0x4524a0..0x4524c0, 0x452650, 0x453000, 0x453058, 0x4533ec, 0x4554d4, 0x455570, 0x456c10 | lettori di +0x118 → 0x8ba634 / 0x8bf158 | [DA VERIFICARE]: con N>4 indice 4 = giocatore 5 |
| 0x4b57b4, 0x4c02bc, 0x4c9cf4 (`orr w8,w8,#0x10`), 0x4c9b50(page,4) a 0x4b82bc/0x4c17dc | bit 4 della maschera pagina +0x1a8 = pad 4 | la pagina accetta il pad 4 = giocatore 5 |
| scanner 0x8b4038 / 0x8b4c0c (vtable 0x12d3d98 / 0x12d3dd0) | per i = 0..4 srotolato: 0x8ba634/0x8ae0d0 → 0x8b4174 / 0x8b4d48 | giocatori 6..8 non scansionati; 0x8b4d48 ha stato s32[5] a +0x20 (`cmp w20,#5`) non estendibile in loco |
| container +0x30 (bit i = pad i ha consumato input; bit31 globale) | 0x4c3fac, 0x8af764, 0x8af848, 0x4e1f4c, 0x537ae8, test 0x8ba680 | 32 bit: indici fino a 8 ci stanno |
| 0x8af3f4 (classe lista/griglia, 64 chiamanti) | stato per pad 4 × 0x20 a +0x28 con `cmp w21,#4` (0x8af474, 0x8af490) | pad 4..7 condividono lo stato del pad 0 (per A2) |
| 0x8beb84-0x8bec08 | fusione su ptr[0..3] | estesa nel calc (§3) |
| 0x4be760/0x4be794 | `cmp w1/w2,#4..#8`: transizioni di STATO pagina → frame animazione | non sono indici pad: nulla da fare |

Nessun ciclo fino a 5 sul contenitore oltre a 0x8bf058; i lettori di 0x8bf09c (17) usano solo +0x28, +0x30 o
ptr[0..3] (0x8beb70) [VERIFICATO, `c_index4_sites.txt` §G]. Opzione "C-bis" se i log mostrano problemi sul
proprietario 4 in 5-8: spostare anche i default a 8 (`0x8ba3bc 321e03ea→321d03ea`, `0x53b6f8 321e03e8→321d03e8`,
`0x46249c 321e03e8→321d03e8`) e i bit maschera (`0x4b57b4/0x4c02bc/0x4c9cf4 321c0108→32180108` = `#0x100`),
più 0x4c9b50 (confronto `w1==4`); NON consigliata subito: tocca anche 1-4P (gli scanner 0..4 non vedrebbero più
il bit del tocco).

## 3. Calc per frame 0x8bf058 e fusione
Chiamante unico 0x8b10f4 (in 0x8b1074). La fusione legge i GamePlayer, non gli altri pad ⇒ basta un
**trampolino su 0x8bf058**: dopo `Orig` (calcola 0..4, compresa la fusione su 0..3) calcolare gli extra e
aggiungere ai pad con +0x1c && +0x2c5 gli input dei GamePlayer dei giocatori 4..7, con la stessa guardia
0x86a958 di 0x8be6a8. L'OR dopo `Orig` è equivalente: +0x18 è già stato consumato e la fusione originale è un
OR puro. Non serve un hook su 0x8be674.

## 4. I 12 siti `cmp #3 → #7` [VERIFICATO] (`c_cmp3_sites.txt`, parola letta da main.bin)
| sito | b.hi → | ramo "falso/vuoto" |
|---|---|---|
| 0x8be040 | 0x8be060 | `mov w0,#-1` (EntryIndex) |
| 0x8be078 | 0x8be0ac | x0 = NULL (GetEntry) |
| 0x8be0cc | 0x8be114 | x0 = NULL (GetCtrl) |
| 0x8be138 | 0x8be180 | *out = 0 (GetPlayerType) |
| 0x8be1ac | 0x8be234 | falso (IsActive) |
| 0x8be25c | 0x8be2c0 | falso (IsReady) |
| 0x8be2fc | 0x8be360 | ret (ResetPlayer) |
| 0x8be388 | 0x8be3e0 | ret (Rumble) |
| 0x8be400 | 0x8be44c | ret (RumbleStop) |
| 0x8be6bc | 0x8be700 | tipo 0 (Calc) |
| 0x8be794 | 0x8be7e4 | ctrl = NULL (Calc) |
| 0x8beb90 | 0x8bec00 | salta il pad (fusione) |
Tutti `71000c1f` seguiti da `b.hi` (cond 8) e da `bl 0x86ebf8` (0x8be040: dopo il prologo). `#7` è corretto:
`71001c1f` (imm12 = 7) lascia passare idx 4..7 e rifiuta 8 (pad tocco) e i negativi (unsigned). Dopo
0x86ebf8 ogni sito controlla `cmn #1`, `e < raceMgr+0x50`, entry/gp NULL: con 0x86ebf8(4..7) = -1 il risultato è
uguale al vecchio ramo falso. 0x8be068 (usato da 0x3fd5a4) è nell'elenco; 0x8be2e8/0x8be36c/0x8be3f0 pure.
**Attenzione** 0x3fd5a4: con w2≠0 deref `[entry+0x29c]` senza controllo NULL (0x3fd5d0-0x3fd5d4); chiamata con
i = slot da 0x4b7454 (charsel): per slot 4..7 l'entry deve esistere (B).

## 5. Effetto su 1-4 giocatori e rischi
- 1-4P [VERIFICATO statico]: idx 0..3 invariati; il pad tocco ha idx 8 → `cmp #7` b.hi come prima con 4;
  GetPad(4) → ptr[4], GetPad(5..7) → ptr[0], GetPad(8) → ptr[4] (= vecchio GetPad(4) dei 4 siti patchati);
  extra mai restituiti né calcolati; la fusione resta su 0..3. Comportamento identico.
- Rischi 5-8:
  1. proprietario/maschera 4 (§2.5): controlli "tocco/comuni" con +0x118 = 4 e pagine con bit 4 diventano del
     giocatore 5; il tocco su quei controlli smette di funzionare (in 5-8 non si gioca in portatile; resta il
     tavolo) [DA VERIFICARE].
  2. scanner pulsanti 0..4 e stato s32[5] (0x8b4d48), cursori a 4 (0x8af3f4), cicli 0..3 su +0x2c4 e sulla pagina
     di gara 0x49c938: i giocatori 6..8 non premono i pulsanti "comuni" né confermano (step successivi/A2).
  3. dipendenza da B: senza l'hook di 0x86ebf8 (e GamePlayer per le entry 4..7, 0x86a594) i pad extra hanno
     input nullo e IsReady falso → applet in loop come prima dello step 4 (se si toglie H4).
  4. 0x3fd5a4 con entry NULL per slot 4..7 → crash (§4).
  5. 0x8bf0b0 (Cfg) per frame dentro il calc: già usata nel menu da PadReadyHook; lo stesso 0x3ffbc4 la chiama
     subito dopo la creazione [IPOTESI: valida durante il calc].

## 6. Sintesi delle modifiche C (ordine)
Prerequisito B: hook `LocalMapHook` su 0x86ebf8 (i = 4..7 → locale i o -1) + riempimento +0x17c/+0x18c e
GamePlayer per le entry 4..7. **Non** patchare 0x86ec08 a #7 senza rilocare +0x17c (leggerebbe +0x18c..).

| # | tipo | offset | orig → nuovo | note |
|---|---|---|---|---|
| C1 | patch ×12 | 0x8be040 0x8be078 0x8be0cc 0x8be138 0x8be1ac 0x8be25c 0x8be2fc 0x8be388 0x8be400 0x8be6bc 0x8be794 0x8beb90 | `71000c1f → 71001c1f` | 0x8be25c è dentro PadReadyHook (trampolino su 0x8be248: tocca solo la 1ª istruzione, ok) |
| C2 | patch ×4 | 0x4b82a4 0x4bbcdc 0x4c17c8 0x4c3c98 | `321e03e0 → 321d03e0` | GetPad(8) = pad tocco |
| C3 | trampolino | 0x8bf040 (1ª istr. `93407c28`) | – | i==4: pad+8 = 8, crea extra |
| C4 | REPLACE | 0x8bf158 (`f81e0ff3`) | – | mappa §2.4 |
| C5 | trampolino | 0x8bf058 (`f81e0ff3`) | – | active8, calc extra, fusione |
| C6 | modifica | PadReadyHook | – | togliere il caso `idx==4 && n>4` di H4 (lo slot 4 è il giocatore 5 vero); tenere il log |
```cpp
/* ---- step 5 C: pad UI 4..7 (report/step5_plan_C.md) ---- */
namespace padui {
    constexpr int Extra = 4;
    alignas(16) static uint8_t mem[Extra][0x2c8];
    static uint8_t* extra[Extra];
    static bool active8;                                             /* N>4, aggiornato per frame in C5 */
    static uint8_t** Cont() { return mk8dx::Fn<uint8_t** (*)()>(0x8bf09c)(); }
}
HOOK_DEFINE_TRAMPOLINE(PadSetHook) {                                 /* 0x8bf040(container, i, pad) */
    static void Callback(uint8_t** c, int32_t i, uint8_t* pad) {
        Orig(c, i, pad);
        if (i != 4 || !pad) return;
        *(int32_t*)(pad + 0x8) = 8;                                  /* pad tocco fuori da 0..7 */
        if (!padui::extra[0]) for (int k = 0; k < padui::Extra; k++) {
            uint8_t* p = padui::mem[k];
            mk8dx::Fn<void (*)(void*)>(0x8bde24)(p);                /* ctor: +0x1c = +0x1d = 1 */
            *(int32_t*)(p + 0x8) = 4 + k;                            /* come 0x8bf040 */
            mk8dx::Fn<void (*)(void*, bool)>(0x8be478)(p, false);   /* tocco spento, come i pad 1..3 */
            padui::extra[k] = p;
        }
        Logging.Log("[mk8dx-8p] padui: cont=%p pad0..4=%p %p %p %p %p, tocco idx->8, extra=%p..%p",
                    c, c[0], c[1], c[2], c[3], c[4], padui::extra[0], padui::extra[3]);
    }
};
HOOK_DEFINE_REPLACE(GetPadHook) {                                    /* 0x8bf158(i) */
    static uint8_t* Callback(int32_t i) {
        uint8_t** c = padui::Cont();
        uint32_t u = (uint32_t)i;
        if (u < 4) return c[u];
        if (u == 8) return c[4];                                     /* pad tocco (siti C2) */
        if (u < 8 && padui::active8 && padui::extra[u - 4]) {
            /* log una volta per chiamante: chi chiede 4..7 in modalità 5-8 */
            return padui::extra[u - 4];
        }
        return u == 4 ? c[4] : c[0];                                 /* = clamp originale */
    }
};
HOOK_DEFINE_TRAMPOLINE(PadCalcAllHook) {                             /* 0x8bf058(container) */
    static void Callback(uint8_t** c) {
        Orig(c);
        int n = menu8::LocalCount();
        bool a = n > 4 && n <= 8 && padui::extra[0];
        if (a != padui::active8) Logging.Log("[mk8dx-8p] padui: active8 %d -> %d (N=%d)", padui::active8, a, n);
        padui::active8 = a;
        if (!a) return;
        for (int k = 0; k < padui::Extra; k++) mk8dx::Fn<void (*)(void*)>(0x8be674)(padui::extra[k]);
        /* fusione (+0x2c5): 0x8beb84 fonde solo ptr[0..3]; aggiunge i giocatori 4..7 (stessa guardia di 0x8be6a8) */
        uint8_t* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)();
        if (!rm || mk8dx::Fn<bool (*)(void*)>(0x86a958)(rm)) return;
        for (int j = 0; j < 5; j++) {
            uint8_t* p = c[j];
            if (!p || !p[0x1c] || !p[0x2c5]) continue;
            for (int k = 0; k < padui::Extra; k++) {
                uint8_t* ctl = mk8dx::Fn<uint8_t* (*)(void*)>(0x8be0bc)(padui::extra[k]);   /* gp+0x3d8 */
                if (!ctl) continue;
                *(uint32_t*)(p + 0xc)  |= *(uint32_t*)(ctl + 0x114);
                *(uint32_t*)(p + 0x10) |= *(uint32_t*)(ctl + 0x8);
                *(uint32_t*)(p + 0x14) |= *(uint32_t*)(ctl + 0x8) | *(uint32_t*)(ctl + 0x10);
            }
        }
    }
};
/* exl_main, dopo le patch/hook dello step 4 e DOPO gli hook di B (0x86ebf8 + riempimenti): */
static const uintptr_t cmp3[] = {0x8be040, 0x8be078, 0x8be0cc, 0x8be138, 0x8be1ac, 0x8be25c,
                                 0x8be2fc, 0x8be388, 0x8be400, 0x8be6bc, 0x8be794, 0x8beb90};
bool okC = true;
for (uintptr_t o : cmp3) okC &= input::Patch(o, 0x71000c1f, 0x71001c1f, "C1 pad UI idx cmp #3 -> #7");
for (uintptr_t o : {0x4b82a4, 0x4bbcdc, 0x4c17c8, 0x4c3c98})
    okC &= input::Patch(o, 0x321e03e0, 0x321d03e0, "C2 GetPad(4) tocco -> GetPad(8)");
if (okC) { PadSetHook::InstallAtOffset(0x8bf040); GetPadHook::InstallAtOffset(0x8bf158); PadCalcAllHook::InstallAtOffset(0x8bf058); }
```
(Se una patch C1/C2 fallisce le altre già scritte restano: meglio verificare tutte le 16 parole prima di
scriverne una, come per P3/P4.) Gli hook vanno installati prima della creazione del sistema menu (exl_main lo è).

### [DA VERIFICARE A RUNTIME] e log
1. Avvio: log di PadSetHook (5 puntatori, extra creati una sola volta; se 0x3ff5d0 gira più volte compare più
   volte). 2. 1-4P: nessun `active8 -> 1`; regressione menu/charsel/gara 4P invariata (tocco in portatile nel
   charsel 1P: pad tocco via GetPad(8)). 3. 8g: `active8 0 -> 1`; log una-tantum (i, LR = `__builtin_return_address(0)`
   – Base) per GetPad(4..7) → elenco reale dei chiamanti (attesi 0x4ba998, 0x492300, 0x4b7458…; quelli da +0x118
   = rischio §5.1). 4. Log variazioni di +0xc/+0x10 degli extra (pressione su controller 5..8 → pad UI 4..7). 5.
   IsReady(extra) vero per 4..N-1 senza H4 (richiede B) e applet chiamato una volta. 6. Pausa in gara con +0x2c5
   sul pad 0: il controller 6 apre/naviga la pausa (log OR non nullo nella fusione).
