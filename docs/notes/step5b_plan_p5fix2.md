# Step 5b – passo 5, correzioni 2 (icone, cornici kart, cursore finestra varianti 4..7) – piano statico

Data: 3 nov 2026. Solo analisi statica: `src/` non modificato, nessuna build.
Base: `../img/main.bin`, `../img/main.ann.asm`, RomFS 4.0.1 (`../romfs-tmp/v4/common.sarc` → `cm_L_KPSlotCursor_00`, `cm_Page_PartsCursor`;
`../romfs-tmp/files.txt`), `exlaunch/source/lib/hook/nx64/inline_impl.cpp`.
Osservazioni di partenza: `step5b_data/eden_5b6_kart8_icone.png`, `eden_log_5b6_kart8.txt`, `eden_5b4_kart_p1vuoto.png`.
Verifica: `step5b_data/p5e_verify.py` → `p5e_verify.txt` (**0 errori**). Colori del bflan: `step5b_data/p5e_kpslot_colorchange.txt`.
Etichette: [VERIFICATO] = letto in main.bin, RomFS o sorgente exlaunch; [IPOTESI] = da confermare in Eden.

## 0. Sintesi
| difetto | causa | correzione |
|---|---|---|
| **1** icone vuote: carrozzeria di P1; carrozzeria e aliante di P5-P8 | La **coda di caricamento delle texture "a colori del personaggio"** (`tc_KP_*.bntx`) ha **capienza 64 = 4 giocatori × 16**. Con 8 giocatori arrivano 128 richieste: le 64 in eccesso vanno perse e **ognuna sovrascrive la coda[0]**, cioè la prima richiesta di P1 (carrozzeria 0, Kart standard). Anche lo heap delle texture è dimensionato per 64 [VERIFICATO] | 2 parole nel ctor 0x40c5f4: coda 64 → **128**, heap `UIHeap_PartsTex` 0x6d0000 → **0xda0000** |
| **2** cornici di P5-P8 invisibili, cornice di P1 rossa/arancio | L'hook **G3** (0x443590) scrive V = 8..11 in **x9**. Quel registro è poi confrontato dal `cmp w9,#8` che l'hook inline esegue *dopo* il callback, quindi `csel` → 0. I controlli 04..07 si posizionano tutti sull'item dello **slot 0** (la carrozzeria di P1) e coprono la cornice gialla di P1 [VERIFICATO] | parola di G3 in `cur8::kClampCmp`: `cmp w9,#8` → **`cmp w9,#0xc`** (0x7100313f) |
| **3** P8 (e P5-P7) nella finestra varianti: si muove ma il riquadro non si vede | Il cursore della CharaList ha 4 slot: P8 vive nello slot 0 (V(c,7) = 0). Il disegno 0x8b0238(c,0) controlla il focus del pad 0 (`mgr+0xc68[0]`, cioè P1) e il cursore grafico +0x1d0[0], quindi esce subito [VERIFICATO] | in `Cur8DrawHook` (REPLACE già esistente su 0x8b01ec), per le CharaList 0x01/0x02/0x5e/0x64: copiare lo slot 0 nello slot V = s+4, disegnare **con il cursore temporaneamente largo** (`+0xbc = 8`) con p = s, poi ricopiare lo slot e rimettere `+0xbc = 4` |

Nuovi indirizzi: **2 parole** (0x40c62c, 0x40c630), **nessun nuovo hook**. Si modificano una voce di tabella esistente (0x443590) e il corpo di
`Cur8DrawHook`. Nessuna collisione con `main.cpp` (p5e_verify).

## 1. Icone mancanti nei selettori parti

### 1.1 Meccanismo [VERIFICATO]
**Cella del selettore.** Classe 0x47446c (oggetto 0x330):
- 6 celle (4 se il selettore è piccolo) a `s+0x288 + k*0x18` = `{+0 sel, +8 pane P_Img_0k, +0x10 id mostrato, +0x14 flag "da ricaricare"}`
  (`s+0x290` = pane, `s+0x29c` = flag della cella 0).
- 0x474bf0, dopo aver ricostruito la lista, imposta in ogni cella la texture vuota `0x423fc4()` e alza il flag (0x474d1c..0x474d34).
- L'aggiornamento della cella **0x473f50(cella, id, regalo, k)** (chiamato da 0x478088) procede così:
  - prende `holder = 0x8bf0b0(&3)` (sistema 3 del menu) + 0x1b0;
  - per carrozzeria e aliante: `tex = 0x41071c(holder, tipo, id, &sel+0x278 (personaggio), sel+0x27c (variante))` (0x474174, 0x47432c);
  - le gomme (0x474254) usano lo stesso percorso, ma non hanno varianti;
  - `tex == NULL` → `0x423fc4()` (cella vuota); poi `0x8ada44(P_Img, tex, 0)`.

