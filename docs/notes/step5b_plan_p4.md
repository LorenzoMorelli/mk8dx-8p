# Step 5b – passo 4: selezione Multi a turno unico con N cursori (5..8), pagina kart ancora a due turni (piano statico)

Data: 3 nov 2026. Solo analisi statica: `src/` non modificato, nessuna build. Base: `../img/main.bin` (offset = indirizzo), `../img/main.ann.asm`.
Dati in `step5b_data (dati non pubblicati)/` (prefisso `p4_`):
- `p4_verify.py` → `p4_verify.txt` (**0 errori**) e `p4_table.inc`: verifica tutte le parole (orig/nuova disassemblate con llvm-objdump),
  le prime parole degli hook, la tabella 0xf200a8 e le collisioni con gli indirizzi già usati in `main.cpp` (nessuna).
- `p4_cfg_uses.txt` (`p4_cfgslice.py`, `p4_uses.py`): tutti gli accessi a cfg+0x1fb4..+0x2083 nella zona 0x3f0000-0x4e0000, con il clamp che
  produce l'indice e tutti gli usi del registro dopo il `csel`.
- `p4_8c0_readers.txt`: i 34 lettori e i 3 scrittori di +0x8c0.

Etichette: [VERIFICATO] = letto in main.bin/main.ann.asm o nei file RomFS (indirizzo o file indicato); [IPOTESI] = da confermare in Eden.

## 0. Sintesi
- **Layout (a)**: la pagina 0x16 resta **sempre Multi**; si aggiunge una **seconda pagina CharaParts "nomi" con layout 8pl sull'id 0x12**
  (fabbrica di 0x16). La selezione usa la 0x16 per la griglia (48 L_Chara legati in onCreate, B_MP, N_ConSuppo) e la pagina nomi per
  i pannelli L_Name/N_Player/N_Window. La griglia è **identica nei due layout** (N_Chara_00 (0,25), L_Chara_00 (-270,192), L_Chara_47
  (269,-193), scala 0.58) [VERIFICATO bflyt]. Le 19 letture `[sel+0x728]` che portano a L_Name (+0x1e8/+0x1f0) vengono spostate su un
  campo nuovo **+0xce8** (oggetto selezione 0xce8 → **0xcf0**). In 1-4P vale +0xce8 = +0x728, quindi il comportamento non cambia.
- **+0x8c0 (b)**: va impostato in **CharselEnterHook (0x4b74a4), prima di Orig**, a **8** per N = 5..8 (gli slot N..7 diventano "vuoti",
  stato 1, come lo slot 3 del vanilla in 3P) e a **4** negli altri casi. Dei 34 lettori, l'unico che va in crash con un valore > 4 è
  **0x4694fc** (posizione della finestra varianti): mappa 1/2/4 → id 0x14/0x15/0x16 e per gli altri valori usa -1, quindi NULL a
  0x4697dc. Si corregge con un hook inline a 0x469760.
- **Clamp (c)**: **50 parole `cmp #4 → #8`**:
  - 16 dei 39 `kClamp904` (selezione e regali; i 23 della pagina kart restano a 4 fino al passo 5);
  - 6 del passo 2;
  - 28 per l'ombra della config.

  Si aggiungono **4 hook kPageSlotFix** e **38 hook inline** sulle `add xB,xB,xI,lsl #2` della config: con 4 ≤ xI < 8 la base diventa
  un'ombra `int32 sh[13][4]` nel modulo.
- **3D (d)**: **0x13d424 non controlla il limite** (0x13d428 `ldr x9,[x9,w1,sxtw #3]` e scritture +0x350/+0x354/+0x361 *prima* del
  controllo) e viene chiamata con lo slot in 7 punti: serve un trampolino di guardia. Le altre funzioni per slot (0x13d47c, 0x13d4ac,
  0x14a7ac..0x14b824) sono null-safe [VERIFICATO]. 0x49ae1c (pagina 0xe = PartsCursor) è chiamata solo dalla pagina kart.
- **Entry (e)**: con `padui::round2 = false` la catena slot i → `0x8bf158(i)` → `0x8be03c` (C1 `cmp #7`) → `0x86ebf8` (B1i `cmp #7`) →
  raceMgr+0x17c[i] scrive il **locale i** anche per i = 4..7 [VERIFICATO]. L'avanzamento 0x4ba938 (stati 7 con i < +0x8c0 == cfg+0x1fa4)
  funziona da solo con N stati 7. La pagina kart resta a due turni (S5.4 invariata).
- **Totale passo 4**:
  - **72 parole**: 50 clamp, 19 `+0x728→+0xce8`, 3 fabbriche;
  - **44 hook inline**: 38 cfg8, 4 kPageSlotFix, 0x469760, 0x4be8fc;
  - **3 trampolini nuovi**: 0x4bb6a0, 0x13d424, 0x43ce4c;
  - **modifiche** a 7 hook esistenti.

  In tutto **47 voci JIT in più**: circa 139 installazioni oggi, circa 186 dopo il passo 4, su 327 disponibili con `JitSize` 0x10000
  [VERIFICATO hook_impl.cpp:56-59].

## 1. (a) Layout: invariante +0x8c0 ≤ w20 della pagina CharaParts

### 1.1 Fatti [VERIFICATO]
- Le pagine nascono all'ingresso nel menu (0x40476c registra; 0x16 a 0x404870, fabbrica `x21 = [GOT 0x1307ad0]` = **0x409070**: alloc
  0x200, ctor 0x4be890; layer w20 = 0xa, 0x404798). In quel momento N non è noto. Multi +0x8c0 = 4 nel ctor (0x4bcc70 `321e03e8`).
- onCreate Multi 0x4bcc84: +0x728 = `0x8bf0ec(0x16)` con cast (0x4bcd1c), +0x730 = 0x19 (0x4bcda8), poi **0x4b5688**. 0x4b5688 crea le 48
  L_Chara sotto `[[+0x728]+0x1d0]+0x30` (0x4b56ac), cioè i controlli della griglia, che sono anche gli item del cursore. Lega anche B_MP_00
  (+0x738), che sta in **N_ConSuppo_00** della parte.
- onEnter 0x4b74a4 apre BG e CharaParts per id: `0x8b21dc(&[+0x730].id,1)` (0x4b7d24) e `0x8b21dc(&[+0x728].id,0)` (0x4b7d3c).
  L'uscita vt84 **0x4bb6a0** (0x8bb368 → `vt+0x290`) le chiude con `0x8b258c` (0x4bb714/0x4bb728), **salvo** quando la destinazione
  (`[page+0x168]+0xc`, 0x8ba5a0) è 0x2f..0x31 (kart, 0x4bb6d0..0x4bb6dc). In quel caso la kart riusa la 0x16: kart Multi +0x3a8 =
  `0x8bf0ec(0x16)` (0x4c4c98..0x4c4d1c) e le sue L_Name a 0x4c0300.., 0x4c1ac8...
- Usi di `[sel+0x728]` (28 siti, `grep #0x728]`): **19 → L_Name** (+0x1e8 conteggio, +0x1f0 array; tabella §7), 4 → parte +0x1d0
  (0x4b56ac griglia, 0x4b5ea8, 0x4b7ca4, 0x4bb538), 2 → id +0xe8 (0x4b7d28 apertura, 0x4bb718 chiusura), 3 scritture (onCreate
  Single/WiFi/Multi).
