# Step 7e – La tabella a 10 posti di 0x8bf11c (NULL per i giocatori 7-8)

Analisi statica del main 4.0.1 (`img/main.ann.asm`, offset = indirizzo). [V] = verificato nel disassemblato,
[I] = ipotesi. Nessuna modifica fatta al codice della mod.

## 0. Riassunto
- La tabella `[[0x1306e70]]+0x28` NON e' una tabella per giocatore: e' un **enum fisso di 7 "maniglie" di layer 2D**
  (oggetti 0xa0 B) registrate **una sola volta** a 0x3ff90c..0x3ffad0, senza cicli e senza dipendere dal numero di
  giocatori: idx 0 = 2D Back, 1..4 = 2D Sub0..3 (vista del giocatore idx-1), **5 = 2D Main**, **6 = 2D Front**, 7..9 vuoti. [V]
- Con indice = giocatore+1: giocatore 4 -> 2D Main, giocatore 5 -> 2D Front (sbagliati ma non nulli), giocatori 6 e 7
  -> posti 7 e 8 = NULL -> 0x8bfafc(NULL) legge [0x10]. [V]
- Non esiste un "limite N" da alzare: il flusso con N=8 non c'e' nel main. L'oggetto e' grande esattamente 0xd8 B
  (10 posti + mappa inversa int[0x20]) e gli indici 5/6 sono gia' occupati: l'estensione in loco e' impossibile. [V]
- Patch proposta: 4 maniglie extra costruite con le funzioni del gioco (0x5fa960 + ctor 0x8bfa18 + 0x8bfa78(h, slot 18..21))
  e un hook REPLACE su 0x8bf11c che, **solo** per i due chiamanti per-giocatore (ritorno 0x551a0c e 0x526830) e
  giocatore 4..7, restituisce la maniglia extra; tutto il resto passa all'originale.

## 1. Che cos'e' la tabella

### 1.1 Contenitore e oggetto tabella [V]
- Singleton `S = [[0x1306e70]]` (GOT 0x1306e70). Campi creati in 0x8b11ec..0x8b1214 e riempiti da 0x3ff5d0:
  `S+0x8` registro di 12 oggetti (0x8b3e3c, id 1..12), `S+0x28` **tabella a 10 posti**, `S+0x58` pad UI ptr[5] (0x8bf158).
- Allocazione tabella: `0x8b11f4 mov w0,#0xd8 ; bl 0x5fa960 ; bl 0x8bfc20 ; str x22,[x20,#0x28]`.
- Ctor 0x8bfc20: vtable, `memset(+0x8, 0, 0x50)` = **10 puntatori**, `memset(+0x58, 0xff, 0x80)` = **int[0x20] = -1**.
  0x8 + 0x50 + 0x80 = 0xd8 = dimensione allocata: nessuno spazio libero.