**Ricerca della texture.** `0x41071c` calcola l'indice con `0x40f884(holder, tipo, id, &pers, var)` e lo usa per leggere `holder+0x20[idx]`:
- indice ≥ 0 se la parte **ha varianti per personaggio**; in questo caso restituisce **NULL se non è stata caricata**;
- altrimenti cerca la texture generica per nome (`0x40f544` + `0x8b6714` nella risorsa del sistema 4, precaricata da 0x40f23c).

**Accodamento.** `0x4104a8(holder, &pers, var)` accoda per un personaggio:
- tutte le carrozzerie 0..0x28 e tutti gli alianti 0..14 con indice ≠ −1, in `holder+0x40[ +0x48++ ]`;
- con la scrittura `csel x8, x11, x10, hi` (0x410514): **se il contatore ≥ capienza (`+0x38`) scrive in coda[0]** e il contatore cresce
  comunque (0x410524).

**Chiamante.** Uscita della selezione (0x4bb450..0x4bb4cc):
- `0x40c360(holder)`: scarica tutto e azzera la coda;
- per i < `+0x8c0` con stato 7: `0x4104a8(holder, profilo(pad i)+0x21c, variante)`;
- `0x40c29c(holder)`: avvia il job.

  Con la mod il ciclo va a 8 (passo 4: 0x4bb470 `cmp #8`, 0x4bb47c → +0xa28).

**Job.** `0x40c000` (GOT 0x1308138) cicla i < `+0x48` su `coda[i]` con lo stesso clamp (i ≥ capienza → coda[0]) e carica
`tc_KP_<parte>_<personaggio>^l.bntx` nello heap `holder+0x10`.

**Capienze.** Ctor 0x40c5f4 → `0x40bd38(holder, 0x230 texture, **w2 = 0x40** coda, nome, **w4 = 0x6d0000** heap, 0)`:
- `+0x38` = 64: capienza della coda;
- pool degli oggetti texture `0x60a440(0x18, 64)`;
- heap "UIHeap_PartsTex" di 0x6d0000.

**RomFS.** Le parti con varianti `tc_KP_*` sono **14 carrozzerie + 2 alianti = 16 gruppi**:
- carrozzerie: K_Std, V_Atv, B_Std, K_Skl, B_Sct, K_Gla, K_Shp, K_Kpc, V_Tnd, V_Spl, K_Ten, K_Snk, K_Fml, B_Mgp;
- alianti: G_Std, G_Umb;
- ogni file pesa 110696 byte; 0x6d0000 / 64 = 0x1b400 = 111616 byte.

Quindi **16 richieste per personaggio, 4 × 16 = 64**: coda e heap del vanilla sono dimensionati esattamente per 4 giocatori.

### 1.2 Causa [VERIFICATO meccanismo; conteggio "16 per personaggio" dedotto da RomFS e capienza]
Con 8 giocatori confermati arrivano 128 richieste:
- le prime 64 (P1-P4) entrano;
- le 64 di P5-P8 scrivono tutte nella coda[0], che alla fine contiene l'ultima richiesta di P8;
- la prima richiesta di P1 (**carrozzerie, id 0 = Kart standard**, la carrozzeria mostrata da P1) va persa.

Questo spiega tutto quello che si vede:
- P1: carrozzeria vuota, aliante (6) e gomme presenti;
- P5-P8: carrozzeria e aliante vuoti, perché le loro texture non vengono mai caricate;
- le gomme ci sono sempre, perché non hanno varianti e usano la texture generica;
- P2-P4 sono corretti;
- il log "C" (`match=0`, valore valido) lo conferma: il problema non è nella lista.

Lo stesso succedeva già con i due turni (`eden_5b4_kart_p1vuoto.png`), perché il ciclo d'uscita della selezione era già a 8 (passo 4).

### 1.3 Correzione (2 parole, da applicare all'avvio: il ctor gira dopo)
| off | originale | nuova | |
|---|---|---|---|
| 0x40c62c | 52a00da4 `mov w4,#0x6d0000` | **52a01b44** `mov w4,#0xda0000` | heap UIHeap_PartsTex: 128 texture |
| 0x40c630 | 321a03e2 `orr w2,wzr,#0x40` | **321903e2** `orr w2,wzr,#0x80` | coda e pool degli oggetti texture: 128 |

