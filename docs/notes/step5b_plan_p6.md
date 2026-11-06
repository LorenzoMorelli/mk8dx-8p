# Step 5b passo 6: impostazioni di P5-P8 persistenti (file su SD)

Obiettivo dello step: "le impostazioni e gli aiuti di P5-P8 restano dopo il riavvio".
Questo documento contiene solo analisi statica e progetto: `src/` non è stato modificato e non è stata fatta nessuna build.
Dati: `step5b_data (dati non pubblicati)/p6_fs_symbols.txt`, `p6_sdk_autoabort.asm`, `p6_hooks.txt`.
Fonti: `img/main.ann.asm`, `img/main.syms`, `img/sdk.syms`, `img/sdk.bin` (sdk del 4.0.1), `exlaunch/deploy/main.npdm`.

## 0. Sintesi
- **Cosa salvare**: `cfg8::sh[13*4]` (ombra dei 13 int[4] cfg+0x1fb4..+0x2083 per gli indici 4..7) e `aid8::code[3][4]` (aiuti Y/L/R di
  P5-P8). Il file è di 0x100 B, con magic, versione e checksum.
- **Dove**: file nostro sulla SD, `sd:/mk8dx-8p/p58.bin`, montato con `nn::fs::MountSdCardForDebug("mk8p8sd")`. Il salvataggio del gioco
  non viene toccato. In Eden il file finisce in `switch_env/eden/user/sdmc/mk8dx-8p/p58.bin`.
- **API**: le funzioni nn::fs esportate dal modulo `sdk`, che il subsdk9 importa per simbolo. I nomi mangled sono stati verificati
  compilando le dichiarazioni (§1.2).
- **Protezione dagli abort**: `nn::fs::ScopedAutoAbortDisabler` (ctor esportato) intorno a ogni chiamata. Nell'sdk 4.0.1 l'auto-abort è
  attivo di default, quindi senza questa protezione un errore come SD assente o permesso negato chiuderebbe il gioco (§1.3).
- **Permessi npdm**: aggiungere il bit 21 (SdCard) a `config.json`, da `0x4000000000040000` a `0x4000000000240000`. Serve su
  Atmosphère; in Eden non dovrebbe servire [IPOTESI].
- **Quando caricare**: una volta sola, al primo `MenuEventHook` (0x4d92e8, sempre installato); come rinforzo anche in testa a
  `ProfileToEntriesHook` (0x3fdd70) e `CharselEnterHook` (0x4b74a4). Tutti e tre precedono qualunque lettura o scrittura dell'ombra.
- **Quando salvare**: in coda ad `AidsSaveHook` (0x3fe040). Il gioco la chiama alla conferma dei kart (0x4c3c1c, quando tutte le parti
  sono decise) e a fine gara (0x535404, dopo i toggle in pausa). Si scrive solo se l'immagine è cambiata.

## 1. API nn::fs usabili dal modulo

### 1.1 Simboli [VERIFICATO]
`p6_fs_symbols.txt`: tutte le funzioni sono **esportate (D) dal modulo sdk**. Il main ne importa la maggior parte; non importa
`MountSdCard*`, `RenameFile` e le funzioni di FsContext/AutoAbort, ma al subsdk9 bastano le esportazioni dell'sdk.