- Accessori: 0x8bfcc0 = `S+0x28`; 0x8bf11c(&w) = `tab[8 + (w<10 ? w : 0)*8]` (cmp #0xa a 0x8bf140, parola `7100291f`).
- Registrazione 0x8bfc6c(tab, &idx, h): `h+0x8 = idx` (0x8bfa6c), `tab[idx<10]` = h (str a 0x8bfcac).
  **Unico chiamante** di 0x8bfc6c: 0x3ff5d0 (7 chiamate). Nessun altro scrittore di `tab+8+i*8`.

### 1.2 La maniglia (oggetto 0xa0, ctor 0x8bfa18) [V]
- `+0x0` vtable, `+0x8` idx (-1 dal ctor), `+0x10` **puntatore al layer** (0 dal ctor), `+0x18` sottoggetto (0x61a7a0),
  `+0x98` flag (bit0, bit1; 0 dal ctor), `+0x9c` int (-1 dal ctor).
- 0x8bfa78(h, n): `h+0x10 = ls->ptr[n]` con `ls = [[0x12fa5e0]]` (layerSystem), clamp `n < [ls+0x40]` altrimenti ptr[0];
  poi `tab+0x58 + n*4 = h+8` per n < 0x20 (mappa inversa slot layer -> idx).
- 0x8bfafc(h) = `[h+0x10] + 0x28`; 0x8bfb08 = `[[h+0x10]+0x70]` (camera/proiezione del layer); 0x8bfb14 = `[[h+0x10]+0x68]`.
- 0x8bfb20(h, b0, b1, &v): `+0x98` bit0 = b0, bit1 = b1, `+0x9c` = v.
- [I] layer+0x28 = `sead::Viewport` del layer (vtable +0x28, rect normalizzato +0x30..+0x3c, orientamento +0x40, cfr.
  step2_renderlayers.md): 0x8bfafc restituisce la viewport del layer 2D.

### 1.3 Contenuto (0x3ff5d0, registri w24..w28 fissati a 0x3ff618/0x3ff664/0x3ff6ac/0x3ff6f4) [V]
| sito | idx tabella | slot layerSystem (0x8bfa78) | layer (step2) | 0x8bfb20 (b0, b1, +0x9c) |
|---|---|---|---|---|
| 0x3ff90c / 0x3ff918 | 0 | 1  | 2D Back  | 0, 0, 5 |
| 0x3ff954 / 0x3ff960 | 1 (w26) | 6  | 2D Sub0 | 0, 0, -1 |
| 0x3ff9a0 / 0x3ff9ac | 2 (w25) | 7  | 2D Sub1 | 0, 0, -1 |
| 0x3ff9e8 / 0x3ff9f4 | 3 (w24) | 8  | 2D Sub2 | 0, 0, -1 |
| 0x3ffa30 / 0x3ffa3c | 4 (w28) | 9  | 2D Sub3 | 0, 0, -1 |
| 0x3ffa78 / 0x3ffa84 | 5 (w27) | 10 | 2D Main | **1**, 0, -1 |
| 0x3ffac4 / 0x3ffad0 | 6 | 12 | 2D Front | 0, 0, -1 |
| – | 7, 8, 9 | – | mai registrati | – |
Quindi indice = "giocatore+1" vale solo per 1..4; il posto 0 e' il 2D Back.

### 1.4 Lettori [V]
- 0x8b4560 (lettore a 0x8b4570) e 0x8b48b0 (0x8b48c0): cicli `w23 < 10` sui posti, saltano NULL (`cbz` 0x8b45e8/0x8b4938)
  e posti senza flag (bit0 / bit1 a +0x98); usano il pad UI 0x8bf158(i) con dati per posto a pad+0xa0+i*0xc
  (clamp #0xa). [I] proiezione tocco/puntatore nei layer 2D abilitati.
- 0x8be8e0..0x8be9c8: ciclo `x27 < 10`, salta NULL (`cbz x24` 0x8be954), considera solo `+0x9c == -1` e bit0.
- 0x422efc(count, flag) (chiamanti: 0x3ffbb8, 0x4b82a0, 0x4bbcd8, 0x4c17c4, 0x4c3c94, 0x4d4008, 0x4dc1b0, 0x4dcaf8,
  0x53e488, **tutti con w1 = 0**, count = 1, 2 o config+0x1fa4): accende il tocco dei pad UI 0..3 con
  `cset` su count (srotolato a 4), poi 0x8bf11c(5) -> 2D Main bit0 = !(flag && count>1) = 1, e 0x8bf11c(1..4) ->
  bit0 = flag && count>i. Con flag sempre 0 i Sub restano spenti; il Main resta acceso.
- Lettori della viewport (0x8bf11c + 0x8bfafc): 0x551914, 0x526764 (per giocatore), 0x547a40 e 0x54d0b4 (idx fisso 1).

## 2. Perche' i posti 7..9 sono NULL con 8 locali
- [V] Non c'e' alcun limite dipendente da N: la tabella e' riempita con 7 chiamate a costanti in 0x3ff5d0 (init del
  singleton, una volta sola). Nessun ciclo, nessun conteggio di controller/giocatori (settings+0x180/+0x184,
  raceMgr+0x178, config+0x1fa4 non sono letti li').
- [V] Il "limite" e' strutturale: l'enum ha 4 Sub (idx 1..4) seguiti da Main (5) e Front (6). Con w = giocatore+1:
  - giocatore 4 -> idx 5 = maniglia **2D Main** (viewport a schermo intero: errata ma valida);
  - giocatore 5 -> idx 6 = **2D Front** (errata ma valida);
  - giocatori 6, 7 -> idx 7, 8 = **NULL** -> 0x8bfafc: `ldr x8,[x0,#0x10]` a 0x8bfafc (parola `f9400808`) legge 0x10.
  Coerente con la misura GDB ([o+0x100]=7 -> w=8 -> NULL, x0 di ritorno 0x28 = 0+0x28).
- [V] L'istruzione che produce l'indice fuori enum e' nel chiamante, non nella tabella:
  `0x551a00: 11000528  add w8, w9, #0x1` (w9 = [o+0x100] **non clampato**; lo stesso w9 e' clampato <4 solo per
  l'indice RLM a 0x5519d4/0x5519d8). Gemello: `0x526824: 11000528  add w8, w9, #0x1` (w9 = [x19+0xdc]).
- [I] La build Switch 2 deve avere un enum diverso (es. Sub0..7 + Main + Front = 11 posti > 10) o una tabella piu'
  grande: in questo main non c'e' traccia.

## 3. Patch

### 3.1 Estensione "stesso flusso con N=8": non possibile in loco [V]
1. Non c'e' un ciclo o un conteggio da portare a 8 (registrazione a costanti).
2. Posti liberi: solo 7, 8, 9 (3 su 4 necessari); l'oggetto e' 0xd8 B pieno (la mappa inversa int[0x20] segue subito
   i 10 puntatori).
3. Anche rilocando la tabella (es. 14 posti) gli indici giocatore+1 di 4 e 5 restano 5 e 6 = Main/Front, e 0x422efc
   usa la costante 5 per il Main: servirebbe comunque rinumerare l'enum o cambiare i due `add w8,w9,#1`. Inoltre
   i clamp `cmp #0xa` sono anche in 0x8b45d8/0x8b4614/0x8b4928/0x8b4964/0x8be914/0x8bfca0 e i dati per posto del
   pad UI (pad+0xa0+i*0xc, 10 voci) non sono estendibili.

### 3.2 Alternativa proposta (minima, usa le funzioni originali per costruire le maniglie)
A. **4 maniglie extra** per 2D Sub4..7 (slot layerSystem 18..21, gia' registrati dalla mod, main.cpp "2D Sub4".."2D Sub7"),
   create con il codice del gioco nello stesso modo di 0x3ff8e8..0x3ff918, ma senza 0x8bfc6c (nessun posto in tabella):
   ```
   h = 0x5fa960(0xa0);  0x8bfa18(h);  0x8bfa78(h, 18 + k);   // k = 0..3
   ```
   - [V] 0x8bfa78 scrive `tab+0x58+(18+k)*4 = h+8 = -1` (ctor): la mappa inversa resta al valore di default (memset 0xff).
   - [V] Prerequisito: `[ls+0x40] > 21`, altrimenti 0x8bfa78 prende ptr[0] (clamp a 0x8bfaa8/0x8bfaac).
   - Momento: dopo la registrazione dei layer 2D Sub4..7 (hook del ctor RLM 0x3ca44c della mod), oppure pigramente alla
     prima richiesta (i layer sono permanenti).
   - Flag a 0 / +0x9c = -1: i cicli tocco (0x8b4560, 0x8b48b0, 0x8be8e0) non le vedono comunque (non sono in tabella).
B. **REPLACE (o TRAMPOLINE) su 0x8bf11c** (prima istruzione `f81e0ff3 str x19,[sp,#-0x20]!`):
   ```
   void* Get(int* w) {
       uintptr_t ra = (uintptr_t)__builtin_return_address(0) - Base();
       if (active8 && (ra == 0x551a0c || ra == 0x526830) && *w >= 5 && *w <= 8)
           return hExtra[*w - 5];            // giocatori 4..7 -> 2D Sub4..7
       return Orig(w);
   }
   ```
   - [V] Il discriminante via indirizzo di ritorno e' necessario: 0x422f9c chiama 0x8bf11c con la costante 5 (2D Main)
     e non deve essere deviato. Gli altri chiamanti (0x422fc0/fe0/3000/3024 = 1..4, 0x547a9c e 0x54d114 = 1) non
     arrivano mai a 5..8.
   - [I] Con il salto `b` dell'hook all'ingresso il LR e' ancora quello del chiamante: va verificato che la macro
     exlaunch usata non passi da un `blr` intermedio (in quel caso leggere il LR salvato o usare un hook inline).
   - Variante senza LR: hook inline su 0x551a00 e 0x526824 (istruzione `add`, rilocabile) che, per giocatore >= 4, mette in
     uno statico la maniglia da restituire e un REPLACE su 0x8bfafc (3 istruzioni) che la usa se l'argomento e' NULL; piu'
     fragile (non corregge i giocatori 4 e 5, che ottengono Main/Front non nulli). Sconsigliata.
C. Nessuna parola del main da cambiare con IPS: la patch e' tutta nel modulo (hook + 4 allocazioni).
   Se si volesse invece una sola toppa IPS "di emergenza" contro il crash (non segue il flusso): a 0x551a00
   `11000528 add w8,w9,#1` -> `52800028 mov w8,#1` (tutti i giocatori prendono la viewport di Sub0) e lo stesso a 0x526824.
   Elimina il NULL ma da' dati sbagliati ai giocatori 2-8: solo diagnostica.

### 3.3 Effetti collaterali [V salvo dove indicato]
- 0x551914 per giocatore >= 4 ha altre due letture clampate (non crash, dati del giocatore 0):
  `0x551950 cmp w9,#0x4` su [o] -> `[[0xc7a48()+0x1f8]+0x60][0]` (x20), e `0x5519d4 cmp w9,#0x4` su [o+0x100] ->
  slot RLM 0 (`mgr+0x9bc`), quindi camera `[layer3D+0x70]` del giocatore 0 (x21). I tre valori (x20, x21, viewport)
  sono scritti solo nei figli `[o+0x58][0..[o+0x50])` a +0xd8/+0xe0/+0xe8: con [o+0x50] = 0 (misurato) non hanno
  consumatori. Se in futuro [o+0x50] > 0, servira' anche la camera del layer 3D Sub4..7 (slot 14..17) per x21.
- 0x526764 (metodo virtuale, puntatori a 0xda7ef0 e 0x1285fa0, thunk 0x527750 con this-0x30): stesso schema con
  [x19+0xdc]: clamp <4 per la camera 3D (0x5267f8) e `add w8,w9,#1` a 0x526824 -> per giocatori 6-7 stesso NULL a
  0x526830 -> 0x8bfafc, per 4-5 Main/Front. Scrive x20/x21/viewport a [x19+0x128..0x138] e +0xe2 = 1.
  [I] e' una parte HUD per giocatore (pagine RaceView): diventera' un problema appena la mod istanzia quelle parti per
  i giocatori 4..7. La patch B la copre gia'.
- 0x547a40 e 0x54d0b4: indice fisso 1 (giocatore 0): nessun problema.
- 0x422efc: indici 1..5 costanti, conteggio srotolato a 4: con count = 8 accende il tocco dei pad 0..3 e (con flag 0)
  lascia i Sub spenti come per 4: nessun NULL. Non va toccata.
- Memoria: la tabella a 10 posti non va ingrandita (le maniglie extra vivono fuori tabella); 4 x 0xa0 B dall'heap del
  gioco (0x5fa960, stesso allocatore di 0x3ff8f0).

## 4. Chiamanti (completo per bl/b diretti)
- 0x551914: 0x5345ac, 0x5345bc, 0x5345cc, 0x5345dc, 0x5345ec (race+0x358[0..4]) + mod (EachExtraObj, 0x5345e0).
- 0x8bf11c: 0x422f9c(5), 0x422fc0(1), 0x422fe0(2), 0x423000(3), 0x423024(4), 0x52682c(p+1), 0x547a9c(1),
  0x551a08(p+1), 0x54d114(1).
- 0x8bfcc0: 0x8b45a0, 0x8b48f0, 0x8be8e0, 0x8bf12c, 0x8bfabc.
- 0x8bfc6c: solo 0x3ff90c..0x3ffac4 (7). 0x8bfa78: solo 0x3ff918..0x3ffad0 (7). 0x8bfc20: solo 0x8b1200.
- [I] Lettori della mappa inversa `tab+0x58`: non trovati con i pattern cercati.
