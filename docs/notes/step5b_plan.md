# Step 5b – selezione personaggi con 8 cursori simultanei: passo 1 (A1-M2) e catalogo aggiornato (analisi statica)

Data: 3 nov 2026. Base: `../img/main.bin` (offset = indirizzo), `../img/main.asm`. `src/` non modificato, nessuna build.
Dati: `step5b_data (dati non pubblicati)/` (`verify_114.txt`, `scan_900_927_4xxxxx.txt`, `scan_900_927_global.{txt,py}`, `clamp_windows.asm`,
`clamp_reuse.txt`, `passo1_table.inc`, `page_registrations.txt`).
Etichette: [VERIFICATO] = letto in main.bin/main.asm (indirizzo indicato); [IPOTESI] = da confermare in Eden.

## 0. Sintesi
- **114/114 parole** di `a1_904_reloc.txt` (3 fabbriche + 72 accessi + 39 clamp) coincidono con main.bin. Le nuove parole,
  disassemblate, differiscono dalle originali solo nell'immediato atteso (0x904→0xa28, 0x908→0xa2c, 0x90c→0xa30, 0x914→0xa38,
  0x91c→0xa40, #4→#8, 0xa28→0xa68) [VERIFICATO, `verify_114.txt`].
- **Nessun accesso sfuggito nel main**: 0 accessi a +0x904..+0x927 dell'oggetto fuori dai 72 (ricerca testuale su tutte le forme
  nell'intervallo 0x400000-0x4e0000, scansione binaria di tutto il .text, basi calcolate, vtable, +0x3b8, getter 0x8bf0ec) [VERIFICATO].
- **Un accesso sfuggito nella mod**: `sel8::SlotState()` (main.cpp:1607) usa `page + 0x904`. È usato dalla selezione a due turni
  (CharselFrameHook, righe 1642/1648): con la rilocazione attiva **deve** passare a +0xa28, altrimenti il turno 2 non parte e
  scrive nel canary [VERIFICATO].
- Coda +0xa28..+0xa67 libera (massimo campo usato +0xa20, s32) e le 3 fabbriche sono le uniche allocazioni [VERIFICATO].
- **Clamp: lasciarli a #4 nel passo 1.** 12 dei 39 clamp proteggono anche un altro array a 4 (selettore cursori +0x1b0, cfg+0x1fb4..,
  kart +0x460); inoltre il ciclo 0x4b80e4 va fino a N locali (cfg+0x1fa4), non fino a +0x8c0: con #8 in 5-8P scriverebbe gli
  slot 4..7 e cambierebbe il comportamento. I clamp vanno nel passo 4 (+0x8c0 = N), insieme agli array condivisi.
- Passo 1 = **75 parole + 1 trampolino (0x4bc844) + correzione di `sel8::SlotState`**.

## 1. Passo 1 (A1-M2): verifiche

### 1.1 Parole della lista [VERIFICATO]
`verify_114.txt`: per ognuna delle 114 voci, la parola a main.bin[off] è uguale a `orig`. Le parole `orig` e `repl` sono state
disassemblate con llvm-objdump e confrontate dopo la sostituzione dell'immediato. Le 3 righe segnate DIFF sono solo il commento
`// =2600` di `mov w0,#0xa28`, quindi sono corrette. Codifiche: `ldr/str w` imm12·4 (0xa40/4 = 0x290 < 0x1000), `add x` imm12 (0xa2c).

