# Step 5 – selezione personaggi 6pl/8pl: piano di implementazione (sintesi, 2 nov 2026)

Fonde i quattro piani statici prodotti in parallelo (tutti [VERIFICATO] sul disassemblato, nessuna prova a runtime):
`step5_plan_A1.md` (classe selezione, layout 6pl/8pl, crash ips_B), `step5_plan_A2.md` (flusso slot → kart →
race-config), `step5_plan_B.md` (raceMgr a 8 locali, giocatori PIM 4..7), `step5_plan_C.md` (pad UI 4..7).
Dati grezzi in `step5_data/`. Offset = main 4.0.1 decompresso; IPS = +0x100.

## 0. Scoperte che cambiano il disegno rispetto a step4_menu_plan §5

| ipotesi di partenza | realtà [VERIFICATO] | fonte |
|---|---|---|
| ips_B crasha perché i layout 8pl non hanno L_CameraPlay | ips_B rinominava anche il **pane** `mn_CharaKartSlct_Multi` (0xeeb5b4): il pane non viene trovato, la parte resta vuota e il bind di L_Name dereferenzia `T_Name_00` NULL a 0x4bdf30. L_CameraPlay/L_Entry sono cercati in modo null-safe. Per caricare 6pl/8pl basta cambiare il nome del Page_Lyt (slot 13 = 0x4be854) | A1 §1-2 |
| il pad UI 4 è un "any-controller" | è il pad del **tocco** (pulsanti/tocco spenti alla creazione; i 4 siti `0x8bf158(4)` chiamano SetTouchOn 0x8be478). La fusione degli input (+0x2c5) la fa il **pad 0** in gara, leggendo i GamePlayer di ptr[0..3] | C §0-1 |
| raceMgr+0x178 resta 4 per un clamp nel riempimento | non c'è clamp: **0x86a594** cerca un GamePlayer libero solo fra gli indici 0..3 e per la 5ª entry restituisce NULL → 0x864fcc declassa l'entry a CPU (tipo 1). Il riempimento conta solo tipo 0 | B §1.1 |
| +0x17c va rilocato altrove | **in loco**: +0x17c diventa int[8] (+0x17c..+0x19b) e la mappa slot→entry passa da +0x18c a **+0x19c int[12]** (12 slot per costruzione); già coperto dalle 5 memset 0x50; 8 patch di immediato + REPLACE 0x86af68 + REPLACE 0x868f00 + 3 `cmp #3→#7` | B §1.4-1.5 |
| +0x904 è l'unico array per slot della selezione | +0x904 ha 72 accessi/39 clamp in **tre** classi; inoltre +0x928 (4 code), +0x660 (4 link), **pagine CharaWin 0x28..0x2b** (17 siti `0x28+i`), 7-13 array `int[4]` della config, selettore cursori del framework a 4 (oggetto 0xb8, p=0..3 srotolato, PageMgr+0xc68[4]), pagina kart +0x460[4]. Il clamp ips_A 0x4bc630 era su +0x660, non +0x904 | A1 §3, A2 §2 |
| 0x4ba77c per N>4 è innocuo | con N>4 i cicli `i<N` di 0x4b7218→0x3fd5a4 e 0x3fdd70 leggono le "pagine CharaWin" 0x2c..0x2f (NULL) e l'entry NULL del pad 4: **35 accessi a indirizzi nulli nel log Eden dello step 4** (Eden li tollera, console no) | A2 §2.3, `a2_eden_unmapped_step4.txt` |
| la selezione scrive il race-config con cicli a 4 | tutto passa dallo **slot** (`0x8be03c(0x8bf158(slot))` → setter 0x42022c/0x4202f4/0x420ac0/0x420b88/0x420c50); nessun ciclo fisso a 4: con pad UI 4..7 e mappa a 8 gli indici diventano validi da soli | A2 §4 |

## 1. Decisione: 8 cursori simultanei vs selezione a due turni