| funzione | simbolo | sdk | main |
|---|---|---|---|
| MountSdCardForDebug(const char*) | `_ZN2nn2fs19MountSdCardForDebugEPKc` | 0x6b6fc | no |
| MountSdCard(const char*) | `_ZN2nn2fs11MountSdCardEPKc` | 0x6b478 | no |
| CreateDirectory(const char*) | `_ZN2nn2fs15CreateDirectoryEPKc` | 0x4f610 | sì |
| CreateFile(const char*, long) | `_ZN2nn2fs10CreateFileEPKcl` | 0x4f4ac | sì |
| OpenFile(FileHandle*, const char*, int) | `_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci` | 0x50080 | sì |
| ReadFile(unsigned long*, FileHandle, long, void*, unsigned long) | `_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm` | 0x4f040 | sì |
| WriteFile(FileHandle, long, const void*, unsigned long, const WriteOption&) | `_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE` | 0x4f068 | sì |
| GetFileSize(long*, FileHandle) | `_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE` | 0x4f370 | sì |
| SetFileSize(FileHandle, long) | `_ZN2nn2fs11SetFileSizeENS0_10FileHandleEl` | 0x4f27c | sì |
| CloseFile(FileHandle) | `_ZN2nn2fs9CloseFileENS0_10FileHandleE` | 0x4ed70 | sì |
| ScopedAutoAbortDisabler::ScopedAutoAbortDisabler() | `_ZN2nn2fs23ScopedAutoAbortDisablerC1Ev` | 0x473d4 | no |
| SetCurrentThreadFsContext(const FsContext*) | `_ZN2nn2fs25SetCurrentThreadFsContextEPKNS0_9FsContextE` | 0x47448 | no |
| CommitSaveData(const char*) (solo per l'alternativa save, §1.5) | `_ZN2nn2fs14CommitSaveDataEPKc` | 0x503bc | sì (0xa803c0) |

### 1.2 Collegamento [VERIFICATO statico]
- `exlaunch/misc/specs/module.specs` collega con `--shared --export-dynamic` e senza `-z defs`, quindi i simboli non definiti restano
  importazioni dinamiche che il rtld del sistema risolve al caricamento, insieme a quelle del main e dell'sdk.
- **Non includere `nn/fs.hpp` di exlaunch.** Diversi overload di `ReadFile` vi sono dichiarati senza `size`; il file tira dentro
  `nn/svc.hpp` e `nn/time.hpp`, accanto a `lib/nx/kernel/svc.h` già incluso; e `GetEntryType`/`SetFileSize` non sono dichiarati.
  Conviene dichiarare in `main.cpp` le poche firme necessarie (§4.1). Le ho compilate con
  `clang++ -std=c++20 -target aarch64-none-elf -c` e controllate con `llvm-nm`: **tutti gli U mangled coincidono con le esportazioni
  dell'sdk**. Lo stesso controllo vale per il ctor dell'AbortDisabler e per SetCurrentThreadFsContext.
- `nn::Result` è restituito in w0 come intero di 4 B, quindi va dichiarato come `uint32_t`. `FileHandle` è una struct da 8 B passata
  in x; `WriteOption` è `{int flags}` passata per riferimento (`WriteOptionFlag_Flush = 1`). Modi di OpenFile: Read = 1, Write = 2,
  AllowAppend = 4.

### 1.3 Auto-abort dell'sdk [VERIFICATO statico, `p6_sdk_autoabort.asm`]
- `SetEnabledAutoAbort` (0x473a4) scrive un byte globale in .data **0x9429f8, che nel file vale 1**: l'auto-abort è attivo di default,
  e il main non importa `SetEnabledAutoAbort`.
- `IsAbortNeeded` (0x4b75c) usa l'handler del FsContext del thread. Quello di default (0x473b4) restituisce 2, cioè nessun abort, solo
  se l'auto-abort è spento. Altrimenti il risultato fa abortire, **tranne** quando il modulo è fs (2) **e** la descrizione è < 1000.
  Quindi PathNotFound (descr. 1) e PathAlreadyExists (2) passano. Fanno invece abortire la SD assente o inaccessibile (2001 e simili),
  PermissionDenied (circa 6400) e lo spazio esaurito.
- `ScopedAutoAbortDisabler()` (0x473d4) salva in `[this]` il contesto corrente (o quello di default, se è nullo) e imposta un contesto
  il cui handler (0x473cc) restituisce sempre 2. Il distruttore non è esportato perché è inline nell'header dell'SDK: lo riscriviamo
  noi come `SetCurrentThreadFsContext(m_prev)` (0x47448 = `SetTlsValue(slot, x0)`, simmetrico a 0x4741c). Il contesto è
  **per thread**, quindi non tocca le chiamate fs del gioco sugli altri thread.
  → Non usare `SetEnabledAutoAbort(false)`, che è globale.

### 1.4 Permessi del filesystem (npdm) [VERIFICATO npdm / IPOTESI forte per il bit]
- Nel `main.npdm` generato, sia la FAC dell'ACID sia la FAH dell'ACI0 valgono `0x4000000000040000`, cioè bit 62 (Debug) + bit 18
  (DeviceSaveData). Il bit 18 corrisponde all'uso di `MountDeviceSaveData("CommonSave")` a 0x8a077c, il che conferma la numerazione.
  npdmtool scrive gli stessi permessi in ACID e ACI0.
- `MountSdCardForDebug` e `MountSdCard` passano entrambe da fsp-srv `OpenSdCardFileSystem`. In FS sul firmware retail (Atmosphère)
  serve il bit **SdCard = 1<<21** [IPOTESI forte: numerazione Atmosphère `fssrv::AccessControlBits`]. Il bit Debug concede tutto solo
  quando il flag di debug di FS è attivo, cosa che sul retail non succede.
  → **Modifica a `config.json`** (fuori da src/): `"permissions": "0x4000000000240000"`. L'npdm è già rigenerato e non firmato (STATE.md:
  va bene per Eden e Atmosphère), quindi la modifica non aggiunge requisiti.
