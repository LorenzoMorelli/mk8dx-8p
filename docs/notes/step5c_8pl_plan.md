# Step 5c – selezione a 8 senza il "secondo a 4": layout 8pl dalla prima immagine (piano statico)

Data: 4 nov 2026. Solo analisi statica (`../img/main.bin`, `../img/main.ann.asm`, RomFS già estratta in `../romfs-tmp/menu4`) e log
esistenti. `src/` non modificato, Eden/GDB non avviati.
Verifica: `step5c_data (dati non pubblicati)/lyt8_verify.py` → `lyt8_verify.txt`: **183 OK, 0 FAIL** (parole, puntatori di vtable/GOT, stringhe, pane
e animazioni della RomFS, prime parole degli hook toccati).
Etichette: [V] = verificato (script o disassemblato, indirizzo citato); [I] = ipotesi da confermare con le misure del §6;
[D] = dato dall'utente.

## 0. Sintesi
- Problema [D]: entrando nella selezione personaggi a 8, nella selezione kart e tornando con B, per circa un secondo si vede la grafica
  a 4 (pagina Multi, 4 pannelli, anteprime 3D nelle posizioni 4P). Poi la schermata passa all'8pl.
- **Causa principale [V]**: il passaggio a 8 delle anteprime 3D (`m3d8::On8`) dipende da un timbro (`Stamp`). Il timbro lo mette
  `p5::Hide3D`, chiamata da `CharselFrameHook`/`KartFrameHook` (vt84 = `vt+0x2a0`). Il framework chiama vt84 **solo nello stato 4**
  della pagina (gestore 0x8bb7d8).
  - Lo stato 3 ("In") dura finché finiscono timer e animazioni di entrata e finché **tutte le unità 3D visibili hanno finito di
    caricare** (0x4b8e44 → 0x14ab30; kart 0x4c243c → 0x14b348). Con 8 KartUnit questo richiede più tempo.
  - Per tutto lo stato 3 il renderer resta nella configurazione Multi del gioco: 4 viste 640×320, modelli 4..7 spenti.
  - Nella kart il timbro di `KartEnterHook` vale 3 frame. Poi le anteprime tornano spente fino allo stato 4, quindi la sequenza è
    8 → 4 → 8.
  - Tornando con B ci sono 10/15 frame di attesa (0x4b8ebc) in più.
- **2D [V/I]**: la pagina nomi 8pl viene aperta e le N_Player della Multi vengono nascoste nella stessa chiamata dell'onEnter, prima di
  qualsiasi frame [V]. Il "Multi con 4 pannelli" non dovrebbe quindi vedersi per un secondo intero. Più probabilmente si tratta delle
  4 celle 3D in posizione 4P. Lo stabilisce la misura M2 [I].
- **8pl dalla costruzione della 0x16: non fattibile [V]**:
  - le pagine si costruiscono all'ingresso nel menu (0x40476c), quando N non è ancora noto;
  - il framework non distrugge le pagine una per volta;
  - `MK8DX_LYT8=8` metterebbe l'8pl anche in 1-4P.

  L'equivalente nel flusso del gioco esiste già: la CharaParts 8pl costruita sull'id 0x12 insieme alle altre pagine.
- **8pl come pagina della selezione: fattibile [V]**. Il layout 8pl contiene tutti i pane, le parti e le animazioni che il codice della
  Multi cerca per nome (unica eccezione L_CameraPlay, ricerca null-safe). Basta legare la selezione 0x2e alla 0x12, come già fa la
  kart (set B del passo 5).
- **Opzione consigliata (B + 3D legato allo stato della pagina)**:
  - 1 trampolino nuovo, `PageEnterHook` 0x8bb3e8;
  - 1 funzione nuova, `sel8b::BuildB`, nell'hook esistente CharaPartsCreateHook;
  - 7 funzioni esistenti modificate;
  - 0 parole patchate in più.

  Con 5-8 giocatori il gioco apre direttamente la 0x12: nessuna pagina sovrapposta, nessun nascondimento della Multi, Change e
  chiusure fatte dal codice originale. Con 1-4 giocatori resta il set A, cioè il vanilla.

## 1. Sequenza di apertura (domanda 1)

