# Step 2E – Crash con R a 8 viste e layer extra attivi (main 4.0.1)

Analisi statica del main decompresso (`img/main.bin`, offset = indirizzi; IPS = offset + 0x100).
Punto di partenza: crash nel thread di render ~1 s dopo "render scene Main viste 4 -> 8" in gara 4P con i
layer "3D Sub4..7" abilitati, PC = 4/5, backtrace
`0x6e223c|0x6e1524 <- 0x6e2808 <- 0x721450 <- 0x621b34 <- 0x6263c0 <- ... <- 0x602104`.

## 0. Risposta breve
- **Causa [VERIFICATO staticamente, da confermare a runtime]**: corruzione di memoria per indice di vista fuori
  limite **nei modelli del gioco, non nel renderer R**. Ogni modello gsys ha un `nn::g3d::ModelObj` con un
  proprio **ViewCount** (byte a `model+0xe0`), e i suoi array "per vista" sono dimensionati con quel numero.
  Il gioco crea quasi tutti i modelli (kart, pilota, oggetti, ...) con **ViewCount = 4 scritto nel codice**
  (`orr w8,wzr,#4` a **0x880704, 0x8807d4, 0x810d9c**, `orr w10,wzr,#4` a **0x880b88**). Quando una vista
  di R con indice 4..7 è attiva, il disegno del modello indicizza `model+0x60[vista]` (stride 0x58, solo 4
  voci) fuori dall'array, legge come puntatore il primo qword dell'allocazione seguente (un puntatore a shape)
  e ci scrive sopra le voci di disegno da 0x48 B -> vtable delle shape sovrascritte -> `blr [[shape]+0x18]`
  salta a 4/5.
- **Fix**: creare i modelli con ViewCount >= Views. O 4 patch da 4 byte (4 -> 8), oppure un hook su
  `0x69bb0c` (init modello) che porta `arg+0x8` da 4 a Views. Tutto il resto (R, liste di disegno, code per
  vista) scala già con `R+0xf0` oppure ha un clamp innocuo.
- **Ordine abilitazione/collegamento (domanda 3)**: non è la causa. Un layer abilitato ma scollegato esce
  subito dal DrawMethod (`0x68d20c cbz [layer+0x1020]`), e il crash richiede che la vista 4..7 sia attiva
  davvero (`view+0x388` bit0), cosa che succede solo con il layer abilitato *e* collegato. Questo spiega
  perché non crasha né nelle demo 1P (layer collegati ma disabilitati) né nella versione senza R a 8 viste.

## 1. La catena del crash [VERIFICATO]
*(estratto di disassemblato del gioco rimosso)*
- Chi scrive `ldrh [q]`: **0x721524**(DLM, R, view) – prende la voce `DLM+0x128 + n*0x3b8` (n = DLM+0x148++,
  clamp su DLM+0x120) e fa `strh view+0x8, [entry]` (0x721580), `strh 1<<idx, [entry+2]` (0x72158c),
  `entry+0x18+k*8 = view+0x18 + k*0x370 + 0x30` e `entry+0xe8+k*8` (26 code della vista, 0x6a1a3c),
  `entry+0x3a0/+0x3a8 = view+0x18 / +0x90`. Chiamata da 0x6abcac (calc per vista di R, 0x6ab60c) solo per le
  viste attive.
- **La lista "voci da 0x48 B" e gli oggetti con vtbl[0x18]**: le voci da 0x48 sono le *draw entry* per
  (vista, shape) del modello (allocate in 0x6db72c); l'oggetto chiamato è la **shape** (`[model+0x20][i]`,
  sottoclasse di ShapeObj gsys; vtbl[0x18] = priorità/tipo di disegno). Non sono command buffer né materiali.

## 2. Gli array per vista del modello e il loro ViewCount [VERIFICATO]
- Init modello **0x69bb0c(model, arg, heap)** (unico chiamante di `nn::g3d::ModelObj::Initialize` 0xa71b74,
  chiamato a 0x69bc3c con ModelObj a `model+0xc8`):
  - builder a sp+0x40: `+0x1c = arg+0x8` (0x69bbd8) = **ViewCount**; `+0x10/+0x14/+0x18 = model+0x15a`
    (= arg+0xc o, se -1, `[[res]+0x60]+0x3e0`, che vale 2 o 3: è il *buffering count*, non le viste).
  - 0xa71bcc `ldr w26,[builder,#0x1c]` -> 0xa71c18 `strb w26,[ModelObj,#0x18]` => **model+0xe0 = ViewCount**.
  - getter: vtable modello `[0x130cd90]+0x10` = 0x12b53f0, slot 0x1f8 = **0x6a02d4 `ldrb w0,[x0,#0xe0]`**.