### 1.2 Ricerca di accessi sfuggiti
| metodo | esito |
|---|---|
| Testo di main.asm, 0x400000-0x4e0000: ogni istruzione con `#0x900..#0x927` (ldr/str/ldrh/ldrb/s, add, mov con commento) | 72 REL + altri, tutti classificati (`scan_900_927_4xxxxx.txt`): nella classe, +0x900 (s32 cursore: 0x4b82c4, 0x4ba82c, 0x4ba904/90c, 0x4baab8/ad0/ae8, 0x4bbd60) è un campo distinto; 0x4b6140/0x4b62a8/0x4b9928 `ldrh [x9,#0x908]` con x9 = `[[0x8aceb4()+0x158]+0x188]+i*2` (salvataggio, 0x4b6100); 0x4b2d00..0x4b30f8 `ldr s,[x9,#0x910]` = costanti via adrp 0xed5000; 0x4a5e18..0x4a8050 `add #0x900` = basi sp/adrp; 0x4c72a4/0x4c72b0, 0x4ceda8, 0x4d2f78, 0x4d6650.. altre classi o adrp [VERIFICATO] |
| Scansione binaria di tutto il .text (0..0xed4000): ldr/str w, x, s e add con offset 0x8fc..0x927 (`scan_900_927_global.py`) | ldp/stp (imm7, max 252/504) e ldur/stur (±256) **non possono** codificare 0x904: servirebbe una base intermedia. Nessuna base intermedia nella classe (sotto) [VERIFICATO] |
| Basi intermedie: `add xD, xN, #0x700..#0x92f` non da adrp in 0x4a0000-0x4cffff | classe: +0x740 (48 puntatori L_Chara, cicli ≤ 0x2f: 0x4b56e8, 0x4b8904, 0x4b97f8, 0x4b9b30, 0x4b5f5c, 0x4b7ad8), +0x700 (quinta transizione, 0x4b8a14, 0x4bc904), +0x8d0 (scanner 0x8b4010: 0x4b5790, 0x4bbd40 solo confronto, 0x4bc914), +0x928 (0x4b4f1c). Lo scanner scrive al massimo fino a +0x8d0+0x2c = +0x8fc (0x8b4010: `stur xzr,[x0,#0x25]`, `str xzr,[x0,#0x20]`), quindi non tocca +0x904. Nessun `add …,#0x900` o `#0x8xx` seguito da offset piccoli verso +0x904 [VERIFICATO] |
| Init srotolati 0x4b80a0-0x4b8294 | 0x4b80c8, 0x4b80ec (+0x4b80fc/0x4b8108 `str w11,[x13,x15]`), 0x4b81dc, 0x4b81f8/0x4b8230/0x4b8248/0x4b8274/0x4b8284: tutti nella lista [VERIFICATO] |
| Ctor base 0x4bc844..0x4bc998 | azzera +0x904/+0x90c/+0x914/+0x91c (nella lista); nessun memset/memcpy sull'oggetto; il ctor padre 0x8ba39c è la base pagina [VERIFICATO] |
| Pagina kart (+0x3b8) | 23 `ldr x8,[x19,#0x3b8]` in 0x4c04c8..0x4c4518, tutti seguiti dagli accessi della lista; 0x4bef84 è un `ldr [x8,#0x3b8]` da adrp (GOT). Le altre ~250 occorrenze di `#0x3b8` sono fuori da 0x4a-0x4e e senza accessi 0x9xx nel blocco [VERIFICATO] |
| Pagina regali (0x8bf0ec(0x2d/0x2e) a 0x4b0fbc/0x4b10ac; `[x19+0x1f8]` a 0x4b0750/0x4b14c8/0x4b1d08) | solo gli accessi della lista (0x4b111c..0x4b11cc, 0x4b1d2c) [VERIFICATO] |
| Pagine CharaWin 0x28..0x2b (0x4b1e00-0x4b5600) | nessun accesso 0x900..0x927 (usano +0x928: 0x4b4f1c, 0x4b53d4) [VERIFICATO] |
| Getter 0x8bf0ec con id costante 0x2c..0x2e (297 chiamate nel .text) | 6 siti: 0x4b0fbc, 0x4b10ac (regali), 0x4b6cd0 (classe), 0x4c480c/0x4c4ae0/0x4c4db4 (ctor kart → +0x3b8). Nessun'altra classe prende la pagina [VERIFICATO]; id calcolati a runtime (link generici 0x8ba5fc) passano solo da codice framework con offset < 0x300 [IPOTESI forte] |
| vtable 0x1278c38 (93 slot) | gli slot fuori 0x4a-0x4e puntano al framework (0x7240d8..0x725a18, 0x8baa34..0x8bacf8) e a stub 0x409f74.., 0x48f14..: nessun accesso 0x9xx in quegli intervalli [VERIFICATO] |
| Framework UI 0x8a0000-0x8c8000 | 0x8abc14..0x8abc24 `add x9,x19,#0x904` e simili: ctor di un'altra classe (campi fino a +0x9a8), non chiamato da 0x4bc844 [VERIFICATO] |
| Codice della mod (main.cpp) | **`sel8::SlotState` +0x904 (riga 1607)**: unico sito. Nessun hook o patch esistente cade su una delle 114 parole (indirizzi 0x4b*/0x4c* in main.cpp: nessuna collisione) [VERIFICATO] |

Conclusione: **0 accessi sfuggiti nel main, 1 nella mod**. Resta il controllo a runtime con il canary o con un watchpoint (§2.3).