### 1.1 Costruzione (ingresso nel menu) [V salvo nota]
| passo | dove | cosa |
|---|---|---|
| registrazione | 0x40476c (`MenuPagesSetupHook`) | 0x16 CharaParts (fabbrica 0x409070), 0x2e selezione Multi (0x409210 → ctor 0x4bcc4c), 0x31 kart, 0x28..0x2b CharaList, 0x19 BG; dopo Orig la mod registra 0x12 (CharaParts 8pl) e le CharaList 4..7 (01/02/5e/64) |
| init pagina | 0x8b3cb0 → 0x726468 → 0x7240d8 (report step5b_plan_p2) | nome del layout da vt13: CharaParts 0x4be854 (`mn_Page_Lyt_CharaParts_Multi`, tabella 0x12797f0; per 0x12 la mod restituisce `_8pl`), selezione 0x8baa34 `cm_Page_Dummy` (nessun layout proprio) |
| cursore | vt72 0x4bcfe8 → `0x8bafb8(page, 50)` → +0x1b0 (kart: 0x4c4ed0, item = 4·+0x39c) | prima dell'onCreate |
| onCreate CharaParts | 0x4be8d0 | parte `mn_CharaKartSlct_Multi` (stesso nome di pane nell'8pl, file `mn_CharaKartSlct_8pl` [V RomFS]); L_Name ×w20 (0x12: 8 da 0x4beb04) |
| onCreate selezione | 0x4bcc84: +0x728 = 0x8bf0ec(0x16), +0x730 = 0x19, poi 0x4b5688 | 48 L_Chara (0x450 B) e B_MP legati a `[[+0x728]+0x1d0]+0x30` (0x4b56ac), +0x740[48], +0x738, ricevitore +0x8d0 (0x8bb2b8), gruppo animazioni `[+0x178][0]` = {parte 0x16, parte 0x19} (0x4b58c4/0x4b58d8), +0x9d8 |
| onCreate kart | 0x4c4c84: +0x3a8 = 0x16 (0x4c4c98) | la mod costruisce il set B sulla 0x12 (`p5::BuildB`, dentro l'onCreate della 0x12) |

### 1.2 Macchina a stati della pagina (+0x120; gestore per frame in +0x148, GOT 0x13109e0..0x1310a00) [V]
| stato | ingresso | per frame |
|---|---|---|
| 3 In | **0x8bb3e8**: azzera +0x124.., stato 3, `0x8afc54(cursore)` (0x8bb454), enter dei controlli (0x8bb470..), transizione +0x160, **vt75 onEnter** (0x8bb4d8), vt+0x2b8, `0x8afdd0` attiva gli slot del cursore, focus pad 0..3 (0x8bb51c..), animazioni In dei gruppi +0x178 (0x8bb59c) | 0x8bb5f4: aspetta timer +0x12c ≥ +0x180, fine delle animazioni In (+0x18 = 0), transizione finita, madre (+0xf8) in stato ≥ 4; poi `+0x128 = 1` e **vt83** ogni frame |
| 4 attiva | 0x8bb6d8 (chiamata da vt83): vt76 | 0x8bb7d8 → **vt84** (`CharselFrameHook` 0x4ba77c / `KartFrameHook` 0x4c2820) |
| 5 / 6 / 7 | 0x8bb7ec / 0x8bbb40 (vt79) / 0x8bbc64 (Out, vt80 0x4bb508) | 0x8bb814 / 0x8bbbb4 (vt85) / 0x8bbd5c |
| chiusura | 0x8bd26c: tolta da mgr+0xb50/+0xb58 (lista pagine aperte), poi 0x8bb368 → vt82 (`CharselExitHook` 0x4bb6a0) | – |

`0x8b21dc(&id, flag)` apre in modo sincrono (0x8bd18c, 0x8bbef4, 0x8bb3e0 → stato 3 e onEnter della pagina aperta) [V].

### 1.3 vt83 della selezione (0x4b8e44): perché lo stato 3 dura "un secondo" [V]
- Sottostadio +0x124 = 0, ingresso normale: per la Multi si passa avanti. Ritorno dalla kart (pagina precedente 0x2f..0x31): attesa di
  +0x130 ≥ 10 o 15 frame (0x4b8ebc/0x4b8ec0).
- Sottostadio 1: `0x14ab30()` (0x4b8ea8/0x4b8e88) deve restituire vero. Scorre tutte le `dir+0xc0` unità (8 con MENU3D8) e aspetta che
  ogni oggetto menu-kart visibile (+0xd4) abbia lo stato di caricamento +0x191 ∈ {3,5}. Poi 0x8bbec4.
- Sottostadio 10: `0x8bd408(mgr, 0x26)`, poi **0x8bb6d8 → stato 4** (0x4b9174).
- Kart 0x4c243c: sottostadi 1/2 con 0x14b258 / **0x14b348** (attesa dei modelli), poi 0x8bb6d8 (0x4c2478).

Quindi la durata dello stato 3 è: animazioni di entrata + caricamento dei modelli 3D visibili. Con le anteprime a 8 si caricano anche i
personaggi di P5-P8 [V codice, durata I].

### 1.4 Dove la mod passa a 8 oggi, e quando
| elemento | dove | quando rispetto al primo frame |
|---|---|---|
| +0x8c0 = 8, +0xce8 = pagina nomi, `cur8::SetWide` (cursore 8 slot, cornici 04..07) | `CharselEnterHook` prima di Orig | onEnter = stesso frame d'ingresso, prima del disegno [V] |
| apertura 0x12 (8pl) + N_Player_00..03 della 0x16 nascosti (`p4::OpenNames`) | `CharselEnterHook` dopo Orig | stesso frame [V]; Orig ha appena aperto 0x16 (0x4b7d3c) e 0x19 (0x4b7d24) |
| preset camera 8pl (`MenuPresetHook` 0x147738) | configurazione degli oggetti menu-kart | con N > 4 sempre; nello stato 3 i preset 8pl finiscono nelle viste Multi 640×320 [V codice; aspetto I] |
| **viste 3D a 8** (celle 640×160, layer extra, bit di vista, piedistalli 4..7) | `m3d8::MenuFrame` (RLM 0x3cc218) se `On8()` = `N8 && p4::one && frames - stamp ≤ 3` | **solo dopo il primo vt84**, cioè all'ingresso nello stato 4 (fine dello stato 3 del §1.3) [V] |
| `Hide3D` con MENU3D8=0 (nasconde i 4 modelli Multi) | vt84 | anche questo solo nello stato 4: prima i 4 modelli si vedono nelle posizioni 4P [V] |
| bit 1<<(4+j) negli item delle CharaList 4..7 (p5fix D) | vt84 | stato 4 (riguarda solo l'input) |
| kart: set B, SetWide, Change sulla 0x12, timbro | `KartEnterHook` | onEnter; il timbro scade dopo 3 frame → **8 → 4 → 8** fino al primo `KartFrameHook` (stato 4) [V] |
| ritorno con B | onEnter della selezione (la 0x12 resta aperta) | 3D come all'ingresso, con in più 10/15 frame (§1.3) [V] |

Il log `eden_log_flow8_parts.txt` (riga 6028-6034) conferma l'ordine [V]:
1. `p4: pagina nomi 8pl ... aperta`;
2. `SceneInit tipo=2`: scena anteprime, con la RLM in modo 3 e i rettangoli Multi 8..11;
3. primo log per frame della selezione (`M2: +0x8c0=8`);
4. solo dopo, `m3d8: anteprime 3D a 8 ACCESE`.

Le righe non hanno l'ora, quindi il numero di frame va misurato (M2).

**Causa del secondo a 4**:
- 3D [V]: le anteprime a 8 sono legate al per-frame dello stato 4, mentre il gioco tiene la pagina nello stato 3 finché i modelli sono
  caricati.
- 2D [I]: la pagina nomi 8pl e i cursori larghi sono a posto dal frame d'ingresso [V codice]. I "4 pannelli" visti dall'utente sono con
  ogni probabilità le 4 celle 3D Multi con i piedistalli. Resta però da escludere con M2 che la 0x16 mostri le N_Player.

## 2. 8pl come layout della pagina principale (domanda 2)

### 2.1 Cosa cerca per nome il codice della Multi e cosa c'è nell'8pl [V RomFS + main]
| chi | nome | Multi | 8pl |
|---|---|---|---|
| CharaParts onCreate 0x4be8d0 | parte `mn_CharaKartSlct_Multi` (0xeeb5b4) | → `mn_CharaKartSlct_Multi` | → `mn_CharaKartSlct_8pl` (stesso nome di pane) |
| " | L_Name_0i (0xeeb5d6), i < w20 | 0..3 → `mn_L_SlctName_01` | 0..7 → `mn_L_SlctName_02` |
| " | L_CameraPlay_0i (0xeeb5e0) | 0..3 | **assente** (0x8ad8e8 + 0x8ade74 `cbz`: nessun effetto) |
| " | L_Entry_0i (0xeeb5cb) | assente | assente (come nel vanilla) |
| CharaParts onEnter 0x4bf018 | N_ConSuppo_00 (0xeeb5f0) | sì | sì |
| bind L_Name 0x4bd9a4 | T_Name_00, P_MachineIcon_00, L_Parameter_00..04, L_SwitchBtns_00; gruppi G_Color/G_InOutWin/G_MachineIconChange/G_PlusBtn/G_Pos_PlusBtn/G_Win | SlctName_01 | SlctName_02 ⊇ SlctName_01, stessi gruppi |
| selezione 0x4b5688 | L_Chara_00..47 (0xeeb42d), B_MP_00 (0xeeb438) | sì | sì (griglia identica, step5b_plan_p4 §1.1) |
| 0x4694fc (finestra varianti) | N_Player_0i (0xee987e), N_Window_0i (0xee8fed) | 0..3 | 0..7 (la mod già sceglie 0x12 con +0x8c0 > 4, 0x469760) |
| kart 0x4bfdd0 | L_KartSlot_0i (0xeeb649), L_KartPartsOKWin_0i (0xeeb677), L_Body/L_Tire/L_Glider_00 | 0..3, KartPartsSlct_00 | 0..7, KartPartsSlctS_00 (stessi pane) |
| uscita 0x4bb538..0x4bb550 / kart 0x4c3360 | animazione G_Change_00 (gruppo 0 della parte) | L_KartSlot_00..03, N_Chara, N_ConSuppo | L_KartSlot_00..07, N_Chara, N_ConSuppo |

Ogni pane del Multi tranne L_CameraPlay esiste anche nell'8pl [V script]. Non c'è niente che possa dare il crash di ips_B. Quel crash
veniva dalla stringa 0xeeb5b4 rinominata: pane non trovato → T_Name_00 nullo → 0x4bdf30 (step5_plan_A1 §2). Qui la stringa non si
tocca: il nome del Page_Lyt lo dà lo slot 13.

### 2.2 Perché non "8pl già alla costruzione della 0x16" [V]
- **Momento**: 0x16 e 0x2e nascono in 0x40476c all'ingresso nel menu. Il numero di giocatori si sceglie dopo, nella stessa scena.
- **Ricostruzione**: il framework non distrugge le pagine una alla volta (step5b_plan_p4 §1.2: nascono in 0x8b3cb0 e muoiono con la
  scena). Inoltre +0x740/+0x738/+0x1b0 della selezione e +0x3a8 della kart puntano a controlli della parte costruita.
- **`MK8DX_LYT8=8`**: 0x16 è sempre 8pl. In 1-4P si avrebbero pannelli 0.67 a 4×2, 4 pannelli vuoti e niente L_CameraPlay, quindi
  1-4P non resta identico.

Il punto di partenza "LYT8=8" esiste già in forma compatibile. La 0x12 è una CharaParts costruita nella stessa fase con il layout 8pl e
w20 = 8 (`CharaPartsLayoutNameHook`, `NamesCreateIdHook`, `CharaPartsSlotCountHook`). Manca solo che la selezione la usi **come sua
CharaParts** invece che come sovrapposizione.

### 2.3 Collegare la selezione alla 0x12 (set B, come la kart) [V meccanismo, I dettagli marcati]
Oggi la griglia resta sulla 0x16 perché le 48 L_Chara, B_MP e il cursore sono legati alla parte 0x16 in onCreate. Per questo la 0x12
viene aperta sopra, con N_Chara e N_ConSuppo nascosti.

La soluzione è costruire un secondo legame:
1. alla creazione della 0x12 si richiama la stessa 0x4b5688 sulla selezione con +0x728 = 0x12 e un cursore nuovo;
2. si salvano i due set di campi;
3. all'ingresso con N > 4 si scambiano.

È lo schema di `p5::BuildB`/`p5::Use` della kart, già verificato in Eden (STATE passo 5, 4P/8P/4P/3P del 4 nov).

Campi da salvare e scambiare (set A = vanilla):

| campo | perché | prova |
|---|---|---|
| +0x728 | parte usata da onEnter (apertura 0x4b7d3c), uscita (chiusura 0x4bb718), Change (0x4bb538) e 4 accessi a +0x1d0 | [V] |
| +0x740[48] (0x180 B), +0x738 | L_Chara e B_MP del set | 0x4b577c, 0x4b5814 [V] |
| +0x1b0 | cursore con i 48+2 item legati alla parte giusta | 0x8bafe4 [V]; item registrati da 0x8bc0c4 → 0x8ae384 nel cursore corrente [I, come la kart] |
| `[+0x178][0]` {u32 n, ptr} (0x18 B) | gruppo animazioni con la parte 0x16 o 0x12 | 0x4b5860..0x4b58d8: **0x5fae48 libera l'array esistente** → azzerarlo prima della costruzione B [V] |
| +0x9d8 | 0x4b58ec lo sovrascrive con una nuova alloc 0x20 | [V]; tenere quello A |
| +0x198 (n ricevitori) | 0x4b57a0 riaggiunge +0x8d0 | [V]; ripristinare il conteggio (come BuildB) |
| +0xce8 | = +0x728 del set (le 19 letture L_Name restano valide) | [V p4] |

Le liste di controlli della pagina (+0x38, 0x8bc23c) ricevono anche i controlli B. Quindi enter/exit (0x8ae3fc/0x8ae3a8) girano anche
sulla parte della pagina chiusa. Con la kart succede lo stesso dal passo 5 senza effetti visibili [I].

Effetti sul flusso:
- con il set B la selezione apre da sola la 0x12 (0x4b7d3c legge `[+0x728].id`), la chiude all'uscita (0x4bb718) e la lascia aperta
  verso la kart (0x4bb6dc);
- il Change di uscita verso la kart (0x4bb550) agisce sulla 0x12;
- la kart B (+0x3a8 = 0x12) la ritrova aperta e la chiude andando avanti; il Change all'indietro (0x4c3360) agisce sulla 0x12;
- **la 0x16 non viene mai aperta con N > 4**.

Si possono quindi togliere `OpenNames`/`MultiNames`/`CloseNames`, il Change in più sulla 0x16 (`KartPreExitHook`), la chiusura
esplicita della 0x16 (`KartExitHook`) e il Change sulla 0x12 in `KartEnterHook`.

### 2.4 Pagina kart
La kart usa già l'8pl come layout (set B sulla 0x12, L_KartSlot 8pl, OKWin 0..7) [V passo 5]: il "secondo a 4" della kart è solo 3D
(timbro di 3 frame, §1.4). Con il set B della selezione, il Change all'ingresso arriva dal codice originale (0x4bb550 sulla 0x12)
invece che da `KartEnterHook`. `0x4c0490(page,1)` sui selettori B va mantenuta, perché il vanilla la chiama a 0x4bb650 prima dello
scambio kart [I]. In alternativa si sposta lo scambio kart in `PageEnterHook` (§3, H1b).

### 2.5 Opzioni
| opzione | cosa | pro | contro |
|---|---|---|---|
| **A** minima | solo 3D legato allo stato della pagina (H7) + Hide3D all'ingresso | 0 hook nuovi; corregge le anteprime in selezione, kart e ritorno con B | resta la sovrapposizione (0x16 aperta con N_Player nascosti, 0x12 sopra); se i "4 pannelli" sono la 0x16 non li toglie; Change 0x16/0x12 sfalsati |
| **B** consigliata | A + selezione legata alla 0x12 (set B) dall'ingresso nello stato 3 | flusso del gioco con N = 8: una sola CharaParts aperta, 8pl dalla prima immagine; meno codice speciale (via OpenNames/MultiNames/CloseNames e chiusure manuali) | costruzione di un secondo set all'ingresso nel menu (~54 KB: 48×0x450 + 0x118 + cursore 0x1a8 + 0x20); scambio di campi da tenere coerente |
| C | 8pl nella 0x16 alla costruzione | – | non fattibile (§2.2) |

## 3. Piano (opzione B) – domanda 3

Nuovo stato `namespace sel8b { uint8_t* built; bool isB; Set A, B; uint8_t* selPg; }`. Parole e prime istruzioni in `lyt8_verify.txt`.

**H1 (nuovo) TRAMPOLINE `PageEnterHook` @0x8bb3e8**:
- prima parola `f81c0ff7 str x23,[sp,#-0x40]!` [V]; nessun hook esistente sullo stesso indirizzo [V].
- Firma: `void (uint8_t* page)`. x1 è già stato salvato in +0x160 da 0x8bb3e0, oppure non è usato dai salti diretti
  0x49bf18/0x4b5390.
- È il punto del gioco **prima** del ripristino del cursore (0x8bb454), dell'enter dei controlli e dell'onEnter.
```
static void Callback(uint8_t* pg) {
    if (*(int32_t*)(pg + 0xe8) == 0x2e && *(uintptr_t*)pg == Base() + 0x1278940) {     /* solo Multi */
        p4::one = Sel1Round(pg);                       /* stesso test di oggi, spostato da CharselEnterHook */
        sel8b::Use(pg, p4::one && sel8b::built == pg);  /* set B o A */
        sel8b::selPg = pg; diag5c::Start(pg);           /* misura M2 */
    }
    /* H1b (facoltativo): if (id == 0x31 && p5::built == pg) p5::Use(pg, p4::one);  -> in KartEnterHook via la 2a 0x8afc54 */
    Orig(pg);
}
```

**H2 (nuova funzione, nell'hook esistente `CharaPartsCreateHook` 0x4be8d0, ramo id 0x12, dopo `p5::BuildB`)**:
```
sel = 0x8bf0ec(&0x2e); nm = p4::Names(); if (!sel || vptr(sel) != Base()+0x1278940 || !nm || sel8b::built == sel) return;
Save(sel, A);                                          /* +0x728 +0x738 +0x740[48] +0x1b0 +0x9d8 +0x198 [+0x178][0](0x18 B) */
g = **(sel+0x178); *(u64*)(g+8) = 0; *(u32*)g = 0;     /* 0x4b5870 non deve liberare il gruppo A */
n198 = *(s32*)(sel+0x198);
0x8bafb8(sel, 50);                                     /* cursore B, come vt72 0x4bcfe8 (con 'CUR8' da Cur8CtorHook) */
*(u8**)(sel+0x728) = nm; 0x4b5688(sel);                /* 48 L_Chara + B_MP sulla parte 8pl; gruppo = {0x12, 0x19} */
*(s32*)(sel+0x198) = n198; *(u8**)(sel+0x9d8) = A.p9d8; /* ricevitore +0x8d0 una volta sola; +0x9d8 di A */
Save(sel, B); Load(sel, A); sel8b::built = sel; sel8b::isB = false;
```

**H3 `CharselEnterHook` (0x4b74a4, esistente)**:
- tiene la diagnostica, il ripristino della voce 0x12 dopo una gara, +0x8c0 = one ? 8 : 4, `SetWide` prima di Orig;
- scrive +0xce8 = +0x728 sempre;
- **toglie** `OpenNames`/`CloseNames`/`MultiNames`. Orig apre `[+0x728]`, cioè 0x12 con il set B e 0x16 con il set A;
- con `MK8DX_P7_MENU3D8=0` chiama `p5::Hide3D()` dopo Orig (anteprime Multi nascoste dal primo frame).

**H4 `CharselExitHook` (0x4bb6a0)**: toglie `CloseNames`. La chiusura è quella di 0x4bb718 su `[+0x728]`. Restano `SetWide(false)` e
`p5::fromSel`.

**H5 `KartEnterHook` (0x4c0898)**: toglie `CloseNames` e `p5::Change(+0x3a8, 1)`, perché il Change l'ha già fatto 0x4bb550 sulla
0x12. Restano lo scambio B, la seconda `0x8afc54`, `SetWide`, `0x4c0490(page,1)` e `Hide3D`.

**H6 `KartPreExitHook` (0x4c3360) e `KartExitHook` (0x4c3564)**: tolgono il Change sulla 0x16 e la chiusura esplicita
`0x8b258c(0x16)` + `MultiNames`. Il vanilla chiude +0x3a8 (= 0x12) e +0x3b0.

**H7 `m3d8::On8` (usata da `MenuFrame` nel RLM 0x3cc218)**: legata alla pagina aperta invece che al per-frame dello stato 4.
```
static bool Open(uint8_t* pg) { /* come cur8::FocusExtra: pg nella lista mgr+0xb58[0..mgr+0xb50) */ }
On8() = N8() && p4::one && (selOpen || (p5::isB && kartOpen) || frames - stamp <= 3);
```
- `selOpen`/`kartOpen` si calcolano lato UI in `Cur8FocusFrameHook` (0x8bd6d8, per frame del gestore, già installato) con `Open(sel8b::selPg)`
  e `Open(p5::built)`.
- Così `MenuFrame` (render) legge solo due bool.
- La lista delle pagine aperte (0x8bd298/0x8bd2a4) è lo stesso criterio usato dal gioco per chiudere (0x8bd26c) [V].
- Lo stato 7 (Out) resta nella lista fino alla chiusura [I, M2].

**H8 `MenuPagesSetupHook` (0x40476c)**: `sel8b::built = nullptr; isB = false; selPg = nullptr`, come per `p5`.

**Conteggio**:
- 1 trampolino nuovo (H1);
- 1 funzione di costruzione in un hook esistente (H2);
- 6 hook esistenti modificati (H3-H8);
- 0 parole patchate in più.

I 19 `+0x728→+0xce8` restano, ora ridondanti (+0xce8 = +0x728). Restano anche `CharaWinPosHook`, che dipende da +0x8c0, e gli hook A1 della
0x12. Pool JIT: +1 voce.

Ordine di lavoro consigliato:
1. **Passo 0**: solo misura M2 sulla build attuale, per confermare la causa.
2. **Passo 1**: H7 + Hide3D in H3 (opzione A, piccolo e reversibile).
3. **Passo 2**: H1 + H2 + H3-H6 + H8 (set B), dietro un'opzione `MK8DX_SEL8B=1` (default 0 finché non è verificato).

## 4. Rischi per 1-4P
| rischio | misura di contenimento |
|---|---|
| H2 alloca il set B a ogni ingresso nel menu, anche in 1-4P (~54 KB; heap di 0x5902d0 [I]) | stessa scelta già in uso per la kart (BuildB sempre); log di `MaxAlloc` prima/dopo; in alternativa costruzione pigra al primo ingresso con N > 4 dentro H1, prima di Orig |
| scambio incompleto → set misto (cursore B con L_Chara A) | `Use` tutto o niente con verifica `*(sel+0x728) == (b ? 0x12 : 0x16)` e `Wide/CUR8` del cursore; log a ogni scambio |
| controlli B nelle liste +0x38 anche in 1-4P | già così per la kart; M3 in 1-4P: nessun pane della 0x12 visibile, 0x12 mai nella lista aperte |
| H1 gira a ogni apertura di pagina | solo confronto di id e vptr; nessuna azione per altri id |
| `p4::one` calcolato prima: `padui::active8`/`cw8::ready` non ancora pronti all'ingresso | stesso test di oggi, solo anticipato dentro la stessa transizione; se falso → set A (vanilla) |
| doppio Change (vanilla su 0x12 + mod) | H5/H6 tolgono quelli della mod |
| Single/WiFi (0x2c/0x2d) | esclusi dal controllo del vptr Multi |

## 5. Punti incerti
1. Natura dei "4 pannelli" [I]: il codice nasconde le N_Player della 0x16 nello stesso onEnter [V]. Con ogni probabilità sono le 4 celle
   3D/piedistalli Multi. M2 lo stabilisce leggendo il bit di visibilità (pane+0x58 bit0, 0x8ade78 [V]).
2. Ordine nello stesso frame fra aggiornamento UI e frame RLM [I]: può restare un frame di ritardo. Va misurato il frame di
   `shown = 1` rispetto all'ingresso.
3. Registrazione degli item del cursore in 0x8bc0c4/0x8ae384 [I]: si assume il cursore corrente +0x1b0, come la kart (verificato a
   runtime al passo 5).
4. Pagine in stato 7 ancora nella lista aperte [I]; durata reale dello stato 3 (animazioni + modelli) [I].
5. La pagina regali (0x4b09dc) e i flussi Mii/amiibo (+0x8c8 = 0x34/0x1d) leggono +0x740/+0x1b0. Con lo scambio vedono il set B,
   coerente con quello che è a video [I].
6. Come fa Switch 2 (pagine distinte per N o costruzione dopo la scelta del numero) è ignoto: nessun codice Switch 2 disponibile.

## 6. Misure (log)
- **M1** (H1): `[5c] enter id=0x%x set=%c +0x728->0x%x cur=%p wide=%d f=%lu`, una riga per ingresso; idem per ogni scambio di `Use`.
- **M2**: diagnostica per i primi 120 frame dopo l'ingresso della 0x2e o della 0x31, una riga ogni frame, da `Cur8FocusFrameHook`
  (lato UI). Campi:
  - `f-f0`;
  - +0x120/+0x124/+0x128 di 0x2e, 0x31, 0x16 e 0x12;
  - appartenenza di 0x16 e 0x12 alla lista mgr+0xb58;
  - bit0 di pane+0x58 di `N_Player_00` della 0x16 e `N_Player_04` della 0x12;
  - `m3d8::shown`;
  - rect di sub3d[0];
  - cursore +0xbc.

  Atteso sulla build attuale (conferma della causa): `shown = 0` finché 0x2e/0x31 ha +0x120 = 3, `shown = 1` dal primo frame con
  +0x120 = 4; nella kart `shown` 1 per ≤ 3 frame, poi 0, poi 1. Atteso dopo il passo 1: `shown = 1` entro f0+1. Atteso dopo il
  passo 2: con N > 4 la 0x16 non compare mai nella lista aperte e la 0x12 vi entra a f0.
- **M3** (regressione 1-4P): set A a ogni ingresso, 0x16 aperta, 0x12 mai aperta, `shown` sempre 0, `+0x8c0 = 4`, nessuna differenza
  nei log A2/M2 rispetto a oggi; 4P/3P fino ai kart e ritorno con B.

## 7. File
- `step5c_data (dati non pubblicati)/lyt8_verify.py` → `lyt8_verify.txt` (183 OK, 0 FAIL).
- Fonti: `../img/main.bin`, `../img/main.ann.asm`, `../romfs-tmp/menu4/*.szs` (Page_Lyt Multi/8pl, CharaKartSlct Multi/8pl, SlctName_01/02,
  KartPartsSlctS_00), `step7b_data (dati non pubblicati)/eden_log_flow8_parts.txt` (ordine dei log).

## 8. Implementazione (opzione B + A, 4 nov 2026)

Tutto dietro `MK8DX_SEL8B` (default 0, `scripts/build.sh` passa `-DMK8DX_SEL8B`). Con 0 il comportamento è quello del passo 4/5: ogni
ramo nuovo è sotto `MK8DX_SEL8B &&` oppure sotto `sel8b::isB`, che diventa vero solo se il set B è stato costruito (`sel8b::on`, cioè
PageEnterHook installato, possibile solo con l'opzione a 1). Verifica statica: `step5c_data (dati non pubblicati)/sel8b_verify.py` → `sel8b_verify.txt`,
**94 OK, 0 FAIL** (prime parole dei trampolini, istruzioni di 0x4b5688/0x8bafb8/0x8bb2b8/0x8af1c4, assenza di letture di x1 in
0x8bb3e8..0x8bb450, un solo `InstallAtOffset` per indirizzo). `lyt8_verify.txt` resta quello del piano: il suo controllo
"0x8bb3e8 non usato" ora fallisce di proposito (182/1).

Compilazione: `MK8DX_P7_MENU3D8=1 MK8DX_SEL8B=1` e `MK8DX_P7_MENU3D8=1 MK8DX_SEL8B=0`, entrambe senza errori né warning (-Wall -Werror).

### 8.1 Hook e parole
| id piano | dove | cosa fa (con SEL8B=1) |
|---|---|---|
| H1 | **nuovo** TRAMPOLINE `PageEnterHook` @0x8bb3e8 (`f81c0ff7 str x23,[sp,#-0x40]!`) | solo per la selezione Multi (id 0x2e e vptr 0x1278940): `p4::RestoreNamesPage()` (voce 0x12 dopo una gara), `p4::CalcOne()` (= test di turno unico di prima), `sel8b::Use(pg, one && built == pg && B.+0x728 == p4::Names())`, M1. Per la kart con set B (0x31 == `p5::built`) solo M1. Poi Orig: ripristino del cursore scelto (0x8bb454), enter dei controlli, onEnter. Installato solo se 0x8bb3e8/0x4b5688/0x8bafb8 hanno le parole attese |
| H2 | `sel8b::BuildB` in `CharaPartsCreateHook` (0x4be8d0, ramo 0x12, dopo `p5::BuildB`) | salva set A; azzera il gruppo `[+0x178][0]` (+0 capacità, +8 array, **+0x10 conteggio**); `0x8bafb8(sel, 50)` (cursore B, 'CUR8' da Cur8CtorHook); +0x728 = 0x12; `0x4b5688(sel)`; ripristina +0x198 (ricevitore +0x8d0 aggiunto due volte) e +0x9d8 (si tiene quello A); salva set B, ricarica A; `p4::namesPage = 0x12`. Log `set B selezione costruito` con cursori, L_Chara_00, B_MP, gruppo, heap prima/dopo |
| H3 | `CharselEnterHook` 0x4b74a4 | ripristino voce 0x12 e calcolo di `p4::one` spostati in `p4::RestoreNamesPage`/`p4::CalcOne` (codice identico): saltati se l'ingresso è già passato da H1 (`sel8b::entered`). Con il set B, dopo Orig niente `OpenNames`/`CloseNames` (Orig ha aperto `[+0x728]` = 0x12 a 0x4b7d3c) e `p5::Hide3D()` (MENU3D8=0: modelli Multi nascosti dal primo frame; MENU3D8=1: timbro). Con il set A (1-4P, o 5-8P senza set B) tutto come prima |
| H4 | `CharselExitHook` 0x4bb6a0 | con il set B: finestre varianti → nulla; verso 0x2f..0x31 `p5::fromSel`; `SetWide(false)`; nessuna chiusura (la fa 0x4bb718 su `[+0x728]`, o resta aperta verso la kart per 0x4bb6dc) |
| H5 | `KartEnterHook` 0x4c0898 | con il set B della selezione niente `p5::Change(+0x3a8, 1)` (l'ha fatto 0x4bb550 sulla 0x12); restano Use, 0x8afc54, SetWide, 0x4c0490, Hide3D |
| H6 | `KartPreExitHook` 0x4c3360 / `KartExitHook` 0x4c3564 | con il set B della selezione niente Change −1 sulla 0x16 e niente chiusura esplicita 0x16 + `MultiNames` andando avanti |
| H7 | `m3d8::On8` + `Cur8FocusFrameHook` 0x8bd6d8 | `On8 = N8 && p4::one && (timbro ≤ 3 frame || ((selOpen || kartOpen) && frames − openAt ≤ 3))`; `selOpen`/`kartOpen` calcolati lato UI in `sel8b::Frame` (selezione Multi / kart con `p5::isB` nella lista mgr+0xb58[0..mgr+0xb50), come 0x8bd298); `openAt` evita valori vecchi se il gestore non gira |
| H8 | `MenuPagesSetupHook` 0x40476c | azzera `built/isB/selPg/entered/selOpen/kartOpen` prima della registrazione della 0x12 |

Campi scambiati (`sel8b::Set`): +0x728, +0x738, +0x740[48], +0x1b0, `[+0x178][0]` (0x18 B). `Use` è tutto o niente; se il cursore che
si lascia è largo viene prima riportato a 4 (`SetWide(false)`, come all'uscita). Nessuna parola patchata in più; pool JIT +1 voce.

### 8.2 Differenze dal piano
1. **Conteggio del gruppo animazioni (correzione)**: il piano azzerava solo +0 e +8 di `[+0x178][0]`. 0x4b5688 non tocca +0x10 (conteggio,
   0x4b5860..0x4b58b8) e 0x8af1c4 scrive in `arr + n*0x20` solo se capacità > n (0x8af1d8..0x8af1ec, altrimenti sovrascrive [0]) e poi
   incrementa +0x10 (0x8af208). Con +0x10 = 2 rimasto dal set A, il gruppo B avrebbe avuto {0x19 in [0], niente 0x12} e n = 4. Si azzera
   anche +0x10, come già fa `p5::BuildB` [V sel8b_verify].
2. **Ripristino della voce 0x12 prima di `p4::Names()`**: il piano spostava solo il test di turno unico in H1; ma dopo una gara la voce
   0x12 della tabella e mgr+0x30[0x12] sono della gara finché `CharselEnterHook` non li ripristina, quindi in H1 `Names()` sarebbe nullo.
   Il ripristino è stato spostato in `p4::RestoreNamesPage()` e chiamato in H1 prima del test. `p4::namesPage` (usato dal ripristino) ora
   lo imposta anche `BuildB`, perché con il set B `OpenNames` non viene più chiamata.
3. **Condizione del set B**: oltre a `p4::one && built == pg` si richiede `B.+0x728 == p4::Names()` (stessa pagina 0x12 su cui è stato
   costruito il set). Se manca, set A con la 0x12 sovrapposta come al passo 4 (log `set B non disponibile`).
4. **Ripiego in `CharselEnterHook`**: se un onEnter della selezione Multi arrivasse senza passare da 0x8bb3e8 (non atteso: tutte le aperture
   passano da 0x8bb3e0/0x8bb3e8, 10 chiamanti + 2 salti verificati), il set viene scelto lì e il cursore ripristinato a mano con 0x8afc54
   (come `KartEnterHook`), con log `onEnter senza PageEnterHook`.
5. H1b (scambio kart in PageEnterHook) non fatto: lo scambio kart resta in `KartEnterHook`, come al passo 5.
6. `KartExitHook`/`KartPreExitHook`/`KartEnterHook` mantengono il codice del passo 5 quando la selezione è nel set A (ripiego); le parti
   tolte dal piano sono saltate solo con `sel8b::isB`.
7. M2 viene stampata anche all'ingresso nella kart B (per vedere 8 → 4 → 8).

### 8.3 Log (`[mk8dx-8p] sel8b:`)
- all'avvio: `opzione B attiva (PageEnterHook 0x8bb3e8, ...)`;
- all'ingresso nel menu: `set B selezione costruito su 0x12 ...` (anche in 1-4P; mai `set B NON costruito`);
- a ogni ingresso: `selezione set A|B: +0x728=... (id 0x16|0x12) ... CUR8=1` (solo quando cambia) e
  `M1 enter id=0x2e set=B +0x728->0x12 cur=... wide=0 one=1 N=8 f=...` (wide diventa 1 subito dopo, in onEnter);
- per 120 frame dall'ingresso: `M2 [0x2e] f=<frame> sel=<+120>/<+124>/<+128> kart=... 16=.../L<in lista> 12=.../L<in lista>
  NP00(16)=<vis> NP04(12)=<vis> shown=<0|1> open=<sel><kart> rect0=... cur+bc=<4|8> set=<A|B>` (−1 = pagina/pane assente);
- uscita: `uscita selezione (set B) verso 0x31, 0x12 lasciata aperta dal gioco` / `... verso 0x.., 0x12 chiusa dal gioco`.

### 8.4 Prove in Eden (build `MK8DX_P7_MENU3D8=1 MK8DX_SEL8B=1`)
| prova | cosa guardare |
|---|---|
| 1. 8P (e 5P/6P/7P): ingresso nella selezione | a video: subito pannelli 8pl e griglia, mai i 4 pannelli Multi; anteprime 3D a 8 dal primo frame. Log: `set B`, M2 con `16=` stato −1 o fuori lista (`L0`) per tutti i 120 frame, `12=3/...` `L1` da f=0, `shown=1` entro f≈1–2 mentre `sel=3/...` (stato In), `cur+bc=8` |
| 2. 8P: scelta personaggi P1-P8, varianti (finestra Y/L/R), Mii/regali se usati | input su tutti gli 8 slot come prima; la finestra varianti 5-8 si posiziona sui pannelli 8pl |
| 3. 8P: avanti alla kart | Change sulla 0x12 una volta sola (nessun doppio salto dei pannelli); M2 `[0x31]`: `shown=1` senza buchi (niente 1 → 0 → 1), `12=` sempre `L1`, `16=` mai `L1` |
| 4. kart → B → selezione → B → numero giocatori | ritorno: 8pl subito, 3D a 8 durante i 10/15 frame di attesa; uscita: `0x12 chiusa dal gioco`, nessun pannello rimasto a video nel menu precedente |
| 5. 8P fino alla gara, gara, ritorno al menu e di nuovo selezione | dopo la gara: `p4: voce 0x12 ... ripristinata` / `pagina nomi ... rimessa` prima di `M1`, poi `set B` e 8pl dal primo frame |
| 6. 1-4P (4P, 3P, 1P) dopo e prima di una sessione a 8 | identico al vanilla: `selezione set A`, `16=3/...` `L1`, `12=` `L0` per tutti i frame, `shown=0`, `cur+bc=4`, `NP00(16)=1`; fino ai kart e ritorno con B |
| 7. 8P → indietro → 4P → selezione → 8P | scambi A/B/A nei log, nessun cursore 5-8 rimasto nella selezione a 4, nessun crash |
| 8. regressione `MK8DX_SEL8B=0` | log senza righe `sel8b:`, comportamento del passo 4/5 |