- 0x6db638/0x6db72c (da 0x69c108): `model+0x60 = new {0x58 B}[VC]` (0x6db774-0x6db7b4); per ogni vista
  `[model+0x60 + v*0x58] = new {0x48 B}[numShapes]` (0x6db888-0x6db8ec); `model+0x68`, `model+0x58`
  (0xc × shapes × VC, 0x6db984) – tutto in un unico blocco lineare (0x68e45c/0x68e4d4): l'elemento VC
  dell'array 0x58 coincide con l'inizio del primo array 0x48 (+0 = puntatore shape). Da qui la corruzione.
- Lo stesso ViewCount dimensiona anche i dati per vista dentro g3d (ShapeObj/LOD/visibilità per vista).

### Sorgenti di ViewCount (tutte le strade che arrivano a 0x69bb0c)
| sito | valore | uso |
|---|---|---|
| 0x6aa80c-0x6aa84c contenitore R+0x5660 (`arg+4 = [arg R]`) | numViews di R | modelli interni di R |
| 0x6aaae8 modello R+0x5668 (`arg+8 = numViews`) | numViews | idem |
| 0x6b375c (0x6b322c, `[x22]` = numViews) | numViews | sottosistema di R |
| 0x72384c -> 0x68e80c (`arg+4 = 8`) | 8 | (già 8) |
| **0x880704** in 0x8806c4 (7 chiamanti: 0x17184, 0x1b3784, 0x3f80a8, 0x429a30, 0x50234c, 0x5025c0, 0x7f8aa8) | **4** | contenitori modelli gioco |
| **0x8807d4** in 0x8807a8 (53 chiamanti: 0xd5274, 0x10c3e8, 0x113f2c, 0x114950, 0x11c7ac, ...) | **4** | contenitori modelli gioco (kart, pilota, oggetti) |
| **0x810d9c** -> 0x818908 -> 0x69bb0c (`arg+8 = 4`) | **4** | modello singolo |
| **0x880b88** -> 0x6f47e8 -> 0x6f3e94 -> 0x6f4140 (`arg+4 = 4`) | **4** | pool modelli in `[R]+0x3fa8` |
Contenitore: `0x68e508` (arg default {0x40, 1, 1,1,1,1}), `0x68e54c` -> `0x68e708` salva `arg+4` in
`container+0x9a`; ogni modello del contenitore riceve `arg+8 = container+0x9a` (0x68eb68).

## 3. Cosa invece scala già (domanda 1, altri candidati) [VERIFICATO salvo dove indicato]
- **DLM** (R+0x54b8): `DLM+0x120/+0x128` (voci 0x3b8) allocate da 0x721ea0 con count = `[arg]` = numViews
  (0x6aa038 `ldr w6,[x21]`, x21 = arg di 0x6a8fec, `[arg]` = numViews). Clamp in 0x721524. OK.
- **Code per vista**: 26 code × 0x370 per vista (view+0x10/+0x18, 0x6a1a3c), dentro la vista stessa. OK.
- Array locale di item in 0x6e2430: 32 posti, ne servono <= numViews. OK. Maschere vista u16
  (`entry+2`, 0x72158c) e u32 (0x6aba4c): OK fino a 16/32.
- `[R+0x4218]` {+0x8 count, +0x10 voci 0x3d68}: usato con `view+0x8` e **clamp** (0x6b3b88-0x6b3b98,
  0x3c48d4); iterato fino a `R+0xf0` (0x3c48ac). [IPOTESI] count = numViews; se fosse 4 le viste 4..7 userebbero
  la voce 0 (dati di vista condivisi con la vista 0: artefatti, non crash).
- Gruppi `R+0x4228/+0x4230` (0x410 B): clamp (0x6aba10 `cmp; csel`) -> voce 0. Innocuo per il crash.
- Sottosistemi di R creati con numViews (0x6d3a28, 0x6d1e40, 0x6c9650, 0x6b322c, 0x6b5b54, R+0x5d40): OK.

## 4. Cosa deve fare il modulo (domanda 2)
### Opzione A (più semplice, globale): 4 patch da 4 byte
| offset main | IPS | originale | 8 viste | 6 viste |
|---|---|---|---|---|
| 0x880704 | 0x880804 | `e8 03 1e 32` orr w8,wzr,#4 | `e8 03 1d 32` | `e8 07 1f 32` |
| 0x8807d4 | 0x8808d4 | `e8 03 1e 32` | `e8 03 1d 32` | `e8 07 1f 32` |
| 0x810d9c | 0x810e9c | `e8 03 1e 32` | `e8 03 1d 32` | `e8 07 1f 32` |
| 0x880b88 | 0x880c88 | `ea 03 1e 32` orr w10,wzr,#4 | `ea 03 1d 32` | `ea 07 1f 32` |
In exlaunch: `exl::patch::CodePatcher(off).WriteInst(...)` (o scrittura diretta a 32 bit) in `exl_main`.
Vale per tutte le scene: più memoria per ogni modello anche nei menu.

