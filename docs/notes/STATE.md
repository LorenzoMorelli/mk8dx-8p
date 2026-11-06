# STATE – mk8dx-8p

> Note di sviluppo pubbliche dal 6 nov 2026. I dati citati (log di Eden, screenshot, disassemblato `main.ann.asm`, cartelle `step*_data`)
> non sono pubblicati; i percorsi `../img`, `../tools` si riferiscono alla vecchia cartella di lavoro locale.

Obiettivo: split-screen locale 6/8 giocatori di MK8D 4.0.1 su Switch 1, via hook exlaunch (subsdk9).
Target: title 0100152000022000, update v1507328 (4.0.1), main BID 158C0C7399AABD7BF83795C2EF3484C7.
Offset = main decompresso (../img/main.bin); IPS = offset + 0x100. Analisi di partenza in ../report/.

## Ambiente portabile
`switch_env/` (non in git): gioco .xci, update 4.0.1 .nsp, Eden portabile con `user/` (chiavi, config a 8 controller, nand con
firmware e update, mod installata). Dettagli e requisiti in docs/DEVELOPMENT.md; gli script usano `switch_env/eden` se presente.
La cartella madre `../` (`tools/`, `img/main.bin`, `img/main.ann.asm`, report dell'analisi 2025-26) serve per l'analisi statica.
PC attuale (3 nov 2026, i7-13700H + Arc A370M, Arch): l'`eden.AppImage` originale crasha con SIGILL (build AVX-512,
`vmovups zmm0`), rimosso; `switch_env/eden/eden.AppImage` = Eden v0.2.1 stable (da `~/Projects/eden-patched/stable/`).
Avviare Eden fuori dalla sandbox. Prestazioni su questo PC: vedi "Problemi aperti".

## Step completati
- Step 6b (3 nov 2026) – gara a 8 completa a video: ordine GPU dei layer, HUD 5-8, intro/Lakitu 5-8, partizioni 8P:
  step6b_plan.md, step6b_plan_hud.md, step6b_plan_lakitu.md, log in step6b_data (dati non pubblicati)/eden_log_6b_*.txt.
  Regressione 1P/4P completa NON eseguita (prestazioni del PC), logo di arrivo nelle celle 5-8 non verificato.
- Step 6 (2 nov 2026, in corso) – gara con 8 locali: parte e si gioca a 8 viste con camere e input corretti, HUD 1-4 con
  layout 8P, HUD 5-8 parziale; aperti: 2D coperto in gara, HUD 5-8, intro/Lakitu, pausa/risultati a 8: step6_gara.md.
- Step 5 (2 nov 2026) – selezione personaggi/kart per 8 locali a DUE TURNI, raceMgr a 8 locali, PIM a 8, pad UI 4..7:
  step5_charsel.md; criterio "8 cursori simultanei" NON raggiunto: blocco_step5.md (riferimento = UI Switch 2).
- Step 0 (1 nov 2026) – toolchain Docker + hook di prova. Evidenze in docs/step0_eden_log.txt.
- Step 4 (2 nov 2026) – flusso menu 5-8 verificato in Eden: step4_menu.md (selezione a 4 posti, raceMgr a 4: step 5).
- Step 3 (2 nov 2026) – input a 8 controller verificato in Eden: step3_input.md (giocatori di input 4..7 e applet 5-8 rimandati allo step 4).
- Step 2 (2 nov 2026) – prototipo 6/8 viste in Eden: step2_prototipo.md (con riserve: HUD, gara completa).
- Step 1 (1 nov 2026) – catalogo statico "max 4 locali": step1_catalogo.md (+ step1_input,
  step1_menu, step1_race_view, step1_race_core, dati in step1_data (dati non pubblicati)/), STRUCTS.md popolato.
  Verdetto: GO con due condizioni (rilocazione Scene/viewport: ~98 siti a passo 0x18; pagine HUD
  fisse 4+4 nel framework). Già pronti per 8: wrapper nn::hid (9 slot), vibrazione (18 target),
  raceMgr locale→kart (12), pagine gara per locale (12), pulsanti 2-8, conteggio int.
  Da trovare a runtime (step 2/3/4): ctor Scene, riempitore array controller +0x58, scrittore di
  settings+0x180, geometria viewport (hook di log su 0x582c94).

## Step 0 – toolchain e hook di prova: COMPLETATO (con una riserva)
Risultato [VERIFICATO] (docs/step0_eden_log.txt):
- build in Docker `devkitpro/devkita64:latest` (immagine già presente; g++ in $DEVKITPRO/devkitA64/bin,
  elf2nso/npdmtool in tools/bin): `scripts/all.sh run` = build + deploy + avvio in un comando.
- Eden: `LayeredExeFS patches applied successfully`, `HasNSOPatch ... name=subsdk9`
  (BID F09A129BF640356772FB35157A1F29F6); exlaunch mappato a 0x828db000, main a 0x80e8d000;
  `insn@4d92e8=d105c3ff`, `hook installato a main+4d92e8`.
- 39 eventi loggati dal trampolino (id 0/1/0xa/0x14/0x1a/0xffffffff, kind 0/1/2, valore di ritorno
  = puntatore pagina o 0), sessione di ~150 s di menu senza Fatal/Unhandled/svcBreak.
- Riserva: la gara 1P e i 5 minuti pieni non sono stati eseguiti (l'utente ha ritenuto sufficiente il
  menu); da recuperare come regressione all'inizio dello step 2.
- Nota Eden: "Read heap size 0x6e6f69 bytes from main.npdm" (nca.cpp:94 legge u64 a +0x28 = dentro la
  stringa "Application"): quirk dell'emulatore, uguale con l'npdm originale, non dipende dalla mod.
- Al termine della sessione il processo eden.AppImage resta vivo dopo la chiusura della GUI
  (ForceStop EmuThread): va terminato con `kill $(pgrep -x eden.AppImage)`.

Dettaglio (1 nov 2026):
- Repo creato, exlaunch come submodule (`exlaunch/`, commit f9f4b0dd, GPL-2).
- Overlay: `src/program/` (main.cpp con hook trampolino su 0x4d92e8, setting.hpp, loggers.hpp,
  version.hpp, offsets.hpp), `config.mk` (Module, PROGRAM_ID 0100152000022000), `config.json`
  (npdm generato da ../tools/npdm2json.py sul main.npdm reale: 42 servizi, kernel flags/app type/
  handle table/debug di MK8D, syscalls = unione dei 47 di MK8D con quelli del template exlaunch,
  perché il npdm originale NON concede 0x73/0x74/0x75/0x77/0x78/0x4b/0x4c richiesti da exlaunch).
- Script: `scripts/build.sh` (copia overlay in exlaunch/, make), `scripts/deploy.sh` (copia
  subsdk9 + main.npdm in `eden/user/load/0100152000022000/mk8dx-8p/exefs/`), `scripts/run-eden.sh`,
  `scripts/all.sh` (build+deploy[+run]).
