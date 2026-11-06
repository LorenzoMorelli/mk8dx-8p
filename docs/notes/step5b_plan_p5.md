# Step 5b – passo 5: pagina kart a 8 slot in un turno solo (piano statico)

Data: 3 nov 2026. Solo analisi statica: `src/` non modificato, nessuna build. Base: `../img/main.bin` (offset = indirizzo), `../img/main.ann.asm`,
bflyt/bflan della RomFS 4.0.1 (`../romfs-tmp/menu4/*.sarc`, `../romfs-tmp/v4/common.sarc`).
Dati in `step5b_data (dati non pubblicati)/` (prefisso `p5_`):
- `p5_verify.py` → `p5_verify.txt` (**0 errori**, nessuna collisione con gli indirizzi di `main.cpp`) e `p5_table.inc`: tutte le parole
  (originale e nuova, disassemblate con llvm-objdump) e le prime parole degli hook.

Etichette: [VERIFICATO] = letto in main.bin / RomFS (indirizzo o file indicato); [IPOTESI] = da confermare in Eden.

## 0. Sintesi
- **Decisione layout**: la pagina kart 0x31 viene costruita **due volte** nella stessa istanza.
  - **Set A** (vanilla): pagina 0x16 Multi, 4 slot. Si usa in 1-4P e in 5-8P a due turni.
  - **Set B**: pagina nomi **0x12** (8pl), **8 slot**, cursore proprio con 32 item, `+0x110[8]`, `+0x450[8]` (OKWin).
  - All'ingresso della kart si scambiano i campi della pagina (`+0x3a8`, `+0x39c`, i 4 PtrArray `+0x3c0..+0x3ff`, `+0x450`, `+0x108/+0x110`,
    `+0x1b0`, contenuto di `**(+0x178)`), secondo `p4::one`.
  - La kart **non può** usare sempre la 0x12: l'onCreate gira una volta sola per menu e in 1-4P la kart deve restare Multi (§2).
- **+0x460[4] → +0x4a0[8]** (stride 8, `+0x464` timer → `+0x4a4`): **40 parole** (ldr/str/add), fabbriche 0x4a0 → **0x4e0** (Single, WiFi,
  Multi, che condividono il codice).
  - Azzeramento srotolato 0x4c157c: con `+0x39c = 8` non si esegue (`cmp w8,#3; b.hi`) [VERIFICATO 0x4c157c..0x4c1580]. Non serve un hook,
    solo la rilocazione delle sue 5 `str`.
- **Clamp**: **34 `cmp #4 → #8`**:
  - i 23 kart di `kClamp904`, con i 7 condivisi;
  - 6 su `+0x460`/cursore (0x4c2948, 0x4c2b70, 0x4c2f20, 0x4c3dc4, 0x4c3e40, 0x4c3fe4);
  - maschere dei selettori 0x4c16d4;
  - 3 config 0x4c1dd8/0x4c1e8c/0x4c1f40, con 3 hook cfg8;
  - PartsCursor 0x49ae2c.
- **Cursori**: 5 kPageSlotFix kart (0x4c192c, 0x4c1b08, 0x4c2bd8, 0x4c3dd0, 0x4c3e4c). Il cursore B è largo (`cur8::SetWide`).
- **PartsCursor 0xe** (cornici colorate `cm_L_KPSlotCursor_0i`):
  - la RomFS ha **già 8 parti** `cm_L_KPSlotCursor_00..07` [VERIFICATO `cm_Page_PartsCursor.szs`];
  - l'onCreate crea i controlli solo per 0..3 (0x49ad48 `cmp x25,#3`) e nasconde 4..7. Patch `#7` + oggetto 0x1f8 → 0x230;
  - i controlli 4..7 vanno a `+0x210..+0x228`, come per cur8;
  - servono 4 hook inline sugli indici.
- **Totale passo 5**: **79 parole** e **18 voci JIT** (14 inline + 4 trampolini), più 5 hook esistenti modificati. Capienza: circa
  186 + 18 = **204 su 327**.
- **Difetto (2), anteprime "sbagliate"**: i 4 modelli mostrano davvero P1..P4. Sono però nelle posizioni Multi (angoli) e il layout 8pl
  **alterna sinistra/destra** (N_Player pari a x = −340, dispari a x = +340) [VERIFICATO bflyt]. Il modello in basso a sinistra (P3) finisce
  accanto al pannello di P5, quello in basso a destra (P4) accanto a P6, e chi legge per colonne (P1-P4 a sinistra) li legge sbagliati (§1.2).
  Al passo 5 le anteprime si **nascondono** a turno unico; le 8 vere sono il passo 7.
- **Crash latente trovato**: `0x14a038(model, …)` va a 0x148b50, che legge `[x0+0xd7]` **senza controllo** [VERIFICATO 0x148b6c]. La kart la
  chiama con `0x13d47c(mgr, i)`, che per i ≥ 4 restituisce NULL (0x4c19a4/0x4c19c0, 0x4c2c64/0x4c2c70, 0x4c40cc/0x4c40ec). Serve un trampolino
  di guardia **prima** di portare `+0x39c` a 8.
- **Difetto (1), icona carrozzeria di P1**: nessun sito del passo 4 è nel percorso del selettore di P1 (§1.1). Causa più probabile: il
  flusso "regalo parti" del vanilla (solo P1, `[+0x3d8][0]+0x324`) oppure il caricamento delle icone. Non risolto staticamente: serve una
  prova mirata in Eden (§1.1).

## 1. I due difetti

### 1.1 Icona della carrozzeria di P1 vuota (`eden_5b4_kart_p1vuoto.png`)
Fatti [VERIFICATO]:
- I selettori ricevono personaggio e parti all'**uscita dalla selezione**, non nell'onEnter della kart:
  - 0x4bb650 `0x4c0490(kart, 1)`: per ogni slot i < `+0x39c` in stato 7, entry `0x8be068(0x8bf158(i))`, poi
    `0x474bf0(sel, &pers, 0x41ffd8(e+0x210), flag)` per carrozzeria, gomme e aliante (0x4c04c4..0x4c063c);
  - l'onEnter chiama 0x4c0490 solo quando si torna dalle regole (0x4c0af4: `w26 = id precedente − 0x2c ≥ 3`).