- Altri utenti dell'id 0x16 con costante: solo 0x4bcc98 e 0x4c4c98 (scanner su 0x8bf0ec/0x8b21dc/0x8b258c/0x8b21cc/0x8b2314/0x8b3cb0).
  0x4694fc lo prende dalla tabella 0xf200a8 = {0x14, 0x15, -1, 0x16} con indice +0x8c0−1 (0x46972c..0x469758).
- **Pane** (bflyt estratti da `romfs-tmp/menu4/*.sarc`):

| pane | Multi | 8pl |
|---|---|---|
| N_Player_0i (L_Name, L_KartSlot, L_KartPartsOKWin, N_Window e, solo in Multi, L_CameraPlay) | 4: (±290, 185 / −115), L_Name scala 1.0 | 8: (±340, 344/186/28/−130), L_Name scala 0.67, nessun L_CameraPlay |
| N_Chara_00 + L_Chara_00..47 | (0,25), L_Chara_00 (−270,192), _05 (115,192), _47 (269,−193), scala 0.58 | **identico** |
| N_ConSuppo_00 (B_MP_00, W_Base, P_CSIcon, P_BtnIcon) | (0,−283) | identico |

### 1.2 Opzioni
| opzione | esito |
|---|---|
| (1) 8pl sempre per la 0x16 | **no**: in 1-4P i 4 pannelli diventerebbero piccoli (0.67) e in due colonne, con 4 pannelli vuoti, nessun L_CameraPlay e le finestre varianti in posizioni diverse (0x4694fc legge N_Player/N_Window della 0x16). 1-4P non sarebbe identico [VERIFICATO bflyt] |
| (2) ricostruire la 0x16 quando cambia N | **no**: nessuna distruzione per singola pagina nel framework. Le pagine nascono con la registrazione (0x8b3cb0 → 0x726330) e muoiono con la scena (0x8b3b6c); +0x740[48], +0x738, il cursore della selezione e +0x3a8 della kart puntano a controlli della vecchia parte [VERIFICATO; ricreazione IPOTESI scartata] |
| (3) seconda CharaParts 8pl e +0x728 rediretto | **sì, ma solo per i nomi**. Spostare tutto +0x728 sulla 8pl lascerebbe la griglia visibile senza controlli, perché le L_Chara sono legate alla 0x16 in onCreate e non ci sono altri momenti di legame. Si redirigono solo le 19 letture L_Name; la griglia, B_MP e N_ConSuppo restano sulla 0x16 |
| (3') seconda selezione completa (classe 0x2e clonata) | scartata: richiede 2 id e la ridirezione dei riferimenti costanti a 0x2e (kart +0x3b8 0x4c4db4, regali 0x4b10ac, link indietro della kart) |

### 1.3 Decisione: "pagina nomi" 0x12 + campo +0xce8
- **Id 0x12** (DebugLocalRoom): nel retail non viene mai registrato [VERIFICATO p1 §4] ed è libero nel menu prima di ogni gara
  (`eden_log_5b2_pagetab.txt`). Dopo una gara a 5-8 contiene la fabbrica RaceView di `hud8` (`[GOT 0x1307d68]`, scritta senza condizioni da
  `RacePagesSetupHook`) con il proprietario della scena di gara ormai distrutto. La mod quindi lo sovrascrive a ogni menu, accettando
  fabbrica nulla, CharaParts o RaceView. Gli id 0x71/0x73/0x78..0x83 sono esclusi perché sono id di gara; 0x01/0x02/0x5e/0x64 sono di
  cw8; 0x1d/0x43/0x5b restano a hud8.
- **Registrazione** in `MenuPagesSetupHook` (0x40476c, già presente): fabbrica `[GOT 0x1307ad0]`, proprietario = scena, `0x8b3cb0(scena,
  &0x12, 0xa)` come 0x4048a4..0x4048b0.
- **onCreate** 0x4be8d0 sceglie il ramo con l'id copiato in `[sp+0x4c]` (0x4be8fc), riletto a 0x4be900/0x4beb30/0x4bee4c. Un **hook inline
  a 0x4be8fc** (`b9004fe8 str w8,[sp,#0x4c]`) porta w8 = 0x16 se vale 0x12. Così tutto l'onCreate segue il ramo Multi: parte
  "mn_CharaKartSlct_Multi", presente con lo stesso nome nel Page_Lyt 8pl (A1 §1.2). Nella classe solo 0x4be858 (slot 13) e 0x4be8f8 leggono
  +0xe8 [VERIFICATO grep]. Il nome del layout (0x4be854) e w20 = 8 (0x4beb04) restano agli hook A1 esistenti, ora legati all'id 0x12.
  Questa configurazione è già stata provata in Eden con `MK8DX_LYT8=8`: il layout 8pl si carica, con 8 pannelli statici.
- **Campo +0xce8** (puntatore alla pagina da cui leggere le L_Name):
  - 19 parole `ldr xD,[xB,#0x728]` → `#0xce8` (imm12 0xce8/8 = 0x19d), tutte verificate;
  - fabbriche `mov w0,#0xce8` → `#0xcf0` (`52819d00 → 52819e00`);
  - valore impostato in CharselEnterHook prima di Orig (vale anche per Single/WiFi, che condividono vt77): pagina nomi se si gioca a turno
    unico, altrimenti +0x728.

  Nessuno dei 19 siti è raggiungibile prima del primo onEnter: 0x4b5688 non chiama 0x4b43b4/0x4be3xx [VERIFICATO]; CharaList, vt92 e vt93
  esistono solo dopo l'onEnter.
- **Apertura e visibilità** (solo a turno unico), dopo `Orig` di CharselEnterHook:
  - `0x8b21dc(&0x12, 0)`, come 0x4b7d3c;
  - nella pagina nomi si nascondono **N_Chara_00** e **N_ConSuppo_00**, dopo l'apertura, perché l'onEnter CharaParts 0x4bf0b0..0x4bf0d8
    riaccende N_ConSuppo_00 (stringa 0xeeb5f0);
  - nella 0x16 si nascondono **N_Player_00..03**.

  Pane via `0x8ad8e8(part+0x30, &SafeString)` + `0x8ade74(pane, on)` [VERIFICATO stesso schema in 0x4697dc..0x46980c e hud8::SetVisible].
  La pagina nomi non prende il focus dei pad: l'onCreate CharaParts fa `0x8ba5f0(page,0)` → +0x190 = 0 (0x4beef8) [VERIFICATO].
- **Chiusura**: trampolino su **vt84 0x4bb6a0** (`d10343ff`). Dopo Orig, se la pagina nomi è aperta e la destinazione non è una CharaList,
  si chiude la pagina nomi (`0x8b258c(&0x12)`), si riaccendono N_Player_00..03 della 0x16 (la kart li riusa) e si chiama
  `cur8::SetWide(page,false)`. Sicurezza aggiuntiva in KartEnterHook.
- **Invariante +0x8c0 ≤ w20**:
  - a turno unico: +0x8c0 = 8 e w20 = 8 (pagina nomi, letta tramite +0xce8);
  - altrimenti 4 e 4;
  - con il passo 4 attivo `CharaPartsLayoutNameHook` non costruisce più la 0x16 come 8pl: oggi, con `MK8DX_LYT8=0`, dopo una gara a 8 il
    menu verrebbe ricostruito con N = 8.

  0x4be3b0 non è null-safe, ma riceve sempre un L_Name esistente.
- **Pagina kart al passo 4**: invariata. Usa la 0x16 (4 pannelli, `+0x39c = 4`, due turni). I suoi L_KartSlot_0i sono figli di N_Player_0i,
  per questo vengono riaccesi all'uscita. I nomi dei pannelli 0..3 della 0x16 non vengono aggiornati dalla selezione a turno unico
  (vanno alla pagina nomi): è un problema estetico [IPOTESI], che si risolve al passo 5 con la kart sulla pagina nomi.

## 2. (b) +0x8c0
- **Dove**: CharselEnterHook (0x4b74a4) **prima di Orig**. La pagina si costruisce una volta sola (0x4bcc70 = 4) e N può cambiare tra un
  ingresso e l'altro. Nessun lettore viene eseguito prima dell'onEnter: le uniche letture esterne sono 0x4b1d0c (regali, via +0x1f8) e
  0x4b2cb8/0x4b2e68 (CharaList, via +0x1d8), tutte dopo l'onEnter. Un trampolino su 0x4bcc84 non serve.
- **Valore**: `one ? 8 : 4` solo per la pagina 0x2e. Con N = 5..7 gli slot N..7 diventano 1 ("vuoto", 0x4b81bc con clamp #8, bit tolto da
  +0x1a8): è lo stesso schema del vanilla con 3P su M4. L'avanzamento confronta `w28` (stati 7 con i < +0x8c0) con `w22 = N` (0x4ba938).
  Alternativa (+0x8c0 = N): obbliga a nascondere N_Player_N..7 nella pagina nomi. Non si sceglie.
- **I 34 lettori** [VERIFICATO, `p4_8c0_readers.txt`]:

| lettori | ruolo | con 8 |
|---|---|---|
| 0x4b1d0c | regali: ciclo slot → stato, cursore, icona → 0x4b43b4 | clamp 0x4b1d20 #8 + fix 0x4b1d40 |
| 0x4b2cb8, 0x4b2e68 | CharaList → **0x4694fc(layout,&anim,count,slot)**: id = tabella[count−1], −1 se count > 4 → `ldr x8,[NULL,#0x1d0]` (0x4697dc) | **hook 0x469760**: count > 4 → id 0x12 (N_Player_0i/N_Window_0i 0..7 esistono nella 8pl) |
| 0x4b7568 | onEnter: ciclo di convalida cfg+0x1fb4[i] (0x4b75ec) | cfg8 |
| 0x4b7ab4, 0x4b7c6c | onEnter: cursore i sull'icona di cfg+0x1fb4[i] (`0x8af3f4(cur,i,…)`) | cfg8 (0x4b7b44) + V(cur,i) (passo 3) |
| 0x4b7cc0 | onEnter: L_Name 0x4be3b0, i < +0x8c0 | +0xce8, conteggio 8 |
| 0x4b7d40 | onEnter: `0x8bb3b8(CharaList 0x28+i)` | LR 0x4b7d7c rimappato (passo 2) |
| 0x4b81a8 | onEnter: stato 1 per i in [N,+0x8c0) | clamp 0x4b81bc #8 |
| 0x4b8308 | onEnter: slot in stato 3 → entry (0x4203e0/0x420948), parti | clamp 0x4b8330 #8 + cfg8 (4 add) |
| 0x4b8630, 0x4b8688, 0x4b8ed8, 0x4bb358, 0x4bb380 | rami `==1` / `>1` (1P contro multi) | come 4 |
| 0x4b8658 | i in [+0x8c0,4) → 0x14a7ac(i,0) | nessuna iterazione |
| 0x4b8748, 0x4b8bd0 | link offline (≤1 → 0x2f, poi 0x30/0x31) e link indietro (≥2 → 0x24) | come 4 |
| 0x4b79b8 | link online (==2) | non usato offline |
| 0x4b87b0 | onEnter: L_Name / cursore per slot | clamp 0x4b87d4 #8 + fix 0x4b885c + +0xce8 |
| 0x4b8934, 0x4b8960 | ciclo `0x8af3f4(cur,i,…)` | V(cur,i) |
| 0x4b9008 | ciclo → 0x4203e0(i) (0x4b90d4) | clamp 0x4b901c #8 + cfg8 (0x4203e0) |
| 0x4b91b4, 0x4b941c | consumo code 0x4b9190 → 0x420948, 0x13d424 | clamp 0x4b91e4 (p2) + cfg8 + guardia 0x13d424 |
| 0x4b9af8 | vt93: Mii/amiibo, cfg+0x1fb4/+0x1fc4, code | clamp 0x4b9b78 + cfg8 (4 add) |
| 0x4ba3c0 | vt93: macchina a stati per slot | clamp 0x4ba404/0x4ba5a4 + fix 0x4ba460/0x4ba600 |
| 0x4ba81c | vt86: conteggio stati 7, CharaList+0x120 | clamp 0x4ba860 + remap LR 0x4ba88c |
| 0x4bab78, 0x4bade4, 0x4bb1bc, 0x4bb278 | vt81/vt87: chiusura e attesa delle CharaList, code | remap (p2) + clamp 0x4bb28c (p2) |
| 0x4bb0f8 | vt87 stadio 1: tipo 0 per gli slot in stato 7 | clamp 0x4bb108 #8; i pad 4..7 danno i locali 4..7 |
| 0x4bb460 | vt87: 0x4104a8 (lista di precaricamento, con limite, 0x410510) | clamp 0x4bb470 #8 [IPOTESI: capacità sufficiente per 8] |

- **Maschera +0x1a8** [VERIFICATO]: valore di partenza 0x80000001 (0x8ba42c/0x8ba444); l'onCreate imposta il bit 4 (0x4b57b4 `orr #0x10`).
  L'onEnter accende i bit 0..N−1 con `lsl` non limitato (0x4b80e4..). Lo stato 5 toglie il bit (0x4bc074, non limitato), 7→3 lo rimette
  (0x4ba734..0x4ba750), lo stato 1 lo toglie (0x4b81bc). Con il tocco spostato a idx 8 (C2), il bit 4 appartiene a P5. Non serve nessuna
  patch.

## 3. (c) Clamp

### 3.1 I 39 `kClamp904`
- **16 a #8** (selezione e regali): 0x4b1d20, 0x4b563c, 0x4b80e4, 0x4b80f4 (forma `csinc`), 0x4b81bc, 0x4b8330, 0x4b87d4, 0x4b901c, 0x4b94dc,
  0x4b9b78, 0x4ba404, 0x4ba5a4, 0x4ba860, 0x4bb108, 0x4bb470, 0x4bc018. Parole in `p4_table.inc` [VERIFICATO].
- **23 della kart restano a 4** (0x4c04cc..0x4c451c). I loro cicli vanno fino a kart+0x39c = 4 e leggono `[+0x3b8]+0xa28[i]` per i < 4:
  servono gli stati 7 degli slot 0..3, che a turno unico ci sono. Passo 5.

I 12 condivisi:

| clamp | oltre +0xa28 indicizza | dopo i passi 1-3 | passo 4 |
|---|---|---|---|
| 0x4b1d20 (regali) | cursore `[sel+0x1b0]+i*0x20+0x30` (0x4b1d3c) | cursore a slot virtuali, ma questo accesso **non passa per V** | #8 + **kPageSlotFix 0x4b1d40** (`x9 += 0x80` se x9−x8 ≥ 0x80 e Wide) |
| 0x4b8330 | cfg+0x1fb4/+0x1fd4/+0x1fe4/+0x1ff4[x28] (0x4b83c4, 0x4b8470, 0x4b84d8, 0x4b853c) | no (cfg a 4) | #8 + 4 hook cfg8 |
| 0x4b9b78 | cfg+0x1fb4/+0x1fc4[x21] (0x4b9bf8, 0x4b9ca0, 0x4b9db4 scrittura, 0x4b9eb0 scrittura), code `madd x21,#0x28` 0x4ba1b8/0x4ba1d8/0x4ba1f8 | code: sì (+0xba8 [8], passo 2); cfg: no | #8 + 4 hook cfg8 |
| 0x4ba404, 0x4ba5a4 | cursore `x0+x23*0x20+0x30` (0x4ba45c/0x4ba5fc → 0x8af3f4) | come 0x4b1d20 | #8 + **fix 0x4ba460, 0x4ba600** |
| 0x4c154c, 0x4c18d0, 0x4c2a10, 0x4c2a9c, 0x4c2ff8, 0x4c3d74, 0x4c3ed0 | kart +0x460[4] e cursore kart | no | **a 4** (passo 5) |

Anche **0x4b87d4** (non condiviso secondo `clamp_reuse.txt`) indicizza il cursore dopo il salto a 0x4b8854: `add x8,x9,x8,lsl #5` con x8 =
indice limitato. Serve quindi **fix 0x4b885c**. I 4 kPageSlotFix sono hook inline dello schema di p3 §6, con in più il controllo
`cur8::Wide(cur)` [VERIFICATO: registri a 0x4b1d3c, 0x4b8858, 0x4ba45c, 0x4ba5fc].

### 3.2 I 7 clamp del passo 2
0x4b4f0c, 0x4b53c8, 0x4b91e4, 0x4bb28c, 0x4bc4f0, 0x4bc630 a #8. 0x4b9b78 è già nel §3.1. Le code (+0xba8) e i link (+0xa68) hanno 8 voci
[VERIFICATO passo 2].

### 3.3 Config: ombra minima `cfg8` già nel passo 4
Le 13 tabelle int[4] cfg+0x1fb4..+0x2083 non si possono allargare sul posto (A2 §4.3). Tutti gli accessi della zona hanno la forma
`ldr x8,[x8,#0x30]` (cfg) → `add xB,xB,xI,lsl #2` → `ldr/str [xB,#imm]` (imm 0x1fb4..0x2074) [VERIFICATO, `p4_cfg_uses.txt`].

- **Schema**: il `cmp #4` del clamp diventa `#8`. Un **hook INLINE PRE sull'`add`** pone `X[B] = sh − 0x1fb4 − 16` quando 4 ≤ X[I] < 8:
  l'`add` spostato calcola `sh − 0x1fb4 + (I−4)·4` e l'accesso `+imm` arriva su `sh[k][I−4]`, con k = (imm−0x1fb4)/0x10. La base è
  sempre ricaricata da [x8,#0x30] subito prima e la `add` ha destinazione = base: modificarla non tocca altro [VERIFICATO,
  `p4_verify.py`].
- **Clamp dell'ombra (28)**:
  - SetChara 0x4203e0/0x420948, varianti: 0x4204c4, 0x420564, 0x420610, 0x4206c0, 0x42077c, 0x420820, 0x4208c4, 0x4209f0;
  - CharaList, scrittura delle varianti: 0x4b394c, 0x4b3a30, 0x4b3b14, 0x4b3bf8, 0x4b3cdc, 0x4b3dc0, 0x4b3ea4, 0x4b3ffc;
  - 0x4b43b4, lettura delle varianti: 0x4b4480, 0x4b45ac, 0x4b46d4, 0x4b47fc, 0x4b4c60, 0x4b4d90, 0x4b4e24;
  - vt92, +0x1fb4/+0x1fc4[slot]: 0x4bbf18, 0x4bbfac, 0x4bc228;
  - onEnter: 0x4b75ec (x27, 3 add), 0x4b7b44.

  Ogni `csel` ha un solo uso, salvo x27 (3 add) [VERIFICATO `p4_uses.py`]. I chiamanti di 0x4203e0/0x420948 sono solo nella selezione
  (11 + 3), con slot < +0x8c0 [VERIFICATO]: in 1-4P l'indice resta < 4.
- **38 add agganciate**: tabella `kCfgAdd`; 5 combinazioni (base, indice): (8,9)×27, (8,27)×3, (8,28)×4, (8,21)×3, (9,21)×1. Un tipo di hook
  inline si può installare in più punti, perché `InlineHook::InstallAtOffset` è senza stato [VERIFICATO inline.hpp].
- **Non toccati**, perché a 4 si ottiene il comportamento vanilla, cioè l'indice 0:
  - 0x4b6558 (0x4b64b4 convalida parti; l'indice x23 serve 3 basi diverse e un indirizzo conservato in x27);
  - 0x4b7888 e 0x4bba4c (i ≤ 1, online);
  - kart 0x4c09f0/0x4c1dd8/0x4c1e8c/0x4c1f40/0x4c37e0.. (passo 5);
  - 0x3fd448.., 0x476634.., 0x486870.., 0x4b6ad4.., 0x4d6d44.. (altri oggetti o indici fissi).
- **Ombra**: `alignas(16) int32_t sh[13*4]` nel modulo, a zero (personaggio 0, parti 0, variante 0, tutti validi; la convalida 0x4b75ec
  riscrive 0 se non valido). Persiste finché il gioco è aperto, come `aid8`.

## 4. (d) vt92 0x4bbd18 e anteprima 3D per gli slot 4..7
- **Tipo 0 (spostamento)**: `cfg+0x1fb4[slot]`/`+0x1fc4` (0x4bbf2c/0x4bbfc0) → ombra. L_Name dello slot tramite 0x4b43b4 → +0xce8. Modello
  3D tramite **0x13d424 (0x4bc6c0)**, che non è sicuro, e 0x13d4ac (0x4bc6e0), che è limitato.
- **Tipo 2 (decide)**: stato 5 (0x4bc028, clamp 0x4bc018 #8), bit tolto (0x4bc074), +0x1fc4 → ombra (0x4bc238), `0x4203e0(slot,&pers)`
  (0x4bc290) → entry del locale slot, finestra varianti `CharaList(slot)` (LR 0x4bc5ac) e link +0xa68+slot·0x28 (clamp 0x4bc630 #8),
  code +0xba8 (0x4bc4f0 #8) [VERIFICATO].
- **3D**: modelli del menu = `0x7e7434()+0x190[4]+0x238`, con +0xc0 conteggio e +0xc8 ptr[]. **0x13d424(mgr, slot, flag)** legge
  `+0xc8[slot]` e ci scrive (0x13d424..0x13d444) **prima** del controllo `slot < +0xc0` (0x13d44c). Chiamanti con lo slot: 0x4b3f28
  (CharaList +0x1e8), 0x4b85a8 (onEnter), 0x4b93ec (code), 0x4ba23c (vt93), 0x4bc6c0 (vt92), 0x4c1fc8 (kart); 0x4b9140 usa 0.
  → **trampolino 0x13d424** (`f9406409`): se slot ≥ 4 e slot ≥ +0xc0 si chiama solo `0x148108(nullptr)`, come il ramo fuori limite
  0x13d46c, e si ritorna. Tutti i chiamanti ignorano x0 [VERIFICATO].
- **Già sicuri** [VERIFICATO]: 0x13d47c, 0x13d4ac, 0x13d540 (controllo prima dell'accesso). Inoltre 0x14a7ac, 0x14a858, 0x14a8f4, 0x14b46c,
  0x14b508, 0x14a990 e 0x14b824 hanno un `cbz` dopo 0x13d47c; 0x14aa2c, 0x14ab30, 0x14ac5c e 0x14b5a4 sono cicli limitati da +0xc0.
- **0x49ae1c** (pagina 0xe = PartsCursor, `cmp w1,#4`): chiamata solo dalla kart (0x4c187c..0x4c18ac, 0x4c27f4, 0x4c3c4c..0x4c3c7c), quindi
  non interessa la selezione.
- Risultato atteso: P5-P8 **senza anteprima 3D**, senza crash. I modelli 0..3 restano attivi nelle posizioni della Multi [IPOTESI: possono
  sovrapporsi ai pannelli 8pl → prova visiva; se serve, `0x14a7ac(i,false)` per i = 0..3 all'apertura della pagina nomi, opzione
  `MK8DX_P4_HIDE3D`].

## 5. (e) Scrittura delle entry e logica a due turni
- **Catena** [VERIFICATO]: slot i → `0x8bf158(i)` (GetPadHook: i < 4 → `c[i]` se `!round2`; 4..7 → `padui::extra[i−4]` con `active8`;
  `pad+8 = 4+k`, PadSetHook) → `0x8be03c` (C1 0x8be040 `cmp #7`) → `0x86ebf8(idx)` (B1i 0x86ec08 `cmp #7`) → `raceMgr+0x17c[idx]`.
  Scrittori: vt92 0x4bc290, CharaList 0x4b3970.., onEnter 0x4b840c/0x4b83f0 e parti 0x4b8478.., vt87 stadio 1 `0x42022c(e,&0)` per gli
  stati 7 con i < +0x8c0 (0x4bb108..), che diventano 8 senza CharselStageHook.
- **Avanzamento** 0x4ba938: `w28 == w22` con N stati 7 → `0x8ba5fc(this,+0x1d0)` → 0x31. Non serve nessun hook; il vecchio trucco
  `cfg+0x1fa4 = 4` di CharselFrameHook non si usa.
- **Modifiche**:
  - `#define MK8DX_SEL1ROUND 1` (0 = S5.4 com'è oggi) e controllo a runtime `p4::one`, calcolato a ogni onEnter;
  - **CharselEnterHook**: calcola `one`, imposta +0x8c0 e +0xce8, `round = one ? 0 : (TwoRounds ? 1 : 0)`, `round2 = false`. Con `one`
    chiama **`cur8::SetWide(page,true,N)` prima di Orig**: l'onEnter resetta il cursore (0x4b79a4 → 0x4b5910 → 0x8af308, Cur8ResetHook
    rimette gli slot 4..7 a 0) e poi posiziona i cursori i < 8 (0x4b7af0..0x4b7c80, `0x8af3f4(cur,i,…)`). Con un cursore stretto gli
    indici 4..7 finirebbero sullo slot 0. Dopo Orig si apre o si chiude la pagina nomi;
  - **CharselFrameHook**: nessuna modifica di logica (con `round == 0` chiama Orig), solo il log;
  - **CharselStageHook**: invariato; con un turno solo non trova entry di tipo 4 e non fa nulla;
  - **KartEnterHook / KartFrameHook / PageLinkHook**: invariati, la kart resta a due turni (`TwoRounds()` = active8 && N > 4). Le parti del
    turno 2 vanno ai locali 4..7 via round2. In più, KartEnterHook chiude per sicurezza la pagina nomi.

  CharselProfileHook e ProfileToCharaWinHook restano limitati a 4: niente profilo nella finestra varianti di P5-P8, con il clamp interno
  0x3fd694 [IPOTESI]. ProfileToEntriesHook/aid8 restano invariati.
- **Annulla (B) per P5-P8**: stato 7 → `0x8bf158(i)+0x10 bit2` (0x4ba504..0x4ba514, i non limitato) → 0x4ba71c: stato 3, bit,
  `0x8ba814(page,i)` (G2) e `0x14b508(i)` (null-safe). Funziona per i = 4..7 [VERIFICATO].

## 6. (f) Controlli
- **Item della griglia**: maschera item+0x58 (= controllo+0x88, ctor 0x8ad888 su obj+0x30) di default 0x8000000f. `SetWide` aggiunge i
  bit 4..N−1 (passo 3) [VERIFICATO main.cpp].
- **CharaList 4..7**:
  - +0x1a8: l'onEnter 0x4b2ab8..0x4b2b6c accende solo i bit 0..3, ma **vt93 0x4b3160** (per frame: 0x8bd83c → 0x8bc378 → 0x8bc46c
    `vt+0x2d8`) fa `setFocus(mgr,slot,page)` e `+0x1a8 |= 1<<slot` senza limite (0x4b3174..0x4b3194) [VERIFICATO]. Non serve un hook
    sull'onEnter;
  - il setFocus con slot ≥ 4 passa da Cur8SetFocusHook verso l'ombra; per questo serve `wideCur` attivo, cosa garantita a turno unico;
  - la CharaList non ha un cursore (+0x1b0 nullo; nessun thunk di 0x8bafb8 in 0x4b27c4..0x4b5600) e usa lo scanner +0x2e0, che
    BtnScanHook estende ai pad 5..7 [IPOTESI sul percorso dei tasti].
  - **L_ChangeBtn_00**: `0x43ce4c(btn,slot)` (0x4b29a4) con slot ≥ 4 azzera i bit 0..3 di +0x88 senza accendere bit 4..7
    (0x43ce94..0x43cf18) e mette il frame slot+1 → **trampolino 0x43ce4c**: dopo Orig `+0x88 = (+0x88 & ~0xff) | 1<<slot` per slot 4..7.
    Il frame resta slot+1 = 5..8 [IPOTESI estetica; i colori P5..P8 del cursore sono 7..10].
- **L_Name (SwitchBtn +0xe8[3], +0x118[3])**: le funzioni 0x4be0d0..0x4be2e0 cambiano solo il bit 31 di +0x88; i bit giocatore restano
  0x0000000f. Sono pulsanti della pagina CharaParts, che non ha focus (+0x190 = 0) [IPOTESI: comandi a tocco/visivi]. Se in Eden
  servono a P5-P8, `CharaPartsCreateHook` (pagina nomi) può aggiungere `1<<i` ai 6 pulsanti dei L_Name_04..07, senza un hook nuovo.
- **+/− e B da stato 3**: passano per lo scanner della selezione +0x8d0 (BtnScanHook) e per vt92 con `ev+0x8 == 0x64` (0x4bbd54..), dove
  alcuni rami sono solo per il pad 0 (0x4bbd7c `cbnz ev+0x2c`). Il comportamento resta quello del vanilla per P2-P4 [IPOTESI].

## 7. Codice (da integrare in main.cpp; tabelle in `step5b_data/p4_table.inc`)
```cpp
/* ---- step 5b passo 4 (step5b_plan_p4.md): selezione Multi a turno unico, kart ancora a due turni ---- */
#ifndef MK8DX_SEL1ROUND
#define MK8DX_SEL1ROUND 1        /* 0 = selezione a due turni S5.4 (comportamento precedente) */
#endif
namespace cfg8 {                 /* ombra dei 13 int[4] cfg+0x1fb4..+0x2083 per gli indici 4..7 (solo RAM) */
    alignas(16) static int32_t sh[13 * 4];
    static uint32_t hits;
    inline uintptr_t Base() { return (uintptr_t)sh - 0x1fb4 - 16; }   /* add xB,xB,xI,lsl #2 con xI = 4..7 -> &sh[k*4 + xI-4] + imm */
}
namespace p4 {
    /* #include "p4_table.inc": kClamp[50], kCfgAdd[38], kNames[19], kFactory[3], kHookWords[14] */
    constexpr int32_t kNamesId = 0x12;             /* DebugLocalRoom: mai registrato dal retail; in gara lo riusa hud8 */
    static bool on, reg, one, namesOpen;
    static uint8_t* Names() {                      /* pagina CharaParts 8pl valida, altrimenti nullptr */
        if (!reg) return nullptr;
        int32_t id = kNamesId; uint8_t* p = mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&id);
        uintptr_t vp = *(uintptr_t*)(mk8dx::Base() + 0x13093a0) + 0x10;            /* vptr CharaParts (ctor 0x4be8a8..0x4be8bc) */
        if (!p || *(int32_t*)(p + 0xe8) != kNamesId || *(uintptr_t*)p != vp || *(int32_t*)(p + 0x1e8) != 8 || !*(uint8_t**)(p + 0x1d0)) return nullptr;
        return p;
    }
    static void SetPane(uint8_t* page, const char* name, bool v) {                  /* come 0x4697dc..0x46980c: root = parte+0x30 */
        uint8_t* part = page ? *(uint8_t**)(page + 0x1d0) : nullptr; if (!part) return;
        hud8::SafeString ss = hud8::Str(name);
        uint8_t* pane = mk8dx::Fn<uint8_t* (*)(uint8_t*, hud8::SafeString*)>(0x8ad8e8)(part + 0x30, &ss);
        if (pane) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8ade74)(pane, v);
    }
    static void MultiNames(uint8_t* sel, bool v) {                                   /* N_Player_00..03 della 0x16 (+0x728) */
        uint8_t* m = sel ? *(uint8_t**)(sel + 0x728) : nullptr; char nm[] = "N_Player_00";
        for (int i = 0; i < 4; i++) { nm[10] = '0' + i; SetPane(m, nm, v); }
    }
    static void OpenNames(uint8_t* sel) {
        uint8_t* nmp = Names(); if (!nmp) return;
        int32_t id = kNamesId; mk8dx::Fn<void (*)(int32_t*, int)>(0x8b21dc)(&id, 0);  /* come 0x4b7d3c */
        SetPane(nmp, "N_Chara_00", false); SetPane(nmp, "N_ConSuppo_00", false);    /* griglia e B_MP restano quelli della 0x16 */
        MultiNames(sel, false); namesOpen = true;
        Logging.Log("[mk8dx-8p] p4: pagina nomi 8pl %p aperta, nomi Multi nascosti", nmp);
    }
    static void CloseNames(uint8_t* sel) {
        if (namesOpen) { int32_t id = kNamesId; mk8dx::Fn<void (*)(int32_t*)>(0x8b258c)(&id); namesOpen = false; }
        MultiNames(sel, true);
    }
}
#define CFG8_ADD(N, B, I) HOOK_DEFINE_INLINE(Cfg8Add##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    if (ctx->X[I] - 4 < 4) { ctx->X[B] = cfg8::Base(); cfg8::hits++; } } };
CFG8_ADD(B8I9, 8, 9) CFG8_ADD(B8I27, 8, 27) CFG8_ADD(B8I28, 8, 28) CFG8_ADD(B8I21, 8, 21) CFG8_ADD(B9I21, 9, 21)
#define P4_SLOTFIX(N, T, C) HOOK_DEFINE_INLINE(P4SlotFix##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    if (ctx->X[T] - ctx->X[C] >= 0x80 && cur8::Wide((const uint8_t*)ctx->X[C])) ctx->X[T] += 0x80; } };
P4_SLOTFIX(Gift, 9, 8)   /* 0x4b1d40 */   P4_SLOTFIX(Enter, 8, 9)   /* 0x4b885c */   P4_SLOTFIX(Vt93, 8, 0)   /* 0x4ba460, 0x4ba600 */
HOOK_DEFINE_INLINE(CharaWinPosHook) {              /* 0x469760 str w8,[sp,#0x10] in 0x4694fc: w8 = id CharaParts per +0x8c0 (w20) */
    static void Callback(exl::hook::InlineCtx* ctx) { uint32_t c = ctx->W[20]; if (c > 4 && c <= 8 && p4::Names()) ctx->W[8] = p4::kNamesId; }
};
HOOK_DEFINE_INLINE(NamesCreateIdHook) {            /* 0x4be8fc str w8,[sp,#0x4c] onCreate CharaParts: id 0x12 -> ramo Multi */
    static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->W[8] == (uint32_t)p4::kNamesId) ctx->W[8] = 0x16; }
};
HOOK_DEFINE_TRAMPOLINE(CharselExitHook) {          /* 0x4bb6a0 vt84 (0x8bb368 -> vt+0x290): uscita della selezione */
    static void Callback(uint8_t* page) {
        uint8_t* l = *(uint8_t**)(page + 0x168); int32_t dest = l ? *(int32_t*)(l + 0xc) : -1;   /* come 0x8ba5a0 */
        Orig(page);
        if (*(int32_t*)(page + 0xe8) != 0x2e || !p4::namesOpen) return;
        if ((dest >= 0x28 && dest <= 0x2b) || cw8::SlotOfId(dest) >= 0) return;            /* finestra varianti: non si esce */
        p4::CloseNames(page);
        cur8::SetWide(page, false, 0);
        Logging.Log("[mk8dx-8p] p4: uscita selezione verso 0x%x, pagina nomi chiusa, nomi Multi visibili", dest);
    }
};
HOOK_DEFINE_TRAMPOLINE(Model3DSlotHook) {          /* 0x13d424(mgr, slot, flag): +0xc8[slot] usato prima del controllo (0x13d428) */
    static void Callback(uint8_t* m, int32_t slot, bool f) {
        if (m && (uint32_t)slot >= 4 && slot >= *(int32_t*)(m + 0xc0)) {
            static int n; if (n++ < 8) Logging.Log("[mk8dx-8p] p4: 0x13d424 slot %d saltato (modelli %d)", slot, *(int32_t*)(m + 0xc0));
            mk8dx::Fn<void (*)(void*)>(0x148108)(nullptr);                                   /* come il ramo fuori limite 0x13d46c */
            return;
        }
        Orig(m, slot, f);
    }
};
HOOK_DEFINE_TRAMPOLINE(BtnPlayerHook) {            /* 0x43ce4c(btn, slot): frame slot+1 e +0x88 (item+0x58) solo bit 0..3 */
    static void Callback(uint8_t* b, int32_t s) {
        Orig(b, s);
        if (s >= 4 && s < 8) *(uint32_t*)(b + 0x88) = (*(uint32_t*)(b + 0x88) & ~0xffu) | (1u << s);
    }
};
```
Modifiche agli hook esistenti:
```cpp
// CharaSelBaseCtorHook (0x4bc844), dopo il codice del passo 2:
        if (p4::on) *(uint8_t**)(self + 0xce8) = nullptr;                       /* impostato a ogni onEnter */
// MenuPagesSetupHook (0x40476c), subito dopo cw8::ready = false (prima dei return di cw8):
        p4::reg = false; p4::namesOpen = false;
        if (p4::on) {
            uint8_t* tab = mk8dx::Fn<uint8_t* (*)()>(0x8bcc5c)();
            uintptr_t fcp = *(uintptr_t*)(mk8dx::Base() + 0x1307ad0);           /* -> 0x409070 (CharaParts) */
            uintptr_t frv = *(uintptr_t*)(mk8dx::Base() + 0x1307d68);           /* fabbrica RaceView di hud8 (voce dopo una gara a 5-8) */
            int32_t id = p4::kNamesId; uintptr_t f = *(uintptr_t*)(tab + 0x8 + id * 8);
            if (fcp != mk8dx::Base() + 0x409070 || *(uintptr_t*)(tab + 0x8 + 0x16 * 8) != fcp || *(uint8_t**)(tab + 0x448 + 0x16 * 8) != self)
                Logging.Log("[mk8dx-8p] p4: 0x40476c non registra CharaParts 0x16, niente pagina nomi");
            else if (f && f != fcp && f != frv) Logging.Log("[mk8dx-8p] p4: id 0x%x occupato (fabbrica %lx)", id, f - mk8dx::Base());
            else {
                *(uintptr_t*)(tab + 0x8 + id * 8) = fcp; *(uint8_t**)(tab + 0x448 + id * 8) = self;
                mk8dx::Fn<void (*)(uint8_t*, int32_t*, int)>(0x8b3cb0)(self, &id, 0xa);   /* layer 0xa come 0x4048a4 */
                p4::reg = true; Logging.Log("[mk8dx-8p] p4: pagina nomi CharaParts 8pl registrata (id 0x%x)", id);
            }
        }
// CharaPartsLayoutNameHook (0x4be854): con p4::on la 0x16 resta Multi, la 0x12 e' 8pl
        int32_t pid = page ? *(int32_t*)(page + 0xe8) : -1;
        if (p4::on && pid == p4::kNamesId) return sel8::kPageLyt8;
        if (p4::on && pid == 0x16) { sel8::built = 4; return r; }               /* r = Orig(page) = Multi */
        /* ... vecchio codice MK8DX_LYT8 invariato ... */
// CharaPartsSlotCountHook (0x4beb04): in testa
        if (p4::on && *(int32_t*)(page + 0xe8) == p4::kNamesId) { if (ctx->W[20] == 4) ctx->W[20] = 8; return; }
// CharaPartsCreateHook (0x4be8d0): condizione  (id == 0x16 && built > 4) || (p4::on && id == p4::kNamesId)
// CharselEnterHook (0x4b74a4):
    static void Callback(uint8_t* page) {
        int n = menu8::LocalCount(); bool multi = *(int32_t*)(page + 0xe8) == 0x2e;
        p4::one = MK8DX_SEL1ROUND && p4::on && multi && padui::active8 && cur8::on && cw8::ready && n > 4 && n <= 8 && p4::Names();
        if (p4::on) {
            if (multi) *(int32_t*)(page + 0x8c0) = p4::one ? 8 : 4;              /* 0x4bcc70: Multi = 4 */
            *(uint8_t**)(page + 0xce8) = p4::one ? p4::Names() : *(uint8_t**)(page + 0x728);
        }
        sel8::round = (!p4::one && sel8::TwoRounds()) ? 1 : 0; padui::round2 = false;
        if (p4::one) cur8::SetWide(page, true, n);                              /* prima di Orig: ciclo cursori 0x4b7af0 a 8 slot */
        Orig(page);
        if (!p4::one) { bool w = cur8::on && padui::active8 && multi && (MK8DX_P3TEST || *(int32_t*)(page + 0x8c0) > 4);
                        cur8::SetWide(page, w, n); }
        if (p4::on && multi) { if (p4::one) p4::OpenNames(page); else p4::CloseNames(page); }
        if (p4::one) Logging.Log("[mk8dx-8p] p4: selezione a turno unico, N=%d, +0x8c0=8, nomi %p", n, *(uint8_t**)(page + 0xce8));
        /* ... log P2 / A2 esistenti ... */
    }
// KartEnterHook (0x4c0898), prima di Orig:
        if (p4::namesOpen) p4::CloseNames(*(uint8_t**)(page + 0x3b8));          /* sicurezza: kart+0x3b8 = pagina selezione */
// CharselFrameHook: nel log ogni 600 frame aggiungere  p4::one, *(void**)(page+0xce8), cfg8::hits
```

## 8. Installazione (exl_main, dopo i blocchi passo 1, passo 2, A1-M1, S5.4 e passo 3; tutto o niente)
```cpp
    /* step5b passo 4: richiede passi 1-3, A1-M1 (hook 0x4be854/0x4beb04/0x4be8d0) e S5.4 (CharselEnterHook) installati */
    if (MK8DX_SEL1ROUND && !MK8DX_P3TEST && sel8::m2 && cw8::active && cur8::on && sel8::s54 && sel8::a1) {
        bool ok = true;
        for (const p4::C& c : p4::kClamp)    if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] p4: parola inattesa a %x", c.off); }
        for (const p4::C& c : p4::kNames)    if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] p4: parola inattesa a %x", c.off); }
        for (const p4::C& c : p4::kFactory)  if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] p4: fabbrica inattesa a %x", c.off); }
        for (const p4::A& a : p4::kCfgAdd)   if (*reinterpret_cast<uint32_t*>(base + a.off) != a.word) { ok = false; Logging.Log("[mk8dx-8p] p4: add inattesa a %x", a.off); }
        for (const p4::H& h : p4::kHookWords) {                       /* le prime 9 voci sono nuove; le altre sono gia' agganciate */
            if (&h - p4::kHookWords >= 9) break;
            if (*reinterpret_cast<uint32_t*>(base + h.off) != h.word) { ok = false; Logging.Log("[mk8dx-8p] p4: hook inatteso a %x", h.off); }
        }
        if (ok) {
            p4::on = true;                                            /* prima della costruzione del menu: ctor e registrazione */
            exl::patch::RandomAccessPatcher rp;                        /* 1) parole: 50 clamp, 19 +0x728 -> +0xce8, 3 fabbriche 0xcf0 */
            for (const p4::C& c : p4::kClamp)   rp.Write<uint32_t>(c.off, c.repl);
            for (const p4::C& c : p4::kNames)   rp.Write<uint32_t>(c.off, c.repl);
            for (const p4::C& c : p4::kFactory) rp.Write<uint32_t>(c.off, c.repl);
            rp.Flush();
            for (const p4::A& a : p4::kCfgAdd) {                       /* 2) 38 hook inline sulle add della config */
                if (a.base == 8 && a.idx == 9)       Cfg8AddB8I9::InstallAtOffset(a.off);
                else if (a.base == 8 && a.idx == 27) Cfg8AddB8I27::InstallAtOffset(a.off);
                else if (a.base == 8 && a.idx == 28) Cfg8AddB8I28::InstallAtOffset(a.off);
                else if (a.base == 8 && a.idx == 21) Cfg8AddB8I21::InstallAtOffset(a.off);
                else if (a.base == 9 && a.idx == 21) Cfg8AddB9I21::InstallAtOffset(a.off);
            }
            P4SlotFixGift::InstallAtOffset(0x4b1d40); P4SlotFixEnter::InstallAtOffset(0x4b885c);
            P4SlotFixVt93::InstallAtOffset(0x4ba460); P4SlotFixVt93::InstallAtOffset(0x4ba600);
            CharaWinPosHook::InstallAtOffset(0x469760); NamesCreateIdHook::InstallAtOffset(0x4be8fc);
            CharselExitHook::InstallAtOffset(0x4bb6a0); Model3DSlotHook::InstallAtOffset(0x13d424); BtnPlayerHook::InstallAtOffset(0x43ce4c);
            Logging.Log("[mk8dx-8p] p4: selezione a turno unico pronta (50 clamp, 19 nomi, 38 cfg8, oggetto 0xcf0, pagina nomi 0x%x)", p4::kNamesId);
        } else Logging.Log("[mk8dx-8p] p4: passo 4 NON attivato, selezione a due turni");
    }
```
- `sel8::s54` e `sel8::a1` sono due flag nuovi, da porre a true nei blocchi esistenti S5.4 e A1-M1. Le fabbriche hanno già la parola del
  passo 2 (0x52819d00), che il controllo verifica.
- Ordine: le parole e il Flush vengono prima degli hook inline. Nessun hook cade su una parola patchata: gli hook sono su `add`, `ldr`
  e `str`, le patch su `cmp` e `ldr #0x728`. Le collisioni con gli indirizzi di `main.cpp` sono 0 [VERIFICATO `p4_verify.py`]. Tutto
  in exl_main, prima che il menu costruisca le pagine.
- `MK8DX_P3TEST` deve restare 0: con 1 `Cur8TestEventHook` scarta gli eventi di P5-P8.

## 9. (g) Prova in Eden
**Log all'avvio**: `p4: selezione a turno unico pronta …`, nessuna riga `parola/add/hook inattes*`. All'ingresso nel menu:
`p4: pagina nomi CharaParts 8pl registrata (id 0x12)`, `A1: layout CharaParts = mn_Page_Lyt_CharaParts_8pl` (solo per 0x12) e le righe
`cw8: …` del passo 2. Dopo una gara a 8 e il ritorno al menu: di nuovo `registrata` (la voce RaceView di hud8 viene accettata), mai `occupato`.

**1P, 2P, 3P, 4P** (atteso identico al vanilla e all'ultima build):
- selezione, muovere, annullare, confermare, scegliere varianti (Yoshi/Inkling/Link), kart, regole, gara, ritorno;
- `+0x8c0 = 4` e `+0xce8 == +0x728` nel log periodico;
- nessuna riga `p4: pagina nomi … aperta`, nessuna `0x13d424 slot … saltato`, `cfg8::hits = 0`;
- layout Multi, pannelli grandi agli angoli, anteprime 3D.

**3P dopo una sessione a 8**: selezione Multi normale, nomi Multi visibili (CloseNames ha riacceso N_Player).

**8P** (8 controller), dopo il passo 3 già provato:
1. `p4: selezione a turno unico, N=8, +0x8c0=8, nomi 0x…`, `p4: pagina nomi 8pl … aperta`,
   `cur8: cursore … della pagina 0x2e a 8 slot (N=8)`.
2. A video [IPOTESI]: la griglia della Multi con **8 pannelli nome 8pl** in due colonne a lato (scala 0.67) al posto dei 4 grandi, e
   **8 cursori** colorati.
3. Ognuno muove il proprio cursore; il pannello del proprio slot aggiorna nome e colore. P5-P8 senza anteprima 3D, con la riga
   `0x13d424 slot 4..7 saltato`.
4. Conferma di P5 su un personaggio con varianti: finestra varianti `CharaList` id 0x01 (`cw8: 0x2c -> 0x1`) posizionata su N_Window_04;
   P5 sceglie il colore con il proprio controller.
5. Conferme indipendenti; B di P6 dopo la conferma → torna a "sceglie".
6. Quando tutti e 8 sono in stato 7 → pagina kart (`p4: uscita selezione verso 0x31…`), con i 4 pannelli Multi visibili.
7. Kart a due turni come oggi (`A2: pagina kart, turno 1`, `kart turno 2`). Poi `A2 kart Multi -> avanti`: **e0..e7 con `tipo=0`,
   personaggio/variante scelti da ciascuno (P1..P8 nell'ordine dei locali), parti ≠ -1, `sB+0x188[0] = 8`**. In gara, gli 8 personaggi
   giusti.
8. B dalla kart → di nuovo la selezione a turno unico: pagina nomi riaperta e cursori 5-8 sul personaggio salvato nell'ombra.

**5P/6P/7P**: +0x8c0 = 8 e gli slot N..7 "vuoti" (pannello come lo slot 3 della Multi in 3P). L'avanzamento avviene con N conferme.

Diagnostica:
- `cfg8::hits` deve crescere solo in 5-8P.
- Watchpoint su `cfg+0x1fc4..+0x1fd3` (la tabella dopo +0x1fb4) mentre si muove solo P5: non deve scattare.
- Breakpoint a 0x4697dc con x8 = 0: non deve mai essere raggiunto con count > 4.

## 10. Rischi
| rischio | prob. | mitigazione |
|---|---|---|
| un accesso alla config con un clamp portato a #8 non coperto da cfg8 scrive nella tabella successiva | bassa: gli usi di ogni `csel` sono verificati fino alla ridefinizione (`p4_cfg_uses.txt`) | watchpoint sulle tabelle vicine; clamp non portati a #8 = vanilla |
| la pagina nomi 0x12 disegnata sopra la griglia, oppure la griglia della 0x12 visibile | media | N_Chara_00 e N_ConSuppo_00 nascosti dopo l'apertura; l'ordine di 0x8bd18c inserisce la pagina prima delle pagine con la stessa priorità [IPOTESI] |
| memoria del menu: una CharaParts 8pl in più (layout e texture) in tutte le modalità | media | log del ctor 0x4be890 e della creazione; in alternativa registrare la 0x12 solo dopo la scelta di 5g-8g (richiede una creazione tardiva, non verificata) |
| pannelli Multi della kart con nomi vecchi (la selezione scrive nella pagina nomi) | alta, solo estetica | passo 5: kart sulla pagina nomi |
| modelli 3D 0..3 nelle posizioni Multi sovrapposti ai pannelli 8pl | media | opzione `MK8DX_P4_HIDE3D` (0x14a7ac(i,false)) |
| colori dei pannelli e dei pulsanti di P5-P8 (frame slot+1 = 5..8) | media, estetica | screenshot; correzione del frame in BtnPlayerHook e in 0x4be55c |
| P5-P8 non controllano la finestra varianti (scanner o maschere) | media | vt93 della CharaList imposta il bit e il focus per frame [VERIFICATO]; log del tasto in PadCalcAllHook; se serve, bit nei SwitchBtn (§6) |
| 0x4104a8 (precaricamento) con 8 voci oltre la capacità | bassa | ha già un limite (0x410510 `csel hi`); log della capacità +0x38 |
| profilo e Mii nella finestra varianti di P5-P8 mancanti (ProfileToCharaWinHook) | certo, estetica | passo 6 |
| +0xce8 nullo letto prima del primo onEnter | molto bassa (nessun percorso statico) | il ctor lo azzera: un eventuale accesso dà un crash a indirizzo basso, facile da riconoscere nel log |
| 1-4P diverso | bassa | tutti gli hook scattano solo con indice o conteggio > 4, oppure id 0x12; +0xce8 = +0x728 |

## 11. Capienza
Il passo 4 aggiunge **47 voci JIT**: 44 inline (38 cfg8 + 4 kPageSlotFix + 2) e 3 trampolini. Oggi le installazioni distinte sono 139
(`grep InstallAtOffset`, alcune alternative tra loro); dopo il passo 4 saranno circa 186, su un massimo di 0x10000 / 200 = **327**. Il
pool inline (0x8000 / 24 B ≈ 1365 voci) basta [VERIFICATO hook_impl.cpp:56-59, inline_impl.cpp:19-24]. Le parole patchate sono 72.
