# Step 7f – animazione di sblocco di un pezzo del veicolo solo per P1-P4 nella selezione kart a 8 (aperto, minore)

Stato: **risolto (5 nov 2026 sera)**: 8P e 4P con sblocco del kart forzato dal debugger, comandi di P5-P8 dopo l'apertura corretti (Dialog a 8 pad). Regalo personaggio verificato con la forzatura alla fonte (byte di stato nel salvataggio). Segnalato dall'utente il 5 nov 2026, due volte (8P, pagina kart 0x31 con il set B a 8 slot).

## Osservato
- Entrando nella selezione kart a 8 con un pezzo appena sbloccato, il gioco mostra l'animazione di sblocco (scatole regalo che si
  aprono con un lampo di luce sul pezzo nuovo) **solo nei pannelli di P1-P4** (prime due righe, colonne sinistra/destra).
- P5-P8 hanno il pezzo nuovo nel proprio elenco (ruota con il segno "!" di novita') ma **nessuna scatola ne' lampo**; durante
  l'animazione i loro pannelli restano oscurati/vuoti.

| file | momento |
|---|---|
| `step7f_data/sblocco8_1_scatole.png` | 15:56:27: scatole regalo solo su P1-P4 (righe 1-2); righe 3-4 (P5-P8) senza scatole |
| `step7f_data/sblocco8_2_apertura.png` | 15:56:31: lampo sul pezzo sbloccato solo su P1-P4; P5-P8 con la ruota "!" ma senza effetto |

Log della sessione: `step7f_data/eden_log_sblocco8_20261105.txt` (8P -> 4P -> 1P, build con raceMgr a 20 entry e pagina kart al
set A per N <= 4). Primo avvistamento: `step7e_data/eden_log_8p_irisxe_sblocco_kart.txt` (t = 1482.9 s, ingresso pagina 0x31).

## Ipotesi di partenza [I]
L'animazione di sblocco e' un elemento per pannello (4 pannelli nell'originale): il set B della pagina kart (p5::BuildB, 8 slot su
nomi 0x12) e' costruito con i controlli 8pl ma l'animazione di sblocco viene probabilmente avviata da un ciclo a 4 slot (o su
oggetti/animazioni del set A). Da cercare: chi avvia l'animazione di sblocco nella pagina 0x31 (onEnter 0x4c0898 e seguenti), su
quanti slot, e se i pannelli 8pl della RomFS (`mn_Page_Lyt_CharaParts_8pl`) hanno le stesse animazioni/parti per i pannelli 5-8.
Confronto atteso con Switch 2: nessun riferimento disponibile per questa schermata.

## Analisi statica (5 nov 2026 sera) [VERIFICATO sul disassemblato e sulla RomFS]
- L'animazione e' la pagina **Present** (enum `PartsPresent`, id 0x27; `CharaPresent` 0x26), layout `mn_Page_Present_Parts`, classe
  0x4b0954..0x4b27c0 (vtable GOT 0x13092f8; onCreate 0x4b09dc, onEnter 0x4b0e20, per frame 0x4b1654/0x4b18cc, 0x4b21d4, 0x4b2414),
  oggetto da 0x368 byte (fabbriche 0x409e38 personaggio, +0x208 = 1; 0x40a174 parti, +0x208 = 0).
- **RomFS**: `mn_Page_Present_Parts` 3.0.5 ha 4 parti `mn_L_KartParts_Gift_00_0..3`; la **4.0.1 ne ha 8** (`_0.._7`, tutte a (0,0): le posiziona
  il codice sul riquadro del pannello). Altro asset per 8 giocatori presente ma non usato dal main Switch 1.
- **Codice 4.0.1**: onCreate cicla su 8 nomi (0x4b0b80 `orr #8`) ma crea il controllo solo per i < 4 (0x4b0ba8 `cmp #4; b.lo`) e **nasconde**
  le parti 4..7 (0x4b0bb4..0x4b0bcc). Nel 3.0.5 (0x4ad810) il ciclo era a 4: l'estensione a 8 e' stata fatta a meta' nel build Switch 1.
- Stato per regalo, tutto a [4] con indice limitato da `cmp #4; csel lo`: flag +0x20a[4] (da stato per slot della selezione +0x904 ->
  +0xa28 della mod, srotolato 0x4b111c..0x4b11d8), controlli Gift +0x218[4], riquadri del pannello +0x238[4], effetti "Unlock"/"UnlockPart"
  (HandleELink) in 4 coppie a passo 0x10: +0x268/+0x270, +0x2a8/+0x2b0, +0x2e8/+0x2f0, +0x328/+0x330 (fine 0x368). Conteggi per le parti `orr #4`
  (csinc: personaggio = 1) in 0x4b0a84, 0x4b11e4, 0x4b1718, 0x4b19b0, 0x4b2288, 0x4b2448.
- Cicli srotolati a 4: focus dei pad 0..3 sulla pagina (0x4b13c8..0x4b1408, 0x8bd5f8), 0x14b824(i) (KartUnit del menu i) per i flag (0x4b1cc0..0x4b1cfc).
- **Lato kart** (0x4c2584..0x4c275c): se il pezzo 0 della colonna (carrozzeria +0x3d0, ruote +0x3e0, deltaplano +0x3f0) ha +0x324 != -1 apre
  la Present e copia i riquadri dei +0x39c pannelli in Present+0x238 con `cmp #4` (0x4c2658, 0x4c26cc, 0x4c2748): col set B (8 pannelli) gli
  indici 4..7 finivano sullo slot 0; il vecchio `PartsPresentFixHook` (0x4c2768) riscriveva 0..3 coi pannelli di P1-P4 -> scatole solo su P1-P4.
- Scrittore esterno anche dal regalo personaggio: 0x4b8a00 (`str [x8,#0x238]`, indice 0).

## Patch (flusso originale con N = 8; `gift8` in src/program/main.cpp, tutte o nessuna)
- 98 parole generate e verificate da `step7f_data/gen7f.py` (esclusi 4 caricamenti GOT `[xN,#0x2e8]`): dimensione 0x368 -> 0x5f0 (2), array in coda
  all'oggetto: flag +0x368[8], controlli +0x370[8], riquadri +0x3b0[8], effetti +0x3f0/+0x470/+0x4f0/+0x570 (passo 0x10, [8]); 6 conteggi 4 -> 8;
  12 limiti `cmp #4` -> #8 (9 nella classe, 3 nel kart); 0x4b8a00 e i 3 `str +0x238` del kart sul nuovo offset.
- Hook: 0x4b0e20 (trampolino onEnter: flag 4..7 a 0 prima dell'originale, serve nel ramo 0x3fe658 che scrive solo 0..3); 0x4b11b0 (inline,
  x8 = selezione: flag 4..7 con la stessa regola `stato != 0,1,4` sugli slot +0xa28+8i, solo per i < N per non leggere stati vecchi di un 8P);
  0x4b140c (focus 0x8bd5f8 anche per i pad 4..N-1, instradati dall'ombra cur8); 0x4b1d00 (0x14b824(i) per i = 4..7 con flag).
- Tolto `PartsPresentFixHook` 0x4c2768 (toppa sull'effetto). Con N <= 4 i controlli 4..7 sono creati e nascosti dall'onEnter (flag 0), come
  facevano le parti nascoste.
- Non cambiato: scala del regalo per N > 2 = 0.7 (0xed5990, come il 4P): manca un riferimento Switch 2 per questa schermata.

## Verifica
- [VERIFICATO in Eden] avvio: `7f: pagina Present a 8 regali (98 parole, oggetto 0x5f0, 4 hook)`, pagina creata al caricamento del menu.
- [VERIFICATO in Eden, 8P, sanitizer GDB] sblocco forzato: breakpoint a main+0x4c2610 (x9 = pezzo 0 della colonna carrozzeria) che scrive
  `+0x324 = 0` (Kart standard, gia' posseduto) alla prima fermata; salvataggio copiato prima (switch_env/save_backup_7f_20261105, non in git) e
  rimesso dopo. Risultato: **8 scatole sugli 8 pannelli** (`step7f_data/sblocco8_fix_1_scatole.png`) e **8 lampi** all'apertura con P5-P8 non
  oscurati e kart 3D visibili (`sblocco8_fix_2_apertura.png`); 1 sola fermata del sanitizer (il breakpoint), 0 letture non mappate del gioco
  (le 7 righe "Unmapped" del log sono letture di gdb: aggancio e `bt`). Log: `step7f_data/eden_log_7f_8regali_20261105.txt`.

## Seconda prova (5 nov sera): comandi di P5-P8 dopo il regalo, pagina Dialog
- Sintomo: dopo l'apertura dei regali P5-P8 non rispondevano a sinistra/destra/A/B (gli 8 cursori si muovono insieme coi tasti condivisi; il
  "giu'" dello stick e' il tasto 3..8 per P3-P8, configurazione di prova dello step 3).
- Misura (gdb): focus dei pad 0..3 (mgr+0xc68) = pagina kart 0x31; ombra cur8::foc dei pad 4..7 = pagina **7 (Dialog)**, +0x190 = fffffff0.
- Causa: la pagina Dialog (sempre in lista, sopra le pagine del menu) all'apertura 0x49be54 imposta il bit p di +0x190 e il focus e alla chiusura
  0x49c118 li toglie, **srotolate per i pad 0..3**: chiusa resta proprietaria dei pad 4..7. Alla chiusura della Present il focus dei pad 4..7 viene
  riscelto (FocusExtra = 0x8bd894..0x8bd930: pagina piu' alta col bit p) e va al Dialog. Senza regalo andava perche' SetWide assegna i pad 4..7 alla
  pagina kart all'ingresso.
- Correzione (flusso N = 8): DialogOpen8Hook 0x49bf0c e DialogClose8Hook 0x49c1ac estendono bit 4..7 di +0x190 (sempre) e setFocus (solo col cursore
  a 8, p < N) ai pad 4..7; Gift8FocusHook con la stessa guardia (senza cursore a 8 lo 0x8bd5f8 originale ridurrebbe p >= 4 al pad 0).
- [VERIFICATO in Eden, gdb] 8P: dopo l'apertura tutti i cursori rispondono (D/A/frecce, tasti 5-8 per P5-P8, X); 4P con regalo forzato a posto;
  0 fermate inattese, 0 letture non mappate del gioco. Log `step7f_data/eden_log_7f_dialog8_ok_20261105.txt`.

## Regalo personaggio (0x26) [VERIFICATO in Eden con forzatura alla fonte]
- Prima prova (a valle): scrivere sel+0x8c8 a 0x4b88f4 apriva la pagina 0x26 ma la scatola restava chiusa e l'input bloccato (0x4b14a8 e 0x4b21d4
  mai chiamati), uguale in 4P e con `MK8DX_GIFT8=0`: forzatura incompleta, non regressione. Log `eden_log_7f_chara8_blocco_20261105.txt`,
  `eden_log_7f_chara4_gift8off_blocco_20261105.txt`, `gdb_7f_chara8_blocco.txt`.
- Fonte: sel+0x8c8 viene da 0x866d50 (chiamata a 0x4b5a5c sui dati di salvataggio 0x8ad024()+0x190): tabella di stato per personaggio, 1 byte x 53
  a [0x8a1fc0(...)+0x188]+0x1f48; 2 = sbloccato da mostrare (restituisce il primo), 3 = gia' mostrato, 0 = bloccato. Nel salvataggio dell'utente
  tutti 3 tranne l'indice 37 (0 = bloccato, presumibilmente Mario d'oro).
- Forzatura alla fonte: breakpoint a main+0x866d98 (x11 = base) che mette a 2 il byte del personaggio 0 alla prima fermata (salvataggio copiato
  prima e rimesso dopo). [VERIFICATO] 8P: il regalo del personaggio si apre col flusso del gioco; 4P (avvio pulito): uguale.
- Ordine: sia in 8P sia in 4P il regalo compare **prima** dell'applet dei controller (il regalo parte nell'onEnter della selezione 0x4b88f4,
  l'applet e' chiesta dopo dalla stessa pagina). In 4P la mod non tocca il percorso dell'applet (P5a/P5b solo per 5-8): comportamento originale
  **confermato senza mod** (4P, gioco originale 4.0.1 con lo stesso breakpoint: prima il regalo, poi l'applet). Log `eden_log_7f_chara_fonte_4p_20261105.txt`.
- Dopo un'interruzione manuale di gdb il breakpoint a 0x866d98 non e' piu' scattato (stub): per riarmare una forzatura riavviare Eden.

## Note di metodo
- Un breakpoint su 0x4b14a8 (prima istruzione `str x19,[sp,#-0x20]!`) ha fatto saltare il gioco a un indirizzo dello heap (pc = lr = 0x215bfbf260):
  quasi certamente il bug dello stub (istruzione col breakpoint rieseguita -> sp spostato due volte); senza quel breakpoint nessun salto. Evitare
  breakpoint sui prologhi con scrittura pre-indicizzata. `gdb_7f_salto_bp_prologo.txt`.
- Con lo stub attivo, aprire le impostazioni di Eden lo fa chiudere (log troncato senza errori del gioco).
- Logging: stallo al titolo col debugger (MainThread in attesa dello spinlock con svcSleepThread(0), Prepare Thread a priorita' piu' bassa fermo
  in snprintf col lock): ora l'attesa e' svcSleepThread(100 us) (src/program/loggers.hpp). Possibile anche su Switch reale.
