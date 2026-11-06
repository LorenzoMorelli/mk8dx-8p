# Step 5 – criterio non raggiunto: 8 cursori simultanei (layout 6pl/8pl con 6/8 slot attivi)

Data: 2 nov 2026. Raggiunto in alternativa: selezione **a due turni** con 4 cursori (step5_charsel.md), che porta 8
locali con personaggio e kart scelti in modo indipendente fino all'init della gara.

## Riferimento: interfaccia ufficiale Switch 2 (screenshot dell'utente, step5_data/ref_switch2_*.jpg, non in git)
- Selezione personaggi: **8 pannelli simultanei**, 4 per lato in colonna (sinistra: giallo, rosso, arancio, magenta;
  destra: ciano, verde, blu, arancio), ciascuno con **anteprima 3D del kart** e targhetta colorata con il nome; **8 cursori**
  sulla stessa griglia di personaggi (cornici colorate). Coincide con `mn_CharaKartSlct_8pl` (N_Player_00..07 a ±340 x,
  y 344/186/28/-130, L_KartSlot scala 0.3): il layout gia' caricato con `MK8DX_LYT8=8` e' quello ufficiale.
- Selezione kart: **8 slot**, due colonne di selettori a tre parti (carrozzeria/gomme/aliante) con cornice del colore del
  giocatore, anteprima 3D e nome del kart ai lati, `-/+` e lente per ogni slot.
Questo e' l'obiettivo dello step: la selezione a due turni e' un passo intermedio.

## Cosa manca per 8 cursori simultanei (catalogo verificato sul disassemblato, step5_plan_A1 §3.4, A2 §2.2-2.5, §3.3)
| componente | limite a 4 | siti | stima |
|---|---|---|---|
| selettore cursori del framework UI | oggetto 0xb8 B (0x8bafcc) con 4 stati da 0x20; input per frame p=0..3 srotolato (0x8b0084-0x8b00d0); aggiornamento cursori 0x8b01f8-0x8b0224; 19 clamp `cmp #4` in 0x8ae1e4..0x8b16b4 | rilocazione dell'oggetto + riscrittura di 2 cicli + 19 patch | alta |
| fuoco per giocatore | PageMgr+0xc68 ptr[4] (0x8bd5f8, 0x8bd6d8 `cmp x20,#4`), subito seguito da +0xc88 | non estendibile in loco: hook REPLACE di 2 funzioni | media |
| memoria cursori / oggetti grafici | `[[0x1306e70]]+0x60` int[0x88][4]; `0x8bf228()+0x1d0` ptr[4]; pagina 0xe +0x1d0 ptr[4] (pannelli anteprima 3D, 0x49ae1c) | nuove tabelle + rimappaggio; 4 pannelli 3D in più nel layout della pagina 0xe (il layout ne ha 4) | alta |
| pagine CharaWin per slot | id 0x28..0x2b (fabbrica 0x40918c); 17 siti `0x28+i`; link +0x660 ×4; maschera +0x1a8 con if srotolato 0..3 | 4 pagine extra su id liberi (0x64..0x67 [IPOTESI]) + trampolino 0x8bf0ec con rimappaggio per indirizzo di ritorno + ctor 0x4b28a4 | alta |
| classe selezione | +0x904 [4] (72 accessi / 39 clamp in 3 classi: a1_904_reloc.txt), +0x928 code [4], +0x660 link [4], init srotolato 0x4b80a0-0x4b8294, fabbriche 0xa28 | rilocazione in coda dell'oggetto (114 parole) + hook ctor | media (meccanica) |
| pagina CharaParts | w20 pannelli (fatto, A1-M1); colori/animazioni per 8 | – | bassa |
| pagina kart | +0x39c, +0x460 [4] (0x4c2948, 0x4c154c…), azzeramento srotolato 0x4c157c, selettori per slot 4..7 (L_KartSlot_04..07), `cmp w11,#4` 0x4c16d4, 0x49ae1c pannelli [4] | rilocazione + hook onEnter | media |
| config per giocatore | 13 array `int[4]` contigui cfg+0x1fb4..+0x2074 con ~50 clamp | array ombra nel modulo + hook sui setter/lettori | media |
| anteprime 3D | 4 posizioni fisse del Multi (screenshot 5c) | layout pagina 0xe / camere | sconosciuta |
| bit 4 di +0x1a8 | pad del tocco (step5_plan_C) collide con lo slot 4 | spostare il tocco a bit 8 in 4 siti (`C-bis`) | bassa |

## Perché ci si è fermati
Decine di siti in cinque sottosistemi del framework UI con semantica in parte sconosciuta (pagina 0xe, cursori grafici,
PageMgr): rischio alto di regressione 1-4P e di settimane di lavoro, contro un'alternativa funzionante (due turni).
Il layout 8pl resta caricabile con `MK8DX_LYT8=8` ma, senza questi lavori, mostra pannelli statici.

## Se si vuole procedere
Ordine consigliato: A1-M2 (rilocazione +0x904, provabile isolata in 1-4P) → pagine CharaWin extra + H-A2-4 → selettore e
PageMgr → +0x8c0 = N → pagina kart → config ombra → layout 8pl + pagina 0xe. Prerequisito: gara con 8 locali stabile (step 6),
altrimenti non si può verificare la fine del flusso.