- 0x474bf0 → 0x474d54 (lista carrozzerie per personaggio):
  - con flag = 1 e offline scrive `sel+0x324` = parte "in regalo" letta dal salvataggio (`0x866dfc(save+0x190)`, tipo 1) e la include nella
    lista anche se bloccata (0x474fa4..0x474ff8, 0x475024..0x475038);
  - poi azzera le texture di tutte le icone visibili (`0x8ada44(pane, 0x423fc4(), 0)`, flag `+0x29c` per icona; 0x474cf0..0x474d34).
- La kart legge solo `[+0x3d8][0]+0x324`, cioè **solo P1** (0x4c0f9c, vt+0x2d8 0x4c2608). Se il campo vale ≠ −1 si attiva il regalo:
  - 3D in modalità carrozzeria (`0x14ae4c(&1)`) e `+0x398 = 0`;
  - i nomi diventano "?" (0x4c1b3c, stringa 0xf26f2c = "?");
  - per frame si apre la pagina **0x27 = PartsPresent** (`img/pages.txt`), passando i selettori in `+0x238[4]`.
- Siti del passo 4 o di cur8 nel percorso: **nessuno**.
  - I clamp kart sono ancora a 4.
  - cfg8 scatta solo con indice 4..7.
  - Le kPageSlotFix del passo 4 sono nella selezione, la kart non le ha ancora.
  - Il cursore kart non è largo (`cur8::Wide` falso).
  - 0x49ae1c lavora su 0..3.
  - CloseNames tocca solo la visibilità di N_Player_00..03 della 0x16 e la pagina 0x12.

Lettura dello screenshot: i nomi mostrano "Kart standard", non "?". Quindi al momento della foto `+0x398` era −1: il regalo non era attivo,
oppure era già stato consumato.

Ipotesi, in ordine:
- **H1** [IPOTESI]: regalo carrozzeria in sospeso, con l'icona della parte regalata tenuta nascosta finché la PartsPresent non la rivela. Il
  passaggio a 0x27 avviene in vt+0x2d8 solo con `+0x120 == 3 && +0x124 == 0` (0x4c2570..0x4c2580), per cui un'animazione o un'uscita diversa
  (due turni, PageLinkHook) può lasciare il selettore di P1 in questo stato.
- **H2** [IPOTESI]: limite del caricatore asincrono delle icone. Con il passo 4 la pagina 0x12 aggiunge 8 icone macchina (`P_MachineIcon_00`
  nelle L_Name) alle 4×4×3 dei selettori.

Prova in Eden (prima del passo 5, poche build):
1. Stesso salvataggio, **4P senza mod**: se l'icona manca anche lì, è il comportamento vanilla (H1).
2. **8P con `MK8DX_SEL1ROUND 0`** (due turni, senza passo 4): se manca anche lì, il difetto c'era già con i due turni.
3. GDB, all'ingresso della kart, breakpoint a 0x4c0f9c, poi lettura di `sel = **(kart+0x3d8)`: `+0x278` (personaggio), `+0x27c`, `+0x324`
   (regalo), `+0x31c`, `+0x320`. Confronto con `*(*(kart+0x3d8)+8)` (P2) e watchpoint su `sel+0x324`.
4. Se `+0x324 ≠ −1`, il difetto è H1. Il passo 5 lo eredita senza peggiorarlo: la vt+0x2d8 legge sempre e solo lo slot 0.

### 1.2 Anteprime 3D (`eden_5b4_charsel8.png`, `eden_5b4_kart_p1vuoto.png`)
- Le pagine nomi 8pl hanno N_Player_0i **alternati**: pari a x = −340, dispari a x = +340, righe a y = 344/186/28/−130 per le coppie (0,1),
  (2,3), (4,5), (6,7) [VERIFICATO `mn_CharaKartSlct_8pl.sarc`].
  - Ordine sullo schermo: P1 in alto a sinistra, P2 in alto a destra, P3 e P4 in seconda riga, …
  - Lo conferma la foto Switch 2 `ref_switch2_kart_8p.jpg` (giallo/ciano, rosa/verde, rosso/blu, viola/arancio per righe).
- Le 4 anteprime restano agli angoli Multi e mostrano gli slot 0..3. La selezione usa lo **slot come indice del modello**:
  - `0x13d4ac(mgr, ev+0x2c, &pers)` a 0x4bc6e0;
  - `0x13d424(mgr, slot, 0)` a 0x4bc6c0 e 0x4ba23c;
  - i dati del modello vanno al viewer (`0x398898()+0x270`) all'indice `model+0xc0` (0x148038/0x42766c, 0x148108/0x427698).
  - Nello screenshot della selezione: Iggy (P1, in alto a sinistra), Abitante (P2, in alto a destra), Larry (P3, in basso a sinistra),
    Waluigi (P4, in basso a destra) = slot 0..3.
- **Causa**: nessuna mappa sbagliata. È una **sovrapposizione di posizioni**:
  - in basso a sinistra c'è il modello di P3 accanto ai pannelli di P5/P7;
  - in basso a destra c'è P4 accanto a P6/P8.

  Chi legge l'8pl per colonne (P1-P4 a sinistra, P5-P8 a destra) vede "in alto a destra = P5" e "in basso a destra = P6".
- **Passo 5**: a turno unico (`p4::one`) si nascondono i 4 modelli con `0x14a7ac(i, false)` a ogni frame, in selezione e in kart (opzione
  `MK8DX_P5_HIDE3D`, default 1).
  - In 1-4P non cambia nulla.
  - La chiamata per frame serve perché la kart riaccende i modelli con 0x14a7ac/0x14a038/0x13d424 in più punti
    (0x4c1010, 0x4c19c0, 0x4c2c70, 0x4c40ec).

  0x14a7ac è null-safe [VERIFICATO passo 4].
- **Rischio crash** (§0):
  - `0x14a038(0x13d47c(mgr, i), …)` con i ≥ 4 dereferenzia NULL (0x148b50 → `ldrb w8,[x19,#0xd7]`) → trampolino di guardia **0x14a038**.
  - Anche 0x148038/0x148108 leggono `[x0+0xc0]` senza controllo, ma solo se `0x398898()+0x270` non è nullo. Il log del passo 4 (`0x13d424
    slot 4 saltato`, con chiamata esplicita a `0x148108(nullptr)`) mostra che nel menu quel ramo non viene raggiunto [VERIFICATO log].