- [VERIFICATO] Eden (eden-repository, patch_manager.cpp:305-333) applica i mod `exefs/` come
  LayeredExeFS (nuovi file inclusi, `subsdk9` e `main.npdm` in EXEFS_FILE_NAMES) e rilegge il
  main.npdm dopo il patch (deconstructed_rom_directory.cpp:170). `svcOutputDebugString` finisce
  nel log come `LOG_INFO(Debug_Emulated, ...)` (svc_debug_string.cpp:30): visibile con il filtro
  di default `*:Info` in `user/log/eden_log.txt`.
Toolchain: devkitPro non è installato sull'host; si usa l'immagine Docker `devkitpro/devkita64:latest`
(scripts/build.sh monta il repo in /work e lancia make in exlaunch/). exlaunch richiede gcc devkitA64 +
libstdc++, `elf2nso`, `npdmtool`, `libnx/switch_rules`: tutti presenti nell'immagine.

## Offset e strutture scoperte
Vedi STRUCTS.md. Riferimenti rapidi (4.0.1):
- 0x4d92e8 gestore eventi menu modalità (this, event*): id = event+0x8, tipo = event+0x28;
  5g-8g = eventi 0x17..0x1a -> dialogo a 0x4d9460.
- 0x7e76b8 split a runtime (n giocatori da 0x86f154): modo 0/1/2, slot 2/4, campo scena+0xcc;
  0x54798c/0x548d20 modo -> nome layout; array viewport 5 x 0x18 a scena+0x3c.
- 0x7c53ec init NPad (5 id, max 4 a 0x7c54b0); 0x5fbdc8 tabella 9 id (0xf554ac); 0x8bf158 array
  controller 5; 0x49ea60 richiesta applet (min,max); 0x4be854 tabella pagine CharaParts (3).

## Hook attivi (src/program/main.cpp)
- Strumentazione (solo log): menu 0x4d92e8, loader NVN 0x582c94 (+ SetViewport/Scissor), SceneInit 0x7e7950
  (salva il tipo scena), SceneInitRace 0x7e76b8, RLM sync 0x3cbd94, registrazione layer 0x667a94, director 0xc8b60.
- Prototipo viste: RLM ctor 0x3ca44c (crea i layer extra), RLM frame 0x3cc218 (griglia, camere CPU, misura),
  renderer "Main" 0x6a8f10 (viste 4->N in gara), RLM attach 0x3cce58 (collega layer extra), modelli 0x69bb0c
  (viste 4->N, sempre), priorità Layer3D 0x68d474 (allinea i layer extra ai Sub; non risolve il 2D coperto).
- Input (step 3): patch P1 0x7c54ac, P2 0x7c54b0, P3 0x7e4468, P4 0x7c5dc0/0x7c5dcc/0x7c5ddc; hook NpadInit 0x7c53ec
  (controller npad 4..7), Pim 0x7c5d88/0x7c58d0/0x7c5794/0x7c4e50 (tabelle 16), log ControllerMgr::calc 0x6192a4,
  diagnosi applet 0x8be248/0x49ea60.
- Menu (step 4): patch P5a 0x4d9468, P5b 0x4d9898; PadReadyHook 0x8be248 (solo log: il caso H4 si attiva solo senza pad extra); log conferma 2g-8g.
- Step 5 B (race8): patch B1 (mappa slot->entry +0x18c -> +0x19c: 0x868e30 0x868ee4 0x869684 0x8699ec 0x86b940 0x86b0b4 0x88236c
  0x882408; cmp #3->#7 a 0x86ec08 0x86901c 0x86b970), REPLACE 0x86af68 (SlotSet), 0x868f00 (compattazione), 0x86a594 (GamePlayer
  libero 0..7); trampolini ctor 0x8689b8, 0x869180 (log), PIM 0xca3e0 (G1/G3) + patch 0xca3fc/0xca410 (G2), 0x7c5340 0x7c6218
  0x7c62bc (G4), 0x7c5044 (G5), 0x7c60c0 (G6), G7 in PimReset1/2Hook, 0x7ce38c 0x7ce8c4 (G8 ordine+1), 0x86a998 0x86adec 0x86ad18
  (B4) + patch 0x86ac14; H-A2-0: 0x3fd5a4, 0x3fdd70.
- Step 5 C (padui): patch C1 ×12 (cmp #3->#7 famiglia pad UI), C2 ×4 (GetPad(4)->GetPad(8) tocco); trampolino 0x8bf040 (pad del
  tocco idx 8, 4 pad extra statici), REPLACE 0x8bf158 (4..7 -> extra con N>4; turno 2: 0..3 -> extra), trampolino 0x8bf058 (calc + fusione).
