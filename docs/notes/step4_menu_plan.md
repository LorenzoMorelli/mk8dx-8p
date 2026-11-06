# Step 4 – flusso menu 5-8 giocatori: piano (analisi statica)

Main 4.0.1 (BID 158C0C73…), offset nel main decompresso; IPS = +0x100. Nessuna prova a runtime in questo
documento: [VERIFICATO statico] salvo dove scritto [IPOTESI] / [DA VERIFICARE A RUNTIME].
Dati grezzi in `step4_data (dati non pubblicati)/` (disassemblati, tabelle di salto, fabbriche pagine, siti per lo step 5/6).

## 0. Sintesi

- Il gestore 0x4d92e8 **gestisce già 2..8**: la seconda tabella di salto (0xf29604) manda gli id 0x14..0x1a
  (2g..8g) tutti a **0x4d9774** con `w21 = id - 0x12` = 2..8. L'unico ostacolo è il filtro a 0x4d9460, che
  intercetta 0x17..0x1a *prima* di quella tabella e mostra il dialogo.
- 0x4d9774 per N > 4 ha un ramo **nuovo nel 4.0.1** (nel 3.0.5 non c'è: `csel` 2/4 e modo 2/3 senza confronto):
  `cmp w21,#4; b.gt 0x4d9928` → modo (cfg+0x1fa8) **non scritto** (resta il valore vecchio) e
  `0x13d56c(audio, 0)`, che legge la tabella {0,1,2,2} all'indice **-1** (dentro un mutex statico).
  Il conteggio (cfg+0x1fa4 = N), `0x869180(raceMgr, N, 1, 0)` e il passaggio alla pagina 0x24 sono invece
  comuni a tutti gli N.
- La "schermata successiva" è la **pagina 0x24** (scelta modalità Multi: `L_GrandPrix_Ml_00`, `L_Battle_Ml_00`,
  `L_OriginalRule_Ml_00`), uguale per 2..8. L'applet non è chiamato lì ma nella selezione personaggi
  (0x4ba77c → 0x4baa6c) e nella selezione coppa (pagina 0x3c, 0x491f50 → 0x492390).
- Loop dell'applet con N=5..8: ciclo `for i<N: 0x8be248(0x8bf158(i))`. 0x8bf158 limita a 5 (i ≥ 5 → pad UI 0),
  quindi **solo i = 4 fallisce**: il pad UI 4 è il pad "speciale" (any-controller), con idx 4 → 0x8be248 falso
  (`cmp w0,#3; b.hi`) a ogni frame, qualunque cosa faccia l'applet.
- **Minimo per lo step 4 = 2 patch + 1 hook** (sotto, §4). Tutto il resto (pad UI 5..8 veri,
  raceMgr+0x17c a 8, giocatori di input 4..7, selezione a 8 posti) è per lo step 5/6 (§5).

## 1. Percorso nel gestore 0x4d92e8 [VERIFICATO]

*(estratto di disassemblato del gioco rimosso)*
Cosa scrive il percorso, per N = 3/4 e per N = 5..8 dopo le patch:

| campo | 3-4P | 5-8P vanilla (ramo b.gt) | 5-8P con P5b |
|---|---|---|---|
| cfg+0x1fa4 (conteggio) | N | N | N |
| cfg+0x1fa8 (modo: 1 Single, 2 Multi-2P, 3 Multi-3/4P, 4/5 online 1/2, 6/7 locale 1/2, 8 = id 0x66) | 3 | invariato (valore precedente) | 3 |
| 0x13d56c arg (oggetto+0xec; tabella 0x131f32c[arg-1] = {0,1,2,2}, usato da 0x13e59c su ≤4 oggetti audio) | 4 → 2 | 0 → indice -1 = parola di un mutex [IPOTESI: 0] | 4 → 2 |
| raceMgr (0x8ace28) +0x50/+0x58 entry | N entry locali | N | N |
| raceMgr +0x178 / +0x17c[4] (riempimento 0x869648, dentro 0x869180) | N / entry 0..N-1 | N / **+0x17c[4..7] sfora in +0x18c[0..3]** | idem |
| settings (system+0x240) +0x180/+0x184/+0x188 | non scritti qui (preparazione gara, step 6) | – | – |

Note: raceMgr+0x178 è il **numero di locali** (azzerato e incrementato dal riempimento), non "==1 handheld"
come in STRUCTS; +0x17c..+0x188 = int[4] locale→entry (0x868f30-0x868f80 li legge come +0x180/+0x184/+0x188);
+0x18c int[16] = slot (entry+0x230) → entry (setter 0x86af68). Il memset 0x50 a 0x868db8 copre +0x17c..+0x1cb.
0x869180 non ha limiti a 4 nella creazione (16 entry); 0x868f00 (compattazione, chiamata perché arg2=1)
è srotolata su +0x17c[0..3] ma restituisce il numero solo se `count < 0` (qui count = N ≥ 0: ininfluente).

## 2. Selezione personaggi 0x4ba77c (slot vtable 86 delle pagine CharaSelect) [VERIFICATO]
Pagine: 0x2c/0x2d/0x2e = fabbriche 0x4091b8/0x4091e4/0x409210 (Single/WiFi/Multi, size 0xa28).
```
N = cfg+0x1fa4 ; slot = +0x8c0 (Multi 4)
per i < slot: conta w28 = slot in stato 7 (+0x904[i]); w26 = slot ≥5 con +0x120 fuori 3..7
w28 == N          -> 0x8ba5fc(this, this+0x1d0): avanti (con N>4 mai: step 5)
!+0x8c4 && w26<N && !+0x8c5:
   per i < N: 0x8be248(0x8bf158(i), N==1); il primo falso -> w20=1
   pad0+0x10 & 0x18  oppure  w20  oppure  +0x9d0  -> 0x49ea60(N, N, 1, 0) (0x4baa6c), pagina +0x4e8
0x8be478 su pad UI 0..3 (evidenzia +0x900)
```
Con N=5: i=4 → pad UI 4 (idx 4) → falso. Con N=6..8: i=4 falso, i=5..7 → pad UI 0 (clamp 5 di 0x8bf158).
Stesso schema a 0x4922fc (pagina 0x3c, selezione coppa). Il sito 0x49c938 (pagina di gara, stato +0x1630)
è srotolato su 0..3 (step 6). 0x49ea60 copia min/max in byte senza limitarli; legge cfg+0x1fa8 (`cmp #7`).

Esperimenti passati alla luce di questo: `ips_A`/`ips_C` "salto applet" = 0x4baa04 `tbz` → nop (non registra
mai il falso: applet mai chiamato); `ips_C` 0x4ba938 `cmp w28,w22` → `cmp w28,w28` (avanza sempre). Nessuna
IPS conteneva la patch a 0x4d9898: con 5-8 il modo restava vecchio e 0x13d56c riceveva 0.

## 3. Pad UI e controllo "pronto" 0x8be248 [VERIFICATO]
- Contenitore `[[0x1306e70]]+0x58` (alloc 0x38 a 0x8b1210): ptr[5] a +0x00..+0x20, +0x28 ptr, +0x30 maschera.
  Non estendibile in loco (+0x28/+0x30 letti a offset fissi: 0x8bab30, 0x8bc398, 0x4c3fac, 0x8af764…).
- Riempito a 0x3ffaec-0x3ffb90: 5 pad 0x2c8 B (ctor 0x8bde24), setter 0x8bf040 (`pad+8 = i`, clamp 5).
  Il pad 4 è "any-controller": spento a 0x3ffba0/0x3ffbac, acceso/spento con `0x8bf158(4)` a 0x4b82a4,
  0x4bbcdc, 0x4c17c8, 0x4c3c98; in 0x8be674 (calc per pad) fonde gli input dei pad 0..3 (0x8beb70-0x8bec08,
  `cmp x22,#0x20`). Non passa mai da 0x8be248 nel gioco originale (i cicli arrivano a N-1 ≤ 3).
- 0x8be248(pad, flag): `idx=[pad+8]`; idx>3 → falso; `p = 0x86ebf8(idx)` (raceMgr+0x17c[idx], idx>3 → -1);
  `e = raceMgr+0x58[p]`; `gp = e+0x200`; flag=0 → falso se gp vt+0xa0 (handheld); poi gp vt+0x80 (stato).
- e+0x200 è assegnato da 0x864fcc → **0x86a594**, srotolato sugli indici giocatore 0..3 di PIM+0x58:
  le entry locali 4..7 hanno gp = NULL anche con PIM a 8 giocatori finché 0x86a594 non è esteso.

## 4. Modifiche minime per l'accettazione dello step 4

### P5a – salto del dialogo (= ips_v2)
| offset | originale | nuovo |
|---|---|---|
| 0x4d9468 | `54001708` b.hi 0x4d9748 | `140000b8` b 0x4d9748 |
Per gli id ≠ 0x17..0x1a il ramo era già 0x4d9748: nessun effetto su 1-4P.

### P5b – 5-8 seguono il ramo 3-4P (modo 3, arg audio 4)
| offset | originale | nuovo |
|---|---|---|
| 0x4d9898 | `5400048c` b.gt 0x4d9928 | `d503201f` nop |
Evita il modo "vecchio" e la lettura fuori tabella in 0x13e59c. Con N ≤ 4 il salto non era mai preso:
nessun effetto su 1-4P. Effetto collaterale voluto: per 5-8 valgono i comportamenti del Multi 3/4P
(lettori di `cfg+0x1fa8 == 3`: `step4_data/cfg1fa8_readers.txt`).

### H4 – "pronto" del pad UI 4 per N > 4 (trampolino su 0x8be248, sostituisce PadReadyHook)
Il pad UI 4 è l'unico interrogato per i ≥ 4 nei cicli 0x4ba994/0x4922fc: per N > 4 lo si fa rispondere per
tutti gli npad 4..N-1. Con N ≤ 4 si chiama sempre `Orig` (comportamento identico).
```cpp
namespace menu8 {
    /* config menu: 0x8bf0b0(&3) -> oggetto, +0x30 = config (come 0x4d9984-0x4d99a4, senza il controllo RTTI) */
    static uint8_t* Cfg() {
        int id = 3;
        uint8_t* o = mk8dx::Fn<uint8_t* (*)(int*)>(0x8bf0b0)(&id);
        return o ? *(uint8_t**)(o + 0x30) : nullptr;
    }
    static int LocalCount() { uint8_t* c = Cfg(); return c ? *(int32_t*)(c + 0x1fa4) : 0; }
    static int Mode()       { uint8_t* c = Cfg(); return c ? *(int32_t*)(c + 0x1fa8) : -1; }
    /* sead::ControllerMgr: GOT main+0x12fbcb0 -> variabile -> mgr (come 0xca054-0xca05c) */
    static uint8_t* CtrlMgr() {
        uintptr_t* var = *(uintptr_t**)(mk8dx::Base() + 0x12fbcb0);
        return var ? (uint8_t*)*var : nullptr;
    }
    static bool NpadConnected(int npad) {       /* NpadController di gioco (step 3: npad 4..7 in pos 6..9) */
        uint8_t* mgr = CtrlMgr(); if (!mgr) return false;
        int n = *(int32_t*)(mgr + 0x188); uint8_t** list = *(uint8_t***)(mgr + 0x190);
        for (int p = 0; p < n; p++) {
            uint8_t* c = list[p];
            if (*(int32_t*)(c + 0x138) == 0x11 && *(int32_t*)(c + 0x178) == npad) return c[0x17c] & 1;
        }
        return false;
    }
}
HOOK_DEFINE_TRAMPOLINE(PadReadyHook) {                       /* 0x8be248(padUI, flag) */
    static bool Callback(uint8_t* pad, uint32_t flag) {
        int idx = pad ? *(int32_t*)(pad + 0x8) : -1;
        int n = menu8::LocalCount();
        bool r;
        if (idx == 4 && n > 4 && n <= 8) {                   /* solo flusso 5-8 */
            r = true;
            for (int k = 4; k < n; k++) r = r && menu8::NpadConnected(k);
        } else r = Orig(pad, flag);
        static int last[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
        if (idx >= 0 && idx < 8 && last[idx] != (int)r) {
            last[idx] = r;
            Logging.Log("[mk8dx-8p] applet: pad UI %d (flag %u, N=%d) pronto=%d", idx, flag, n, (int)r);
        }
        return r;
    }
};
```
Installazione in `exl_main` (dopo le patch dello step 3):
```cpp
input::Patch(0x4d9468, 0x54001708, 0x140000b8, "P5a 5g-8g: salto del dialogo");
input::Patch(0x4d9898, 0x5400048c, 0xd503201f, "P5b 5-8 nel ramo Multi 3/4P (modo 3)");
/* PadReadyHook::InstallAtOffset(0x8be248) resta dov'è: cambia solo il Callback */
```
Rischio: H4 non controlla lo stato del GamePlayer per 4..N-1 (che non esiste ancora: §3), solo la connessione
del controller. Basta per uscire dal loop: dopo `0x49ea60(N,N,1,0)` Eden collega gli npad 0..N-1
(take_over/left_justify, step 3 §3) e alla frame successiva tutti i controlli sono veri.
Esito atteso in Eden: 5g..8g → pagina 0x24 → (Gran Premio/VS) → selezione personaggi Multi a **4 posti**,
applet chiamato **una volta** con min=max=N, nessun dialogo; i 4 posti si confermano ma la pagina non
avanza (w28 ≤ 4 ≠ N: step 5).

### Log di accettazione
1. Numero memorizzato – nel `MenuEventHook` esistente, dopo `Orig`, anche per la conferma:
```cpp
if (kind == 2 && id >= 0x14 && id <= 0x1a) {
    uint8_t* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)();
    int32_t* l = (int32_t*)(rm + 0x17c);
    Logging.Log("[mk8dx-8p] step4: pulsante %ug -> cfg+0x1fa4=%d +0x1fa8=%d race+0x178=%d +0x17c={%d,%d,%d,%d}",
                id - 0x12, menu8::LocalCount(), menu8::Mode(), *(int32_t*)(rm + 0x178), l[0], l[1], l[2], l[3]);
}
```
   Atteso per 5..8: `1fa4 = N`, `1fa8 = 3`, `+0x178 = N`; nessuna riga "menu event" del dialogo.
2. Chiamate applet – nel `ControllerAppletHook`: chiamante e rilevatore di loop (frame da `input::calls`,
   incrementato da ControllerMgrCalcHook ogni frame):
```cpp
uintptr_t lr = (uintptr_t)__builtin_return_address(0) - mk8dx::Base();   /* exlaunch: x30 del chiamante [DA VERIFICARE] */
static unsigned long lastFrame; static int burst;
burst = (input::calls - lastFrame < 120) ? burst + 1 : 0; lastFrame = input::calls;
Logging.Log("[mk8dx-8p] applet: da main+%lx min=%u max=%u N=%d modo=%d%s", lr, mn, mx,
            menu8::LocalCount(), menu8::Mode(), burst >= 2 ? " LOOP!" : "");
```
   Atteso: `da main+4baa70` (selezione personaggi) o `+492394` (selezione coppa), una sola volta per ingresso.
3. Pad pronti – PadReadyHook: `pad UI 1..3 pronto=1` e `pad UI 4 (N=n) pronto=1` dopo l'applet.
4. Facoltativo: riga ogni 600 frame da ControllerMgrCalcHook con `conn` dei pos 6..9 (già presente).

Accettazione: per N = 5, 6, 7, 8 (Eden: profili 1..8 connessi) → riga step4 con N, una chiamata applet
min=max=N, nessuna riga LOOP, `pad UI 4 pronto=1`, schermata personaggi visibile; nessun crash per 2 minuti
nel menu. Regressione: 1P, 2P, 3P, 4P con solo N profili connessi (le righe step4 devono dare 1fa8 = 2/3 come
prima, applet e pad UI 0..3 come nel log 3d dello step 3).

## 5. Cosa può aspettare (step 5/6), con patch e codice indicativo

Siti con parola originale in `step4_data/sites_step5.txt`.

### 5.1 raceMgr+0x17c a 8 locali e riparazione di +0x18c (consigliato all'inizio dello step 5)
Il riempimento (5 copie: funzioni 0x868cfc, 0x868e5c, 0x869180, 0x869760, 0x86b19c) scrive +0x17c[k++] senza
limite: con N>4 sovrascrive +0x18c[0..N-5] (slot→entry). Nei menu non dà problemi visibili, in gara sì.
Trampolino **dopo** `Orig` su ciascuna delle 5 funzioni (stessa firma x0 = raceMgr):
```cpp
namespace race8 {
    static int32_t local[8]; static int localN;
    static void AfterFill(uint8_t* rm) {
        int cnt = *(int32_t*)(rm + 0x50); uint8_t** e = *(uint8_t***)(rm + 0x58);
        localN = 0;
        for (int j = 0; j < cnt; j++) if (*(int32_t*)(e[j] + 0x44) == 0 && localN < 8) local[localN++] = *(int32_t*)(e[j] + 0x48);
        if (localN <= 4) return;                                    /* 1-4P: nulla da riparare */
        for (int s = 0; s < 16; s++) *(int32_t*)(rm + 0x18c + 4 * s) = -1;
        for (int j = 0; j < cnt; j++) { int32_t s = *(int32_t*)(e[j] + 0x230);
            if (s >= 0 && s < 16) *(int32_t*)(rm + 0x18c + 4 * s) = *(int32_t*)(e[j] + 0x48); }
    }
}
HOOK_DEFINE_TRAMPOLINE(LocalMapHook) {                             /* 0x86ebf8(i) -> entry del locale i */
    static int32_t Callback(int32_t i) { return (i >= 4 && i < 8) ? (i < race8::localN ? race8::local[i] : -1) : Orig(i); }
};
```
Poi `cmp #3 → #7` nei 12 siti del pad UI (`71000c1f → 71001c1f`) e a 0x86ec08 (`71000e7f → 71001e7f`), solo
insieme ai pad UI 5..8 (5.2). Restano srotolati a 4: 0x868f00 (compattazione), 0x8653cc (+0x214[4]).

### 5.2 Pad UI 4..7 veri, "speciale" spostato a 8
- Hook REPLACE su 0x8bf158(i): `i<4 → orig`; `4≤i<8 → extra[i-4]`; `i==8 → ptr[4]` (lo speciale);
  extra creati la prima volta con `0x5fa960(0x2c8)` + ctor 0x8bde24 + `pad+8 = i` (come 0x8bf040) e flag
  `0x8be458/0x8be478(p, 0)` come il pad 4 a 0x3ffb94.
- Patch `orr w0,wzr,#4` → `#8` (`321e03e0 → 321d03e0`) a 0x4b82a4, 0x4bbcdc, 0x4c17c8, 0x4c3c98.
- Trampolino dopo `Orig` su 0x8bf058 (calc dei 5 pad): `0x8be674(extra[k])` per k=0..3.
- Fusione input del pad speciale (0x8beb70-0x8bec08) sui pad 0..3: hook o ricalcolo per gli extra.
- Con 5.1+5.2 si toglie il caso speciale di H4 (0x8be248 torna a `Orig` con `cmp #7`).

### 5.3 Giocatori di input 4..7 (piano step 3 §2c G1-G8) + 0x86a594
0x86a594(raceMgr, entryId) assegna il GamePlayer libero cercando gli indici 0,1,2,3 (srotolato): REPLACE
```cpp
HOOK_DEFINE_TRAMPOLINE(AssignGamePlayerHook) {                    /* 0x86a594 */
    static void* Callback(uint8_t* rm, uint32_t entryId) {
        uint8_t* pim = mk8dx::Fn<uint8_t* (*)()>(0x8ace0c)();
        int pc = *(int32_t*)(pim + 0x50); void** pl = *(void***)(pim + 0x58);
        uint8_t** e = *(uint8_t***)(rm + 0x58);
        for (int k = 0; k < pc && k < 8; k++) {
            bool used = false;
            for (uint32_t j = 0; j < entryId && j < 16 && !used; j++) {
                uint8_t* gp = *(uint8_t**)(e[j] + 0x200);
                used = gp && *(int32_t*)(gp + 0x768) == k;
            }
            if (!used) return pl[k];
        }
        return nullptr;
    }
};
```
(richiede G1/G3: PIM+0x50 a 8 giocatori; con PIM a 4 il comportamento è identico all'originale).

### 5.4 Selezione personaggi a 6/8 posti (step 5)
+0x8c0 (0x4bcc70), array +0x904 (17 clamp, size 0xa28), layout 8pl, avanzamento `w28 == N` a 0x4ba938,
0x422efc (4 iterazioni), pagine CharaParts: catalogo step 1 §3.

### 5.5 Gara / console reale (step 6+)
0x49c938 (pronto/applet srotolati su 0..3 in gara), 0x13e59c (audio ≤4 oggetti), arg applet 0x21c
(4 giocatori) su console reale.

## 6. Rischi di regressione 1-4P

| modifica | effetto su 1-4P | verifica |
|---|---|---|
| P5a 0x4d9468 | nessuno: per id ≠ 0x17..0x1a il salto era già preso | 2g/3g/4g: riga step4 con 1fa8 = 2/3/3 |
| P5b 0x4d9898 | nessuno: con N ≤ 4 `b.gt` mai preso | idem |
| H4 0x8be248 | nessuno: ramo nuovo solo con idx==4 e N>4; il pad 4 non arriva mai a 0x8be248 con N≤4 | log `pad UI 0..3` come nel log 3d |
| log MenuEvent/applet | solo letture (0x8bf0b0 è lo stesso getter del gioco) | – |
| 5.1 (step 5) | AfterFill esce subito con ≤4 locali; LocalMapHook passa a Orig per i<4 | gara 1P/4P |
| 5.2/5.3 (step 5) | cambiano oggetti condivisi (pad speciale, assegnazione GamePlayer) | regressione completa 1-4P |

Rischi residui per 5-8 nello step 4 (accettati): selezione a 4 posti senza avanzamento; entry 4..7 senza
GamePlayer (gp NULL, già gestito con `cbz` nei lettori visti: 0x8be28c, 0x8be6ec, 0x8653e0); +0x18c
sporcato dal riempimento (5.1); Eden: tenere connessi esattamente N profili prima della scelta.

## 7. Lista ordinata delle modifiche
1. P5a 0x4d9468 `54001708 → 140000b8` (dialogo saltato).
2. P5b 0x4d9898 `5400048c → d503201f` (5-8 nel ramo 3/4P: 1fa8 = 3, 0x13d56c arg 4).
3. H4: Callback di PadReadyHook (0x8be248) con il caso `idx==4 && N>4` → npad 4..N-1 connessi.
4. Log: MenuEventHook (tipo 2, id 0x14..0x1a), ControllerAppletHook (chiamante + LOOP), PadReadyHook (N).
5. Test Eden N=5..8 e regressione 1-4P.
Step 5: 5.1 (raceMgr+0x17c/+0x18c, 0x86ebf8) → 5.2 (pad UI 4..7, speciale a 8, `cmp #3→#7`) → 5.3 (G1-G8,
0x86a594) → 5.4 (selezione a 6/8 posti). Step 6: 5.5.