Otto cursori simultanei (A2-full) richiedono di estendere il framework UI (selettore 0xb8 con 4 stati, 19 clamp
`cmp #4`, PageMgr+0xc68[4], memoria cursori [0x88][4], oggetti grafici cursore [4], pannelli pagina 0xe [4]),
4 pagine CharaWin extra con rimappaggio per indirizzo di ritorno, la pagina kart (+0x460[4], +0x39c, selettori
4..7) e array ombra della config: decine di siti con semantica incerta. **Non è raggiungibile in questo step con
rischio accettabile** (dettagli e catalogo in step5_plan_A1 §3.4, A2 §2.2/§2.5/§3.3/§6.1 "A2-full").

Scelta per lo step 5: **selezione a due turni con 4 cursori**. Turno 1: i pad UI 0..3 (locali 0..3) scelgono e
confermano. Turno 2: il modulo rimappa `0x8bf158(i)` per i<4 sui pad extra (idx 4..7 → locali 4..7), riporta
gli slot 0..3 allo stato "sceglie" e i giocatori 5-8 scelgono con i propri controller; la catena del gioco
(`slot → pad → 0x86ebf8(idx) → entry`) scrive il race-config dei locali 4..7 con i setter originali. Stessa cosa
nella pagina kart. Ogni giocatore sceglie personaggio e kart in modo indipendente (non simultaneo). Il layout
6pl/8pl viene caricato (A1-M1) ma con 4 pannelli attivi: i pannelli 4..7 restano statici [limite documentato].

## 2. Sotto-obiettivi, in ordine (un commit ciascuno, test in Eden dopo ognuno)

### S5.1 – raceMgr a 8 locali e giocatori di input 4..7 (piano B, + H-A2-0)
- B1: mappa slot→entry a +0x19c: patch `b9018d8b→b9019d8b` a 0x868e30 0x868ee4 0x869684 0x8699ec 0x86b940,
  `b9018d01→b9019d01` a 0x86b0b4, `b9818d1a→b9819d1a` a 0x88236c, `b9818d15→b9819d15` a 0x882408; REPLACE
  0x86af68 (SlotSet a 12 voci); trampolino ctor 0x8689b8 (memset -1 su +0x19c..+0x1cb); REPLACE 0x868f00
  (compattazione a 8); `71000e7f→71001e7f` a 0x86ec08; `71000d1f→71001d1f` a 0x86901c, 0x86b970.
- B2: PIM a 8 giocatori: patch 0xca3fc `52800500→52800900`, 0xca410 `528000a8→52800128`; trampolino 0xca3e0
  (G1 buffer esterno a 8, G3 creazione GamePlayer 4..7); trampolini 0x7c5340, 0x7c6218, 0x7c62bc (G4), 0x7c5044
  (G5), 0x7c60c0 (G6); G7 in coda a PimReset1/2Hook; G8 su 0x7ce38c e 0x7ce8c4 (ordine +1 per 4..7).
- B3: REPLACE 0x86a594 (GamePlayer libero fra 0..7).
- B4: trampolini 0x86a998 (dopo), 0x86adec (prima), 0x86ad18 (prima); patch 0x86ac14 `f100129f→f100229f`.
- H-A2-0: trampolini 0x3fd5a4 (salta i≥4 / obj NULL) e 0x3fdd70 (N limitato a 4 durante la chiamata) finché
  gli slot 4..7 non hanno entry valide (poi il secondo diventa inerte).
- Log: MenuEventHook esteso a +0x17c[0..7] e S[0..11]; InitPlayersHook; AssignGamePlayerHook.
- Accettazione: 8g → `race+0x178=8`, `+0x17c={0..7}`, S={0..7,-1,…}; nessun accesso nullo @0x1d0/0x368; 1P e 4P
  come prima (mappa slot spostata è attiva sempre: gara completa 4P + schermata risultati).

