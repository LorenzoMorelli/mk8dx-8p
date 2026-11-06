# Step 5 – A1: classe selezione personaggi e caricamento del layout 6pl/8pl (piano statico)

Main 4.0.1 (BID 158C0C73…), offset = main decompresso `../img/main.bin`; IPS = offset + 0x100.
[VERIFICATO] = letto nel disassemblato in questa sessione (indirizzi citati); [IPOTESI] = dedotto;
[DA VERIFICARE A RUNTIME] = da controllare in Eden. Nessuna modifica al codice del progetto.
Dati grezzi in `step5_data (dati non pubblicati)/a1_*`:

| file | contenuto |
|---|---|
| `a1_layout_load.asm` | 0x4be854, tabella 0x12797f0, slot 13/14/15, "%s.bflyt", onCreate CharaParts 0x4be8d0, 0x8bbf04/0x5917a4/0x591a9c, FindPane |
| `a1_crash_4bd9a4.asm` | catena del crash di ips_B, funzione 0x4bd9a4 (bind della parte L_Name), deref non controllata 0x4bdf30 |
| `a1_904_reloc.txt` | 72 accessi all'array +0x904 (charasel, pagina kart, pagina regali) e 39 clamp, con parola originale e nuova |
| `a1_slot_limits.txt` | contatori di slot, +0x660, pagine 0x28+i (17 siti), code +0x928, array per giocatore della config |
| `a1_fields_660_a28.txt` | istogramma degli accessi della classe agli offset +0x660..+0xa28 |

## 0. Sintesi

1. **La pagina che carica il layout non è la classe da 0xa28 B.** Il layout lo carica la pagina **CharaParts**
   (id 0x14/0x15/0x16, classe ctor 0x4be890, vptr 0x1279040). Il suo slot 13 (0x4be854) restituisce il nome
   `mn_Page_Lyt_CharaParts_{Single,WiFi,Multi}`. La classe da 0xa28 B (pagina id 0x2c/0x2d/0x2e) ha come nome
   `cm_Page_Dummy` (slot 13 = 0x8baa34). Lavora sulla parte della pagina CharaParts (`[+0x728]+0x1d0`).
2. **`mn_CharaKartSlct_Multi` (0xeeb5b4) è il nome di un pane, non un nome di file.** 0x4be8d0 lo passa a
   0x8bbf04, che cerca il pane Parts con quel nome nel layout della pagina (0x5917a4, confronto su 0x18 byte).
   Il file della parte si risolve dal `prt1` del Page_Lyt ("%s.bflyt", 0x591a9c). In
   `mn_Page_Lyt_CharaParts_6pl/8pl` il pane si chiama ancora `mn_CharaKartSlct_Multi` e punta ai file
   `mn_CharaKartSlct_6pl/8pl`. **Basta quindi cambiare il nome del Page_Lyt (hook dello slot 13). La stringa
   0xeeb5b4 non va toccata.**
3. **Crash di ips_B spiegato** [VERIFICATO statico, PC esatto DA VERIFICARE]. ips_B rinominava anche 0xeeb5b4.
   Il pane non veniva trovato, quindi la parte restava senza layout e lo stesso valeva per le parti figlie
   L_Name_0i. Il bind di L_Name (0x4bd9a4, raggiunto da 0x8ae384 → slot 6 → thunk 0x4be078) cerca
   `T_Name_00`, ottiene NULL e lo dereferenzia senza controllo a **0x4bdf30**. "0x4bd9dc" è l'inizio di
   funzione (sbagliato) che fstart.py attribuisce a quel PC: l'istruzione a 0x4bd9dc è un `orr`, non può
   fare fault.
4. **Con il nome giusto (H-A1a) e w20 = 6/8 (H-A1b), il layout 6pl/8pl si carica senza crash statici noti.**
   Tutti i pane cercati per nome esistono nei file 6pl/8pl (L_Name_0i, L_Chara_00..47, B_MP_00,
   N_ConSuppo_00, T_Name_00 in `mn_L_SlctName_02`) oppure sono cercati in modo null-safe
   (L_CameraPlay_0i, L_Entry_0i).