### 1.3 Coda libera e allocazioni [VERIFICATO]
- Offset massimo usato dalla classe: `+0xa20` (s32: 0x4b5bd0, 0x4b7988, 0x4bbb84), quindi fine dell'ultimo campo +0xa24 e oggetto da
  0xa28. Nessuna istruzione di 0x4a0000-0x4d0000 con base pagina usa offset ≥ 0xa28: le occorrenze 0xa60..0xa98 sono a 0x4ac750
  e 0x4d3028..0x4d5c80 (altre classi). Pagina regali e kart arrivano a `[+0x3b8]+0x904..+0x91c`.
- `mov w0,#0xa28` compare 3 volte (0x4091c4/0x4091f0/0x40921c); la quarta, 0x9299dc `mov w27,#0xa28`, è una costante di un
  ciclo non correlato. 0x4bc80c/0x4bcad8/0x4bcc4c sono chiamati solo dalle 3 fabbriche (0x4091d0/0x4091fc/0x409228); 0x4bc844
  solo dai 3 ctor (0x4bc81c/0x4bcae8/0x4bcc5c). Nessun salto `b` né puntatore in dati verso i ctor. Le fabbriche sono
  referenziate solo dalle tabelle 0xe856e0/0xe856f8/0xe85710 e dal GOT 0x1307b40..b50.
- Fabbrica = `mov w0,#size; bl 0x5fa960; bl ctor`: la dimensione non è usata altrove.

### 1.4 Clamp `cmp #4`: indici condivisi con altri array a 4 [VERIFICATO, `clamp_reuse.txt`, `clamp_windows.asm`]
Nel blocco del clamp, 12 dei 39 clamp usano l'indice limitato anche su un altro array a 4:

| clamp | dopo +0x904 lo stesso indice indicizza | con #8 e indice 4..7 |
|---|---|---|
| 0x4b1d20 (regali) | selettore cursori `[page+0x1b0]+i*0x20+0x30` (0x4b1d3c), 4 slot in un oggetto da 0xb8 | lettura a +0xb0..+0x110: fuori dall'oggetto per i ≥ 5 |
| 0x4b8330 (classe) | cfg+0x1fb4 / +0x1fd4 / +0x1fe4 / +0x1ff4 `[cfg + i*4]` (0x4b83c8, 0x4b8474, 0x4b84dc, 0x4b8540) | legge l'array successivo della config |
| 0x4b9b78 | cfg+0x1fb4 (0x4b9bfc) | come sopra |
| 0x4ba404 / 0x4ba5a4 | selettore cursori `[this+0x1b0]+i*0x20+0x30` (0x4ba45c / 0x4ba5fc, poi `0x8af3f4`) | indice cursore spazzatura |
| 0x4c154c (kart) | **scrittura** kart `+0x460+i*8` (0x4c1574) | scrive +0x480..+0x498, e +0x498 è il flag di avanzamento |
| 0x4c18d0 | selettore cursori kart `[kart+0x1b0]+i*0x20+0x30` (0x4c1928) | come sopra |
| 0x4c2a10 / 0x4c2a9c / 0x4c2ff8 / 0x4c3d74 / 0x4c3ed0 | kart +0x460 (lettura) | fuori dall'array |

Gli altri 27 clamp usano l'indice solo per +0x904. I siti CharaWin (`add w8,wI,#0x28`) usano l'indice **non** limitato
(es. 0x4ba87c `w22`, 0x4ba490 `w20`), quindi non dipendono da questi clamp.

**Ciclo 0x4b80e4/0x4b80f4** [VERIFICATO 0x4b80a0]: il limite è `cfg+0x1fa4` (N locali), non +0x8c0. Oggi, con N = 5..8 (due
turni), gli indici 4..7 vanno a 0 (`str 3` sullo slot 0, già 3) e i bit 4..7 di +0x1a8 si accendono comunque (lsl non limitato).
Con #8 gli slot 4..7 della nuova tabella prenderebbero lo stato 3: il comportamento in 5-8P cambierebbe.

**Raccomandazione: nel passo 1 nessun clamp, solo 75 parole.** Motivi:
1. Con +0x8c0 = 4 e clamp a 4 la rilocazione è indipendente dall'indice: stesse 4 voci, solo spostate. Il comportamento è
   identico al vanilla in 1-4P **e** in 5-8P (due turni). Il passo diventa una prova pura della rilocazione.
2. Portare i clamp a #8 non serve finché +0x8c0 ≤ 4. Inoltre 12 siti diventerebbero letture o scritture fuori limite appena un
   indice ≥ 4 arriva (ciclo 0x4b80e4 con N > 4).