- **Per il passo 7** (8 anteprime vere):
  - modelli in `[0x7e7434()+0x190+4*8]+0x238`: `+0xc0` conteggio = 4, `+0xc8` array; oggetto modello `+0x30`, indice viewer `+0xc0`;
    stato `+0x350/+0x354/+0x358/+0x361`;
  - viewer `0x398898()+0x270 → +0x70`, con set del personaggio 0x430e24 e reload 0x430e58.
  - Il punto in cui si creano i 4 modelli e la costante 4 non sono stati trovati in questa analisi (da cercare: scrittori di `+0xc0` del
    gestore, chiamanti di 0x7e7434 con +0x238).

## 2. Decisione: la kart usa la pagina nomi 0x12 solo a turno unico, con un secondo set di controlli
Fatti [VERIFICATO]:
- vtable kart Multi `0x127a0f0` (vptr +0x10):
  - +0x240 = 0x4c4ed0: cursore `0x8bafb8(page, +0x39c<<2)`;
  - **+0x248 = onCreate 0x4c4c84**: `+0x3a8 = 0x8bf0ec(0x16)`, `+0x3b0 = 0x19`, `+0x3b8 = 0x2e`, poi 0x4bfdd0;
  - +0x258 = onEnter 0x4c0898; +0x290 = uscita 0x4c3564; +0x2a0 = per frame 0x4c2820; +0x2d0 = eventi 0x4c3cdc; +0x2d8 = 0x4c24f4.
- Gli onCreate li chiama 0x8bad70 (vt+0x248, 0x8baf70) una volta per pagina, alla costruzione del menu. N non è noto in quel momento.
- 0x4bfdd0 costruisce tutto dalla pagina `+0x3a8` con `+0x39c` slot:
  - `0x8bb0ac(page,4)` → `+0x108/+0x110` (colore per giocatore);
  - PtrArray +0x3c0 (`L_KartSlot_0i`), +0x3d0/+0x3e0/+0x3f0 (`L_Body/L_Tire/L_Glider_00`, controlli 0x330, item del cursore tramite
    `[ctl+0x50]+0x1b0` 0x474b34), +0x450 (`L_KartPartsOKWin_0i`);
  - `0x8bb180(page,i,slot,1)` (colore);
  - `0x8bb2b8(page,+0x420)` (scanner in +0x198);
  - `B_MP_00` in ogni L_Name;
  - il blocco `**(+0x178)` (buffer di 2 parti, `0x8af1c4(obj, radice)`).
- Nel layout 8pl ci sono `L_KartSlot_00..07` e `L_KartPartsOKWin_00..07` sotto `N_Player_00..07` [VERIFICATO bflyt].

Opzioni:

| opzione | esito |
|---|---|
| kart sempre sulla 0x12 (8pl) | no: in 1-4P la kart sarebbe a 8 pannelli piccoli, quindi non identica |
| clonare la pagina kart con un id nuovo | no: costanti 0x31 in 20 siti del menu (indietro da regole, link, …) da rimappare |
| **secondo set nello stesso oggetto (B), scambiato all'ingresso** | **sì**: i riferimenti a 0x31 restano validi. 0x4bfdd0 è riusabile con pochi salvataggi di campi (§3.1) |

Quando costruire B: serve che **entrambe** le pagine (kart 0x31 e nomi 0x12) abbiano fatto l'onCreate. L'ordine dipende dalla registrazione
(0x12 è registrata dopo `Orig` di 0x40476c, quindi probabilmente dopo la 0x31) [IPOTESI]. Si costruisce B nel **secondo** dei due onCreate:
- trampolino nuovo sull'onCreate kart **0x4c4c84**;
- `CharaPartsCreateHook` esistente per la 0x12.

Entrambi girano nel thread di preparazione, come il vanilla [IPOTESI].

## 3. Costruzione e scambio dei set

### 3.1 Campi salvati (struct `p5::Set`)
| campo | contenuto | nota |
|---|---|---|
| +0x3a8 | pagina CharaParts | A = 0x16, B = 0x12 |
| +0x39c | numero di slot | A = 4, B = 8 |
| +0x3c0..+0x3ff | 4 PtrArray {count, cap, ptr} | 0x5fb2a4 alloca sempre un buffer nuovo, senza free [VERIFICATO 0x5fb2a4] |
| +0x450..+0x45f | PtrArray OKWin | |
| +0x108, +0x110 | colore per giocatore | 0x8bb0ac **senza** free: per B serve W1 = 8 (hook 0x4bfdf4) |
| +0x1b0 | cursore | B: `0x8bafb8(page, 32)`, ctor cur8 (magic) |
| `**(+0x178)` 0x18 B | {cap, buf, count} | 0x4c03e4 **libera** il buffer se c'è e non azzera count: prima di B si azzerano buf/cap/count, così A non viene liberato |
| +0x198 (count) | lista scanner | 0x8bb2b8 riaggiungerebbe +0x420: si ripristina il count |

`+0x1a8 |= 0x10` e `+0x440 = 0x1d` sono idempotenti. I controlli di B (8 L_KartSlot, 24 selettori, 8 OKWin, 8 B_MP) entrano nelle liste
di controlli della pagina (0x8bc0c4 → +0x38/+0x48/[+0x30]+8) e ricevono enter/update anche in modalità A: sono legati a pane della
0x12, che in A è chiusa [IPOTESI innocuo].

### 3.2 Ingresso, uscita, selezione
- **CharselExitHook (0x4bb6a0)**: se la destinazione è 0x2f..0x31, `p4::one` è attivo e B è costruito, la pagina nomi **resta aperta** e
  N_Player_00..03 della 0x16 restano nascosti; si fa solo `SetWide(sel,false)`. `p5::fromSel = true`.
