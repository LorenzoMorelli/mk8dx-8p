# Step 3 – input a 8 controller: report (2 nov 2026)

Piano statico: step3_input_plan.md (architettura input, correzioni allo step 1, applet). Qui: cosa è stato
implementato e verificato in Eden. Codice: src/program/main.cpp (namespace input, hook NpadInit/Pim*/ControllerMgrCalc).

## Esito
| criterio | stato | prova |
|---|---|---|
| 8 controller riconosciuti | OK: NpadDevice interroga 8 pad + handheld; 10 controller in lista (pad 0..3, handheld, misc, **pad 4..7**) | log "input: lista controller 10/12", "pad interrogati=8" |
| input distinti per gli indici 0..7 | OK: tasto `N` -> X (`00000008`) solo sullo slot npad N-1 e solo sul controller di gioco corrispondente | eden_log_3a_raw8 (livello hid), eden_log_3b_ctrl8 + 3c (livello gioco, npad 7 in 3c) |
| regressione 1P | OK: gara 1P a 60 frame/s, guida normale | eden_log_3b_ctrl8 |
| regressione 4P | OK: applet min=max=4, poi pad UI 1..3 pronti, gara avviata | eden_log_3d_4p_applet |
| assegnazione a 8 giocatori locali | NON ancora: i giocatori di input (PIM+0x50) restano 4 (passo 3b del piano: G1-G8), serve con il menu (step 4) | step3_input_plan.md §2c |
| applet (count,count) per 5-8 | analizzato, non implementato: il loop dipende da 0x8be248 (`cmp #3` sull'indice del pad UI), da sistemare con il flusso menu (step 4) | step3_input_plan.md §3 |

Mappa verificata (livello gioco, sead::ControllerMgr+0x188):
| posizione | controller | npad | stato |
|---|---|---|---|
| 0..3 | NpadController (originali) | 0..3 | connessi |
| 4 | NpadController handheld | 8 | invariato (ordine 4 = handheld usato come costante dal gioco) |
| 5 | Misc (id 0x12) | – | invariato |
| 6..9 | **NpadController nuovi** | 4..7 | connessi, hold distinti |

## Modifiche
| # | tipo | offset | effetto |
|---|---|---|---|
| P1 | patch `bl` -> `nop` | 0x7c54ac | non riduce più gli npad supportati a 5: resta il set a 9 id del ctor NpadDevice (0x5fbdc8) |
| P2 | `orr w1,#4` -> `#8` | 0x7c54b0 | NpadDevice interroga i pad 0..7 |
| P3 | `orr w8,#7` -> `mov w8,#12` | 0x7e4468 | capacità della lista controller 7 -> 12 |
| P4a-c | dimensioni `new[]` | 0x7c5dc0 / 0x7c5dcc / 0x7c5ddc | tabelle PIM per posizione +0x98/+0xa0/+0xa8 da 7 a 16 voci |
| H1 | trampolino | 0x7c53ec | crea i NpadController 4..7 (stesso corpo del ciclo 0x7c54e4-0x7c5590) e li accoda (posizioni 6..9) |
| H3 | trampolini | 0x7c5d88 (dopo), 0x7c58d0 e 0x7c5794 (prima), 0x7c4e50 (dopo) | inizializzazione e gestione per frame delle voci 7..15 delle tabelle PIM |
| H2 | trampolino (log) | 0x6192a4 | log variazioni slot NpadDevice 0..8 e hold dei controller di gioco |
| diag | trampolini (log) | 0x8be248, 0x49ea60 | pad UI pronto / chiamate applet |
Le patch verificano la parola originale; H1/H3 si installano solo se P3 e P4 sono andate a buon fine.

## Eden con più di 4 controller (documentazione)
- Eden emula 8 Pro Controller + handheld: `qt-config.ini` sezione `[Controls]`, chiavi `player_N_connected`,
  `player_N_type` (0 = Pro Controller) e le mappature `player_N_button_*` / `player_N_lstick` per N = 0..7
  (player_8 = handheld). Per attivare un'opzione serve anche `<chiave>\default=false`.
- Un pad è visibile al gioco solo se il gioco ne dichiara l'id: senza P1 MK8D dichiara 5 id
  {0,1,2,3,handheld} e i giocatori 5..8 di Eden non arrivano allo slot NpadDevice.
- Configurazione usata per il test (backup `eden/user/config/qt-config.ini.bak-step3`): giocatori 1..8 connessi,
  stessa mappatura da tastiera condivisa, più X diverso per ognuno: tasti `1`..`8` (codici Qt 49..56).
  Con la mappatura condivisa tutti i kart/cursori ricevono lo stesso input di direzione (atteso).
- Il frontend dell'applet controller di Eden prende `max_players` dall'header senza limitarlo a 4
  (applet_controller.cpp:37-38, step3_input_plan.md §3).

## Note / problemi aperti
1. In una sessione (eden_log_3c) la partita 4P è partita senza applet: nessuna chiamata a 0x49ea60 registrabile
   (il log diagnostico non c'era ancora). Con avvio pulito (3d) l'applet compare come prima della mod.
   Ipotesi: in 3c l'utente aveva premuto tasti sui pad al titolo e i giocatori erano già associati.
   Da ricontrollare nello step 4 con il log diagnostico attivo.
2. Vibrazione: i controller 4..7 non sono collegati ai target di vibrazione (già 9 nel gioco).
3. Console reale [IPOTESI]: l'arg dell'applet del gioco è la versione a 4 giocatori (0x21c B); per 5-8 potrebbe
   servire l'arg nuovo da 0x430 B.
4. In una sessione Eden si è chiuso da solo durante il caricamento shader (3-4 s, prima dei moduli del gioco,
   nessun errore nel log): non attribuibile alla mod.
