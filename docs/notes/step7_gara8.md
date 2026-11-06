# Step 7 – pausa, fine gara e classifica a 8: esito e riferimenti Switch 2

Data: 3 nov 2026. Piani: step7_plan.md (pausa, risultati, debug), step7_plan_cursor.md (cursore/colore pausa).
Screenshot in step7_data/ (non in git: *.png).

## Riferimenti dell'interfaccia ufficiale Switch 2 (forniti dall'utente)
- `ref_switch2_gara8.png` – gara VS a 8 (Mario Kart Stadium), griglia 4x2:
  - minimappa al centro dello schermo **più piccola** che nel 4P (stima dal confronto con eden_7_gara8_fine.png: circa
    0.75 della dimensione 4P, icone dei giocatori più piccole), non copre l'HUD delle celle vicine;
  - riquadro oggetti in alto nelle celle: colonne 1-2 a sinistra, colonne 3-4 a destra (specchiati rispetto al centro);
  - posizione in basso a sinistra (colonne 1-2) / a destra (colonne 3-4), monete e giri in basso a sinistra; giri anche in
    grande "2/3" in alto a destra in alcune celle (oggetto HUD separato).
- `ref_switch2_classifica8.png` – classifica di fine gara a 8: 12 righe; le righe dei giocatori hanno il colore del
  giocatore: giallo, ciano, blu, rosso, arancio, viola, verde, rosa (le CPU in grigio scuro).

## Esito in Eden (gara a 8 completa, guidata con acceleratore automatico)
- `eden_7_gara8_fine.png`: logo "FINE!" nella cella giusta (P2), HUD completo; **minimappa troppo grande**: le icone
  della mappa coprono l'HUD (riquadro oggetti) delle celle 6 e 7 → corretto con RaceMapParamHook (0x533368, scala
  e traslazione ×0.75 con split 8).
- `eden_7_classifica8.png`: classifica a 12 righe corretta (posizioni, punti, personaggi); **colori delle righe dei giocatori
  5-8 sbagliati** (ripetono giallo/verde/rosso/ciano dei giocatori 1-4 invece di quelli propri; P5-P8 hanno colori ≥ 4 da
  0x86a708 rimappati a 0..3 dal nostro PlayerColorIdxHook) → C1 di step7_plan.md §2: va usata la palette a 8 (frame/colori
  5-8 della RomFS 4.0.1) invece della rimappatura, insieme ai cursori a 8 dello step 5b.
- `eden_7_menu_fine.png`: menu di fine gara (Prossima corsa / Momenti salienti / Abbandona) con sfondo catturato e scurito
  in tutte le 8 celle.
- Pausa (`eden_7_pausa8`, sessione delle 14:38): 8 pannelli 4x2, sfondo corretto in tutte le celle.

## Correzioni dopo il confronto con Switch 2 (3 nov 2026, verificate a inizio gara in Eden)
- **HUD per colonna**: 0x50efa8(lyt, slot) imposta il frame PlayerPos = (slot&1)+2 (pari = sinistra); con split 8 lo slot passato
  diventa 0 per le colonne 1-2 e 1 per le colonne 3-4 (RaceViewSideHook). Celle 2,3,6,7 con l'HUD verso l'esterno come su Switch 2.
- **Minimappa**: 0x533368 carica scala/traslazione da tabella (race+0x338/+0x33c/+0x340; 4P Mario Kart Stadium: scala 0.6);
  con split 8 moltiplicate per 0.65 (MK8DX_MAP8_SCALE, scelto dall'utente provando 0.55/0.60/0.65/0.75).
- **Aiuti P5-P8** (step7_plan_hud8.md §B): 0x3fe040 (salva) e 0x3fdd70 (carica) ciclavano su 8 locali scrivendo/leggendo gli
  array ospiti int[4] cfg+0x2310/+0x2320/+0x2330 fuori limiti (aiuti di P2-P4 e struttura cfg+0x2340 con un puntatore). Ora limitate
  a 4; P5-P8 in un'ombra del modulo (aid8::code) valida finché il gioco resta aperto. Persistenza su file: step 5b (config per giocatore).
- **Pool di exlaunch**: superati gli ~81 trampolini di JitSize 0x4000 (abort in hook_impl.cpp:602 all'avvio): JitSize e
  InlinePoolSize portati a 0x8000 in src/program/setting.hpp.

## Esito dello step 7
Completato per quanto verificabile: pausa a 8 completa, fine gara a 8 con logo FINE nella cella giusta, classifica a 12 righe,
menu di fine gara, HUD e minimappa come Switch 2. Rimandati: colori dei giocatori 5-8 (HUD posizione, righe della classifica,
cursori: palette a 8, step 5b), persistenza aiuti P5-P8 (step 5b), Gran Premio completo e regressione 1P/4P (PC veloce).

## Confronto Switch 2 del 4 nov 2026 sera (PC Ryzen Z2 Extreme) [VERIFICATO in Eden]
Screenshot normalizzati a 1280x720 (riferimento senza la barra del titolo di 34 px): monete/giri, posizione, lato del riquadro
oggetti, anello colorato e minimappa coincidono. Differenza trovata e corretta: **disposizione interna del riquadro oggetti** nelle
colonne 3-4 (Switch 2: speculare, cerchietto del secondo oggetto a destra; mod: cerchietto a sinistra, cerchio grande a x 264/320
invece di 246 = specchio di 74).
- Causa (analisi statica, subagente): onCreate dell'ItemBox 0x510460 sceglie N_All_01 (cerchietto a sinistra) / N_All_02 (speculare)
  con la parita' del posto locale k della Scene (0x510820..0x51098c, `tbnz w25,#0` a 0x510a6c); w25 riletto solo a 0x510e38 (id
  ItemTeresa 0x34|k). Nel 4P originale le celle di destra (k dispari) sono speculari. Con 8 celle: celle 1-4 = k 0..3 (colonne 2 e 3
  invertite), celle 5-8 non nella Scene (k = 0, tutte a sinistra).
- Correzione (split 8): inline 0x548024 registra ItemBox -> cella (oggetto viewport X24 +0x100), inline 0x510a68 imposta w25 = colonna
  (0 per 1-2, 1 per 3-4), inline 0x510e38 ripristina k. Screenshot 23:22: colonne 3-4 speculari come Switch 2.
- Classifica finale a 8 (screenshot 23:06) uguale al riferimento: 12 righe, punti 15..1, 8 colori dei giocatori, CPU grigio scuro.
  Logo d'arrivo nelle celle 5-8: presente (utente). Gare a 8 complete senza errori.