- Step 5 A1 (sel8, opzione MK8DX_LYT8=6|8, default 0): trampolini 0x4be854 (nome Page_Lyt), 0x4be8d0 (animazione lato), inline 0x4beb04 (w20).
- Step 5 S5.4 (sel8): trampolini 0x4b74a4 (onEnter charsel), 0x4ba77c (turni), 0x4badb0 (tipi locali 4..7), 0x4c0898 (onEnter kart),
  0x4c2820 (per frame kart), 0x8ba5fc (intercetta l'avanzamento kart al turno 1), 0x4203e0 (log personaggi).

## Hook attivi aggiunti nello step 6 (src/program/main.cpp)
- 6A (kart8): 15 patch (array K*[8] a M+0x3000, s32[8] a M+0x3040, new M 0x3080) + trampolini ctor 0xa8c00 e registrazione 0xac290.
- N1 (view8): SceneInitRaceHook 0x7e76b8 riscrive la Scene come 4P coerente con n>4 (`split` 6/8, `kartOf`, `extraVp`).
- N2/N3 (proto): viste extra solo con split; camera reale dei kart 4..7 (DescCam) con fallback; layer 2D Sub0..3 e 2D Sub4..7
  (slot 18..21) sulle celle con camera ×2 in 8P; copia bit 0x2/0x4 di layer+0xde4 da Sub0; extra disegnati nel job di Sub0 (0x683ecc).
- H1 (hud8): REPLACE 0x54798c/0x548d20 (rc_RaceView_6P/8P_Ml, Front_6P), trampolino 0x536a10 (partizioni, differite).
- H2 (hud8): trampolini 0x53186c (race+0x358 a 9), 0x406870 (pagine RaceView4..7 id 0x12/0x1d/0x43/0x5b su layer 18..21,
  oggetti viewport 5..8), 0x8b21dc (log apertura; apertura delle 4..7 da PerFrame), inline 0x547d5c, trampolini 0x86eb84/0x86eb1c.
- Diag: CharselProfileHook 0x4b7218, diag8, dump id pagina.

## Hook attivi aggiunti nello step 6b (src/program/main.cpp)
- E1 (order8, MK8DX_SLOT_REMAP): id dei job Draw 0x683ecc / SubDraw 0x684148 rimappati per rango (0..5, 14..17→6..9, 6..9→10..13,
  18..21→14..17, 10..13→18..21): 2D Main/Front/Overlay sopra a tutte le viste. MK8DX_DRAW_IN_SUB0 = 0.
- H6b (hud8): inline 0x86a720 (colore giocatore idx 4..7 → 0..3), inline 0x5350a8 (0x5500e4 sugli oggetti viewport extra),
  trampolini 0x534848/0x53467c/0x5345e0/0x5349a8 (cicli srotolati su race+0x358[0..4] estesi a 5..n-1), 0x53635c (allarme per pagina).
- Partizioni: `hud8::cmnPage` = pagina Race (onCreate 0x53198c, layout Cmn a +0x320), impostato in AddVpObjects; 0x536a10 in gara a
  8 non viene chiamata.
- Accensione extra per frame sincronizzata con 3D Sub0 (`+0x78` bit0 && !mgr+0xc72) e 2D Sub0: niente flash prima dell'intro.
- RelCam: durante l'intro (testa della pila del director vp0 = nodo tipo 2) le celle 5-8 copiano la camera di Sub0 trasportata dal
  kart di P1 al proprio kart, fovy di Sub0. KartCam usa kartOf[0].
- Lakitu: inline 0x10f198 (SetSlot non nasconde slot 4..7), trampolino 0x7e9b04 (SetSlot(4+j)), trampolino 0x7e9d40 (H6b-L3:
  array mgr+0x40 riallocato, 4 Lakitu extra con la fabbrica vt+0xc0), inline sui 5 `cmp w8,#1` 0x7e8bd0/0x7e8c8c/0x7e8df0/
  0x7e8eb0/0x7e92b8 (con split 8 preset camera del 2P: celle 320x360 con lo stesso aspetto del 2P).

## Hook aggiunti il 3 nov sera (src/program/main.cpp)
- Obiettivo 1: Model3DSlotHook 0x13d424 senza 0x148108(nullptr); Model3DReadyHook 0x14899c.
- Passo 7 B (namespace m3d8): KartDirInitHook 0x13d778 (sempre: log; con l'opzione 8 KartUnit) e, solo con MK8DX_P7_MENU3D8: KartWorkSizeHook
  0x13da28 (I), ResWaitHook 0x8843a8, ResReqHook 0x884bd8, KartResHook 0x89fc4c, MenuKartCfgHook 0x13e59c, MenuPresetHook 0x147738 (I),
  6 guardie viewer 0x430cc4/0x430d5c/0x430d7c/0x430d8c/0x430e24/0x430e58, ramo menu in proto::PerFrame (m3d8::MenuFrame), renderer
  "Main" a 8 viste anche con tipo scena 1, order8 attivo con m3d8::shown. Sonda m3d8::Probe nella selezione (frame 90/600, solo log).

## Problemi aperti
- **Lettura GPU non mappata: comportamento del gioco originale in Eden (chiuso 5-6 nov)**. `Unmapped Device ReadBlock` (4 KiB) una volta per
  sessione al caricamento di una scena di gara (prima partenza o 3a-4a gara), **anche senza mod** (0x36A6A000; con la mod 0x3CE65000/0x3CE05000,
  layout diverso). Log del driver (Service.NVDRV:Debug, step8_data (dati non pubblicati)/eden_log_nvdrv_lettura_20261105.txt): il gioco smappa/libera in blocco
  i buffer della scena precedente (49.9 s), Eden legge la pagina 2.7 s dopo -> cache del GPU emulato su memoria gia' liberata. Non e' un accesso
  CPU (il sanitizer non si ferma); non si corregge nella mod. Tabella prove: step8_data (dati non pubblicati)/ (log 1p_verde_mod, 1p_vanilla, 1p_texdesc0, 1p_normale_bis).
- **Dominante verde intermittente (aperto)**: vista UNA volta (5 nov 22:08, 1P, GP Coppa Fungo gara 3 = 4a della sessione, build normale, shader
  asincroni spenti) subito dopo la lettura GPU; verde su geometria illuminata, cielo e HUD opaco normali -> illuminazione/nebbia della pista
  con dati errati [I]. Non riprodotta in 5 sessioni successive (mod normale, TEXDESC=0, senza mod, 6 gare 1P + una gara a 8). Stessi errori di Eden
  in tutte le sessioni; anello descrittori della mod identico (0 fallimenti). Strumenti pronti: RenderDoc 1.46 (Eden avviato con
  `ENABLE_VULKAN_RENDERDOC_CAPTURE=1 scripts/run-eden.sh`, `renderdoc_hotkey=true`, cattura **Ctrl+F12** due volte, file in /tmp/RenderDoc) + sanitizer.
  Se ricompare: catturare, screenshot, NON uscire dalla gara. NB: `Service.NVDRV:Debug` riempie il log (limite 100 MB) in ~25 min.
- **Cicli srotolati a 4 pad nel menu: chiuso (6 nov)**. Censimento dei chiamanti di 0x8bd5f8 e delle scritture del bit p di +0x190: Dialog,
  Present, attivazione pagina gia' estesi; GhostScrollList 0x491a9c (solo 1P) e TitleSelect 0x4da82c (prima della scelta di N) senza effetto a 8;
  0x4b317c/0x4b956c/0x4e77d0 a pad singolo. Esteso 0x8b3b3c (scaricamento gruppo pagine): hook 0x8b3c0c azzera anche foc[4..7] (commit 1d54c53).
- **Risolto il 4 nov (P8)**: lo stub degli hook inline di exlaunch non salvava i flag NZCV; un hook inline fra un `cmp` e il suo `b.cond`
  deviava il salto con i flag del callback C. P8 (slot 7) nella selezione a 8: 0x4bbf24 `cmp w25,#0x34` / 0x4bbf28 hook cfg8 / 0x4bbf30
  `b.eq` -> sempre ramo Mii, nome e anteprima fermi (mai funzionato). Stub corretto in src/exlaunch_patch/inline_asm.s (mrs/msr nzcv a
  ctx+0xF8), copiato da scripts/build.sh. Verificato: nome di P8 aggiornato, senza hook di tracciamento. **Resta da fare un censimento
  statico** di tutti gli hook inline: callback che scrivono un registro poi confrontato dall'istruzione agganciata o dalle successive
  (regola G3: l'hook gira PRIMA dell'istruzione), e hook fra `cmp` e `b.cond` (ora sicuri per i flag, ma da elencare).
  **Censimento fatto (6 nov)**: il censimento degli hook inline (non pubblicato: tabella di istruzioni del gioco), 149 siti, nessun caso a rischio; i tre hook fra cmp e b.eq (0x4bbf28, 0x4bbfbc,
  0x4b9db4) dipendono dallo stub NZCV. Pool trampolini exlaunch 327 -> 655 (JitSize 0x20000) e limite `i >= HookMax` (src/exlaunch_patch/hook_impl.cpp);
  all'avvio `trampolini usati 288 / 655`.
- Il main.npdm rigenerato non è firmato: ok per Eden/Atmosphère, non per firmware stock (limite strutturale: la mod gira solo con CFW).
- **Pacchetto per Switch reale (6 nov)**: `scripts/package.sh` -> dist/mk8dx-8p-<commit>.zip (atmosphere/contents/0100152000022000/exefs/
  subsdk9 + main.npdm, LEGGIMI.md = docs/INSTALLA_SWITCH.md con 11 prove minime, SHA256SUMS). Nessuna cartella romfs: i layout 8pl sono gia' nella
  RomFS 4.0.1. Eden provato solo in dock e con layout di memoria 4 GB (come la Switch 1). **Prova su console non ancora fatta.**
- **Regressione 1-4P: fatta (6 nov, col sanitizer)**: gara 1P completa con risultati, 2P e 3P brevi, 4P con pausa/ripresa: 0 arresti, nessun
  errore di Eden (step8_data (dati non pubblicati)/eden_log_regressione_1234p_20261106.txt). GP a 4 completo verificato il 5 nov.
- [RISOLTO 4 nov] Logo di arrivo (FinishLogo) nelle celle 5-8: presente (sessione del 4 nov sera).
- [RISOLTO 4 nov, step6d §15: anello descrittori texture esaurito] Flash/macchie in gara a 8 (celle intere bianche con sagome ciano/magenta, sia celle extra sia originali: screenshot 22:40/22:41 del 3 nov,
  non in git): presenti con use_asynchronous_shaders=true anche con 0 letture non mappate; assenti in una gara a 8 con shader sincroni.
  In quella gara con flash Eden ha compilato solo 3 pipeline nuove (nessuna ripetuta): non e' una ricompilazione continua. L'utente
  osserva che il gioco originale non li ha mai e che compaiono "da qualche build" della mod: causa nostra da trovare (ipotesi: un
  passo per vista/post-processing che con 8 viste legge dati non scritti, visibile solo con gli shader asincroni). Da indagare con una gara
  a 8 con shader asincroni e MK8DX_VIEWS=4 / extra spente per bisezione.
- [RISOLTO step 7] Pausa e classifica a 8.
- [RISOLTO 5 nov: Eden sulla Iris Xe, vedi Step 2] Prestazioni sul PC attuale (3 nov): 1P 36-41 fps, 8P 8-12 fps. Collo di bottiglia = un solo thread di Eden (VulkanWorker 87-100%
  di un P-core); la Arc resta a ~800 MHz e forzata a 2050 MHz non cambia nulla. 25/25 campioni gdb del thread nel driver ANV
  (libvulkan_intel), 20/25 in un memcpy di libc chiamato dal driver; lato Eden la catena passa per BufferCache::InlineMemory
  (simboli della build Switch Sports, corrispondenza da confermare). Stesso limite con Eden v0.2.1 (Mesa 26.1.1 incluso) e con
  la build switch-sports-clean (Mesa 26.2.5 di sistema). Non è un costo della patch: già l'1P è limitato.

## Step 2 – prototipo viewport 6/8: COMPLETATO CON RISERVE (2 nov 2026)
Report: step2_prototipo.md. Commit 3c3e158 (8 viste) + commit finale step 2.
[VERIFICATO in Eden, gara 4P con split forzato]
- **[RISOLTO 5 nov] Prestazioni sul PC i7-13700H + Arc A370M**: con `vulkan_device=1` (**Iris Xe integrata**, memoria
  unificata) la gara a 8 va a **51-60 fps** contro 8-16 sulla Arc (stessa build e impostazioni). Il collo di bottiglia sulla Arc era
  il VulkanWorker nel memcpy del driver ANV (sopra): ipotesi [I] letture CPU da memoria device-local host-visible via PCIe
  (Resizable BAR attivo, BAR 4G), costo che con la GPU integrata non c'e'. Con la Iris il thread piu' carico e' quello GPU di Eden
  (96%). Su questo PC usare la Iris Xe (impostazione legata all'hardware, non al gioco).
- **Animazione di sblocco di un kart solo per P1-P4** (5 nov, 8P): entrando nella selezione kart (pagina 0x31, set A, N=8) e'
  stato sbloccato un veicolo nuovo; l'animazione "sbloccato" compare solo nei pannelli di P1-P4, P5-P8 hanno il veicolo nel pool
  ma senza animazione. Minore, da riprendere: step7f_sblocco_kart.md (screenshot e log).
- 8 viste (4x2) e 6 viste (3x2) con il mondo disegnato in tutte le celle; celle 5..N seguono i kart CPU 4..7
  con camera ricostruita dalla posa del kart. Nessun crash in gare da 30-90 s.
- Ingredienti indispensabili: layer 3D extra registrati (slot 14..17) + renderer "Main" creato a N viste
  (0x6a8f10) + layer collegati (0x68d4f4) + modelli g3d a N viste (0x69bb0c: senza, crash per corruzione shape).
- Prestazioni: 8 viste 58 frame/s contro 60 del 4P (host normale); altre misure falsate dal risparmio energetico.
  Memoria del processo invariata (heap preallocati); nessuna allocazione fallita.
Riserve / rotto (dettagli nel report):
- HUD fermo al layout 4P; nessun HUD per le viste 5..8; metà bassa della minimappa e 2D dei quadranti bassi
  coperti dalle viste extra (non è la priorità dei job: da indagare, step2_draworder.md + dump run11).
- Mappatura +0xcc -> rc_RaceView_6P_Ml / 8P_Ml non fatta (rimandata allo step HUD).
- Gara completa 8 viste OK (run12, 56-60 frame/s) e transizioni di scena con distruzione/ricreazione del
  renderer OK (run13). Regressione 1P non eseguita (data per buona dall'utente); 6 viste solo gara parziale.

## Step 3 – input a 8 controller: COMPLETATO (2 nov 2026)
Report: step3_input.md (piano statico: step3_input_plan.md).
[VERIFICATO in Eden] 8 pad interrogati, 10 controller in lista (npad 4..7 in posizione 6..9, handheld in 4),
tasti 1..8 -> X solo sul rispettivo npad 0..7 sia a livello hid sia a livello controller di gioco; gara 1P e
gara 4P (applet min=max=4) come prima.
Non fatto (serve con il menu, step 4): giocatori di input 4..7 (PIM+0x50 fisso a 4, G1-G8 del piano), applet
per 5-8 (0x8be248 `cmp #3`), raceMgr+0x17c a 8.
Config Eden di test: giocatori 1..8 connessi, X = tasti 1..8; backup in eden/user/config/qt-config.ini.bak-step3.

## Step 4 – flusso menu 5-8: COMPLETATO (2 nov 2026)
Report: step4_menu.md (piano: step4_menu_plan.md).
[VERIFICATO in Eden] 8g/5g -> niente dialogo, cfg+0x1fa4 = N, modo 3, applet 0x49ea60(N,N) chiamata una sola volta,
pad UI pronti, si arriva alla selezione personaggi (4 posti). Regressione 4P OK.
Aperti (step 5): raceMgr+0x178/+0x17c restano a 4; pad UI 4..7; giocatori di input PIM 4..7; selezione 6/8 posti.

## Step 5 – selezione personaggi per 8 locali: COMPLETATO CON RISERVA (2 nov 2026)
Report: step5_charsel.md (piano: step5_plan.md + step5_plan_A1/A2/B/C.md; blocco: blocco_step5.md).
[VERIFICATO in Eden] 8g -> raceMgr +0x178=8, +0x17c={0..7}, mappa slot {0..7}; PIM 8 giocatori, locali 4..7 rilegati ai
controller npad 4..7 dopo l'applet; pad UI 4..7 veri (tasti 5-8 distinti); selezione personaggi e kart a due turni (turno 1
giocatori 1-4, turno 2 giocatori 5-8 sugli stessi 4 pannelli) -> 8 entry con personaggio/kart distinti, settingsB +0x180=8;
init della scena di gara a 8 viste, poi crash (PC=0) in gara -> step 6. Regressione 4g nel menu OK; gara 4P non rifatta.
Riserva: l'interfaccia ufficiale Switch 2 ha 8 pannelli e 8 cursori simultanei (layout 8pl, screenshot in step5_data/ref_*):
richiede l'estensione del framework UI (blocco_step5.md). Layout 8pl caricabile con MK8DX_LYT8=8 ma solo cosmetico.

## Step 6 – gara con 8 locali: COMPLETATO A VIDEO (2-3 nov 2026, step 6b)
Report: step6_gara.md (piani: step6_plan_A/B/C.md). Scelta dell'utente: step 6 prima dello step 5b (8 cursori).
[VERIFICATO in Eden] 8g → gara a 8 viste a 60 frame/s, 8 kart controllati dai rispettivi controller, HUD 1-4 con layout 8P,
risultati e gara successiva; pausa/risultati non più coperti. Aperti: vedi Known issues del report.
Step 6b [VERIFICATO in Eden, screenshot dell'utente]: mappa e 3-2-1 sopra le celle 5-8, HUD corretto in tutte le 8 celle (colori,
posizione), linee divisorie 4x2, nessun flash prima dell'intro, camera d'intro che gira nelle celle 5-8, Lakitu con semaforo intero
in tutte le 8 celle (8 Lakitu). Da fare: regressione 1P/4P completa, logo d'arrivo 5-8.

## Step 5b – selezione a 8 come Switch 2: IN CORSO (3 nov 2026)
Piani: step5b_plan.md (catalogo aggiornato + passo 1), step5b_plan_p2.md, _p3.md, _p4.md; log in step5b_data (dati non pubblicati)/eden_log_5b*.txt.
[VERIFICATO in Eden]
- Passo 1 (A1-M2): stato per slot della classe selezione +0x904[4] -> +0xa28[8] (75 parole, canary sulla vecchia area mai toccato).
- Passo 2: finestre varianti CharaList per gli slot 4..7 su id 01/02/5e/64 (rimappate per LR in 0x8bf0ec), link +0x660 -> +0xa68[8],
  code +0x928 -> +0xba8[8] (oggetto 0xce8); prova varianti in 1P OK.
- Passo 3: selettore cursori a 8 slot "virtuali" (V = s+4, cursore 0xb8 -> 0x1a8, 20 clamp + hook PRE, focus/memoria in ombra,
  cm_L_BtnCursor_04..07 colori frame 7..10 = rosso/blu/viola/arancio, confermati dall'utente). Il reset del cursore largo NON va in
  SceneInit (tipo 2 = scena anteprime, chiamata dopo l'ingresso in selezione) ma in 0x40476c e SceneInitRace.
- Passo 4: selezione Multi a turno unico con N>4: +0x8c0 = 8, 50 clamp a #8, ombra cfg8 (RAM) per i 13 int[4] della config,
  pagina nomi CharaParts 8pl sull'id 0x12 (sel+0xce8 per le 19 letture L_Name), guardia 0x13d424; 8 personaggi distinti in un turno
  e gara a 8. Kart ancora a due turni. `MK8DX_SEL1ROUND=0` torna ai due turni; `MK8DX_P3TEST=1` = prova del passo 3.
- Passo 5: kart a 8 slot a turno unico: secondo set di controlli della pagina kart 0x31 costruito sulla pagina nomi 0x12 (p5::BuildB,
  scambio in KartEnterHook), +0x460 -> +0x4a0, 34 clamp, cornici KPSlotCursor 04..07; correzioni: animazione "Change" sulla 0x12,
  clamp del selettore parti 0x476628/0x477400/0x477e38 (+cfg8), coda texture tc_KP 64 -> 128 e heap UIHeap_PartsTex 0x6d0000 ->
  0xda0000 (icone kart), G3 0x443590 `cmp #0xc` (gli hook inline girano PRIMA dell'istruzione: se il callback scrive il registro
  confrontato il cmp va allargato). Kart a 8 completa e corretta.
- Passo 6: cfg8::sh + aid8::code salvati in sd:/mk8dx-8p/p58.bin (0x100 B, 'M8P8', FNV-1a) con nn::fs dell'sdk (MountSdCardForDebug,
  ScopedAutoAbortDisabler), npdm SdCard bit 21; caricamento al primo evento menu, salvataggio in AidsSaveHook. Verificato dopo riavvio.
- Passo 7 A: finestre varianti in scala 8P (G_Ptn frame 3 della RomFS 4.0.1), riquadro dei costumi di P5-P8 (CwCellGfxHook
  0x4553fc/0x455474, attivazione dello slot 0 "come largo"). Ritorno dalla gara: la gara riusa l'id 0x12 (HUD di P5) -> all'ingresso in
  selezione si rimettono voce di tabella e mgr+0x30[0x12] (0x8bd0c8) della pagina nomi.
- Passo 8: palette a 8 in gara (MK8DX_PAL8=1): H8-A 0x86a770 (locali 4..7 -> codice HUD 7..10, frame P5-P8 gia' nella RomFS 4.0.1),
  H8-B 0x420df4 (righe/minimappa 10..13), H8-C 0x513ca8 (Rank tc_RC_5P..8P), H8-D 0x520830 (classifica in pausa), H8-E 0x5057a8 (classifica
  finale); P5 rosso, P6 blu, P7 viola, P8 arancio come Switch 2. Gara e pausa a 8 senza errori; classifica finale da provare sul PC veloce.
- Letture nulle (3 nov sera, obiettivo 1): con il debugger di Eden attivo ogni accesso non mappato ferma il thread e GDB ne da' PC e
  catena (`tools/debug/aloop.sh`). Trovate e corrette: `@0xc0` nella selezione/kart a 8 = la nostra guardia Model3DSlotHook chiamava
  0x148108(nullptr) (che legge [x0+0xc0] e agiva sul piedistallo di P1) -> ora ritorna; guardia 0x14899c (modello nullo = "non pronto",
  chiamanti 0x4b2ffc/0x4b5484). Le due `Read64 @0x10` e la `Device ReadBlock` in gara NON si sono ripresentate (0 fermate del debugger in
  tutta l'intro, 0 letture in una gara a 8 senza debugger): non sono la causa dei flash. Log: step5b_data (dati non pubblicati)/eden_log_o1_*.txt, gdb_o1_abort.txt.
- Passo 7 B (anteprime 3D a 8) IN CORSO: piano step7b_plan.md (analisi statica, 216 OK) + §7 esito in Eden. Opzione di build
  `MK8DX_P7_MENU3D8=1` (default 0): 8 KartUnit nel menu (descrittori 4..7 copiati con tipo 0, richiesta risorse limitata a 0..3, attesa e
  risorse dagli slot 0..3, KartWork 0x30c8000), renderer del menu a 8 viste, preset 8pl, celle 640x180, camere extra, maschere 4..7,
  ordine E1 nel menu. Verificato a video: P1-P5 con l'anteprima sopra la propria targhetta come Switch 2. Aperti: risorse dei personaggi di
  P5-P8 al cambio (slot del caricatore 4..7 occupati dal menu -> kart senza pilota e letture nulle per frame), "fantasmi" a x=±160,
  pagina kart/finestre varianti (tappa 4). Con l'opzione a 0 la mod e' quella di prima (verificato: 4 KartUnit, 0 letture non mappate).
  Aggiornamento 4 nov 00:40 (step7b_plan.md §7.1-7.2): risorse di P5-P8 risolte (caricatori 4..7 preparati all'ingresso nella selezione a 8),
  celle 640x160 centrate, fantasmi risolti (layer+0xde4 bit 0x4). Aperti: monete nelle celle P6-P8, P8 che non cambia nome/anteprima
  (regressione anche con l'opzione spenta, da bisecare da cba7303), letture nulle intermittenti 0x150c7c.
Aperti: regressione completa 1-4P (selezione/kart verificate a tratti), flash in gara (vedi "Problemi aperti"). Il layout 8pl del menu
coincide al pixel col riferimento Switch 2 (step5b_plan_p7.md §5.1).

## Step 7 – pausa e risultati a 8: COMPLETATO PER QUANTO VERIFICABILE (3 nov 2026)
Piani: step7_plan.md (pausa, risultati, helper debug), step7_plan_cursor.md (cursore/colore della pausa).
Esito e riferimenti Switch 2: step7_gara8.md (gara a 8 portata a fine gara dall'utente: logo FINE, classifica 12 righe, menu
fine gara OK). Aggiunti dopo il confronto: HUD per colonna (0x50efa8), minimappa ×0.65 (0x533368), aiuti P5-P8 senza scritture fuori
limiti (0x3fe040/0x3fdd70 limitate a 4, ombra aid8 in RAM), pool exlaunch JitSize/InlinePoolSize 0x8000 (~81 trampolini superati).
[VERIFICATO in Eden] Pausa a 8: layout rc_Page_RacePause_SgOunce (REPLACE 0x53c818), 8 pannelli L_SwitchBtns8pl 4x2 (cmp 0x53d68c
4->8, nomi a 0x53d3a8/0x53d678, frame 3 di G_SingleMultiChange_00 da 0x51bc10), pausa apribile da tutti (scanner 0x8b4038 esteso ai
pad 5..7), cursore e conferma per il pauser 5-8 (INLINE 0x53e404 + TRAMPOLINE 0x8b0084: slot 0 condiviso; cursore grafico 0
ricolorato frame 7..10, ripristino in PageLinkHook), aiuti L/Y/R dei giocatori 5-8 (maschera +0x58 dei controlli L_SwitchBtn,
vtable 0x1285598, giocatore a obj+0xc8 = ctl-0x30+0xc8, da 0x8000000f ristretta solo per i<4).
Sfondo della pausa: corretto in tutte le 8 celle (screenshot 14:38); una sola volta, in una pausa subito dopo il via, le celle 6-8
mostravano un'immagine vecchia non scurita: non riprodotto. Risultati/classifica a 8 verificati
con una gara completa; restano C1 colori righe P5-P8 (palette a 8, step 5b), C2 parità punti (step7_plan.md §2) e Gran
Premio completo: rimandati al PC veloce con la regressione 1P/4P. L'helper "gara a 1 giro" (D1/D2) è stato provato e rimosso su richiesta dell'utente.

## Prossima azione
**ATTENZIONE (6 nov 2026): `scripts/build.sh` senza variabili produce ora la build DISTRIBUIBILE (log spenti). Per Eden usare
`MK8DX_LOG=1 scripts/build.sh`**, altrimenti mancano tutte le righe `[mk8dx-8p]` usate nei controlli (patch 7c/7d, trampolini...).
Prova su Switch reale (6 nov 2026, report in step8_data (dati non pubblicati)/console/LEGGIMI.md): crash all'avvio risolto (ModelCreateHook chiamava 0x68fa38
su modelli con shape non ancora collegate), compatibilita' SaltyNX corretta (svcGetInfo), diagnostica crash nei report di Atmosphère
(hook nnSdk+0x90f38: X03 pc, X04 far, X05 esr, X06 lr, X07 sp; il kernel azzera x8+ nei break). Avvio e gare ok sulla console;
l'utente ha 6 Joy-Con: GP a 6 e 5 in corso, 7-8P sulla console in futuro con amici. Pacchetti: `scripts/package.sh` ->
dist/mk8dx-8p-distribuibile-<commit> (MK8DX_LOG=0) e dist/mk8dx-8p-diagnostica-<commit> (MK8DX_LOG=1), entrambi con MK8DX_EXCDIAG=1.
Finestra varianti P5-P8 (6 nov): il riquadro restava dopo la scelta del costume -> CwDecideHook 0x8affa4 (step8_varianti_chiusura.md),
verificato in Eden. Console: GP a 5 e 6 ok. Difetto grafico intermittente rivisto in 8P (viola, step8_data (dati non pubblicati)/viola_8p_*.png): anello
descrittori sano (0 falliti, col debugger), sparisce alla gara successiva. Cattura: F12 di RenderDoc (un fotogramma); il tasto avvia/ferma
di Eden (renderdoc_hotkey) ha bloccato il PC.
Prossimi passi, da concordare con l'utente:
1. GP a 5 e 6 sulla console fatti; poi prove a 7-8 con amici (build diagnostica), poi uso della distribuibile.
2. Regressione in Eden della build distribuibile (MK8DX_LOG=0) col sanitizer: verificato solo l'avvio.
3. Verde intermittente (Problemi aperti): RenderDoc pronto (`ENABLE_VULKAN_RENDERDOC_CAPTURE=1`, `renderdoc_hotkey=true`, Ctrl+F12).
**Test "sanitizer"** consigliato dopo ogni modifica di rilievo: Eden con `use_gdbstub=true` + `tools/debug/start.sh` +
`tools/debug/recloop.sh` (con lo stub Eden si ferma a ogni lettura non mappata; recloop registra pc/lr/registri/bt in rec.log e
riprende). Lo stub accetta UNA sola connessione: se gdb muore, Eden va riavviato. Mai `pkill -f`/`pgrep -f` con nomi di script (anche nei
cicli di attesa: `pgrep -f recloop.sh` trova la propria shell). Con lo stub attivo non aprire le impostazioni di Eden (si chiude).
Breakpoint: non sui prologhi con `str ...,[sp,#-N]!` (lo stub rieseguiva l'istruzione: salto nello heap); dopo un'interruzione manuale
`stepi` prima di `continue`, e un breakpoint puo' non scattare piu' (riavviare Eden per riarmare una forzatura).
Impostazioni di Eden: predefinite (shader asincroni SPENTI) tranne `vulkan_device=1` (Iris Xe) sul PC i7/Arc. Config dei controlli: tutti gli
8 giocatori con gli stessi tasti (WASD/frecce, A = C, B = X) tranne lo **stick giu'**: P1-P2 = S, P3..P8 = tasti 3..8 (dal 3 nov sera; serve a
muovere un giocatore alla volta). Screenshot: Ctrl+P in Eden -> switch_env/eden/user/screenshots/.

## Sessione del 5 nov 2026 sera (PC i7-13700H + Arc, Iris Xe) [VERIFICATO in Eden]
- **Step 7f** (step7f_sblocco_kart.md): l'animazione di sblocco e' la pagina Present (0x26 personaggio, 0x27 parti). La RomFS 4.0.1 ha
  8 parti Gift in mn_Page_Present_Parts (3.0.5: 4) e il main Switch 1 ne creava 4 nascondendo 4..7. `gift8`: array per regalo [4] -> [8] in
  coda all'oggetto (0x368 -> 0x5f0), 98 parole (generate da step7f_data/gen7f.py) + 4 hook; tolto PartsPresentFixHook (toppa). Opzione
  `MK8DX_GIFT8` (1; 0 = originale per bisezioni).
- **Pagina Dialog a 8 pad**: la Dialog (id 7, sempre in lista) all'apertura/chiusura (0x49be54/0x49c118) gestiva bit di +0x190 e focus solo
  per i pad 0..3: chiusa possedeva i pad 4..7 e dopo il regalo P5-P8 perdevano sinistra/destra/A/B. DialogOpen8Hook 0x49bf0c /
  DialogClose8Hook 0x49c1ac estendono ai pad 4..7.
- **Logging**: stallo al titolo col debugger per inversione di priorita' nello spinlock (svcSleepThread(0) cede solo a pari priorita'): ora
  l'attesa e' un sonno di 100 us (loggers.hpp). Possibile anche su Switch reale.
- Verifiche con sblocco forzato col debugger (salvataggio copiato e rimesso): kart a 0x4c2610 (`+0x324 = 0`); personaggio alla fonte, byte
  di stato nel salvataggio ([...]+0x1f48, 53 voci, 2 = da mostrare) a 0x866d98. 8 scatole/lampi in 8P, 4P invariato, comandi P5-P8 ok, regalo
  personaggio ok in 8P e 4P. Ordine regalo -> applet dei controller = gioco originale (provato senza mod).
- **Premiazione a 8** (step7d_data (dati non pubblicati)/gp8_*.png, eden_log_gp8_premiazione1/2_20261105.txt): MK8 Deluxe non ha una cerimonia sul podio:
  dopo l'ultima gara riassunto MKTV (replay + piazzamenti) e "Congratulazioni!" con classifica a 12 righe e coppa (bronzo con umano 3°,
  argento con umano 2°). Due GP a 8 completi, 0 arresti del sanitizer, ritorno al menu. Una sola "Unmapped Device ReadBlock" (lettura GPU,
  0x3CE65000, 4 KiB) all'inizio della quarta gara del primo GP: non un accesso CPU, non ripetuta nel secondo GP (Problemi aperti).

## Sessione del 5 nov 2026 (PC i7-13700H + Arc, Eden sulla Iris Xe) [VERIFICATO in Eden]
Con lo stub GDB attivo Eden si ferma a ogni lettura non mappata ("Stopping execution due to unmapped memory access"): due bug
latenti della mod, innocui in Eden (che restituisce 0) ma crash su Switch reale, trovati e corretti:
- **Logging non thread-safe**: lo snprintf di newlib (stato globale `_impure_ptr`) con `%f` da piu' thread corrompeva la freelist dei
  bigint (`_dtoa_r` -> `_Balloc` legge 0x300000). Ora `Logging` (src/program/loggers.hpp) e' un wrapper con spinlock + svcSleepThread(0).
- **Tabella delle maniglie dei layer 2D** (step7e_tabella10.md): 0x551914/0x526764 cercano il layer 2D della cella con
  0x8bf11c({giocatore+1}) in una tabella a costanti (0 Back, 1..4 Sub0..3, 5 Main, 6 Front, 7..9 NULL): P5/P6 prendevano Main/Front,
  P7/P8 NULL -> `Unmapped Read64 @ 0x10` a ogni partenza a 7-8. Hook inline 0x551a0c e 0x526830 (dopo la ricerca): per i giocatori
  4..7 la maniglia del layer 2D Sub4..7 (slot 18..21), costruita come all'avvio (0x8bfa18 + 0x8bfa78) in memoria statica
  (0x5fa960 restituisce NULL alla partenza: heap corrente nullo). Verificato: 0 letture non mappate in una gara a 8, 48-54 fps.
- Crash a fine GP: watchpoint su entry8+0x21c -> scritto da **main+0x86a244** (8 -> 10 = personaggio di P1) nella transizione verso
  la scena 5: le copie dei locali sovrascrivono le entry 8+ (conferma dell'ipotesi di step7d). Analisi della formula in
  step7d_copie.md. Log: step7d_data (dati non pubblicati)/eden_log_gp8_watch_20261105.txt.
- **Crash a fine GP RISOLTO (opzione A, flusso originale)**: 0x869fd4 carica la registrazione (12 kart) nelle entry N..N+11 dopo
  aver nascosto gli umani; il raceMgr aveva 16 entry = 4 locali + 12 kart. Ora 20 entry (8 + 12): RaceMgrCtorHook sposta gli array
  +0x58/+0xf0 su buffer da 20, 34 parole 16 -> 20 (step7d_censimento20.md "PATCH DEFINITIVE", tutte o nessuna, `race8::cap20`),
  cicli della mod su rm+0x50. Verificato: GP a 8 completo con debugger, 0 arresti, KartDirInit della premiazione +0x180 = 12 e
  n = 12 (prima 8/8 e crash), premiazione e ritorno al menu. Log: step7d_data (dati non pubblicati)/eden_log_gp8_20entry_ok.txt.
- Regressione GP a 4 completo fino alla schermata finale (KartDirector 12, nessun errore). Durante il passaggio 8P -> 4P il
  debugger ha preso la lettura su entry nulla (+0x21c/+0x220) gia' vista: 0x4c0490 (ciclo su pagina kart +0x39c) chiamato da
  0x4bb650 prima dell'onEnter della pagina kart, che aveva ancora il set B (8 slot) dell'8P. Corretto: PageEnterHook della selezione
  riporta la pagina kart al set A quando N <= 4. Verificato 8P -> 4P -> 1P: 0 arresti, 0 letture non mappate.

## Sessione del 4 nov 2026 sera (PC Ryzen Z2 Extreme, Eden v0.2.1 senza SIGILL) [VERIFICATO in Eden]
- Step 7c (6d11dc6, bc45dca): cella vuota 5P/7P come la quarta cella del 3P: quinto controller di camera in modo spettatore (P1-P7 di
  step7c_spettatore.md + correzioni: camera dalla matrice di vista obj+0x68, scelta automatica del kart 0x86ca88, sfera della
  camera 0xcdd18 e flag effetti 0x1088c8 per la cella vuota), nessun HUD/Lakitu nella cella vuota, Lakitu nelle proprie celle;
  bordi delle colonne 3x2 interi (426/854): tremolio 5P/6P risolto.
- Confronto Switch 2 gara a 8 (d33a8f0, step7_gara8.md in fondo): riquadro oggetti speculare nelle colonne 3-4 (N_All_01/02
  per colonna, 0x548024/0x510a68/0x510e38). Classifica finale a 8 uguale al riferimento (12 righe, punti 15..1, colori P5-P8); logo
  d'arrivo nelle celle 5-8 presente; due gare a 8 complete e un GP a 8 (4 gare) senza errori fino al crash della scena finale.
- Regressione con gare brevi: 1P, 3P, 4P, 5P, 6P, 7P, 8P senza errori.

## Selezione a 8 con layout 8pl diretto (step 5c, 4 nov sera) [VERIFICATO in Eden]
Opzione B di step5c_8pl_plan.md (commit a788af0, attiva di default da 1b24778, `MK8DX_SEL8B=0` la spegne): con 5-8 giocatori
la selezione personaggi usa direttamente la pagina 8pl 0x12 con un secondo set di controlli (PageEnterHook 0x8bb3e8, BuildB in
CharaPartsCreateHook) e le anteprime 3D si accendono con la pagina aperta: niente piu' layout a 4 per ~1 s all'ingresso, nella kart e
con B. Verificato dall'utente: selezione personaggi e kart a 8 "perfette", 4P identico. Logger viewport/scissor corretto (a tabella
piena loggava ogni chiamata: gara a 4 fps).

## Step 7 B – 4 nov (pomeriggio): liste 12 -> 16, anello 5 -> 9, 4P, monete [VERIFICATO in Eden]
Build `MK8DX_P7_MENU3D8=1` (REC16 e RING9 attivi di default con le anteprime; `MK8DX_P7_REC16=0`, `MK8DX_P7_RING9=0`, `MK8DX_P7_FLOW8=0`
per bisezioni; le misure delle monete COINTEST sono state rimosse il 4 nov).
- S0 (4e3e46b): misure; 5 liste a 12/12 bloccati sotto stress a 8 con somma ref 15 -> -1 -> guardia m_Body -> thread fermo (causa confermata).
- S1-S3 (1373c2b): record 12..15 in coda a sys (0x9a50 -> 0xb4f0), heap creati come l'avvio (TaskHeap d'avvio 0x19500000, BackgroundLoad
  0x17500000, DriverBodySE 0x350e000), 8 ricerche e 6 precarichi a N = 16, 8 accessi per indice, 4 rilasci per chiave, flag "usato",
  anello nei record estesi. Stress a 8: 0 ricerche -1 (Arm a parte: acquire fallito, anche nell'originale), 0 guardie, thread mai fermo.
  Piano: step7b_records_plan.md (525 OK) §7-9.
- 4P bloccato dopo la scelta dei personaggi (regressione di ada67de) risolto (7f8d5cb): ingresso in selezione nasconde le unita'
  [N, 8) (0x4b865c `cmp #7`, 0x4b8674 `cmp #8`), tolta 0x4fa254 dal flusso a 8 (6 parole). 4P/8P/4P/3P fino ai kart. step7b_4p_plan.md.
- S4 (98e6f90): anello Body/Wing a 9 voci (voci 5..8 in coda a sys, parte 0 a N = 9; Wing con percorso x2+0x138 e caricatore 0x89bc70),
  CourseResource 0x10004000. Wing: 0 ripieghi dopo la correzione.
- Monete in P6-P8 (125c637): modello globale creato all'avvio (0x2744b8 -> 0x7f8aac) con maschera di disegno 0xffff disegnato nelle viste
  extra del menu; DrawObjHook 0x6e2430 esclude le viste 4..7 per gli oggetti a maschera piena solo nel menu a 8. Trovato con misure
  (drawreg/coinreg) e bisezione per cella. step7b_coin.md §9.
- Aperti: flash in gara a 8 (sopra); gara a 4 dopo queste modifiche non riprovata.

## 6 nov 2026 pomeriggio: repo pubblico
`mk8dx-8p/` e' diventato il repo PUBBLICO (https://github.com/LorenzoMorelli/mk8dx-8p.git, ramo `main`, storia nuova dal commit 8242fcb,
inglese, GPL-2.0, CI `.github/workflows/build.yml` con build release in devkitpro/devkita64 e `DKP_NATIVE=1`). La storia precedente e' nel
ramo locale `archivio-privato` (mai da pubblicare; push limitato a main con `remote.origin.push`). Queste note (STATE, prompt, report,
memoria, log) vivono qui in `` (repo locale privato). Pacchetti: `scripts/package.sh` -> dist/mk8dx-8p-{release,diagnostic}-<commit>.
