# Step 6 – gara con 8 locali: report intermedio (2 nov 2026, sera)

Piani statici: step6_plan_A.md (gestore kart M), step6_plan_B.md (viste/HUD/camere), step6_plan_C.md (ordine di
disegno). Codice: src/program/main.cpp (namespace kart8, view8, hud8; hook 6A, N1-N3, H1, H2, 6C). Log filtrati e
screenshot in step6_data/ (eden_log_6*.txt, eden_6*.png).

## Esito [VERIFICATO in Eden, 8g]
| criterio | stato | prova |
|---|---|---|
| la gara con 8 locali parte | OK (prima: salto a PC=0 in 0xaa16c per l'array K*[4] di M+0xa0 riempito a 8) | gdb_crash1_race8.txt, eden_log_6b |
| 8 viste, ognuna con la camera del proprio kart | OK: Scene 4P coerente (locali 0..3 → vp 0..3) + viste extra con il descrittore camera reale dei kart 4..7; 60 frame/s | eden_6c_race8_views.png |
| ogni giocatore controlla il proprio kart | OK: 8 GamePlayer in stato 4 legati a npad 0..7, input filtrato per tutti | diag8 in eden_log_6e |
| HUD nelle celle 1-4 | OK con layout `rc_RaceView_8P_Ml` (posizione, oggetto, monete, giri) | eden_6d/6f |
| HUD nelle celle 5-8 | PARZIALE: pagine RaceView4..7 create e aperte (monete/giri visibili) ma **tinta rossa e senza numero di posizione** | eden_6l_race8_hud58.png |
| 2D a schermo intero (mappa, 3-2-1, classifica, menu) | pausa e risultati OK (layer extra nascosti in cattura); **in gara mappa e 3-2-1 restano coperti dalle celle 5-8** | eden_6h_pause_ok.png, eden_6m |
| gara completa → risultati → gara successiva | OK (il Gran Premio prosegue) | eden_log_6g |
| pausa / risultati a 8 | layout 4P (pausa) e classifica 4P: step 7 | eden_6h |
| regressione 1-4P | menu 4g OK; **gara 1P/4P completa con la mod non ancora rifatta** | – |

## Modifiche (tutte con verifica della parola originale / dei prologhi; gruppi tutto-o-niente)
| gruppo | tipo | siti | effetto |
|---|---|---|---|
| 6A | 15 patch imm + trampolini 0xa8c00, 0xac290 | 0x9c190 (new M 0x2978→0x3080), 0xac360, 0xaa1b0, 0xaaa24, 0xab4d0, 0xaad58, 0xaad7c, 0xaae30, 0xb2070, 0xb2094, 0xac374, 0xac380, 0xaa9e4, 0xaa9f0, 0xaad9c | array dei locali del gestore kart M: K*[8] a +0x3000, s32[8] a +0x3040 |
| N1 | esteso SceneInitRaceHook 0x7e76b8 | – | con n>4: Scene 4P coerente, `view8::split` 6/8, `kartOf[8]`, `extraVp[4]` |
| N2/N3 | proto::PerFrame | – | viste extra solo con split; celle da split; camera reale (desc tipo 1/2: pos/at/up/fovy) con fallback alla posa |
| H1 | REPLACE 0x54798c/0x548d20, trampolino 0x536a10 | – | layout 6P/8P (Front_6P), partizioni y3/y4 (trovate, impostate, ma non visibili), layer 2D Sub0..3 sulle celle con camera ×2 |
| H2 | trampolini 0x53186c, 0x406870, 0x8b21dc; inline 0x547d5c; trampolini 0x86eb84, 0x86eb1c; 4 Layer2D (slot 18..21) | – | pagine RaceView4..7 (id 0x12/0x1d/0x43/0x5b), oggetti per viewport 5..8 (race+0x358 a 9), apertura come il gioco |
| 6C | copia bit 0x2/0x4 di layer+0xde4 da Sub0; trampolino 0x683ecc | – | extra nascosti in modo cattura (pausa/risultati OK); extra disegnati nel job di Sub0 (nessun effetto in gara) |
| diag | CharselProfileHook 0x4b7218 (clamp N a 4), dump id pagina, diag8, gruppi R | – | niente letture nulle nella selezione; dati per H2 |

## Scoperte
- Il crash era l'array `M+0xa0` K*[4] (M = G+0x850, 0x2978 B): la registrazione 0xac290 lo riempie con indice M+0x104
  senza limite; i locali 4..7 finivano su +0xc0..+0xd8 (altri membri). Con 0x3cd194 e 0x7e76b8 letti per intero, la
  Scene (0x128 B) non si può ingrandire: scelta la Scene 4P coerente + tabella del modulo.
- Le pagine RaceView0..3 (id 0x65..0x68) e Front (0x6a..0x6d) sono aperte da 0x8b21dc(&id, flag) (lista di
  aggiornamento 0x8bd18c + 0x8bbef4 + 0x8bb3e0); la pagina Race (0x69) da 0x8b208c.
- Ordine di disegno: la pausa/risultati funzionano perché 0x703738 (cattura) nasconde con il bit 0x2 di +0xde4 i layer
  con slot inferiore al layer di cattura; i nostri (slot 14..17) restavano fuori → copia del bit. In gara la mappa e il
  3-2-1 (2D Main) restano coperti anche disegnando gli extra dentro il job Draw di Sub0 (6C) e configurando i gruppi R
  4..7 come 0..3 (+0x409 = 1; i gruppi 4..7 hanno +0x408 = 0 e +0x409 = 0xff): la causa NON è la priorità/ordine dei
  job né la configurazione del gruppo. Ipotesi residue: il 2D Main viene disegnato prima dei 3D e protetto da
  depth/stencil che le viste extra non rispettano (es. clear del depth sul rect della cella nel FinalImage di R per
  le viste 4..7), oppure il 2D Main è su un target diverso composto prima. Da verificare con il log dei job (plan C §6 L2)
  o con una cattura GPU di Eden.
- In 4P le due viste inferiori del gioco hanno priorità 5 in certi frame (dump run11) → probabile rendering alternato
  (30 Hz per vista) nel 4P vanilla.

## Known issues (osservati dall'utente il 2 nov 2026, da affrontare negli step 6b/7)
1. **Mappa e conto alla rovescia 3-2-1 tagliati** dalle celle 5-8 in gara (vedi sopra).
2. **HUD celle 5-8**: numeri in rosso e **numero di posizione assente** (le pagine RaceView4..7 usano slot 4..7 in tabelle
   per giocatore a 4 voci: colori, rank; 0x50efa8(obj, slot), 0x547f78).
3. **Flash delle 4 viste inferiori** al caricamento, prima del filmato di intro del circuito (le viste extra si accendono
   appena `split` e il renderer sono pronti: gate da legare allo stato di gara/intro).
4. **Intro e partenza**: il Lakitu con il semaforo e la camera che gira intorno al kart ci sono solo nelle 4 celle in alto;
   nelle celle 5-8 la camera resta fissa (Lakitu per vista 0x7e9b04 `cmp #4`; intro camera per viewport del director).
5. **Pausa a 4** (layout `rc_Page_RacePause_SgOunce` con `L_SwitchBtns8pl_00..07` previsto nello step 7) e **classifica
   risultati in formato 4P** (anteprime 3D dei kart solo per 4).
6. Partizioni `P_Partition_x/y4l/y4r`: pane trovati e impostati visibili ma non si vedono (coperti dagli stessi layer).
7. Pagina risultati: la conferma con A vale per i pad 0..3; i giocatori 5-8 non devono confermare (0x49c938 controlla
   solo 0..3): funziona, ma senza riscontro visivo per 5-8.
8. Nei log restano letture a indirizzi nulli sporadiche nella selezione (Read64 @0x1d0/@0x258) con N>4: Eden le
   tollera, su console no (0x4b7218 ora limitato a 4; altri siti da trovare con il GDB stub, che si ferma su ognuna).

## Prossimi passi proposti
- 6b: ordine di disegno 2D/3D in gara (log L2 dei job di un frame; prova: layer extra senza clear del depth, oppure
  spostare il 2D Main/Front su un layer con priorità sopra la coda 0); HUD 5-8 (colori/rank per slot ≥4); gate delle
  viste extra all'intro; Lakitu e intro camera per viste 5-8.
- Regressione gara 1P e 4P completa (mappa slot +0x19c e M a 0x3080 sono attivi sempre).
- Step 7: pausa 8P (`rc_Page_RacePause_SgOunce`), risultati/classifica a 8, Gran Premio completo.