- Eden/yuzu non applica il controllo d'accesso FS e implementa `OpenSdCardFileSystem` su `user/sdmc` [IPOTESI forte, dal sorgente
  yuzu]. Il bit non è necessario in Eden, ma è innocuo.
- Se il mount fallisce comunque, l'AbortDisabler evita il crash: la funzione resta spenta per la sessione e il log scrive
  `p6: MountSdCardForDebug fallito r=...`.
- `MountSdCardForDebug` (0x6b6fc) è una semplice chiamata interna senza access log; `MountSdCard` (0x6b478) aggiunge l'access log.
  Scelgo **ForDebug**, la stessa usata dagli altri mod exlaunch e già dichiarata in `exlaunch/source/nn/fs/fs_mount.hpp`.

### 1.5 Alternativa: file nel salvataggio del gioco (scartata)
Il gioco monta `CommonSave` (device save, 0x8a077c), `UserSave` (utente, 0x8a07bc, file `userdata.dat`) e `SubSave0..7` (0x8a09b4,
tabella 0x12d2f48). Fa il commit con il FileDevice sead 0xa8039c, che chiama `CommitSaveData(name)` a 0xa803c0.
Si potrebbe creare `CommonSave:/mk8p8.bin` e poi chiamare `CommitSaveData("CommonSave")`. Rischi:
1. **Spazio e journal fissi** (dimensioni del NACP): `CreateFile`/`WriteFile` possono fallire con UsableSpaceNotEnough (descr. ≥ 1000,
   quindi abort senza disabler) o esaurire il journal del gioco.
2. **Commit concorrente**: `CommitSaveData` rende definitive *anche* le scritture pendenti del gioco. Se il thread di salvataggio sta
   scrivendo, si rischia di consolidare un file a metà, cioè di **corrompere il salvataggio**.
3. Il file estraneo finirebbe nei backup (JKSV/Checkpoint) e nel cloud, e resterebbe nel salvataggio dopo la rimozione della mod.
→ La SD è isolata: nel caso peggiore il nostro file è corrotto, il checksum se ne accorge e si torna ai default.

## 2. Quando caricare e quando salvare

### 2.1 Cosa persiste il gioco vanilla [VERIFICATO statico / IPOTESI forte]
- La cfg (oggetto menu 3, +0x30) è in RAM: la sottostruttura cfg+0x1fa4 viene costruita a ogni avvio da 0x3fd32c (ctor della cfg
  0x402ec4, poi 0x48adc0).
- **P1** (indice 0, account) viene salvato nella parola dell'utente: carrozzeria cfg+0x1fd4 ≤ 0x28 → `0x42581c(0)+0x2054` bit 6..11
  (0x4c3a70..0x4c3a84), più gomme e aliante subito dopo (0x4c3af4, 0x4c3b78). Gli aiuti vanno in save+0x13618 bit 26..31 (0x3fe0bc).
- **Ospiti** P2-P4: gli aiuti stanno in cfg+0x2310/+0x2320/+0x2330, personaggi e parti in cfg+0x1fb4.. Nessun codice li copia nel
  buffer di salvataggio: la ricerca degli offset immediati trova solo siti della selezione [IPOTESI forte]. **Quindi per gli ospiti il
  vanilla non conserva nulla al riavvio.** La persistenza di P5-P8 va oltre il vanilla; è voluta, perché è l'accettazione dello step.