### Opzione B (consigliata nel prototipo): hook su 0x69bb0c
```
HOOK_DEFINE_TRAMPOLINE(ModelInitHook) {   // bool 0x69bb0c(Model* m, Arg* arg, Heap* heap)
  static bool Callback(uint8_t* m, int32_t* arg, void* heap) {
    int32_t old = arg[2];                         // arg+0x8 = ViewCount
    if (old == 4 && proto::Want8) arg[2] = proto::Views;
    bool r = Orig(m, arg, heap);
    arg[2] = old;                                 // arg è un locale del chiamante
    if (old == 4 && proto::Want8) Logging.Log("ViewCount %p 4->%d ok=%d e0=%d", m, proto::Views, r, m[0xe0]);
    return r;
  }
};
ModelInitHook::InstallAtOffset(0x69bb0c);
```
- Copre tutte e 4 le sorgenti (i contenitori passano `container+0x9a` via `arg+8`, 0x68eb6c). `Want8` deve
  essere vero **prima del caricamento dei modelli della gara** (i modelli si creano al load della scena,
  prima del primo frame): nel prototipo basta `true` sempre, poi restringere a "gara con 5-8 giocatori".
- Verifica a runtime: dopo il load ogni modello disegnato deve avere `model+0xe0 >= R+0xf0`. Controllo di
  sicurezza (debug): breakpoint/hook su 0x6dd3c8 con `ldrh [entry] < ldrb [model+0xe0]`.
- Rischio [IPOTESI]: memoria. Per modello: VC × (0x58 + shapes×0x48 + shapes×0xc) CPU più i dati per vista
  di g3d (ShapeObj, eventuali blocchi uniform per vista delle shape view-dependent, memoria GPU). Se un heap
  di modelli si esaurisce, 0x69bb0c ritorna false o abort: loggare `r`. Con 6 viste (Views=6) il costo è minore.
- Non serve toccare R+0x4228 / 0x3cd5c8 / view+0x388 per evitare il crash (clamp). Per evitare artefatti
  (ombre/postfx condivisi con la vista 0) si potrà poi estendere 0x3cd5c8 (gruppi +0x409) alle viste 4..7.

## 5. Ordine abilitazione / collegamento (domanda 3) [VERIFICATO + valutazione]
- Layer abilitato con `+0x1020 = NULL`: il DrawMethod esce a 0x68d20c, nessun accesso a R: sicuro (confermato
  dai test "layer abilitati, nessun R a 8 viste": 60 FPS, nessun crash).
- Distruzione di R: il dtor della vista (0x6bc928/0x6bc978) fa `0x68d4f4(view+0x390, NULL)` sui layer collegati,
  extra compresi, come per i 4 Sub originali: nessuna differenza di ordine rispetto al gioco vanilla.
- La vista 4..7 diventa attiva (bit0 di view+0x388, da 0x6bcd30 che legge i flag del layer) solo con layer
  collegato *e* abilitato; solo allora 0x721524 crea voci DLM con indice 4..7 e scatta l'accesso fuori limite.
  Le demo 1P (layer collegati ma disabilitati: 18 gare senza crash) e il crash "~1 s dopo" (corruzione che si
  accumula finché una vtable di shape viene usata) sono coerenti con la causa del §0, non con un problema di
  ordine. Il log del collegamento mancante è quasi certamente solo non flushato.

## 6. Azioni in ordine
1. Hook 0x69bb0c (Opzione B) con log di `m`, ok, `m[0xe0]`; oppure Opzione A per un test rapido.
2. Ripetere la gara 4P con R a 8 viste e layer extra attivi: atteso nessun crash; loggare FPS e memoria.
3. Se resta un crash: break su 0x6dd3c8/0x6e1f94 con `ldrh [[item+0x20]]` vs `ldrb [model+0xe0]` per trovare
   modelli creati da un percorso non coperto; poi controllare altri sistemi per vista del gioco (effetti
   ptcl, 0x108a2c/0x3d230 a 4 viste, Lakitu 0x7e9b04).
4. Log a runtime di `[R+0x4218]+0x8` e `R+0x4228` (count dei due array con clamp) per sapere se le viste
   4..7 condividono i dati della vista 0.