### S5.2 – pad UI 4..7 (piano C)
- C1 (12 × `71000c1f→71001c1f`), C2 (4 × `321e03e0→321d03e0`), tutto-o-niente; C3 trampolino 0x8bf040 (pad tocco
  idx 4→8, 4 pad extra in memoria statica); C4 REPLACE 0x8bf158 (4..7 → extra solo con N>4; 8 → tocco);
  C5 trampolino 0x8bf058 (calc extra + fusione); C6 PadReadyHook senza il caso speciale H4.
- Accettazione: 8g → applet una volta, `pad UI 4..7 pronto=1` (veri), tasti 5..8 → pulsanti dei pad extra;
  1-4P: nessun `active8`, regressione menu/charsel 4P.

### S5.3 – layout 6pl/8pl (piano A1-M1)
- H-A1a trampolino 0x4be854 (nome Page_Lyt 6pl/8pl per la pagina 0x16 con N 5-6 / 7-8), H-A1b inline 0x4beb04
  (w20 = 6/8). Nessuna patch di stringhe.
- Accettazione: 8g → grafica 8pl, 4 cursori attivi, nessun crash; 2-4P invariato. Rischio: momento di
  costruzione della pagina 0x16 (backtrace nel log).

### S5.4 – selezione a due turni e log dei personaggi assegnati (piano A2, variante)
- Charsel: trampolino 0x4ba77c. Turno 1 → quando gli slot 0..3 sono tutti in stato 7 e N>4: `round=2`,
  rimappa, slot 0..3 → stato 3 come 0x4ba71c (stato 3, bit i di +0x1a8, 0x8ba814(this,i), 0x14b508(i)).
  Turno 2 → quando sono di nuovo tutti 7: cfg+0x1fa4 = 4 per la sola chiamata (w28 == w22 a 0x4ba938 → avanza).
- Tipi: trampolino 0x4badb0: dopo Orig, se stadio 1 e N>4, `0x42022c(e, &0)` per tutti i locali 0..N-1
  (il ciclo 0x4bb108 copre solo i 4 pad correnti) e cfg+0x1434 coerente.
- Kart: trampolino 0x4c0898 (onEnter: round=1) e 0x4c2820: prima di Orig, se tutti gli slot con charsel 7 hanno
  +0x460[i]==3 e N>4 e round==1: round=2, rimappa, +0x460[i] = 4 (reinizio) e `0x4c1bfc(page,i,0/1/2,1)` per i<4
  (parti iniziali dei locali 4..7). Turno 2 completo → Orig avanza.
- Log "personaggi assegnati" (A2 §4.4) su 0x4c2820 quando +0x498 passa a 1: entry, personaggio/parti/tipo, descrittori
  settingsB, sB+0x188[0] == N.
- Accettazione: 8 giocatori scelgono personaggio e kart (due turni), selezione coppa, gara con 8 locali
  (`race+0x178=8`, N entry tipo 0 con campi ≠ -1).

### Chiusura
STATE.md, STRUCTS.md (correzioni: pad UI 4 = tocco, +0x17c int[8]/+0x19c, 0x86a594, selettore a 4, CharaWin,
campi classe selezione), `step5_charsel.md` con esiti, e `blocco_step5.md` per il criterio
"8 cursori simultanei / layout 8pl con 8 slot attivi" (non raggiunto: catalogo e stima).

## 3. Rischi di regressione 1-4P da verificare
| modifica | perché può toccare 1-4P | verifica |
|---|---|---|
| B1 mappa slot +0x19c | attiva in ogni partita | gara 4P completa + risultati + "gara successiva" |
| B2 8 GamePlayer sempre | giocatori 4..7 legati al misc, stato 0 (come 1..3 in 1P) | gara 1P, log periodico stato giocatori |
| C4 GetPad REPLACE | risposte identiche per N≤4 (clamp replicato) | menu/charsel 4P |
| S5.4 trampolini | attivi solo con N>4 | charsel 4P → kart → gara |