### 2.2 Caricamento
L'ombra è letta o scritta solo dalla selezione personaggi (38 add `kCfgAdd`), dalla pagina kart, da 0x3fdd70 (aiuti) e dalla gara;
nessuna di queste è raggiungibile senza passare dal menu multigiocatore.
- **Punto principale**: `MenuEventHook` (0x4d92e8, parola `d105c3ff`, installato sempre in `exl_main`), al primo evento.
  - Gira sul thread principale.
  - Il salvataggio del gioco è già montato: 0x89fff8 (MountSaveData) gira prima del menu.
  - È sempre precedente alla scelta 5g-8g, perché il numero di giocatori si sceglie a ogni sessione: cfg+0x1fa4 è in RAM e il ctor lo
    mette a 1.
- **Rinforzi** (non fanno nulla dopo la prima chiamata): in testa a `ProfileToEntriesHook` (0x3fdd70, `d10203ff`), *prima* del
  `return` per n ≤ 4, e in testa a `CharselEnterHook` (0x4b74a4, `d10783ff`, installato solo con S5.4).
- **Mai a `exl_main`**: lì il gioco non ha ancora inizializzato nn::fs (allocatore, contesto) [IPOTESI forte].
- **Regola anti-sovrascrittura**: il caricamento si tenta una sola volta (`tried`). Se un `Save` arriva prima di qualunque
  caricamento, segna `tried = true` e da quel momento la RAM vince: il file non viene più ricaricato sopra dati più recenti.

### 2.3 Salvataggio
- **In coda ad `AidsSaveHook`** (0x3fe040, `d10283ff`), dopo l'aggiornamento di `aid8::code`, solo sul ramo n > 4 e `padui::active8`.
  Chiamanti [VERIFICATO]:
  - 0x4c3c1c in 0x4c3a04: conferma dei kart. A questo punto personaggi, varianti e parti di tutti gli 8 sono definitivi, e la stessa
    funzione scrive anche le parti di P1 nel salvataggio utente;
  - 0x535404 in 0x535358, metodo virtuale delle pagine gara (puntato da 0x12873f0/0x12879e0/0x1287cd8): fine gara, quindi dopo gli
    eventuali toggle degli aiuti in pausa. Nel log Eden compare una volta per gara (`step7_data/eden_log_7_hud_mappa_aiuti.txt`).
- **Scrittura solo se cambia**: l'immagine da 0x100 B viene confrontata con l'ultima letta o scritta; a ogni gara senza modifiche non
  c'è nessun I/O.
- [IPOTESI] Se si esce dalla gara dal menu di pausa, 0x535358 dovrebbe girare comunque (vt di uscita pagina). Se no, i toggle di
  quella gara si salvano alla gara successiva; personaggi e parti sono già salvati alla conferma dei kart.
- Non salvo in `CharselExitHook` (0x4bb6a0): si torna indietro anche senza confermare, e la conferma vera è la 0x4c3c1c.
  Facoltativo: aggiungere lì `p6::Save("selezione")`, che non costa nulla grazie al confronto.

## 3. Formato del file `sd:/mk8dx-8p/p58.bin` (0x100 B, little endian)

| off | tipo | campo |
|---|---|---|
| 0x00 | u32 | magic `'M8P8'` = 0x3850384d |
| 0x04 | u16 | versione = 1 |
| 0x06 | u16 | dimensione = 0x100 |
| 0x08 | s32[13][4] | `cfg8::sh`: [k][i-4], k = (off_cfg − 0x1fb4)/0x10 (0 personaggio, 1 variante, 2 carrozzeria, 3 gomme, 4 aliante, 5..12 = +0x2004..+0x2074) |
| 0xd8 | u8[3][4] | `aid8::code`: [0] Y, [1] L, [2] R per P5..P8; 0 = default, 1 = acceso, 2 = spento |
| 0xe4 | u8[0x18] | riservato (0) |
| 0xfc | u32 | FNV-1a 32 di 0x00..0xfb |

Gestione degli errori:
- File assente (PathNotFound, non fa abortire): si tengono i default (ombra a zero) e il file si crea al primo salvataggio.
- Magic, versione, dimensione o checksum errati, oppure lettura corta: si ignora il file (log) e alla prossima occasione lo si riscrive.
- Controllo dei valori dopo la lettura:
  - `sh`: valori accettati −1..255, altrimenti 0 (−1 = non impostato, come in 0x4b75fc).
  - `aid`: valori 0..2, altrimenti 0.
  - La convalida del gioco (0x4b75ec, a ogni ingresso nella selezione) riporta comunque a 0 i personaggi non validi.