3. I clamp vanno applicati nel passo 4 (+0x8c0 = N), **dopo** il selettore cursori a 8 slot (passo 3, che copre 0x4b1d20,
   0x4ba404, 0x4ba5a4, 0x4c18d0) e insieme alle soluzioni per kart +0x460 (passo 5: 0x4c154c, 0x4c2a10.., 0x4c3ed0) e per la
   config (passo 6: 0x4b8330, 0x4b9b78). Tabella pronta in `passo1_table.inc` (`kClamp904`, colonna S = condiviso).
Variante facoltativa (passo 1b): applicare solo i 27 clamp "puri" è innocuo in 1-4P, ma in 5-8P accende gli slot 4..7 tramite
0x4b80f4. Non conviene.

## 2. Passo 1 – patch esatte

### 2.1 Tabella (75 parole, verificate) – da `step5b_data/passo1_table.inc`
```cpp
namespace sel8 {
    struct P { uint32_t off, orig, repl; };
    static uint32_t slotOff = 0x904;                         /* 0xa28 se A1-M2 attivo: usato da SlotState */
    static const P kReloc904[] = {
        {0x4091c4, 0x52814500, 0x52814d00}, {0x4091f0, 0x52814500, 0x52814d00}, {0x40921c, 0x52814500, 0x52814d00}, /* 0xa28->0xa68 */
        /* regali (pagina 0x2d/0x2e) */
        {0x4b111c, 0xb9490509, 0xb94a2909}, {0x4b112c, 0xb9490509, 0xb94a2909}, {0x4b1134, 0xb9490509, 0xb94a2909},
        {0x4b1148, 0xb9490509, 0xb94a2909}, {0x4b1158, 0xb9490d09, 0xb94a3109}, {0x4b1160, 0xb9490d09, 0xb94a3109},
        {0x4b1174, 0xb9490d09, 0xb94a3109}, {0x4b1184, 0xb9491509, 0xb94a3909}, {0x4b118c, 0xb9491509, 0xb94a3909},
        {0x4b11a0, 0xb9491509, 0xb94a3909}, {0x4b11b0, 0xb9491d09, 0xb94a4109}, {0x4b11b8, 0xb9491d09, 0xb94a4109},
        {0x4b11cc, 0xb9491d08, 0xb94a4108}, {0x4b1d2c, 0xb9490508, 0xb94a2908},
        /* classe selezione */
        {0x4b564c, 0xb9090509, 0xb90a2909}, {0x4b80c8, 0xb909066a, 0xb90a2a6a}, {0x4b80ec, 0x9124126d, 0x9128a26d},
        {0x4b81dc, 0xb909058b, 0xb90a298b}, {0x4b81f8, 0xb909055f, 0xb90a295f}, {0x4b8230, 0xb909059f, 0xb90a299f},
        {0x4b8248, 0xb909057f, 0xb90a297f}, {0x4b8274, 0xb909059f, 0xb90a299f}, {0x4b8284, 0xb909055f, 0xb90a295f},
        {0x4b833c, 0xb9490508, 0xb94a2908}, {0x4b87e0, 0xb9490529, 0xb94a2929}, {0x4b9028, 0xb9490508, 0xb94a2908},
        {0x4b94e8, 0xb9490509, 0xb94a2909}, {0x4b94f8, 0x91241117, 0x9128a117}, {0x4b953c, 0xb9490668, 0xb94a2a68},
        {0x4b9b84, 0xb9490508, 0xb94a2908}, {0x4ba410, 0xb9490788, 0xb94a2b88}, {0x4ba428, 0x9124139a, 0x9128a39a},
        {0x4ba4ec, 0xb9490b88, 0xb94a2f88}, {0x4ba518, 0x91242389, 0x9128b389}, {0x4ba55c, 0xb9090b9f, 0xb90a2f9f},
        {0x4ba5b0, 0xb9490788, 0xb94a2b88}, {0x4ba5c8, 0x9124139a, 0x9128a39a}, {0x4ba68c, 0xb9490b88, 0xb94a2f88},
        {0x4ba6a4, 0x91242389, 0x9128b389}, {0x4ba6e8, 0xb9090b9f, 0xb90a2f9f}, {0x4ba86c, 0x91241114, 0x9128a114},
        {0x4ba870, 0xb9490508, 0xb94a2908}, {0x4bb114, 0xb9490508, 0xb94a2908}, {0x4bb47c, 0xb9490508, 0xb94a2908},
        {0x4bc028, 0xb9090515, 0xb90a2915}, {0x4bc958, 0xb909067f, 0xb90a2a7f}, {0x4bc95c, 0xb9090e7f, 0xb90a327f},
        {0x4bc960, 0xb909167f, 0xb90a3a7f}, {0x4bc964, 0xb9091e7f, 0xb90a427f},
        /* pagina kart: [kart+0x3b8] = pagina selezione */
        {0x4c04d8, 0xb9490508, 0xb94a2908}, {0x4c0664, 0xb9490508, 0xb94a2908}, {0x4c0ddc, 0xb9490508, 0xb94a2908},
        {0x4c0e78, 0xb9490508, 0xb94a2908}, {0x4c0f1c, 0xb9490508, 0xb94a2908}, {0x4c0ff0, 0xb9490508, 0xb94a2908},
        {0x4c1564, 0xb949054a, 0xb94a294a}, {0x4c18dc, 0xb9490508, 0xb94a2908}, {0x4c19f8, 0xb9490529, 0xb94a2929},
        {0x4c1ab0, 0xb9490529, 0xb94a2929}, {0x4c1b98, 0xb9490508, 0xb94a2908}, {0x4c27cc, 0xb9490508, 0xb94a2908},
        {0x4c2870, 0xb9490508, 0xb94a2908}, {0x4c2a1c, 0xb949058c, 0xb94a298c}, {0x4c2aa8, 0xb949058c, 0xb94a298c},
        {0x4c3004, 0xb949058c, 0xb94a298c}, {0x4c3138, 0xb9490508, 0xb94a2908}, {0x4c31ac, 0xb9490508, 0xb94a2908},
        {0x4c3210, 0xb9490508, 0xb94a2908}, {0x4c3518, 0xb9490508, 0xb94a2908}, {0x4c3d80, 0xb949058c, 0xb94a298c},
        {0x4c3edc, 0xb949058c, 0xb94a298c}, {0x4c4528, 0xb9490508, 0xb94a2908},
    };                                                        /* 3 + 14 + 35 + 23 = 75 */
    static constexpr uint32_t kCanary = 0xA1A1A1A1;
    static bool m2;                                           /* A1-M2 attivo */
}
```
Totale: 3 fabbriche + 14 regali + 35 classe + 23 kart = **75 parole** [VERIFICATO main.bin]. I 39 clamp (`kClamp904` in
`passo1_table.inc`) **non** fanno parte del passo 1.