- **KartEnterHook (0x4c0898)**, prima di Orig:
  - `Use(page, one)`;
  - `kartRound = one ? 0 : TwoRounds()`;
  - con one: `0x8afc54(curB)`, perché la page-enter 0x8bb454 ha già ripristinato il cursore A; poi `cur8::SetWide(page,true,N)`, prima del
    ciclo maschere 0x4c15e0;
  - se `fromSel`: `0x4c0490(page, 1)`, perché a 0x4bb650 il gioco ha inizializzato i selettori A, non quelli B.

  Dopo Orig:
  - PartsCursor 4..7 `ctl+0xb8 = page` (0x49af58 è srotolato su 4);
  - 3D nascosto.
- **KartExitHook nuovo (0x4c3564, vt+0x290)**, dopo Orig, se B è attivo:
  - PartsCursor 4..7 `+0xb8 = 0` e `0x49ae1c(pc,i,0)`;
  - `SetWide(page,false)`;
  - se si va avanti: chiusura della 0x16 (`0x8b258c(&0x16)`), che in B nessuno chiude, `p4::namesOpen = false`, `MultiNames(true)`. Orig
    chiude già +0x3a8 (= 0x12) e +0x3b0 (0x4c35c0..0x4c35e4).
  - Se si torna alla selezione, la 0x12 resta aperta e la selezione la riapre (OpenNames) [IPOTESI: 0x8b21dc su pagina già aperta è
    innocuo, come la 0x16 nel vanilla].
- Dalle regole verso la kart (0x32 → 0x31): l'onEnter apre `[+0x3b0]` e `[+0x3a8]` (0x4c108c..0x4c10bc), quindi con B la 0x12. La 0x16 è
  chiusa. Corretto.

## 4. Siti per slot nella kart (tutti [VERIFICATO])
| zona | indice | trattamento |
|---|---|---|
| onEnter 0x4c0dcc / 0x4c0e64 / 0x4c0edc / 0x4c0fe0 | stati selezione `+0xa28[i]` (clamp 0x4c0dd0, 0x4c0e68, 0x4c0f0c, 0x4c0fe4) | #8 |
| onEnter L_Entry `[+0x3a8]+0x1d8/+0x1e0` (0x4c0edc; 0x4c0f20 **non controllato**) | i < +0x39c | la 0x12 ha `+0x1d8` = w20 = 8 (0x4beb04..0x4bec1c). [IPOTESI] L_Entry_04..07 con `+0x70` valido: da verificare (crash a 0x4c0f30 se nullo) |
| init +0x460 0x4c1548 | clamp 0x4c154c (condiviso) | #8 + rilocazione |
| azzeramento 0x4c157c | [+0x39c, 4) | con 8 non si esegue; str rilocate |
| maschere 0x4c15e0 | `cmp w11,#4` 0x4c16d4 | #8 (per i < 4 cancella anche i bit 4..7, che di default sono 0) |
| `0x8af3f4(cur,i,3i+k)` 0x4c1820 | slot cursore i | cur8 V(cur,i) con cursore B largo |
| PartsCursor `0x49ae1c(pc,0..3,0)` 0x4c1870..0x4c18ac | srotolato | per 4..7 KartEnterHook (spenti) |
| scrittura parti 0x4c18cc → 0x4c1bfc | clamp 0x4c18d0 + slot cursore 0x4c192c | #8 + kPageSlotFix |
| 3D 0x4c1948..0x4c19c0 | `0x13d47c(mgr,i)` → `0x14a038` | **guardia 0x14a038** |
| colore `+0x110` 0x4c19dc | `csel hi` su +0x108 (i ≥ +0x108 → [0]) | B: +0x108 = 8 |
| nomi 0x4c1a84 / 0x4c1b84 | clamp 0x4c1aa4/0x4c1b8c, cursore 0x4c1b08, L_Name **non controllato** a 0x4c1ae4 | #8, kPageSlotFix, 0x12 ha 8 L_Name |
| 0x4c1bfc entry/cfg | `0x8bf158(i)` → padui extra (passo 4); cfg `+0x1fd4/+0x1fe4/+0x1ff4` clamp 0x4c1dd8/0x4c1e8c/0x4c1f40 | #8 + cfg8 (0x4c1dec, 0x4c1ea0, 0x4c1f54) |
| per frame 0x4c2860 (+/− e lente da L_Name) | clamp 0x4c2864 | #8 |
| macchina a stati 0x4c2948 | clamp solo +0x460 | #8 + rilocazione |
| **avanzamento 0x4c2a08** | clamp 0x4c2a10: tutti gli i < +0x39c in stato 7 con +0x460 == 3 | #8 + rilocazione. Slot N..7 in stato 1 ("vuoto") ignorati. Con `kartRound = 0` PageLinkHook non interviene |
| online 0x4c2a88 | 0x4c2a9c | #8 (non usato offline) |
| **annulla 0x4c2b40** | clamp 0x4c2b70 su +0x460, PartsCursor (0x4c2ba4), cursore (0x4c2bd8), 3D (0x4c2c70) | #8, fix PartsCursor, kPageSlotFix, guardia |
| conferma 0x4c2e6c | clamp 0x4c2f20 su +0x460 e PartsCursor (0x4c2fa0) | #8 + fix |
| 0x4c2fe4 / 0x4c3118.. / 0x4c34c4 / 0x4c44f4 | clamp 0x4c2ff8, 0x4c312c, 0x4c31a0, 0x4c3204, 0x4c350c, 0x4c451c | #8 |
| eventi 0x4c3cdc | ev+0x2c: clamp 0x4c3dc4 (cursore 0x4c3dd0), 0x4c3e40 (cursore 0x4c3e4c), 0x4c3fe4 (+0x460); 0x64/0x65: 0x4c3d74/0x4c3ed0 | #8 + kPageSlotFix |
| 0x4c4124 (statistiche) | selettori `[+0x3d8][slot]` **non controllati** | B ha 8 |
| uscita 0x4c3564 | cfg 0x4c37e0 solo i ≤ 1 (0x4c377c) | invariato |
| PartsPresent vt+0x2d8 | `0x27+0x238[clamp4 i]` (0x4c2658, 0x4c26cc, 0x4c2748) | restano a 4 (sicuri); hook 0x4c2768 riscrive [0..3] con gli slot 0..3 (altrimenti [0] = slot 7) |
| online 0x4c09f0, 0x4c37e0 | i ≤ 1 | invariati |