- Mount fallito: la funzione resta spenta per la sessione, con un solo log.
- Tutte le chiamate sono sotto `ScopedAutoAbortDisabler`, quindi nessun abort.
- Scrittura: si sovrascrive nello stesso file (Open Write|AllowAppend, SetFileSize 0x100, WriteFile con Flush). Un'interruzione a metà
  lascia un checksum errato e quindi il ritorno ai default: niente tmp+rename, perché `RenameFile` fallisce se la destinazione esiste.
- Gli altri dati in ombra della mod (`cur8::mem8` cursori per pagina, `cur8::foc` focus) sono solo stato UI e non vanno persistiti; i
  0x18 B riservati lasciano spazio per una versione 2.

## 4. Codice (stile main.cpp)

### 4.1 Dichiarazioni nn::fs (dopo gli `#include`, a livello globale)
```cpp
/* ---- step 5b passo 6: nn::fs dal modulo sdk (simboli verificati: step5b_data (dati non pubblicati)/p6_fs_symbols.txt) ---- */
namespace nn::fs {
    struct FileHandle  { void* handle; };
    struct WriteOption { int flags; };                       /* 1 = Flush */
    struct FsContext;
    uint32_t MountSdCardForDebug(const char* name);          /* sdk 0x6b6fc */
    uint32_t CreateDirectory(const char* path);              /* sdk 0x4f610 */
    uint32_t CreateFile(const char* path, long size);        /* sdk 0x4f4ac */
    uint32_t OpenFile(FileHandle* out, const char* path, int mode);   /* 1 Read, 2 Write, 4 AllowAppend */
    uint32_t ReadFile(unsigned long* outRead, FileHandle h, long off, void* buf, unsigned long size);
    uint32_t WriteFile(FileHandle h, long off, const void* buf, unsigned long size, const WriteOption& opt);
    uint32_t GetFileSize(long* out, FileHandle h);
    uint32_t SetFileSize(FileHandle h, long size);
    void     CloseFile(FileHandle h);
    void     SetCurrentThreadFsContext(const FsContext* ctx);   /* sdk 0x47448 */
    class ScopedAutoAbortDisabler {                          /* ctor sdk 0x473d4 salva il contesto in [this]; dtor inline nell'SDK */
        const FsContext* m_prev; uint64_t m_pad;
    public:
        ScopedAutoAbortDisabler();
        ~ScopedAutoAbortDisabler() { SetCurrentThreadFsContext(m_prev); }
    };
}
```

### 4.2 Dichiarazione anticipata (prima di `MenuEventHook`, riga ~1051, vicino a `namespace cur8 { static void Reset(); }`)
```cpp
namespace p6 { static void EnsureLoaded(); static void Save(const char* why); }   /* step 5b passo 6, definito piu' avanti */
```