- Nel codice: nuova tabella `p5::kTexQueue[] = {{0x40c62c,0x52a00da4,0x52a01b44},{0x40c630,0x321a03e2,0x321903e2}}`, controllata con `chk` e
  scritta con `put` nel blocco p5.
- Alternativa in un blocco proprio, sempre attivo quando `menu8` è a 8: è indipendente dal resto del passo 5.
- 1-4P: comportamento identico, con al massimo 64 richieste. Cambia solo la memoria riservata, +7 MB.
- [IPOTESI] Spazio nello heap genitore. Lo heap nasce con `0x605128(dim, nome, 0, 8, 1)` dentro l'oggetto di sistema 3, creato da
  0x40293c (funzione virtuale, vtable 0x1265d08), e non so se viva solo nel menu o per tutta la sessione.
  - Se l'allocazione fallisce (crash all'avvio o al menu): usare **0x52a01684** (`mov w4,#0xb40000`, 106 texture). Basta perché, con 8
    personaggi diversi, le texture distinte sono al massimo 8×5 (K_Std, V_Atv, B_Std, K_Skl, G_Std: una per personaggio) + i gruppi
    piccoli limitati dal numero di varianti (8+8+8+8+7+7+5+4+4+3+3) = 105. Le richieste duplicate non occupano heap (il job salta
    `holder+0x20[idx] != NULL`).
  - La coda però deve restare a 128.

### 1.4 Prova in Eden
- 8P, selezione → kart: tutte le carrozzerie e gli alianti hanno l'icona colorata del personaggio, anche quella di P1.
- Facoltativo: log una tantum in `KartEnterHook` di `h = 0x8bf0b0(&3)+0x1b0`: `+0x38` (deve valere 128), `+0x48` (≤ 128), `+0x10`
  (heap ≠ NULL).
- Scorrere tutte le carrozzerie di P8: non devono comparire celle vuote.
- 1-4P e gara 8P: nessun crash, quindi la memoria regge.

## 2. Cornici KPSlotCursor (pagina PartsCursor 0xe)

### 2.1 Fatti [VERIFICATO]
- **Layout.** `cm_Page_PartsCursor` ha già 8 parti, `cm_L_KPSlotCursor_00..07`. Nel vanilla l'onCreate 0x49acdc nasconde 04..07
  (0x49adcc). Con la mod (0x49ad48 `cmp x25,#7` + `PcCreateHook`) li crea tutti e 8, 04..07 a +0x210.
- **Colore.** `0x443738(ctl, i+1)` (0x49adac) posiziona l'animazione **ColorChange** (gruppo 2 della parte) al frame i+1. Il bflan
  ColorChange ha **8 frame**, con un colore giocatore per frame (`p5e_kpslot_colorchange.txt`, base RGB):

  | frame | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
  |---|---|---|---|---|---|---|---|---|
  | RGB | 255,200,0 | 0,255,255 | 255,65,65 | 60,230,10 | 240,40,30 | 20,60,220 | 140,20,255 | 240,80,20 |
  | colore | giallo | ciano | rosa | verde | rosso | blu | viola | arancio |

  I colori di 04..07 sono quindi già giusti, frame 5..8.
- **Accensione.** `0x4c24f4` (stato `+0x120 == 3` della kart), per i < `+0x39c`: `0x49ae1c(pc, i, stato_sel[i] == 7)`. Con `PcIndexHook`,
  4..7 → +0x210.. Mostra la parte (`0x8ae098(part+0x30,1)`) e porta "In" alla fine. Le cornici 04..07 **vengono accese**.
- **Posizione.** Metodo del controllo **0x44355c**:
  - `slot = clamp(ctl+0xc0)` (0x443588..0x443594, G3), poi `idx = [[ctl+0xb8]+0x1b0] + slot*0x20 + 0x30` e `item = cur+0x18[idx]`;
  - copia posizione e scala dal pane dell'item.
- **Ordine di esecuzione exlaunch** (`inline_impl.cpp`): l'entry fa `bl impl` (callback) **prima** di `b trampoline`, che esegue
  l'istruzione originale spostata.
  - G3 è installato **sul `cmp w9,#4`** stesso (0x443590), patchato a `#8`. Il callback `CUR8_CLAMP(G3, 9, 9, X8)` scrive
    `x9 = V(cur, w9)` = **8..11** per i controlli 4..7 (cursore B largo).
  - Subito dopo viene eseguito `cmp w9,#8` → `csel x9,x9,xzr,lo` → **0**.
  - È l'unico `CUR8_CLAMP` con PAD == SRC (p5e_verify: `['G3']`). Negli altri il registro confrontato è il pad originale (< 8) e il
    valore V sta in un altro registro.