**PartsCursor (pagina 0xe)**:
- onCreate 0x49acdc: ciclo w22 = 0x41..0x3a, crea per `x25 ≤ 3` (0x49ad48), salva in `[x19, x22*8]` (+0x1d0..), colore
  `0x443738(ctl, i+1)`;
- 0x49ae1c(pc,i,on) con clamp 0x49ae2c;
- `0x49af58(pc,page)` imposta `+0xb8` su 4;
- Loop per frame 0x49ae90 su 4;
- posizione del controllo `0x44355c`: cursore `[ctl+0xb8]+0x1b0`, slot `ctl+0xc0` con clamp G3 0x443590, già cur8;
- `ColorChange` di `cm_L_KPSlotCursor_00` ha 8 frame [VERIFICATO bflan], quindi i colori P5-P8 = frame 5..8 [IPOTESI forte].

## 5. Tabelle (verificate con `p5_verify.py`, estratto: elenco completo in `p5_table.inc`)
- `kClampKart` (23) + `kClampKart2` (11: 6 su +0x460/cursore, 0x4c16d4, 3 cfg, 0x49ae2c): `cmp …,#4 → #8` (+0x1000).
- `kReloc460` (40):
  - 28 nel codice kart: `ldr/str w …#0x460/0x464` +0x4000, `add …#0x460/0x464` +0x10000;
  - 12 nei 3 ctor (`str wzr,[x19,#0x460/468/470/478]` → +0x4a0/4a8/4b0/4b8). Nessun altro accesso w a +0x460..+0x47f nella classe 0x4bfdd0..0x4c4ee0
    [VERIFICATO scansione; gli `ldr x…,#0x460` a 0x4bf4c0.. e 0x4c5018.. sono di altre classi e a 64 bit].
- `kFactory` (3): 0x409248, 0x409274, 0x4092a0 `52809400 → 52809c00` (0x4a0 → 0x4e0).
- `kPartsCursor` (2): 0x49ad48 `f1000f3f → f1001f3f`, 0x408f28 `321d17e0 → 52804600` (0x1f8 → 0x230).
- `kHookWords` (18, prime parole):

| off | parola | tipo | scopo |
|---|---|---|---|
| 0x4bfdf4 | aa0003f3 | I | `mov x19,x0` in 0x4bfdd0: W1 = 8 durante la costruzione di B (0x8bb0ac) |
| 0x4c192c | b9803108 | I | kPageSlotFix, cursore = `[x19+0x1b0]` |
| 0x4c1b08 | b9803108 | I | kPageSlotFix T8 C9 (= P4SlotFixEnter) |
| 0x4c2bd8 | b9803129 | I | kPageSlotFix T9 C8 (= P4SlotFixGift) |
| 0x4c3dd0 | b980314a | I | kPageSlotFix T10 C9 (nuovo) |
| 0x4c3e4c | b9803129 | I | kPageSlotFix T9 C8 (= P4SlotFixGift) |
| 0x4c1dec / 0x4c1ea0 / 0x4c1f54 | 8b090908 | I | Cfg8AddB8I9 (+0x1fd4 / +0x1ff4 / +0x1fe4) |
| 0x49ad98 | 5100eac8 | I | onCreate PartsCursor: `x26 += 0x40` se x22 ≥ 0x3e |
| 0x49ae34 | 8b080c08 | I | 0x49ae1c: `x8 = w1+4` per w1 in 4..7 |
| 0x4c2ba4 | 8b160c08 | I | annulla: `x0 += 0x20` se x22 in 4..7 |
| 0x4c2fa0 | 8b150c08 | I | conferma: `x0 += 0x20` se x21 in 4..7 |
| 0x4c2768 | f9400529 | I | PartsPresent +0x238[0..3] |
| 0x4c4c84 | d10103ff | T | onCreate kart Multi → costruzione B |
| 0x4c3564 | d10283ff | T | vt+0x290 uscita kart |
| 0x14a038 | 91010008 | T | guardia modello nullo |
| 0x49ae90 | f81d0ff5 | T | Loop KPSlotCursor 4..7 (opzionale, estetico) |

Collisioni con gli indirizzi già in `main.cpp`: **nessuna** [VERIFICATO p5_verify]. Nessun hook cade su una parola patchata.