### 2.2 Hook e modifiche alla mod
1. **Trampolino sul ctor base 0x4bc844** (prima istruzione `a9be4ff4` `stp x20,x19,[sp,#-0x20]!`, rilocabile; epilogo 0x4bc998 `ret`)
   [VERIFICATO]:
```cpp
HOOK_DEFINE_TRAMPOLINE(CharaSelBaseCtorHook) {               /* 0x4bc844 ctor base selezione (Single/WiFi/Multi), oggetto 0xa68 */
    static void Callback(uint8_t* self) {
        Orig(self);                                          /* azzera +0xa28/+0xa30/+0xa38/+0xa40 (0x4bc958..964 patchati) */
        for (int i = 0; i < 16; i++) *(uint32_t*)(self + 0xa28 + 4 * i) = 0;          /* 8 voci {stato, valore} */
        for (int i = 0; i < 0x20; i += 4) *(uint32_t*)(self + 0x904 + i) = sel8::kCanary;
        static int n; if (n++ < 6) Logging.Log("[mk8dx-8p] M2: ctor selezione %p, coda +0xa28 azzerata, canary +0x904", self);
    }
};
```
2. **Installazione tutto o niente** (in exl_main, prima che il menu costruisca le pagine; le pagine nascono all'ingresso nel menu,
   0x4048b4 → 0x726468 → 0x7240d8):