### 2.2 Causa
- I controlli 04..07 leggono l'indice dello **slot 0** (P1) e si posizionano tutti sulla cella che P1 ha selezionato, cioè la carrozzeria
  di P1.
- Sono disegnati sopra la 00 (ordine dei pane 00..07): la cornice di P1 appare arancio-rosata, miscela di 07 arancio, 06, 05 e 04 sopra
  il giallo. Pixel dello screenshot sul bordo: (255,143,96), con G > B, cioè arancio; la cornice di P3 ha invece (255,152,153).
- P5-P8 «non hanno» cornice perché le loro sono sopra P1.
- [VERIFICATO meccanismo; IPOTESI forte sulla miscela dei colori]

### 2.3 Correzione
In `cur8::kClampCmp` cambiare **solo la parola nuova di G3**:
```
        {0x443590, 0x7100113f, 0x7100313f},   /* w9 x9 cur=X8 ... G3: cmp w9,#0xc (PAD==SRC: il callback scrive V=0..11 in w9) */
```
- V è sempre < 12: `p<4 → p`; largo → `p+4` ≤ 11; altrimenti 0.
- Il `csel` lascia quindi passare V; `add x9,x8,x9,lsl #5` → slot V (+0x128.. per 4..7), `ldrsw [..,#0x30]` → indice dello slot.
- 1-4P: V = p < 4, nessuna differenza.
- Il controllo all'avvio confronta solo `orig` (7100113f), che resta invariato.

### 2.4 Prova
- 8P kart: P1 con cornice **gialla**; P5 rossa, P6 blu, P7 viola, P8 arancio, ciascuna sulla parte (carrozzeria/gomme/aliante)
  scelta da quel giocatore, e si spostano con il suo pad.
- Il Loop (pulsazione) di 04..07 lo dà `KpLoopHook`, già installato.

## 3. Riquadro del cursore nella finestra varianti degli slot 4..7

### 3.1 Fatti [VERIFICATO]
- **Cursore della CharaList.** Creato da `0x409f90 → 0x8bafb8(page,10)`: oggetto 0x1a8 con magic `CUR8`, `+0xbc = 4`, quindi non largo.
  Per P8: `V(c,7) = 0`, e la logica d'ingresso/decisione usa lo **slot 0** (log D: `slot0 2/0`, poi `7/0`).
- **Focus.** Il focus di P8 è la pagina 0x64: `cur8::foc[3] == cl`, log `foc=1`.
- **Disegno.** `0x8b01ec` (= `Cur8DrawHook`, REPLACE) → `0x8b0238(c, p)` per p < 4. In 0x8b0238 lo stesso `x22 = V(c, w21)` (F14) indicizza:
  - il **focus** `mgr+0xc68[x22]`; `Cur8FocD` lo porta su `foc[x22-8]` per x22 ≥ 8, e il valore va confrontato con `c+8` (pagina);
  - il **cursore grafico** `cp+0x1d0[x22]`, con x22 = 11 → +0x228 = `cm_L_BtnCursor_07` (0x8b02d0);
  - lo **slot** `c+0x28+x22*0x20`, letto e anche **scritto**: stato 5/6 → 4/7, +0x10 = −1, posizione +0x14..+0x1c (0x8b0500..0x8b0788).

  Il pad reale `w21` serve solo per il bit `c+0xb0` (0x8b04b4, 0x8b082c) e per `item vt+0x70(item, w21)` (0x8b07e4/0x8b081c).
- **Perché P8 non si vede.** Con p = 0, `mgr+0xc68[0]` è il focus di P1, cioè la selezione e non la 0x64: uscita immediata, e P8 non
  viene mai disegnato. Il cursore della selezione (largo) per p = 7 esce anch'esso, perché `foc[3]` ≠ selezione.