## 6. Codice (da integrare in main.cpp, dopo il blocco p4; tabelle da `p5_table.inc`)
```cpp
/* ---- step 5b passo 5 (step5b_plan_p5.md): pagina kart a 8 slot a turno unico (set B sulla pagina nomi 0x12) ---- */
#ifndef MK8DX_P5_HIDE3D
#define MK8DX_P5_HIDE3D 1        /* 1 = a turno unico i 4 modelli 3D (posizioni Multi) sono nascosti */
#endif
namespace p5 {
    /* #include "p5_table.inc" */
    struct Set { uint8_t* p3a8; int32_t n39c; uint8_t arr[0x40]; uint8_t okw[0x10]; int32_t c108; uint8_t* p110; uint8_t* cur; uint8_t o178[0x18]; };
    static bool on, building, isB, fromSel;
    static uint8_t* built;                       /* pagina kart con set B costruito (nullptr = no) */
    static Set A, B;
    static uint8_t* Obj178(uint8_t* pg) { uint8_t** q = *(uint8_t***)(pg + 0x178); return q ? *q : nullptr; }   /* come 0x4c03e4 */
    static void Save(uint8_t* pg, Set& s) {
        s.p3a8 = *(uint8_t**)(pg + 0x3a8); s.n39c = *(int32_t*)(pg + 0x39c);
        memcpy(s.arr, pg + 0x3c0, 0x40); memcpy(s.okw, pg + 0x450, 0x10);
        s.c108 = *(int32_t*)(pg + 0x108); s.p110 = *(uint8_t**)(pg + 0x110); s.cur = *(uint8_t**)(pg + 0x1b0);
        if (uint8_t* o = Obj178(pg)) memcpy(s.o178, o, 0x18);
    }
    static void Load(uint8_t* pg, const Set& s) {
        *(uint8_t**)(pg + 0x3a8) = s.p3a8; *(int32_t*)(pg + 0x39c) = s.n39c;
        memcpy(pg + 0x3c0, s.arr, 0x40); memcpy(pg + 0x450, s.okw, 0x10);
        *(int32_t*)(pg + 0x108) = s.c108; *(uint8_t**)(pg + 0x110) = s.p110; *(uint8_t**)(pg + 0x1b0) = s.cur;
        if (uint8_t* o = Obj178(pg)) memcpy(o, s.o178, 0x18);
    }
    static void BuildB(uint8_t* pg) {
        uint8_t* nm = p4::Names();
        if (!on || !pg || !nm || built == pg || *(int32_t*)(pg + 0xe8) != 0x31 || *(int32_t*)(pg + 0x3c0) != 4) return;
        uint8_t* o = Obj178(pg); if (!o) return;
        Save(pg, A);
        int32_t n198 = *(int32_t*)(pg + 0x198);
        *(uint8_t**)(pg + 0x3a8) = nm; *(int32_t*)(pg + 0x39c) = 8;
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x8bafb8)(pg, 32);                 /* cursore B: 8 slot x 4 item (come 0x4c4ed0) */
        *(uint64_t*)(o + 8) = 0; *(int32_t*)o = 0; *(int32_t*)(o + 0x10) = 0;  /* 0x4c03e4 non libera il buffer A */
        building = true; mk8dx::Fn<void (*)(uint8_t*)>(0x4bfdd0)(pg); building = false;
        *(int32_t*)(pg + 0x198) = n198;                                         /* 0x8bb2b8 ha riaggiunto +0x420 */
        for (int k = 0; k < 8; k++) { *(int32_t*)(pg + 0x4a0 + 8 * k) = 0; *(int32_t*)(pg + 0x4a4 + 8 * k) = 0; }
        Save(pg, B); Load(pg, A); built = pg; isB = false;
        Logging.Log("[mk8dx-8p] p5: set B kart costruito: slot=%d sel=%d okw=%d cur=%p (A cur=%p)", *(int32_t*)(B.arr + 0),
                    *(int32_t*)(B.arr + 0x10), *(int32_t*)B.okw, B.cur, A.cur);
    }
    static void Use(uint8_t* pg, bool b) {
        if (built != pg || b == isB) return;
        Save(pg, isB ? B : A); Load(pg, b ? B : A); isB = b;
        Logging.Log("[mk8dx-8p] p5: kart set %c (+0x39c=%d)", b ? 'B' : 'A', *(int32_t*)(pg + 0x39c));
    }
    static uint8_t* PcCtl(int i) { uint8_t* pc = cur8::GfxPage() ? mk8dx::Fn<uint8_t* (*)()>(0x8bf2c8)() : nullptr;
                                   return pc ? *(uint8_t**)(pc + 0x1d0 + (i < 4 ? i : i + 4) * 8) : nullptr; }
    static void Hide3D() { if (MK8DX_P5_HIDE3D) for (int i = 0; i < 4; i++) mk8dx::Fn<void (*)(int, bool)>(0x14a7ac)(i, false); }
}
HOOK_DEFINE_INLINE(KartBuildCountHook) {               /* 0x4bfdf4 mov x19,x0: W1 = conteggio di 0x8bb0ac(+0x108/+0x110) */
    static void Callback(exl::hook::InlineCtx* ctx) { if (p5::building) ctx->W[1] = 8; }
};
HOOK_DEFINE_INLINE(P5SlotFixPage) {                    /* 0x4c192c: x8 = cur+i*0x20, cur = [x19+0x1b0] */
    static void Callback(exl::hook::InlineCtx* ctx) { uint8_t* c = *(uint8_t**)(ctx->X[19] + 0x1b0);
        if (cur8::Wide(c) && ctx->X[8] - (uintptr_t)c >= 0x80) ctx->X[8] += 0x80; }
};
P4_SLOTFIX(Ev, 10, 9)                                  /* 0x4c3dd0 */
HOOK_DEFINE_INLINE(PcCreateHook) {                     /* 0x49ad98: [x19, x26] con x26 = x22*8 -> slot 4..7 a +0x210.. */
    static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->X[22] >= 0x3e) ctx->X[26] += 0x20; }   /* corretto in Eden: +0x40 usciva dalla pagina 0x230 */
};
HOOK_DEFINE_INLINE(PcIndexHook) {                      /* 0x49ae34 add x8,x0,x8,lsl #3 (cmp 0x49ae2c a #8) */
    static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->W[1] - 4u < 4) ctx->X[8] = ctx->W[1] + 4; }
};
#define P5_PCFIX(N, I) HOOK_DEFINE_INLINE(P5PcFix##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    if (ctx->X[I] - 4 < 4) ctx->X[0] += 0x20; } };
P5_PCFIX(Cancel, 22) /* 0x4c2ba4 */  P5_PCFIX(Confirm, 21) /* 0x4c2fa0 */
HOOK_DEFINE_INLINE(PartsPresentFixHook) {              /* 0x4c2768: x8 = pagina 0x27, x9 = &PtrArray tipo, x19 = kart */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (*(int32_t*)(ctx->X[19] + 0x39c) <= 4) return;
        uint8_t** a = *(uint8_t***)(ctx->X[9] + 8);
        for (int k = 0; k < 4; k++) *(uint64_t*)(ctx->X[8] + 0x238 + 8 * k) = a[k] ? (uintptr_t)a[k] + 0x30 : 0;
    }
};
HOOK_DEFINE_TRAMPOLINE(KartCreateHook) {               /* 0x4c4c84 onCreate kart Multi */
    static void Callback(uint8_t* pg) { Orig(pg); p5::BuildB(pg); }
};
HOOK_DEFINE_TRAMPOLINE(Model3DModeHook) {              /* 0x14a038(model, a, b) -> 0x148b50 legge [x0+0xd7] */
    static void Callback(uint8_t* m, bool a, bool b) { if (m) Orig(m, a, b); }
};
HOOK_DEFINE_TRAMPOLINE(KartExitHook) {                 /* 0x4c3564 vt+0x290 */
    static void Callback(uint8_t* pg) {
        uint8_t* l = *(uint8_t**)(pg + 0x168); int32_t dest = l ? *(int32_t*)(l + 0xc) : -1;
        bool b = p5::isB && p5::built == pg;
        Orig(pg);
        if (!b) return;
        for (int i = 4; i < 8; i++) { if (uint8_t* c = p5::PcCtl(i)) *(uint8_t**)(c + 0xb8) = nullptr;
            uint8_t* pc = mk8dx::Fn<uint8_t* (*)()>(0x8bf2c8)(); if (pc) mk8dx::Fn<void (*)(uint8_t*, int, bool)>(0x49ae1c)(pc, i, false); }
        cur8::SetWide(pg, false, 0);
        if (dest < 0x2c || dest > 0x2e) {                                       /* avanti: Orig ha chiuso +0x3a8 (= 0x12) e +0x3b0 */
            int32_t id = 0x16; mk8dx::Fn<void (*)(int32_t*)>(0x8b258c)(&id);
            p4::namesOpen = false; p4::MultiNames(*(uint8_t**)(pg + 0x3b8), true);
        }
        Logging.Log("[mk8dx-8p] p5: uscita kart B verso 0x%x", dest);
    }
};
HOOK_DEFINE_TRAMPOLINE(KpLoopHook) {                   /* 0x49ae90 (opzionale): Loop dei KPSlotCursor 4..7 */
    static void Callback(uint8_t* pc) {
        Orig(pc); if (!p5::isB) return;
        for (int k = 0; k < 4; k++) { uint8_t* c = *(uint8_t**)(pc + 0x210 + 8 * k);
            if (c && mk8dx::Fn<bool (*)(uint8_t*)>(0x8adf84)(c + 0x30)) mk8dx::Fn<void (*)(uint8_t*, float)>(0x44375c)(c, *(float*)(pc + 0x1f0)); }
    }
};
```
Modifiche agli hook esistenti:
```cpp
// CharaPartsCreateHook (0x4be8d0), in coda: se la pagina e' la 0x12 e la kart esiste gia', costruire B qui
        if (p5::on && cid == p4::kNamesId) { int32_t k = 0x31; p5::BuildB(mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&k)); }
// MenuPagesSetupHook (0x40476c), accanto a p4::reg = false:
        p5::built = nullptr; p5::isB = false; p5::fromSel = false;
// CharselExitHook (0x4bb6a0): dopo il controllo CharaList, prima di p4::CloseNames
        if (dest >= 0x2f && dest <= 0x31) p5::fromSel = true;
        if (dest == 0x31 && p4::one && p5::built) { cur8::SetWide(page, false, 0);
            Logging.Log("[mk8dx-8p] p5: verso la kart B, pagina nomi aperta"); return; }
// KartEnterHook (0x4c0898): sostituisce il corpo attuale
    static void Callback(uint8_t* page) {
        bool one = p4::one && p5::built == page;
        p5::Use(page, one);
        padui::round2 = false; sel8::kartRound = (!one && sel8::TwoRounds()) ? 1 : 0;
        if (!one && p4::namesOpen) p4::CloseNames(*(uint8_t**)(page + 0x3b8));
        if (one) {
            mk8dx::Fn<void (*)(uint8_t*)>(0x8afc54)(*(uint8_t**)(page + 0x1b0));      /* 0x8bb454 aveva ripristinato il cursore A */
            cur8::SetWide(page, true, menu8::LocalCount());                          /* prima del ciclo maschere 0x4c15e0 */
            if (p5::fromSel) mk8dx::Fn<void (*)(uint8_t*, bool)>(0x4c0490)(page, true);   /* come 0x4bb650, sui selettori B */
        }
        p5::fromSel = false;
        Orig(page);
        if (one) { for (int i = 4; i < 8; i++) if (uint8_t* c = p5::PcCtl(i)) *(uint8_t**)(c + 0xb8) = page;   /* come 0x49af58 */
                   p5::Hide3D(); Logging.Log("[mk8dx-8p] p5: kart a turno unico, +0x39c=%d", *(int32_t*)(page + 0x39c)); }
        if (sel8::kartRound) { /* log esistente */ }
    }
// KartFrameHook (0x4c2820) e CharselFrameHook (0x4ba77c): con p4::one -> p5::Hide3D() dopo Orig
```
`p4::MultiNames` è `static` nello stesso file: va bene. `cur8::GfxPage()` si usa solo per sapere se il menu è attivo; si può anche togliere.

