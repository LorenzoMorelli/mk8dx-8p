# Step 4 – flusso menu 5-8 giocatori: report (2 nov 2026)

Piano statico: step4_menu_plan.md. Codice: src/program/main.cpp (namespace menu8, PadReadyHook, patch P5a/P5b).

## Esito [VERIFICATO in Eden]
| criterio | stato | prova |
|---|---|---|
| 5-8 portano alla schermata successiva invece che al dialogo | OK: 8g e 5g -> scelta modalità Multi -> selezione personaggi (4 posti, atteso: step 5) | eden_log_4a_menu58 |
| numero corretto memorizzato | OK: 8g -> cfg+0x1fa4=8, 5g -> 5; modo cfg+0x1fa8=3 (Multi) | righe "step4: pulsante" |
| nessun loop dell'applet | OK: 0x49ea60(min=8, max=8, 1, 0) chiamata una volta (da main+4baa70), poi pad UI 1..4 pronti | righe "applet:" |
| regressione 4P | OK: 4g -> applet min=max=4 una volta -> gara (58.8 frame/s) | eden_log_4b_regr4p |
Dopo 8g, tornando indietro e scegliendo 5g l'applet non compare: gli 8 controller erano già associati e i pad
UI risultano pronti (stesso comportamento del gioco originale passando da 4 a meno giocatori).

## Modifiche
| # | tipo | offset | effetto |
|---|---|---|---|
| P5a | `b.hi` -> `b` (54001708 -> 140000b8) | 0x4d9468 | 5g-8g non aprono il dialogo "serve Switch 2" (= ips_v2) |
| P5b | `b.gt` -> `nop` (5400048c -> d503201f) | 0x4d9898 | blocco count>4 (nuovo nel 4.0.1): 5-8 seguono il ramo 3/4P (modo 3, argomento audio 4 invece di 0 -> lettura fuori tabella in 0x13e59c) |
| H4 | trampolino | 0x8be248 | pad UI 4 ("speciale", rifiutato da `cmp #3`) pronto per N>4 se gli npad 4..N-1 sono connessi: esce dal loop dell'applet. N<=4: Orig |
| log | trampolini | 0x4d92e8 (conferma 2g-8g), 0x49ea60 (chiamante, LOOP!) | accettazione |

## Aperti per lo step 5/6 (dettagli in step4_menu_plan.md §5 e step4_data/sites_step5.txt)
1. raceMgr+0x178 (n locali) resta 4 e +0x17c {0,1,2,3}: 0x869180 non porta N al gestore di gara; la tabella
   +0x17c è int[4] e il riempimento senza limite sforerebbe in +0x18c con N>4 (rilocazione a 8).
2. Pad UI 4..7 veri (lo speciale da 4 a 8), `cmp #3` -> `#7` in 12 siti 0x8be040..0x8beb90 e 0x86ec08.
3. Giocatori di input PIM 4..7 (G1-G8 di step3_input_plan.md) e 0x86a594 (GamePlayer solo per 0..3).
4. Selezione personaggi a 6/8 posti; pagina di gara 0x49c938; audio 0x13e59c; arg applet a 0x430 B per console.