### 3.2 Correzione (nel corpo di `Cur8DrawHook`, nessun nuovo indirizzo)
```cpp
HOOK_DEFINE_REPLACE(Cur8DrawHook) {                        /* 0x8b01ec: 0x8b0238(cur,0..3) srotolato, poi +0xb0 = 0 */
    static void Callback(uint8_t* c) {
        using Draw = void (*)(uint8_t*, int);
        int n = cur8::Wide(c) ? 8 : 4;
        for (int p = 0; p < n; p++) mk8dx::Fn<Draw>(0x8b0238)(c, p);
        /* p5fix2 §3: finestra varianti degli slot 4..7 (CharaList 0x01/0x02/0x5e/0x64, cursore a 4 slot: il giocatore 4+j usa lo
           slot 0). Disegno con il cursore temporaneamente largo: slot 0 -> slot V = s+4, focus foc[s-4], grafico cp+0x210+(s-4)*8. */
        uint8_t* pg = n == 4 && p4::one && *(uint32_t*)(c + 0xb8) == cur8::kMagic ? *(uint8_t**)(c + 8) : nullptr;
        if (pg) {
            static constexpr int32_t kIds[4] = { 0x01, 0x02, 0x5e, 0x64 };   /* = cw8::kIds (cw8 e' definito piu' avanti) */
            int32_t id = *(int32_t*)(pg + 0xe8); int s = -1;
            for (int j = 0; j < 4; j++) if (kIds[j] == id) s = 4 + j;
            if (s >= 4 && cur8::foc[s - 4] == pg) {
                uint8_t* s0 = cur8::Slot(c, 0); uint8_t* sv = cur8::Slot(c, s); uint8_t keep[0x20];
                memcpy(keep, sv, 0x20); memcpy(sv, s0, 0x20);
                *(int32_t*)(c + 0xbc) = 8;                   /* Wide(c): V(c,s) = s+4 in F14/FocD */
                mk8dx::Fn<Draw>(0x8b0238)(c, s);
                *(int32_t*)(c + 0xbc) = 4;
                memcpy(s0, sv, 0x20); memcpy(sv, keep, 0x20); /* lo stato aggiornato dal disegno torna nello slot 0 */
            }
        }
        *(uint32_t*)(c + 0xb0) = 0;
    }
};
```
- Funziona per tutti gli slot 4..7: P5 → 0x01, P6 → 0x02, P7 → 0x5e, P8 → 0x64.
- Le CharaList 0x28..0x2b (1-4P e slot 0..3) non cambiano: id diversi e `p4::one` falso in 1-4P.
- La chiamata originale con p = 0 resta. Nella CharaList k esce subito (focus di P1 ≠ pagina) senza toccare lo slot 0.
- Il cursore grafico 04..07 è già visibile (`ShowPanes(true)` della selezione larga). Colore (frame 7..10) e Loop sono gestiti da
  `Cur8GfxStoreHook` e `Cur8GfxLoopHook`.
- `CwGfxCursorHook` (0x46c10c) agisce già sullo stesso grafico (+0x228 per lo slot 7), quindi è coerente.
- [IPOTESI] `item vt+0x70(item, s)` riceve il pad reale 4..7, come accade già con il cursore largo della selezione (P5-P8 vi
  disegnano correttamente). Se l'item della finestra (bottone `mn_L_CharaWinSlctBtn`) avesse un clamp a 4 senza hook, il riquadro
  avrebbe il colore o il frame di P1; se succede, cercare il clamp in vt+0x70 di quella classe.
- **Alternativa scartata.** Rendere la CharaList «larga» in modo permanente (`+0xbc = 8` all'apertura, come la selezione) sposterebbe
  P8 dallo slot 0 allo slot V = 11 anche per input, attivazione e decisione. Oggi quel flusso funziona sullo slot 0, va riverificato tutto
  (CursorPoll, Activate, Decided, maschere, `wideCur` unico) e il rischio è alto. La copia temporanea tocca solo il disegno.

### 3.3 Prova
- 8P: P8 con Link (o qualunque personaggio con varianti), P5 con Yoshi.
- Nella finestra varianti compare il riquadro del colore di quel giocatore attorno al costume e segue il movimento.
- Alla conferma: animazione "deciso" e il riquadro sparisce.
- P1-P4 con le loro finestre: invariati.
- Controllare che il cursore di P8 nella selezione non riappaia mentre la finestra è aperta.

## 4. Tabella riassuntiva da integrare (verificata, `p5e_verify.txt`)
| tipo | indirizzo | parola attesa | nuova / azione |
|---|---|---|---|
| parola | 0x40c62c | 52a00da4 `mov w4,#0x6d0000` | 52a01b44 `mov w4,#0xda0000` (riserva: 52a01684 `#0xb40000`) |
| parola | 0x40c630 | 321a03e2 `orr w2,wzr,#0x40` | 321903e2 `orr w2,wzr,#0x80` |
| parola (tabella esistente) | 0x443590 | 7100113f `cmp w9,#4` | 7100313f `cmp w9,#0xc` (era 7100213f) |
| hook esistente | 0x8b01ec `Cur8DrawHook` (R, prima parola f81e0ff3) | — | nuovo corpo (§3.2) |

- Nessun nuovo trampolino né hook inline: la capienza JIT non cambia.
- `0x40c62c` e `0x40c630` non compaiono in `main.cpp`.
- Diagnostica facoltativa: log di `holder+0x38/+0x48/+0x10` (§1.4).