```cpp
    {   /* step 5b passo 1: A1-M2 rilocazione +0x904 -> +0xa28 (75 parole + ctor), tutto o niente */
        bool ok = *reinterpret_cast<uint32_t*>(base + 0x4bc844) == 0xa9be4ff4;
        for (const sel8::P& p : sel8::kReloc904)
            if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok = false; Logging.Log("[mk8dx-8p] M2: parola inattesa a %x", p.off); }
        if (ok) {
            CharaSelBaseCtorHook::InstallAtOffset(0x4bc844);  /* prima l'hook, poi le patch */
            exl::patch::RandomAccessPatcher rp;               /* niente input::Patch: 75 righe di log */
            for (const sel8::P& p : sel8::kReloc904) rp.Write<uint32_t>(p.off, p.repl);
            rp.Flush();
            sel8::slotOff = 0xa28; sel8::m2 = true;
            Logging.Log("[mk8dx-8p] M2: +0x904 rilocato a +0xa28 (%d parole), oggetto 0xa68", (int)(sizeof(sel8::kReloc904) / sizeof(sel8::P)));
        } else Logging.Log("[mk8dx-8p] M2: rilocazione +0x904 NON attivata");
    }
```
   Se `RandomAccessPatcher` non ha `Flush()` per scritture multiple, usare la stessa sequenza di `input::Patch` (una `Read`
   di verifica è già fatta sopra) [IPOTESI sull'API: copiare da input::Patch righe 1068-1075].
3. **`sel8::SlotState`** (main.cpp:1607): `page + 0x904` → `page + sel8::slotOff`. È l'**unico accesso sfuggito**. Senza questa
   correzione, con N > 4 il turno 2 legge il canary (0xA1A1A1A1 ≠ 7, quindi `confirmed` non arriva mai a 4) e la selezione si blocca
   al turno 1.
4. **Controllo del canary** nel `CharselFrameHook` esistente (0x4ba77c, per frame), in testa e prima di qualunque ritorno:
```cpp
        if (sel8::m2) {
            static bool bad;
            for (int i = 0; i < 0x20 && !bad; i += 4) {
                uint32_t v = *(uint32_t*)(page + 0x904 + i);
                if (v != sel8::kCanary) { bad = true; Logging.Log("[mk8dx-8p] M2: CANARY +0x%x = %08x (accesso sfuggito!) page=%p", 0x904 + i, v, page); }
            }
        }
```
   CharselFrameHook è installato solo con S5.4 (`race8::mapOk`). Altrimenti si usa il watchpoint GDB (§2.3).

### 2.3 Verifica in Eden
1. Log all'avvio: `M2: +0x904 rilocato a +0xa28 (75 parole)`. Nessuna riga `M2: parola inattesa`.
2. Ingresso nel menu: 3 righe `M2: ctor selezione` (pagine Single/WiFi/Multi costruite all'ingresso nel menu).
3. **1P, 2P, 4P**: selezione personaggi, annulla (B) e riscelta, conferma, pagina kart (conferma e annulla, 0x4c2b40), regali o
   sblocchi se presenti, gara, ritorno al menu e nuova selezione. Atteso: comportamento identico al vanilla, nessuna riga `CANARY`.
4. **5-8P (due turni)**: turno 1, poi turno 2, poi kart e gara come prima. Verifica che `SlotState` sia corretto: la riga
   `A2: turno 2: i giocatori 5-…` deve comparire.
5. Watchpoint (se serve, o se S5.4 è assente): con il gioco fermo al menu, `monitor get info` → BASE; indirizzo della pagina 0x2e
   dal log `M2: ctor selezione`; `watch *(int*)(page+0x904)` e `watch *(int*)(page+0x91c)` (i watchpoint funzionano, CLAUDE.md).
   Nessuna scrittura attesa dopo il ctor.
6. Facoltativo: dump di `page+0xa28..+0xa67` nel log del CharselFrameHook ogni 120 frame (stati 3/5/6/7 negli slot 0..3,
   0 negli slot 4..7).

### 2.4 Rischi
| rischio | probabilità | mitigazione |
|---|---|---|
| accesso a +0x904 calcolato a runtime non trovato staticamente (es. puntatore a +0x904 conservato da un'altra classe) | bassa (nessun `add #0x904` fuori lista; i puntatori interni `x23/x26/x20` vivono solo nei blocchi patchati) | canary + watchpoint |
| allocatore 0x5fa960 con blocchi a dimensione fissa o pool per tipo | bassa (riceve solo w0 = dimensione) | 3 righe di log del ctor, nessun crash all'ingresso nel menu |
| hook futuri o esistenti che riscrivono una delle 75 parole | nulla oggi (nessuna collisione) | la verifica tutto-o-niente le rileva |
| codice mod che legge +0x904 aggiunto in futuro | – | usare sempre `sel8::SlotState` / `slotOff` |
| +0x904..+0x923 ora canary: un lettore sfuggito vedrebbe 0xA1A1A1A1 (stato sconosciuto) invece di 0 | voluto | rende visibile l'errore (log) invece di nasconderlo |

## 3. Catalogo aggiornato (riprende blocco_step5.md, tenendo conto dello step 7)

Fatto nello step 7 (pausa), riutilizzabile qui [VERIFICATO in main.cpp]: `BtnScanHook` 0x8b4038 (scanner tasti esteso ai pad 5..7,
vale per **ogni** pagina con scanner, compreso +0x8d0 della selezione); `CursorPollHook` 0x8b0084 (oggi **solo** per Race_Pause 0x6f
con pauser ≥ 4: legge un pad in più su uno slot condiviso, senza estendere il selettore); maschera +0x58 dei controlli (solo L_SwitchBtn);
cursore grafico: frame ColorChange 7..10 = colori P5..P8 via `0x442ca0(obj, frame)`; pagina `cm_Page_Cursor` `0x8bf228()+0x1d0` ptr[4]
con 8 parti nel layout 4.0.1 (04..07 nascoste a 0x49a9cc); `PlayerColorIdxHook` 0x86a720 (colore giocatore in gara, non nel menu).

| passo | componente | stato | cosa resta |
|---|---|---|---|
| 1 | classe selezione +0x904 (A1-M2) | **pronto** (§2): 75 parole + trampolino 0x4bc844 + `slotOff` | prova in Eden; clamp nel passo 4 |
| 2 | pagine CharaWin per slot 4..7 + H-A2-4 | da fare | 4 pagine 0x310 B (fabbrica 0x40918c, ctor 0x4b27c4, +0x1e8 = indice) su id liberi (§4), rimappaggio dei 17 siti `0x28+i` (trampolino 0x8bf0ec con indirizzo di ritorno), link +0x660 ×4 e +0x928 code ×4 rilocati in coda (oltre +0xa68: 8×0x28 + 8×0x28, rifare le fabbriche), clamp +0x928 (0x4b91e4, 0x4bb28c, 0x4bc4f0, 0x4b53c8, 0x4b4f0c) e 0x4bc630 (+0x660) |
| 3 | selettore cursori (0xb8 B, 4 slot a +0x28+s*0x20) + PageMgr +0xc68 ptr[4] + cursori grafici 04..07 | parziale: lo step 7 legge un pad in più (0x8b0084) ma **non** estende gli slot; scanner già esteso | rilocare l'oggetto selettore (8 slot, +0xa8/+0xac/+0xb0 spostati), riscrivere 0x8b0084 (pad 0..7 invece di 0..3 + hook) e 0x8b01f8-0x8b0224, 19 clamp in 0x8ae1e4..0x8b16b4; PageMgr: REPLACE 0x8bd5f8/0x8bd6d8 (focus per pad a 8); cursori grafici: creare 04..07 invece di nasconderli (0x49a9cc) e colore frame 7..10 alla creazione (0x49a9c4); i 4 clamp condivisi di §1.4 (0x4b1d20, 0x4ba404, 0x4ba5a4, 0x4c18d0) diventano sicuri qui |
| 4 | +0x8c0 = N | da fare | trampolino su 0x4bcc84 (prima di `Orig`), ≤ w20 di CharaParts; ora i **39 clamp** #4→#8 (`kClamp904`); tocco già a bit 8 (C2 fatto: 0x4b82a4..); verificare le maschere +0x1a8 bit 4..7 e la rimozione della logica a due turni (CharselFrameHook) quando N ≤ +0x8c0 |
| 5 | pagina kart | da fare | +0x39c = N (0x4c4c70), +0x460 [4] rilocato (8 voci; clamp condivisi 0x4c154c, 0x4c2a10, 0x4c2a9c, 0x4c2ff8, 0x4c3d74, 0x4c3ed0), azzeramento srotolato 0x4c157c, `cmp w11,#4` 0x4c16d4, selettori L_KartSlot_04..07, 0x49ae1c pannelli [4] |
| 6 | config per giocatore (13 int[4] cfg+0x1fb4..+0x2074) | da fare (oggi i due turni scrivono gli slot 0..3 due volte e l'entry del locale 4+i via 0x4203e0) | array ombra persistente nel modulo per i = 4..7 + hook su lettori e scrittori (~50 clamp; condivisi 0x4b8330, 0x4b9b78); salvataggio/caricamento come per gli aiuti (cfg+0x2310.. senza clamp: attenzione) |
| 7 | layout 8pl + pagina 0xe (anteprime 3D) | layout caricabile (A1-M1, `MK8DX_LYT8=8`), pannelli statici | pagina 0xe +0x1d0 ptr[4] (0x49ae1c) → 8 pannelli 3D, camere; semantica sconosciuta |
| 8 | palette a 8 | parziale: colori P5..P8 noti (frame ColorChange 7..10, `cm_L_BtnCursor`) | L_Name colori (0x4bed2c `and w20,#7` solo 2/4), targhette N_Player_04..07, cursori 04..07 (passo 3) |
| – | bit 4 di +0x1a8 (tocco) | **fatto** (C2: GetPad(4)→GetPad(8) a 0x4b82a4/0x4bbcdc/0x4c17c8/0x4c3c98) | – |

## 4. Passo 2 – preliminare: id liberi per 4 pagine CharaWin

Tabella `0x8bcc5c()`: +0x8+id*8 fabbrica, +0x448+id*8 proprietario, 0x88 voci; ogni registrazione è limitata con
`cmp w9,#0x88; csel → 0` (es. 0x404d68) [VERIFICATO].
Registrazioni con id costante (`bl 0x8bcc5c; mov w8,#id`, `page_registrations.txt`) [VERIFICATO]: boot/menu 0x4026..0x405e
(id 0x03..0x63 salvo eccezioni), gara 0x4060..0x4079 (0x6e..0x86 salvo eccezioni). **Mai registrati con costante**: 0x00, 0x01,
0x02, 0x0a, 0x0c, 0x12, 0x1d, 0x43, 0x5b, 0x5e, 0x64..0x6d, 0x73, 0x75, 0x79, 0x7e, 0x87. Il log Eden (step 6, `H2: id pagina
senza fabbrica`, preso in gara) mostra però che 0x0a/0x0c hanno una fabbrica (registrazione con id non costante) e che 0x65..0x68
(RaceView0..3) sono registrati da un ciclo [VERIFICATO log, step6_data].

Nomi (img/pages.txt, id = riga-2) e disponibilità **nel menu**:
| id | nome | stato |
|---|---|---|
| 0x00 | DebugMenuSeq / Invalid | evitare (0 = id nullo nei clamp `csel → 0`) |
| 0x01, 0x02 | DebugMessage, DebugViewer | liberi, debug [VERIFICATO statico + log] |
| 0x5e | WiFi_CompeCreateConfirm | libero nel retail [VERIFICATO]; serve che nessun flusso WiFi lo apra [IPOTESI: pagina rimossa] |
| 0x12, 0x1d, 0x43, 0x5b | DebugLocalRoom, Lyt_GhostScrollList, WiFi_Rule, WiFi_CompeCreateDescription | liberi nel retail, ma la **mod li usa in gara** (`hud8::kIds`/`kIdsAlarm`, RacePagesSetupHook 0x406870 scrive fabbrica e proprietario) |
| 0x64, 0x69..0x6d, 0x73, 0x75, 0x79, 0x7e, 0x87 | TrialTitle, Race, RaceViewFront0..3, Race_Watch, Race_Title, TrialThankyou, Theater_Result, Viewer | pagine di gara o trial: non registrati nel menu con costante, ma alcuni lo sono in gara (0x69 Race, 0x75 Race_Title = `rid` di RacePagesSetupHook) |

Quindi i soli id **certamente** liberi in entrambe le scene sono **3** (0x01, 0x02, 0x5e): non bastano per 4 pagine.
Opzioni [IPOTESI, da decidere nel passo 2]:
- (a) riusare 0x12/0x1d/0x43/0x5b anche nel menu. È possibile solo se la tabella viene ripopolata a ogni scena, oppure se la mod
  riscrive fabbrica e proprietario a ogni ingresso nel menu e li ripristina all'uscita (oggi RacePagesSetupHook non ripulisce: in un
  menu successivo a una gara resterebbero fabbrica RaceView e proprietario = scena di gara liberata). Da verificare con un dump della
  tabella all'ingresso nel menu, dopo una gara (estendere il log `H2: id pagina senza fabbrica` a CharselEnterHook).
- (b) 0x01, 0x02, 0x5e + uno fra 0x64 (TrialTitle) / 0x79 (TrialThankyou) / 0x87 (Viewer): pagine trial/debug mai registrate dal
  retail. Va verificato a runtime che restino senza fabbrica nel menu.
- (c) evitare la tabella: le pagine CharaWin 4..7 come oggetti della mod, raggiunte solo dal trampolino su 0x8bf0ec (rimappaggio per
  indirizzo di ritorno dei 17 siti). Restano necessari un id per i link (+0x660, 0x8ba5fc) e il PageMgr, quindi probabilmente non
  evita (a)/(b).
Raccomandazione preliminare: (b) con 0x01, 0x02, 0x5e, 0x87, dopo un dump di verifica della tabella nel menu (prima e dopo una gara).