## 7. Installazione (exl_main, dopo il blocco passo 4, tutto o niente)
```cpp
    if (p4::on) {
        bool ok = true;
        for (auto* t : {p5::kClampKart, ...})  /* verifica orig di kClampKart, kClampKart2, kReloc460, kFactory, kPartsCursor e kHookWords */
        ...
        if (ok) {
            p5::on = true;
            exl::patch::RandomAccessPatcher rp;                 /* 1) 79 parole */
            /* kClampKart, kClampKart2, kReloc460, kFactory, kPartsCursor */
            rp.Flush();
            /* 2) inline */
            KartBuildCountHook::InstallAtOffset(0x4bfdf4);
            P5SlotFixPage::InstallAtOffset(0x4c192c); P4SlotFixEnter::InstallAtOffset(0x4c1b08);
            P4SlotFixGift::InstallAtOffset(0x4c2bd8); P4SlotFixEv::InstallAtOffset(0x4c3dd0); P4SlotFixGift::InstallAtOffset(0x4c3e4c);
            Cfg8AddB8I9::InstallAtOffset(0x4c1dec); Cfg8AddB8I9::InstallAtOffset(0x4c1ea0); Cfg8AddB8I9::InstallAtOffset(0x4c1f54);
            PcCreateHook::InstallAtOffset(0x49ad98); PcIndexHook::InstallAtOffset(0x49ae34);
            P5PcFixCancel::InstallAtOffset(0x4c2ba4); P5PcFixConfirm::InstallAtOffset(0x4c2fa0);
            PartsPresentFixHook::InstallAtOffset(0x4c2768);
            /* 3) trampolini */
            KartCreateHook::InstallAtOffset(0x4c4c84); KartExitHook::InstallAtOffset(0x4c3564);
            Model3DModeHook::InstallAtOffset(0x14a038); KpLoopHook::InstallAtOffset(0x49ae90);
        } else Logging.Log("[mk8dx-8p] p5: passo 5 NON attivato, kart a due turni");
    }
```
- Tutto in exl_main, prima della costruzione del menu: fabbriche 0x4e0 e 0x230, onCreate PartsCursor.
- Le parole e il Flush vengono prima degli hook. Nessun hook cade su una parola patchata.
- Il blocco richiede p4 (`p4::Names`, cfg8, CharselExitHook) e cur8.
- Il guard **0x14a038** si può installare anche fuori dal blocco: protegge già oggi lo stesso percorso nella selezione (0x4b8614).