5. **Portare a 6/8 gli slot *attivi* (+0x8c0) è molto più che rilocare +0x904.** Sono dimensionati a 4:
   - +0x904 (4×8): 72 accessi e 39 clamp, sparsi in **tre** classi (selezione, kart via `[+0x3b8]`, regali);
   - +0x928 (4 code da 0x28 B);
   - +0x660 (4 transizioni verso le pagine 0x28..0x2b);
   - le **pagine finestra personaggio per giocatore 0x28..0x2b**: 17 siti calcolano `0x28+i`, e con i ≥ 4
     ottengono una pagina sbagliata e poi NULL;
   - **7 array int[4] per giocatore nella config** (cfg+0x1fb4..+0x2014).

   Il clamp ips_A 0x4bc630 era sbagliato: indicizza +0x660, non +0x904.
6. **Piano consigliato in due tappe.**
   - **A1-M1** (subito, basso rischio): H-A1a + H-A1b + log. Il layout 8pl/6pl appare con 4 slot attivi
     (+0x8c0 = 4) e kart a 4.
   - **A1-M2**: rilocazione di +0x904 (114 parole + 3 fabbriche + hook ctor). Va messa insieme alle
     estensioni di +0x928, +0x660, delle pagine 0x28+i e della config prima di alzare +0x8c0. Prerequisito:
     A2/A3 per pagine e config.

## 1. Caricamento del layout

### 1.1 Catena [VERIFICATO]
```
pagina CharaParts (id 0x14 Single / 0x15 WiFi / 0x16 Multi = [this+0xe8])
  vt slot 9  0x7240d8 init: x21 = vt+0xc0 (accessor, slot 24 = 0x8bab3c)
             0x724254 ldr [vt+0x68] (slot 13) -> nome ; 0x724260..0x724270 SafeString::format("%s.bflyt" = 0xef5f56)
             0x724278 vt+0xd0 (slot 26) build del layout con l'accessor
  vt slot 13 0x4be854: w8 = [x0+0xe8]-0x14 ; >2 -> NULL ; altrimenti ldr x0,[0x12797f0 + w8*8]
  vt slot 14 0x72561c e slot 15 0x725628: ldr x1,[vt+0x68]; br x1  (inoltro a slot 13)
  vt slot 73 0x4be8d0 onCreate: per lo stato 0x16 alloca la parte (0xb8 B, GOT 0x13093a8) e
             0x8bbf04(page, part+0x30, &{"mn_CharaKartSlct_Multi"}, &0) ; [+0x1d0] = parte ; w20 = 4 (0x4bea58)
```
- **Tabella 0x12797f0**: tre `char*` (0xeeb52f, 0xeeb54d, 0xeeb569) con rilocazioni RELATIVE a
  0xd845a0/0xd845b8/0xd845d0. Sono puntatori a stringhe, non a strutture. Nessun altro riferimento alle tre
  stringhe né alla tabella (xref adrp: nessuno; ricerca qword: solo tabella e rilocazioni).
- **Accessor**: 0x8bab3c crea l'accessor (0x50 B, risorse globali 0x8b6d60/0x8b36cc). Poi chiede slot 15
  (`vt+0x78`) e aggancia l'archivio con quel nome (`vt+0x40(&name)` di 0x8b6d60(), poi 0x733c20). Lo stesso
  nome via slot 15 è usato a 0x7257c4; via slot 14 a 0x7258a0 (`[[x21+0x20]+0xb30]`). Sono tutte ricerche di
  risorse per nome dell'archivio/layout della pagina.
- **Directory archivi**: 0x404410 registra `UI/cmn/menu` (0xee76c5) e `UI/%s/menu` con 0x8b5b00. Gli archivi
  `<nome>.szs` della RomFS (`/UI/cmn/menu/mn_Page_Lyt_CharaParts_8pl.szs` ecc., step6_romfs) si caricano per
  nome [IPOTESI forte: niente lista statica di nomi nel main; i file 6pl/8pl non hanno stringhe nel main
  eppure ips_B ha superato il caricamento ed è caduto nel bind].

### 1.2 Risoluzione delle parti (prt1) [VERIFICATO]
- 0x8bbf04: se il nome non è vuoto, `0x8bb01c(page, page+0x28, &name)`. Questa visita le parti del layout
  (0x5917a4: lista +0x48/+0x50, confronta 0x18 byte con `node-0x28`, restituisce `node-0xe0` = pane Parts) e
  ricorre nei sotto-layout. Il layout della parte è `pane+0xf0` (con controllo RTTI 0x130daa0). Se il pane
  non c'è, `x22 = 0` e `0x8ad8e0` salva handle+0x10 = NULL. **Nessun errore: la parte resta vuota.**
- 0x8bc0c4 fa lo stesso per le parti figlie (genitore = handle di una parte; 0x8bb01c con layout NULL → cbz →
  NULL).