### 4.3 Modulo p6 (subito dopo `namespace cfg8 { ... }`, riga ~2353: richiede cfg8::sh e aid8::code)
```cpp
/* ---- step 5b passo 6 (step5b_plan_p6.md): cfg8::sh e aid8::code persistenti su SD ------------------- */
namespace p6 {
    static constexpr const char* kMount = "mk8p8sd";
    static constexpr const char* kDir   = "mk8p8sd:/mk8dx-8p";
    static constexpr const char* kPath  = "mk8p8sd:/mk8dx-8p/p58.bin";
    static constexpr uint32_t kMagic = 0x3850384d;            /* 'M8P8' */
    struct File {
        uint32_t magic; uint16_t version, size;
        int32_t  sh[13 * 4];                                  /* 0x08 */
        uint8_t  aid[3][4];                                   /* 0xd8 */
        uint8_t  reserved[0x18];                              /* 0xe4 */
        uint32_t sum;                                         /* 0xfc FNV-1a di 0x00..0xfb */
    };
    static_assert(sizeof(File) == 0x100 && offsetof(File, aid) == 0xd8 && offsetof(File, sum) == 0xfc, "p6::File");
    static_assert(sizeof(cfg8::sh) == sizeof(File::sh) && sizeof(aid8::code) == sizeof(File::aid), "p6 ombre");
    static bool tried, mounted, disabled;
    static File last;                                         /* ultima immagine letta/scritta (zero = nessuna) */

    static uint32_t Fnv(const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p; uint32_t h = 0x811c9dc5;
        for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x01000193; }
        return h;
    }
    static void Build(File& f) {
        memset(&f, 0, sizeof(f));
        f.magic = kMagic; f.version = 1; f.size = sizeof(File);
        memcpy(f.sh, cfg8::sh, sizeof(f.sh)); memcpy(f.aid, aid8::code, sizeof(f.aid));
        f.sum = Fnv(&f, offsetof(File, sum));
    }
    static bool Mount() {                                     /* chiamare sotto ScopedAutoAbortDisabler */
        if (mounted) return true;
        if (disabled) return false;
        uint32_t r = nn::fs::MountSdCardForDebug(kMount);
        if (r) { disabled = true;
                 Logging.Log("[mk8dx-8p] p6: MountSdCardForDebug fallito r=0x%x (modulo %u descr %u): persistenza 5-8 spenta", r, r & 0x1ff, (r >> 9) & 0x1fff);
                 return false; }
        mounted = true; return true;
    }
    static void EnsureLoaded() {                              /* una volta, prima di ogni uso dell'ombra */
        if (tried) return;
        tried = true;
        nn::fs::ScopedAutoAbortDisabler na;
        if (!Mount()) return;
        nn::fs::FileHandle h; uint32_t r = nn::fs::OpenFile(&h, kPath, 1);
        if (r) { Logging.Log("[mk8dx-8p] p6: %s assente (r=0x%x): P5-P8 ai valori di default", kPath, r); return; }
        File f; unsigned long got = 0; long sz = 0;
        nn::fs::GetFileSize(&sz, h);
        r = nn::fs::ReadFile(&got, h, 0, &f, sizeof(f));
        nn::fs::CloseFile(h);
        if (r || got != sizeof(f) || sz != (long)sizeof(f) || f.magic != kMagic || f.version != 1 || f.size != sizeof(f)
            || f.sum != Fnv(&f, offsetof(File, sum))) {
            Logging.Log("[mk8dx-8p] p6: file non valido (r=0x%x letti=%lu dim=%ld magic=%x ver=%u), ignorato", r, got, sz, f.magic, f.version);
            return;                                           /* last resta a zero: verra' riscritto al prossimo salvataggio */
        }
        for (int k = 0; k < 13 * 4; k++) { int32_t v = f.sh[k]; cfg8::sh[k] = (v >= -1 && v <= 0xff) ? v : 0; }
        for (int a = 0; a < 3; a++) for (int i = 0; i < 4; i++) aid8::code[a][i] = f.aid[a][i] <= 2 ? f.aid[a][i] : 0;
        Build(last);
        Logging.Log("[mk8dx-8p] p6: caricati P5-P8 da SD: pers={%d,%d,%d,%d} carr={%d,%d,%d,%d} Y={%d,%d,%d,%d} L={%d,%d,%d,%d} R={%d,%d,%d,%d}",
                    cfg8::sh[0], cfg8::sh[1], cfg8::sh[2], cfg8::sh[3], cfg8::sh[8], cfg8::sh[9], cfg8::sh[10], cfg8::sh[11],
                    aid8::code[0][0], aid8::code[0][1], aid8::code[0][2], aid8::code[0][3], aid8::code[1][0], aid8::code[1][1],
                    aid8::code[1][2], aid8::code[1][3], aid8::code[2][0], aid8::code[2][1], aid8::code[2][2], aid8::code[2][3]);
    }
    static void Save(const char* why) {
        tried = true;                                         /* da qui in poi la RAM e' la fonte: mai ricaricare sopra */
        if (disabled) return;
        File f; Build(f);
        if (!memcmp(&f, &last, sizeof(f))) return;           /* niente di nuovo: nessun I/O */
        nn::fs::ScopedAutoAbortDisabler na;
        if (!Mount()) return;
        nn::fs::CreateDirectory(kDir);                        /* PathAlreadyExists ignorato */
        nn::fs::FileHandle h; uint32_t r = nn::fs::OpenFile(&h, kPath, 2 | 4);
        if (r) { nn::fs::CreateFile(kPath, sizeof(f)); r = nn::fs::OpenFile(&h, kPath, 2 | 4); }
        if (r) { Logging.Log("[mk8dx-8p] p6: apertura in scrittura di %s fallita r=0x%x (%s)", kPath, r, why); return; }
        nn::fs::SetFileSize(h, sizeof(f));
        nn::fs::WriteOption wo{1};
        r = nn::fs::WriteFile(h, 0, &f, sizeof(f), wo);
        nn::fs::CloseFile(h);
        if (r) { Logging.Log("[mk8dx-8p] p6: scrittura fallita r=0x%x (%s)", r, why); return; }
        last = f;
        Logging.Log("[mk8dx-8p] p6: salvati P5-P8 su SD (%s)", why);
    }
}
```

