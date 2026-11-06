# Step 2 – prototipo viewport 6/8: report finale (2 nov 2026)

Main 4.0.1 (BID 158C0C73...), modulo exlaunch `subsdk9` (src/program/main.cpp), test in Eden v0.2.0 docked.
Build: `MK8DX_VIEWS=8|6|4 scripts/build.sh` (4 = riferimento: stessi hook, nessuna vista extra).

## Esito
| criterio | stato | prova |
|---|---|---|
| 8 viste corrette in gara | OK: griglia 4x2, mondo disegnato in tutte, celle 5-8 seguono kart CPU | screenshot_2c_8v_ok.png, run7 |
| 6 viste corrette in gara | OK: griglia 3x2, celle 5-6 seguono kart CPU | screenshot_2c_6v_ok.png, run8 |
| allineate al layout | 3D sì (celle esatte); HUD/minimappa NO (restano al 4P, vedi "cosa è rotto") | screenshot_2c_8v_prio.png |
| nessun crash in una gara completa | OK (8 viste): gara 4P completa fino ai risultati a 55-60 frame/s (run12, screenshot_2c_8v_fine_gara.png); poi transizioni gara -> tipo 4 -> gara (renderer riusato) e gara -> tipo 5 -> nuova gara (renderer distrutto e ricreato, layer ricollegati 4..7) ripetute senza crash (run13). 6 viste: solo gara parziale | log run12, run13 (0 ExceptionRaised) |
| FPS / memoria vs 4P | vedi sotto (misura poco affidabile: host in risparmio energetico in parte delle prove) | misura: nei log |
| mappare +0xcc ai layout rc_RaceView_6P_Ml / 8P_Ml | NON fatto: rimandato allo step HUD (il modo split resta 2 = 4P; l'HUD a 8 serve comunque pagine nuove) | – |

Tutti i test sono gare 4P (4 umani) con split forzato: le viste 5..8 sono kart CPU (indici kart 4..7).

## Cosa fa il prototipo [VERIFICATO in Eden]
1. **Layer 3D extra**: 2 o 4 `gsys::Layer3D` (alloc 0x5faba8(0x1090, heap corrente), ctor 0x68d0e4) registrati con
   0x667a94 agli slot 14..17 (capacità layerSystem 32; la LayerHeap è piena, da qui heap nullptr), camera
   LookAtCamera e proiezione propri (copie di mgr+0x1d8 / mgr+0x238) in layer+0x68 / +0x70.
2. **Renderer di scena "Main" a N viste**: hook 0x6a8f10(name, arg, heap): in scena gara (tipo 3 passato a
   SceneInit 0x7e7950) arg.numViews 4 -> N; R+0xf0 = N. Hook 0x3cce58(mgr, R): dopo Orig, setter
   0x68d4f4(layerExtra, R) -> indici vista 4..N-1 (log "collega ... -> indice 4..7").
3. **Modelli a N viste**: hook 0x69bb0c(model, arg, heap): arg+0x8 4 -> N (sempre attivo). Senza, con viste 4..7
   i dati per vista model+0x60 (4 x 0x58) vengono letti oltre la fine e le shape corrotte -> crash nel thread di
   render (PC=4/5, 0x6e1524/0x6e223c <- 0x721450): run4, run6; bisezione run5 (R a 8 + layer collegati ma spenti: OK).
   Report: step2_crash8v.md.
4. **Griglia per frame** (dopo 0x3cc218): sub3d[0..3] -> celle 0..3, extra j -> cella 4+j; rect in unità
   1280x720 con **y=0 in basso** (riga 0 della griglia = y 360..720). 8 viste: celle 320x360; 6 viste: 426.67x360.
   Aspect della proiezione aggiornato ([proj]+0xb0, flag [proj]+0x8 = 1).
5. **Camere dei kart CPU**: il descrittore del director per i CPU resta fisso (tipo 1, pos 0,0,10: KartCamUnit
   con +0xc8 = 0 per i CPU, step2_extraviews.md §2). La camera si ricostruisce dalla posa del kart
   P = [[[e0-8+0xb0]+8]+0x28]+8 (X lato, U su, F avanti, T posizione; verificato ortonormale: X=-1,0,0 U=0,1,0
   F=0,0,-1) riapplicando l'offset della camera del giocatore 1 nel sistema del suo kart (eye ~ +24 su, ~49 dietro).
   Poi 0x619a08(cam, cam+8) (doUpdateMatrix).
6. Attivazione solo in gara con modo split >= 2 e layer collegati al renderer corrente; spegnimento prima
   della ricreazione del renderer.

## Geometria (docked 1920x1080; unità interne 1280x720, scala mgr+0xbdc = 1.0)
| modo | celle | dimensione cella (px) | ordine |
|---|---|---|---|
| 1P | 1 | 1920x1080 | – |
| 2P (gioco) | 2 affiancate | 960x1080 | sx, dx |
| 4P (gioco) | 4 quadranti | 960x540 | P1 alto-sx, P2 alto-dx, P3 basso-sx, P4 basso-dx |
| 6 viste (mod) | 3x2 | 640x540 | riga alta P1..P3; riga bassa P4, CPU 5, CPU 6 |
| 8 viste (mod) | 4x2 | 480x540 | riga alta P1..P4; riga bassa CPU 5..8 |
Viewport NVN osservati in 8 viste: x = 0/480/960/1440, y = 0/540, w=480 h=540 (log run4 "nvnSetViewport nuovo rect").

## Prestazioni e memoria
| build | condizioni host | frame/s gara (log modulo) | Eden |
|---|---|---|---|
| 4P originale (2A, run3 prima della mod a 8) | normale | 60 | 60 FPS |
| 8 viste (run7) | normale | 58.1 (55.7-60) | 57 FPS |
| 6 viste (run8) | dopo riavvio, cache shader fredda | 32-39 | 29 FPS |
| riferimento 4 viste (run9) | risparmio energetico | 19-23 | – |
| 8 viste (run11) | risparmio energetico | 35.7 | 34 FPS |
| 8 viste, gara completa (run12) | normale | 56-60 (quasi sempre 60.0) | 60 FPS |
Le misure con l'host in risparmio energetico non sono confrontabili fra loro (run9 4P è più lenta di run11 8P):
il dato confrontabile è run7 (8 viste 58 vs 60 in 4P). In Eden il costo GPU dell'host domina; su Switch reale
il costo CPU/GPU di 8 viste va rimisurato.
Memoria: `svcGetInfo(UsedMemorySize)` resta 3213352-3213356 KiB / 3363840 KiB in tutte le build (gli heap del
gioco sono preallocati, quindi non misura il consumo reale). Nessuna allocazione fallita: 0x6a8f10 restituisce
R valido, 0x69bb0c ret=1 per tutti i ~6400 modelli estesi, nessun abort. Costo stimato [IPOTESI]: per modello
+(N-4) x (0x58 B + dati per vista g3d), più N-4 viste da 0x3e0 B + 0x1450 B in R.

## Cosa è rotto / limiti
1. **HUD**: restano le 4 pagine 2D del 4P posizionate ai quadranti 960x540 (2D Sub0..3 slot 6..9, rect
   quadranti). In 8 viste (screenshot_2c_8v_prio.png) l'HUD compare solo nelle celle 1 e 4: cella 1 = HUD del
   giocatore 1 (angolo basso-sx del quadrante alto-sx), cella 4 = HUD del **giocatore 2** (angolo basso-dx del
   quadrante alto-dx), quindi mal attribuito. Le celle 2 e 3 (giocatori 2 e 3) non hanno HUD; gli HUD dei
   giocatori 3 e 4 (quadranti bassi) sono coperti dalle viste extra. In 6 viste stesso schema (celle 1, 3, 4).
   Nessun HUD per le viste 5..8 (le pagine RaceView sono fisse a 4+4, step 1).
2. **Minimappa e 2D nelle celle 5..8**: la metà inferiore della minimappa (2D Main, slot 10) e l'HUD dei
   quadranti bassi non si vedono sopra le viste extra. Non è la priorità dei job (dump run11: Sub0/1 = 546,
   Sub2/3 = 5, extra = 544, 2D = 10, soglia bucket ls+0x50c = 10; le celle di Sub0..3 mostrano il 2D sopra
   sia con 546 sia con 5). Ipotesi: la composizione delle viste 4..7 avviene in un passaggio successivo del
   renderer di scena; da indagare (report step2_draworder.md, dump in eden_log_2c_run11_8v_layers.txt).
3. **Camere CPU**: copiano l'offset della camera del giocatore 1 (niente Lakitu/retromarcia/look-back
   propri, niente transizioni di partenza/arrivo). Il kart 0 è assunto = giocatore 1.
4. Schermata di fine gara: il menu 2D ("Prossima corsa", "Momenti salienti") è coperto nella riga bassa come
   la minimappa. Regressione 1P non eseguita (l'utente l'ha data per buona); 6 viste non provate su gara completa.
5. Il modo split del gioco resta 2 (4P): pausa, risultati, effetti per vista e audio per vista restano a 4.
6. Le viste extra seguono indici kart fissi 4..7, non giocatori umani (gli umani 5-8 sono step successivi).

## Dati
step2_data (dati non pubblicati): log filtrati eden_log_2c_run1..11 (*.txt; grezzi *.raw.txt e screenshot *.png solo in
locale, esclusi da git), analisi ev_*.asm, vm_*.txt. Report correlati: step2_renderlayers.md,
step2_viewmatrix.md, step2_extraviews.md, step2_crash8v.md, step2_draworder.md.