- I file dei sotto-layout si costruiscono in nw::lyt: 0x591a9c/0x591b98 fanno `SNPrintf("%s.bflyt", nome
  dal prt1)` e `accessor vt+0x40('blyt', nome)`.
- Page_Lyt (step6_panes_401 righe 62/66/72 e `.str`): Multi = `RootPane, mn_CharaKartSlct_Multi` → file
  `mn_CharaKartSlct_Multi`; 8pl = stesso nome di pane → file `mn_CharaKartSlct_8pl`; 6pl idem.

### 1.3 Altri usi del nome come chiave [VERIFICATO]
| chiave | dove | effetto della sostituzione con 6pl/8pl |
|---|---|---|
| nome Page_Lyt (slot 13) | 0x7240d8 (file .bflyt), slot 14/15 → 0x8bab3c/0x7257c4/0x7258a0 (archivio/risorsa) | coerente: tutti passano dallo slot 13 |
| `mn_CharaKartSlct_Multi` (pane) | solo 0x4be9d8 (onCreate CharaParts) | **non cambiare**: il pane ha lo stesso nome nei tre Page_Lyt |
| `mn_CharaKartSlct_MultiBG` | 0x4bf144 (slot 13 della pagina BG 0x19) | resta MultiBG (non esiste un 8plBG); indipendente [IPOTESI] |
| L_Entry_0i, L_Name_0i, L_CameraPlay_0i | 0x4be8d0, conteggio w20 | §4 |
| animazioni (`In`,`Out`,`Color`,`Loop`,`Ptn`,`In_Win`…) | 0x8ad158 per nome, null-safe se manca la risorsa | i nomi nei FLAN 8pl/SlctName_02 non sono stati confrontati [DA VERIFICARE A RUNTIME: nessun crash, eventualmente animazione ferma] |
Nessuna cache di layout indicizzata per nome è stata trovata nel percorso. Il nome è un `const char*` letto a
ogni build.