### 4.4 Modifiche agli hook esistenti
```cpp
/* MenuEventHook (0x4d92e8, d105c3ff), prima riga di Callback: */
        p6::EnsureLoaded();
/* ProfileToEntriesHook (0x3fdd70, d10203ff), prima riga di Callback (prima del return per n <= 4): */
        p6::EnsureLoaded();
/* CharselEnterHook (0x4b74a4, d10783ff), prima riga di Callback: */
        p6::EnsureLoaded();
/* AidsSaveHook (0x3fe040, d10283ff), dopo il Logging.Log finale (ramo n > 4 && padui::active8): */
        p6::Save("0x3fe040");      /* chiamante (conferma kart 0x4c3c1c / fine gara 0x535404) dal log precedente */
```
Serve `#include <cstring>` e `<cstddef>` (memcpy/memcmp/offsetof) se non sono già inclusi tramite lib.hpp: main.cpp usa già
memcpy/memset, quindi [IPOTESI forte] che ci siano.

### 4.5 Fuori da src/
- `config.json`: `"filesystem_access": { "permissions": "0x4000000000240000" }`, che aggiunge SdCard (bit 21). È necessario per
  Atmosphère; in Eden è innocuo.
- Nessun nuovo hook e nessuna parola da verificare in `exl_main`: si riusano 4 hook già installati, le cui prime parole sono state
  ricontrollate (`p6_hooks.txt`). Il pool dei trampolini non cambia.

## 5. Rischi residui
- [IPOTESI] Thread: tutti i punti di aggancio girano sul thread principale dei menu e della gara. Le chiamate fs sono sincrone, su un
  file da 0x100 B scritto solo quando cambia, quindi pochi ms sulla SD.
- [IPOTESI] Ospiti con account in posizione 5-8: il vanilla scriverebbe gli aiuti nella parola dell'account, mentre noi li teniamo
  nell'ombra e nel file. È coerente con lo step 7.
- Il file vale per tutto il dispositivo (non per account), come gli ospiti P2-P4 nella cfg.
- Se la mod viene rimossa, resta solo `sd:/mk8dx-8p/p58.bin`; il salvataggio del gioco non è stato toccato.

## 6. Verifica proposta (Eden)
1. Prima esecuzione: `p6: ... assente` nel log. Fare 8 giocatori, scegliere personaggi e kart diversi per P5-P8, attivare o disattivare
   gli aiuti in pausa, finire la gara. Nel log devono comparire `p6: salvati ... (0x3fe040)` e il file
   `switch_env/eden/user/sdmc/mk8dx-8p/p58.bin` di 256 B (`xxd`: magic `4d 38 50 38`).
2. Riavviare Eden e tornare a 8 giocatori. Nel log: `p6: caricati P5-P8 da SD: pers={...}` con gli stessi valori. Nella selezione
   personaggi i cursori o le icone di P5-P8 devono stare sui personaggi salvati, nella pagina kart le parti (log `C: sel4..7 cfg=`)
   e in gara gli aiuti (log `aid8: caricati aiuti 5-8 ... R=`).
3. Corruzione: alterare un byte del file, riavviare, verificare il log `file non valido` e che il gioco non vada in crash.
4. Atmosphère, se e quando si proverà: senza il bit 21 deve comparire `MountSdCardForDebug fallito` (senza crash); con il bit, il file
   in `sd:/mk8dx-8p/`.