## 8. Requisito 1-4P identico
- Rilocazione +0x460 → +0x4a0: semantica invariata, anche per le kart Single/WiFi (stessi ctor rilocati).
- Clamp #8: con `+0x39c ≤ 4` gli indici restano < 4. Fanno eccezione:
  - la maschera 0x4c16d4, che per i < 4 ora cancella anche i bit 4..7 degli item, bit che nel vanilla sono già 0
    (0x8ad8d4 = 0x8000000f) [VERIFICATO];
  - 0x49ae2c, con w1 sempre < 4.
- Hook: scattano solo con indice 4..7, `p5::building`, set B, o `p4::one`. 0x14a038 cambia comportamento solo con modello nullo, che oggi
  sarebbe un crash.
- **PartsCursor**: i 4 controlli 04..07 vengono ora creati in tutte le modalità e spenti da `0x8ae098(ctl+0x30, 0)` (0x49adbc), invece di
  nascondere il pane (0x49addc). [IPOTESI forte] invisibili. Prova: 1P/4P kart senza cornici in più. Se compaiono, CharaPartsCreateHook o
  KartEnterHook in A riapplicano `pane+0x58 &= ~1`, come fa `cur8::ShowPanes(false)`.
- Pagina kart più grande (0x4e0) e una CharaParts 8pl con controlli kart in più: memoria del menu [IPOTESI sufficiente; il passo 4 non ha
  mostrato problemi].

## 9. Prova in Eden
**Log all'avvio**: nessuna `parola/hook inattes*`. Nel menu: `p5: set B kart costruito: slot=8 sel=8 okw=8 cur=…`, una volta per menu
(anche dopo una gara).

**1P, 2P, 4P**:
- kart Multi/Single invariata, nessuna riga `p5: kart set B`;
- cornici, nomi, 3D e annulla come prima;
- avanzamento e regole;
- ritorno dalle regole (0x4c0490 flag 0).

**8P** (passo 4 attivo):
1. Selezione a turno unico → `p5: verso la kart B, pagina nomi aperta` → `p5: kart set B (+0x39c=8)`, `cur8: cursore … della pagina 0x31 a 8
   slot`, `p5: kart a turno unico, +0x39c=8`.
2. A video [IPOTESI]: 8 selettori su due colonne (layout 8pl, come `ref_switch2_kart_8p.jpg`), con nomi delle parti e −/+/lente nelle L_Name
   0..7, cornici KPSlotCursor colorate per 8 giocatori e anteprime 3D nascoste.
3. Ognuno cambia carrozzeria/gomme/aliante con il proprio controller.
   - Log `A2: personaggio…` assenti: scrive 0x4c1bfc.
   - Con `LogAssigned` all'avanzamento: e0..e7 con parti ≠ −1, scelte da ciascuno.
4. A per confermare (stato 2 → 3 in 25 frame), B per annullare (0x4c2b40), anche per P5-P8.
5. Quando gli 8 slot sono in stato 3: `A2 kart Multi -> avanti` (una sola volta, nessun `kart turno 2`) → regole → coppa → gara con le
   parti scelte. Watchpoint su `kart+0x480..+0x49f`: non deve scattare (vecchia area +0x460 non più usata).
6. B dalla kart → selezione a turno unico con la pagina nomi visibile. Di nuovo la kart → set B.
7. Dalle regole → indietro → kart B (0x12 riaperta dall'onEnter, 0x16 chiusa).

**5P/7P**: gli slot N..7 sono "vuoti" (stato selezione 1, kart 0) e l'avanzamento avviene con N confermati. Senza giocatore nessuno muove
quei selettori.

Diagnostica:
- `cfg8::hits` cresce con le parti di P5-P8;
- breakpoint a 0x148b6c con x19 = 0: non deve mai essere raggiunto (guard);
- dump di `kart+0x3c0..+0x45f` in A e in B.

## 10. Rischi
| rischio | prob. | mitigazione |
|---|---|---|
| ordine degli onCreate (0x12 dopo la 0x31) o creazione su thread diversi | media | B costruito nel secondo onCreate; log di BuildB; in alternativa costruzione pigra al primo KartEnterHook con one |
| `**(+0x178)` condiviso tra pagine (non solo kart) | media | log del puntatore in BuildB; se è condiviso si salva/ripristina solo in Use e non si azzera in BuildB |
| L_Entry_04..07 della 0x12 con `+0x70` nullo (0x4c0f2c, non controllato) | media | prima prova con un breakpoint a 0x4c0f30; se nullo, hook inline 0x4c0f2c (salto se x9 nullo) |
| `+0x190` della kart senza bit 4..7: P5-P8 senza focus (Cur8PageFocusHook/SetFoc) | media | in KartEnterHook con one `+0x190 \|= 0xf0` se mancano [IPOTESI] |
| colori `+0x110` e `ColorChange` frame 5..8 su L_KartSlot 8pl | bassa, estetica | screenshot |
| 0x8b21dc sulla 0x12 già aperta (ritorno alla selezione) | bassa | log PageOpenHook |
| difetto (1) H1 (regalo per P1) presente anche in B | media | §1.1; vt+0x2d8 resta solo slot 0, PartsPresent corretta da 0x4c2768 |
| doppio set di controlli in update per frame | bassa (CPU) | solo nel menu kart |

## 11. Capienza JIT
- Inline: 0x4bfdf4, 5 kPageSlotFix, 3 cfg8, 0x49ad98, 0x49ae34, 0x4c2ba4, 0x4c2fa0, 0x4c2768 = **14**.
- Trampolini: 0x4c4c84, 0x4c3564, 0x14a038, 0x49ae90 = **4** (0x49ae90 opzionale).

Da circa 186 a circa **204** voci, su 327 (`JitSize` 0x10000). Parole patchate: **79** (34 clamp, 40 rilocazione, 3 fabbriche, 2 PartsCursor).