### 1.4 Proposta H-A1a – nome del Page_Lyt (trampolino su 0x4be854)
Firma: `const char* GetLayoutName(Page* this)` (slot 13, foglia, prima istruzione `d10043ff sub sp,sp,#0x10`).
```cpp
namespace sel8 {
    static constexpr const char* kPageLyt6 = "mn_Page_Lyt_CharaParts_6pl";
    static constexpr const char* kPageLyt8 = "mn_Page_Lyt_CharaParts_8pl";
    /* capacità del layout: 1-4 -> 4 (vanilla), 5-6 -> 6, 7-8 -> 8 */
    static int Slots() { int n = menu8::LocalCount(); return n >= 7 && n <= 8 ? 8 : (n >= 5 ? 6 : 4); }
}
HOOK_DEFINE_TRAMPOLINE(CharaPartsLayoutNameHook) {           /* 0x4be854, slot 13 pagina CharaParts */
    static const char* Callback(uint8_t* page) {
        const char* r = Orig(page);
        if (page && *(int32_t*)(page + 0xe8) == 0x16) {       /* solo Multi */
            int s = sel8::Slots();
            if (s == 8) r = sel8::kPageLyt8; else if (s == 6) r = sel8::kPageLyt6;
            static const char* last;
            if (r != last) { last = r;
                Logging.Log("[mk8dx-8p] A1: layout CharaParts = %s (N=%d)", r, menu8::LocalCount());
                mk8dx::LogBacktrace("A1 layout", __builtin_frame_address(0), 6); }
        }
        return r;
    }
};
/* exl_main: CharaPartsLayoutNameHook::InstallAtOffset(0x4be854); */
```
Le stringhe stanno nel `.rodata` del modulo e vivono per tutto il processo. Lo slot 13 è chiamato più volte
(0x724254 e gli inoltri 14/15): il valore restituito è identico a ogni chiamata finché N non cambia.
**Rischio principale** [DA VERIFICARE A RUNTIME]: *quando* si costruisce la pagina 0x16. Le pagine si
registrano in 0x40476c (fabbriche da 0x1307ad0.., chiamate 0x8b3cb0/0x8bcc5c). Se il layout si costruisce una
volta sola (all'ingresso del menu) e non a ogni ingresso nella selezione, al momento del build N vale il
valore precedente. Il log con backtrace di H-A1a lo dice subito. In quel caso servirebbe forzare la
distruzione e ricostruzione delle pagine 0x16/0x2e (fuori da A1).

## 2. Perché ips_B crasha [VERIFICATO statico]

*(estratto di disassemblato del gioco rimosso)*
- "Prepare Thread" coincide con l'onCreate della pagina (thread di preparazione). "Pannello nullo" coincide
  con `T_Name_00`. fstart.py dà 0x4bd9dc come inizio funzione per 0x4bdf30 (a1_crash_4bd9a4.asm), ma la
  funzione inizia a 0x4bd9a4.
- Anche con `menu_8pl_mod.sarc` la stringa del pane restava rinominata, quindi il crash è lo stesso.
- Con il nome di pane originale `T_Name_00` esiste in `mn_L_SlctName_01` (Multi) e in `mn_L_SlctName_02`
  (usato da 6pl/8pl; panes in `romfs-tmp/menu4/*.panes`).
- **Correzione minima: nessuna patch sul codice.** Non riscrivere 0xeeb5b4; cambiare solo il nome del
  Page_Lyt (H-A1a). Non servono pane fittizi né `cbz`.

### 2.1 Ricerche per nome su pane presenti nel Multi ma assenti in 6pl/8pl [VERIFICATO]
Differenze di pane (step6_panes_401 righe 45/50/55): il 6pl/8pl **non** hanno `L_CameraPlay_00..03`, mentre
hanno N_Player/L_KartSlot/L_KartPartsOKWin/L_Name/N_Window per 6/8 slot. `L_Entry_0i` manca in tutti e tre.

| nome | sito | controllo | esito con 6pl/8pl |
|---|---|---|---|
| L_CameraPlay_0i (i < w20) | 0x4bee5c..0x4beeb0: 0x8ad8e8 → 0x8ade74(pane, 0) | 0x8ad8e8 restituisce 0 se layout/pane mancano; 0x8ade74 `cbz x0` | nessun effetto |
| L_CameraPlay_00 | 0x4e4f7c, 0x52d10c, 0x5488ac | altre pagine (non usano la parte CharaKartSlct) | – |
| L_Entry_0i (stato 0x15/0x16) | 0x4beb68: 0x8bc0c4 | parte vuota, anche nel Multi vanilla | invariato |

Nessun altro nome assente: L_Chara_00..47 (0x4b56c0, 48 in tutti e tre), B_MP_00 (0x4b582c → 0x434a2c con
`cbz x1`), N_ConSuppo_00 (0x4bf0a4, null-safe), L_KartSlot_%02d / L_KartPartsOKWin_%02d (pagina kart
0x4bfdd0, presenti 0..7). N_Player_0i e N_Window_0i della CharaKartSlct non sono cercati dal codice (gli
unici xref a "N_Player_00"/"N_Window_00", 0x469700/0x442c00/0x46970c, sono di altre classi): li muovono solo
le animazioni del layout [IPOTESI].

## 3. Struttura della classe selezione personaggi (alloc 0xa28)

### 3.1 Gerarchia e vtable [VERIFICATO]
- Fabbriche 0x4091b8/0x4091e4/0x409210 (`mov w0,#0xa28` a 0x4091c4/0x4091f0/0x40921c, operator new 0x5fa960,
  poi ctor 0x4bc80c/0x4bcad8/0x4bcc4c), registrate come pagine 0x2c/0x2d/0x2e in 0x40476c
  (0x404e78/0x404ec8/0x404f18).
- Ctor base 0x4bc844 (vtable GOT 0x1309338 → 0x1278c28, vptr 0x1278c38). Single +0x8c0 = 1 (0x4bc830), WiFi
  2 (0x4bcafc), **Multi 4 (0x4bcc70 `321e03e8`)**.
- Slot (vptr): 13 = 0x8baa34 → `"cm_Page_Dummy"` (0xf0d48b), quindi niente layout proprio; 73 = onCreate
  (Single 0x4bc99c, WiFi 0x4bcb10, **Multi 0x4bcc84**, base 0x4b5688); 84 = 0x4ba77c (= "slot 86" dello step
  4, che contava dall'inizio del simbolo vtable).
- Multi onCreate 0x4bcc84: +0x728 = pagina 0x16 (CharaParts, cast RTTI 0x1308d00), +0x730 = pagina 0x19
  (BG), poi 0x4b5688. Questo crea 48 parti L_Chara_%02d (0x450 B) sotto `[[+0x728]+0x1d0]+0x30`, in
  +0x740[48], e lega B_MP_00 (+0x738).

### 3.2 Mappa dei campi rilevanti (a1_fields_660_a28.txt) [VERIFICATO]
| offset | tipo | significato | per slot? |
|---|---|---|---|
| +0x1a8 | u32 | maschera pad UI (base pagina): bit i per slot (0x4b80c4..), bit 4 = pad speciale (0x4b57b4 `orr #0x10`) | bit, u32 basta |
| +0x660/+0x688/+0x6b0/+0x6d8 | 4 × 0x28 | transizioni verso le pagine 0x28..0x2b (0x4b7a38..0x4b7a94); +0x700 quinta transizione | **sì, 4** |
| +0x728/+0x730/+0x738 | ptr | pagina CharaParts 0x16, pagina BG 0x19, oggetto B_MP | no |
| +0x740 | ptr[48] | parti L_Chara (personaggi, non giocatori) | no |
| +0x8c0 | s32 | numero di slot (34 letture) | contatore |
| +0x8c4/+0x8c5/+0x8c8 | u8/u8/s32 | stato pagina | no |
| +0x8d0 | oggetto 0x8b4010 | ricevitore input (0x8bb2b8), +0x8f0 = 4 (0x4b5794) = pad UI 4 [IPOTESI] | no |
| +0x900 | s32 | primo slot libero/cursore (-1) | indice |
| **+0x904** | 4 × {s32 stato, s32 valore} | stato per slot: 3 = slot di un giocatore locale (i < N, 0x4b80c4), 1 = slot oltre N (0x4b81dc), 0 = oltre +0x8c0 (0x4b81f8..); ≥ 5 apre la pagina 0x28+i (0x4ba874), 7 = confermato (0x4ba914) [significati IPOTESI] | **sì, 4** |
| **+0x928** | 4 × 0x28 | coda per slot: +0 ptr→dati inline (+0x14), +8 capacità 2, +0xc testa, +0x10 conteggio (ctor 0x4bc924..0x4bc988) | **sì, 4** |
| +0x9c8..+0x9d0 | u8 | flag | no |
| +0x9cc | s32 | slot corrente (0x4b94d8, clamp 0x4b94dc) | indice |
| +0x9d8 | ptr | oggetto 0x20 B (0x3fce0c) | no |
| +0x9e0..+0xa20 | 9 × s32[2] | copia per pad 0..1 (ciclo 0x4b77c0 `cmp x26,#1`, clamp 2 a 0x4b7894) di cfg+0x1fb4.. | solo 2 pad: **non** da estendere |

### 3.3 Array +0x904: rilocazione [VERIFICATO statico]
Elenco completo con parole in `a1_904_reloc.txt`: **72 accessi** e **39 clamp**.
- **Accessi**: 14 nella pagina regali (13 srotolati a 0x4b111c..0x4b11cc, più 0x4b1d2c), 35 nella classe e
  23 nella pagina kart (`ldr x8,[x19,#0x3b8]` = pagina 0x2e).
- **Clamp**: i 17 di ips_A meno 0x4bc630 (16, compresi 0x4b1d20 della pagina regali e 0x4b80f4) più i 23
  della pagina kart (0x4c04cc..0x4c451c): 16 + 23 = 39.

Le forme da patchare:
- `ldr/str wT,[xB,#0x904]` → `#0xa28` (es. `b9490508 → b94a2908`), `#0x908 → #0xa2c`; srotolati `#0x90c/#0x914/#0x91c → #0xa30/#0xa38/#0xa40`;
- `add xD,xB,#0x904` → `#0xa28` (`91241114 → 9128a114` ecc.), `add …,#0x908 → #0xa2c`;
- clamp `cmp xI,#4` → `#8` (`f100129f → f100229f`, `7100111f → 7100211f` …);
- fabbriche `52814500 → 52814d00` (0xa28 → 0xa68) a 0x4091c4/0x4091f0/0x40921c;
- **hook dopo il ctor base 0x4bc844**: azzerare +0xa28..+0xa67. Il ctor azzera solo +0 delle 4 voci vecchie
  (0x4bc958..0x4bc964); l'allocatore non garantisce memoria azzerata [IPOTESI].

Valutazione:
- **In coda (+0xa28)** è l'unica posizione sicura. Dopo +0x923 c'è subito +0x928 (puntatori interni delle
  code): non si può allargare sul posto. Tutti gli immediati restano codificabili (imm12: 0xa40/4 = 0x290;
  add 0xa2c < 0x1000).
- Alternativa "hook delle ~30 funzioni": peggiore (logica da riscrivere).
- Alternativa "indice ≥ 4 in un array ombra del modulo": impossibile con le patch di immediati, perché
  l'indirizzo è `base + i*8 + imm`.
- Applicazione **tutto o niente**: verificare prima tutte le 114 parole e poi patchare (tabella in C++, §5).
  Con la rilocazione attiva e +0x8c0 = 4 il comportamento resta identico al vanilla (stesse 4 voci, solo
  spostate). Questo permette di provarla isolata in 1-4P.
- Vecchia area +0x904..+0x923 dopo la rilocazione: inutilizzata. Si può riempire con un canary e
  controllarla a runtime (o watchpoint GDB) per scoprire accessi sfuggiti [DA VERIFICARE A RUNTIME].

### 3.4 Gli altri limiti a 4 (a1_slot_limits.txt) [VERIFICATO salvo nota]
| limite | siti | perché blocca +0x8c0 > 4 | proposta | rischio |
|---|---|---|---|---|
| +0x660 transizioni (4+1) | 0x4b7a38..0x4b7a94 (srotolato), 0x4bc630 clamp → `add x1,x8,#0x660` | con i ≥ 4 `0x8ba5fc` riceverebbe +0x700/+0x728.. | rilocare in coda insieme a +0x928 (8 × 0x28) con init nel hook ctor, oppure clamp invariato + hook di 0x4bc61c per i ≥ 4 | alto |
| pagine finestra 0x28..0x2b | 17 siti `add wX,wY,#0x28` → `bl 0x8bf0ec` (0x4b4a30 … 0x4bc59c) | i ≥ 4 → id 0x2c.. (pagine selezione) → cast NULL → es. 0x4ba8d8 `ldr w8,[x8,#0x120]` | 4 pagine 0x310 B nuove (fabbrica 0x40918c, ctor 0x4b27c4, +0x1e8 = indice) su id liberi + hook inline sui 17 siti (id 0x2c+k → nuovo id) | alto (A2) |
| +0x928 code (4 × 0x28) | ~35 accessi (madd #0x28), clamp 0x4b91e4, 0x4bb28c, 0x4bc4f0, anche dalla pagina finestra 0x4b4f0c, 0x4b53c8 | indice ≥ 4 → 0 (codice) o fuori array (con #8) | rilocare in coda (8 × 0x28 = 0x140) **riscrivendo il puntatore +0 di ogni voce** | alto |
| config cfg+0x1fb4/+0x1fc4/+0x1fd4/+0x1fe4/+0x1ff4/+0x2004/+0x2014 (7 × int[4]) | ~50 siti (`cmp #4; csel`), scrittori 0x4b7770, 0x4b9db8, 0x4b9eb4, 0x4bbf2c, 0x4bbfc0, 0x4bc238, 0x4c1df0/0x4c1ea4/0x4c1f58 | i ≥ 4 → indice 0: le scelte dei giocatori 5-8 sovrascrivono quelle del giocatore 1 | array ombra nel modulo, oppure ingrandire la config (A3: chi copia queste scelte nel raceMgr per la gara) | alto |
| CharaParts L_Name (dinamico, conteggio w20) | 0x4b7cdc/0x4b50b8/0x4b54f8: `cmp idx,count; b.ls → NULL`, poi **0x4be3b0 non null-safe** (0x4be3c0 `ldr x8,[x19,#0x70]`) | se +0x8c0 > w20, deref NULL | invariante: **+0x8c0 ≤ w20** | medio |
| pagina kart +0x39c | 0x4c4c70 (Multi = 4) e 25 accessi a `[+0x3b8]+0x904` | slot kart 4..7 | dopo M2: +0x39c = Slots() nell'onCreate kart (0x4c4cxx → 0x4bfdd0) | medio |
| 0x422efc (4 iterazioni), pad speciale 0x4b82a4 `orr w0,wzr,#4`, pad UI 4..7 | step 4 §5.2 | input dei giocatori 5-8 | step 5.2 | – |

Nessun altro array per slot nella classe, con una correzione rispetto allo step 1. **+0x218 (stride 8, conteggio
fino a 8) e +0x2a8/+0x268 (stride 16) di 0x4b11cc/0x4b19a4 appartengono alla pagina "regali/sblocchi"**
(0x4b09dc: `mn_L_CharaSlct_Gift_00_0`, `PresentBox`, `UnlockChara`, `mn_GiftBG_00`, modelli 3D 0x8ca144).
Quella pagina crea fino a 8 regali (x22 = 8 se +0x208 == 0, 0x4b0b9c) e nasconde quelli ≥ 4 (0x4b0ba8
`cmp x23,#4` → 0x725ad0 + SetVisible off). Quindi le sue strutture sono dimensionate a 4 + 4 nascosti e non
sono slot giocatore. Va solo rilocata la sua lettura di +0x904.

## 4. Pane per slot [VERIFICATO]

| chi | nome (costruito copiando "X_00" e cambiando l'ultima cifra) | iterazioni | dove sono memorizzati | per 8 slot |
|---|---|---|---|---|
| CharaParts 0x4be8d0 | L_Entry_%02d (0x4beb20) | w20 (0x4bea58 = 4) | PtrArray +0x1d8/+0x1e0 (capacità w20, 0x4beb14) | w20 = 8 (assenti, null-safe) |
| CharaParts 0x4be8d0 → 0x4bef20 | L_Name_%02d (0x4bec94) | w20 | PtrArray +0x1e8/+0x1f0 (capacità w20, 0x4bec88) | w20 = 6/8: presenti 0..5 / 0..7 |
| CharaParts 0x4be8d0 | colori L_Name (0x4bed2c `and w20,#7`: solo 2 o 4) | – | – | con 6/8 nessun colore impostato [DA VERIFICARE visivo] |
| CharaParts 0x4be8d0 | L_CameraPlay_%02d (0x4bee60) | w20 | nessuno (solo SetVisible off) | assenti, null-safe |
| CharaParts 0x4be8d0 | +0x1a8 maschera | w20 > 3 → `orr #0xf` | – | bit 4..7 non impostati (u32 ok) |
| pagina kart 0x4bfdd0 | L_KartSlot_%02d (0x4bfe90), L_KartPartsOKWin_%02d (0x4c0178), L_Body/L_Tire/L_Glider_00 | +0x39c (Multi 4, 0x4c4c70) | PtrArray +0x3c0/+0x3d0/+0x3e0/+0x3f0/+0x450 (capacità +0x39c) | +0x39c = 6/8 dopo M2 |
| classe selezione 0x4b5688 | L_Chara_%02d (personaggi) | 48 fissi | +0x740[48] | invariato |
| N_Player_0i, N_Window_0i | nessuna ricerca nel codice | – | – | solo layout/animazioni |

Il pattern del nome (`"L_Name_0" + ('0'+i)`, 0x4becbc) regge fino a 9 slot: 8 va bene.

### 4.1 Proposta H-A1b – w20 della pagina CharaParts (hook inline a 0x4beb04)
0x4beb04 (`91076260 add x0,x19,#0x1d8`) è il punto di confluenza dopo i tre rami (0x4be9bc/0x4bea5c/0x4beafc)
e prima di qualsiasi uso di w20. x19 = pagina.
```cpp
HOOK_DEFINE_INLINE(CharaPartsSlotCountHook) {                 /* 0x4beb04, onCreate CharaParts */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* page = (uint8_t*)ctx->X[19];
        if (*(int32_t*)(page + 0xe8) != 0x16) return;          /* solo Multi */
        int s = sel8::Slots();
        if (s > 4 && ctx->W[20] == 4) {
            ctx->W[20] = s;
            Logging.Log("[mk8dx-8p] A1: CharaParts slot %d (N=%d)", s, menu8::LocalCount());
        }
    }
};
/* exl_main: CharaPartsSlotCountHook::InstallAtOffset(0x4beb04); */
```
Deve usare lo **stesso** `Slots()` di H-A1a, con lo stesso N (layout 6pl con w20 = 6, 8pl con 8). Se i due
valori divergono, 0x4bef20 cerca L_Name_06/07 nel 6pl: pane assente, quindi T_Name_00 NULL e lo **stesso
crash di ips_B**. Per sicurezza Slots() si calcola una volta in H-A1a, si memorizza (`sel8::built`) e H-A1b
usa quel valore.

## 5. Sintesi: modifiche ordinate

### A1-M1 – layout 6pl/8pl con 4 slot attivi (consigliato subito)
1. **H-A1a** trampolino 0x4be854 (§1.4): `const char* (uint8_t* page)`. Per lo stato 0x16 e N 5-6/7-8
   restituisce `mn_Page_Lyt_CharaParts_6pl/8pl` e memorizza `sel8::built = 6/8`.
2. **H-A1b** inline 0x4beb04 (§4.1): `w20 = sel8::built` se lo stato è 0x16 e `built > 4`.
3. **Nessuna** patch su 0xeeb5b4 / 0xeeb569; nessuna patch su +0x8c0 (resta 4 a 0x4bcc70) né su +0x39c.
4. Log: H-A1a (nome, N, backtrace), H-A1b. Facoltativo: trampolino su 0x4be8d0 che dopo `Orig` registra
   `[+0x1d0]` e `*(u64*)([+0x1d0]+0x40)` (layout parte ≠ 0), `[+0x1e8]` (numero di L_Name) e, per ogni
   L_Name, `[obj+0x40]` ≠ 0.

   Esito atteso in Eden con 5g/8g: selezione con la grafica 6pl/8pl, 4 cursori attivi, nessun crash. Con 2-4
   giocatori tutto identico (H-A1a/H-A1b passano a `Orig`).

### A1-M2 – rilocazione +0x904 (provabile da sola, comportamento invariato con +0x8c0 = 4)
```cpp
namespace sel8 {
    struct P { uint32_t off, orig, repl; };
    static const P kReloc904[] = {               /* 72 accessi + 39 clamp + 3 fabbriche: a1_904_reloc.txt */
        {0x4091c4, 0x52814500, 0x52814d00}, {0x4091f0, 0x52814500, 0x52814d00}, {0x40921c, 0x52814500, 0x52814d00},
        {0x4b111c, 0xb9490509, 0xb94a2909}, /* ... */ {0x4c451c, 0xf100129f, 0xf100229f},
    };
    static bool ApplyAll(const P* t, size_t n, const char* what) {   /* tutto o niente */
        for (size_t i = 0; i < n; i++)
            if (*(uint32_t*)(mk8dx::Base() + t[i].off) != t[i].orig) {
                Logging.Log("[mk8dx-8p] %s: parola inattesa a %x, nessuna patch", what, t[i].off); return false; }
        for (size_t i = 0; i < n; i++) input::Patch(t[i].off, t[i].orig, t[i].repl, what);
        return true;
    }
}
HOOK_DEFINE_TRAMPOLINE(CharaSelBaseCtorHook) {               /* 0x4bc844 ctor base */
    static void Callback(uint8_t* self) {
        Orig(self);
        for (int i = 0; i < 8; i++) { *(int32_t*)(self + 0xa28 + 8 * i) = 0; *(int32_t*)(self + 0xa2c + 8 * i) = 0; }
        for (int i = 0; i < 0x20; i += 4) *(uint32_t*)(self + 0x904 + i) = 0xA1A1A1A1;   /* canary vecchia area */
    }
};
```
Per input::Patch serve un log solo in caso di errore, altrimenti produce 114 righe. Il controllo del canary
va nel log periodico esistente (ControllerMgrCalcHook) leggendo la pagina 0x2e via 0x8bf0ec.

### A1-M3 – slot attivi 6/8 (dipende da A2/A3)
Ordine: +0x928 e +0x660 rilocati in coda (oltre +0xa68, rifare le fabbriche); 4 pagine finestra nuove e
rimappaggio dei 17 siti `0x28+i`; array per giocatore della config (ombra); poi **+0x8c0 = sel8::built**
(trampolino su 0x4bcc84 prima di `Orig`), sempre ≤ w20. Poi +0x39c kart (onCreate kart), 0x422efc e i pad UI
4..7 (step 4 §5.2).

### Da verificare a runtime
1. Quando si chiama 0x4be854 per la pagina 0x16 (ingresso in selezione o costruzione del menu). Dipende da
   questo se N è già noto (backtrace del log H-A1a).
2. Che `[+0x1d0]+0x40` (layout parte) ≠ 0 con 6pl/8pl e che i T_Name_00 dei L_Name siano trovati (nessun
   crash a 0x4bdf30).
3. Che l'archivio `mn_Page_Lyt_CharaParts_8pl` e la parte `mn_CharaKartSlct_8pl` si carichino per nome dalla
   RomFS (nessun errore di risorsa nel log Eden).
4. Aspetto: colori L_Name con 6/8 (0x4bed2c li salta), animazioni del FLAN 8pl, slot 4..7 non attivi
   (stato iniziale dei pannelli).
5. Dopo M2: canary intatto in 1-4P e nessuna differenza di comportamento; watchpoint su +0x904..+0x923.
6. Maschera +0x1a8 e pad UI: con +0x8c0 = 4 invariata.
