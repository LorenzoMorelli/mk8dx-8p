#include "lib.hpp"
#include "lib/nx/kernel/svc.h"
#include <cstddef>
#include <cstring>

/* ---- step 5b passo 6: nn::fs dal modulo sdk (simboli verificati: report/step5b_data/p6_fs_symbols.txt) ---- */
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

/* mk8dx-8p – step 2A: strumentazione (nessuna modifica di comportamento).
 *  - log della geometria viewport/scissor NVN (puntatori risolti da 0x582c94 in [[0x130b2b8..2d0]])
 *  - dump della Scene dopo l'init di gara (0x7e76b8) e dell'init generico (0x7e7950)
 *  - catena dei chiamanti (fp/lr) per trovare il ctor della Scene
 * Offset = main 4.0.1 (BID 158C0C73...). */

namespace mk8dx {
    constexpr uintptr_t MenuEventHandler      = 0x4d92e8;   /* sub sp,sp,#0x170 */
    constexpr uintptr_t NvnProcLoader         = 0x582c94;   /* (device, nvnDeviceGetProcAddress) */
    constexpr uintptr_t SceneInitRace         = 0x7e76b8;   /* (Scene*) caso "gara" */
    constexpr uintptr_t SceneInit             = 0x7e7950;   /* (Scene*, int* type) */
    constexpr uintptr_t GetSettingsA          = 0x86f7bc;   /* () -> settings* */
    constexpr uintptr_t GetLocalPlayerCount   = 0x86f154;   /* (settings*) -> int */
    /* Il loader (0x582c94) salva il risultato di GetProcAddress(nomeK) nella slot letta subito
     * dopo la chiamata: slot[0x130b2c0] <- "nvnCommandBufferSetViewport" (slot reale 0x1346008),
     * [0x130b2c8] <- SetViewports, [0x130b2d8] <- SetScissor, [0x130b2e0] <- SetScissors. */
    constexpr uintptr_t SlotNvnSetViewport    = 0x130b2c0;  /* -> ptr alla variabile pfnc_ */
    constexpr uintptr_t SlotNvnSetViewports   = 0x130b2c8;
    constexpr uintptr_t SlotNvnSetScissor     = 0x130b2d8;
    constexpr uintptr_t SlotNvnSetScissors    = 0x130b2e0;
    constexpr uintptr_t RlmSyncFromScene      = 0x3cbd94;   /* (mgr) : Scene -> slot vista -> rect idx */
    constexpr uintptr_t LayerSystemGlobal     = 0x12fa5e0;  /* -> layerSystem (+0x40 count, +0x48 ptr[]) */
    constexpr uintptr_t RectModeTable         = 0x133eacc;  /* int [modo][4 slot] -> indice rect */
    constexpr uint32_t  MenuEventHandlerInsn  = 0xd105c3ff;
    constexpr uint32_t  SceneInitRaceInsn     = 0xd10283ff; /* sub sp,sp,#0xa0 */

    inline uintptr_t Base() { return exl::util::GetMainModuleInfo().m_Total.m_Start; }
    template<typename T> T Fn(uintptr_t off) { return reinterpret_cast<T>(Base() + off); }
    inline uintptr_t Rel(uintptr_t addr) { /* indirizzo -> offset nel main (0 se fuori) */
        auto& m = exl::util::GetMainModuleInfo().m_Total;
        return (addr >= m.m_Start && addr < m.GetEnd()) ? addr - m.m_Start : 0;
    }

    /* Catena dei chiamanti via frame pointer (il gioco è compilato con x29). */
    static void LogBacktrace(const char* tag, void* fp, int max = 8) {
        for (int i = 0; i < max && fp; i++) {
            uintptr_t* f = reinterpret_cast<uintptr_t*>(fp);
            uintptr_t lr = f[1];
            if (lr == 0) break;
            Logging.Log("[mk8dx-8p] %s bt[%d] lr=main+%lx (abs %lx)", tag, i, Rel(lr), lr);
            fp = reinterpret_cast<void*>(f[0]);
        }
    }
}

/* ---- step 4: flusso menu 5-8 (report/step4_menu_plan.md) ---------------------------- */
namespace menu8 {
    /* config menu: 0x8bf0b0(&3) -> oggetto, +0x30 = config (come 0x4d9984-0x4d99a4) */
    static uint8_t* Cfg() {
        int id = 3;
        uint8_t* o = mk8dx::Fn<uint8_t* (*)(int*)>(0x8bf0b0)(&id);
        return o ? *(uint8_t**)(o + 0x30) : nullptr;
    }
    static int LocalCount() { uint8_t* c = Cfg(); return c ? *(int32_t*)(c + 0x1fa4) : 0; }
    static int Mode()       { uint8_t* c = Cfg(); return c ? *(int32_t*)(c + 0x1fa8) : -1; }
    /* sead::ControllerMgr: GOT main+0x12fbcb0 -> variabile -> mgr */
    static uint8_t* CtrlMgr() {
        uintptr_t* var = *(uintptr_t**)(mk8dx::Base() + 0x12fbcb0);
        return var ? (uint8_t*)*var : nullptr;
    }
    static bool NpadConnected(int npad) { /* NpadController di gioco (step 3: npad 4..7 in pos 6..9) */
        uint8_t* mgr = CtrlMgr(); if (!mgr) return false;
        int n = *(int32_t*)(mgr + 0x188); uint8_t** list = *(uint8_t***)(mgr + 0x190);
        for (int p = 0; p < n; p++) {
            uint8_t* c = list[p];
            if (*(int32_t*)(c + 0x138) == 0x11 && *(int32_t*)(c + 0x178) == npad) return c[0x17c] & 1;
        }
        return false;
    }
}

namespace race8 {
    inline uint8_t* RaceMgr() { return mk8dx::Fn<uint8_t* (*)()>(0x8ace28)(); }
    inline uint8_t* Pim()     { return mk8dx::Fn<uint8_t* (*)()>(0x8ace0c)(); }
    inline int32_t* Locals(uint8_t* rm) { return reinterpret_cast<int32_t*>(rm + 0x17c); }   /* locale -> entry id */
    inline int32_t* Slots(uint8_t* rm)  { return reinterpret_cast<int32_t*>(rm + 0x19c); }   /* slot -> entry id (12) */
    inline uint8_t* EntryById(uint8_t* rm, int32_t id) {
        if (id < 0 || (uint32_t)id >= *(uint32_t*)(rm + 0x50)) return nullptr;
        return ((uint8_t**)*(uintptr_t*)(rm + 0x58))[id];
    }
    inline uint8_t* Entry(uint8_t* rm, int local) { return (local >= 0 && local < 8) ? EntryById(rm, Locals(rm)[local]) : nullptr; }
    inline int PimCount(uint8_t* p) { return *(int32_t*)(p + 0x50); }
    inline uint8_t* Player(uint8_t* p, int i) { return ((uint8_t**)*(uintptr_t*)(p + 0x58))[i]; }
    static void* pl[8];          /* buffer esterno della FixedPtrArray PIM+0x50 (G1) */
    static bool  pim8;           /* G1/G3 riusciti: giocatori 4..7 esistono */
    static bool  mapOk;          /* patch B1 riuscite: mappa slot a +0x19c */
    static void LogLocals(const char* tag) {
        uint8_t* rm = RaceMgr(); int32_t* l = Locals(rm); int32_t* s = Slots(rm);
        Logging.Log("[mk8dx-8p] B %s: race+0x178=%d +0x17c={%d,%d,%d,%d,%d,%d,%d,%d} slot={%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d} entry=%d",
                    tag, *(int32_t*)(rm + 0x178), l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7],
                    s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8], s[9], s[10], s[11], *(int32_t*)(rm + 0x50));
    }
}

namespace rlm { static unsigned long calls; static int lastMode = -1; static uint8_t* mgr; static int sceneType = -1; /* tipo passato a SceneInit (3 = gara); Scene+0xd4 torna a 0 dopo l'init */ }

/* ---- geometria viewport / scissor ------------------------------------------------- */
namespace geom {
    using SetViewportFn  = void (*)(void* cmd, int x, int y, int w, int h);
    using SetViewportsFn = void (*)(void* cmd, int first, int count, const float* ranges);
    using SetScissorFn   = void (*)(void* cmd, int x, int y, int w, int h);
    using SetScissorsFn  = void (*)(void* cmd, int first, int count, const int* rects);
    static SetViewportFn  origSetViewport;
    static SetViewportsFn origSetViewports;
    static SetScissorFn   origSetScissor;
    static SetScissorsFn  origSetScissors;

    struct Rect { int x, y, w, h; };
    static Rect seenVp[64], seenSc[64];
    static int nVp, nSc;
    static unsigned long callsVp, callsVps, callsSc;

    static bool Remember(Rect* tab, int& n, Rect r) {
        for (int i = 0; i < n; i++)
            if (tab[i].x == r.x && tab[i].y == r.y && tab[i].w == r.w && tab[i].h == r.h) return false;
        if (n >= 64) return false;   /* tabella piena: smette di loggare (prima loggava ogni chiamata -> gara a 4 fps) */
        tab[n++] = r;
        return true;
    }
    static int nBt;
    static void DumpLayerCams() {
        uint8_t* mgr = rlm::mgr;
        if (!mgr) return;
        uint8_t** sub = *reinterpret_cast<uint8_t***>(mgr + 0x128);
        for (int i = 0; i < 4 && sub; i++) {
            uint8_t* ly = sub[i]; float* r = reinterpret_cast<float*>(ly + 0x30);
            uint8_t* c = *(uint8_t**)(ly + 0x68);
            float* m = reinterpret_cast<float*>(c);
            Logging.Log("[mk8dx-8p] frame sub3d[%d] on=%x rect=%.0f,%.0f,%.0f,%.0f cam68=%p vt=main+%lx f[2..13]=%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f",
                        i, *(uint32_t*)(ly + 0x78), r[0], r[1], r[2], r[3], c, c ? mk8dx::Rel(*(uintptr_t*)c) : 0,
                        m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13]);
        }
    }
    static void MySetViewport(void* cmd, int x, int y, int w, int h) {
        callsVp++;
        if (callsVp % 200000 == 0) DumpLayerCams();
        if (Remember(seenVp, nVp, {x, y, w, h})) {
            Logging.Log("[mk8dx-8p] nvnSetViewport nuovo rect #%d: x=%d y=%d w=%d h=%d (call %lu)", nVp, x, y, w, h, callsVp);
            if ((x != 0 || y != 0) && nBt < 0) { nBt++; mk8dx::LogBacktrace("vp", __builtin_frame_address(0), 12); }
        }
        origSetViewport(cmd, x, y, w, h);
    }
    static void MySetViewports(void* cmd, int first, int count, const float* r) {
        callsVps++;
        for (int i = 0; i < count && i < 8; i++) {
            Rect q{(int)r[i*4], (int)r[i*4+1], (int)r[i*4+2], (int)r[i*4+3]};
            if (Remember(seenVp, nVp, q))
                Logging.Log("[mk8dx-8p] nvnSetViewports[%d+%d] nuovo rect #%d: x=%d y=%d w=%d h=%d (call %lu)", first, i, nVp, q.x, q.y, q.w, q.h, callsVps);
        }
        origSetViewports(cmd, first, count, r);
    }
    static void MySetScissors(void* cmd, int first, int count, const int* r) {
        for (int i = 0; i < count && i < 8; i++)
            if (Remember(seenSc, nSc, {r[i*4], r[i*4+1], r[i*4+2], r[i*4+3]}))
                Logging.Log("[mk8dx-8p] nvnSetScissors[%d+%d] nuovo rect #%d: x=%d y=%d w=%d h=%d", first, i, nSc, r[i*4], r[i*4+1], r[i*4+2], r[i*4+3]);
        origSetScissors(cmd, first, count, r);
    }
    static void MySetScissor(void* cmd, int x, int y, int w, int h) {
        callsSc++;
        if (Remember(seenSc, nSc, {x, y, w, h}))
            Logging.Log("[mk8dx-8p] nvnSetScissor nuovo rect #%d: x=%d y=%d w=%d h=%d (call %lu)", nSc, x, y, w, h, callsSc);
        origSetScissor(cmd, x, y, w, h);
    }

    template<typename F> static void Swap(uintptr_t slotOff, F& orig, F mine, const char* name) {
        F* slot = *reinterpret_cast<F**>(mk8dx::Base() + slotOff);
        if (!slot || !*slot) { Logging.Log("[mk8dx-8p] %s: slot vuoto", name); return; }
        orig = *slot; *slot = mine;
        Logging.Log("[mk8dx-8p] %s agganciato (orig %p)", name, (void*)orig);
    }
}

HOOK_DEFINE_TRAMPOLINE(NvnLoaderHook) {
    static void Callback(void* device, void* getProc) {
        Orig(device, getProc);
        geom::Swap(mk8dx::SlotNvnSetViewport,  geom::origSetViewport,  geom::MySetViewport,  "nvnCommandBufferSetViewport");
        geom::Swap(mk8dx::SlotNvnSetViewports, geom::origSetViewports, geom::MySetViewports, "nvnCommandBufferSetViewports");
        geom::Swap(mk8dx::SlotNvnSetScissor,   geom::origSetScissor,   geom::MySetScissor,   "nvnCommandBufferSetScissor");
        geom::Swap(mk8dx::SlotNvnSetScissors,  geom::origSetScissors,  geom::MySetScissors,  "nvnCommandBufferSetScissors");
    }
};

/* ---- Scene --------------------------------------------------------------------------- */
static void DumpScene(const char* tag, uint8_t* s) {
    int32_t* vp = reinterpret_cast<int32_t*>(s + 0x3c);
    int32_t* tab = reinterpret_cast<int32_t*>(s + 0xb0);
    Logging.Log("[mk8dx-8p] %s scene=%p modo=%d d0=%d tipo=%d slot->vp={%d,%d,%d,%d}", tag, s,
                *(int32_t*)(s + 0xcc), *(int32_t*)(s + 0xd0), *(int32_t*)(s + 0xd4), tab[0], tab[1], tab[2], tab[3]);
    for (int i = 0; i < 5; i++)
        Logging.Log("[mk8dx-8p]   vp[%d] slot=%d attivo=%d tipo=%d f3=%d kart=%d id=%d", i,
                    vp[i*6], vp[i*6+1], vp[i*6+2], vp[i*6+3], vp[i*6+4], vp[i*6+5]);
}

/* ---- step 6 N1 (report/step6_plan_B.md §1.5): con n>4 0x7e76b8 cicla n volte su 5 viewport (clamp) e sporca vp0/+0xb0.
 * Qui la Scene viene riscritta come un 4P coerente (locali 0..3 -> viewport 0..3, modo 2); i locali 4..7 vivono nel
 * modulo (view8::kartOf) e sono mostrati dalle viste extra dello step 2 (N2/N3). Con n<=4 nulla cambia. */
namespace view8 {
    struct Vp { int32_t idx, slot, active, type, locSlot, kart; };       /* Scene+0x38 + i*0x18 */
    static int nLocal, split;                                             /* split 0 = vanilla, 6 o 8 */
    static int kartOf[8];                                                 /* locale -> kart (indice descrittore) */
}
/* ---- step 7c (report/step7c_spettatore.md, opzione a): cella vuota con 5/7 locali come la quarta cella del 3P originale.
 * Il gestore dei controller di camera (sys+0x248) crea 5 controller invece di 4 (P1 0xcf040); con 5 o 7 locali vp4 della
 * Scene e' un viewport di tipo 2 (P4) e 0xcf224 mette il controller 4 in modo 1 (sotto-camera spettatore obj+0x118).
 * Il director ha solo 4 pile (clamp vp>=4 -> 0): per il controller 4 si saltano spinta/rimozione e la scrittura di
 * RLM slot+0x1b (P2/P3); la mod copia la camera calcolata dal controller nella cella vuota (P6). */
namespace spect {
    static uint8_t* ctrl;                                                 /* controller con vp 4 (ctor 0xcc834) */
    static constexpr uintptr_t kVt = 0x11b8d50;                           /* vtable dei controller (0x11b8d40 + 0x10) */
    static bool Valid(uint8_t* o) {
        return o && *(uintptr_t*)o == mk8dx::Base() + kVt && *(int32_t*)(o + 8) == 4;
    }
}
HOOK_DEFINE_TRAMPOLINE(CamCtrlCtorHook) {                 /* P5 0xcc834(obj, vp): ctor del controller di camera */
    static void Callback(uint8_t* o, int32_t vp) {
        Orig(o, vp);
        if (*(int32_t*)(o + 8) == 4) { spect::ctrl = o; Logging.Log("[mk8dx-8p] 7c: controller di camera 4 = %p", o); }
    }
};
namespace spect {
    /* vista del renderer della cella vuota: cella 4+j = vista 4+j, la cella vuota e' la nLocal-esima (5P -> 5, 7P -> 7) */
    static int View() { return view8::split && (view8::nLocal & 1) ? view8::nLocal : -1; }
}
#ifndef MK8DX_NO7CFIX
#define MK8DX_NO7CFIX 0   /* prova (bisezione verde): 1 = senza ScreenFxFlagHook 0x1088c8, 2 = senza CamCtrlSphereHook 0xcdd18 */
#endif
HOOK_DEFINE_TRAMPOLINE(CamCtrlSphereHook) {               /* 0xcdd18(obj): sfera della camera -> 0x19d3e8(attore, 1/0, vp) nasconde gli
                                                             oggetti vicini nella vista vp (0xcdd54/0xcdef0/0xcdf84); per il controller 4 la
                                                             vista e' quella della cella vuota, non la 4 (= cella di P5) */
    static void Callback(uint8_t* o) {
        int32_t vp = *(int32_t*)(o + 8);
        if (vp < 4) { Orig(o); return; }
        int v = spect::View(); if (v < 0) return;
        *(int32_t*)(o + 8) = v; Orig(o); *(int32_t*)(o + 8) = vp;
    }
};
HOOK_DEFINE_TRAMPOLINE(ScreenFxFlagHook) {                /* 0x1088c8(sys+0x268, vp): flag per vista con csel sulla voce 0 se vp >= count:
                                                             con vp 4 finirebbe su P1; la cella vuota non ha effetti a schermo. Si rifa' solo il
                                                             conteggio finale (tail call 0x108388) */
    static void Callback(uint8_t* self, int32_t vp) {
        if (vp >= 4) { mk8dx::Fn<void (*)(uint8_t*)>(0x108388)(self); return; }
        Orig(self, vp);
    }
};
HOOK_DEFINE_INLINE(SpectAutoHook) {                       /* 0x86ca88 `cmp w8,#3` (w8 = settings+0x184): con 3 il gioco chiama 0x7d4c94(sys+0x228, 3)
                                                             (0x86cbc8) -> ctrl[3]+0xf4 = 1 e scelta del kart (vt+0x48): lo spettatore cambia kart
                                                             seguito. Stesso flusso con 5/7 locali per il controller 4 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        int n = (int)ctx->W[8];
        static int logged; if (logged < 4) { logged++; Logging.Log("[mk8dx-8p] 7c: settings+0x184 = %d (split %d, locali %d)", n, view8::split, view8::nLocal); }
        if (!(n == 5 || n == 7) || spect::View() < 0 || !spect::Valid(spect::ctrl)) return;
        uint8_t* g = mk8dx::Fn<uint8_t* (*)()>(0x7e7434)(); if (!g) return;
        uint8_t* sys = *(uint8_t**)(g + 0x190 + 4 * 8); if (!sys) return;           /* come 0x86ca98..0x86cac0 */
        uint8_t* mgr = *(uint8_t**)(sys + 0x228); if (!mgr) return;
        if (*(int32_t*)(mgr + 0x38) <= 4 || (*(uint8_t***)(mgr + 0x40))[4] != spect::ctrl) {
            Logging.Log("[mk8dx-8p] 7c: gestore %p senza controller 4 (count %d)", mgr, *(int32_t*)(mgr + 0x38)); return; }
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x7d4c94)(mgr, 4);
        Logging.Log("[mk8dx-8p] 7c: scelta automatica del kart attivata per il controller 4 (f4=%d)", spect::ctrl[0xf4]);
    }
};
HOOK_DEFINE_TRAMPOLINE(CamCtrlAttachHook) {               /* P2 0xce29c(obj): aggancio al director (0xc9074) + slot+0x1b */
    static void Callback(uint8_t* o) {
        if (*(int32_t*)(o + 8) >= 4) return;                  /* niente pila di vp0 (clamp) ne' RLM slot[0]+0x1b */
        Orig(o);
    }
};
HOOK_DEFINE_TRAMPOLINE(CamCtrlDetachHook) {               /* P3 0xce35c(obj): sgancio; per vp>=4 solo la parte senza director/RLM */
    static void Callback(uint8_t* o) {
        int32_t vp = *(int32_t*)(o + 8);
        if (vp < 4) { Orig(o); return; }
        *(uint64_t*)(o + 0xe8) = 0;                           /* come 0xce3b0 */
        int32_t n = *(int32_t*)(o + 0x170); void** v = *(void***)(o + 0x178);
        int view = spect::View();                             /* stessa vista usata in CamCtrlSphereHook */
        if (view >= 0) for (int i = 0; i < n && v; i++) mk8dx::Fn<void (*)(void*, int, int)>(0x19d3e8)(v[i], 0, view);   /* come 0xce3d8..0xce3fc */
        *(int32_t*)(o + 0x170) = 0;
    }
};
/* ---- step 6 H1: HUD 6P/8P per le 4 pagine RaceView (report/step6_plan_B.md §2.2) --------------------------------------
 * Nome layout (vtable slot 13: 0x54798c RaceView, 0x548d20 Front) scelto da Scene+0xcc (0/1/2 -> 1P/2P/4P): con split
 * 6/8 si restituiscono i layout 6P/8P della RomFS (stringhe del modulo). Partizioni del layout comune (0x536a10):
 * P_Partition_y3l/y3r (6P) e y4l/y4r (8P), nomi assenti nel main. I layer 2D Sub0..3 (slot 6..9) prendono il rect
 * della cella i in proto::PerFrame; per l'8P (layout autorato 640x720 per una cella 320x360) la camera dei layer 2D
 * viene allontanata x2. */
namespace hud8 {
    static constexpr const char* kView6 = "rc_RaceView_6P_Ml", * kView8 = "rc_RaceView_8P_Ml", * kFront6 = "rc_RaceView_Front_6P_Ml";
    static constexpr const char* kPartY = "P_Partition_y", * kPartX = "P_Partition_x";
    static constexpr const char* kPart6[] = { "P_Partition_y3l", "P_Partition_y3r" };
    static constexpr const char* kPart8[] = { "P_Partition_y4l", "P_Partition_y4r" };
    struct SafeString { uintptr_t vptr; const char* str; };
    static SafeString Str(const char* c) { return { *(uintptr_t*)(mk8dx::Base() + 0x12f9e28) + 0x10, c }; }
    static void SetVisible(uint8_t* root, const char* name, bool on) {
        SafeString ss = Str(name);
        uint8_t* pane = mk8dx::Fn<uint8_t* (*)(uint8_t*, SafeString*)>(0x8ad8e8)(root, &ss);
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x8ade74)(pane, on);
        Logging.Log("[mk8dx-8p] hud8: %s -> %p visibile=%d", name, pane, (int)on);
    }
}
HOOK_DEFINE_REPLACE(RaceViewNameHook) {                  /* 0x54798c: const char* (page) */
    static const char* Callback(void*) {
        uint8_t* sc = mk8dx::Fn<uint8_t* (*)()>(0x8ace60)();
        int mode = *(int32_t*)(sc + 0xcc);
        if (view8::split == 8) return hud8::kView8;
        if (view8::split == 6) return hud8::kView6;
        return mode == 0 ? (const char*)(mk8dx::Base() + 0xeef890) : mode == 1 ? (const char*)(mk8dx::Base() + 0xeef8a2)
             : mode == 2 ? (const char*)(mk8dx::Base() + 0xeef8b4) : (const char*)(mk8dx::Base() + 0xed6b28);
    }
};
HOOK_DEFINE_REPLACE(RaceViewFrontNameHook) {             /* 0x548d20 */
    static const char* Callback(void*) {
        uint8_t* sc = mk8dx::Fn<uint8_t* (*)()>(0x8ace60)();
        int mode = *(int32_t*)(sc + 0xcc);
        if (view8::split) return hud8::kFront6;           /* Front_8P non esiste nella RomFS */
        return mode == 0 ? (const char*)(mk8dx::Base() + 0xeef964) : mode == 1 ? (const char*)(mk8dx::Base() + 0xeef97c)
             : mode == 2 ? (const char*)(mk8dx::Base() + 0xeef994) : (const char*)(mk8dx::Base() + 0xed6b28);
    }
};
namespace hud8 {
    /* H2: pagine HUD per i locali 4..7 su 4 layer 2D extra (slot 18..21), id di pagina mai registrati nel retail
       (dump "H2: id pagina senza fabbrica"), oggetti per viewport 5..8 nella pagina comune (race+0x358 a 9 voci) */
    static constexpr int kIds[4] = { 0x12, 0x1d, 0x43, 0x5b };
    static constexpr int kFirstSlot2D = 18;
    static uint8_t* layer2d[4]; static float baseZ2d[4];
    static float sub2dZ[4]; static bool sub2dHave[4];                  /* camZ originale dei layer 2D Sub0..3 (slot 6..9), permanenti */
    static void RestoreSub2D(uint8_t* ls) {                            /* fine gara a 8: la camera x2 non deve restare nelle gare 1-4P */
        if (!ls) return;
        uint8_t** tab = *(uint8_t***)(ls + 0x48);
        for (int i = 0; i < 4; i++) {
            uint8_t* ly = tab ? tab[6 + i] : nullptr; if (!ly || !sub2dHave[i]) continue;
            uint8_t* c = *(uint8_t**)(ly + 0x68); if (!c) continue;
            *(float*)(c + 0x40) = sub2dZ[i];
            mk8dx::Fn<void (*)(void*, void*)>(0x619a08)(c, c + 8);
        }
    }
    static view8::Vp extraVp[4];                                       /* entry "viewport" fittizie 5..8 per 0x54fcc4 */
    /* P7 (step 7c): pagine prese in prestito solo per i locali reali (equivalente di 0x406cec `cmp tipo,#1`): con 5/7 locali
       la cella vuota non ha pagina HUD ne' Lakitu */
    static int Pages() { int e = view8::split ? view8::nLocal - 4 : 0; return e < 0 ? 0 : e > 4 ? 4 : e; }
    static int SlotOfId(int id) { for (int j = 0; j < 4; j++) if (kIds[j] == id) return 4 + j; return -1; }
    static uint8_t* cmnPage; static int partSplit = -1;                /* pagina comune e split per cui le partizioni sono state impostate */
    static void ApplyPartitions() {                                    /* chiamata per frame da proto::PerFrame */
        int sp = view8::split;
        if (!sp || !cmnPage || partSplit == sp) return;                 /* 1-4P: lasciamo quanto deciso dal gioco */
        uint8_t* lyt = *(uint8_t**)(cmnPage + 0x320); if (!lyt) return;  /* rc_RaceView_Cmn_00 (0x531c88 / 0x536a5c) */
        uint8_t* root = lyt + 0x30;
        partSplit = sp;
        SetVisible(root, kPartX, true);
        SetVisible(root, kPartY, sp == 8);
        for (int i = 0; i < 2; i++) { SetVisible(root, kPart6[i], sp == 6); SetVisible(root, kPart8[i], sp == 8); }
    }
}
/* step 7 (screenshot Switch 2): con 8 celle la minimappa e' piu' piccola; 0x533368(race) carica dalla tabella per percorso/modo
   race+0x338 scala, +0x33c/+0x340 traslazione (usati a 0x532c54/0x532c74 con 0x8ae020/0x8adff0 e a 0x5364cc). */
#define MK8DX_MAP8_SCALE 0.65f
HOOK_DEFINE_TRAMPOLINE(RaceMapParamHook) {
    static void Callback(uint8_t* race) {
        Orig(race);
        if (view8::split != 8) return;
        float* f = (float*)(race + 0x338);
        Logging.Log("[mk8dx-8p] map8: scala %.3f pos %.1f,%.1f -> x%.2f", f[0], f[1], f[2], MK8DX_MAP8_SCALE);
        f[0] *= MK8DX_MAP8_SCALE; f[1] *= MK8DX_MAP8_SCALE; f[2] *= MK8DX_MAP8_SCALE;
    }
};
HOOK_DEFINE_TRAMPOLINE(RacePageCtorHook) {                /* 0x53186c ctor pagina Race (0x69/0x75): PtrArray +0x350 cap 5 -> 9 (buffer esterno) */
    static void Callback(uint8_t* page) {
        Orig(page);
        void** buf = (void**)mk8dx::Fn<void* (*)(size_t, void*, int)>(0x5faba8)(9 * sizeof(void*), nullptr, 8);
        if (!buf) return;
        memset(buf, 0, 9 * sizeof(void*));
        *(void***)(page + 0x358) = buf; *(int32_t*)(page + 0x354) = 9;
    }
};
namespace hud8 {
    /* oggetti per viewport 5..8 (0x118 B, 0x54fcc4) nella pagina Race (0x69, o 0x75 Race_Watch): la pagina comune nasce prima
       di SceneInitRace, quindi si aggiungono qui, al momento della registrazione delle pagine HUD */
    static void AddVpObjects(uint8_t* race) {
        if (!race || *(int32_t*)(race + 0x354) != 9) { Logging.Log("[mk8dx-8p] H2: pagina Race %p senza buffer a 9", race); return; }
        int32_t& n = *(int32_t*)(race + 0x350);
        while (n < 5 + (view8::split - 4) && n < 9) {
            int j = n - 5;
            uint8_t* obj = mk8dx::Fn<uint8_t* (*)(size_t)>(0x5fa960)(0x118);
            if (!obj) break;
            mk8dx::Fn<void (*)(uint8_t*, int, void*, uint8_t*)>(0x54fcc4)(obj, 5 + j, &hud8::extraVp[j], race);
            (*(void***)(race + 0x358))[n++] = obj;
        }
        Logging.Log("[mk8dx-8p] H2: oggetti per viewport nella pagina Race %p: %d", race, n);
        /* 6b: la pagina Race (onCreate 0x53198c) tiene anch'essa rc_RaceView_Cmn_00 a +0x320 e ne gestisce le partizioni
           (0x531cd8..); 0x536a10 (altra classe) in gara a 8 non viene chiamata, quindi le partizioni si applicano da qui */
        hud8::cmnPage = race; hud8::partSplit = -1;
    }
}
HOOK_DEFINE_TRAMPOLINE(RacePagesSetupHook) {              /* 0x406870: registra le pagine RaceView per viewport; aggiunge RaceView4..7 */
    static void Callback(uint8_t* self) {
        Orig(self);
        if (!view8::split || !hud8::layer2d[0]) return;
        int rid = *(int32_t*)(mk8dx::Fn<uint8_t* (*)()>(0x86f7bc)() + 8) == 4 ? 0x75 : 0x69;
        uint8_t* race = mk8dx::Fn<uint8_t* (*)(int*)>(0x8bf0ec)(&rid);
        hud8::AddVpObjects(race);
        if (!race || *(int32_t*)(race + 0x350) < 5 + view8::split - 4) { Logging.Log("[mk8dx-8p] H2: niente pagine extra (oggetti viewport mancanti)"); return; }
        uint8_t* tab = mk8dx::Fn<uint8_t* (*)()>(0x8bcc5c)();
        uintptr_t factory = *(uintptr_t*)(mk8dx::Base() + 0x1307d68);          /* come 0x406ca4 */
        for (int j = 0; j < hud8::Pages(); j++) {
            int id = hud8::kIds[j];
            *(uintptr_t*)(tab + 0x8 + id * 8) = factory;
            *(uint8_t**)(tab + 0x448 + id * 8) = self;
            mk8dx::Fn<void (*)(uint8_t*, int*, int)>(0x8b3cb0)(self, &id, hud8::kFirstSlot2D + j);
            Logging.Log("[mk8dx-8p] H2: pagina RaceView%d registrata con id 0x%x sul layer %d", 4 + j, id, hud8::kFirstSlot2D + j);
        }
    }
};
HOOK_DEFINE_INLINE(RaceViewSlotHook) {                    /* 0x547d5c `sub w20,w8,#0x65`: w8 = id -> slot; per gli id presi in prestito slot 4..7 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        int sl = hud8::SlotOfId((int)ctx->W[8]);
        if (sl >= 0) ctx->W[8] = 0x65 + sl;
    }
};
HOOK_DEFINE_TRAMPOLINE(LocalSlotToVpHook) {               /* 0x86eb84(slot) -> viewport: 4..7 -> 5..8 con split */
    static int32_t Callback(int32_t slot) { if (slot >= 4 && slot < 8) return view8::split && slot < view8::nLocal ? 5 + (slot - 4) : -1; return Orig(slot); }
};
HOOK_DEFINE_TRAMPOLINE(LocalSlotToKartHook) {             /* 0x86eb1c(slot) -> kart: 4..7 -> kartOf con split */
    static int32_t Callback(int32_t slot) { if (slot >= 4 && slot < 8) return view8::split && slot < view8::nLocal ? view8::kartOf[slot] : -1; return Orig(slot); }
};
/* ---- step 6b H6b (report/step6b_plan_hud.md §1): HUD celle 5-8 uguale alle celle 1-4 ------------------------------------
 * C: colore giocatore 0x86a708(entry+0x4c): indice >= 4 -> 4 (+1 = colore 5, squadra rossa) -> con split 4..7 -> 0..3.
 * R: la pagina Race chiama gli aggiornamenti per oggetto viewport srotolati su race+0x358[0..4]: gli oggetti 5..8 (celle 5-8)
 *    non venivano mai aggiornati (timer del Rank, fine gara, nascondi, allarme). Qui gli stessi passi per gli oggetti 5..n-1. */
namespace hud8 {
    template<typename F> static void EachExtraObj(uint8_t* race, F fn) {
        if (!view8::split || !race || *(int32_t*)(race + 0x354) != 9) return;
        int n = *(int32_t*)(race + 0x350); uint8_t** v = *(uint8_t***)(race + 0x358);
        for (int i = 5; i < n && i < 9; i++) if (v[i]) fn(v[i]);
    }
}
HOOK_DEFINE_INLINE(PlayerColorIdxHook) {                  /* 0x86a720 `cmp w20,#4` in 0x86a708(indice locale) -> colore-1 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t l = ctx->W[20];
        if (view8::split && l >= 4 && l < 8) ctx->W[20] = l - 4;
    }
};
#ifndef MK8DX_PAL8
#define MK8DX_PAL8 1      /* 0 = P5-P8 con i colori di P1-P4 (step 6b) */
#endif
namespace pal8 {   /* step5b passo 8: palette a 8 colori (report/step5b_plan_p8.md) */
    static bool Local8() { uint8_t* rm = race8::RaceMgr(); return rm && *(int32_t*)(rm + 0x178) > 4; }
}
/* H8-A: 0x86a770 `str w8,[x19]` (0xb9000268) in 0x86a708(indice locale) -> raw colore. Locali 4..7 -> 6..9
 * (codice HUD 0x420d18 = 7..10: frame P5-P8 di ItemBox/LapCoin/BtScore/Alarm 4.0.1). Sostituisce il vecchio 0x86a720. */
HOOK_DEFINE_INLINE(PlayerColor8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t l = ctx->W[20];
        if (l >= 4 && l < 8 && pal8::Local8()) ctx->W[8] = l + 2;
    }
};
/* H8-B: 0x420df4 `orr w8,wzr,#3` (0x320007e8) in 0x420da0: raw 6..9 -> 9..12 (codice righe 10..13 dopo il +1) */
HOOK_DEFINE_INLINE(RowColorP8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t r = ctx->W[20];
        if (r >= 6 && r <= 9) ctx->W[20] = r + 3;
    }
};
/* H8-C: 0x513ca8 `ucvtf s0,w8` (0x1e230100) in 0x513b20 (Rank, ramo c > 4): c 7..10 -> Metal5P..8P (16..19) / 5P..8P (20..23) */
HOOK_DEFINE_INLINE(RankColorP8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t c = ctx->W[8];
        if (c >= 7 && c <= 10) ctx->W[8] = c + ((*(int16_t*)((uint8_t*)ctx->X[19] + 0x1d4) < 3) ? 10 : 12);
    }
};
/* H8-D: 0x520830 `cmp w8,#0` (0x7100011f) in 0x5205cc (classifica nella pausa): frame = l+1 -> l+6 per i locali 4..7 */
HOOK_DEFINE_INLINE(PauseRowColorP8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t l = ctx->W[8];
        if (l >= 4 && l < 8 && pal8::Local8()) ctx->W[8] = l + 5;
    }
};
/* H8-E: 0x5057a8 `cmp w8,#7` (0x71001d1f) in 0x50541c (ed_L_TotalResult): Loop della barra anche per 10..13 */
HOOK_DEFINE_INLINE(TotalResultLoopP8Hook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t c = ctx->W[8];
        if (c >= 10 && c <= 13) ctx->W[8] = 1;
    }
};
HOOK_DEFINE_INLINE(RaceVpFrameHook) {                     /* 0x5350a8 in 0x534de4: dopo 0x5500e4(obj[3]) -> per frame degli oggetti 5..8 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        hud8::EachExtraObj((uint8_t*)ctx->X[19], [](uint8_t* o) { mk8dx::Fn<void (*)(uint8_t*)>(0x5500e4)(o); });
    }
};
HOOK_DEFINE_INLINE(RaceFinishReduceHook) {                /* 0x534848: riduzioni di fine gara (and/or) sugli oggetti 5..8 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        hud8::EachExtraObj((uint8_t*)ctx->X[19], [ctx](uint8_t* o) {
            if (!o[0x110]) return;
            ctx->W[22] |= o[0x111] != 0;
            ctx->W[20] &= mk8dx::Fn<bool (*)(uint8_t*)>(0x551294)(o) ? 1 : 0;
            ctx->W[21] |= mk8dx::Fn<bool (*)(uint8_t*)>(0x5512c4)(o) ? 1 : 0;
        });
    }
};
HOOK_DEFINE_INLINE(RaceHideObjsHook) {                    /* 0x53467c: dopo 5x 0x551378 (nasconde ItemBox/LapCoin/Rank) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        hud8::EachExtraObj((uint8_t*)ctx->X[19], [](uint8_t* o) { mk8dx::Fn<void (*)(uint8_t*)>(0x551378)(o); });
    }
};
HOOK_DEFINE_INLINE(RaceResetObjsHook) {                   /* 0x5345e0: prima dell'indice 4 di 5x {+0x108 = 0; 0x551914} */
    static void Callback(exl::hook::InlineCtx* ctx) {
        hud8::EachExtraObj((uint8_t*)ctx->X[19], [](uint8_t* o) { *(int32_t*)(o + 0x108) = 0; mk8dx::Fn<void (*)(uint8_t*)>(0x551914)(o); });
    }
};
/* step 7e (report/step7e_tabella10.md): 0x551914 e 0x526764 cercano il layer 2D della cella con 0x8bf11c({giocatore+1}) nella tabella
   delle maniglie dei layer 2D ([[0x1306e70]]+0x28: 0 Back, 1..4 Sub0..3, 5 Main, 6 Front, 7..9 vuoti, riempita a costanti a
   0x3ff90c..0x3ffad0). Per i giocatori 4..7 l'indice 5..8 cade su Main/Front o su posti NULL (0x8bfafc(NULL) legge [0x10]).
   Stessa regola "cella -> layer 2D della cella": maniglie dei layer 2D Sub4..7 (slot 18..21, creati in proto) costruite come
   quelle dell'avvio (0x5fa960 + 0x8bfa18 + 0x8bfa78) e restituite dopo la ricerca per le celle 5..8. */
namespace hud8 {
    /* memoria statica del modulo: 0x5fa960 (operator new sull'heap corrente del thread, TLS) restituisce NULL alla partenza della gara
       (misurato 5 nov: h2d mai creati); le maniglie vivono quanto quelle dell'avvio (mai liberate) */
    alignas(16) static uint8_t h2dMem[4][0xa0];
    static uint8_t* h2d[4];
    static uint8_t* Handle2D(int j) {
        if (j < 0 || j >= 4 || !layer2d[j]) { static int lg; if (lg++ < 4) Logging.Log("[mk8dx-8p] 7e: layer 2D Sub%d assente", 4 + j); return nullptr; }
        if (!h2d[j]) {
            uint8_t* h = h2dMem[j];
            mk8dx::Fn<void (*)(uint8_t*)>(0x8bfa18)(h);
            mk8dx::Fn<void (*)(uint8_t*, int)>(0x8bfa78)(h, kFirstSlot2D + j);
            h2d[j] = h;
            Logging.Log("[mk8dx-8p] 7e: maniglia layer 2D Sub%d = %p (slot %d, layer %p)", 4 + j, h, kFirstSlot2D + j, *(uint8_t**)(h + 0x10));
        }
        return h2d[j];
    }
    static void FixHandle(exl::hook::InlineCtx* ctx, int player) {
        if (!view8::split || player < 4 || player >= 8) return;
        if (uint8_t* h = Handle2D(player - 4)) ctx->X[0] = (uint64_t)h;
    }
}
HOOK_DEFINE_INLINE(HudHandleCellHook) {                   /* 0x551a0c (bl 0x8bfafc) in 0x551914: x0 = maniglia, giocatore = [x19+0x100] */
    static void Callback(exl::hook::InlineCtx* ctx) { hud8::FixHandle(ctx, *(int32_t*)(ctx->X[19] + 0x100)); }
};
HOOK_DEFINE_INLINE(HudHandlePartHook) {                   /* 0x526830 (bl 0x8bfafc) in 0x526764: x0 = maniglia, giocatore = [x19+0xdc] */
    static void Callback(exl::hook::InlineCtx* ctx) { hud8::FixHandle(ctx, *(int32_t*)(ctx->X[19] + 0xdc)); }
};
HOOK_DEFINE_INLINE(RaceObjCalcHook) {                     /* 0x5349a8: fra indice 3 e 4 di 5x 0x5502f0 (x8 non toccato) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        hud8::EachExtraObj((uint8_t*)ctx->X[19], [](uint8_t* o) { if (!o[0x111]) mk8dx::Fn<void (*)(uint8_t*)>(0x5502f0)(o); });
    }
};
namespace hud8 { static constexpr int kIdsAlarm[4] = { 0x12, 0x1d, 0x43, 0x5b }; }
HOOK_DEFINE_TRAMPOLINE(RaceAlarmHook) {                   /* 0x53635c(race, which, kart): allarme oggetti in arrivo, solo vp 0..4 / slot < 4 */
    static void Callback(uint8_t* race, int32_t which, int32_t kart) {
        Orig(race, which, kart);
        if (!view8::split || kart < 0) return;
        bool done = false;
        hud8::EachExtraObj(race, [&](uint8_t* o) {
            if (done || *(int32_t*)(o + 0xf4) != kart) return;
            done = true;
            int s = *(int32_t*)(o + 0x100); if (s < 4 || s > 7) return;
            int id = hud8::kIdsAlarm[s - 4];
            uint8_t* pg = mk8dx::Fn<uint8_t* (*)(int*)>(0x8bf0ec)(&id);
            if (pg) mk8dx::Fn<void (*)(uint8_t*, int, int)>(0x547aec)(pg, which, kart);
        });
    }
};
/* 6b H6b-L (step6b_plan_hud.md §2.3): Lakitu per le celle 5-8. SetSlot (vt+0xb8 = 0x10f120) nasconde il Lakitu se slot >= 4
   (0x10f198); 0x7e9b04 assegna lo slot solo ai Lakitu dei kart dei vp 0..3. */
HOOK_DEFINE_INLINE(LakituHideGateHook) {                  /* 0x10f198 `cmp w8,#4` (0x7100111f) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (view8::split && ctx->W[8] >= 4 && ctx->W[8] < 8) ctx->W[8] = 0;   /* b.lo: niente nascondi per le viste 5..8 */
    }
};
/* 6b H6b-L3 (step6b_plan_lakitu.md): i Lakitu nascono in 0x7e9d40 (vt+0x70 del gestore, da 0x1101f0) solo per i vp 0..3
   della Scene; l'array mgr+0x40 ha esattamente count elementi. Si rialloca e si accodano i Lakitu dei locali 4..7 (fabbrica
   vt+0xc0 = 0x110298, desc {kart, slot}); init/reset/SetSlot li fa poi il gioco su tutto l'array. */
HOOK_DEFINE_TRAMPOLINE(LakituCreateHook) {                /* 0x7e9d40(mgr), 0xd10103ff */
    static void Callback(uint8_t* mgr) {
        Orig(mgr);
        int n0 = *(int32_t*)(mgr + 0x38);
        uint8_t** old = *(uint8_t***)(mgr + 0x40);
        Logging.Log("[mk8dx-8p] L3: CreateLakitus count=%d split=%d", n0, view8::split);
        if (!view8::split || n0 < 1 || n0 > 4 || !old) return;
        uint16_t desc[4]; int ne = 0;
        for (int j = 0; j < view8::split - 4 && j < 4; j++) {
            const view8::Vp& v = hud8::extraVp[j];
            bool ok = (v.active | 2) == 3 && v.type == 1 && v.kart >= 0 && v.kart < 12 && 4 + j < MK8DX_VIEWS
                      && !mk8dx::Fn<uint8_t* (*)(uint8_t*, int)>(0x7e9a68)(mgr, v.kart);
            Logging.Log("[mk8dx-8p] L3: extra %d attivo=%d tipo=%d kart=%d -> %s", j, v.active, v.type, v.kart, ok ? "creo" : "salto");
            if (ok) desc[ne++] = (uint16_t)((v.kart & 0xff) | ((4 + j) << 8));    /* byte0 -> +0x169 kart, byte1 -> +0x168 slot */
        }
        if (!ne) return;
        uint8_t** arr = (uint8_t**)mk8dx::Fn<void* (*)(size_t, void*, int)>(0x5fad70)((size_t)(n0 + ne) * 8, nullptr, 0x40);
        if (!arr) { Logging.Log("[mk8dx-8p] L3: alloc array fallita"); return; }
        for (int i = 0; i < n0; i++) arr[i] = old[i];
        for (int i = n0; i < n0 + ne; i++) arr[i] = nullptr;
        auto create = reinterpret_cast<uint8_t* (*)(uint8_t*, uint16_t*)>((*(uintptr_t**)mgr)[0xc0 / 8]);   /* 0x110298 */
        int n = n0;
        for (int k = 0; k < ne; k++) {
            uint8_t* lak = create(mgr, &desc[k]);
            if (!lak) continue;
            *(uint8_t**)(lak + 0xf0 + 0x28) = mgr;                         /* come 0x7e9fbc */
            *(uint64_t*)(lak + 0xf0 + 0x30) = *(uint64_t*)(mgr + 0x30);    /* come 0x7e9fc0 */
            arr[n++] = lak;
        }
        *(uint8_t***)(mgr + 0x40) = arr;                                  /* il vecchio array resta nell'heap (il gioco non lo libera) */
        *(int32_t*)(mgr + 0x38) = n;
        *(int32_t*)(mgr + 0x4c) = n;
        Logging.Log("[mk8dx-8p] L3: Lakitu %d -> %d", n0, n);
    }
};
/* 6b: posizione del Lakitu rispetto alla camera. Cinque siti leggono Scene+0xcc e con modo 1 (2P, celle 640x720, aspetto
   0.89) usano la tabella 0x1358e40+0x50 invece di +0x38; le celle 8P (320x360) hanno lo stesso aspetto, quindi si usa
   quella del 2P (senza, il Lakitu esce a sinistra dalla cella). Siti: `ldr w8,[x0,#0xcc]` + `cmp w8,#1`. */
#define LAKITU_MODE_HOOK(N) HOOK_DEFINE_INLINE(LakituModeHook##N) { \
    static void Callback(exl::hook::InlineCtx* ctx) { if (view8::split == 8) ctx->W[8] = 1; } };
LAKITU_MODE_HOOK(0) LAKITU_MODE_HOOK(1) LAKITU_MODE_HOOK(2) LAKITU_MODE_HOOK(3) LAKITU_MODE_HOOK(4)
HOOK_DEFINE_TRAMPOLINE(LakituAssignHook) {                /* 0x7e9b04(mgr) */
    static void Callback(uint8_t* mgr) {
        Orig(mgr);
        if (!view8::split) return;
        static int logged;
        for (int j = 0; j < hud8::Pages(); j++) {                  /* P7: niente SetSlot per la cella vuota */
            uint8_t* lak = mk8dx::Fn<uint8_t* (*)(uint8_t*, int)>(0x7e9a68)(mgr, view8::kartOf[4 + j]);
            if (logged < 8) { logged++; Logging.Log("[mk8dx-8p] L: Lakitu del kart %d = %p (count %d) -> vista %d", view8::kartOf[4 + j], lak, *(int32_t*)(mgr + 0x38), 4 + j); }
            if (!lak) continue;
            auto setSlot = reinterpret_cast<void (*)(uint8_t*, int)>(*(uintptr_t*)(*(uintptr_t*)lak + 0xb8));
            setSlot(lak, 4 + j);
        }
    }
};
namespace hud8 { static int openFlag = -1; }
HOOK_DEFINE_TRAMPOLINE(PageOpenHook) {                    /* 0x8b21dc(&id, flag): apre la pagina id (lista di aggiornamento 0x8bd18c + 0x8bbef4 + 0x8bb3e0) */
    static void Callback(int32_t* pid, uint32_t flag) {
        int id = pid ? *pid : -1;
        if (id >= 0x65 && id <= 0x6d) {
            hud8::openFlag = (int)(flag & 1);
            Logging.Log("[mk8dx-8p] H2: apertura pagina 0x%x flag=%u da main+%lx", id, flag, (uintptr_t)__builtin_return_address(0) - mk8dx::Base());
        }
        Orig(pid, flag);
    }
};
HOOK_DEFINE_TRAMPOLINE(RaceCmnCreateHook) {              /* 0x536a10 onCreate di rc_RaceView_Cmn_00 (prima di SceneInitRace?) */
    static void Callback(uint8_t* page) {
        Orig(page);
        hud8::cmnPage = page; hud8::partSplit = -1;
        Logging.Log("[mk8dx-8p] hud8: pagina comune %p creata (split=%d al momento)", page, view8::split);
        /* H2: id di pagina senza fabbrica nella tabella 0x8bcc5c() (+0x8+id*8), candidati per le pagine HUD dei locali 4..7 */
        static bool dumped; if (!dumped) { dumped = true;
            uint8_t* tab = mk8dx::Fn<uint8_t* (*)()>(0x8bcc5c)();
            char buf[400]; int n = 0;
            for (int id = 0; id < 0x88 && n < 380; id++) if (tab && !*(void**)(tab + 0x8 + id * 8)) n += snprintf(buf + n, sizeof(buf) - n, "%x ", id);
            Logging.Log("[mk8dx-8p] H2: id pagina senza fabbrica: %s", buf);
        }
    }
};

namespace cur8 { static void Reset(); }   /* step 5b passo 3, definito piu' avanti */
HOOK_DEFINE_TRAMPOLINE(SceneInitRaceHook) {
    static void Callback(uint8_t* scene) {
        auto st = mk8dx::Fn<uint8_t* (*)()>(mk8dx::GetSettingsA)();
        int n = mk8dx::Fn<int (*)(uint8_t*)>(mk8dx::GetLocalPlayerCount)(st);
        Logging.Log("[mk8dx-8p] SceneInitRace: n_locali=%d settings=%p modo_settings=%d", n, st, *(int32_t*)(st + 8));
        cur8::Reset();   /* step 5b passo 3: si esce dal menu */
        Orig(scene);
        if (uint8_t* rm = race8::RaceMgr()) {   /* step 7d: indirizzi delle entry per i watchpoint GDB (verifica 2) */
            uint32_t ne = *(uint32_t*)(rm + 0x50); uint8_t** ea = (uint8_t**)*(uintptr_t*)(rm + 0x58);
            Logging.Log("[mk8dx-8p] 7d: raceMgr=%p entry=%u e8=%p e9=%p e10=%p e11=%p e12=%p e15=%p", rm, ne,
                        ne > 8 ? ea[8] : nullptr, ne > 9 ? ea[9] : nullptr, ne > 10 ? ea[10] : nullptr, ne > 11 ? ea[11] : nullptr,
                        ne > 12 ? ea[12] : nullptr, ne > 15 ? ea[15] : nullptr);
        }
        bool offline = !(st[0x24] & 0x10) && *(int32_t*)(st + 4) != 1;   /* come 0x7e7708/0x7e7718 */
        view8::split = 0; view8::nLocal = n;
        if (n > 4 && n <= 8 && offline) {
            view8::split = n <= 6 ? 6 : 8;
            auto* vp = reinterpret_cast<view8::Vp*>(scene + 0x38);
            for (int i = 0; i < 5; i++) vp[i] = { i, -1, 0, 0, -1, -1 };
            for (int i = 0; i < n; i++) {
                int d = *(int32_t*)(st + 8) == 4 ? i : mk8dx::Fn<int (*)(uint8_t*, int)>(0x86f64c)(st, i);
                int t = *(int32_t*)(st + 0x44 + (d >= 0 && d < 12 ? d : 0) * 0x1c) != 0 ? 2 : 1;
                int loc = mk8dx::Fn<int (*)(int)>(0x86e8e4)(d);
                view8::kartOf[i] = d;
                if (i < 4) vp[i] = { i, i, 1, t, loc, d }; else hud8::extraVp[i - 4] = { 5 + (i - 4), i, 1, t, loc, d };
            }
            for (int i = n; i < 8; i++) view8::kartOf[i] = -1;               /* locali inesistenti (prima restava il valore vecchio) */
            for (int i = n; i < view8::split; i++) hud8::extraVp[i - 4] = { 5 + (i - 4), -1, 0, 0, -1, -1 };
            if (n & 1) {
                /* P4 (step 7c): cella vuota come il ciclo 2 di 0x7e76b8 (0x7e78b8..0x7e78f4): kart min(0, n kart) = 0,
                   locSlot buf[0] = 0x86e8e4(0), tipo = settings+8 == 2 ? 1 : 2. Slot -1 invece di 4: 0x7e7af8 scriverebbe
                   +0xb0[clamp4(4)=0] = 4 togliendo il viewport a P1. Letto da 0xcf224 per il controller di camera 4. */
                int loc0 = mk8dx::Fn<int (*)(int)>(0x86e8e4)(0);
                int t = *(int32_t*)(st + 8) == 2 ? 1 : 2;
                vp[4] = { 4, -1, 1, t, loc0, 0 };
                hud8::extraVp[n - 4] = { 5 + (n - 4), -1, 1, t, loc0, 0 };   /* oggetto per viewport della cella vuota (come il 3P) */
                Logging.Log("[mk8dx-8p] 7c: vp4 spettatore tipo=%d loc=%d (cella %d vuota)", t, loc0, n + 1);
            }
            *(int32_t*)(scene + 0xcc) = 2;
            mk8dx::Fn<void (*)(uint8_t*)>(0x7e7af8)(scene);
            Logging.Log("[mk8dx-8p] view8: n=%d split=%d kart={%d,%d,%d,%d,%d,%d,%d,%d}", n, view8::split, view8::kartOf[0], view8::kartOf[1],
                        view8::kartOf[2], view8::kartOf[3], view8::kartOf[4], view8::kartOf[5], view8::kartOf[6], view8::kartOf[7]);
        }
        DumpScene("SceneInitRace ->", scene);
        geom::nVp = geom::nSc = 0; /* ricomincia a loggare i rect da qui */
    }
};

HOOK_DEFINE_TRAMPOLINE(SceneInitHook) {
    static void Callback(uint8_t* scene, int* type) {
        Logging.Log("[mk8dx-8p] SceneInit scene=%p tipo=%d", scene, type ? *type : -1);
        rlm::sceneType = type ? *type : -1;
        mk8dx::LogBacktrace("SceneInit", __builtin_frame_address(0));
        Orig(scene, type);
        DumpScene("SceneInit ->", scene);
    }
};

/* ---- RenderLayerManager ------------------------------------------------------------ */
HOOK_DEFINE_TRAMPOLINE(RlmSyncHook) {
    static void Callback(uint8_t* mgr) {
        using rlm::calls; using rlm::lastMode;
        rlm::mgr = mgr;
        Orig(mgr);
        calls++;
        int mode = *(int32_t*)(mgr + 0xbd8);
        if (calls == 1 || mode != lastMode) {
            lastMode = mode;
            uint8_t* ls = *reinterpret_cast<uint8_t**>(mk8dx::Base() + mk8dx::LayerSystemGlobal);
            int32_t* tab = reinterpret_cast<int32_t*>(mk8dx::Base() + mk8dx::RectModeTable);
            Logging.Log("[mk8dx-8p] RLM sync#%lu mgr=%p modo=%d scala=%f ls=%p ls.count=%d liste: all=%d/%d sub3d=%d/%d sub2d=%d/%d",
                        calls, mgr, mode, *(float*)(mgr + 0xbdc), ls, ls ? *(int32_t*)(ls + 0x40) : -1,
                        *(int32_t*)(mgr + 0x38), *(int32_t*)(mgr + 0x3c), *(int32_t*)(mgr + 0x120), *(int32_t*)(mgr + 0x124),
                        *(int32_t*)(mgr + 0x158), *(int32_t*)(mgr + 0x15c));
            for (int i = 0; i < 4; i++) {
                uint8_t* sl = mgr + 0x9bc + i * 0x20;
                Logging.Log("[mk8dx-8p]   slot[%d] vp=%d layer3d=%d rect=%d flags=%02x %02x %02x %02x", i,
                            *(int32_t*)sl, *(int32_t*)(sl + 4), *(int32_t*)(sl + 0x14), sl[0x18], sl[0x19], sl[0x1a], sl[0x1b]);
            }
            for (int m = 0; m < 4; m++)
                Logging.Log("[mk8dx-8p]   rectTab[modo %d] = {%d,%d,%d,%d}", m, tab[m*4], tab[m*4+1], tab[m*4+2], tab[m*4+3]);
            if (ls) {
                int64_t* q = reinterpret_cast<int64_t*>(ls);
                Logging.Log("[mk8dx-8p]   ls +0x38..+0x68: %lx %lx %lx %lx %lx %lx %lx", q[7], q[8], q[9], q[10], q[11], q[12], q[13]);
            }
            uint8_t** sub = *reinterpret_cast<uint8_t***>(mgr + 0x128);
            for (int i = 0; i < *(int32_t*)(mgr + 0x120) && sub; i++) {
                uint8_t* ly = sub[i]; float* r = reinterpret_cast<float*>(ly + 0x30);
                Logging.Log("[mk8dx-8p]   sub3d[%d]=%p rect=%.3f %.3f %.3f %.3f orient=%d flags=%x slot=%d cam68=%p cam70=%p", i, ly,
                            r[0], r[1], r[2], r[3], *(int32_t*)(ly + 0x40), *(uint32_t*)(ly + 0x78), *(int32_t*)(ly + 0x84),
                            *(void**)(ly + 0x68), *(void**)(ly + 0x70));
            }
            for (int r = 0; r < 13; r++) {
                float* q = reinterpret_cast<float*>(mgr + 0xa38 + r * 16);
                Logging.Log("[mk8dx-8p]   rect[%d] = %.1f %.1f %.1f %.1f", r, q[0], q[1], q[2], q[3]);
            }
        }
        if (calls % 600 == 0) {
            for (int c = 0; c < 2; c++) {
                float* cam = reinterpret_cast<float*>(mgr + 0x238 + c * 0xc0);
                Logging.Log("[mk8dx-8p]   cam[%d]@%p flag=%d m: %.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f | near/far %.1f %.1f fov %.3f aspect %.3f",
                            c, cam, *(int32_t*)(cam + 2), cam[4], cam[5], cam[6], cam[7], cam[8], cam[9], cam[10], cam[11],
                            cam[12], cam[13], cam[14], cam[15], cam[0x98/4], cam[0x9c/4], cam[0xa0/4], cam[0xb0/4]);
            }
        }
    }
};


/* ---- step 2C: prototipo viste extra --------------------------------------------------- */
#ifndef MK8DX_ENABLE_EXTRA
#define MK8DX_ENABLE_EXTRA 1 /* 0 = R a 8 viste + layer collegati ma viste extra mai accese (bisezione crash) */
#endif
#ifndef MK8DX_VIEWS
#define MK8DX_VIEWS 8
#endif
#ifndef MK8DX_COPY_DE4
#define MK8DX_COPY_DE4 1   /* esperimento step 6: copia layer+0xde4 di 3D Sub0 sui layer extra */
#endif
#ifndef MK8DX_P7_MENU3D8
#define MK8DX_P7_MENU3D8 1       /* step 7 B: 8 KartUnit e 8 viste 3D nel menu (anteprime della selezione a 8); 0 = menu a 4 viste */
#endif
#ifndef MK8DX_P7_FORCE8
#define MK8DX_P7_FORCE8 1        /* prova A/B: 8 KartUnit nel menu */
#endif
#ifndef MK8DX_P7_MENUR8
#define MK8DX_P7_MENUR8 1        /* prova A/B: renderer "Main" del menu a 8 viste */
#endif
#ifndef MK8DX_P7_KARTWORK
#define MK8DX_P7_KARTWORK 0x30c8000   /* heap KartWork del menu con 8 KartUnit (vanilla 0x1864000) */
#endif
namespace m3d8 { static void MenuFrame(uint8_t* m); }   /* step 7 B, definito piu' avanti */
static void DescFrame();   /* step 6d M10, definita piu' avanti */
namespace proto {
    constexpr int Views = MK8DX_VIEWS;            /* 6 o 8 */
    constexpr int Extra = Views - 4;
    constexpr int FirstSlot = 14;                 /* slot layerSystem liberi (capacità 32) */
    static uint8_t* ls; static void* heap; static uint8_t* mgr; static uint8_t* dir; static uint8_t* R; /* renderer di scena "Main" corrente */
    static unsigned long models, modelsWide; /* modelli creati / estesi da ModelCreateHook */
    static uint8_t* layer[4]; static uint8_t* cam[4]; static uint8_t* proj[4];
    static bool active; static unsigned long frames; static uint64_t tick0; static unsigned long frames0;
    static const char* names[4] = {"3D Sub4", "3D Sub5", "3D Sub6", "3D Sub7"};

    using AllocFn = void* (*)(size_t, void*, int);
    using CtorFn = void (*)(void*);
    using RegFn = void (*)(void*, void*, int, void*, int, void*);
    using EnableFn = void (*)(void*, bool);
    using UpdMtxFn = void (*)(void*, void*);
    using SceneFn = uint8_t* (*)();

    static void Create(uint8_t* m) {
        mgr = m;
        if (!ls) { Logging.Log("[mk8dx-8p] proto: layerSystem non catturato"); return; }
        auto alloc = mk8dx::Fn<AllocFn>(0x5faba8);
        for (int j = 0; j < Extra; j++) {
            uint8_t* ly = (uint8_t*)alloc(0x1090, nullptr, 8); /* LayerHeap è piena: heap corrente */
            if (!ly) { Logging.Log("[mk8dx-8p] proto: alloc layer %d fallita", j); return; }
            mk8dx::Fn<CtorFn>(0x68d0e4)(ly);
            void* nameArg[2] = { (void*)(*(uintptr_t*)(mk8dx::Base() + 0x12f9e28) + 0x10), (void*)names[j] };
            mk8dx::Fn<RegFn>(0x667a94)(ls, ly, FirstSlot + j, nameArg, 0, nullptr);
            cam[j] = (uint8_t*)alloc(0x60, nullptr, 8);
            proj[j] = (uint8_t*)alloc(0xc0, nullptr, 8);
            memcpy(cam[j], m + 0x1d8, 0x60);
            memcpy(proj[j], m + 0x238, 0xc0);
            *(uint8_t**)(ly + 0x68) = cam[j];
            *(uint8_t**)(ly + 0x70) = proj[j];
            mk8dx::Fn<EnableFn>(0x6651a8)(ly, false);
            layer[j] = ly;
            Logging.Log("[mk8dx-8p] proto: creato %s layer=%p slot=%d cam=%p proj=%p", names[j], ly, FirstSlot + j, cam[j], proj[j]);
        }
        /* H2: 4 layer 2D extra (slot 18..21) per le pagine HUD dei locali 4..7, copiando camera/proiezione di 2D Sub0 (slot 6) */
        uint8_t** tab = *(uint8_t***)(ls + 0x48);
        uint8_t* s2d = tab ? tab[6] : nullptr;
        static const char* names2d[4] = {"2D Sub4", "2D Sub5", "2D Sub6", "2D Sub7"};
        for (int j = 0; j < Extra && s2d; j++) {
            uint8_t* ly = (uint8_t*)alloc(0x1678, nullptr, 8);
            if (!ly) break;
            mk8dx::Fn<CtorFn>(0x3c8734)(ly);
            void* nameArg[2] = { (void*)(*(uintptr_t*)(mk8dx::Base() + 0x12f9e28) + 0x10), (void*)names2d[j] };
            mk8dx::Fn<RegFn>(0x667a94)(ls, ly, hud8::kFirstSlot2D + j, nameArg, 0, nullptr);
            uint8_t* c = (uint8_t*)alloc(0x60, nullptr, 8); uint8_t* pj = (uint8_t*)alloc(0xc0, nullptr, 8);
            memcpy(c, *(uint8_t**)(s2d + 0x68), 0x60); memcpy(pj, *(uint8_t**)(s2d + 0x70), 0xc0);
            *(uint8_t**)(ly + 0x68) = c; *(uint8_t**)(ly + 0x70) = pj; ly[0x83] = 0;
            *(uint16_t*)(ly + 0xde4) = *(uint16_t*)(s2d + 0xde4);
            mk8dx::Fn<EnableFn>(0x6651a8)(ly, false);
            hud8::layer2d[j] = ly; hud8::baseZ2d[j] = *(float*)(c + 0x40);
            Logging.Log("[mk8dx-8p] H2: creato %s layer=%p slot=%d (camZ base %.1f)", names2d[j], ly, hud8::kFirstSlot2D + j, hud8::baseZ2d[j]);
        }
    }

    static int Cols() { return view8::split == 6 ? 3 : 4; }
    static int Used() { int e = view8::split ? view8::split - 4 : 0; return e < Extra ? e : Extra; }   /* viste extra in uso */
    static void Cell(int i, float W, float H, float* r) {
        int cols = Cols();
        float cw = W / cols, ch = H / 2;
        /* bordi delle colonne su coordinate pari (interi anche a 1.5x): con 3 colonne 426.67/853.33 -> 426/854 (bordi che tremavano
           nel 5P/6P); con 4 colonne 320/640/960 invariati */
        auto edge = [&](int c) { return 2.0f * __builtin_roundf(c * cw * 0.5f); };
        int c = i % cols;
        r[0] = edge(c); r[1] = H - (i / cols + 1) * ch; /* y=0 in basso: riga 0 = sopra */ r[2] = c + 1 == cols ? W : edge(c + 1); r[3] = r[1] + ch;
    }
    static void SetRect(uint8_t* ly, const float* r) {
        memcpy(ly + 0x30, r, 16);
        uint8_t* pj = *(uint8_t**)(ly + 0x70);
        *(float*)(pj + 0xb0) = (r[2] - r[0]) / (r[3] - r[1]);
        pj[0x8] = 1;
    }
    static uint8_t* KartDesc(int k) {
        if (!dir) return nullptr;
        uint8_t** e = reinterpret_cast<uint8_t**>(dir + 0x3d0 + k * 0x28);
        uint8_t* d = (e[3] && *e[3]) ? e[1] : e[0];
        return d;
    }
    /* KartCamUnit = e[0]-8 -> KartUnit [unit+0xb0] -> Vehicle [KartUnit+8] -> posa P = [Vehicle+0x28]+8:
       X lato, U su, F avanti, T posizione (3 float ciascuno) [IPOTESI forte, report/step2_extraviews.md §2] */
    static uint8_t* Vehicle(int k) {
        if (!dir) return nullptr;
        uint8_t* e0 = *reinterpret_cast<uint8_t**>(dir + 0x3d0 + k * 0x28);
        if (!e0) return nullptr;
        uint8_t* ku = *reinterpret_cast<uint8_t**>(e0 - 8 + 0xb0);
        return ku ? *reinterpret_cast<uint8_t**>(ku + 8) : nullptr;
    }
    static float* Pose(int k) {
        uint8_t* v = Vehicle(k);
        uint8_t* p = v ? *reinterpret_cast<uint8_t**>(v + 0x28) : nullptr;
        return p ? reinterpret_cast<float*>(p + 8) : nullptr;
    }
    static float Dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
    /* N3 (step6_plan_B §3.3): camera reale del kart k (descrittore del director, tipo 1/2) come 0xc9568:
       pos/at/up -> cam+0x38/+0x44/+0x50, up normalizzato, setFovy 0x61bdd0(proj, d+0x30), doUpdateMatrix. */
    static bool DescCam(int k, uint8_t* c, uint8_t* pj) {
        if (!dir) return false;
        uint8_t** e = reinterpret_cast<uint8_t**>(dir + 0x3d0 + k * 0x28);
        uint8_t* unit = e[0] ? e[0] - 8 : nullptr;
        if (!unit || !*(uint8_t*)(unit + 0xc8)) return false;            /* niente camera giocatore */
        uint8_t* d = KartDesc(k);
        if (!d) return false;
        int t = *(int32_t*)(d + 0x8);
        if (t != 1 && t != 2) return false;
        memcpy(c + 0x38, d + 0xc, 12); memcpy(c + 0x44, d + 0x18, 12); memcpy(c + 0x50, d + 0x24, 12);
        float* up = (float*)(c + 0x50); float l = up[0] * up[0] + up[1] * up[1] + up[2] * up[2];
        if (l > 0) { l = 1.0f / __builtin_sqrtf(l); up[0] *= l; up[1] *= l; up[2] *= l; }
        mk8dx::Fn<void (*)(void*, float)>(0x61bdd0)(pj, *(float*)(d + 0x30));
        mk8dx::Fn<UpdMtxFn>(0x619a08)(c, c + 8);
        return true;
    }
    /* P6 (step 7c): camera calcolata dal controller di camera 4 in modo 1 (sotto-camera spettatore). 0x7d2928..0x7d2938 scrivono
       in obj+0x68..0x97 la MATRICE DI VISTA 3x4 (righe R|t) costruita da 0xc9e54 [misurato in Eden: riga 0 = (-1,0,0,tx) in tutti
       i controller, anche nel 3P originale], non pos/at/up; 0x7d2944 imposta il fovy della proiezione obj+0x28. Come in RelCam:
       occhio = -R^T t, at = occhio - riga 2, su = riga 1; il director non la applica (P2): matrice ricalcolata qui. */
#ifndef MK8DX_SPECTOFF
#define MK8DX_SPECTOFF 0
#endif
    static bool SpectCam(uint8_t* c, uint8_t* pj) {
        uint8_t* o = spect::ctrl;
        if (!spect::Valid(o) || *(int32_t*)(o + 0xe8) != 1) return false;
        uint8_t* p4 = *(uint8_t**)(o + 0x28);
        if (!p4) return false;
        const float* M = (const float*)(o + 0x68);
        float* E = (float*)(c + 0x38); float* A = (float*)(c + 0x44); float* U = (float*)(c + 0x50);
        for (int i = 0; i < 3; i++) E[i] = -(M[0 + i] * M[3] + M[4 + i] * M[7] + M[8 + i] * M[11]);
        for (int i = 0; i < 3; i++) { A[i] = E[i] - M[8 + i]; U[i] = M[4 + i]; }
        mk8dx::Fn<void (*)(void*, float)>(0x61bdd0)(pj, *(float*)(p4 + 0xa0));
        mk8dx::Fn<UpdMtxFn>(0x619a08)(c, c + 8);
        static int logged;
        if (logged < 3) { logged++; Logging.Log("[mk8dx-8p] 7c: SpectCam occhio=%.0f,%.0f,%.0f at=%.0f,%.0f,%.0f su=%.2f,%.2f,%.2f | o+0x20 = o%+ld",
                                                E[0], E[1], E[2], A[0], A[1], A[2], U[0], U[1], U[2], (long)(*(uint8_t**)(o + 0x20) - o)); }
        return true;
    }
    static float offEye[3], offAt[3], offUp[3];
    /* La camera del giocatore 1 (kart 0) è aggiornata: ne ricaviamo pos/at/up nel sistema del suo kart e li
       riapplichiamo al kart k (i descrittori dei kart CPU restano fissi a 0,0,10). */
    static void KartCam(int k, uint8_t* c) {
        int k0 = view8::split ? view8::kartOf[0] : 0;         /* 6b: kart del giocatore 1 (non sempre il kart 0) */
        float* p0 = Pose(k0); float* pk = Pose(k); uint8_t* d0 = KartDesc(k0);
        if (!p0 || !pk || !d0 || *(int32_t*)(d0 + 0x8) != 1) return;
        const float* eye0 = (float*)(d0 + 0xc); const float* at0 = (float*)(d0 + 0x18); const float* up0 = (float*)(d0 + 0x24);
        float ve[3], va[3];
        for (int i = 0; i < 3; i++) { ve[i] = eye0[i] - p0[9 + i]; va[i] = at0[i] - p0[9 + i]; }
        for (int a = 0; a < 3; a++) { offEye[a] = Dot(ve, p0 + 3 * a); offAt[a] = Dot(va, p0 + 3 * a); offUp[a] = Dot(up0, p0 + 3 * a); }
        float* eye = (float*)(c + 0x38); float* at = (float*)(c + 0x44); float* up = (float*)(c + 0x50);
        for (int i = 0; i < 3; i++) {
            eye[i] = pk[9 + i]; at[i] = pk[9 + i]; up[i] = 0;
            for (int a = 0; a < 3; a++) { eye[i] += pk[3 * a + i] * offEye[a]; at[i] += pk[3 * a + i] * offAt[a]; up[i] += pk[3 * a + i] * offUp[a]; }
        }
        mk8dx::Fn<UpdMtxFn>(0x619a08)(c, c + 8);
    }
    /* 6b H6b-I (step6b_plan_hud.md §2.2): durante l'intro la testa della pila del director per vp 0 e' un nodo tipo 2
       (prio 20, camera d'orbita, solo vp 0..3). Le celle extra copiano la camera effettiva di 3D Sub0 (matrice vista a
       cam+0x8) trasportata dal kart di P1 al kart della cella, con il fovy di Sub0 (proj+0xa0, 0x61bdd0). */
    static bool IntroActive() {
        uint8_t* head = dir ? *(uint8_t**)(dir + 0x78) : nullptr;
        return head && *(int32_t*)(head - 0x10 + 0xc) == 2;
    }
    static bool RelCam(int k, uint8_t* c, uint8_t* pj, uint8_t* sub0) {
        uint8_t* c0 = *(uint8_t**)(sub0 + 0x68); uint8_t* p0j = *(uint8_t**)(sub0 + 0x70);
        float* P0 = Pose(view8::kartOf[0]); float* Pk = Pose(k);
        if (!c0 || !P0 || !Pk) return false;
        const float* M = (const float*)(c0 + 0x8);
        float eye[3], at[3], up[3];
        for (int i = 0; i < 3; i++) eye[i] = -(M[0 + i] * M[3] + M[4 + i] * M[7] + M[8 + i] * M[11]);
        for (int i = 0; i < 3; i++) { at[i] = eye[i] - M[8 + i]; up[i] = M[4 + i]; }
        float le[3], la[3], lu[3], ve[3], va[3];
        for (int i = 0; i < 3; i++) { ve[i] = eye[i] - P0[9 + i]; va[i] = at[i] - P0[9 + i]; }
        for (int a = 0; a < 3; a++) { le[a] = Dot(ve, P0 + 3 * a); la[a] = Dot(va, P0 + 3 * a); lu[a] = Dot(up, P0 + 3 * a); }
        float* E = (float*)(c + 0x38); float* A = (float*)(c + 0x44); float* U = (float*)(c + 0x50);
        for (int i = 0; i < 3; i++) {
            E[i] = Pk[9 + i]; A[i] = Pk[9 + i]; U[i] = 0;
            for (int a = 0; a < 3; a++) { E[i] += Pk[3 * a + i] * le[a]; A[i] += Pk[3 * a + i] * la[a]; U[i] += Pk[3 * a + i] * lu[a]; }
        }
        if (p0j) mk8dx::Fn<void (*)(void*, float)>(0x61bdd0)(pj, *(float*)(p0j + 0xa0));
        mk8dx::Fn<UpdMtxFn>(0x619a08)(c, c + 8);
        return true;
    }

    static void PerFrame(uint8_t* m) {
        static unsigned long calls;
        uint8_t* scene = mk8dx::Fn<SceneFn>(0x8ace60)();
        int mode = *(int32_t*)(m + 0xbd8);
        if (calls++ % 3000 == 0)
            Logging.Log("[mk8dx-8p] proto diag: call=%lu m=%p mgr=%p layer0=%p scene=%p d4=%d tipo=%d modo=%d", calls, m, mgr, layer[0], scene,
                        scene ? *(int32_t*)(scene + 0xd4) : -1, rlm::sceneType, mode);
        if (m != mgr) return;
        DescFrame();
        /* accese solo se collegate al renderer corrente (accenderle scollegate durante il cambio scena crasha) */
        bool attached = R && layer[0] && *(uint8_t**)(layer[0] + 0x1020) == R;
        bool want = Used() > 0 && MK8DX_ENABLE_EXTRA && rlm::sceneType == 3 && mode >= 2 && attached;   /* N2: solo con 5-8 locali */
        if (want != active) {
            active = want;
            for (int j = 0; j < Extra; j++) { mk8dx::Fn<EnableFn>(0x6651a8)(layer[j], want && j < Used());
                if (hud8::layer2d[j]) mk8dx::Fn<EnableFn>(0x6651a8)(hud8::layer2d[j], want && j < Used()); }
            if (!want) hud8::RestoreSub2D(ls);
            Logging.Log("[mk8dx-8p] proto: viste extra %s (modo=%d viste=%d)", want ? "ATTIVE" : "spente", mode, Views);
            frames0 = frames; tick0 = svcGetSystemTick();
        }
        if (rlm::sceneType != 3) m3d8::MenuFrame(m);   /* step 7 B: anteprime 3D a 8 nel menu */
        /* 6b H6b-A (step6b_plan_hud.md §2.1): ogni frame gli extra seguono lo stato effettivo di 3D Sub0 (sub[0]+0x78 bit0 e
           !mgr+0xc72, come 0x3cd264) e i 2D extra quello di 2D Sub0 (slot 6); 0x6651a8 non fa nulla se il bit non cambia */
        if (active) {
            uint8_t** s3 = *reinterpret_cast<uint8_t***>(m + 0x128);
            uint8_t** t2 = ls ? *(uint8_t***)(ls + 0x48) : nullptr;
            bool on3 = s3 && s3[0] && (s3[0][0x78] & 1) && !m[0xc72];
            bool on2 = t2 && t2[6] && (t2[6][0x78] & 1);
            static int last3 = -1;
            if ((int)on3 != last3) { last3 = on3; Logging.Log("[mk8dx-8p] 6b: 3D Sub0 %s -> extra %s", on3 ? "acceso" : "spento", on3 ? "accesi" : "spenti"); }
            for (int j = 0; j < Used(); j++) {
                mk8dx::Fn<EnableFn>(0x6651a8)(layer[j], on3);
                if (hud8::layer2d[j]) mk8dx::Fn<EnableFn>(0x6651a8)(hud8::layer2d[j], on2);
            }
        }
        /* misura comune a tutte le build (anche MK8DX_VIEWS=4 = riferimento 4P): frame/s in gara e memoria */
        static unsigned long gFrames, gFrames0; static uint64_t gTick0;
        if (rlm::sceneType == 3) {
            gFrames++;
            uint64_t t = svcGetSystemTick();
            if (!gTick0) { gTick0 = t; gFrames0 = gFrames; }
            else if (t - gTick0 >= 19200000ULL * 5) {
                uint64_t used = 0, total = 0;
                svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
                svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
                Logging.Log("[mk8dx-8p] misura: viste=%d attive=%d modo=%d %.1f frame/s mem usata=%lu KiB / %lu KiB modelli=%lu estesi=%lu",
                            active ? 4 + Used() : 4, (int)active, mode, (gFrames - gFrames0) * 19200000.0f / (float)(t - gTick0),
                            (unsigned long)(used >> 10), (unsigned long)(total >> 10), models, modelsWide);
                gTick0 = t; gFrames0 = gFrames;
            }
        } else gTick0 = 0;
        if (!active) return;
        static int dumped;
        if (frames == 300 && dumped < 2 && ls) { /* una volta per attivazione (max 2): tabella layer del layerSystem */
            dumped++;
            int n = *(int32_t*)(ls + 0x40);
            uint8_t** tab = *(uint8_t***)(ls + 0x48);
            Logging.Log("[mk8dx-8p] layers: n=%d soglie 50c=%d 510=%d", n, *(int32_t*)(ls + 0x50c), *(int32_t*)(ls + 0x510));
            for (int i = 0; i < n && i < 32; i++) {
                uint8_t* ly = tab[i];
                if (!ly) continue;
                float* r = (float*)(ly + 0x30);
                Logging.Log("[mk8dx-8p]   slot %d ly=%p vt=main+%lx +84=%d prio=%d fl78=%x rect=%.0f,%.0f,%.0f,%.0f +40=%d", i, ly,
                            *(uintptr_t*)ly - mk8dx::Base(), *(int32_t*)(ly + 0x84), *(int32_t*)(ly + 0x88), *(uint16_t*)(ly + 0x78),
                            r[0], r[1], r[2], r[3], *(int32_t*)(ly + 0x40));
            }
        }
        frames++;
        float* full = reinterpret_cast<float*>(m + 0xa38 + 16);
        float sc = *(float*)(m + 0xbdc), W = full[2] * sc, H = full[3] * sc, r[4];
        uint8_t** sub = *reinterpret_cast<uint8_t***>(m + 0x128);
        static unsigned long camLog;
        for (int i = 0; i < 4; i++) { Cell(i, W, H, r); SetRect(sub[i], r); }
        hud8::ApplyPartitions();
        /* esperimento ordine di disegno (step 6): confronto campi Sub0 vs layer extra e copia di +0xde4 (bit 2 da mgr+0x9d6) */
        {
            uint8_t* s0 = sub[0];
            static unsigned long dlog;
            if (MK8DX_COPY_DE4) for (int j = 0; j < Used(); j++) {      /* solo bit 0x2 (nascosto in cattura) e 0x4 (flag vista) */
                uint16_t* d = (uint16_t*)(layer[j] + 0xde4); *d = (*d & ~0x6) | (*(uint16_t*)(s0 + 0xde4) & 0x6);
            }
            /* H2: le pagine RaceView4..7 vanno aperte come le 0..3 (prova: stesso opener 0x8bb3e0 con l'argomento della RaceView0) */
            if (view8::split) {
                int id0 = 0x65; uint8_t* p0 = mk8dx::Fn<uint8_t* (*)(int*)>(0x8bf0ec)(&id0);
                if (p0 && *(int32_t*)(p0 + 0x120) == 4) for (int j = 0; j < hud8::Pages(); j++) {   /* P7: non la cella vuota */
                    int id = hud8::kIds[j]; uint8_t* pj = mk8dx::Fn<uint8_t* (*)(int*)>(0x8bf0ec)(&id);
                    if (pj && *(int32_t*)(pj + 0x120) == 2 && hud8::openFlag >= 0) {
                        Logging.Log("[mk8dx-8p] H2: apro la pagina 0x%x (RaceView%d) con 0x8b21dc flag=%d", id, 4 + j, hud8::openFlag);
                        mk8dx::Fn<void (*)(int32_t*, uint32_t)>(0x8b21dc)(&id, (uint32_t)hud8::openFlag);
                    }
                }
            }
            /* H2: stato delle pagine RaceView0 (id 0x65) e RaceView4 (id 0x12) e dei loro layer */
            if (view8::split && (dlog % 600) == 1) {
                for (int q = 0; q < 2; q++) {
                    int id = q ? hud8::kIds[0] : 0x65;
                    uint8_t* pg = mk8dx::Fn<uint8_t* (*)(int*)>(0x8bf0ec)(&id);
                    uint8_t* l2 = q ? hud8::layer2d[0] : (*(uint8_t***)(ls + 0x48))[6];
                    uint8_t* obj = pg ? *(uint8_t**)(pg + 0x1d0) : nullptr;
                    Logging.Log("[mk8dx-8p] H2: pagina id 0x%x = %p e8=%d 120=%d 124=%d 184=%d 1d0=%p (vp=%d f4=%d 100=%d) layer=%p fl78=%x rect=%.0f,%.0f,%.0f,%.0f de4=%x",
                                id, pg, pg ? *(int32_t*)(pg + 0xe8) : -1, pg ? *(int32_t*)(pg + 0x120) : -1, pg ? *(int32_t*)(pg + 0x124) : -1,
                                pg ? *(int32_t*)(pg + 0x184) : -1, obj, obj ? *(int32_t*)obj : -1, obj ? *(int32_t*)(obj + 0xf4) : -1, obj ? *(int32_t*)(obj + 0x100) : -1,
                                l2, l2 ? *(uint32_t*)(l2 + 0x78) : 0, l2 ? *(float*)(l2 + 0x30) : 0.f, l2 ? *(float*)(l2 + 0x34) : 0.f, l2 ? *(float*)(l2 + 0x38) : 0.f, l2 ? *(float*)(l2 + 0x3c) : 0.f,
                                l2 ? *(uint16_t*)(l2 + 0xde4) : 0);
                }
            }
            if ((dlog++ % 600) == 0)
                Logging.Log("[mk8dx-8p] draw: Sub0 fl78=%x de4=%x b80=%02x%02x%02x%02x 218=%p prio=%d | extra0 fl78=%x de4=%x b80=%02x%02x%02x%02x 218=%p prio=%d copia=%d",
                            *(uint32_t*)(s0 + 0x78), *(uint16_t*)(s0 + 0xde4), s0[0x80], s0[0x81], s0[0x82], s0[0x83], *(void**)(s0 + 0x218), *(int32_t*)(s0 + 0x88),
                            *(uint32_t*)(layer[0] + 0x78), *(uint16_t*)(layer[0] + 0xde4), layer[0][0x80], layer[0][0x81], layer[0][0x82], layer[0][0x83],
                            *(void**)(layer[0] + 0x218), *(int32_t*)(layer[0] + 0x88), MK8DX_COPY_DE4);
        }
        /* H1: layer 2D Sub0..3 (slot 6..9 del layerSystem) sulle celle 0..3; camera x2 per l'8P (layout 640x720 su 320x360) */
        if (ls) {
            uint8_t** tab = *(uint8_t***)(ls + 0x48);
            using hud8::sub2dZ; using hud8::sub2dHave;
            for (int i = 0; i < 4; i++) {
                uint8_t* ly = tab[6 + i]; if (!ly) continue;
                Cell(i, W, H, r); SetRect(ly, r);
                uint8_t* c = *(uint8_t**)(ly + 0x68); if (!c) continue;
                float* z = (float*)(c + 0x40);
                if (!sub2dHave[i]) { sub2dZ[i] = *z; sub2dHave[i] = true; }
                *z = sub2dZ[i] * (view8::split == 8 ? 2.0f : 1.0f);
                mk8dx::Fn<UpdMtxFn>(0x619a08)(c, c + 8);
                if (i == 0 && (camLog % 600) == 0) Logging.Log("[mk8dx-8p] hud8: 2D Sub0 rect=%.0f,%.0f,%.0f,%.0f camZ=%.1f (base %.1f) fovy=%.3f", r[0], r[1], r[2], r[3], *z, sub2dZ[i], *(float*)(*(uint8_t**)(ly + 0x70) + 0xa0));
            }
        }
        if (dir && view8::split && frames < 1200 && (frames % 30) == 0) {   /* 6b: camera d'intro (step6b_plan_hud.md §2.2) */
            uint8_t* head = *(uint8_t**)(dir + 0x78);
            uint8_t* node = head ? head - 0x10 : nullptr;
            uint8_t* nd = node ? *(uint8_t**)node : nullptr;
            uint8_t* d0 = KartDesc(view8::kartOf[0]); uint8_t* d4 = KartDesc(view8::kartOf[4]);
            uint8_t** e4 = reinterpret_cast<uint8_t**>(dir + 0x3d0 + view8::kartOf[4] * 0x28);
            Logging.Log("[mk8dx-8p] intro f=%lu vp0: n=%d testa prio=%d tipoNodo=%d desc=%p tipoDesc=%d (=d0 %d) | d0 tipo=%d pos=%.0f,%.0f,%.0f | d4 tipo=%d pos=%.0f,%.0f,%.0f unit4+c8=%d",
                        frames, *(int32_t*)(dir + 0x80), node ? *(int32_t*)(node + 8) : -1, node ? *(int32_t*)(node + 0xc) : -1, nd, nd ? *(int32_t*)(nd + 8) : -1,
                        (int)(nd == d0), d0 ? *(int32_t*)(d0 + 8) : -1, d0 ? *(float*)(d0 + 0xc) : 0.f, d0 ? *(float*)(d0 + 0x10) : 0.f, d0 ? *(float*)(d0 + 0x14) : 0.f,
                        d4 ? *(int32_t*)(d4 + 8) : -1, d4 ? *(float*)(d4 + 0xc) : 0.f, d4 ? *(float*)(d4 + 0x10) : 0.f, d4 ? *(float*)(d4 + 0x14) : 0.f,
                        e4[0] ? (int)*(uint8_t*)(e4[0] - 8 + 0xc8) : -1);
        }
        for (int j = 0; j < Used(); j++) {
            bool empty = view8::split && 4 + j >= view8::nLocal;           /* step 7c: cella vuota (5/7 locali) */
            int k = empty ? 0 : view8::split ? view8::kartOf[4 + j] : 4 + j;
            memcpy(proj[j], m + 0x238, 0xc0);
            bool intro = false, real;
            if (empty) {                                                   /* P6: camera spettatore del controller 4, anche nell'intro */
                bool sp = !MK8DX_SPECTOFF && SpectCam(cam[j], proj[j]);   /* MK8DX_SPECTOFF=1: solo prova (bisezione verde) */
                real = sp || DescCam(0, cam[j], proj[j]);                  /* fallback = viewport extra di tipo 1 sul kart 0 */
                static int lastSp = -1;
                if ((int)sp != lastSp) { lastSp = sp; uint8_t* o = spect::ctrl;
                    Logging.Log("[mk8dx-8p] 7c: cella %d camera %s (c4=%p valido=%d modo=%d kart=%d) frame %lu", 5 + j, sp ? "spettatore" : real ? "kart 0 (ripiego)" : "assente",
                                o, (int)spect::Valid(o), spect::Valid(o) ? *(int32_t*)(o + 0xe8) : -1, spect::Valid(o) ? *(int32_t*)(o + 0xf0) : -1, frames); }
            } else {
                intro = view8::split && IntroActive() && RelCam(k, cam[j], proj[j], sub[0]);
                real = intro || DescCam(k, cam[j], proj[j]);
            }
            if (!real) KartCam(k, cam[j]);                                 /* fallback: posa del kart */
            if (j == 0) { static bool lastIntro; if (intro != lastIntro) { lastIntro = intro;
                Logging.Log("[mk8dx-8p] 6b: camera intro relativa %s (frame %lu)", intro ? "ON" : "OFF", frames); } }
            Cell(4 + j, W, H, r); SetRect(layer[j], r);
            if (j == 0 && (camLog++ % 600) == 0) Logging.Log("[mk8dx-8p] view8: cella 5 kart=%d camera %s", k, real ? "reale" : "ricostruita");
            uint8_t* l2 = hud8::layer2d[j];                                /* H2: layer 2D della cella 4+j */
            if (l2) {
                SetRect(l2, r);
                uint8_t* c2 = *(uint8_t**)(l2 + 0x68);
                *(float*)(c2 + 0x40) = hud8::baseZ2d[j] * (view8::split == 8 ? 2.0f : 1.0f);
                mk8dx::Fn<UpdMtxFn>(0x619a08)(c2, c2 + 8);
            }
        }
        /* diagnostica 8 locali: GamePlayer, controller e input per locale (ogni ~5 s) */
        if (view8::split && (frames % 1800) == 0) {
            uint8_t* rm = race8::RaceMgr(); uint8_t* pim = race8::Pim();
            for (int l = 0; l < view8::nLocal && l < 8; l++) {
                uint8_t* e = race8::Entry(rm, l); if (!e) { Logging.Log("[mk8dx-8p] diag8: locale %d senza entry", l); continue; }
                uint8_t* gp = *(uint8_t**)(e + 0x200);
                uint8_t* ctl = gp ? *(uint8_t**)(gp + 0x760) : nullptr;
                Logging.Log("[mk8dx-8p] diag8: locale %d entry %d tipo=%d/%d gp=%p idx=%d stato=%d pos=%d (misc %d) ctrl=%p npad=%d hold=%08x filtrato=%08x kart=%d",
                            l, race8::Locals(rm)[l], *(int32_t*)(e + 0x44), *(int32_t*)(e + 0x224), gp,
                            gp ? *(int32_t*)(gp + 0x768) : -1, gp ? *(int32_t*)(gp + 0x78c) : -1, gp ? *(int32_t*)(gp + 0x770) : -1,
                            pim ? *(int32_t*)(pim + 0xb0) : -1, ctl, ctl ? *(int32_t*)(ctl + 0x178) : -1, ctl ? *(uint32_t*)(ctl + 0x114) : 0,
                            gp ? *(uint32_t*)(gp + 0x3d8 + 0x114) : 0, *(int32_t*)(e + 0x22c));
            }
        }
        uint64_t now = svcGetSystemTick();
        if (now - tick0 >= 19200000ULL * 5) {
            float fps = (frames - frames0) * 19200000.0f / (float)(now - tick0);
            float* p0 = Pose(0); float* p4 = Pose(4); uint8_t* d0 = KartDesc(0);
            uint8_t* v4 = Vehicle(4);
            Logging.Log("[mk8dx-8p] proto: %d viste, %.1f update/s, R=%p | kart0 pose X=%.2f,%.2f,%.2f U=%.2f,%.2f,%.2f F=%.2f,%.2f,%.2f T=%.0f,%.0f,%.0f desc0 pos=%.0f,%.0f,%.0f at=%.0f,%.0f,%.0f"
                        " | off eye=%.1f,%.1f,%.1f at=%.1f,%.1f,%.1f | kart4 T=%.0f,%.0f,%.0f v+2a8=%.0f,%.0f,%.0f | cam4 pos=%.0f,%.0f,%.0f",
                        Views, fps, R,
                        p0 ? p0[0] : 0.f, p0 ? p0[1] : 0.f, p0 ? p0[2] : 0.f, p0 ? p0[3] : 0.f, p0 ? p0[4] : 0.f, p0 ? p0[5] : 0.f,
                        p0 ? p0[6] : 0.f, p0 ? p0[7] : 0.f, p0 ? p0[8] : 0.f, p0 ? p0[9] : 0.f, p0 ? p0[10] : 0.f, p0 ? p0[11] : 0.f,
                        d0 ? *(float*)(d0 + 0xc) : 0.f, d0 ? *(float*)(d0 + 0x10) : 0.f, d0 ? *(float*)(d0 + 0x14) : 0.f,
                        d0 ? *(float*)(d0 + 0x18) : 0.f, d0 ? *(float*)(d0 + 0x1c) : 0.f, d0 ? *(float*)(d0 + 0x20) : 0.f,
                        offEye[0], offEye[1], offEye[2], offAt[0], offAt[1], offAt[2],
                        p4 ? p4[9] : 0.f, p4 ? p4[10] : 0.f, p4 ? p4[11] : 0.f,
                        v4 ? *(float*)(v4 + 0x2a8) : 0.f, v4 ? *(float*)(v4 + 0x2ac) : 0.f, v4 ? *(float*)(v4 + 0x2b0) : 0.f,
                        *(float*)(cam[0] + 0x38), *(float*)(cam[0] + 0x3c), *(float*)(cam[0] + 0x40));
            tick0 = now; frames0 = frames;
        }
    }
}

/* Renderer di scena "Main" (R, 0x6a8f10): creato a ogni scena con 4 viste fisse; in gara lo portiamo a Views
   e colleghiamo i layer extra con il setter 0x68d4f4 (report/step2_extraviews.md). */
namespace proto {
    using RSetFn = int8_t (*)(void*, void*);
}
HOOK_DEFINE_TRAMPOLINE(RenderSceneCreateHook) {
    static uint8_t* Callback(void** name, int32_t* arg, void* heap) {
        const char* s = name ? reinterpret_cast<const char*>(name[1]) : nullptr;
        bool main = s && s[0] == 'M' && s[1] == 'a' && s[2] == 'i' && s[3] == 'n' && s[4] == 0;
        int before = arg ? arg[0] : -1;
        if (main && arg && arg[0] == 4 && (rlm::sceneType == 3 || (MK8DX_P7_MENU3D8 && MK8DX_P7_MENUR8 && rlm::sceneType == 1))) arg[0] = proto::Views;
        if (main && proto::active) { /* spegne le viste extra prima che il renderer venga ricreato */
            proto::active = false;
            for (int j = 0; j < proto::Extra; j++) mk8dx::Fn<proto::EnableFn>(0x6651a8)(proto::layer[j], false);
            Logging.Log("[mk8dx-8p] proto: viste extra spente (nuovo renderer)");
        }
        if (main) proto::R = nullptr;
        uint8_t* r = Orig(name, arg, heap);
        Logging.Log("[mk8dx-8p] render scene \"%s\" viste %d -> %d (tipo scena %d) R=%p R+0xf0=%d", s ? s : "?", before,
                    arg ? arg[0] : -1, rlm::sceneType, r, r ? *(int32_t*)(r + 0xf0) : -1);
        if (main && arg) arg[0] = before;
        return r;
    }
};
/* I modelli (nn::g3d::ModelObj, 0x69bb0c) allocano i dati per vista (model+0x60, 0x58 B ciascuno) per
   arg+0x8 viste, 4 fisso in 4 siti (0x880704, 0x8807d4, 0x810d9c, 0x880b88): con viste 4..7 si legge oltre
   l'array e si corrompono le shape (report/step2_crash8v.md). Sempre attivo: i modelli nascono al caricamento. */

/* ==== step 6d (report/step6d_flash_root.md §15): descrittori texture ==== */
/* step 6d M10 (report/step6d_flash_root.md §15): anello dei descrittori texture 0x6290bc (obj+0x180 dim, +0x188 stati, +0x190 base,
   +0x194 cursore). Uno slot rilasciato torna libero quando il cursore lo supera di dim/2: misura delle registrazioni per frame. */
namespace desc6d { static uint8_t* obj[4]; static uint32_t regs[4], last[4], maxF[4], sumF[4], nF, fail[4]; }
HOOK_DEFINE_TRAMPOLINE(DescRegHook) {      /* 0x6290bc d101c3ff (anello, texture, builder) -> id descrittore (0 = fallito) */
    static uint32_t Callback(uint8_t* o, void* a, void* b) {
        uint32_t id = Orig(o, a, b);
        using namespace desc6d;
        for (int i = 0; i < 4; i++) {
            if (obj[i] != o) { if (obj[i]) continue; obj[i] = o; }
            __atomic_fetch_add(&regs[i], 1, __ATOMIC_RELAXED); if (!id) __atomic_fetch_add(&fail[i], 1, __ATOMIC_RELAXED);
            break;
        }
        return id;
    }
};
static void DescFrame() {                  /* da SegFrameHook, una volta per frame */
    using namespace desc6d;
    nF++;
    for (int i = 0; i < 4 && obj[i]; i++) { uint32_t d = regs[i] - last[i]; last[i] = regs[i]; if (d > maxF[i]) maxF[i] = d; sumF[i] += d; }
    if (nF % 300) return;
    for (int i = 0; i < 4 && obj[i]; i++) {
        uint8_t* o = obj[i];
        Logging.Log("[mk8dx-8p] 6d M10 anello %d obj=%p dim=%d base=%u cursore=%u 19c=%d | registrazioni/frame media %.1f max %u (dim/2=%d) falliti %u | viste=%d",
                    i, o, *(int32_t*)(o + 0x180), *(uint16_t*)(o + 0x190), *(uint32_t*)(o + 0x194), *(int32_t*)(o + 0x19c),
                    sumF[i] / 300.0f, maxF[i], *(int32_t*)(o + 0x180) / 2, fail[i], proto::active ? 4 + proto::Used() : 4);
        maxF[i] = 0; sumF[i] = 0;
    }
}
/* step 6d T11 (report/step6d_flash_root.md §15): numero di descrittori texture del device ([cfg+8] -> dev+0xb0, 0x6044f8). Il pool
   di memoria (0x604640) e l'anello dinamico (0x628a90: potenza di 2 <= totale - statici) si dimensionano da questo valore.
   MK8DX_TEXDESC > 0: valore imposto prima della creazione se maggiore (0 = valore del gioco). */
#ifndef MK8DX_TEXDESC
#define MK8DX_TEXDESC 16640   /* gioco: 8492 (anello 8192); in gara a 8 servono ~8250 descrittori dinamici */
#endif
HOOK_DEFINE_TRAMPOLINE(GfxDeviceCtorHook) {   /* 0x6044a8 a9be4ff4 (device, cfg): cfg+8 descrittori texture */
    static void Callback(uint8_t* dev, uint8_t* cfg) {
        uint32_t n0 = *(uint32_t*)(cfg + 8);
        if (MK8DX_TEXDESC > 0 && (uint32_t)MK8DX_TEXDESC > n0) *(uint32_t*)(cfg + 8) = MK8DX_TEXDESC;
        Orig(dev, cfg);
        Logging.Log("[mk8dx-8p] 6d T11 device=%p descrittori texture %u -> %u (sampler %u) lr=main+%lx", dev, n0, *(uint32_t*)(dev + 0xb0),
                    *(uint32_t*)(dev + 0xb4), (uintptr_t)__builtin_return_address(0) - mk8dx::Base());
    }
};
#ifndef MK8DX_MODELDIAG
#define MK8DX_MODELDIAG 0
#endif
HOOK_DEFINE_TRAMPOLINE(ModelCreateHook) {
    static uint64_t Callback(uint8_t* model, uint8_t* arg, void* heap) {
        int32_t* views = arg ? reinterpret_cast<int32_t*>(arg + 0x8) : nullptr;
        int before = views ? *views : -1;
        if (views && before == 4) *views = proto::Views;
        uint64_t r = Orig(model, arg, heap);
        if (views) *views = before;
#if MK8DX_MODELDIAG
        /* diagnostica console (6 nov 2026): crash su 0x68fa38 (shape array model+0x28 nullo). Ferma con codice nei registri:
           X03 = 0xDEAD0001 creazione fallita / 0xDEAD0002 riuscita ma +0x28 nullo; X04 model, X05 r, X06 viste prima, X07 model[0xe0],
           X08 [model+0x20], X09 [model+0x9c], X10 modelli creati finora, X11 arg, X12 [arg] (risorsa). */
        if (model && before == 4 && (!(r & 1) || (!*(uint64_t*)(model + 0x28) && *(uint32_t*)(model + 0x20) && *(uint16_t*)(model + 0x9c)))) {
            register uint64_t c3 asm("x3") = (r & 1) ? 0xDEAD0002 : 0xDEAD0001;
            register uint64_t c4 asm("x4") = (uint64_t)model;
            register uint64_t c5 asm("x5") = r;
            register uint64_t c6 asm("x6") = (uint64_t)before;
            register uint64_t c7 asm("x7") = model[0xe0];
            register uint64_t c8 asm("x8") = *(uint32_t*)(model + 0x20);
            register uint64_t c9 asm("x9") = *(uint16_t*)(model + 0x9c);
            register uint64_t c10 asm("x10") = proto::models;
            register uint64_t c11 asm("x11") = (uint64_t)arg;
            register uint64_t c12 asm("x12") = arg ? *(uint64_t*)arg : 0;
            asm volatile("mov x0, #0\n mov x1, #0\n mov x2, #0\n svc #0x26\n 1: b 1b\n"
                         :: "r"(c3), "r"(c4), "r"(c5), "r"(c6), "r"(c7), "r"(c8), "r"(c9), "r"(c10), "r"(c11), "r"(c12) : "x0", "x1", "x2", "memory");
        }
#endif
        /* step 7 B: nel menu le viste 4..7 mostrano solo cio' che la mod accende (KartUnit/piedistalli in m3d8::MenuFrame); di default
           ogni modello nasce visibile in tutte le viste (monete e fantasmi nelle celle 5-8, screenshot 23:54). 0x68fa38 = maschera per shape */
        /* 6 nov 2026: su console alcuni modelli escono da 0x69bb0c (riuscita, 8 viste) con l'array delle shape model+0x28 ancora nullo
           (collegato dopo): 0x68fa38 leggeva [0] -> crash all'avvio (PC 0x68fac8, FAR 0). Come il gioco: maschera solo con shape presenti. */
        if (MK8DX_P7_MENU3D8 && rlm::sceneType != 3 && model && (r & 1) && before == 4 && model[0xe0] >= 8 && *(uint64_t*)(model + 0x28))
            for (int b = 4; b < 8; b++) mk8dx::Fn<void (*)(uint8_t*, bool, int)>(0x68fa38)(model, false, b);
        proto::models++;
        if (before == 4) proto::modelsWide++;
        if (proto::models <= 3 || (proto::models & 0x3ff) == 0)
            Logging.Log("[mk8dx-8p] modello #%lu (estesi %lu) viste %d -> model+0xe0=%u ret=%lx", proto::models, proto::modelsWide,
                        before, model ? model[0xe0] : 0, r);
        return r;
    }
};
/* Ordine di disegno: i job dei layer sono ordinati per priorità layer+0x88 (alta = prima), ricalcolata ogni
   frame da vtbl[0x90] (Layer3D: 0x68d474; 2D = 10). I nostri layer ottengono 5 (sotto l'HUD 2D): li allineiamo
   ai 3D Sub (report/step2_draworder.md). */
HOOK_DEFINE_TRAMPOLINE(Layer3DPrioHook) {
    static void Callback(uint8_t* ly) {
        Orig(ly);
        if (!proto::mgr) return;
        for (int j = 0; j < proto::Extra; j++) {
            if (ly != proto::layer[j]) continue;
            uint8_t* sub0 = (*reinterpret_cast<uint8_t***>(proto::mgr + 0x128))[0];
            if (!sub0) return;
            int mine = *reinterpret_cast<int32_t*>(ly + 0x88);
            Orig(sub0); /* idempotente: scrive solo sub0+0x88 */
            int p = *reinterpret_cast<int32_t*>(sub0 + 0x88);
            *reinterpret_cast<int32_t*>(ly + 0x88) = p > 12 ? p - 2 : p;
            static int logged;
            if (proto::active && logged < proto::Extra) {
                logged++;
                Logging.Log("[mk8dx-8p] prio %s: orig=%d sub0=%d -> %d (vista %d)", proto::names[j], mine, p,
                            *reinterpret_cast<int32_t*>(ly + 0x88), (int)*reinterpret_cast<int8_t*>(ly + 0x1088));
            }
            return;
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(RlmAttachHook) {
    static void Callback(uint8_t* mgr, uint8_t* r) {
        Orig(mgr, r);
        if (mgr != proto::mgr || !proto::layer[0]) return;
        proto::R = r;
        int n = r ? *(int32_t*)(r + 0xf0) : 0;
        for (int j = 0; j < proto::Extra; j++) {
            bool fits = r && n > 4 + j;
            int8_t v = mk8dx::Fn<proto::RSetFn>(0x68d4f4)(proto::layer[j], fits ? r : nullptr);
            if (r) Logging.Log("[mk8dx-8p] proto: collega %s a R=%p (viste %d) -> indice %d", proto::names[j], r, n, v);
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(RlmCtorHook) {
    static void Callback(uint8_t* mgr) { Orig(mgr); proto::Create(mgr); }
};
HOOK_DEFINE_TRAMPOLINE(RlmFrameHook) {
    static void Callback(uint8_t* mgr) { Orig(mgr); proto::PerFrame(mgr); }
};
HOOK_DEFINE_TRAMPOLINE(DirectorUpdateHook) {
    static void Callback(uint8_t* dir) { proto::dir = dir; Orig(dir); }
};

HOOK_DEFINE_TRAMPOLINE(LayerRegisterHook) {
    static void Callback(uint8_t* ls, uint8_t* layer, int slot, void** nameArg, int w4, void* heap) {
        const char* name = nameArg ? reinterpret_cast<const char*>(nameArg[1]) : "?";
        if (slot < proto::FirstSlot) { proto::ls = ls; proto::heap = heap; }
        Orig(ls, layer, slot, nameArg, w4, heap);
        Logging.Log("[mk8dx-8p] registra layer ls=%p layer=%p slot=%d nome=\"%s\" w4=%d heap=%p | ls+0x40=%d ls+0x44=%d", ls, layer, slot, name, w4, heap,
                    *(int32_t*)(ls + 0x40), *(int32_t*)(ls + 0x44));
    }
};

namespace p6 { static void EnsureLoaded(); static void Save(const char* why); }   /* step 5b passo 6, definito piu' avanti */
HOOK_DEFINE_TRAMPOLINE(MenuEventHook) {
    static uint64_t Callback(void* self, uint32_t* ev) {
        p6::EnsureLoaded();
        uint32_t id = ev ? ev[0x8 / 4] : 0, kind = ev ? ev[0x28 / 4] : 0;
        uint64_t r = Orig(self, ev);
        if (kind == 0) Logging.Log("[mk8dx-8p] menu event id=%u (0x%x) -> %lx", id, id, r);
        if (kind == 2 && id >= 0x14 && id <= 0x1a) {
            uint8_t* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)();
            int32_t* l = (int32_t*)(rm + 0x17c);
            Logging.Log("[mk8dx-8p] step4: pulsante %ug -> cfg+0x1fa4=%d +0x1fa8=%d race+0x178=%d +0x17c={%d,%d,%d,%d,%d,%d,%d,%d}",
                        id - 0x12, menu8::LocalCount(), menu8::Mode(), *(int32_t*)(rm + 0x178), l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7]);
        }
        return r;
    }
};

/* ---- step 3: input a 8 controller (report/step3_input_plan.md) ------------------------ */
namespace input {
    /* patch di una istruzione con verifica della parola originale */
    static bool Patch(uintptr_t off, uint32_t orig, uint32_t repl, const char* what) {
        exl::patch::RandomAccessPatcher p;
        uint32_t cur = p.Read<uint32_t>(off);
        if (cur != orig) { Logging.Log("[mk8dx-8p] patch %s @%lx: trovato %08x, atteso %08x: saltata", what, off, cur, orig); return false; }
        p.Write<uint32_t>(off, repl);
        p.Flush();
        Logging.Log("[mk8dx-8p] patch %s @%lx: %08x -> %08x", what, off, orig, repl);
        return true;
    }
    struct Last { uint64_t raw; uint32_t attr; };
    static Last last[9];
    static unsigned long calls;
}

/* Log di accettazione: ControllerMgr::calc (0x6192a4) per frame; stato grezzo degli slot NpadDevice 0..8
   (slot = npad id, 8 = handheld: +0x60 pulsanti, +0x78 attributi) e hold dei NpadController di gioco. */
HOOK_DEFINE_TRAMPOLINE(ControllerMgrCalcHook) {
    static void Callback(uint8_t* mgr) {
        Orig(mgr);
        using namespace input;
        uint8_t* dev = mk8dx::Fn<uint8_t* (*)(void*, int)>(0x619220)(mgr, 0xe);
        if (!dev) return;
        for (int s = 0; s < 9; s++) {
            uint8_t* st = dev + s * 0xc40;
            uint64_t raw = *(uint64_t*)(st + 0x60); uint32_t attr = *(uint32_t*)(st + 0x78);
            if (raw == last[s].raw && attr == last[s].attr) continue;
            last[s].raw = raw; last[s].attr = attr;
            Logging.Log("[mk8dx-8p] npad slot %d: attr=%x pulsanti=%016lx stile=%d", s, attr, raw, *(int32_t*)(dev + 0x30 + 4 * s));
        }
        { /* controller di gioco (NpadController, id 0x11): variazioni di hold per posizione */
            static uint32_t lastHold[16];
            int n = *(int32_t*)(mgr + 0x188);
            uint8_t** list = *(uint8_t***)(mgr + 0x190);
            for (int p = 0; p < n && p < 16; p++) {
                uint8_t* c = list[p];
                if (*(int32_t*)(c + 0x138) != 0x11) continue;
                uint32_t hold = *(uint32_t*)(c + 0x114);
                if (hold == lastHold[p]) continue;
                lastHold[p] = hold;
                Logging.Log("[mk8dx-8p] controller pos %d npad %d: hold=%08x", p, *(int32_t*)(c + 0x178), hold);
            }
        }
        if (calls++ % 600 == 0) {
            int n = *(int32_t*)(mgr + 0x188);
            uint8_t** list = *(uint8_t***)(mgr + 0x190);
            Logging.Log("[mk8dx-8p] input: pad interrogati=%d controller=%d/%d", *(int32_t*)(dev + 0x28), n, *(int32_t*)(mgr + 0x18c));
            for (int p = 0; p < n; p++) {
                uint8_t* c = list[p];
                if (*(int32_t*)(c + 0x138) != 0x11) { Logging.Log("[mk8dx-8p]   pos %d id=%x", p, *(int32_t*)(c + 0x138)); continue; }
                Logging.Log("[mk8dx-8p]   pos %d npad=%d conn=%d stile=%d hold=%08x", p, *(int32_t*)(c + 0x178), c[0x17c] & 1, c[0x17d],
                            *(uint32_t*)(c + 0x114));
            }
        }
    }
};

/* H1: dopo l'init NPad (0x7c53ec) crea i NpadController di gioco per gli npad 4..7 e li accoda alla lista del
   ControllerMgr (posizioni 6..9: l'handheld resta in posizione 4 / ordine 4, il misc in 5). Replica il corpo del
   ciclo 0x7c54e4-0x7c5590. */
HOOK_DEFINE_TRAMPOLINE(NpadInitHook) {
    static void Callback(uint8_t* mgr, void* out) {
        Orig(mgr, out);
        auto New = mk8dx::Fn<void* (*)(size_t, void*, int)>(0x5faba8);
        uintptr_t vt = *(uintptr_t*)(mk8dx::Base() + 0x130edc0);
        for (int idx = 4; idx < 8; idx++) {
            int32_t& n = *(int32_t*)(mgr + 0x188);
            if (n >= *(int32_t*)(mgr + 0x18c)) { Logging.Log("[mk8dx-8p] input: lista controller piena (%d), npad %d escluso", n, idx); break; }
            uint8_t* c = (uint8_t*)New(0x298, nullptr, 8);
            if (!c) { Logging.Log("[mk8dx-8p] input: alloc controller %d fallita", idx); break; }
            mk8dx::Fn<void (*)(void*, void*, int)>(0xa7f1a0)(c, mgr, idx);
            *(uintptr_t*)c = vt + 0x10;
            *(uintptr_t*)(c + 0x180) = vt + 0x68;
            mk8dx::Fn<void (*)(void*)>(0x8cd878)(c + 0x188);
            *(uint32_t*)(c + 0x1b8) = 0; *(uint32_t*)(c + 0x1fc) = 0; *(uint32_t*)(c + 0x204) = 0;
            memset(c + 0x230, 0, 0x5c);
            mk8dx::Fn<void (*)(void*)>(0x7c6a34)(c);
            uint8_t* a = (uint8_t*)New(0x40, nullptr, 8);
            mk8dx::Fn<void (*)(void*, void*, int)>(0xa7fad0)(a, c, 0);
            mk8dx::Fn<void (*)(void*, void*)>(0x5faf8c)(c + 0x148, a + *(int32_t*)(c + 0x15c));
            (*(int32_t*)(c + 0x158))++;
            ((uint8_t**)*(uintptr_t*)(mgr + 0x190))[n++] = c;
            Logging.Log("[mk8dx-8p] input: controller npad %d creato %p in posizione %d", idx, c, n - 1);
        }
        Logging.Log("[mk8dx-8p] input: lista controller %d/%d", *(int32_t*)(mgr + 0x188), *(int32_t*)(mgr + 0x18c));
    }
};

/* H3: tabelle del gestore giocatori (PIM) per indice dispositivo (= posizione in lista): +0x98 int, +0xa0 u8,
   +0xa8 int. Allocate a 16 (P4); il codice originale gestisce solo 0..6, qui le voci 7..15. */
namespace input {
    static void ResetTail(uint8_t* pim) {
        int32_t* a98 = *(int32_t**)(pim + 0x98); uint8_t* a0 = *(uint8_t**)(pim + 0xa0); int32_t* a8 = *(int32_t**)(pim + 0xa8);
        if (!a98 || !a0 || !a8) return;
        for (int i = 7; i < 16; i++) { a98[i] = -1; a0[i] = 0; a8[i] = 0; }
    }
}
HOOK_DEFINE_TRAMPOLINE(PimInitHook) {
    static uint64_t Callback(uint8_t* pim, uint64_t a1, uint64_t a2, uint64_t a3) {
        uint64_t r = Orig(pim, a1, a2, a3);
        input::ResetTail(pim);
        Logging.Log("[mk8dx-8p] input: tabelle PIM 16 voci inizializzate (pim=%p)", pim);
        return r;
    }
};
/* G7 (step 5 B): dopo Orig la stessa chiamata per giocatore (0x7ce2fc / 0x7ce168, srotolate su 0..3) per i giocatori 4..count-1 */
namespace input { static void TailPlayers(uint8_t* pim, uintptr_t fn) {
    int n = *(int32_t*)(pim + 0x50); uint8_t** pl = *(uint8_t***)(pim + 0x58);
    for (int i = 4; i < n && i < 8; i++) if (pl[i]) mk8dx::Fn<void (*)(void*)>(fn)(pl[i]);
} }
HOOK_DEFINE_TRAMPOLINE(PimReset1Hook) {                 /* 0x7c58d0 */
    static uint64_t Callback(uint8_t* pim, uint64_t a1, uint64_t a2, uint64_t a3) { input::ResetTail(pim); uint64_t r = Orig(pim, a1, a2, a3); input::TailPlayers(pim, 0x7ce2fc);
        if (menu8::LocalCount() > 4) { Logging.Log("[mk8dx-8p] B: reset PIM 0x7c58d0 (tutti al misc) da main+%lx", (uintptr_t)__builtin_return_address(0) - mk8dx::Base()); }
        return r; }
};
HOOK_DEFINE_TRAMPOLINE(PimReset2Hook) {                 /* 0x7c5794 */
    static uint64_t Callback(uint8_t* pim, uint64_t a1, uint64_t a2, uint64_t a3) { input::ResetTail(pim); uint64_t r = Orig(pim, a1, a2, a3); input::TailPlayers(pim, 0x7ce168);
        if (menu8::LocalCount() > 4) { Logging.Log("[mk8dx-8p] B: reset PIM 0x7c5794 da main+%lx", (uintptr_t)__builtin_return_address(0) - mk8dx::Base()); }
        return r; }
};
HOOK_DEFINE_TRAMPOLINE(PimFrameHook) {
    static uint64_t Callback(uint8_t* pim, uint64_t a1, uint64_t a2, uint64_t a3) {
        uint64_t r = Orig(pim, a1, a2, a3);
        int32_t* a98 = *(int32_t**)(pim + 0x98); uint8_t* a0 = *(uint8_t**)(pim + 0xa0); int32_t* a8 = *(int32_t**)(pim + 0xa8);
        if (a98 && a0 && a8)
            for (int i = 7; i < 16; i++) {
                if (a0[i]) { a98[i] = -1; a0[i] = 0; a8[i] = 0; }
                if (a8[i] - 1 >= 0) a8[i]--;
            }
        return r;
    }
};

/* Diagnosi applet controller: 0x8be248(padUI, flag) = "giocatore del pad pronto" (falso -> il menu 0x4ba77c
   richiama l'applet 0x49ea60(min, max, ...)). Log solo delle variazioni. */
namespace padui { extern uint8_t* extra[4]; }        /* step 5 C, definito piu' avanti */
HOOK_DEFINE_TRAMPOLINE(PadReadyHook) {
    static bool Callback(uint8_t* pad, uint32_t flag) {
        int idx = pad ? *(int32_t*)(pad + 0x8) : -1;
        int n = menu8::LocalCount();
        bool r;
        /* H4 (step 4): senza i pad UI extra, per i>=4 veniva interrogato solo il pad UI 4 (tocco, rifiutato da cmp #3):
           per N>4 rispondeva con la connessione degli npad 4..N-1. Con i pad extra (S5.2, padui::extra) i pad 4..7
           sono veri e si usa sempre Orig (cmp #7 + raceMgr+0x17c[4..7]). */
        if (idx == 4 && n > 4 && n <= 8 && !padui::extra[0]) {
            r = true;
            for (int k = 4; k < n; k++) r = r && menu8::NpadConnected(k);
        } else r = Orig(pad, flag);
        static int last[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
        if (idx >= 0 && idx < 9 && last[idx] != (int)r) {
            last[idx] = r;
            Logging.Log("[mk8dx-8p] applet: pad UI %d (flag %u, N=%d) pronto=%d", idx, flag, n, (int)r);
        }
        return r;
    }
};
HOOK_DEFINE_TRAMPOLINE(ControllerAppletHook) {
    static uint64_t Callback(uint32_t mn, uint32_t mx, uint64_t a2, uint64_t a3) {
        static unsigned long lastFrame; static int burst;
        unsigned long f = input::calls;
        burst = (f - lastFrame < 120) ? burst + 1 : 0;
        lastFrame = f;
        Logging.Log("[mk8dx-8p] applet: chiamata 0x49ea60(min=%u, max=%u, %lx, %lx) da main+%lx frame %lu%s", mn, mx, a2, a3,
                    (uintptr_t)__builtin_return_address(0) - mk8dx::Base(), f, burst >= 2 ? " LOOP!" : "");
        uint64_t r = Orig(mn, mx, a2, a3);
        Logging.Log("[mk8dx-8p] applet: ritorno %lx", r);
        return r;
    }
};

/* ---- step 5 B: raceMgr a 8 locali e giocatori di input 4..7 (report/step5_plan_B.md) --------------
 * raceMgr: +0x17c diventa int[8] (+0x17c..+0x19b) e la mappa slot->entry passa da +0x18c a +0x19c int[12]
 * (patch B1); PIM: 8 GamePlayer (G1-G8); 0x86a594 cerca il GamePlayer libero fra 0..7 (B3). */
/* B1h: setter slot->entry (0x86af68(rm, id, slot)); l'originale azzera 16 parole da +0x18c (ora: locali 4..7 + mappa) */
HOOK_DEFINE_REPLACE(SlotSetHook) {
    static void Callback(uint8_t* rm, int32_t id, int32_t slot) {
        int32_t* s = race8::Slots(rm);
        for (int i = 0; i < 12; i++) if (s[i] == id) s[i] = -1;
        if (slot == -1) return;
        if (slot >= 0 && slot < 12) s[slot] = id;
        else Logging.Log("[mk8dx-8p] B: slot %d fuori 0..11 (entry %d)", slot, id);
    }
};
/* B1j: ctor raceMgr 0x8689b8: -1 anche sulla nuova mappa slot (+0x19c..+0x1cb) */
/* step 7d A (report/step7d_copie.md, step7d_censimento20.md): raceMgr a 20 entry = 8 umani + 12 kart della registrazione (originale
   16 = 4 + 12). Array delle entry +0x50/+0x54/+0x58 e dei backup +0xe8/+0xec/+0xf0 (16 posti inline a +0x60/+0xf8, nessun altro
   utente) spostati su buffer da 20; le costanti 16 -> 20 sono in race8::Patch20. */
namespace race8 {
    static bool cap20; static int ctorCalls;
    alignas(16) static void* entBuf[20]; alignas(16) static void* bakBuf[20];
}
HOOK_DEFINE_TRAMPOLINE(RaceMgrCtorHook) {
    static void Callback(uint8_t* rm) {
        Orig(rm); memset(rm + 0x19c, 0xff, 0x30);
        race8::ctorCalls++;
        if (!race8::cap20) return;
        if (race8::ctorCalls > 1 || *(int32_t*)(rm + 0x50) || *(int32_t*)(rm + 0xe8)) {
            Logging.Log("[mk8dx-8p] 7d: ctor raceMgr %p chiamata %d (n=%d/%d): buffer da 20 NON applicati", rm, race8::ctorCalls,
                        *(int32_t*)(rm + 0x50), *(int32_t*)(rm + 0xe8));
            return;
        }
        *(int32_t*)(rm + 0x54) = 20; *(void***)(rm + 0x58) = race8::entBuf;
        *(int32_t*)(rm + 0xec) = 20; *(void***)(rm + 0xf0) = race8::bakBuf;
        Logging.Log("[mk8dx-8p] 7d: raceMgr %p a 20 entry (buffer %p / %p)", rm, race8::entBuf, race8::bakBuf);
    }
};
/* B1k: compattazione dei locali 0x868f00(rm) -> n locali; l'originale e' srotolato su +0x17c[0..3] e 0x8653cc per e+0x4c<=3 */
HOOK_DEFINE_REPLACE(CompactLocalsHook) {
    static int32_t Callback(uint8_t* rm) {
        auto swap = mk8dx::Fn<void (*)(uint8_t*, int32_t, int32_t)>(0x868cfc);
        int32_t k = 0;
        for (int l = 0; l < 8; l++) {
            int32_t id = race8::Locals(rm)[l];                 /* riletto: 0x868cfc riempie di nuovo l'array */
            if (id < 0) continue;
            if (id != k) swap(rm, k, id);
            k++;
        }
        for (int j = 0; j < *(int32_t*)(rm + 0x50); j++) {          /* 7d: 20 entry */
            uint8_t* e = race8::EntryById(rm, j);
            if (e && (uint32_t)*(int32_t*)(e + 0x4c) <= 7) mk8dx::Fn<void (*)(uint8_t*)>(0x8653cc)(e);
        }
        return k;
    }
};
/* B3: 0x86a594(rm, entryId) -> GamePlayer libero (indice +0x768 non usato da nessun'altra entry), fra 0..7 */
HOOK_DEFINE_REPLACE(AssignGamePlayerHook) {
    static void* Callback(uint8_t* rm, uint32_t entryId) {
        uint8_t* pim = race8::Pim();
        int pc = race8::PimCount(pim);
        uint8_t** e = *(uint8_t***)(rm + 0x58);
        for (int k = 0; k < 8; k++) {
            bool used = false;
            for (uint32_t j = 0; j < *(uint32_t*)(rm + 0x50) && !used; j++) {   /* 7d: 20 entry */
                if (j == entryId || !e[j]) continue;
                uint8_t* gp = *(uint8_t**)(e[j] + 0x200);
                used = gp && *(int32_t*)(gp + 0x768) == k;
            }
            if (!used) {
                void* r = k < pc ? race8::Player(pim, k) : nullptr;
                Logging.Log("[mk8dx-8p] B: entry %u -> GamePlayer %d (%p, PIM %d)", entryId, k, r, pc);
                return r;
            }
        }
        return nullptr;
    }
};
/* G1/G3: 0xca3e0 (PIM vt+0xc8, initPlayers): buffer esterno a 8 e GamePlayer 4..7 come 0xca4b0-0xca52c */
HOOK_DEFINE_TRAMPOLINE(InitPlayersHook) {
    static void Callback(uint8_t* pim) {
        Orig(pim);
        int n = race8::PimCount(pim);
        if (n != 4 || *(int32_t*)(pim + 0x54) != 4) { Logging.Log("[mk8dx-8p] B: PIM giocatori %d/%d inattesi, niente G1", n, *(int32_t*)(pim + 0x54)); return; }
        for (int i = 0; i < 4; i++) race8::pl[i] = race8::Player(pim, i);
        *(void***)(pim + 0x58) = race8::pl; *(int32_t*)(pim + 0x54) = 8;
        for (int idx = 4; idx < 8; idx++) {
            uint8_t* p = mk8dx::Fn<uint8_t* (*)(size_t, int)>(0x5faae0)(0x7b8, 8);
            if (!p) { Logging.Log("[mk8dx-8p] B: alloc GamePlayer %d fallita", idx); return; }
            mk8dx::Fn<void (*)(void*)>(0xcb5b4)(p);
            *(uint8_t**)(p + 0x28) = pim; *(uintptr_t*)(p + 0x30) = *(uintptr_t*)(pim + 0x30);
            int32_t& cnt = *(int32_t*)(pim + 0x4c);
            void** buf = *(void***)(pim + 0x40);
            void** slot = (*(int32_t*)(pim + 0x38) > cnt) ? &buf[cnt] : &buf[0];     /* lista figli (G2: cap 9) */
            if (!*slot) { *slot = p; cnt++; } else Logging.Log("[mk8dx-8p] B: lista figli PIM piena (%d/%d)", cnt, *(int32_t*)(pim + 0x38));
            mk8dx::Fn<void (*)(void*, int)>(0x7cdfac)(p, idx);                        /* +0x768 = idx */
            int32_t& c = *(int32_t*)(pim + 0x50);
            if (c < *(int32_t*)(pim + 0x54)) race8::pl[c++] = p;
        }
        race8::pim8 = race8::PimCount(pim) == 8;
        Logging.Log("[mk8dx-8p] B: giocatori PIM %d/%d, figli %d/%d, pim8=%d", race8::PimCount(pim), *(int32_t*)(pim + 0x54),
                    *(int32_t*)(pim + 0x4c), *(int32_t*)(pim + 0x38), (int)race8::pim8);
    }
};
/* G4: 0x7c5340(pim, bit, set), 0x7c6218 (|= 8), 0x7c62bc (&= ~8) su +0x270 (e +0x610) dei giocatori 0..3 senza count */
HOOK_DEFINE_TRAMPOLINE(PimBitHook) {
    static void Callback(uint8_t* pim, uint32_t bit, uint32_t set) {
        Orig(pim, bit, set);
        for (int i = 4; i < race8::PimCount(pim); i++) { uint32_t& f = *(uint32_t*)(race8::Player(pim, i) + 0x270);
            f = (set & 1) ? (f | (1u << bit)) : (f & ~(1u << bit)); }
    }
};
HOOK_DEFINE_TRAMPOLINE(PimSet8Hook) {
    static void Callback(uint8_t* pim) { Orig(pim);
        for (int i = 4; i < race8::PimCount(pim); i++) { uint8_t* p = race8::Player(pim, i);
            *(uint32_t*)(p + 0x270) |= 8; *(uint32_t*)(p + 0x610) |= 8; } }
};
HOOK_DEFINE_TRAMPOLINE(PimClr8Hook) {
    static void Callback(uint8_t* pim) { Orig(pim);
        for (int i = 4; i < race8::PimCount(pim); i++) { uint8_t* p = race8::Player(pim, i);
            *(uint32_t*)(p + 0x270) &= ~8u; *(uint32_t*)(p + 0x610) &= ~8u; } }
};
/* G5: 0x7c5044(pim, w1, int* x2): replica di 0x7c5050-0x7c532c per i giocatori 4..count-1 */
HOOK_DEFINE_TRAMPOLINE(PimFlagsHook) {
    static void Callback(uint8_t* pim, uint32_t w1, int32_t* x2) {
        int32_t v2 = x2 ? *x2 : 0;
        Orig(pim, w1, x2);
        bool b0 = ((uint32_t)(v2 - 7) < 2) || !(w1 & 1);
        uint8_t v = (w1 & 1) ? *(uint8_t*)(mk8dx::Fn<uint8_t* (*)()>(0x86f7bc)() + 0x26) : 0;
        for (int i = 4; i < race8::PimCount(pim); i++) {
            uint8_t* p = race8::Player(pim, i); uint32_t& f = *(uint32_t*)(p + 0x270);
            f = b0 ? (f | 1) : (f & ~1u); f &= ~6u;
            mk8dx::Fn<void (*)(void*)>(0x7ce140)(p);
            p[0x39b] = v; p[0x275] = v;
        }
    }
};
/* G6: 0x7c60c0(pim, w1) -> bool: un giocatore in attesa (vt+0x90) con +0x778 != -1 e < w1 ha la precedenza */
HOOK_DEFINE_TRAMPOLINE(PimOrderHook) {
    static bool Callback(uint8_t* pim, int32_t w1) {
        bool r = Orig(pim, w1);
        if (!r || w1 < 0) return r;
        for (int i = 4; i < race8::PimCount(pim); i++) {
            uint8_t* p = race8::Player(pim, i);
            bool waiting = (*(bool (**)(void*))(*(uintptr_t*)p + 0x90))(p);
            int32_t o = *(int32_t*)(p + 0x778);
            if (waiting && o != -1 && o < w1) return false;
        }
        return r;
    }
};
/* G8: getControllerByOrder(0x11, n): ordine 4 = handheld, quindi i locali/npad 4..7 sono agli ordini 5..8 */
HOOK_DEFINE_TRAMPOLINE(BindByLocalHook) {              /* 0x7ce38c(gp, idx, a, b, c) */
    static void Callback(void* gp, int32_t idx, uint32_t a, uint32_t b, uint32_t c) {
        int32_t n = (idx >= 4 && idx < 8) ? idx + 1 : idx;
        if (n != idx) Logging.Log("[mk8dx-8p] B: bind locale %d -> ordine %d", idx, n);
        Orig(gp, n, a, b, c);
    }
};
HOOK_DEFINE_TRAMPOLINE(BindByOrderHook) {              /* 0x7ce8c4(gp, n, x2) */
    static void Callback(void* gp, int32_t n, void* x2) { if (n >= 4) Logging.Log("[mk8dx-8p] B: bind per ordine gp=%p n=%d -> %d", gp, n, n + 1); Orig(gp, (n >= 4 && n < 8) ? n + 1 : n, x2); }
};
/* B4: rilegatura per indice locale (0x86a998, dopo), flag +0x76d (0x86adec / 0x86ad18, prima) per i locali 4..7 */
HOOK_DEFINE_TRAMPOLINE(RebindByIndexHook) {
    static void Callback(uint8_t* rm, uint32_t a, uint32_t b) {
        Orig(rm, a, b);
        for (int l = 4; l < 8; l++) {
            uint8_t* e = race8::Entry(rm, l);
            if (e && *(void**)(e + 0x200)) {
                Logging.Log("[mk8dx-8p] B: rilega locale %d (entry %d)", l, race8::Locals(rm)[l]);
                mk8dx::Fn<void (*)(uint8_t*, int, uint32_t, uint32_t, uint32_t)>(0x8652f8)(e, l, a & 1, b & 1, 0);
            }
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(RebindFlagSetHook) {            /* 0x86adec(rm, a, b): gp+0x76d = 1 poi (se +0x1fd) 0x86a998 */
    static void Callback(uint8_t* rm, uint32_t a, uint32_t b) {
        for (int l = 4; l < 8; l++) if (uint8_t* e = race8::Entry(rm, l)) if (*(void**)(e + 0x200)) mk8dx::Fn<void (*)(uint8_t*)>(0x8654a0)(e);
        Orig(rm, a, b);
    }
};
HOOK_DEFINE_TRAMPOLINE(RebindFlagClrHook) {            /* 0x86ad18(rm): gp+0x76d = 0 */
    static void Callback(uint8_t* rm) {
        for (int l = 4; l < 8; l++) if (uint8_t* e = race8::Entry(rm, l)) if (*(void**)(e + 0x200)) mk8dx::Fn<void (*)(uint8_t*)>(0x865494)(e);
        Orig(rm);
    }
};
/* Log: dopo il riempimento del menu (0x869180(rm, N, 1, 0)) */
HOOK_DEFINE_TRAMPOLINE(RaceCfgSetupHook) {
    static void Callback(uint8_t* rm, int32_t n, uint32_t a, uint32_t b) {
        { static int k; if (k++ < 20) Logging.Log("[mk8dx-8p] B 0x869180(N=%d, %u, %u) da main+%lx tipoScena=%d", n, a, b,
                                                    (uintptr_t)__builtin_return_address(0) - mk8dx::Base(), rlm::sceneType); }
        Orig(rm, n, a, b);
        if (race8::mapOk) race8::LogLocals("0x869180");
    }
};
/* H-A2-0 (report/step5_plan_A2.md §6.1): con N>4 i cicli i<N di 0x4b7218 passano a 0x3fd5a4 la "pagina CharaWin"
 * 0x28+i (per i>=4: pagine 0x2c.. -> NULL) e 0x3fdd70 legge l'entry del pad i (NULL per il pad tocco): 35 accessi
 * nulli nel log Eden dello step 4. Finche' gli slot 4..7 non hanno pad/entry veri si limitano a 4. */
namespace sel8 { static bool slotsReady; /* S5.2: pad UI 4..7 con entry valide */ }
HOOK_DEFINE_TRAMPOLINE(ProfileToCharaWinHook) {       /* 0x3fd5a4(cfg+0x1fa4, CharaWin+0x1d0, i) */
    static void Callback(int32_t* field, uint8_t* obj, int32_t i) {
        if (i >= 4 || !obj) { Logging.Log("[mk8dx-8p] A2: 0x3fd5a4 saltata (i=%d obj=%p)", i, obj); return; }
        Orig(field, obj, i);
    }
};
HOOK_DEFINE_TRAMPOLINE(CharselProfileHook) {         /* 0x4b7218 (vt94 charsel): cicli i<N su CharaWin(0x28+i)+0x1d0 -> NULL per i>=4 */
    static void Callback(uint8_t* page) {
        uint8_t* cfg = menu8::Cfg(); int32_t n = cfg ? *(int32_t*)(cfg + 0x1fa4) : 0;
        if (n > 4) { *(int32_t*)(cfg + 0x1fa4) = 4; Orig(page); *(int32_t*)(cfg + 0x1fa4) = n; }
        else Orig(page);
    }
};
/* step 7 B (report/step7_plan_hud8.md §B): gli aiuti degli ospiti sono salvati da 0x3fe040 / caricati da 0x3fdd70 in tre array int[4]
   cfg+0x2310 (Y), +0x2320 (L), +0x2330 (R) senza clamp: con i = 4..7 si scriveva/leggeva fuori limiti (aiuti di P2-P4, struttura a
   cfg+0x2340 con un puntatore). Ora entrambe sono limitate a 4 e i giocatori 4..7 usano un array ombra nel modulo (solo RAM). */
namespace aid8 {
    static uint8_t code[3][4];                            /* [0]=Y(+0x208) [1]=L(+0x20c) [2]=R(+0x209); 0 = default, 1 = on, 2 = off */
    static uint8_t* GpOf(int i) { uint8_t* pad = mk8dx::Fn<uint8_t* (*)(int)>(0x8bf158)(i);
                                  return pad ? mk8dx::Fn<uint8_t* (*)(uint8_t*)>(0x8be068)(pad) : nullptr; }
    static void Apply(uint8_t* cfg, int i) {
        uint8_t* gp = GpOf(i); if (!gp) return;
        uint8_t* kart = *(uint8_t**)(gp + 0x200);
        bool ctl3 = kart && *(int32_t*)(kart + 0x73c) == 3;                        /* default di Y e R */
        int a = code[0][i - 4], b = code[1][i - 4], c = code[2][i - 4];
        bool y = a ? a == 1 : ctl3;
        gp[0x208] = y; if (kart) mk8dx::Fn<void (*)(uint8_t*, bool)>(0x7ce99c)(kart, y);
        bool off = *(int32_t*)(cfg + 0x1fb0) == 1 || mk8dx::Fn<bool (*)()>(0x4247dc)();
        *(int32_t*)(gp + 0x20c) = off ? 0 : (b ? b == 1 : 1);
        gp[0x209] = (off || mk8dx::Fn<bool (*)()>(0x422548)()) ? 0 : (c ? c == 1 : ctl3);
    }
}
HOOK_DEFINE_TRAMPOLINE(ProfileToEntriesHook) {        /* 0x3fdd70(cfg+0x1fa4): ciclo i<*field su 0x8be068(pad i) */
    static void Callback(int32_t* field) {
        p6::EnsureLoaded();
        int32_t n = *field;
        if (n <= 4) { Orig(field); return; }
        *field = 4; Orig(field); *field = n;
        if (!sel8::slotsReady) { Logging.Log("[mk8dx-8p] A2: 0x3fdd70 con N=%d limitata a 4", n); return; }
        uint8_t* cfg = (uint8_t*)field - 0x1fa4;
        for (int i = 4; i < n && i < 8; i++) aid8::Apply(cfg, i);
        Logging.Log("[mk8dx-8p] aid8: caricati aiuti 5-%d dall'ombra (R=%d,%d,%d,%d)", n, aid8::code[2][0], aid8::code[2][1], aid8::code[2][2], aid8::code[2][3]);
    }
};

/* ---- step 5 C: pad UI 4..7 veri, pad del tocco spostato all'indice 8 (report/step5_plan_C.md) -------------
 * Contenitore [[0x1306e70]]+0x58: ptr[5] (0..3 giocatori, 4 = pad del tocco). I pad extra 4..7 vivono in memoria
 * statica del modulo e vengono restituiti da 0x8bf158(4..7) solo in modalita' 5-8 (active8). Con il turno 2 della
 * selezione (S5.4) 0x8bf158(0..3) restituisce i pad extra: i giocatori 5-8 usano i 4 cursori del gioco. */
namespace padui {
    constexpr int Extra = 4;
    alignas(16) static uint8_t mem[Extra][0x2c8];
    uint8_t* extra[Extra];
    static bool active8;                                 /* N in 5..8 e pad extra creati (aggiornato per frame) */
    static bool round2;                                  /* S5.4: turno 2, pad 0..3 -> extra */
    static uint8_t** Cont() { return mk8dx::Fn<uint8_t** (*)()>(0x8bf09c)(); }
    [[maybe_unused]] static uint8_t* Get(int i) { return mk8dx::Fn<uint8_t* (*)(int)>(0x8bf158)(i); }   /* usato da S5.4 */
}
HOOK_DEFINE_TRAMPOLINE(PadSetHook) {                     /* 0x8bf040(container, i, pad): pad+8 = i; ptr[i] = pad */
    static void Callback(uint8_t** c, int32_t i, uint8_t* pad) {
        Orig(c, i, pad);
        if (i != 4 || !pad) return;
        *(int32_t*)(pad + 0x8) = 8;                      /* pad del tocco: fuori da 0..7 (cmp #7 dei lettori) */
        if (!padui::extra[0]) for (int k = 0; k < padui::Extra; k++) {
            uint8_t* p = padui::mem[k];
            mk8dx::Fn<void (*)(void*)>(0x8bde24)(p);    /* ctor: +0x1c (pulsanti) = +0x1d (tocco) = 1 */
            *(int32_t*)(p + 0x8) = 4 + k;
            mk8dx::Fn<void (*)(void*, bool)>(0x8be478)(p, false);   /* tocco spento come i pad 1..3 */
            padui::extra[k] = p;
        }
        Logging.Log("[mk8dx-8p] padui: cont=%p pad0..4=%p %p %p %p %p (tocco idx->8), extra=%p..%p", c, c[0], c[1], c[2], c[3], c[4],
                    padui::extra[0], padui::extra[3]);
    }
};
HOOK_DEFINE_REPLACE(GetPadHook) {                        /* 0x8bf158(i) -> pad UI */
    static uint8_t* Callback(int32_t i) {
        uint8_t** c = padui::Cont();
        uint32_t u = (uint32_t)i;
        if (u < 4) return (padui::round2 && padui::active8) ? padui::extra[u] : c[u];
        if (u == 8) return c[4];                         /* pad del tocco (siti C2) */
        if (u < 8 && padui::active8 && padui::extra[u - 4]) {
            static uintptr_t seen[16]; static int nSeen;
            uintptr_t lr = (uintptr_t)__builtin_return_address(0) - mk8dx::Base();
            bool nw = true;
            for (int k = 0; k < nSeen; k++) if (seen[k] == lr) nw = false;
            if (nw && nSeen < 16) { seen[nSeen++] = lr; Logging.Log("[mk8dx-8p] padui: GetPad(%d) da main+%lx", i, lr); }
            return padui::extra[u - 4];
        }
        return u == 4 ? c[4] : c[0];                     /* clamp originale (cmp #5) */
    }
};
HOOK_DEFINE_TRAMPOLINE(PadCalcAllHook) {                 /* 0x8bf058(container): calc dei 5 pad, ogni frame del menu */
    static void Callback(uint8_t** c) {
        Orig(c);
        int n = menu8::LocalCount();
        bool a = n > 4 && n <= 8 && padui::extra[0];
        if (a != padui::active8) Logging.Log("[mk8dx-8p] padui: active8 %d -> %d (N=%d)", (int)padui::active8, (int)a, n);
        padui::active8 = a;
        sel8::slotsReady = a;
        if (!a) { padui::round2 = false; return; }
        static uint32_t lastTrig[padui::Extra];
        for (int k = 0; k < padui::Extra; k++) {
            uint8_t* p = padui::extra[k];
            mk8dx::Fn<void (*)(void*)>(0x8be674)(p);
            uint32_t t = *(uint32_t*)(p + 0x10);
            if (t && t != lastTrig[k]) Logging.Log("[mk8dx-8p] padui: pad UI %d trig=%08x hold=%08x", 4 + k, t, *(uint32_t*)(p + 0xc));
            lastTrig[k] = t;
        }
        /* fusione (+0x2c5, pad 0 in gara): 0x8beb84 fonde solo ptr[0..3]; aggiunge i giocatori 4..7 con la stessa guardia di 0x8be6a8 */
        uint8_t* rm = race8::RaceMgr();
        if (!rm || mk8dx::Fn<bool (*)(void*)>(0x86a958)(rm)) return;
        for (int j = 0; j < 5; j++) {
            uint8_t* p = c[j];
            if (!p || !p[0x1c] || !p[0x2c5]) continue;
            for (int k = 0; k < padui::Extra; k++) {
                uint8_t* ctl = mk8dx::Fn<uint8_t* (*)(void*)>(0x8be0bc)(padui::extra[k]);   /* gp+0x3d8 */
                if (!ctl) continue;
                *(uint32_t*)(p + 0xc)  |= *(uint32_t*)(ctl + 0x114);
                *(uint32_t*)(p + 0x10) |= *(uint32_t*)(ctl + 0x8);
                *(uint32_t*)(p + 0x14) |= *(uint32_t*)(ctl + 0x8) | *(uint32_t*)(ctl + 0x10);
            }
        }
    }
};

/* ---- step 5 A1-M1: layout 6pl/8pl della pagina CharaParts (report/step5_plan_A1.md §1.4, §4.1) -----------------
 * La pagina CharaParts (id 0x16 = Multi, [page+0xe8]) prende il nome del Page_Lyt dallo slot 13 (0x4be854); il pane
 * "mn_CharaKartSlct_Multi" ha lo stesso nome nei Page_Lyt 6pl/8pl, che puntano ai file mn_CharaKartSlct_6pl/8pl.
 * Il numero di L_Name_0i e' w20 = 4 fisso a 0x4bea58: a 0x4beb04 (confluenza) lo si porta a 6/8. */
namespace p4 {                   /* step 5b passo 4: stato (il resto piu' avanti) */
    constexpr int32_t kNamesId = 0x12;             /* DebugLocalRoom: mai registrato dal retail; in gara lo riusa hud8 */
    static bool on, reg, one, namesOpen;
    static uintptr_t regFac; static uint8_t* regOwner;
    static uint8_t* namesPage;                     /* pagina nomi del menu (mgr+0x30[0x12] viene riscritto e azzerato dalla gara) */   /* voce della tabella pagine per 0x12 nel menu (la gara la riusa per hud8) */
}
namespace sel8 { static bool s54, a1; }
namespace p5 {                   /* step 5b passo 5: stato (il resto piu' avanti) */
    static bool on, building, isB, fromSel;
    static uint8_t* built;                       /* pagina kart con set B costruito (nullptr = no) */
    static void BuildB(uint8_t* pg);
}      /* blocchi S5.4 e A1-M1 installati */
/* ---- step 5c opzione B (report/step5c_8pl_plan.md §3): selezione Multi legata alla pagina 8pl 0x12 con un secondo set di controlli
 * (come p5 per la kart), scelto all'apertura della pagina (0x8bb3e8) prima del ripristino del cursore e dell'onEnter; anteprime 3D a 8
 * legate alla pagina aperta (opzione A). 0 = comportamento del passo 4 (0x12 sovrapposta alla 0x16). */
#ifndef MK8DX_SEL8B
#define MK8DX_SEL8B 0
#endif
namespace sel8b {
    static bool on;                              /* PageEnterHook installato */
    static uint8_t* built;                       /* selezione con set B costruito (nullptr = no) */
    static bool isB;                             /* la selezione usa il set B (0x12) */
    static uint8_t* selPg;                       /* selezione Multi vista da PageEnterHook (per H7/M2) */
    static uint8_t* entered;                     /* PageEnterHook ha gia' scelto il set per questo ingresso (consumato dall'onEnter) */
    static bool selOpen, kartOpen;               /* H7: selezione / kart B nella lista pagine aperte (lato UI) */
    static unsigned long openAt = ~0ul;          /* m3d8::frames all'ultimo aggiornamento di selOpen/kartOpen */
    static void BuildB(uint8_t* nm);
    static void Frame(uint8_t* mgr);
}
namespace sel8 {
    static constexpr const char* kPageLyt6 = "mn_Page_Lyt_CharaParts_6pl";
    static constexpr const char* kPageLyt8 = "mn_Page_Lyt_CharaParts_8pl";
    static int built;                                    /* slot del layout CharaParts costruito: 4 / 6 / 8 */
#ifndef MK8DX_LYT8
#define MK8DX_LYT8 0      /* 0 = per N al momento della costruzione (inefficace: il menu costruisce le pagine con N=1); 6/8 = layout forzato */
#endif
    static int Slots() {
        if (MK8DX_LYT8 == 8 || MK8DX_LYT8 == 6) return MK8DX_LYT8;
        int n = menu8::LocalCount(); return (n >= 7 && n <= 8) ? 8 : (n >= 5 ? 6 : 4);
    }
}
/* 0x4be8d0 (onCreate CharaParts): il gioco imposta l'animazione "lato" dei pannelli L_Name solo per 2 o 4 (0x4bed2c:
 * frame 1,0,1,0 via 0x8ad3c4([obj+0x70](+0x18 se [obj+0x68]>3), 0, f)); con 6/8 pannelli lo si fa qui. */
HOOK_DEFINE_TRAMPOLINE(CharaPartsCreateHook) {
    static void Callback(uint8_t* page) {
        Orig(page);
        if (!page) return;
        int32_t cid = *(int32_t*)(page + 0xe8);
        if (!((cid == 0x16 && sel8::built > 4) || (p4::on && cid == p4::kNamesId))) return;
        if (p5::on && cid == p4::kNamesId) { int32_t k = 0x31; p5::BuildB(mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&k)); }
        if (MK8DX_SEL8B && sel8b::on && cid == p4::kNamesId) sel8b::BuildB(page);      /* step 5c H2: set B della selezione sulla 0x12 */
        int n = *(int32_t*)(page + 0x1e8); uint8_t** arr = *(uint8_t***)(page + 0x1f0);
        uint8_t* part = *(uint8_t**)(page + 0x1d0);
        Logging.Log("[mk8dx-8p] A1: CharaParts creata: parte=%p layout=%p L_Name=%d", part, part ? *(void**)(part + 0x40) : nullptr, n);
        for (int i = 0; i < n && i < 8 && arr; i++) {
            uint8_t* obj = arr[i]; if (!obj) continue;
            uint8_t* p = *(uint8_t**)(obj + 0x70); if (*(int32_t*)(obj + 0x68) > 3) p += 0x18;
            void* anim = *(void**)p;
            if (anim) mk8dx::Fn<void (*)(void*, int, float)>(0x8ad3c4)(anim, 0, (i & 1) ? 0.f : 1.f);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(CharaPartsLayoutNameHook) {       /* 0x4be854 (slot 13 pagina CharaParts) -> const char* */
    static const char* Callback(uint8_t* page) {
        const char* r = Orig(page);
        int32_t pid = page ? *(int32_t*)(page + 0xe8) : -1;
        if (p4::on && pid == p4::kNamesId) return sel8::kPageLyt8;              /* passo 4: la 0x12 e' 8pl, la 0x16 resta Multi */
        if (p4::on && pid == 0x16) { sel8::built = 4; return r; }
        if (page && *(int32_t*)(page + 0xe8) == 0x16) {
            int s = sel8::Slots();
            sel8::built = s;
            if (s == 8) r = sel8::kPageLyt8; else if (s == 6) r = sel8::kPageLyt6;
            static const char* last;
            if (r != last) {
                last = r;
                Logging.Log("[mk8dx-8p] A1: layout CharaParts = %s (N=%d)", r, menu8::LocalCount());
                mk8dx::LogBacktrace("A1 layout", __builtin_frame_address(0), 6);
            }
        }
        return r;
    }
};
HOOK_DEFINE_INLINE(CharaPartsSlotCountHook) {            /* 0x4beb04 (onCreate CharaParts, x19 = pagina, w20 = n slot) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* page = reinterpret_cast<uint8_t*>(ctx->X[19]);
        if (page && p4::on && *(int32_t*)(page + 0xe8) == p4::kNamesId) { if (ctx->W[20] == 4) ctx->W[20] = 8; return; }
        if (!page || *(int32_t*)(page + 0xe8) != 0x16) return;
        if (sel8::built > 4 && ctx->W[20] == 4) {
            ctx->W[20] = sel8::built;
            Logging.Log("[mk8dx-8p] A1: CharaParts con %d slot (N=%d)", sel8::built, menu8::LocalCount());
        }
    }
};

/* ---- step 5b passo 1 (A1-M2): array per slot +0x904 [4] della classe selezione rilocato in coda a +0xa28 [8] ----------
 * (report/step5b_plan.md §2). Oggetto 0xa28 -> 0xa68 (3 fabbriche), 72 accessi spostati; i 39 clamp `cmp #4` restano a 4
 * fino al passo 4 (12 usano lo stesso indice su altri array a 4). Vecchia area +0x904..+0x923 = canary. */
namespace sel8 {
    struct P { uint32_t off, orig, repl; };
    static constexpr uint32_t kCanary = 0xA1A1A1A1;
    static uint32_t slotOff = 0x904;                     /* +0xa28 con la rilocazione attiva */
    static bool m2;
    /* step5b passo 1 (A1-M2): 3 fabbriche + 72 accessi = 75 parole, verificate su main.bin (verify_114.txt) */
    static const sel8::P kReloc904[] = {
        {0x4091c4, 0x52814500, 0x52814d00},   /* mov w0,#0xa28 -> #0xa68 */
        {0x4091f0, 0x52814500, 0x52814d00},   /* mov w0,#0xa28 -> #0xa68 */
        {0x40921c, 0x52814500, 0x52814d00},   /* mov w0,#0xa28 -> #0xa68 */
        {0x4b111c, 0xb9490509, 0xb94a2909},   /* gift     ldr w9, [x8, #0x904] */
        {0x4b112c, 0xb9490509, 0xb94a2909},   /* gift     ldr w9, [x8, #0x904] */
        {0x4b1134, 0xb9490509, 0xb94a2909},   /* gift     ldr w9, [x8, #0x904] */
        {0x4b1148, 0xb9490509, 0xb94a2909},   /* gift     ldr w9, [x8, #0x904] */
        {0x4b1158, 0xb9490d09, 0xb94a3109},   /* gift     ldr w9, [x8, #0x90c] */
        {0x4b1160, 0xb9490d09, 0xb94a3109},   /* gift     ldr w9, [x8, #0x90c] */
        {0x4b1174, 0xb9490d09, 0xb94a3109},   /* gift     ldr w9, [x8, #0x90c] */
        {0x4b1184, 0xb9491509, 0xb94a3909},   /* gift     ldr w9, [x8, #0x914] */
        {0x4b118c, 0xb9491509, 0xb94a3909},   /* gift     ldr w9, [x8, #0x914] */
        {0x4b11a0, 0xb9491509, 0xb94a3909},   /* gift     ldr w9, [x8, #0x914] */
        {0x4b11b0, 0xb9491d09, 0xb94a4109},   /* gift     ldr w9, [x8, #0x91c] */
        {0x4b11b8, 0xb9491d09, 0xb94a4109},   /* gift     ldr w9, [x8, #0x91c] */
        {0x4b11cc, 0xb9491d08, 0xb94a4108},   /* gift     ldr w8, [x8, #0x91c] */
        {0x4b1d2c, 0xb9490508, 0xb94a2908},   /* gift     ldr w8, [x8, #0x904] */
        {0x4b564c, 0xb9090509, 0xb90a2909},   /* charasel str w9, [x8, #0x904] */
        {0x4b80c8, 0xb909066a, 0xb90a2a6a},   /* charasel str w10, [x19, #0x904] */
        {0x4b80ec, 0x9124126d, 0x9128a26d},   /* charasel add x13, x19, #0x904 */
        {0x4b81dc, 0xb909058b, 0xb90a298b},   /* charasel str w11, [x12, #0x904] */
        {0x4b81f8, 0xb909055f, 0xb90a295f},   /* charasel str wzr, [x10, #0x904] */
        {0x4b8230, 0xb909059f, 0xb90a299f},   /* charasel str wzr, [x12, #0x904] */
        {0x4b8248, 0xb909057f, 0xb90a297f},   /* charasel str wzr, [x11, #0x904] */
        {0x4b8274, 0xb909059f, 0xb90a299f},   /* charasel str wzr, [x12, #0x904] */
        {0x4b8284, 0xb909055f, 0xb90a295f},   /* charasel str wzr, [x10, #0x904] */
        {0x4b833c, 0xb9490508, 0xb94a2908},   /* charasel ldr w8, [x8, #0x904] */
        {0x4b87e0, 0xb9490529, 0xb94a2929},   /* charasel ldr w9, [x9, #0x904] */
        {0x4b9028, 0xb9490508, 0xb94a2908},   /* charasel ldr w8, [x8, #0x904] */
        {0x4b94e8, 0xb9490509, 0xb94a2909},   /* charasel ldr w9, [x8, #0x904] */
        {0x4b94f8, 0x91241117, 0x9128a117},   /* charasel add x23, x8, #0x904 */
        {0x4b953c, 0xb9490668, 0xb94a2a68},   /* charasel ldr w8, [x19, #0x904] */
        {0x4b9b84, 0xb9490508, 0xb94a2908},   /* charasel ldr w8, [x8, #0x904] */
        {0x4ba410, 0xb9490788, 0xb94a2b88},   /* charasel ldr w8, [x28, #0x904] */
        {0x4ba428, 0x9124139a, 0x9128a39a},   /* charasel add x26, x28, #0x904 */
        {0x4ba4ec, 0xb9490b88, 0xb94a2f88},   /* charasel ldr w8, [x28, #0x908] */
        {0x4ba518, 0x91242389, 0x9128b389},   /* charasel add x9, x28, #0x908 */
        {0x4ba55c, 0xb9090b9f, 0xb90a2f9f},   /* charasel str wzr, [x28, #0x908] */
        {0x4ba5b0, 0xb9490788, 0xb94a2b88},   /* charasel ldr w8, [x28, #0x904] */
        {0x4ba5c8, 0x9124139a, 0x9128a39a},   /* charasel add x26, x28, #0x904 */
        {0x4ba68c, 0xb9490b88, 0xb94a2f88},   /* charasel ldr w8, [x28, #0x908] */
        {0x4ba6a4, 0x91242389, 0x9128b389},   /* charasel add x9, x28, #0x908 */
        {0x4ba6e8, 0xb9090b9f, 0xb90a2f9f},   /* charasel str wzr, [x28, #0x908] */
        {0x4ba86c, 0x91241114, 0x9128a114},   /* charasel add x20, x8, #0x904 */
        {0x4ba870, 0xb9490508, 0xb94a2908},   /* charasel ldr w8, [x8, #0x904] */
        {0x4bb114, 0xb9490508, 0xb94a2908},   /* charasel ldr w8, [x8, #0x904] */
        {0x4bb47c, 0xb9490508, 0xb94a2908},   /* charasel ldr w8, [x8, #0x904] */
        {0x4bc028, 0xb9090515, 0xb90a2915},   /* charasel str w21, [x8, #0x904] */
        {0x4bc958, 0xb909067f, 0xb90a2a7f},   /* charasel str wzr, [x19, #0x904] */
        {0x4bc95c, 0xb9090e7f, 0xb90a327f},   /* charasel str wzr, [x19, #0x90c] */
        {0x4bc960, 0xb909167f, 0xb90a3a7f},   /* charasel str wzr, [x19, #0x914] */
        {0x4bc964, 0xb9091e7f, 0xb90a427f},   /* charasel str wzr, [x19, #0x91c] */
        {0x4c04d8, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c0664, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c0ddc, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c0e78, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c0f1c, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c0ff0, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c1564, 0xb949054a, 0xb94a294a},   /* kart     ldr w10, [x10, #0x904] */
        {0x4c18dc, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c19f8, 0xb9490529, 0xb94a2929},   /* kart     ldr w9, [x9, #0x904] */
        {0x4c1ab0, 0xb9490529, 0xb94a2929},   /* kart     ldr w9, [x9, #0x904] */
        {0x4c1b98, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c27cc, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c2870, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c2a1c, 0xb949058c, 0xb94a298c},   /* kart     ldr w12, [x12, #0x904] */
        {0x4c2aa8, 0xb949058c, 0xb94a298c},   /* kart     ldr w12, [x12, #0x904] */
        {0x4c3004, 0xb949058c, 0xb94a298c},   /* kart     ldr w12, [x12, #0x904] */
        {0x4c3138, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c31ac, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c3210, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c3518, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
        {0x4c3d80, 0xb949058c, 0xb94a298c},   /* kart     ldr w12, [x12, #0x904] */
        {0x4c3edc, 0xb949058c, 0xb94a298c},   /* kart     ldr w12, [x12, #0x904] */
        {0x4c4528, 0xb9490508, 0xb94a2908},   /* kart     ldr w8, [x8, #0x904] */
    };
}
#ifndef MK8DX_P3TEST
#define MK8DX_P3TEST 0    /* 1 = prova del passo 3: 8 cursori in 5-8P al turno 1, eventi di P5-P8 scartati */
#endif
/* step5b passo 3 (p3_verify.py su main.bin): 20 clamp `cmp #4` -> `#8` con hook PRE sul cmp (sorgente := V(cur,p)) */
namespace cur8 {
    struct C { uint32_t off, orig, repl; };
    static const C kClampCmp[] = {
        {0x8ae1e4, 0x7100103f, 0x7100203f},   /* w1   x8   cur=X0        csel x8, x8, xzr, lo ; F1 0x8ae1d0(item,p) seleziona se non selezionato */
        {0x8ae228, 0x7100103f, 0x7100203f},   /* w1   x10  cur=X9        csel x10, x10, xzr, lo ; F2 0x8ae21c(item,p) item selezionato da p */
        {0x8ae274, 0x7100103f, 0x7100203f},   /* w1   x9   cur=X0        csel x9, x9, xzr, lo ; F3 0x8ae264(item,p) rilascia se selezionato */
        {0x8ae334, 0x7100103f, 0x7100203f},   /* w1   x10  cur=X9        csel x10, x10, xzr, lo ; F4 0x8ae328(item,p) deciso da p */
        {0x8ae64c, 0x7100129f, 0x7100229f},   /* w20  x8   cur=X19+0x60  csel x8, x8, xzr, lo ; F5 0x8ae628(item,p) posa cursore grafico [gfx] */
        {0x8af474, 0x710012bf, 0x710022bf},   /* w21  x10  cur=X19       csel x11, x10, xzr, lo ; F6 0x8af3f4 seleziona: stato */
        {0x8af490, 0x710012bf, 0x710022bf},   /* w21  x10  cur=X19       csel x27, x10, xzr, lo ; F7 0x8af3f4 seleziona: x27 [slot+memtab+gfx] */
        {0x8afb48, 0x7100127f, 0x7100227f},   /* w19  x9   cur=X20       csel x9, x9, xzr, lo ; F8 0x8afaf0 rilascia */
        {0x8afc1c, 0x7100103f, 0x7100203f},   /* w1   x8   cur=X0        csel x8, x8, xzr, lo ; F9 0x8afbec ripristina idx [memtab] */
        {0x8aff70, 0x7100103f, 0x7100203f},   /* w1   x8   cur=X0        csel x8, x8, xzr, lo ; F10 0x8aff64 perdita focus */
        {0x8affcc, 0x7100129f, 0x7100229f},   /* w20  x8   cur=X19       csel x21, x8, xzr, lo ; F11 0x8affa4 decide [gfx] */
        {0x8b0040, 0x7100103f, 0x7100203f},   /* w1   x8   cur=X0        csel x8, x8, xzr, lo ; F12 0x8b0034 fine decide */
        {0x8b0108, 0x7100127f, 0x7100227f},   /* w19  x8   cur=X20       csel x22, x8, xzr, lo ; F13 0x8b00e0 lettura pad [focus] */
        {0x8b0278, 0x710012bf, 0x710022bf},   /* w21  x8   cur=X19       csel x22, x8, xzr, lo ; F14 0x8b0238 disegno [focus+gfx] */
        {0x8b08a4, 0x7100127f, 0x7100227f},   /* w19  x8   cur=glob      csel x8, x8, xzr, lo ; F15 0x8b0888(x,p) cursore grafico di p [gfx] */
        {0x8ba7a4, 0x7100127f, 0x7100227f},   /* w19  x10  cur=X8        csel x10, x10, xzr, lo ; G1 0x8ba77c(page,p) evento all item del cursore */
        {0x8ba828, 0x7100103f, 0x7100203f},   /* w1   x10  cur=X8        csel x10, x10, xzr, lo ; G2 0x8ba814(page,p) item deciso */
        {0x443590, 0x7100113f, 0x7100313f},   /* w9   x9   cur=X8        csel x9, x9, xzr, lo ; G3 0x44355c ctl(+0xb8 pagina,+0xc0 giocatore) */
        {0x49dfbc, 0x7100129f, 0x7100229f},   /* w20  x8   cur=X9        csel x8, x8, xzr, lo ; G4 0x49df40 pagina +0x1e4 giocatore */
        {0x49abdc, 0x7100103f, 0x7100203f},   /* w1   x8   cur=glob      csel x8, x8, xzr, lo ; G5 0x49abc4(cp,p,f) scala cursore grafico [gfx] */
    };
    static const C kWords[] = {
        {0x8bafcc, 0x52801700, 0x52803500},   /* mov w0,#0xb8 -> #0x1a8: oggetto cursore (unica alloc, 0x8bafb8) */
        {0x408efc, 0x321d17e0, 0x52804600},   /* mov w0,#0x1f8 -> #0x230: pagina cm_Page_Cursor (unica alloc) */
        {0x49a968, 0xf1000f3f, 0xf1001f3f},   /* cmp x25,#3 -> #7: crea anche cm_L_BtnCursor_04..07 (onCreate 0x49a8fc) */
        {0x49a9c0, 0xf8367a75, 0xd503201f},   /* str x21,[x19,x22,lsl#3] -> nop (la scrittura la fa GfxStoreHook) */
    };
    struct H { uint32_t off, word; };
    static const H kHookWords[] = {
        {0x8af218, 0xf81e0ff3},   /* T ctor cursore */
        {0x8af308, 0xb940100b},   /* T reset cursore */
        {0x8afc54, 0xd10043ff},   /* T init da memtab */
        {0x8afdd0, 0xa9be4ff4},   /* T attiva slot */
        {0x8affa4, 0xf81d0ff5},   /* T conferma (decide) finestra varianti 4..7 */
        {0x8ae2a4, 0xf9403008},   /* T item selezionato da qualcuno */
        {0x8b0084, 0xf81e0ff3},   /* T lettura pad (CursorPollHook esistente) */
        {0x8b01ec, 0xf81e0ff3},   /* R disegno */
        {0x8bd6d8, 0xd105c3ff},   /* T focus per frame */
        {0x8bd5f8, 0xf81d0ff5},   /* T setFocus */
        {0x49a8fc, 0xd10203ff},   /* T onCreate cm_Page_Cursor */
        {0x49aa1c, 0xf81d0ff5},   /* T sync Loop gfx */
        {0x49ab04, 0xf81e0ff3},   /* T gfx occupati */
        {0x49ab68, 0xa9be4ff4},   /* T gfx attiva/disattiva */
        {0x8b0114, 0xf9463508},   /* I focus lettura (poll) */
        {0x8b0284, 0xf9463508},   /* I focus lettura (disegno) */
        {0x8af5d0, 0xb83b7900},   /* I memtab scrittura 1 */
        {0x8af6d8, 0xb83b7928},   /* I memtab scrittura 2 */
        {0x8afc28, 0xb8687929},   /* I memtab lettura */
        {0x49a9c0, 0xf8367a75},   /* I gfx store (dopo patch nop) */
        {0x8bb900, 0xb9408908},   /* I uscita pagina: slot 5..6 */
        {0x8bbc2c, 0xb9408908},   /* I attesa pagina: slot 5..6 */
        {0x8bbaa4, 0xb9419a75},   /* I attivazione pagina: slot 6/7 -> item vt+0x28 */
        {0x8bb568, 0xf940b268},   /* I attivazione pagina: dopo setFocus 0..3 (x19 pagina, x20 mgr) */
        {0x4bbd18, 0xd10303ff},   /* T (solo prova) vt92 selezione */
    };
}
/* ---- step 5b passo 3 (report/step5b_plan_p3.md): selettore cursori a 8 slot "virtuali". Lo slot s>=4 usa l'indice V=s+4 (cur+0x128..),
 * gli slot 0..3 non si spostano. Clamp `cmp #4`->`#8` + hook PRE: V(cur,p) = p<4 ? p : (p<8 && n==8) ? p+4 : 0 (n=4 -> clamp vanilla). */
namespace cur8 {
    constexpr uint32_t kMagic = 0x38525543;                 /* 'CUR8' a cur+0xb8 */
    static bool on;                                          /* passo 3 installato */
    static uint8_t* wideCur;                                 /* cursore largo attivo (selezione Multi), nullptr = nessuno */
    static uint8_t* foc[4];                                  /* focus dei pad 4..7 (ombra di mgr+0xc68[4]) */
    static int32_t mem8[0x88][4];                            /* ultimo indice per pagina degli slot 4..7 (ombra di (O+0x60)[0x88][4]) */
    static int32_t kFive = 5;
    inline bool Wide(const uint8_t* c) { return c && *(const uint32_t*)(c + 0xb8) == kMagic && *(const int32_t*)(c + 0xbc) == 8; }
    inline uint64_t V(const uint8_t* c, uint32_t p) { return p < 4 ? p : (p < 8 && Wide(c)) ? p + 4 : 0; }
    inline uint64_t Vg(uint32_t p) { return p < 4 ? p : (p < 8 && wideCur) ? p + 4 : 0; }
    inline uint8_t* Slot(uint8_t* c, int s) { return c + 0x28 + (s < 4 ? s : s + 4) * 0x20; }
    inline uint8_t* ItemCur(uint64_t item) { return item ? *(uint8_t**)(item + 0x60) : nullptr; }      /* 0 se non registrato (0x8ad894) */
    static uint8_t* GfxPage() { return mk8dx::Fn<uint8_t* (*)()>(0x8bf228)(); }
    static uint8_t* Gfx(int s) { uint8_t* cp = GfxPage(); return cp ? *(uint8_t**)(cp + 0x1d0 + (s < 4 ? s : s + 4) * 8) : nullptr; }
    static uintptr_t MemTab() {                              /* [[GOT 0x1306e70]]+0x60 */
        uint8_t** g = *(uint8_t***)(mk8dx::Base() + 0x1306e70); uint8_t* o = g ? *g : nullptr;
        return o ? *(uintptr_t*)(o + 0x60) : 0;
    }
    static void InitSlot(uint8_t* sl) {                      /* come 0x8af218/0x8af308 */
        const uint32_t* g = *(const uint32_t**)(mk8dx::Base() + 0x1310790);                   /* (-FLT_MAX)x3 */
        *(int32_t*)sl = 0; sl[4] = 0; *(int32_t*)(sl + 8) = 0; *(int32_t*)(sl + 0xc) = -1; *(int32_t*)(sl + 0x10) = -1;
        for (int k = 0; k < 3; k++) *(uint32_t*)(sl + 0x14 + 4 * k) = g[k];
    }
    static void ShowPanes(bool v) {                          /* pane cm_L_BtnCursor_04..07 (nel vanilla nascosti a 0x49a9cc) */
        uint8_t* cp = GfxPage(); if (!cp) return;
        char nm[] = "cm_L_BtnCursor_00";
        for (int i = 4; i < 8; i++) {
            nm[16] = '0' + i;
            uint8_t* pane = mk8dx::Fn<uint8_t* (*)(uint8_t*, const char*)>(0x725ad0)(cp, nm);   /* come 0x49a9d4 */
            if (pane) pane[0x58] = v ? (pane[0x58] | 1) : (pane[0x58] & ~1);
        }
    }
    static bool OwnsSlots(uint8_t* pg) { uint8_t* c = pg ? *(uint8_t**)(pg + 0x1b0) : nullptr; return !c || Wide(c); }
    static void SetFoc(int p, uint8_t* pg) {                 /* 0x8bd5f8 per p = 4..7, sull'ombra */
        uint8_t*& f = foc[p - 4];
        if (f == pg) return;
        if (f && OwnsSlots(f)) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8ba624)(f, p);
        f = pg;
        if (pg && OwnsSlots(pg)) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8ba614)(pg, p);
        else if (uint8_t* g = Gfx(p)) mk8dx::Fn<void (*)(uint8_t*)>(0x443290)(g);              /* pagina senza slot p: nasconde */
    }
    static void FocusExtra(uint8_t* mgr) {                   /* 0x8bd890..0x8bd970 per p = 4..7 */
        int32_t n = *(int32_t*)(mgr + 0xb50); uint8_t** list = *(uint8_t***)(mgr + 0xb58);
        for (int p = 4; p < 8; p++) {
            uint8_t* f0 = foc[p - 4];
            if (f0) {                                        /* 0x8bd894..0x8bd8e0: tiene il focus se la pagina e' ancora in lista */
                int32_t id = *(int32_t*)(f0 + 0xe8);
                uint8_t* pg = mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&id);
                bool in = false;
                for (int i = 0; pg && i < n; i++) if (list[i] == pg) { in = true; break; }
                if (in) continue;
            }
            uint8_t* top = nullptr;                          /* 0x8bd8fc..0x8bd930: dall'alto, bit p in +0x190 */
            for (int k = n - 1; k >= 0 && !top; k--) if (list[k] && ((*(uint32_t*)(list[k] + 0x190) >> p) & 1)) top = list[k];
            if (top) SetFoc(p, top);
            else if (!f0) { if (uint8_t* g = Gfx(p)) mk8dx::Fn<void (*)(uint8_t*)>(0x443290)(g); }   /* 0x8bd860..0x8bd888 */
        }
    }
    static bool AnyDeciding(uint8_t* c) {
        if (!Wide(c)) return false;
        for (int s = 4; s < 8; s++) if ((uint32_t)(*(int32_t*)Slot(c, s) - 5) < 2) return true;
        return false;
    }
    static void SetWide(uint8_t* page, bool w, int nLocal) { /* passo 4 (o prova): cursore della pagina a 8 slot */
        uint8_t* c = page ? *(uint8_t**)(page + 0x1b0) : nullptr;
        if (!on || !c || *(uint32_t*)(c + 0xb8) != kMagic) return;
        int32_t cnt = *(int32_t*)(c + 0x10); uint8_t** it = *(uint8_t***)(c + 0x18);
        uint32_t bits = 0; for (int p = 4; p < nLocal && p < 8; p++) bits |= 1u << p;
        if (w) {
            *(int32_t*)(c + 0xbc) = 8; wideCur = c;
            for (int k = 0; k < cnt; k++) if (it[k]) *(uint32_t*)(it[k] + 0x58) |= bits;      /* 0x8af87c: bit p negli item */
            ShowPanes(true);
            for (int p = 4; p < nLocal && p < 8; p++) {                                        /* come 0x8bb51c.. + 0x8afdd0 */
                if ((*(uint32_t*)(page + 0x190) >> p) & 1) SetFoc(p, page);
                uint8_t* sl = Slot(c, p);
                if (*(int32_t*)sl < 3) {
                    *(int32_t*)sl = 1;
                    mk8dx::Fn<bool (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(c, p, *(int32_t*)(sl + 8), 0, 1, 0);
                    if (*(int32_t*)sl == 1) *(int32_t*)sl = 2;
                }
            }
            Logging.Log("[mk8dx-8p] cur8: cursore %p della pagina 0x%x a 8 slot (N=%d, item=%d)", c, *(int32_t*)(page + 0xe8), nLocal, cnt);
        } else if (Wide(c)) {
            for (int p = 4; p < 8; p++) {
                InitSlot(Slot(c, p));
                if (uint8_t* g = Gfx(p)) mk8dx::Fn<void (*)(uint8_t*)>(0x443290)(g);
                if (foc[p - 4] == page) foc[p - 4] = nullptr;
            }
            for (int k = 0; k < cnt; k++) if (it[k]) *(uint32_t*)(it[k] + 0x58) &= ~0xf0u;
            *(int32_t*)(c + 0xbc) = 4; if (wideCur == c) wideCur = nullptr;
            ShowPanes(false);
            Logging.Log("[mk8dx-8p] cur8: cursore %p di nuovo a 4 slot", c);
        }
    }
    static void Reset() { wideCur = nullptr; for (auto& f : foc) f = nullptr; if (on) ShowPanes(false); }   /* SceneInitHook */
    static int cwAct, cwDraw;                              /* diagnostica passo 7 (b) */
    static int CwSlot(uint8_t* c) {                         /* cursore a 4 slot di una CharaList 0x01/0x02/0x5e/0x64 -> 4..7, altrimenti -1 */
        if (!p4::one || !c || *(uint32_t*)(c + 0xb8) != kMagic || Wide(c)) return -1;
        uint8_t* pg = *(uint8_t**)(c + 8); if (!pg) return -1;
        static constexpr int32_t kIds[4] = { 0x01, 0x02, 0x5e, 0x64 };
        int32_t id = *(int32_t*)(pg + 0xe8);
        for (int j = 0; j < 4; j++) if (kIds[j] == id) return 4 + j;
        return -1;
    }
    template <class F> static void AsWide(uint8_t* c, int s, F f) { /* slot 0 vissuto come slot V = s+4 del cursore largo */
        uint8_t* s0 = Slot(c, 0); uint8_t* sv = Slot(c, s); uint8_t keep[0x20];
        memcpy(keep, sv, 0x20); memcpy(sv, s0, 0x20);
        *(int32_t*)(c + 0xbc) = 8; f(); *(int32_t*)(c + 0xbc) = 4;
        memcpy(s0, sv, 0x20); memcpy(sv, keep, 0x20);
    }
}
#define CUR8_CLAMP(N, PAD, SRC, CUR) HOOK_DEFINE_INLINE(Cur8Clamp##N) { \
    static void Callback(exl::hook::InlineCtx* ctx) { ctx->X[SRC] = cur8::V((const uint8_t*)(CUR), ctx->W[PAD]); } };
#define CUR8_CLAMPG(N, PAD, SRC) HOOK_DEFINE_INLINE(Cur8Clamp##N) { \
    static void Callback(exl::hook::InlineCtx* ctx) { ctx->X[SRC] = cur8::Vg(ctx->W[PAD]); } };
CUR8_CLAMP(F1, 1, 8, ctx->X[0])   CUR8_CLAMP(F2, 1, 10, ctx->X[9])  CUR8_CLAMP(F3, 1, 9, ctx->X[0])   CUR8_CLAMP(F4, 1, 10, ctx->X[9])
CUR8_CLAMP(F5, 20, 8, cur8::ItemCur(ctx->X[19]))                    CUR8_CLAMP(F6, 21, 10, ctx->X[19]) CUR8_CLAMP(F7, 21, 10, ctx->X[19])
CUR8_CLAMP(F8, 19, 9, ctx->X[20]) CUR8_CLAMP(F9, 1, 8, ctx->X[0])   CUR8_CLAMP(F10, 1, 8, ctx->X[0])  CUR8_CLAMP(F11, 20, 8, ctx->X[19])
CUR8_CLAMP(F12, 1, 8, ctx->X[0])  CUR8_CLAMP(F13, 19, 8, ctx->X[20]) CUR8_CLAMP(F14, 21, 8, ctx->X[19]) CUR8_CLAMPG(F15, 19, 8)
CUR8_CLAMP(G1, 19, 10, ctx->X[8]) CUR8_CLAMP(G2, 1, 10, ctx->X[8])  CUR8_CLAMP(G3, 9, 9, ctx->X[8])   CUR8_CLAMP(G4, 20, 8, ctx->X[9])
CUR8_CLAMPG(G5, 1, 8)
#define CUR8_MEM(N, BASE, IDX) HOOK_DEFINE_INLINE(Cur8Mem##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    uint64_t v = ctx->X[IDX]; if (v >= 8 && v < 12) { uintptr_t t = cur8::MemTab(); \
    if (t) ctx->X[BASE] = ctx->X[BASE] - t + (uintptr_t)cur8::mem8 - 0x20; } } };
CUR8_MEM(A, 8, 27)   /* 0x8af5d0 str w0,[x8,x27,lsl #2] */
CUR8_MEM(B, 9, 27)   /* 0x8af6d8 str w8,[x9,x27,lsl #2] */
CUR8_MEM(C, 9, 8)    /* 0x8afc28 ldr w9,[x9,x8,lsl #2]  */
#define CUR8_FOC(N) HOOK_DEFINE_INLINE(Cur8Foc##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    uint64_t v = (ctx->X[8] - ctx->X[0]) >> 3; if (v >= 8 && v < 12) ctx->X[8] = (uintptr_t)&cur8::foc[v - 8] - 0xc68; } };
CUR8_FOC(P)          /* 0x8b0114 ldr x8,[x8,#0xc68]  (x0 = mgr) */
CUR8_FOC(D)          /* 0x8b0284 ldr x8,[x8,#0xc68]  (x0 = mgr) */
HOOK_DEFINE_INLINE(Cur8GfxStoreHook) {                     /* 0x49a9c0 (gia' nop): x19 = cm_Page_Cursor, x22 = 0x3a+i, x21 = oggetto, w1 = i+1 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* cp = (uint8_t*)ctx->X[19]; int i = (int)ctx->X[22] - 0x3a;
        if (i < 4) *(uint64_t*)(cp + ctx->X[22] * 8) = ctx->X[21];                             /* +0x1d0+i*8 come l'originale */
        else { *(uint64_t*)(cp + 0x210 + (i - 4) * 8) = ctx->X[21]; ctx->W[1] = i + 3; }      /* slot virtuale, frame 7..10 */
    }
};
HOOK_DEFINE_INLINE(Cur8ExitWaitHook) {                     /* 0x8bb900 ldr w8,[x8,#0x88] (x8 = cursore): uscita, attende slot in 5..6 */
    static void Callback(exl::hook::InlineCtx* ctx) { if (cur8::AnyDeciding((uint8_t*)ctx->X[8])) ctx->X[8] = (uintptr_t)&cur8::kFive - 0x88; }
};
HOOK_DEFINE_INLINE(Cur8IdleWaitHook) {                     /* 0x8bbc2c ldr w8,[x8,#0x88]: idem (b.lo -> attesa) */
    static void Callback(exl::hook::InlineCtx* ctx) { if (cur8::AnyDeciding((uint8_t*)ctx->X[8])) ctx->X[8] = (uintptr_t)&cur8::kFive - 0x88; }
};
HOOK_DEFINE_INLINE(Cur8DecidedHook) {                      /* 0x8bbaa4 (x19 = pagina): dopo i 4 slot 6/7 -> item vt+0x28(item, s) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* c = *(uint8_t**)(ctx->X[19] + 0x1b0); if (!cur8::Wide(c)) return;
        uint32_t cnt = *(uint32_t*)(c + 0x10); uint8_t** it = *(uint8_t***)(c + 0x18);
        for (int s = 4; s < 8; s++) {
            uint8_t* sl = cur8::Slot(c, s); uint32_t i = *(uint32_t*)(sl + 8);
            if ((*(int32_t*)sl | 1) != 7 || i >= cnt || !it[i]) continue;
            (*(void (**)(uint8_t*, int))(*(uintptr_t*)it[i] + 0x28))(it[i], s);
        }
    }
};
HOOK_DEFINE_INLINE(Cur8PageFocusHook) {                    /* 0x8bb568: dopo setFocus 0..3 dell'ingresso pagina (x19 = pagina) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* pg = (uint8_t*)ctx->X[19];
        if (!cur8::Wide(*(uint8_t**)(pg + 0x1b0))) return;
        for (int p = 4; p < 8; p++) if ((*(uint32_t*)(pg + 0x190) >> p) & 1) cur8::SetFoc(p, pg);
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8CtorHook) {                     /* 0x8af218 ctor (oggetto 0x1a8 con la patch 0x8bafcc) */
    static void Callback(uint8_t* c) {
        Orig(c);
        for (int k = 0xb8; k < 0x128; k += 4) *(uint32_t*)(c + k) = 0;
        *(uint32_t*)(c + 0xb8) = cur8::kMagic; *(int32_t*)(c + 0xbc) = 4;
        for (int s = 4; s < 8; s++) cur8::InitSlot(cur8::Slot(c, s));
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8ResetHook) {                    /* 0x8af308 reset (da 0x4b5f50, 0x4f6dc4) */
    static void Callback(uint8_t* c) { Orig(c); if (*(uint32_t*)(c + 0xb8) == cur8::kMagic) for (int s = 4; s < 8; s++) cur8::InitSlot(cur8::Slot(c, s)); }
};
HOOK_DEFINE_TRAMPOLINE(Cur8RestoreHook) {                  /* 0x8afc54: slot dalla memoria all'ingresso della pagina (0x8bb454) */
    static void Callback(uint8_t* c) {
        Orig(c);
        if (!cur8::Wide(c)) return;
        uint32_t pid = *(uint32_t*)(*(uint8_t**)(c + 8) + 0xe8); if (pid >= 0x88) pid = 0;
        uint32_t cnt = *(uint32_t*)(c + 0x10);
        for (int s = 4; s < 8; s++) {
            uint8_t* sl = cur8::Slot(c, s); cur8::InitSlot(sl);
            int32_t m = cur8::mem8[pid][s - 4]; *(int32_t*)(sl + 8) = (m >= 0 && (uint32_t)m < cnt) ? m : 0;
        }
    }
};
/* step 7f: pagina Dialog (id 7, sempre in lista sopra le pagine del menu): apertura 0x49be54 e chiusura 0x49c118 aggiornano bit p di +0x190 e
   focus solo per i pad 0..3 (srotolate). Chiusa restava fffffff0: possedeva i pad 4..7 e, quando il focus dei pad 4..7 va riscelto (FocusExtra:
   pagina piu' alta col bit p), li prendeva lei -> dopo il regalo P5-P8 senza giu'/sinistra/destra/A/B. Stesso flusso esteso ai pad 4..7. */
HOOK_DEFINE_INLINE(DialogOpen8Hook) {                    /* 0x49bf0c (x19 = Dialog, x20 = mgr UI): dopo i 4 setFocus dell'apertura */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* dg = (uint8_t*)ctx->X[19]; *(uint32_t*)(dg + 0x190) |= 0xf0u;
        if (!cur8::wideCur) return;
        int n = menu8::LocalCount(); if (n > 8) n = 8;
        for (int p = 4; p < n; p++) mk8dx::Fn<void (*)(uint8_t*, int, uint8_t*)>(0x8bd5f8)((uint8_t*)ctx->X[20], p, dg);
    }
};
HOOK_DEFINE_INLINE(DialogClose8Hook) {                   /* 0x49c1ac (x19 = Dialog, x20 = mgr UI): dopo i 4 setFocus(p, nullptr) della chiusura */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* dg = (uint8_t*)ctx->X[19]; *(uint32_t*)(dg + 0x190) &= ~0xf0u;
        if (!cur8::wideCur) return;
        int n = menu8::LocalCount(); if (n > 8) n = 8;
        for (int p = 4; p < n; p++) mk8dx::Fn<void (*)(uint8_t*, int, uint8_t*)>(0x8bd5f8)((uint8_t*)ctx->X[20], p, nullptr);
    }
};
/* 0x8b3b3c (scaricamento di un gruppo di pagine): toglie le pagine dalla lista, azzera il focus dei pad 0..3 (0x8bd5f8(mgr, p, nullptr)
   srotolato 0x8b3bd4..0x8b3c04) e poi distrugge il gruppo. Stesso flusso per i pad 4..7 sull'ombra foc[]: senza, foc[p] poteva restare su una
   pagina distrutta (letta da SetWide/SetFoc al rientro in selezione nella stessa scena). Sempre, come l'originale per 0..3. */
HOOK_DEFINE_INLINE(PageGroupUnload8Hook) {               /* 0x8b3c0c ldr x0,[x19,#0x18] (x21 = mgr UI): dopo i 4 setFocus(p, nullptr) */
    static void Callback(exl::hook::InlineCtx*) {
        for (int p = 4; p < 8; p++) {
            if (!cur8::foc[p - 4]) continue;
            Logging.Log("[mk8dx-8p] cur8: scaricamento gruppo pagine con focus del pad %d sulla pagina 0x%x: azzerato", p, *(int32_t*)(cur8::foc[p - 4] + 0xe8));
            cur8::SetFoc(p, nullptr);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8ActivateHook) {                 /* 0x8afdd0: attiva gli slot (0x8bb4fc) */
    static void Callback(uint8_t* c) {
        Orig(c);
        if (int s = cur8::CwSlot(c); s >= 4 && *(int32_t*)(c + 0x10) && *(int32_t*)cur8::Slot(c, 0) == 2)    /* passo 7 (b): finestra varianti 4..7 */
            cur8::AsWide(c, s, [&] { uint8_t* sv = cur8::Slot(c, s); *(int32_t*)sv = 1; cur8::cwAct++;
                mk8dx::Fn<bool (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(c, s, *(int32_t*)(sv + 8), 0, 1, 0);
                if (*(int32_t*)sv == 1) *(int32_t*)sv = 2; });
        if (!cur8::Wide(c) || !*(int32_t*)(c + 0x10)) return;
        for (int s = 4; s < 8; s++) {
            uint8_t* sl = cur8::Slot(c, s);
            if (sl[4]) { *(int32_t*)sl = 2; sl[4] = 0; continue; }
            *(int32_t*)sl = 1;
            mk8dx::Fn<bool (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(c, s, *(int32_t*)(sl + 8), 0, 1, 0);
            if (*(int32_t*)sl == 1) *(int32_t*)sl = 2;
        }
    }
};
/* 6 nov 2026 (report/step8_varianti_chiusura.md): conferma del costume 0x8affa4(cursore, p) da 0x4b3f34 = mostra il riquadro e animazione
   "Decide" (lo fa sparire). Per la finestra varianti di P5-P8 (cursore a 4 slot) girava fuori da AsWide: F11 dava V = 0 -> slot 0 giusto ma
   riquadro di P1; il riquadro 04..07 restava visibile. Stesso passo del gioco per 0..3, eseguito da cursore largo come attivazione e disegno. */
HOOK_DEFINE_TRAMPOLINE(CwDecideHook) {                     /* 0x8affa4 f81d0ff5 (cursore, p) */
    static void Callback(uint8_t* c, int32_t p) {
        int s = cur8::CwSlot(c);
        if (s >= 4 && p == s) {
            cur8::AsWide(c, s, [&] { Orig(c, p); });
            static int n; if (n++ < 16) { uint8_t* g = cur8::Gfx(s);
                Logging.Log("[mk8dx-8p] p8v: decide finestra P%d slot0 stato %d gfx%d=%p c0=%d", s + 1, *(int32_t*)cur8::Slot(c, 0), s, g, g ? g[0xc0] : -1); }
            return;
        }
        Orig(c, p);
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8AnyHook) {                      /* 0x8ae2a4(item): selezionato da un pad qualsiasi */
    static bool Callback(uint8_t* item) {
        if (Orig(item)) return true;
        uint8_t* c = *(uint8_t**)(item + 0x60); if (!cur8::Wide(c)) return false;
        int32_t idx = *(int32_t*)(item + 0x68);
        for (int s = 4; s < 8; s++) { uint8_t* sl = cur8::Slot(c, s); if (*(int32_t*)sl >= 3 && *(int32_t*)(sl + 8) == idx) return true; }
        return false;
    }
};
HOOK_DEFINE_REPLACE(Cur8DrawHook) {                        /* 0x8b01ec: 0x8b0238(cur,0..3) srotolato, poi +0xb0 = 0 */
    static void Callback(uint8_t* c) {
        using Draw = void (*)(uint8_t*, int);
        int n = cur8::Wide(c) ? 8 : 4;
        for (int p = 0; p < n; p++) mk8dx::Fn<Draw>(0x8b0238)(c, p);
        if (int s = cur8::CwSlot(c); s >= 4 && cur8::foc[s - 4] == *(uint8_t**)(c + 8))    /* passo 7 (b): finestra varianti 4..7 */
            cur8::AsWide(c, s, [&] { cur8::cwDraw++; mk8dx::Fn<Draw>(0x8b0238)(c, s); });
        *(uint32_t*)(c + 0xb0) = 0;
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8FocusFrameHook) {               /* 0x8bd6d8(mgr) per frame */
    static void Callback(uint8_t* mgr) {
        Orig(mgr);
        if (cur8::wideCur) cur8::FocusExtra(mgr);
        if (MK8DX_SEL8B) sel8b::Frame(mgr);                  /* step 5c H7 (pagine aperte) + misura M2 */
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8SetFocusHook) {                 /* 0x8bd5f8(mgr, p, pagina) */
    static void Callback(uint8_t* mgr, int32_t p, uint8_t* page) {
        if (cur8::wideCur && p >= 4 && p < 8) { cur8::SetFoc(p, page); return; }
        Orig(mgr, p, page);
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8GfxCreateHook) {                /* 0x49a8fc onCreate cm_Page_Cursor */
    static void Callback(uint8_t* cp) {
        Orig(cp);
        cur8::ShowPanes(false);                              /* come il vanilla: 04..07 nascosti */
        Logging.Log("[mk8dx-8p] cur8: cursori grafici 00..03 %p %p %p %p, 04..07 %p %p %p %p", *(void**)(cp + 0x1d0), *(void**)(cp + 0x1d8),
                    *(void**)(cp + 0x1e0), *(void**)(cp + 0x1e8), *(void**)(cp + 0x210), *(void**)(cp + 0x218), *(void**)(cp + 0x220), *(void**)(cp + 0x228));
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8GfxLoopHook) {                  /* 0x49aa1c: frame Loop condiviso (+0x1f0) */
    static void Callback(uint8_t* cp) {
        float f = *(float*)(cp + 0x1f0);
        Orig(cp);
        if (!cur8::wideCur) return;
        bool any = false;
        for (int i = 0; i < 4; i++) {
            uint8_t* g = *(uint8_t**)(cp + 0x210 + 8 * i);
            if (g && mk8dx::Fn<bool (*)(uint8_t*)>(0x442e70)(g) && g[0xc0]) { mk8dx::Fn<void (*)(uint8_t*, float)>(0x442ef0)(g, f); any = true; }
        }
        if (any && *(float*)(cp + 0x1f0) == 0.0f)            /* nessuno dei 00..03 attivo: avanza comunque (0x49aadc) */
            *(float*)(cp + 0x1f0) = mk8dx::Fn<float (*)(uint8_t*, float)>(0x442f10)(*(uint8_t**)(cp + 0x1d0), f);
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8GfxBusyHook) {                  /* 0x49ab04: un cursore grafico occupato */
    static bool Callback(uint8_t* cp) {
        if (Orig(cp)) return true;
        if (!cur8::wideCur) return false;
        for (int i = 0; i < 4; i++) { uint8_t* g = *(uint8_t**)(cp + 0x210 + 8 * i); if (g && mk8dx::Fn<bool (*)(uint8_t*)>(0x8adf84)(g + 0x30)) return true; }
        return false;
    }
};
HOOK_DEFINE_TRAMPOLINE(Cur8GfxEnableHook) {                /* 0x49ab68(cp, b) */
    static void Callback(uint8_t* cp, bool b) {
        Orig(cp, b);
        if (!cur8::wideCur) return;
        for (int i = 0; i < 4; i++) { uint8_t* g = *(uint8_t**)(cp + 0x210 + 8 * i); if (g) mk8dx::Fn<void (*)(uint8_t*, bool)>(0x8ae098)(g + 0x30, b); }
    }
};
#if MK8DX_P3TEST
HOOK_DEFINE_TRAMPOLINE(Cur8TestEventHook) {                /* 0x4bbd18 vt92 selezione (pagina, evento): ev+0x2c = slot/pad */
    static void Callback(uint8_t* page, uint8_t* ev) {
        uint32_t p = *(uint32_t*)(ev + 0x2c);
        if (p >= 4 && p < 8 && cur8::Wide(*(uint8_t**)(page + 0x1b0))) {
            static int n; if (n++ < 24) Logging.Log("[mk8dx-8p] cur8 TEST: evento id %d tipo %d di P%d scartato", *(int32_t*)(ev + 0x8), *(int32_t*)(ev + 0x28), p + 1);
            return;
        }
        Orig(page, ev);
    }
};
#endif

/* ---- step 5b passo 2 (report/step5b_plan_p2.md): pagine CharaList (0x28..0x2b, finestra varianti) per gli slot 4..7 su id
 * liberi 01/02/5e/64, rimappate per indirizzo di ritorno in 0x8bf0ec; link +0x660[4] -> +0xa68[8], code +0x928[4] -> +0xba8[8],
 * oggetto 0xa68 -> 0xce8. Le vecchie aree restano costruite dal ctor (sentinelle). Clamp lasciati a 4 fino al passo 4. */
namespace cw8 {
    static constexpr int kIds[4] = { 0x01, 0x02, 0x5e, 0x64 };       /* liberi prima e dopo una gara (dump 0x8bcc5c) */
    static constexpr uintptr_t kLR[18] = {                           /* bl 0x8bf0ec + 4 nei siti id = 0x28+i */
        0x4b4a40, 0x4b7038, 0x4b711c, 0x4b7180, 0x4b73a4, 0x4b77d8, 0x4b7d7c, 0x4b9f0c, 0x4ba4a0,
        0x4ba640, 0x4ba88c, 0x4babb8, 0x4bae28, 0x4baea4, 0x4bb1f8, 0x4bb974, 0x4bc33c, 0x4bc5ac };
    static bool ready;                                               /* 4 pagine registrate nella scena menu corrente */
    static bool active;                                              /* passo 2 installato (link/code rilocati) */
    static int SlotOfId(int id) { for (int j = 0; j < 4; j++) if (kIds[j] == id) return 4 + j; return -1; }
    static int hits;                                                 /* rimappaggi effettuati (0 finche' +0x8c0 = 4) */
    static const sel8::P kReloc[] = {
        {0x4b7a38, 0x91198260, 0x9129a260},   /* link init vt77    add x0, x19, #0x660 -> #0xa68 */
        {0x4b7a50, 0x911a2260, 0x912a4260},   /* link init vt77    add x0, x19, #0x688 -> #0xa90 */
        {0x4b7a68, 0x911ac260, 0x912ae260},   /* link init vt77    add x0, x19, #0x6b0 -> #0xab8 */
        {0x4b7a80, 0x911b6260, 0x912b8260},   /* link init vt77    add x0, x19, #0x6d8 -> #0xae0 */
        {0x4bc640, 0x91198101, 0x9129a101},   /* link uso vt92     add x1, x8, #0x660  -> #0xa68 */
        {0x4b4f1c, 0x9124a12a, 0x912ea12a},   /* CharaList 0x4b4f08  add x10, x9, #0x928 -> #0xba8 */
        {0x4b4f20, 0x9124e12b, 0x912ee12b},   /*                     add x11, x9, #0x938 -> #0xbb8 */
        {0x4b4f24, 0xb9493929, 0xb94bb929},   /*                     ldr w9, [x9, #0x938] */
        {0x4b4f40, 0xb949318c, 0xb94bb18c},   /*                     ldr w12, [x12, #0x930] */
        {0x4b4f5c, 0xb9493508, 0xb94bb508},   /*                     ldr w8, [x8, #0x934] */
        {0x4b4f90, 0xb949318c, 0xb94bb18c},   /*                     ldr w12, [x12, #0x930] */
        {0x4b4fac, 0xb9493508, 0xb94bb508},   /*                     ldr w8, [x8, #0x934] */
        {0x4b4fdc, 0xb94931ad, 0xb94bb1ad},   /*                     ldr w13, [x13, #0x930] */
        {0x4b4ff8, 0xb9493508, 0xb94bb508},   /*                     ldr w8, [x8, #0x934] */
        {0x4b53d4, 0xb9493908, 0xb94bb908},   /* CharaList vt88      ldr w8, [x8, #0x938] (x8 = [+0x1d8] + slot*0x28) */
        {0x4b8fa8, 0xb9493a68, 0xb94bba68},   /* coda[0] 0x4b8e44    ldr w8, [x19, #0x938] (slot 0 fisso, +0x8c8 = 0x34/0x1d) */
        {0x4b8fb4, 0xb9493269, 0xb94bb269},   /*                     ldr w9, [x19, #0x930] */
        {0x4b8fc0, 0xb949366b, 0xb94bb66b},   /*                     ldr w11, [x19, #0x934] */
        {0x4b8fc8, 0xb9093a6a, 0xb90bba6a},   /*                     str w10, [x19, #0x938] */
        {0x4b8fcc, 0xf944966a, 0xf945d66a},   /*                     ldr x10, [x19, #0x928] */
        {0x4b91f0, 0xb9493909, 0xb94bb909},   /* 0x4b9190 (consumo)  ldr w9, [x8, #0x938] */
        {0x4b9200, 0x9124e116, 0x912ee116},   /*                     add x22, x8, #0x938 (poi [x22] a 0x4b9234/0x4b93f0/0x4b9418) */
        {0x4b9238, 0xf944953c, 0xf945d53c},   /*                     ldr x28, [x9, #0x928] */
        {0x4b9244, 0xb9493509, 0xb94bb509},   /*                     ldr w9, [x8, #0x934] */
        {0x4b9248, 0xb9493108, 0xb94bb108},   /*                     ldr w8, [x8, #0x930] */
        {0x4b9400, 0xb949352a, 0xb94bb52a},   /*                     ldr w10, [x9, #0x934] */
        {0x4b9404, 0xb949312c, 0xb94bb12c},   /*                     ldr w12, [x9, #0x930] */
        {0x4b9414, 0xb909352a, 0xb90bb52a},   /*                     str w10, [x9, #0x934] */
        {0x4ba1bc, 0xb9493948, 0xb94bb948},   /* vt93 0x4b99c8       ldr w8, [x10, #0x938] */
        {0x4ba1dc, 0xb9493129, 0xb94bb129},   /*                     ldr w9, [x9, #0x930] */
        {0x4ba1e8, 0x9124e14a, 0x912ee14a},   /*                     add x10, x10, #0x938 */
        {0x4ba1fc, 0xf944954b, 0xf945d54b},   /*                     ldr x11, [x10, #0x928] */
        {0x4ba200, 0xb949354a, 0xb94bb54a},   /*                     ldr w10, [x10, #0x934] */
        {0x4bb298, 0xb949396b, 0xb94bb96b},   /* vt87 0x4badb0       ldr w11, [x11, #0x938] */
        {0x4bc500, 0xb9493969, 0xb94bb969},   /* vt92 0x4bbd18       ldr w9, [x11, #0x938] */
        {0x4bc514, 0xb949314a, 0xb94bb14a},   /*                     ldr w10, [x10, #0x930] */
        {0x4bc520, 0x9124e16b, 0x912ee16b},   /*                     add x11, x11, #0x938 */
        {0x4bc534, 0xf944950b, 0xf945d50b},   /*                     ldr x11, [x8, #0x928] */
        {0x4bc538, 0xb9493508, 0xb94bb508},   /*                     ldr w8, [x8, #0x934] */
    };                                                       /* 5 link + 34 code = 39 */
    static const sel8::P kFactory[] = {                      /* orig = parola del passo 1 (mov w0,#0xa68), vanilla 0x52814500 */
        {0x4091c4, 0x52814d00, 0x52819d00}, {0x4091f0, 0x52814d00, 0x52819d00}, {0x40921c, 0x52814d00, 0x52819d00},  /* #0xce8 */
    };
}
HOOK_DEFINE_TRAMPOLINE(CharaSelBaseCtorHook) {               /* 0x4bc844 ctor base selezione (Single/WiFi/Multi), oggetto 0xa68 */
    static void Callback(uint8_t* self) {
        Orig(self);
        for (int i = 0; i < 16; i++) *(uint32_t*)(self + 0xa28 + 4 * i) = 0;          /* 8 voci {stato, valore} */
        for (int i = 0; i < 0x20; i += 4) *(uint32_t*)(self + 0x904 + i) = sel8::kCanary;
        static int n; if (n++ < 6) Logging.Log("[mk8dx-8p] M2: ctor selezione %p, coda +0xa28 azzerata, canary +0x904", self);
        if (cw8::active) {                                       /* passo 2: link [8] a +0xa68 e code [8] a +0xba8 */
            uintptr_t vt = *(uintptr_t*)(mk8dx::Base() + 0x1307ec8) + 0x10;           /* come 0x4bc8d4..0x4bc8e4 */
            for (int i = 0; i < 8; i++) {
                uint8_t* l = self + 0xa68 + i * 0x28;
                mk8dx::Fn<void (*)(uint8_t*)>(0x8b18cc)(l);
                *(uintptr_t*)l = vt;
            }
            for (int i = 0; i < 8; i++) {
                uint8_t* q = self + 0xba8 + i * 0x28;
                for (int k = 0; k < 0x28; k += 4) *(uint32_t*)(q + k) = 0;
                *(uint8_t**)q = q + 0x14;                                               /* come 0x4bc924/0x4bc930 */
                *(int32_t*)(q + 0x8) = 2;                                               /* capacita' (0x4bc93c) */
            }
        }
        if (p4::on) *(uint8_t**)(self + 0xce8) = nullptr;                    /* passo 4: impostato a ogni onEnter */
    }
};

HOOK_DEFINE_TRAMPOLINE(PageGetRemapHook) {                           /* 0x8bf0ec(int* id) -> pagina */
    static uint8_t* Callback(int32_t* pid) {
        int v = *pid;
        if (v >= 0x2c && v <= 0x2f && cw8::ready) {
            uintptr_t lr = (uintptr_t)__builtin_return_address(0) - mk8dx::Base();
            for (uintptr_t x : cw8::kLR) if (x == lr) {
                int32_t nid = cw8::kIds[v - 0x2c];                   /* non si tocca *pid: alcuni siti lo riusano */
                uint8_t* p = Orig(&nid);
                if (p && *(int32_t*)(p + 0xe8) == nid) {
                    if (cw8::hits++ < 16) Logging.Log("[mk8dx-8p] cw8: 0x%x -> 0x%x (%p) da main+%lx", v, nid, p, lr);
                    return p;
                }
                break;                                               /* pagina assente: comportamento originale */
            }
        }
        return Orig(pid);
    }
};
HOOK_DEFINE_TRAMPOLINE(MenuPagesSetupHook) {                         /* 0x40476c(scena menu): registra le pagine del menu */
    static void Callback(uint8_t* self) {
        Orig(self);
        cur8::Reset();                                               /* step 5b passo 3: nuove pagine del menu, cursore largo non piu' valido */
        cw8::ready = false;
        p4::reg = false; p4::namesOpen = false;
        p5::built = nullptr; p5::isB = false; p5::fromSel = false;
        if (MK8DX_SEL8B) { sel8b::built = nullptr; sel8b::isB = false; sel8b::selPg = nullptr; sel8b::entered = nullptr;   /* step 5c H8 */
                           sel8b::selOpen = sel8b::kartOpen = false; }
        if (p4::on) {                                                /* passo 4: pagina nomi CharaParts 8pl sull'id 0x12 */
            uint8_t* tab = mk8dx::Fn<uint8_t* (*)()>(0x8bcc5c)();
            uintptr_t fcp = *(uintptr_t*)(mk8dx::Base() + 0x1307ad0);           /* -> 0x409070 (CharaParts) */
            uintptr_t frv = *(uintptr_t*)(mk8dx::Base() + 0x1307d68);           /* fabbrica RaceView di hud8 (voce dopo una gara a 5-8) */
            int32_t id = p4::kNamesId; uintptr_t f = *(uintptr_t*)(tab + 0x8 + id * 8);
            if (fcp != mk8dx::Base() + 0x409070 || *(uintptr_t*)(tab + 0x8 + 0x16 * 8) != fcp || *(uint8_t**)(tab + 0x448 + 0x16 * 8) != self)
                Logging.Log("[mk8dx-8p] p4: 0x40476c non registra CharaParts 0x16, niente pagina nomi");
            else if (f && f != fcp && f != frv) Logging.Log("[mk8dx-8p] p4: id 0x%x occupato (fabbrica %lx)", id, f - mk8dx::Base());
            else {
                *(uintptr_t*)(tab + 0x8 + id * 8) = fcp; *(uint8_t**)(tab + 0x448 + id * 8) = self;
                p4::reg = true;                                          /* prima: l'onCreate della 0x12 gira dentro 0x8b3cb0 (p5::BuildB) */
                p4::regFac = fcp; p4::regOwner = self;
                mk8dx::Fn<void (*)(uint8_t*, int32_t*, int)>(0x8b3cb0)(self, &id, 0xa);   /* layer 0xa come 0x4048a4 */ Logging.Log("[mk8dx-8p] p4: pagina nomi CharaParts 8pl registrata (id 0x%x)", id);
            }
        }
        if (!cw8::active) return;
        uint8_t* tab = mk8dx::Fn<uint8_t* (*)()>(0x8bcc5c)();
        uintptr_t fac = *(uintptr_t*)(mk8dx::Base() + 0x1307b38);   /* GOT -> 0x40918c (CharaList) */
        if (fac != mk8dx::Base() + 0x40918c || *(uintptr_t*)(tab + 0x8 + 0x28 * 8) != fac || *(uint8_t**)(tab + 0x448 + 0x28 * 8) != self) {
            Logging.Log("[mk8dx-8p] cw8: 0x40476c non e' la scena delle CharaList, niente pagine extra"); return; }
        for (int j = 0; j < 4; j++) {                                /* tutto o niente: voci libere o gia' nostre */
            uintptr_t f = *(uintptr_t*)(tab + 0x8 + cw8::kIds[j] * 8);
            if (f && f != fac) { Logging.Log("[mk8dx-8p] cw8: id 0x%x occupato (fabbrica %lx)", cw8::kIds[j], f - mk8dx::Base()); return; }
        }
        for (int j = 0; j < 4; j++) {
            int32_t id = cw8::kIds[j];
            *(uintptr_t*)(tab + 0x8 + id * 8) = fac;
            *(uint8_t**)(tab + 0x448 + id * 8) = self;
            mk8dx::Fn<void (*)(uint8_t*, int32_t*, int)>(0x8b3cb0)(self, &id, 0xa);   /* layer 0xa come 0x404d8c */
        }
        cw8::ready = true;
        Logging.Log("[mk8dx-8p] cw8: CharaList slot 4..7 registrate (id 01 02 5e 64)");
    }
};
HOOK_DEFINE_INLINE(CharaListSlotHook) {                              /* 0x4b28d8 onCreate CharaList: x19 = pagina, w8 = id-0x28 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* pg = (uint8_t*)ctx->X[19];
        int s = cw8::SlotOfId(*(int32_t*)(pg + 0xe8));
        if (s >= 0) { ctx->W[8] = s; Logging.Log("[mk8dx-8p] cw8: CharaList %p id 0x%x -> slot %d", pg, *(int32_t*)(pg + 0xe8), s); }
    }
};
HOOK_DEFINE_INLINE(CharselLinksHook) {                               /* 0x4b7a98 in vt77 0x4b74a4, x19 = pagina selezione */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* page = (uint8_t*)ctx->X[19];
        for (int j = 0; j < 4; j++) {
            int32_t id = cw8::kIds[j];
            mk8dx::Fn<void (*)(uint8_t*, int32_t*, int)>(0x8b21cc)(page + 0xa68 + (4 + j) * 0x28, &id, -1);
        }
        /* sentinelle: vecchie code (conteggio +0x10) e vecchi link (id +0xc) non devono essere usati */
        static bool bad;
        for (int i = 0; i < 4 && !bad; i++) {
            int32_t qc = *(int32_t*)(page + 0x928 + i * 0x28 + 0x10), lid = *(int32_t*)(page + 0x660 + i * 0x28 + 0xc);
            if (qc != 0 || lid != -1) { bad = true; Logging.Log("[mk8dx-8p] cw8: SENTINELLA slot %d: coda vecchia=%d link vecchio=0x%x", i, qc, lid); }
        }
    }
};

namespace p4 {
    struct C { uint32_t off, orig, repl; };
    static const C kClamp[] = {          /* `cmp #4` -> `#8`: 16 kClamp904 (selezione+regali) + 6 passo 2 + 28 cfg8 */
        {0x4b1d20, 0xf10012bf, 0xf10022bf},   /* k904  cmp x21, #0x4          -> #0x8 ; csel x9, x21, xzr, lo */
        {0x4b563c, 0x7100127f, 0x7100227f},   /* k904  cmp w19, #0x4          -> #0x8 ; csel x8, x19, xzr, lo */
        {0x4b80e4, 0xf100115f, 0xf100215f},   /* k904  cmp x10, #0x4          -> #0x8 ; csel x15, x10, xzr, lo */
        {0x4b80f4, 0xf10011df, 0xf10021df},   /* k904  cmp x14, #0x4          -> #0x8 ; csinc x15, xzr, x10, hs */
        {0x4b81bc, 0x7100113f, 0x7100213f},   /* k904  cmp w9, #0x4           -> #0x8 ; csel x12, x9, xzr, lo */
        {0x4b8330, 0xf10012df, 0xf10022df},   /* k904  cmp x22, #0x4          -> #0x8 ; csel x28, x22, xzr, lo */
        {0x4b87d4, 0xf100129f, 0xf100229f},   /* k904  cmp x20, #0x4          -> #0x8 ; csel x8, x20, xzr, lo */
        {0x4b901c, 0xf100129f, 0xf100229f},   /* k904  cmp x20, #0x4          -> #0x8 ; csel x8, x20, xzr, lo */
        {0x4b94dc, 0x710012bf, 0x710022bf},   /* k904  cmp w21, #0x4          -> #0x8 ; csel x8, x21, xzr, lo */
        {0x4b9b78, 0xf100129f, 0xf100229f},   /* k904  cmp x20, #0x4          -> #0x8 ; csel x21, x20, xzr, lo */
        {0x4ba404, 0xf100129f, 0xf100229f},   /* k904  cmp x20, #0x4          -> #0x8 ; csel x23, x20, xzr, lo */
        {0x4ba5a4, 0xf100129f, 0xf100229f},   /* k904  cmp x20, #0x4          -> #0x8 ; csel x23, x20, xzr, lo */
        {0x4ba860, 0xf10012df, 0xf10022df},   /* k904  cmp x22, #0x4          -> #0x8 ; csel x8, x22, xzr, lo */
        {0x4bb108, 0xf10012bf, 0xf10022bf},   /* k904  cmp x21, #0x4          -> #0x8 ; csel x8, x21, xzr, lo */
        {0x4bb470, 0xf10012bf, 0xf10022bf},   /* k904  cmp x21, #0x4          -> #0x8 ; csel x8, x21, xzr, lo */
        {0x4bc018, 0x7100111f, 0x7100211f},   /* k904  cmp w8, #0x4           -> #0x8 ; csel x8, x8, xzr, lo */
        {0x4b4f0c, 0x7100103f, 0x7100203f},   /* p2    cmp w1, #0x4           -> #0x8 ; csel x8, x8, xzr, lo */
        {0x4b53c8, 0x7100113f, 0x7100213f},   /* p2    cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b91e4, 0xf100129f, 0xf100229f},   /* p2    cmp x20, #0x4          -> #0x8 ; csel x27, x20, xzr, lo */
        {0x4bb28c, 0xf100113f, 0xf100213f},   /* p2    cmp x9, #0x4           -> #0x8 ; csel x11, x9, xzr, lo */
        {0x4bc4f0, 0x7100111f, 0x7100211f},   /* p2    cmp w8, #0x4           -> #0x8 ; csel x8, x8, xzr, lo */
        {0x4bc630, 0x7100111f, 0x7100211f},   /* p2    cmp w8, #0x4           -> #0x8 ; csel x8, x8, xzr, lo */
        {0x4204c4, 0x7100129f, 0x7100229f},   /* cfg   cmp w20, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x420564, 0x7100129f, 0x7100229f},   /* cfg   cmp w20, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x420610, 0x7100129f, 0x7100229f},   /* cfg   cmp w20, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4206c0, 0x7100129f, 0x7100229f},   /* cfg   cmp w20, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x42077c, 0x7100129f, 0x7100229f},   /* cfg   cmp w20, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x420820, 0x7100129f, 0x7100229f},   /* cfg   cmp w20, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4208c4, 0x7100129f, 0x7100229f},   /* cfg   cmp w20, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4209f0, 0x710012bf, 0x710022bf},   /* cfg   cmp w21, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b394c, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b3a30, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b3b14, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b3bf8, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b3cdc, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b3dc0, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b3ea4, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b3ffc, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b4480, 0x7100127f, 0x7100227f},   /* cfg   cmp w19, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b45ac, 0x7100127f, 0x7100227f},   /* cfg   cmp w19, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b46d4, 0x7100127f, 0x7100227f},   /* cfg   cmp w19, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b47fc, 0x7100127f, 0x7100227f},   /* cfg   cmp w19, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b4c60, 0x7100127f, 0x7100227f},   /* cfg   cmp w19, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b4d90, 0x7100127f, 0x7100227f},   /* cfg   cmp w19, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b4e24, 0x7100127f, 0x7100227f},   /* cfg   cmp w19, #0x4          -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4bbf18, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4bbfac, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4bc228, 0x7100113f, 0x7100213f},   /* cfg   cmp w9, #0x4           -> #0x8 ; csel x9, x9, xzr, lo */
        {0x4b75ec, 0xf100131f, 0xf100231f},   /* cfg   cmp x24, #0x4          -> #0x8 ; csel x27, x24, xzr, lo */
        {0x4b7b44, 0xf100131f, 0xf100231f},   /* cfg   cmp x24, #0x4          -> #0x8 ; csel x9, x24, xzr, lo */
    };
    struct A { uint32_t off, word; uint8_t base, idx; };
    static const A kCfgAdd[] = {        /* hook INLINE PRE: se 4 <= X[idx] < 8 -> X[base] = cfg8::Base() (ombra) */
        {0x4204d4, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w8, [x8, #0x2034] */
        {0x420574, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w8, [x8, #0x2054] */
        {0x420620, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w8, [x8, #0x2074] */
        {0x4206d0, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w8, [x8, #0x2044] */
        {0x42078c, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w2, [x8, #0x2014] */
        {0x420830, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w2, [x8, #0x2024] */
        {0x4208d4, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w2, [x8, #0x2064] */
        {0x420a00, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w23, [x8, #0x2004] */
        {0x4b395c, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w21, [x8, #0x2014] */
        {0x4b3a38, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w21, [x8, #0x2024] */
        {0x4b3b1c, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w21, [x8, #0x2034] */
        {0x4b3c00, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w21, [x8, #0x2044] */
        {0x4b3ce4, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w21, [x8, #0x2054] */
        {0x4b3dc8, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w21, [x8, #0x2064] */
        {0x4b3eac, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w21, [x8, #0x2074] */
        {0x4b4004, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w21, [x8, #0x2004] */
        {0x4b4490, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w22, [x8, #0x2034] */
        {0x4b45bc, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w22, [x8, #0x2054] */
        {0x4b46e4, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w22, [x8, #0x2074] */
        {0x4b480c, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w22, [x8, #0x2044] */
        {0x4b4c70, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w22, [x8, #0x2014] */
        {0x4b4da0, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w22, [x8, #0x2064] */
        {0x4b4e34, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w22, [x8, #0x2024] */
        {0x4bbf28, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w10, [x8, #0x1fb4] */
        {0x4bbfbc, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w10, [x8, #0x1fc4] */
        {0x4bc234, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> str w10, [x8, #0x1fc4] */
        {0x4b75f4, 0x8b1b0908,  8, 27},   /* add x8, x8, x27, lsl #2    -> ldr w8, [x8, #0x1fb4] */
        {0x4b76f0, 0x8b1b0908,  8, 27},   /* add x8, x8, x27, lsl #2    -> ldr w8, [x8, #0x1fb4] */
        {0x4b776c, 0x8b1b0908,  8, 27},   /* add x8, x8, x27, lsl #2    -> str wzr, [x8, #0x1fb4] */
        {0x4b7b50, 0x8b090908,  8,  9},   /* add x8, x8, x9, lsl #2     -> ldr w8, [x8, #0x1fb4] */
        {0x4b83c4, 0x8b1c0908,  8, 28},   /* add x8, x8, x28, lsl #2    -> ldr w8, [x8, #0x1fb4] */
        {0x4b8470, 0x8b1c0908,  8, 28},   /* add x8, x8, x28, lsl #2    -> ldr w8, [x8, #0x1fd4] */
        {0x4b84d8, 0x8b1c0908,  8, 28},   /* add x8, x8, x28, lsl #2    -> ldr w8, [x8, #0x1fe4] */
        {0x4b853c, 0x8b1c0908,  8, 28},   /* add x8, x8, x28, lsl #2    -> ldr w8, [x8, #0x1ff4] */
        {0x4b9bf8, 0x8b150908,  8, 21},   /* add x8, x8, x21, lsl #2    -> ldr w8, [x8, #0x1fb4] */
        {0x4b9ca0, 0x8b150908,  8, 21},   /* add x8, x8, x21, lsl #2    -> ldr w8, [x8, #0x1fc4] */
        {0x4b9db4, 0x8b150908,  8, 21},   /* add x8, x8, x21, lsl #2    -> str w9, [x8, #0x1fb4] */
        {0x4b9eb0, 0x8b150929,  9, 21},   /* add x9, x9, x21, lsl #2    -> str w8, [x9, #0x1fc4] */
    };
    static const C kNames[] = {          /* L_Name (+0x1e8/+0x1f0) della pagina CharaParts: +0x728 -> campo nuovo +0xce8 */
        {0x4b3404, 0xf9439529, 0xf9467529},   /* ldr x9, [x9, #0x728]     ; ldr w10, [x9, #0x1e8] */
        {0x4b4300, 0xf94396c8, 0xf94676c8},   /* ldr x8, [x22, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b437c, 0xf9439688, 0xf9467688},   /* ldr x8, [x20, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b48a8, 0xf9439688, 0xf9467688},   /* ldr x8, [x20, #0x728]    ; ldr x8, [x8, #0x1f0] */
        {0x4b4978, 0xf9439688, 0xf9467688},   /* ldr x8, [x20, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b49a8, 0xf9439688, 0xf9467688},   /* ldr x8, [x20, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b49d8, 0xf9439688, 0xf9467688},   /* ldr x8, [x20, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b4a0c, 0xf9439688, 0xf9467688},   /* ldr x8, [x20, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b4b4c, 0xf9439688, 0xf9467688},   /* ldr x8, [x20, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b4c98, 0xf9439688, 0xf9467688},   /* ldr x8, [x20, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b50ac, 0xf94396c8, 0xf94676c8},   /* ldr x8, [x22, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b54ec, 0xf94396a8, 0xf94676a8},   /* ldr x8, [x21, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b7cd4, 0xf9439668, 0xf9467668},   /* ldr x8, [x19, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b87f8, 0xf9439668, 0xf9467668},   /* ldr x8, [x19, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b8814, 0xf9439668, 0xf9467668},   /* ldr x8, [x19, #0x728]    ; ldr x8, [x8, #0x1f0] */
        {0x4b882c, 0xf9439668, 0xf9467668},   /* ldr x8, [x19, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b88b8, 0xf9439668, 0xf9467668},   /* ldr x8, [x19, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4b9ed8, 0xf9439668, 0xf9467668},   /* ldr x8, [x19, #0x728]    ; ldr w9, [x8, #0x1e8] */
        {0x4bc300, 0xf9439689, 0xf9467689},   /* ldr x9, [x20, #0x728]    ; ldr w10, [x9, #0x1e8] */
    };
    static const C kFactory[] = {        /* orig = parola del passo 2 (#0xce8), vanilla 0x52814500 (#0xa28) */
        {0x4091c4, 0x52819d00, 0x52819e00},   /* mov w0,#0xce8 -> #0xcf0 */
        {0x4091f0, 0x52819d00, 0x52819e00},   /* mov w0,#0xce8 -> #0xcf0 */
        {0x40921c, 0x52819d00, 0x52819e00},   /* mov w0,#0xce8 -> #0xcf0 */
    };
    struct H { uint32_t off, word; };
    static const H kHookWords[] = {
        {0x4b1d40, 0xb9803129},   /* ldrsw x9, [x9, #0x30]        INLINE kPageSlotFix regali     (x9 = cur+i*0x20, cur = x8) */
        {0x4b885c, 0xb9803108},   /* ldrsw x8, [x8, #0x30]        INLINE kPageSlotFix onEnter    (x8 = cur+i*0x20, cur = x9) */
        {0x4ba460, 0xb9403102},   /* ldr w2, [x8, #0x30]          INLINE kPageSlotFix vt93 A     (x8 = cur+i*0x20, cur = x0) */
        {0x4ba600, 0xb9403102},   /* ldr w2, [x8, #0x30]          INLINE kPageSlotFix vt93 B     (x8 = cur+i*0x20, cur = x0) */
        {0x469760, 0xb90013e8},   /* str w8, [sp, #0x10]          INLINE 0x4694fc: w8 = id CharaParts da +0x8c0 (w20); >4 -> pagina nomi */
        {0x4be8fc, 0xb9004fe8},   /* str w8, [sp, #0x4c]          INLINE onCreate CharaParts: id pagina nomi -> 0x16 (ramo Multi) */
        {0x4bb6a0, 0xd10343ff},   /* sub sp, sp, #0xd0            TRAMPOLINE vt84 uscita selezione (0x8bb368 -> vt+0x290) */
        {0x13d424, 0xf9406409},   /* ldr x9, [x0, #0xc8]          TRAMPOLINE modello 3D per slot (non controlla il limite) */
        {0x43ce4c, 0xa9be4ff4},   /* stp x20, x19, [sp, #-0x20]!  TRAMPOLINE colore/maschera L_ChangeBtn (CharaList onCreate 0x4b29a4) */
        {0x4b74a4, 0xd10783ff},   /* sub sp, sp, #0x1e0           CharselEnterHook esistente (vt77) */
        {0x4be854, 0xd10043ff},   /* sub sp, sp, #0x10            CharaPartsLayoutNameHook esistente */
        {0x4beb04, 0x91076260},   /* add x0, x19, #0x1d8          CharaPartsSlotCountHook esistente */
        {0x4be8d0, 0xd10303ff},   /* sub sp, sp, #0xc0            CharaPartsCreateHook esistente */
        {0x40476c, 0xf81d0ff5},   /* str x21, [sp, #-0x30]!       MenuPagesSetupHook esistente (passo 2) */
    };
}
/* ---- step 5b passo 4 (report/step5b_plan_p4.md): selezione Multi a turno unico, kart ancora a due turni ---- */
#ifndef MK8DX_SEL1ROUND
#define MK8DX_SEL1ROUND 1        /* 0 = selezione a due turni S5.4 (comportamento precedente) */
#endif
namespace cfg8 {                 /* ombra dei 13 int[4] cfg+0x1fb4..+0x2083 per gli indici 4..7 (solo RAM) */
    alignas(16) static int32_t sh[13 * 4];
    static uint32_t hits;
    inline uintptr_t Base() { return (uintptr_t)sh - 0x1fb4 - 16; }   /* add xB,xB,xI,lsl #2 con xI = 4..7 -> &sh[k*4 + xI-4] + imm */
}
/* ---- step 5b passo 6 (report/step5b_plan_p6.md): cfg8::sh e aid8::code persistenti su SD ------------------- */
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
namespace p4 {
    static uint8_t* Names() {                      /* pagina CharaParts 8pl valida, altrimenti nullptr */
        if (!reg) return nullptr;
        int32_t id = kNamesId; uint8_t* p = mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&id);
        uintptr_t vp = *(uintptr_t*)(mk8dx::Base() + 0x13093a0) + 0x10;            /* vptr CharaParts (ctor 0x4be8a8..0x4be8bc) */
        if (!p || *(int32_t*)(p + 0xe8) != kNamesId || *(uintptr_t*)p != vp || *(int32_t*)(p + 0x1e8) != 8 || !*(uint8_t**)(p + 0x1d0)) return nullptr;
        return p;
    }
    static void SetPane(uint8_t* page, const char* name, bool v) {                  /* come 0x4697dc..0x46980c: root = parte+0x30 */
        uint8_t* part = page ? *(uint8_t**)(page + 0x1d0) : nullptr; if (!part) return;
        hud8::SafeString ss = hud8::Str(name);
        uint8_t* pane = mk8dx::Fn<uint8_t* (*)(uint8_t*, hud8::SafeString*)>(0x8ad8e8)(part + 0x30, &ss);
        if (pane) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8ade74)(pane, v);
    }
    static void MultiNames(uint8_t* sel, bool v) {                                   /* N_Player_00..03 della 0x16 (+0x728) */
        uint8_t* m = sel ? *(uint8_t**)(sel + 0x728) : nullptr; char nm[] = "N_Player_00";
        for (int i = 0; i < 4; i++) { nm[10] = '0' + i; SetPane(m, nm, v); }
    }
    static void OpenNames(uint8_t* sel) {
        uint8_t* nmp = Names(); if (!nmp) return;
        namesPage = nmp;
        int32_t id = kNamesId; mk8dx::Fn<void (*)(int32_t*, int)>(0x8b21dc)(&id, 0);  /* come 0x4b7d3c */
        SetPane(nmp, "N_Chara_00", false); SetPane(nmp, "N_ConSuppo_00", false);    /* griglia e B_MP restano quelli della 0x16 */
        MultiNames(sel, false); namesOpen = true;
        Logging.Log("[mk8dx-8p] p4: pagina nomi 8pl %p aperta, nomi Multi nascosti", nmp);
    }
    static void CloseNames(uint8_t* sel) {
        if (namesOpen) { int32_t id = kNamesId; mk8dx::Fn<void (*)(int32_t*)>(0x8b258c)(&id); namesOpen = false; }
        MultiNames(sel, true);
    }
}
#define CFG8_ADD(N, B, I) HOOK_DEFINE_INLINE(Cfg8Add##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    if (ctx->X[I] - 4 < 4) { ctx->X[B] = cfg8::Base(); cfg8::hits++; } } };
CFG8_ADD(B8I9, 8, 9) CFG8_ADD(B8I27, 8, 27) CFG8_ADD(B8I28, 8, 28) CFG8_ADD(B8I21, 8, 21) CFG8_ADD(B9I21, 9, 21)
#define P4_SLOTFIX(N, T, C) HOOK_DEFINE_INLINE(P4SlotFix##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    if (ctx->X[T] - ctx->X[C] >= 0x80 && cur8::Wide((const uint8_t*)ctx->X[C])) ctx->X[T] += 0x80; } };
P4_SLOTFIX(Gift, 9, 8)   /* 0x4b1d40 */   P4_SLOTFIX(Enter, 8, 9)   /* 0x4b885c */   P4_SLOTFIX(Vt93, 8, 0)   /* 0x4ba460, 0x4ba600 */
#define MK8DX_P7_PTN8 3.0f   /* fotogramma G_Ptn con 7-8 giocatori: 3 = N_SC 0.3, 2 = 0.4 */
namespace cw8 { static uint8_t ptnSet[8]; }  /* passo 7 (a) */                 /* finestra dello slot s con Ptn forzato */
HOOK_DEFINE_INLINE(CharaWinPosHook) {              /* 0x469760 str w8,[sp,#0x10] in 0x4694fc */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint32_t c = ctx->W[20], s = ctx->W[22]; uint8_t* win = (uint8_t*)ctx->X[19];
        int32_t na = *(int32_t*)(win + 0x68); uint8_t** an = *(uint8_t***)(win + 0x70);
        if (c > 4 && c <= 8 && p4::Names()) {
            ctx->W[8] = p4::kNamesId;
            if (an && na > 2) {                            /* anim[2] = Ptn (come 0x4694cc) */
                mk8dx::Fn<void (*)(uint8_t*, int, float)>(0x8ad3c4)(an[2], 0, c > 6 ? MK8DX_P7_PTN8 : 2.0f);
                if (s < 8) cw8::ptnSet[s] = 1;
            }
        } else if (s < 8 && cw8::ptnSet[s]) {             /* sessione 1-4P dopo una a 8: Ptn come il vanilla */
            cw8::ptnSet[s] = 0;
            mk8dx::Fn<void (*)(uint8_t*, bool)>(0x4694cc)(win, mk8dx::Fn<bool (*)()>(0x3fe658)());
        }
    }
};
HOOK_DEFINE_INLINE(NamesCreateIdHook) {            /* 0x4be8fc str w8,[sp,#0x4c] onCreate CharaParts: id 0x12 -> ramo Multi */
    static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->W[8] == (uint32_t)p4::kNamesId) ctx->W[8] = 0x16; }
};
HOOK_DEFINE_TRAMPOLINE(CharselExitHook) {          /* 0x4bb6a0 vt84 (0x8bb368 -> vt+0x290): uscita della selezione */
    static void Callback(uint8_t* page) {
        uint8_t* l = *(uint8_t**)(page + 0x168); int32_t dest = l ? *(int32_t*)(l + 0xc) : -1;   /* come 0x8ba5a0 */
        Orig(page);
        if (MK8DX_SEL8B && *(int32_t*)(page + 0xe8) == 0x2e && sel8b::isB && sel8b::built == page) {   /* step 5c H4: set B */
            if ((dest >= 0x28 && dest <= 0x2b) || cw8::SlotOfId(dest) >= 0) return;
            if (dest >= 0x2f && dest <= 0x31) p5::fromSel = true;
            cur8::SetWide(page, false, 0);               /* la 0x12 l'ha chiusa 0x4bb718 (o resta aperta verso la kart, 0x4bb6dc) */
            Logging.Log("[mk8dx-8p] sel8b: uscita selezione (set B) verso 0x%x, 0x12 %s dal gioco", dest,
                        (dest >= 0x2f && dest <= 0x31) ? "lasciata aperta" : "chiusa");
            return;
        }
        if (*(int32_t*)(page + 0xe8) != 0x2e || !p4::namesOpen) return;
        if ((dest >= 0x28 && dest <= 0x2b) || cw8::SlotOfId(dest) >= 0) return;            /* finestra varianti: non si esce */
        if (dest >= 0x2f && dest <= 0x31) p5::fromSel = true;
        if (dest == 0x31 && p4::one && p5::built) { cur8::SetWide(page, false, 0);
            Logging.Log("[mk8dx-8p] p5: verso la kart B, pagina nomi aperta"); return; }
        p4::CloseNames(page);
        cur8::SetWide(page, false, 0);
        Logging.Log("[mk8dx-8p] p4: uscita selezione verso 0x%x, pagina nomi chiusa, nomi Multi visibili", dest);
    }
};
HOOK_DEFINE_TRAMPOLINE(Model3DSlotHook) {          /* 0x13d424(mgr, slot, flag): +0xc8[slot] usato prima del controllo (0x13d428) */
    static void Callback(uint8_t* m, int32_t slot, bool f) {
        if (m && (uint32_t)slot >= 4 && slot >= *(int32_t*)(m + 0xc0)) {
            static int n; if (n++ < 8) Logging.Log("[mk8dx-8p] p4: 0x13d424 slot %d saltato (modelli %d)", slot, *(int32_t*)(m + 0xc0));
            /* niente 0x148108(nullptr) come il ramo fuori limite 0x13d46c: 0x148108 legge [x0+0xc0] senza controllo (0x14813c)
               -> lettura nulla @0xc0 e indice 0 = piedistallo di P1 (catturato con GDB, obiettivo 1 del 3 nov) */
            return;
        }
        Orig(m, slot, f);
    }
};
HOOK_DEFINE_TRAMPOLINE(Model3DReadyHook) {         /* 0x14899c(model): modello pronto ([0x13d540(...,[model+0xc0])]+0x358 == 3), legge
                                                      [x0+0xc0] senza controllo; chiamanti 0x4b2ffc/0x4b5484 con 0x13d47c(mgr,4..7) = NULL */
    static bool Callback(void* model) {
        if (!model) return false;                     /* nessun KartUnit per lo slot: i chiamanti saltano l'azione 3D (prima: stato di P1) */
        return Orig(model);
    }
};
namespace p8d { static int n; static bool Ok(uint8_t* ev) { uint32_t p = ev ? *(uint32_t*)(ev + 0x2c) : 0; return p >= 4 && p < 8 && n < 200; } }
HOOK_DEFINE_INLINE(P8dEntryHook) {   /* 0x4bbe78 b9402a68 (x19 = evento, w21 = entry, w22 = icona) */
    static void Callback(exl::hook::InlineCtx* c) { uint8_t* ev = (uint8_t*)c->X[19]; if (!p8d::Ok(ev)) return; p8d::n++;
        Logging.Log("[mk8dx-8p] P8d: A slot=%u tipo=%d entry=%d icona=%d", *(uint32_t*)(ev + 0x2c), *(int32_t*)(ev + 0x28), (int)c->W[21], (int)c->W[22]); }
};
HOOK_DEFINE_INLINE(P8dWriteHook) {   /* 0x4bbf2c b91fb50a str w10, [x8, #0x1fb4] */
    static void Callback(exl::hook::InlineCtx* c) { uint8_t* ev = (uint8_t*)c->X[19]; if (!p8d::Ok(ev)) return; p8d::n++;
        Logging.Log("[mk8dx-8p] P8d: B slot=%u scrive %d a %lx (ombra cfg8 %p)", *(uint32_t*)(ev + 0x2c), (int)c->W[10], c->X[8] + 0x1fb4, cfg8::sh); }
};
HOOK_DEFINE_INLINE(P8dNameHook) {    /* 0x4bc304 b9802e68 (x9 = pagina nomi) */
    static void Callback(exl::hook::InlineCtx* c) { uint8_t* ev = (uint8_t*)c->X[19]; if (!p8d::Ok(ev)) return; p8d::n++;
        uint8_t* nm = (uint8_t*)c->X[9];
        Logging.Log("[mk8dx-8p] P8d: C slot=%u pagina nomi %p n=%d", *(uint32_t*)(ev + 0x2c), nm, nm ? *(int32_t*)(nm + 0x1e8) : -1); }
};
HOOK_DEFINE_INLINE(P8dPartHook) {    /* 0x4bc320 910e3e1 add x1, sp, #0x38 (x0 = L_Name[slot]) */
    static void Callback(exl::hook::InlineCtx* c) { uint8_t* ev = (uint8_t*)c->X[19]; if (!p8d::Ok(ev)) return; p8d::n++;
        Logging.Log("[mk8dx-8p] P8d: D slot=%u L_Name=%lx", *(uint32_t*)(ev + 0x2c), c->X[0]); }
};
/* Guardie "materiale non trovato" (4 nov): 0x122cd4 cerca m_Body / emission_intensity nel modello del KartUnit con 0x68ff80; con un modello
   di ripiego (moneta: risorsa di default dello slot non ancora caricato) l'indice e' 0xFFFF e 0x68fffc chiama [model+0x38][-1]->vt+0xc8 ->
   salto a 0 nel thread di caricamento (0x618610 -> 0x8830d8 -> 0x16f100 ... 0x122e84 -> 0x690034): da li' nessun caricamento si completa
   piu' (anteprime ferme). Il risultato NULL finisce in obj+0x50 e 0x11dc0c lo legge subito a [x0+0x10] (= le Read64 @0x10 della gara a 8). */
namespace m3d8 { static void DumpUnits(const char* tag); static uint8_t* CurHeap(); static size_t MaxAlloc(uint8_t* h); static uint8_t* Dir(); static uint8_t* Unit(uint8_t* d, int i); }   /* definite piu' avanti */
#ifndef MK8DX_P7_RING9
#define MK8DX_P7_RING9 1   /* step 7 B S4: anello Body/Wing del menu da 5 a 9 voci (richiede REC16) */
#endif
#ifndef MK8DX_P7_REC16
#define MK8DX_P7_REC16 MK8DX_P7_MENU3D8   /* step 7 B S1-S3 (report/step7b_records_plan.md): liste di modelli del caricatore da 12 a 16 record */
#endif
namespace rec16 {   /* record 12..15 di ogni lista in coda a sys (0x9a50 -> 0xb4f0); i record 0..11 restano dove sono */
    enum { K1, K2, K2B, K3, KARM };
    struct LD { const char* name; uint32_t base, S, ext, cur, heap, loaded, ref, used, name_, nameBuf, ring, ringN, parts, nParts; int key;
                uintptr_t acq, pre; int w3; bool idx; uint32_t size; bool dbse; };
    static const LD kL[8] = {   /* piano §1.1-1.2 e §3 (tabella LD) */
        {"Driver",  0x5060, 0xc0, 0x9a50, 0x990, 0x10, 0x34, 0x30, 0x35, 0x38, 0x4c, 0x90, 1, 0x18, 3, K2,   0x3dd1d4, 0x3dd1d4, 0, true,  0x500000, false},
        {"Emblem",  0x59f8, 0xb0, 0x9d50, 0x8c0, 0x10, 0x24, 0x20, 0x25, 0x28, 0x3c, 0x80, 1, 0x18, 1, K2,   0x89333c, 0x3dd350, 1, false, 0x2147a,  false},
        {"Tire",    0x62c0, 0xb0, 0xa010, 0x8c0, 0x10, 0x24, 0x20, 0x25, 0x28, 0x3c, 0x80, 1, 0x18, 1, K2,   0x89353c, 0x3dd60c, 1, false, 0x100000, false},
        {"TireSub", 0x6b88, 0x88, 0xa2d0, 0x660, 0x18, 0x2c, 0x28, 0x2d, 0x30, 0x44, 0,    0, 0x20, 1, K2B,  0x893218, 0,        0, false, 0x1e6666, true },
        {"L71f0",   0x71f0, 0x80, 0xa4f0, 0x600, 0x10, 0x24, 0x20, 0x25, 0x28, 0x3c, 0,    0, 0x18, 1, K1,   0x893448, 0,        0, false, 0x166666, true },
        {"Body",    0x77f8, 0xc8, 0xa6f0, 0xc58, 0x18, 0x3c, 0x38, 0x3d, 0x40, 0x54, 0x98, 5, 0x20, 3, K3,   0x3dd478, 0x3dd478, 0, true,  0x533333, false},
        {"Wing",    0x8458, 0xc8, 0xaa10, 0xc58, 0x18, 0x3c, 0x38, 0x3d, 0x40, 0x54, 0x98, 5, 0x20, 3, K3,   0x3dd734, 0x3dd734, 0, true,  0x1b3333, false},
        {"Arm",     0x90b8, 0xc0, 0xad30, 0x990, 0x18, 0x34, 0x30, 0x35, 0x38, 0x4c, 0x90, 1, 0x20, 2, KARM, 0x3dd8c8, 0x3dd8c8, 0, true,  0x119999, false} };
    constexpr uint32_t kOldSize = 0x9a50, kNewSize = 0xb4f0;
    constexpr uint32_t kRingExt[2] = {0xb030, 0xb290};   /* S4: voci 5..8 dell'anello Body / Wing (4 x 0x98) */
    static bool sizeOk, ready;
    static uint8_t* sys;
    static int N() { return ready ? 16 : 12; }
    static bool Active() { return ready; }
    static uint8_t* RecS(uint8_t* s, int k, int i) { const LD& L = kL[k]; return i < 12 ? s + L.base + i * L.S : s + L.ext + (i - 12) * L.S; }
    static uint8_t* Rec(uint8_t* list, int k, int i) { return RecS(list - kL[k].base, k, i); }
    static bool Match(int k, const uint8_t* r, const uint8_t* key) {
        const LD& L = kL[k]; const int32_t* q = (const int32_t*)key;
        if (q[0] != *(const int32_t*)(r + 8)) return false;
        if (L.key == K1) return true;
        if (q[1] != *(const int32_t*)(r + 0xc)) return false;
        if (L.key == K2) return true;
        if (L.key == K2B) return key[8] == r[0x10];
        uint16_t a = *(const uint16_t*)(key + 0xc);
        if (L.key == KARM && a < 0x100) return true;
        if (q[2] != *(const int32_t*)(r + 0x10)) return false;
        return L.key == K3 || (a & 0xff) == r[0x14];
    }
    static bool Heap(int k, uint8_t* r) { return *(void**)(r + kL[k].heap) != nullptr; }
    static bool Loaded(int k, uint8_t* r) { return r[kL[k].loaded] != 0; }
    static int32_t& Ref(int k, uint8_t* r) { return *(int32_t*)(r + kL[k].ref); }
    using AcqI = uint32_t (*)(void*, void*, void*, int32_t);
    using AcqP = void* (*)(void*, void*, void*, int32_t);
    /* ricerca a N = 16 con la stessa sequenza dell'originale (0x88f5b4 e analoghe): 1. stessa chiave fra i caricati; 2. primo con heap e non
       caricato; 3. giro dal cursore sui record con refcount <= 0, cursore = (c+1) % N. Indice (liste "idx") o puntatore restituito da acquire. */
    static intptr_t Search(int k, uint8_t* list, const void* key, void* x2, bool* failed) {
        const LD& L = kL[k];
        uint8_t kc[16]; memcpy(kc, key, 16);   /* copia della chiave come l'originale (8..14 byte usati) */
        auto acq = [&](uint8_t* r) -> intptr_t { return L.idx ? (intptr_t)(mk8dx::Fn<AcqI>(L.acq)(r, kc, x2, L.w3) & 1) : (intptr_t)mk8dx::Fn<AcqP>(L.acq)(r, kc, x2, L.w3); };
        *failed = false;
        for (int i = 0; i < 16; i++) { uint8_t* r = Rec(list, k, i); if (Heap(k, r) && Loaded(k, r) && Match(k, r, kc)) { intptr_t a = acq(r); return L.idx ? i : a; } }
        for (int i = 0; i < 16; i++) { uint8_t* r = Rec(list, k, i); if (Heap(k, r) && !Loaded(k, r)) { intptr_t a = acq(r);
            if (L.idx) { if (a) return i; *failed = true; return -1; } *failed = !a; return a; } }
        int32_t* cur = (int32_t*)(list + L.cur); int c = *cur;
        for (int t = 0; t < 16; t++) { int j = (c + t) % 16; if (j < 0) j += 16; uint8_t* r = Rec(list, k, j);
            if (Heap(k, r) && Ref(k, r) <= 0) { *cur = (c + 1) % 16; intptr_t a = acq(r);
                if (L.idx) { if (a) return j; *failed = true; return -1; } *failed = !a; return a; } }
        *failed = true; return L.idx ? -1 : 0;
    }
    /* precarico (0x3dc340 -> 0x3dc4f0 e analoghe): presente -> niente; primo libero; primo con refcount <= 0 e non usato; acquire con w3 = 1 */
    static void Pre(int k, uint8_t* list, const void* key, void* x2) {
        const LD& L = kL[k];
        uint8_t kc[16]; memcpy(kc, key, 16);
        for (int i = 0; i < 16; i++) { uint8_t* r = Rec(list, k, i); if (Heap(k, r) && Loaded(k, r) && Match(k, r, kc)) return; }
        for (int i = 0; i < 16; i++) { uint8_t* r = Rec(list, k, i); if (Heap(k, r) && !Loaded(k, r)) { mk8dx::Fn<AcqP>(L.pre)(r, kc, x2, 1); return; } }
        for (int i = 0; i < 16; i++) { uint8_t* r = Rec(list, k, i); if (Heap(k, r) && Ref(k, r) <= 0 && !r[L.used]) { mk8dx::Fn<AcqP>(L.pre)(r, kc, x2, 1); return; } }
    }
    /* S1: dopo il ctor base 0x880c8c i record 12..15 sono copie del record 11 appena costruito (heap 0), con il puntatore del nome su se' stessi */
    static void CtorExt(uint8_t* s) {
        if (!sizeOk) return;
        for (int k = 0; k < 8; k++) { const LD& L = kL[k];
            for (int e = 0; e < 4; e++) { uint8_t* r = s + L.ext + e * L.S; memcpy(r, s + L.base + 11 * L.S, L.S); *(uint8_t**)(r + L.name_ + 8) = r + L.nameBuf; } }
        for (int l = 0; l < 2; l++) { uint32_t lb = l ? 0x8458 : 0x77f8;   /* S4: voci 5..8 dell'anello come la voce 4 appena costruita */
            for (int e = 0; e < 4; e++) { uint8_t* v = s + kRingExt[l] + e * 0x98; memcpy(v, s + lb + 0x960 + 4 * 0x98, 0x98); *(uint8_t**)(v + 0x48) = v + 0x54; } }
        sys = s; ready = true;
        Logging.Log("[mk8dx-8p] rec16: sys %p 0x%x -> 0x%x, record 12..15 costruiti in 8 liste", s, kOldSize, kNewSize);
    }
    /* S2: heap dei record 12..15 come l'avvio (0x3da428: 0x3db670..0x3db7bc e analoghe) */
    static void StableExt(uint8_t* s) {
        if (!ready || s != sys) return;
        uint8_t* bg = *(uint8_t**)(s + 0x4ed0); uint8_t* se = *(uint8_t**)(s + 0x4ee0);
        Logging.Log("[mk8dx-8p] rec16: heap 12..15, BackgroundLoad allocabile 0x%lx, DriverBodySE 0x%lx", (unsigned long)m3d8::MaxAlloc(bg), (unsigned long)m3d8::MaxAlloc(se));
        int fails = 0;
        for (int k = 0; k < 8; k++) { const LD& L = kL[k]; uint8_t* par = L.dbse ? se : bg;
            for (int i = 12; i < 16; i++) { uint8_t* r = RecS(s, k, i);
                for (uint32_t j = 0; j < L.ringN; j++) *(void**)(r + L.ring + j * 8) = nullptr;
                int cap = *(int32_t*)(r + L.name_ + 0x10); if (cap <= 0 || cap > 0x40) cap = 0x40;
                if (L.dbse) snprintf((char*)r + L.nameBuf, cap, "SmartCacheList noname:%d", i);
                else snprintf((char*)r + L.nameBuf, cap, "ModelSmartCacheList(Stable) %s:%d", L.name, i);
                void* h = mk8dx::Fn<void* (*)(size_t, void*, void*, int32_t, int32_t, int32_t)>(0x605128)(L.size, r + L.name_, par, 8, 1, 0);
                *(void**)(r + L.heap) = h;
                if (h) { mk8dx::Fn<void (*)(void*)>(0x8ad040)(h); r[L.loaded] = 0; for (uint32_t j = 0; j < L.nParts; j++) *(void**)(r + L.parts + j * 8) = nullptr; }
                else fails++;
                Ref(k, r) = 0; r[L.used] = 0;
            } }
        Logging.Log("[mk8dx-8p] rec16: heap 12..15 creati (%d falliti), BackgroundLoad allocabile 0x%lx, DriverBodySE 0x%lx", fails,
                    (unsigned long)m3d8::MaxAlloc(bg), (unsigned long)m3d8::MaxAlloc(se));
    }
}
namespace ring9 { static unsigned long evict[2], reuse[2], fresh[2], fallback[2]; }
namespace cachediag {   /* step 7 B S0 (report/step7b_records_plan.md §3 S0): misure delle 8 liste di modelli da 12 record (solo log) */
    struct LD { const char* name; uint32_t base, S, ref, loaded; };
    static const LD kL[8] = {   /* passo, refcount e "caricato" per lista (piano §1.1-1.2) */
        {"Driver", 0x5060, 0xc0, 0x30, 0x34}, {"Emblem", 0x59f8, 0xb0, 0x20, 0x24}, {"Tire", 0x62c0, 0xb0, 0x20, 0x24},
        {"TireSub", 0x6b88, 0x88, 0x28, 0x2c}, {"L71f0", 0x71f0, 0x80, 0x20, 0x24}, {"Body", 0x77f8, 0xc8, 0x38, 0x3c},
        {"Wing", 0x8458, 0xc8, 0x38, 0x3c}, {"Arm", 0x90b8, 0xc0, 0x30, 0x34} };
    static unsigned long ok[8], fail[8];
    static int maxBlocked[8], maxLoaded[8];
    static uint8_t* Sys() { return mk8dx::Fn<uint8_t* (*)()>(0x8ace44)(); }
    static int Blocked(uint8_t* sys, int k) { int n = 0; for (int i = 0; i < rec16::N(); i++) if (*(int32_t*)(rec16::RecS(sys, k, i) + kL[k].ref) > 0) n++; return n; }
    static void Refs(uint8_t* sys, int k, char* buf, int cap) {
        int p = 0; for (int i = 0; i < rec16::N() && p < cap - 12; i++) { uint8_t* r = rec16::RecS(sys, k, i);
            p += snprintf(buf + p, cap - p, "%d%s ", *(int32_t*)(r + kL[k].ref), r[kL[k].loaded] ? "" : "u"); } }
    static void Report(int k, bool failed, uintptr_t lr) {
        if (!failed) { ok[k]++; return; }
        fail[k]++;
        uint8_t* sys = Sys();
        if (fail[k] > 6 || !sys) return;
        char buf[200]; Refs(sys, k, buf, sizeof buf);
        Logging.Log("[mk8dx-8p] cache: %s ricerca FALLITA (fallite %lu, riuscite %lu) da main+%lx | ref (u = non caricato): %s", kL[k].name, fail[k], ok[k],
                    lr - mk8dx::Base(), buf);
        if (k == 0 || k >= 5) m3d8::DumpUnits("cache-1");
    }
    static unsigned long frames, busy, busyMax;
    static void Frame() {   /* da m3d8::MenuFrame: record bloccati per lista e thread di caricamento fermo */
        uint8_t* sys = Sys(); if (!sys) return;
        frames++;
        for (int k = 0; k < 8; k++) {
            int b = Blocked(sys, k), l = 0;
            for (int i = 0; i < rec16::N(); i++) if (rec16::RecS(sys, k, i)[kL[k].loaded]) l++;
            if (l > maxLoaded[k]) maxLoaded[k] = l;
            if (b > maxBlocked[k]) { maxBlocked[k] = b; char buf[200]; Refs(sys, k, buf, sizeof buf);
                Logging.Log("[mk8dx-8p] cache: %s nuovo massimo bloccati %d/%d (f=%lu) ref: %s", kL[k].name, b, rec16::N(), frames, buf); }
        }
        int32_t st = *(int32_t*)(sys + 0x5050);
        if (st) { busy++; if (busy == 121) Logging.Log("[mk8dx-8p] cache: sys+0x5050=%d FERMO da 120 frame (f=%lu)", st, frames); }
        else { if (busy > busyMax) busyMax = busy; busy = 0; }
        if (frames % 300 == 150) {   /* sonda: KartUnit non a riposo (stato != 3) -> stato di tutte le unita' */
            static int n; uint8_t* d = m3d8::Dir(); bool busyU = false;
            for (int i = 0; d && i < *(int32_t*)(d + 0xc0) && i < 8; i++) if (uint8_t* u = m3d8::Unit(d, i)) if (*(int32_t*)(u + 0x358) != 3) busyU = true;
            if (busyU && n++ < 40) { Logging.Log("[mk8dx-8p] cache: unita' non a riposo (f=%lu, modelli %d)", frames, d ? *(int32_t*)(d + 0xc0) : -1); m3d8::DumpUnits("stato"); }
        }
        if (frames % 600 == 0) {
            char buf[200]; int p = 0;
            for (int k = 0; k < 8; k++) p += snprintf(buf + p, sizeof buf - p, "%s %d/%d/%d-%lu ", kL[k].name, Blocked(sys, k), maxBlocked[k], maxLoaded[k], fail[k]);
            Logging.Log("[mk8dx-8p] cache: f=%lu bloccati/max/maxcaricati-fallite: %s | 0x5050 occupato max %lu frame", frames, buf, busyMax);
            if (rec16::ready) Logging.Log("[mk8dx-8p] ring9: Body riuso/libera/sfratto/ripiego %lu/%lu/%lu/%lu cursore %d | Wing %lu/%lu/%lu/%lu cursore %d",
                ring9::reuse[0], ring9::fresh[0], ring9::evict[0], ring9::fallback[0], *(int32_t*)(sys + 0x77f8 + 0xc5c),
                ring9::reuse[1], ring9::fresh[1], ring9::evict[1], ring9::fallback[1], *(int32_t*)(sys + 0x8458 + 0xc5c));
        }
    }
    static void Mem(const char* tag, uint8_t* h) { Logging.Log("[mk8dx-8p] mem: %s heap %p allocabile 0x%lx", tag, h, (unsigned long)m3d8::MaxAlloc(h)); }
}
/* S3: con rec16::ready le 8 ricerche girano a N = 16 (rec16::Search), altrimenti l'originale; in entrambi i casi misura */
#define CACHE_DIAG(N, K, STRIDE) HOOK_DEFINE_TRAMPOLINE(N) { static int32_t Callback(uint8_t* list, void* key, void* c, void* d) { \
    if (rec16::Active()) { bool f; int32_t r = (int32_t)rec16::Search(K, list, key, c, &f); cachediag::Report(K, f, (uintptr_t)__builtin_return_address(0)); return r; } \
    int32_t r = Orig(list, key, c, d); cachediag::Report(K, r < 0, (uintptr_t)__builtin_return_address(0)); return r; } };
CACHE_DIAG(CacheDriverHook, 0, 0xc0)   /* 0x88e6f4 d10103ff */
CACHE_DIAG(CacheBodyHook,   5, 0xc8)   /* 0x88f5b4 d10183ff */
CACHE_DIAG(CacheWingHook,   6, 0xc8)   /* 0x890374 d10183ff */
CACHE_DIAG(CacheArmHook,    7, 0xc0)   /* 0x890944 d10183ff */
#define CACHE_DIAGP(N, K) HOOK_DEFINE_TRAMPOLINE(N) { static void* Callback(uint8_t* list, void* key, void* c, void* d) { \
    if (rec16::Active()) { bool f; void* r = (void*)rec16::Search(K, list, key, c, &f); cachediag::Report(K, f, (uintptr_t)__builtin_return_address(0)); return r; } \
    void* r = Orig(list, key, c, d); cachediag::Report(K, r == nullptr, (uintptr_t)__builtin_return_address(0)); return r; } };
CACHE_DIAGP(CacheTireSubHook, 3)   /* 0x88ecfc d10103ff (ricerca a puntatore, NULL = fallita) */
CACHE_DIAGP(CacheEmblemHook,  1)   /* 0x88f0c0 d100c3ff */
CACHE_DIAGP(Cache71f0Hook,    4)   /* 0x88fb0c d100c3ff */
CACHE_DIAGP(CacheTireHook,    2)   /* 0x88fecc d100c3ff */
HOOK_DEFINE_TRAMPOLINE(MemBgLoadHook) {     /* 0x881ce8 d100c3ff (sys): crea BackgroundLoad 0x13b00000 (genitore = heap corrente) e chiama 0x3da428 */
    static void* Callback(uint8_t* sys, void* b, void* c, void* d) {
        cachediag::Mem("genitore di BackgroundLoad (heap corrente) prima di 0x881ce8", m3d8::CurHeap());
        void* r = Orig(sys, b, c, d);
        cachediag::Mem("heap corrente dopo 0x881ce8", m3d8::CurHeap());
        return r;
    }
};
HOOK_DEFINE_TRAMPOLINE(MemStableHook) {     /* 0x3da428 d10383ff (sys): heap Stable dei record */
    static void* Callback(uint8_t* sys, void* b, void* c, void* d) {
        void* r = Orig(sys, b, c, d);
        rec16::StableExt(sys);   /* S2 */
        cachediag::Mem("BackgroundLoad [sys+0x4ed0] dopo 0x3da428", *(uint8_t**)(sys + 0x4ed0));
        cachediag::Mem("DriverBodySE [sys+0x4ee0] dopo 0x3da428", *(uint8_t**)(sys + 0x4ee0));
        return r;
    }
};
HOOK_DEFINE_TRAMPOLINE(MemCourseHook) {     /* 0x3d5794 d10103ff (this, genitore): DriverBodySE 0x279e000 + CourseResource 0xe404000 */
    static void* Callback(void* t, uint8_t* par, void* c, void* d) {
        cachediag::Mem("genitore di DriverBodySE/CourseResource prima di 0x3d5794", par);
        void* r = Orig(t, par, c, d);
        cachediag::Mem("stesso genitore dopo 0x3d5794", par);
        return r;
    }
};
/* ---- step 7 B S1-S3 (report/step7b_records_plan.md §3): hook delle liste a 16 record ---------------------------------------- */
HOOK_DEFINE_TRAMPOLINE(RecCtorHook) {       /* 0x880c8c d10203ff (sys): ctor base di sys (chiamanti 0x3da08c, 0x8ac24c) */
    static void Callback(uint8_t* s) { Orig(s); rec16::CtorExt(s); }
};
#define REC_PRE(N, K) HOOK_DEFINE_TRAMPOLINE(N) { static void Callback(uint8_t* list, void* key, void* x2, void* x3) { \
    if (rec16::Active()) { rec16::Pre(K, list, key, x2); return; } Orig(list, key, x2, x3); } };
REC_PRE(RecPreDriverHook, 0)   /* 0x3dc4f0 d100c3ff */
REC_PRE(RecPreEmblemHook, 1)   /* 0x3dc6ec d100c3ff */
REC_PRE(RecPreBodyHook,   5)   /* 0x3dc8e8 d10103ff */
REC_PRE(RecPreTireHook,   2)   /* 0x3dcb0c d100c3ff */
REC_PRE(RecPreWingHook,   6)   /* 0x3dcd08 d10103ff */
REC_PRE(RecPreArmHook,    7)   /* 0x3dcf2c d10103ff */
/* accessi per indice (lookup e rilascio): con i >= 12 il csel del gioco ha gia' azzerato l'indice, X8 = sys + 0*S + ...: si aggiunge
   (ext - base) + (i - 12) * S */
#define REC_IDX(N, K, HREG, IOFF) HOOK_DEFINE_INLINE(N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    int32_t i = rec16::Active() ? *(int32_t*)(ctx->X[HREG] + IOFF) : -1; \
    if (i >= 12 && i < 16) ctx->X[8] += (uint64_t)(rec16::kL[K].ext - rec16::kL[K].base) + (uint64_t)(i - 12) * rec16::kL[K].S; } };
REC_IDX(RecIdxDriverHook, 0, 20, 0x90)   /* 0x88e63c f9683d08 ldr x8,[x8,#0x5078] */
REC_IDX(RecIdxBodyHook,   5, 20, 0x98)   /* 0x88f4f4 f97c0d08 ldr x8,[x8,#0x7818] */
REC_IDX(RecIdxWingHook,   6, 20, 0x98)   /* 0x8902b4 f8696908 ldr x8,[x8,x9] */
REC_IDX(RecIdxArmHook,    7, 20, 0x98)   /* 0x89087c f8696908 ldr x8,[x8,x9] */
REC_IDX(RecRelDriverHook, 0, 19, 0x90)   /* 0x88ea40 b9400109 ldr w9,[x8] (refcount) */
REC_IDX(RecRelBodyHook,   5, 19, 0x98)   /* 0x88f92c b9400109 */
REC_IDX(RecRelWingHook,   6, 19, 0x98)   /* 0x8906ec b9400109 */
REC_IDX(RecRelArmHook,    7, 19, 0x98)   /* 0x890d14 b9400109 */
/* rilascio per chiave delle liste a puntatore: a fine ciclo senza corrispondenza (puntatore = fine dei 12) cerca nei record 12..15 */
#define REC_PTRR(N, K, PREG, END) HOOK_DEFINE_INLINE(N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    if (!rec16::ready || ctx->X[PREG] != ctx->X[0] + (END)) return; \
    uint8_t* key = (uint8_t*)ctx->X[19] + 8; \
    for (int i = 12; i < 16; i++) { uint8_t* r = rec16::RecS((uint8_t*)ctx->X[0], K, i); \
        if (rec16::Heap(K, r) && rec16::Loaded(K, r) && rec16::Match(K, r, key)) { if (rec16::Ref(K, r) - 1 >= 0) rec16::Ref(K, r)--; break; } } } };
REC_PTRR(RecPtrRTireSubHook, 3, 9, 0x6bb4 + 0x660)   /* 0x88f018 f9002e7f str xzr,[x19,#0x58] */
REC_PTRR(RecPtrREmblemHook,  1, 8, 0x5a1c + 0x840)   /* 0x88f3f0 f900527f str xzr,[x19,#0xa0] */
REC_PTRR(RecPtrR71f0Hook,    4, 8, 0x7214 + 0x600)   /* 0x88fdc0 f9002a7f str xzr,[x19,#0x50] */
REC_PTRR(RecPtrRTireHook,    2, 8, 0x62e4 + 0x840)   /* 0x8901b4 f900527f str xzr,[x19,#0xa0] */
HOOK_DEFINE_INLINE(RecUsedClrHook) {        /* 0x884ae8 aa1f03f4: dopo i 144 azzeramenti di 0x8845f4 (X19 = sys): "usato" = 0 anche per 12..15 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!rec16::ready) return;
        static const int ks[6] = {0, 1, 2, 5, 6, 7};
        for (int k : ks) for (int i = 12; i < 16; i++) { uint8_t* r = rec16::RecS((uint8_t*)ctx->X[19], k, i); if (rec16::Heap(k, r)) r[rec16::kL[k].used] = 0; }
    }
};
/* segna "usato" (0x3dbff0): all'uscita di ogni giro X8 == 12 = nessuna corrispondenza nei 12; la chiave e' ancora nei registri */
static void RecUsedSet(exl::hook::InlineCtx* ctx, int k, int32_t a, int32_t b, int32_t c, int32_t byte, bool arm3) {
    if (!rec16::ready || ctx->X[8] != 12) return;
    uint8_t* s = mk8dx::Fn<uint8_t* (*)()>(0x8ace44)();
    for (int i = 12; i < 16; i++) { uint8_t* r = rec16::RecS(s, k, i);
        if (!rec16::Heap(k, r) || !rec16::Loaded(k, r)) continue;
        if (*(int32_t*)(r + 8) != a || *(int32_t*)(r + 0xc) != b) continue;
        const int kk = rec16::kL[k].key;
        if ((kk == rec16::K3 || (kk == rec16::KARM && arm3)) && *(int32_t*)(r + 0x10) != c) continue;
        if (kk == rec16::KARM && arm3 && r[0x14] != (uint8_t)byte) continue;
        r[rec16::kL[k].used] = 1; return; }
}
HOOK_DEFINE_INLINE(RecUsedDriverHook) { static void Callback(exl::hook::InlineCtx* c) { RecUsedSet(c, 0, c->W[20], c->W[21], 0, 0, false); } };   /* 0x3dc068 294257f4 */
HOOK_DEFINE_INLINE(RecUsedEmblemHook) { static void Callback(exl::hook::InlineCtx* c) { RecUsedSet(c, 1, c->W[20], c->W[21], 0, 0, false); } };   /* 0x3dc0bc b9400274 */
HOOK_DEFINE_INLINE(RecUsedBodyHook)   { static void Callback(exl::hook::InlineCtx* c) { RecUsedSet(c, 5, c->W[20], c->W[21], c->W[22], 0, false); } };   /* 0x3dc128 b9400674 */
HOOK_DEFINE_INLINE(RecUsedTireHook)   { static void Callback(exl::hook::InlineCtx* c) { RecUsedSet(c, 2, c->W[20], c->W[21], 0, 0, false); } };   /* 0x3dc180 29415674 */
HOOK_DEFINE_INLINE(RecUsedWingHook)   { static void Callback(exl::hook::InlineCtx* c) { RecUsedSet(c, 6, c->W[20], c->W[21], c->W[22], 0, false); } };   /* 0x3dc1e8 29405674 */
HOOK_DEFINE_INLINE(RecUsedArmHook)    { static void Callback(exl::hook::InlineCtx* c) { RecUsedSet(c, 7, c->W[20], c->W[21], c->W[22], c->W[19] & 0xff, c->W[23] == 1); } };   /* 0x3dc308 910023e0 */
/* anello Body/Wing: i record 12..15 ricevono gli stessi puntatori dei record 0..11 (0x885268..0x885290) e li perdono allo smontaggio
   (0x881e8c..0x881f84) */
static void RecRingFill(uint8_t* list) {
    if (!rec16::ready) return;
    int k = (list == rec16::sys + 0x77f8) ? 5 : (list == rec16::sys + 0x8458) ? 6 : -1; if (k < 0) return;
    for (int i = 12; i < 16; i++) { uint8_t* r = rec16::Rec(list, k, i);
        for (int e = 0; e < 5; e++) *(uint8_t**)(r + 0x98 + e * 8) = list + 0x960 + e * 0x98;
        *(uint8_t**)(r + 0xc0) = list + 0xc5c; }
}
static void RecRingDown(uint8_t* list) {
    if (!rec16::ready) return;
    int k = (list == rec16::sys + 0x77f8) ? 5 : (list == rec16::sys + 0x8458) ? 6 : -1; if (k < 0) return;
    for (int i = 12; i < 16; i++) { uint8_t* r = rec16::Rec(list, k, i); for (int e = 0; e < 6; e++) *(void**)(r + 0x98 + e * 8) = nullptr; }
}
/* S4 (report/step7b_records_plan.md §3 S4): anello Body/Wing da 5 a 9 voci (8 anteprime + 1 in caricamento, come 4 + 1 nell'originale) */
namespace ring9 {
    static uint8_t* Ext(int l, int e) { return rec16::sys + rec16::kRingExt[l] + e * 0x98; }
    static void Fill(uint8_t* list, uint64_t size, void* parent) {   /* come 0x885214..0x885258 per le voci 5..8 */
        int l = (list == rec16::sys + 0x77f8) ? 0 : (list == rec16::sys + 0x8458) ? 1 : -1; if (l < 0) return;
        int fails = 0;
        for (int e = 0; e < 4; e++) { uint8_t* v = Ext(l, e);
            int cap = *(int32_t*)(v + 0x50); if (cap <= 0 || cap > 0x40) cap = 0x40;
            snprintf(*(char**)(v + 0x48), cap, "ModelSmartCacheList(Menu) noname:%d", 5 + e);
            void* h = mk8dx::Fn<void* (*)(size_t, void*, void*, int32_t, int32_t, int32_t)>(0x605128)((uint32_t)size, v + 0x40, parent, 8, 1, 0);
            *(void**)(v + 0x18) = h;
            if (h) { mk8dx::Fn<void (*)(void*)>(0x8ad040)(h); v[0x3c] = 0; *(void**)(v + 0x20) = nullptr; *(void**)(v + 0x28) = nullptr; *(void**)(v + 0x30) = nullptr; }
            else fails++;
            *(int32_t*)(v + 0x38) = 0; v[0x3d] = 0; }
        Logging.Log("[mk8dx-8p] ring9: %s voci 5..8 (0x%lx) create, %d fallite, genitore allocabile 0x%lx", l ? "Wing" : "Body", (unsigned long)size, fails,
                    (unsigned long)m3d8::MaxAlloc((uint8_t*)parent));
    }
    static void Down(uint8_t* list) {   /* come 0x881d98..0x881dc8 */
        int l = (list == rec16::sys + 0x77f8) ? 0 : (list == rec16::sys + 0x8458) ? 1 : -1; if (l < 0) return;
        for (int e = 0; e < 4; e++) { uint8_t* v = Ext(l, e);
            if (*(int32_t*)(v + 0x38) - 1 >= 0) (*(int32_t*)(v + 0x38))--;
            if (void* h = *(void**)(v + 0x18)) { reinterpret_cast<void (*)(void*)>(*(uintptr_t*)(*(uintptr_t*)h + 0x20))(h); *(void**)(v + 0x18) = nullptr; **(char**)(v + 0x48) = 0; } }
    }
    /* parte 0 di 0x89d668 / 0x89dae4 con E[0..4] = rec+0x98..+0xb8 e E[5..8] = voci estese; stesso ordine e stesse funzioni del gioco */
    static uintptr_t Part0(int l, uint8_t* rec, uint8_t* x2) {
        using Vt18 = uintptr_t (*)(void*, void*, void*);
        using Vt0 = uintptr_t (*)(void*, void*, int32_t);
        using Load = void* (*)(void*, void*, void*, int32_t);
        uintptr_t* vt = *(uintptr_t**)rec; uint8_t* path = x2 + (l ? 0x138 : 0x88);   /* Body 0x89d7dc / Wing 0x89dc58 */
        const uintptr_t ld = l ? 0x89bc70 : 0x89b290;                               /* caricamento della voce: Body 0x89b290, Wing 0x89bc70 */
        uint8_t* E[9]; for (int i = 0; i < 5; i++) E[i] = *(uint8_t**)(rec + 0x98 + i * 8); for (int i = 0; i < 4; i++) E[5 + i] = Ext(l, i);
        int32_t key[3] = { *(int32_t*)(rec + 8), *(int32_t*)(rec + 0xc), *(int32_t*)(rec + 0x10) };
        uint8_t* hit = nullptr;                                                      /* 1. voce con ref >= 1 e chiave0 uguale (l'ultima vince) */
        for (int i = 0; i < 9; i++) if (E[i] && *(int32_t*)(E[i] + 0x38) >= 1 && *(int32_t*)(E[i] + 8) == key[0]) hit = E[i];
        if (hit) { reuse[l]++; return reinterpret_cast<Vt18>(vt[3])(rec, path, *(void**)(hit + 0x20)); }
        for (int i = 0; i < 9; i++) if (E[i] && *(void**)(E[i] + 0x18) && !E[i][0x3c]) {   /* 2. voce libera (non caricata) */
            if (mk8dx::Fn<Load>(ld)(E[i], key, path, 1)) { fresh[l]++; return reinterpret_cast<Vt18>(vt[3])(rec, path, *(void**)(E[i] + 0x20)); } }
        int32_t* cur = *(int32_t**)(rec + 0xc0);                                     /* 3. sfratto a giro: c = c > 7 ? 0 : c + 1 */
        if (cur) { int c = *cur; uint8_t* v = nullptr;
            for (int t = 0; t < 9 && !v; t++) { c = (c > 7) ? 0 : c + 1; v = (E[c] && *(void**)(E[c] + 0x18)) ? E[c] : nullptr; }
            if (v) { if (*(int32_t*)(v + 0x38) - 1 >= 0) (*(int32_t*)(v + 0x38))--;
                void* r = mk8dx::Fn<Load>(ld)(v, key, path, 1); *cur = c; evict[l]++;
                if (r && *(void**)(v + 0x20)) return reinterpret_cast<Vt18>(vt[3])(rec, path, *(void**)(v + 0x20)); } }
        fallback[l]++;
        return reinterpret_cast<Vt0>(vt[0])(rec, path, 1);                           /* 4. ripiego nell'heap del record */
    }
}
#define REC_RINGFILL(N) HOOK_DEFINE_TRAMPOLINE(N) { static void Callback(uint8_t* list, uint64_t size, void* parent, void* x3, void* name, void* x5) { \
    Orig(list, size, parent, x3, name, x5); RecRingFill(list); if (rec16::ready && MK8DX_P7_RING9) ring9::Fill(list, size, parent); } };
#define REC_RINGDOWN(N) HOOK_DEFINE_TRAMPOLINE(N) { static void Callback(uint8_t* list) { Orig(list); RecRingDown(list); if (rec16::ready && MK8DX_P7_RING9) ring9::Down(list); } };
#define RING_PART0(N, L) HOOK_DEFINE_TRAMPOLINE(N) { static uintptr_t Callback(uint8_t* rec, int32_t part, uint8_t* x2) { \
    return (part != 0 || !rec16::ready || !MK8DX_P7_RING9 || !*(void**)(rec + 0x98)) ? Orig(rec, part, x2) : ring9::Part0(L, rec, x2); } };
RING_PART0(RingPart0BodyHook, 0)   /* 0x89d668 d101c3ff (record, parte, x2): vt+0x10 del record Body */
RING_PART0(RingPart0WingHook, 1)   /* 0x89dae4 d101c3ff: Wing */
REC_RINGFILL(RecRingFillBodyHook)   /* 0x884f2c d10383ff (lista, dim, genitore, x3, nome) */
REC_RINGFILL(RecRingFillWingHook)   /* 0x8852b8 d10383ff */
REC_RINGDOWN(RecRingDownBodyHook)   /* 0x881d80 a9be4ff4 (lista), da 0x881c90 */
REC_RINGDOWN(RecRingDownWingHook)   /* 0x881fa0 a9be4ff4, da 0x881c9c */
HOOK_DEFINE_TRAMPOLINE(MemRingHook) {       /* 0x884bd8 fc1a0fe8 (sys, flag, settings, n): risorse del menu + anello Body/Wing */
    static void Callback(uint8_t* sys, bool f, void* st, int32_t n) {
        size_t a = sys ? m3d8::MaxAlloc(*(uint8_t**)(sys + 0x320)) : 0;
        Orig(sys, f, st, n);
        if (sys) Logging.Log("[mk8dx-8p] mem: [sys+0x320] %p allocabile 0x%lx -> 0x%lx dopo 0x884bd8 (flag %d n %d); BackgroundLoad 0x%lx DriverBodySE 0x%lx",
                             *(uint8_t**)(sys + 0x320), (unsigned long)a, (unsigned long)m3d8::MaxAlloc(*(uint8_t**)(sys + 0x320)), f, n,
                             (unsigned long)m3d8::MaxAlloc(*(uint8_t**)(sys + 0x4ed0)), (unsigned long)m3d8::MaxAlloc(*(uint8_t**)(sys + 0x4ee0)));
    }
};
HOOK_DEFINE_TRAMPOLINE(MatFindHook) {                     /* 0x68fffc a9be4ff4 (model, u16 idx[2], x2) */
    static void* Callback(void* model, uint16_t* idx, void* x2) {
        if (idx && idx[0] == 0xffff) { static int n; if (n++ < 8) { Logging.Log("[mk8dx-8p] guardia: 0x68fffc materiale non trovato (modello %p) da main+%lx", model,
                                                                                (uintptr_t)__builtin_return_address(0) - mk8dx::Base()); m3d8::DumpUnits("guardia"); } return nullptr; }
        return Orig(model, idx, x2);
    }
};
HOOK_DEFINE_TRAMPOLINE(MatUseHook) {                      /* 0x11dc0c d101c3ff (obj+0x50, x1): legge [x0+0x10] senza controllo */
    static void Callback(void* p, void* x1) { if (!p) { static int n; if (n++ < 8) Logging.Log("[mk8dx-8p] guardia: 0x11dc0c con NULL da main+%lx",
                                                          (uintptr_t)__builtin_return_address(0) - mk8dx::Base()); return; } Orig(p, x1); }
};
HOOK_DEFINE_TRAMPOLINE(SelEventDiagHook) {               /* 0x4bbd18 vt92 selezione (pagina, evento): diagnosi P8 (solo log) */
    static void Callback(uint8_t* page, uint8_t* ev) {
        uint32_t p = ev ? *(uint32_t*)(ev + 0x2c) : 99;
        static int n;
        if (p >= 4 && p < 8 && n < 120) { n++; int32_t* q = (int32_t*)(page + 0xa28);
            Logging.Log("[mk8dx-8p] P8diag: evento id=%d tipo=%d slot=%u (stato slot %d) pagina=%x", *(int32_t*)(ev + 0x8), *(int32_t*)(ev + 0x28), p,
                        q[2 * p], *(int32_t*)(page + 0xe8)); }
        Orig(page, ev);
    }
};
HOOK_DEFINE_TRAMPOLINE(BtnPlayerHook) {            /* 0x43ce4c(btn, slot): frame slot+1 e +0x88 (item+0x58) solo bit 0..3 */
    static void Callback(uint8_t* b, int32_t s) {
        Orig(b, s);
        if (s >= 4 && s < 8) *(uint32_t*)(b + 0x88) = (*(uint32_t*)(b + 0x88) & ~0xffu) | (1u << s);
    }
};

/* step5b passo 5 (report/step5b_plan_p5.md): generato da p5_verify.py su main.bin */
namespace p5 {
    struct C { uint32_t off, orig, repl; };
    static const C kClampKart[] = {
        {0x4c04cc, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
        {0x4c0658, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
        {0x4c0dd0, 0xf10012df, 0xf10022df},   /* cmp x22, #0x4                      -> cmp x22, #0x8 */
        {0x4c0e68, 0xf10012df, 0xf10022df},   /* cmp x22, #0x4                      -> cmp x22, #0x8 */
        {0x4c0f0c, 0xf10012bf, 0xf10022bf},   /* cmp x21, #0x4                      -> cmp x21, #0x8 */
        {0x4c0fe4, 0xf10012df, 0xf10022df},   /* cmp x22, #0x4                      -> cmp x22, #0x8 */
        {0x4c154c, 0xf100113f, 0xf100213f},   /* cmp x9, #0x4                       -> cmp x9, #0x8 */
        {0x4c18d0, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
        {0x4c19ec, 0xf10012df, 0xf10022df},   /* cmp x22, #0x4                      -> cmp x22, #0x8 */
        {0x4c1aa4, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
        {0x4c1b8c, 0xf10012bf, 0xf10022bf},   /* cmp x21, #0x4                      -> cmp x21, #0x8 */
        {0x4c27c0, 0xf10012bf, 0xf10022bf},   /* cmp x21, #0x4                      -> cmp x21, #0x8 */
        {0x4c2864, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
        {0x4c2a10, 0xf100113f, 0xf100213f},   /* cmp x9, #0x4                       -> cmp x9, #0x8 */
        {0x4c2a9c, 0xf100113f, 0xf100213f},   /* cmp x9, #0x4                       -> cmp x9, #0x8 */
        {0x4c2ff8, 0xf100113f, 0xf100213f},   /* cmp x9, #0x4                       -> cmp x9, #0x8 */
        {0x4c312c, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
        {0x4c31a0, 0xf10012df, 0xf10022df},   /* cmp x22, #0x4                      -> cmp x22, #0x8 */
        {0x4c3204, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
        {0x4c350c, 0xf10012bf, 0xf10022bf},   /* cmp x21, #0x4                      -> cmp x21, #0x8 */
        {0x4c3d74, 0xf100113f, 0xf100213f},   /* cmp x9, #0x4                       -> cmp x9, #0x8 */
        {0x4c3ed0, 0xf100113f, 0xf100213f},   /* cmp x9, #0x4                       -> cmp x9, #0x8 */
        {0x4c451c, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
    };
    static const C kClampKart2[] = {
        {0x4c2948, 0xf100129f, 0xf100229f},   /* cmp x20, #0x4                      -> cmp x20, #0x8 */
        {0x4c2b70, 0x7100127f, 0x7100227f},   /* cmp w19, #0x4                      -> cmp w19, #0x8 */
        {0x4c2f20, 0x7100129f, 0x7100229f},   /* cmp w20, #0x4                      -> cmp w20, #0x8 */
        {0x4c3dc4, 0x7100111f, 0x7100211f},   /* cmp w8, #0x4                       -> cmp w8, #0x8 */
        {0x4c3e40, 0x7100103f, 0x7100203f},   /* cmp w1, #0x4                       -> cmp w1, #0x8 */
        {0x4c3fe4, 0x7100103f, 0x7100203f},   /* cmp w1, #0x4                       -> cmp w1, #0x8 */
        {0x4c16d4, 0x7100117f, 0x7100217f},   /* cmp w11, #0x4                      -> cmp w11, #0x8 */
        {0x4c1dd8, 0x7100127f, 0x7100227f},   /* cmp w19, #0x4                      -> cmp w19, #0x8 */
        {0x4c1e8c, 0x7100127f, 0x7100227f},   /* cmp w19, #0x4                      -> cmp w19, #0x8 */
        {0x4c1f40, 0x7100127f, 0x7100227f},   /* cmp w19, #0x4                      -> cmp w19, #0x8 */
        {0x49ae2c, 0x7100103f, 0x7100203f},   /* cmp w1, #0x4                       -> cmp w1, #0x8 */
    };
    static const C kReloc460[] = {
        {0x4c1574, 0xb904616a, 0xb904a16a},   /* str w10, [x11, #0x460]             -> str w10, [x11, #0x4a0] */
        {0x4c158c, 0xb904615f, 0xb904a15f},   /* str wzr, [x10, #0x460]             -> str wzr, [x10, #0x4a0] */
        {0x4c15a0, 0xb904615f, 0xb904a15f},   /* str wzr, [x10, #0x460]             -> str wzr, [x10, #0x4a0] */
        {0x4c15b4, 0xb904615f, 0xb904a15f},   /* str wzr, [x10, #0x460]             -> str wzr, [x10, #0x4a0] */
        {0x4c15d0, 0xb904615f, 0xb904a15f},   /* str wzr, [x10, #0x460]             -> str wzr, [x10, #0x4a0] */
        {0x4c15dc, 0xb904613f, 0xb904a13f},   /* str wzr, [x9, #0x460]              -> str wzr, [x9, #0x4a0] */
        {0x4c2954, 0xb944612a, 0xb944a12a},   /* ldr w10, [x9, #0x460]              -> ldr w10, [x9, #0x4a0] */
        {0x4c2958, 0x91118128, 0x91128128},   /* add x8, x9, #0x460                 -> add x8, x9, #0x4a0 */
        {0x4c2974, 0xb944652a, 0xb944a52a},   /* ldr w10, [x9, #0x464]              -> ldr w10, [x9, #0x4a4] */
        {0x4c2980, 0xb904652b, 0xb904a52b},   /* str w11, [x9, #0x464]              -> str w11, [x9, #0x4a4] */
        {0x4c2988, 0x91119129, 0x91129129},   /* add x9, x9, #0x464                 -> add x9, x9, #0x4a4 */
        {0x4c2998, 0xb944652a, 0xb944a52a},   /* ldr w10, [x9, #0x464]              -> ldr w10, [x9, #0x4a4] */
        {0x4c29a4, 0xb904652b, 0xb904a52b},   /* str w11, [x9, #0x464]              -> str w11, [x9, #0x4a4] */
        {0x4c29ac, 0x91119129, 0x91129129},   /* add x9, x9, #0x464                 -> add x9, x9, #0x4a4 */
        {0x4c2a2c, 0xb944616b, 0xb944a16b},   /* ldr w11, [x11, #0x460]             -> ldr w11, [x11, #0x4a0] */
        {0x4c2ab8, 0xb944616c, 0xb944a16c},   /* ldr w12, [x11, #0x460]             -> ldr w12, [x11, #0x4a0] */
        {0x4c2ac4, 0x9111816b, 0x9112816b},   /* add x11, x11, #0x460               -> add x11, x11, #0x4a0 */
        {0x4c2b80, 0xb904612a, 0xb904a12a},   /* str w10, [x9, #0x460]              -> str w10, [x9, #0x4a0] */
        {0x4c2f2c, 0x9111812a, 0x9112812a},   /* add x10, x9, #0x460                -> add x10, x9, #0x4a0 */
        {0x4c2f40, 0xb904653f, 0xb904a53f},   /* str wzr, [x9, #0x464]              -> str wzr, [x9, #0x4a4] */
        {0x4c2f78, 0xb904653f, 0xb904a53f},   /* str wzr, [x9, #0x464]              -> str wzr, [x9, #0x4a4] */
        {0x4c3014, 0xb944616c, 0xb944a16c},   /* ldr w12, [x11, #0x460]             -> ldr w12, [x11, #0x4a0] */
        {0x4c3020, 0x9111816b, 0x9112816b},   /* add x11, x11, #0x460               -> add x11, x11, #0x4a0 */
        {0x4c3d90, 0xb944616c, 0xb944a16c},   /* ldr w12, [x11, #0x460]             -> ldr w12, [x11, #0x4a0] */
        {0x4c3d9c, 0x9111816b, 0x9112816b},   /* add x11, x11, #0x460               -> add x11, x11, #0x4a0 */
        {0x4c3eec, 0xb944616c, 0xb944a16c},   /* ldr w12, [x11, #0x460]             -> ldr w12, [x11, #0x4a0] */
        {0x4c3ef8, 0x9111816b, 0x9112816b},   /* add x11, x11, #0x460               -> add x11, x11, #0x4a0 */
        {0x4c3ff8, 0xb9446108, 0xb944a108},   /* ldr w8, [x8, #0x460]               -> ldr w8, [x8, #0x4a0] */
        {0x4c4690, 0xb9046a7f, 0xb904aa7f},   /* str wzr, [x19, #0x468]             -> str wzr, [x19, #0x4a8] */
        {0x4c4694, 0xb904727f, 0xb904b27f},   /* str wzr, [x19, #0x470]             -> str wzr, [x19, #0x4b0] */
        {0x4c4698, 0xb9047a7f, 0xb904ba7f},   /* str wzr, [x19, #0x478]             -> str wzr, [x19, #0x4b8] */
        {0x4c469c, 0xb904627f, 0xb904a27f},   /* str wzr, [x19, #0x460]             -> str wzr, [x19, #0x4a0] */
        {0x4c4964, 0xb9046a7f, 0xb904aa7f},   /* str wzr, [x19, #0x468]             -> str wzr, [x19, #0x4a8] */
        {0x4c4968, 0xb904727f, 0xb904b27f},   /* str wzr, [x19, #0x470]             -> str wzr, [x19, #0x4b0] */
        {0x4c496c, 0xb9047a7f, 0xb904ba7f},   /* str wzr, [x19, #0x478]             -> str wzr, [x19, #0x4b8] */
        {0x4c4970, 0xb904627f, 0xb904a27f},   /* str wzr, [x19, #0x460]             -> str wzr, [x19, #0x4a0] */
        {0x4c4c38, 0xb9046a7f, 0xb904aa7f},   /* str wzr, [x19, #0x468]             -> str wzr, [x19, #0x4a8] */
        {0x4c4c3c, 0xb904727f, 0xb904b27f},   /* str wzr, [x19, #0x470]             -> str wzr, [x19, #0x4b0] */
        {0x4c4c40, 0xb9047a7f, 0xb904ba7f},   /* str wzr, [x19, #0x478]             -> str wzr, [x19, #0x4b8] */
        {0x4c4c44, 0xb904627f, 0xb904a27f},   /* str wzr, [x19, #0x460]             -> str wzr, [x19, #0x4a0] */
    };
    static const C kFactory[] = {
        {0x409248, 0x52809400, 0x52809c00},   /* mov w0, #0x4a0                     -> mov w0, #0x4e0 */
        {0x409274, 0x52809400, 0x52809c00},   /* mov w0, #0x4a0                     -> mov w0, #0x4e0 */
        {0x4092a0, 0x52809400, 0x52809c00},   /* mov w0, #0x4a0                     -> mov w0, #0x4e0 */
    };
    static const C kPartsCursor[] = {
        {0x49ad48, 0xf1000f3f, 0xf1001f3f},   /* cmp x25, #0x3                      -> cmp x25, #0x7 */
        {0x408f28, 0x321d17e0, 0x52804600},   /* orr w0, wzr, #0x1f8                -> mov w0, #0x230 */
    };
    struct H { uint32_t off, word; };
    static const H kHookWords[] = {
        {0x4bfdf4, 0xaa0003f3},   /* I mov x19, x0                    mov x19,x0 in 0x4bfdd0: W1 (conteggio +0x110 di 0x8bb0ac) = 8 durante la costruzione B */
        {0x4c192c, 0xb9803108},   /* I ldrsw x8, [x8, #0x30]          kPageSlotFix onEnter (cursore = [x19+0x1b0]) */
        {0x4c1b08, 0xb9803108},   /* I ldrsw x8, [x8, #0x30]          kPageSlotFix onEnter nomi (T8 C9) */
        {0x4c2bd8, 0xb9803129},   /* I ldrsw x9, [x9, #0x30]          kPageSlotFix annulla (T9 C8) */
        {0x4c3dd0, 0xb980314a},   /* I ldrsw x10, [x10, #0x30]        kPageSlotFix eventi mossa (T10 C9) */
        {0x4c3e4c, 0xb9803129},   /* I ldrsw x9, [x9, #0x30]          kPageSlotFix eventi decide (T9 C8) */
        {0x4c1dec, 0x8b090908},   /* I add x8, x8, x9, lsl #2         cfg8 +0x1fd4 (B8 I9) */
        {0x4c1ea0, 0x8b090908},   /* I add x8, x8, x9, lsl #2         cfg8 +0x1ff4 (B8 I9) */
        {0x4c1f54, 0x8b090908},   /* I add x8, x8, x9, lsl #2         cfg8 +0x1fe4 (B8 I9) */
        {0x49ad98, 0x5100eac8},   /* I sub w8, w22, #0x3a             PartsCursor onCreate: x26 += 0x40 per i>=4 */
        {0x49ae34, 0x8b080c08},   /* I add x8, x0, x8, lsl #3         0x49ae1c: x8 = w1+4 per 4..7 */
        {0x4c2ba4, 0x8b160c08},   /* I add x8, x0, x22, lsl #3        annulla: PartsCursor x0 += 0x20 se x22>=4 */
        {0x4c2fa0, 0x8b150c08},   /* I add x8, x0, x21, lsl #3        conferma: PartsCursor x0 += 0x20 se x21>=4 */
        {0x4c4c84, 0xd10103ff},   /* T sub sp, sp, #0x40              onCreate kart Multi (costruzione B) */
        {0x4c3564, 0xd10283ff},   /* T sub sp, sp, #0xa0              vt+0x290 uscita kart */
        {0x14a038, 0x91010008},   /* T add x8, x0, #0x40              guardia modello nullo (0x148b50 legge [x0+0xd7]) */
        {0x49ae90, 0xf81d0ff5},   /* T str x21, [sp, #-0x30]!         PartsCursor Loop 4..7 (opzionale) */
    };
}
/* ---- step 5b passo 5 (report/step5b_plan_p5.md): pagina kart a 8 slot a turno unico (set B sulla pagina nomi 0x12) ---- */
#ifndef MK8DX_P5_HIDE3D
#define MK8DX_P5_HIDE3D 1        /* 1 = a turno unico i 4 modelli 3D (posizioni Multi) sono nascosti */
#endif
/* ---- step 7 B (report/step7b_plan.md): anteprime 3D a 8 nella selezione. Tappa 0 = sonde di log + piedistalli nascosti a turno unico */
namespace m3d8 {
    static uint8_t* CurHeap(); static size_t MaxAlloc(uint8_t* h); extern uint8_t* heapResident;
    static uint8_t* Settings() { return mk8dx::Fn<uint8_t* (*)()>(0x86f7bc)(); }
    static uint8_t* ViewerOwner() { uint8_t* s = mk8dx::Fn<uint8_t* (*)()>(0x398898)(); return s ? *(uint8_t**)(s + 0x270) : nullptr; }
    static uint8_t* Viewer() { uint8_t* o = ViewerOwner(); return o ? *(uint8_t**)(o + 0x70) : nullptr; }
    static uint8_t* Dir() { uint8_t* d = mk8dx::Fn<uint8_t* (*)()>(0x7e7434)(); d = d ? *(uint8_t**)(d + 0x190 + 4 * 8) : nullptr;
                            return d ? *(uint8_t**)(d + 0x238) : nullptr; }
    static uint8_t* Unit(uint8_t* d, int i) { if (!d || i < 0 || i >= *(int32_t*)(d + 0xc0)) return nullptr;
                                              uint8_t** a = *(uint8_t***)(d + 0xc8); return a ? a[i] : nullptr; }
    static uint8_t* Obj(uint8_t* d, int i) { uint8_t* u = Unit(d, i); return u ? *(uint8_t**)(u + 0x30) : nullptr; }   /* come 0x13d47c */
    static void HidePedestals() {                       /* 0x427648(owner, voce, on, vista) -> 0x430cc4: bit di vista del piedistallo */
        uint8_t* o = ViewerOwner(); if (!o || !*(uint8_t**)(o + 0x70)) return;
        for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) mk8dx::Fn<void (*)(uint8_t*, int, bool, int)>(0x427648)(o, i, false, k);
    }
    static void LogDesc(const char* tag, uint8_t* st) {
        if (!st) return;
        for (int i = 0; i < 8; i++) { int32_t* q = (int32_t*)(st + 0x30 + i * 0x1c);
            Logging.Log("[mk8dx-8p] m3d8 %s desc%d: %d %d %d %d %d %d %d", tag, i, q[0], q[1], q[2], q[3], q[4], q[5], q[6]); }
    }
    static void Probe(const char* tag) {
        uint8_t* st = Settings(); uint8_t* d = Dir(); uint8_t* v = Viewer(); uint8_t* m = rlm::mgr;
        uint8_t* sc = mk8dx::Fn<uint8_t* (*)()>(0x8ace60)();
        uint64_t used = 0, total = 0;
        svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0); svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        Logging.Log("[mk8dx-8p] m3d8 %s: settings+8=%d +0x180=%d dir=%p n=%d e9=%d ea=%d heap=%p viewer=%p rlm modo=%d tipoScena=%d mem=%lu/%lu KiB",
            tag, st ? *(int32_t*)(st + 8) : -1, st ? *(int32_t*)(st + 0x180) : -1, d, d ? *(int32_t*)(d + 0xc0) : -1, d ? d[0xe9] : -1,
            d ? d[0xea] : -1, d ? *(void**)(d + 0xd0) : nullptr, v, m ? *(int32_t*)(m + 0xbd8) : -1, rlm::sceneType,
            (unsigned long)(used >> 10), (unsigned long)(total >> 10));
        Logging.Log("[mk8dx-8p] m3d8 %s: HeapResident %p allocabile 0x%lx, heap corrente allocabile 0x%lx", tag, heapResident,
                    (unsigned long)MaxAlloc(heapResident), (unsigned long)MaxAlloc(CurHeap()));
        if (uint8_t* sys = mk8dx::Fn<uint8_t* (*)()>(0x8ace44)()) {       /* slot di risorse (0x884b50, ciclo 0x882fd4): sys+i*0x20+0x160/+0x168/+0x170 */
            uint8_t* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)();
            int32_t* m1cc = rm ? (int32_t*)(rm + 0x1cc) : nullptr;
            Logging.Log("[mk8dx-8p] m3d8 %s: sys=%p +5040=%08x +5050=%lx entry n=%d rm+1cc={%d,%d,%d,%d,%d,%d,%d,%d}", tag, sys, *(uint32_t*)(sys + 0x5040),
                *(uint64_t*)(sys + 0x5050), rm ? *(int32_t*)(rm + 0x50) : -1, m1cc ? m1cc[0] : -9, m1cc ? m1cc[1] : -9, m1cc ? m1cc[2] : -9, m1cc ? m1cc[3] : -9,
                m1cc ? m1cc[4] : -9, m1cc ? m1cc[5] : -9, m1cc ? m1cc[6] : -9, m1cc ? m1cc[7] : -9);
            for (int i = 0; rm && i < 8 && i < *(int32_t*)(rm + 0x50); i++) { uint8_t* e = (*(uint8_t***)(rm + 0x58))[i]; if (!e) continue; int32_t* q = (int32_t*)(e + 0x210);
                Logging.Log("[mk8dx-8p] m3d8 %s: entry%d +210: %d %d %d %d %d %d %d | richiesta slot %d: %d %d %d %d", tag, i, q[0], q[1], q[2], q[3], q[4], q[5], q[6], i,
                    *(int32_t*)(sys + 0x4d1c + i * 0x1c), *(int32_t*)(sys + 0x4d20 + i * 0x1c), *(int32_t*)(sys + 0x4d24 + i * 0x1c), *(int32_t*)(sys + 0x4d28 + i * 0x1c)); }
            for (int i = 0; i < 12; i++) { uint8_t* b = sys + i * 0x20; uint8_t* l0 = *(uint8_t**)(b + 0x160); uint8_t* l1 = *(uint8_t**)(b + 0x168);
                int32_t* q = st ? (int32_t*)(st + 0x30 + i * 0x1c) : nullptr;
                uint8_t* e = (rm && i < *(int32_t*)(rm + 0x50)) ? (*(uint8_t***)(rm + 0x58))[i] : nullptr;
                Logging.Log("[mk8dx-8p] m3d8 %s: slot%d cur=%d L0=%p d38=%p L1=%p d38=%p def=%p | desc %d %d %d %d tipo=%d | entry+230=%d",
                    tag, i, *(int32_t*)(b + 0x170), l0, l0 ? *(void**)(l0 + 0xd38) : nullptr, l1, l1 ? *(void**)(l1 + 0xd38) : nullptr,
                    *(void**)(sys + 0xf8 + i * 8), q ? q[0] : -9, q ? q[1] : -9, q ? q[2] : -9, q ? q[3] : -9, q ? q[5] : -9, e ? *(int32_t*)(e + 0x230) : -9); }
        }
        if (sc) { uint8_t* mk = sc + 0xc0;
            Logging.Log("[mk8dx-8p] m3d8 %s: scene modo=%d maschere kart %02x %02x %02x %02x %02x %02x %02x %02x", tag, *(int32_t*)(sc + 0xcc),
                mk[0], mk[1], mk[2], mk[3], mk[4], mk[5], mk[6], mk[7]);
            DumpScene("m3d8 scene", sc); }
        for (int i = 0; d && i < 8; i++) {
            uint8_t* u = Unit(d, i); uint8_t* o = Obj(d, i); if (!u) continue;
            Logging.Log("[mk8dx-8p] m3d8 %s: unit%d=%p +40=%d +360=%d +362=%04x obj=%p slot=%d tipo=%d vis=%d eye=%.1f,%.1f,%.1f at=%.1f,%.1f,%.1f fovy=%.3f sh=%.3f,%.3f pos=%.0f,%.0f,%.0f",
                tag, i, u, *(int32_t*)(u + 0x40), u[0x360], *(uint16_t*)(u + 0x362), o, o ? *(int32_t*)(o + 0xc0) : -1, o ? *(int32_t*)(o + 0xc4) : -1,
                o ? o[0xd4] : -1, o ? *(float*)(o + 0x8c) : 0, o ? *(float*)(o + 0x90) : 0, o ? *(float*)(o + 0x94) : 0,
                o ? *(float*)(o + 0x98) : 0, o ? *(float*)(o + 0x9c) : 0, o ? *(float*)(o + 0xa0) : 0, o ? *(float*)(o + 0xb0) : 0,
                o ? *(float*)(o + 0xb4) : 0, o ? *(float*)(o + 0xb8) : 0, o ? *(float*)(o + 0xec) : 0, o ? *(float*)(o + 0xf0) : 0, o ? *(float*)(o + 0xf4) : 0);
        }
        for (int i = 0; v && i < 4; i++) { uint8_t* e = v + 0x568 + i * 0x138; uint8_t* mdl = *(uint8_t**)(e + 8);
            Logging.Log("[mk8dx-8p] m3d8 %s: piedistallo%d mdl=%p pos=%.0f,%.0f,%.0f rot=%.2f anm=%04x/%04x", tag, i, mdl,
                *(float*)(e + 0x34), *(float*)(e + 0x38), *(float*)(e + 0x3c), *(float*)(e + 0x44), *(uint16_t*)(e + 0xc8), *(uint16_t*)(e + 0xca)); }
        if (m) {
            uint8_t** sub = *(uint8_t***)(m + 0x128);
            for (int k = 0; sub && k < 4; k++) { uint8_t* ly = sub[k]; if (!ly) continue;
                float* r = (float*)(ly + 0x30); uint8_t* pj = *(uint8_t**)(ly + 0x70); uint8_t* cm = *(uint8_t**)(ly + 0x68);
                Logging.Log("[mk8dx-8p] m3d8 %s: sub%d=%p on=%d rect=%.0f,%.0f,%.0f,%.0f proj=%p fovy=%.3f asp=%.3f b4=%.3f b8=%.3f bc=%.3f cam=%.1f,%.1f,%.1f",
                    tag, k, ly, ly[0x78] & 1, r[0], r[1], r[2], r[3], pj, pj ? *(float*)(pj + 0xa0) : 0, pj ? *(float*)(pj + 0xb0) : 0,
                    pj ? *(float*)(pj + 0xb4) : 0, pj ? *(float*)(pj + 0xb8) : 0, pj ? *(float*)(pj + 0xbc) : 0,
                    cm ? *(float*)(cm + 0x38) : 0, cm ? *(float*)(cm + 0x3c) : 0, cm ? *(float*)(cm + 0x40) : 0); }
            float* a = (float*)(m + 0xa38);
            for (int k = 0; k < 13; k++) Logging.Log("[mk8dx-8p] m3d8 %s: rlm rect%d %.0f %.0f %.0f %.0f", tag, k, a[k * 4], a[k * 4 + 1], a[k * 4 + 2], a[k * 4 + 3]);
        }
    }
}
namespace m3d8 {
    static bool init8;                                  /* l'init corrente del KartDirector ha 8 KartUnit */
    static unsigned long frames, stamp = ~0ul, lgDump;          /* frame del menu / ultimo frame con selezione o kart a 8 visibile */
    static bool shown;                                  /* viste extra del menu accese */
    static float subRect[4][4], subAsp[4]; static bool saved;
    alignas(16) static uint8_t preset8[8][0x74]; static bool presetsMade;
    /* taratura (report/step7b_plan.md §7): celle 640x160 centrate sul modello (riga r: centro y = 274 - 160 r, unita' 1280x720 con 0 al
       centro, come il riferimento Switch 2); dy = base - sh_y*H con base = -44 * (H/320) * tan(0.25)/tan(fovy'/2) (Multi), dx = -sh_x*W.
       Celle da 180 a griglia fissa tagliavano il modello delle righe basse (screenshot 23:49) */
    /* scala e posizione come Switch 2 (report/step7b_plan.md §8, misure sugli screenshot di riferimento a 1280x720): piedistallo largo
       ~92 px, base a y = 132 + 158 r, centro x 160 / 1120. Con celle alte 160 e fovy dei preset Multi la larghezza e' 99-101 px:
       tan(fovy/2) x 1/0.925 (zoom 0.925); sh_y = 0 (come i preset Multi della riga bassa 5/6) tiene la base sulla targhetta */
    static constexpr float kZoom = 0.925f;
    static constexpr float kShX = 0.25f;                /* 160/640 */
    static constexpr float kShY[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static constexpr float kCellH = 160.0f, kRow0Y = 554.0f;   /* y0 (dal basso) della riga 0; riga r: kRow0Y - 160 r */
    static bool N8() { uint8_t* d = Dir(); return MK8DX_P7_MENU3D8 && d && *(int32_t*)(d + 0xc0) >= 8; }
    static bool On8() { return N8() && p4::one && (frames - stamp <= 3                     /* timbro per frame (vt84, stato 4) */
                               || (MK8DX_SEL8B && (sel8b::selOpen || sel8b::kartOpen) && frames - sel8b::openAt <= 3)); }   /* step 5c H7 */
    static uint8_t* sub0;                               /* step 5c M2: sub3d[0] dell'ultimo MenuFrame (solo log) */
    static void Stamp() { stamp = frames; }
    static void MakePresets() {
        if (presetsMade) return;
        uint8_t* base = (uint8_t*)(mk8dx::Base() + 0x131f450);
        for (int s = 0; s < 8; s++) {
            memcpy(preset8[s], base + 0x38 + (3 + (s & 1)) * 0x74, 0x74);       /* colonna sinistra = preset 3, destra = preset 4 */
            for (int r = 0; r < 3; r++) { float* rec = (float*)(preset8[s] + r * 0x1c);
                rec[4] = 2.0f * __builtin_atanf(__builtin_tanf(rec[4] * 0.5f) / kZoom);
                rec[5] = (s & 1) ? -kShX : kShX; rec[6] = kShY[s >> 1]; }
        }
        presetsMade = true;
        float* r0 = (float*)preset8[0];
        Logging.Log("[mk8dx-8p] m3d8: preset 8pl pronti (A: off %.1f,%.1f,%.1f pitch %.2f fovy %.3f sh %.3f,%.3f)", r0[0], r0[1], r0[2], r0[3], r0[4], r0[5], r0[6]);
    }
    static void Cell(int s, float W, float H, float* r) {        /* slot s: colonna s&1, riga s>>1; y = 0 in basso */
        float k = H / 720.0f;
        r[0] = (s & 1) * W * 0.5f; r[1] = (kRow0Y - (s >> 1) * kCellH) * k; r[2] = r[0] + W * 0.5f; r[3] = r[1] + kCellH * k;
    }
    static void UnitViewBit(uint8_t* u, int b, bool on) {       /* come 0x16f030..0x16f048 + 0x16e288: nodi del modello attivo */
        int32_t sel = *(int32_t*)(u + 0x348); uint8_t* lst = u + 0x248 + ((uint32_t)sel < 2 ? sel : 0) * 0x78;
        int guard = 0;
        for (uint8_t* nd = *(uint8_t**)(lst + 8); nd && nd != lst && guard < 64; nd = *(uint8_t**)(nd + 8), guard++)
            if (uint8_t* w = *(uint8_t**)(nd + 0x10)) mk8dx::Fn<void (*)(uint8_t*, bool, int)>(0x113c88)(w, on, b);
    }
}
namespace m3d8 {
    static uint8_t* resSys; static uint8_t* resHeap; static bool loadersReady, kickSched;
    /* slot di risorse 4..7 (sys+i*0x20+0x160/+0x168): caricatori preparati come 0x884c90..0x884ce0 (s0 = 1.5), ma solo all'ingresso nella
       selezione a 8: preparati all'init del menu rompono gli attori creati dopo (anim Shutter, resfile NULL, salto a 0 in 0x42f454) */
    [[maybe_unused]] static void PrepareLoaders() {   /* superato dal flusso a 8 (slot preparati dal gioco) */
        if (loadersReady || !resSys || !resHeap) return;
        loadersReady = true;
        for (int i = 4; i < 8; i++) for (int b = 0; b < 2; b++)
            if (uint8_t* l = *(uint8_t**)(resSys + i * 0x20 + 0x160 + b * 8))
                mk8dx::Fn<void (*)(void*, void*, int, void*, float)>(0x891984)(l, resHeap, 0, nullptr, 1.5f);
        /* ricostruzione delle unita' 4..7 dai propri slot, prima di qualunque altro caricamento (scheduler 0x3deee8, this+0x8d4 = 4) */
        if (uint8_t* d = Dir()) for (int k = 4; k < 8; k++) if (uint8_t* u = Unit(d, k)) { u[0x350] = 1; *(int32_t*)(u + 0x354) = 0x15; }
        kickSched = true;
        Logging.Log("[mk8dx-8p] m3d8: caricatori degli slot 4..7 preparati (sys=%p heap=%p)", resSys, resHeap);
    }
}
namespace m3d8 {
    static uint8_t* CurHeap() { uintptr_t g = *(uintptr_t*)(mk8dx::Base() + 0x12f9e48); return g ? mk8dx::Fn<uint8_t* (*)(void*)>(0x609f64)(*(void**)g) : nullptr; }
    static size_t MaxAlloc(uint8_t* h) { if (!h) return 0; auto f = reinterpret_cast<size_t (*)(void*, int)>(*(uintptr_t*)(*(uintptr_t*)h + 0x80)); return f(h, 8); }   /* vt+0x80 come 0x605188 */
}
#ifndef MK8DX_P7_MENUHEAP
#define MK8DX_P7_MENUHEAP 0x10000000   /* heap della scena menu con 8 KartUnit (originale 0xd200000) */
#endif
#ifndef MK8DX_P7_EXP8INIT
#define MK8DX_P7_EXP8INIT 0      /* misura (report/step7b_menu8.md §4): 8 slot di risorse preparati all'init del menu */
#endif
namespace m3d8 { uint8_t* heapResident; }
HOOK_DEFINE_TRAMPOLINE(HeapCreateHook) {                  /* 0x605130 f81b0ff9 (dim, nome {vt, char*}, genitore, allineamento, w4, w5): solo log */
    static uint8_t* Callback(size_t sz, void** name, uint8_t* parent, int32_t al, int32_t w4, int32_t w5) {
        uint8_t* par = parent ? parent : m3d8::CurHeap();
        size_t before = (MK8DX_LOG && sz >= 0x100000) ? m3d8::MaxAlloc(par) : 0;   /* solo per il log */
        const char* nm = name ? (const char*)name[1] : "?";
        /* heap della scena menu (task, creato da 0x603290 con la dimensione della voce del task): 0xd200000 basta per 4 KartUnit; a 8 servono
           ~35 MiB in piu' (KartWork x2, 4 slot di risorse) altrimenti HeapResident (100 MiB, 0x425c9c) non entra e l'heap si esaurisce
           (eden_log_heap_exp0.txt: allocabile 0 dopo il menu) */
        if (MK8DX_P7_MENU3D8 && rlm::sceneType == 1 && sz == 0xd200000 && nm && nm[0] == 'T' && nm[4] == 'H' &&
            (uintptr_t)__builtin_return_address(0) - mk8dx::Base() == 0x603290) {
            sz = MK8DX_P7_MENUHEAP; Logging.Log("[mk8dx-8p] heap: scena menu 0xd200000 -> 0x%lx", (unsigned long)sz); }
        /* step 7 B S2: TaskHeap d'avvio (genitore di BackgroundLoad, 16 MiB liberi dopo di essa: eden_log_rec_s0.txt) +0x3a00000 come
           BackgroundLoad 0x13b00000 -> 0x17500000 (record 12..15 di 6 liste, 0x38851e4) */
        if (MK8DX_P7_REC16 && rec16::sizeOk && rlm::sceneType == -1 && sz == 0x15b00000 && nm && nm[0] == 'T' && nm[4] == 'H' &&
            (uintptr_t)__builtin_return_address(0) - mk8dx::Base() == 0x603290) {
            sz = 0x19500000; Logging.Log("[mk8dx-8p] heap: TaskHeap d'avvio 0x15b00000 -> 0x%lx", (unsigned long)sz); }
        uint8_t* h = Orig(sz, name, parent, al, w4, w5);
        if (nm && nm[0] == 'H' && nm[1] == 'e' && nm[4] == 'R' && nm[5] == 'e') m3d8::heapResident = h;   /* "HeapResident" */
        if (sz >= 0x100000 || !h) { static int n; if (n++ < 200) Logging.Log("[mk8dx-8p] heap: crea \"%s\" 0x%lx da %p (allocabile prima 0x%lx) -> %p tipoScena=%d lr=main+%lx", nm ? nm : "?",
                                    (unsigned long)sz, par, (unsigned long)before, h, rlm::sceneType, (uintptr_t)__builtin_return_address(0) - mk8dx::Base()); }
        return h;
    }
};
namespace m3d8 {
    static void LogEntries(const char* tag) {
        uint8_t* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)(); if (!rm) return;
        for (int i = 0; i < 8 && i < *(int32_t*)(rm + 0x50); i++) { uint8_t* e = (*(uint8_t***)(rm + 0x58))[i]; if (!e) continue; int32_t* q = (int32_t*)(e + 0x210);
            Logging.Log("[mk8dx-8p] m3d8 %s: entry%d +44=%d +50=%d tipo224=%d parti %d %d %d %d +22c=%d +230=%d", tag, i, *(int32_t*)(e + 0x44), *(int32_t*)(e + 0x50),
                *(int32_t*)(e + 0x224), q[0], q[1], q[2], q[3], *(int32_t*)(e + 0x22c), *(int32_t*)(e + 0x230)); }
    }
}
HOOK_DEFINE_TRAMPOLINE(SettingsBuildHook) {                /* 0x869a14 (raceMgr): settings B dai descrittori delle entry valide (solo log) */
    static void Callback(uint8_t* rm) {
        static int n; bool lg = n++ < 6;
        if (lg) { Logging.Log("[mk8dx-8p] m3d8: 0x869a14 da main+%lx tipoScena=%d", (uintptr_t)__builtin_return_address(0) - mk8dx::Base(), rlm::sceneType); m3d8::LogEntries("pre869a14"); }
        Orig(rm);
        uint8_t* sb = mk8dx::Fn<uint8_t* (*)()>(0x86f7d4)();
        if (lg && sb) Logging.Log("[mk8dx-8p] m3d8: dopo 0x869a14 settingsB+8=%d +0x180=%d +0x184=%d", *(int32_t*)(sb + 8), *(int32_t*)(sb + 0x180), *(int32_t*)(sb + 0x184));
    }
};
namespace m3d8 {   /* istantanea delle unita' e del caricatore (solo log) */
    static void DumpUnits(const char* tag) {
        uint8_t* d = Dir(); uint8_t* sys = mk8dx::Fn<uint8_t* (*)()>(0x8ace44)();
        Logging.Log("[mk8dx-8p] %s: sys+5050=%lx +5040=%08x", tag, sys ? *(uint64_t*)(sys + 0x5050) : 0, sys ? *(uint32_t*)(sys + 0x5040) : 0);
        for (int i = 0; d && i < *(int32_t*)(d + 0xc0) && i < 8; i++) { uint8_t* u = Unit(d, i); if (!u) continue;
            Logging.Log("[mk8dx-8p] %s: unit%d stato=%d cambiato=%d attesa=%d sel=%d 361=%d | slot cur=%d", tag, i, *(int32_t*)(u + 0x358), u[0x350], *(int32_t*)(u + 0x354),
                *(int32_t*)(u + 0x34c), u[0x361], sys ? *(int32_t*)(sys + i * 0x20 + 0x170) : -1); }
    }
}
HOOK_DEFINE_TRAMPOLINE(KartDirInitHook) {                  /* 0x13d778 d10303ff: init KartDirector */
    static void Callback(uint8_t* dir) {
        uint8_t* st = m3d8::Settings();
        int32_t n0 = st ? *(int32_t*)(st + 0x180) : -1;
        Logging.Log("[mk8dx-8p] m3d8 KartDirInit %p: settings+8=%d +0x180=%d tipoScena=%d", dir, st ? *(int32_t*)(st + 8) : -1, n0, rlm::sceneType);
        m3d8::LogDesc("init", st);
        { uint8_t* h = m3d8::CurHeap(); Logging.Log("[mk8dx-8p] m3d8: heap corrente %p max allocabile 0x%lx", h, (unsigned long)m3d8::MaxAlloc(h)); }
        /* flusso originale con N = 8 (report/step7b_menu8.md): il gioco arriva qui con settings+0x180 = 8 da solo (locali di default 8,
           limite del conteggio 8). Unica estensione di capacita' durante l'init: l'heap KartWork (costante per 4 unita'). */
        m3d8::init8 = MK8DX_P7_MENU3D8 && st && *(int32_t*)(st + 8) == 5 && n0 == 8;
        if (!m3d8::init8) { Orig(dir); Logging.Log("[mk8dx-8p] m3d8 KartDirInit -> n=%d KartWork libero 0x%lx", *(int32_t*)(dir + 0xc0),
                                                    (unsigned long)m3d8::MaxAlloc(*(uint8_t**)(dir + 0xd0))); return; }
        Orig(dir);
        m3d8::init8 = false;
        uint64_t used = 0; svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
        if (uint8_t* sc = mk8dx::Fn<uint8_t* (*)()>(0x7e7434)()) if (uint8_t* dg = *(uint8_t**)(sc + 0x190 + 4 * 8)) {   /* gestore +0x218 (letture 0x182cf0) */
            uint8_t* o = *(uint8_t**)(dg + 0x218); uint64_t* w = o ? (uint64_t*)(o + 0x40) : nullptr;
            Logging.Log("[mk8dx-8p] m3d8 diag218: dg=%p +218=%p vt=main+%lx +40: %lx %lx %lx %lx %lx %lx %lx %lx", dg, o, o ? *(uintptr_t*)o - mk8dx::Base() : 0,
                w ? w[0] : 0, w ? w[1] : 0, w ? w[2] : 0, w ? w[3] : 0, w ? w[4] : 0, w ? w[5] : 0, w ? w[6] : 0, w ? w[7] : 0);
            for (int off = 0x200; off <= 0x250; off += 8) { uint8_t* g = *(uint8_t**)(dg + off);
                if (g) Logging.Log("[mk8dx-8p] m3d8 diagdg: +%x = %p vt=main+%lx +e0=%d +e4=%d", off, g, *(uintptr_t*)g - mk8dx::Base(), *(int32_t*)(g + 0xe0), *(int32_t*)(g + 0xe4)); }
            uint8_t** a = o ? *(uint8_t***)(o + 0x68) : nullptr;
            for (int i = 0; a && i < 8; i++) Logging.Log("[mk8dx-8p] m3d8 diag218: [%d] = %p", i, a[i]);
        }
        Logging.Log("[mk8dx-8p] m3d8: dopo l'init KartWork libero 0x%lx, heap corrente libero 0x%lx", (unsigned long)m3d8::MaxAlloc(*(uint8_t**)(dir + 0xd0)),
                    (unsigned long)m3d8::MaxAlloc(m3d8::CurHeap()));
        Logging.Log("[mk8dx-8p] m3d8 KartDirInit -> n=%d e9=%d ea=%d heap=%p (KartWork 0x%x) mem=%lu KiB", *(int32_t*)(dir + 0xc0), dir[0xe9], dir[0xea],
                    *(void**)(dir + 0xd0), MK8DX_P7_KARTWORK, (unsigned long)(used >> 10));
    }
};
HOOK_DEFINE_TRAMPOLINE(KartResHook) {                      /* 0x89fc4c f81e0ff3 (kart): 0x8659ec(raceMgr+0x58[raceMgr+0x1cc[kart]]) = 0x884b50(sys, e+0x230);
                                                              nel menu le entry sono 4: per i kart 4..7 e = NULL -> [NULL+0x230] (176 letture, GDB) */
    static void* Callback(int32_t kart) {
        int32_t idx = mk8dx::Fn<int32_t (*)(int32_t)>(0x86e8e4)(kart);
        uint8_t* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)();
        /* prova con l'indice 4..7: init bloccato e letture nulle sparse (eden_log_7b1_res47_fail.txt) -> risorse dello slot kart-4,
           che esistono (prima: slot 0 per [NULL+0x230] = 0) */
        if (rm && kart >= 4 && kart < 8 && (uint32_t)idx >= *(uint32_t*)(rm + 0x50)) {
            /* report/step7b_loader.md H1: dopo PrepareLoaders l'unita' k usa il proprio slot (prima: slot k-4 condiviso col doppio buffer
               di Pk-3 -> modello distrutto al secondo cambio, thread di caricamento morto) */
            if (m3d8::loadersReady) return mk8dx::Fn<void* (*)(void*, int32_t)>(0x884b50)(mk8dx::Fn<void* (*)()>(0x8ace44)(), kart);
            return Orig(kart - 4);
        }
        return Orig(kart);
    }
};
HOOK_DEFINE_TRAMPOLINE(ResReqHook) {                       /* 0x884bd8 fc1a0fe8 (sys, flag, settings, n): risorse per i descrittori < n con tipo 0 */
    static void Callback(void* sys, bool f, void* st, int32_t n) {
        size_t a = m3d8::MaxAlloc(m3d8::CurHeap());
        int32_t m = (m3d8::init8 && n > 4 && !MK8DX_P7_EXP8INIT) ? 4 : n;
        Orig(sys, f, st, m);
        if (m3d8::init8 && f && sys) { m3d8::resSys = (uint8_t*)sys; m3d8::resHeap = m3d8::CurHeap(); m3d8::loadersReady = false; }
        Logging.Log("[mk8dx-8p] m3d8: 0x884bd8 n=%d -> %d, heap corrente 0x%lx -> 0x%lx", n, m, (unsigned long)a, (unsigned long)m3d8::MaxAlloc(m3d8::CurHeap()));
    }
};
HOOK_DEFINE_TRAMPOLINE(ResWaitHook) {                      /* 0x8843a8 f81b0ff9 (sys, i): attende il bit 1<<(i+2) di sys+0x5040 senza limite (R1) */
    static void Callback(void* sys, uint32_t i) { Orig(sys, (m3d8::init8 && i >= 4 && i < 8) ? i - 4 : i); }
};
HOOK_DEFINE_TRAMPOLINE(MenuCharaModelHook) {               /* 0x148038 d100c3ff (obj menu-kart, int* pers): cambio modello del personaggio (diag) */
    static void Callback(uint8_t* o, int32_t* p) {
        if (o && *(int32_t*)(o + 0xc0) >= 4) { static int n; if (n++ < 60) Logging.Log("[mk8dx-8p] m3d8: modello slot %d -> personaggio %d", *(int32_t*)(o + 0xc0), p ? *p : -1); }
        Orig(o, p);
    }
};
HOOK_DEFINE_TRAMPOLINE(DirGroupInitHook) {                /* 0x7e4008 a9bd57f6 (nodo, arg): init dell'albero dei gestori (vt+0x60 sui figli +0x38/+0x40).
                                                              Per il gruppo dg = 0x7e7434()+0x190[4] (KartDirector +0x238, gestori per kart +0x218/+0x240...) nel menu
                                                              settings+0x180 vale 4 per tutti tranne il KartDirector: si forza 8 per l'intero init del gruppo */
    static void Callback(uint8_t* node, void* arg) {
        uint8_t* st = m3d8::Settings(); uint8_t* sc = mk8dx::Fn<uint8_t* (*)()>(0x7e7434)();
        uint8_t* dg = sc ? *(uint8_t**)(sc + 0x190 + 4 * 8) : nullptr;
        bool force = MK8DX_P7_MENU3D8 && node && node == dg && st && *(int32_t*)(st + 8) == 5 && *(int32_t*)(st + 0x180) == 4;
        if (force) *(int32_t*)(st + 0x180) = 8;
        Orig(node, arg);
        if (force) { *(int32_t*)(st + 0x180) = 4; Logging.Log("[mk8dx-8p] m3d8: init del gruppo dg %p con settings+0x180 = 8", node); }
    }
};
namespace m3d8 {   /* init dei gestori per kart del gruppo dg che dimensionano gli array con settings+0x180 (4 nel menu) */
    static bool Force8Begin() { uint8_t* st = Settings();
        bool f = MK8DX_P7_MENU3D8 && st && *(int32_t*)(st + 8) == 5 && *(int32_t*)(st + 0x180) == 4; if (f) *(int32_t*)(st + 0x180) = 8; return f; }
    static void Force8End(bool f, const char* who) { if (!f) return; *(int32_t*)(Settings() + 0x180) = 4;
        static int n; if (n++ < 12) Logging.Log("[mk8dx-8p] m3d8: init %s con settings+0x180 = 8", who); }
}
#define M3D8_FORCE8(N, WHO) HOOK_DEFINE_TRAMPOLINE(N) { static uint64_t Callback(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t g, uint64_t h) { \
    bool x = m3d8::Force8Begin(); uint64_t r = Orig(a, b, c, d, e, f, g, h); m3d8::Force8End(x, WHO); return r; } };
M3D8_FORCE8(KartMgr240InitHook, "dg+0x240 (vt 0x11ad188, 0x2a9c)")   /* 0x2a9c d10443ff: array +0xe0/+0xe8 letto a 0x150f04 (KartUnit 4..7 -> NULL) */
M3D8_FORCE8(KartMgr228InitHook, "dg+0x228 (vt 0x11b8db0, 0xcefb4)")  /* 0xcefb4 a9bd57f6 */
HOOK_DEFINE_TRAMPOLINE(KartMgrInitHook) {                 /* 0x86d918 f81c0ff7 (vt14 dei gestori per kart, base 0x86d4a4): array dimensionati da
                                                              settings+0x180 (0x86d938). Nel menu 4 -> il gestore a dg+0x218 (vt 0x1260dd8, PtrArray<4> a +0x60/+0x68)
                                                              letto dai KartUnit 6..7 a 0x182cf0 oltre la fine (letture nulle intermittenti 0x150c7c) */
    static void Callback(uint8_t* self) {
        uint8_t* st = m3d8::Settings();
        bool force = MK8DX_P7_MENU3D8 && st && *(int32_t*)(st + 8) == 5 && *(int32_t*)(st + 0x180) == 4;
        if (force) *(int32_t*)(st + 0x180) = 8;
        Orig(self);
        if (force) *(int32_t*)(st + 0x180) = 4;
        static int n; if (n++ < 16) Logging.Log("[mk8dx-8p] m3d8: init gestore per kart %p vt=main+%lx %s", self, *(uintptr_t*)self - mk8dx::Base(), force ? "a 8" : "invariato");
    }
};
HOOK_DEFINE_TRAMPOLINE(MenuSchedHook) {                   /* 0x3deee8 d101c3ff (gestore): un caricamento di slot per volta, a giro da this+0x8d4 */
    static void Callback(uint8_t* self) {
        if (m3d8::kickSched && self) { m3d8::kickSched = false; *(int32_t*)(self + 0x8d4) = 4; Logging.Log("[mk8dx-8p] m3d8: scheduler riparte dallo slot 4"); }
        /* lo scheduler scorre tutte le unita' (0x13d540, NULL solo oltre dir+0xc0) e per k = 4..7 legge rm+0x58[rm+0x1cc[k]] senza controllo
           di -1 (0x3df0f8): fuori dalla selezione a 8 (o prima di PrepareLoaders) chiederebbe caricamenti negli slot 4..7 non preparati
           (crash del thread di caricamento anche in 4P, eden_log_7b16_desc4.txt) -> unita' limitate a 4 durante la chiamata */
        uint8_t* d = m3d8::Dir();
        bool lim = d && *(int32_t*)(d + 0xc0) > 4 && !(m3d8::loadersReady && menu8::LocalCount() > 4);
        int32_t n0 = lim ? *(int32_t*)(d + 0xc0) : 0;
        if (lim) *(int32_t*)(d + 0xc0) = 4;
        Orig(self);
        if (lim) *(int32_t*)(d + 0xc0) = n0;
    }
};
HOOK_DEFINE_TRAMPOLINE(MenuPartsResetHook) {              /* 0x490828 d10143ff: azzera personaggio/carrozzeria/ruote/deltaplano degli slot 0..3 (srotolato:
                                                              0x4202f4(s,&0,0,0), 0x420ac0, 0x420b88, 0x420c50). Con 8 locali anche 4..7, come 0..3: le richieste di caricamento
                                                              del menu chiedono solo il personaggio (parti -1 = default), parti vecchie = carrozzeria mai caricata -> NULL */
    static void Callback(uint8_t* self) {
        Orig(self);
        uint8_t* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)();
        { static int k; if (k++ < 20) Logging.Log("[mk8dx-8p] m3d8: 0x490828(%p vt=main+%lx) da main+%lx locali=%d tipoScena=%d", self, self ? *(uintptr_t*)self - mk8dx::Base() : 0,
                                                  (uintptr_t)__builtin_return_address(0) - mk8dx::Base(), rm ? *(int32_t*)(rm + 0x178) : -1, rlm::sceneType); }
        if (!MK8DX_P7_MENU3D8) return;
        if (!rm || *(int32_t*)(rm + 0x178) <= 4) return;
        for (int s = 4; s < 8; s++) {
            int32_t z = 0;
            mk8dx::Fn<void (*)(int32_t, int32_t*, int32_t, int32_t)>(0x4202f4)(s, &z, 0, 0); z = 0;
            mk8dx::Fn<void (*)(int32_t, int32_t*)>(0x420ac0)(s, &z); z = 0;
            mk8dx::Fn<void (*)(int32_t, int32_t*)>(0x420b88)(s, &z); z = 0;
            mk8dx::Fn<void (*)(int32_t, int32_t*)>(0x420c50)(s, &z);
        }
        static int n; if (n++ < 4) Logging.Log("[mk8dx-8p] m3d8: 0x490828 esteso agli slot 4..7 (locali %d)", *(int32_t*)(rm + 0x178));
    }
};
HOOK_DEFINE_TRAMPOLINE(UnitRebuildDiagHook) {             /* 0x16eddc a9bd57f6 (KartUnit): ricostruzione dopo un caricamento (solo log) */
    static void Callback(uint8_t* u) {
        int32_t k = u ? *(int32_t*)(u + 0x40) : -1;
        uint8_t* sys = mk8dx::Fn<uint8_t* (*)()>(0x8ace44)(); uint8_t* rm = mk8dx::Fn<uint8_t* (*)()>(0x8ace28)();
        void* res = (k >= 0 && k < 12) ? mk8dx::Fn<void* (*)(int32_t)>(0x89fc4c)(k) : nullptr;
        int slot = -1, buf = -1;
        for (int i = 0; sys && i < 12 && slot < 0; i++) for (int b = 0; b < 2; b++) if (*(void**)(sys + i * 0x20 + 0x160 + b * 8) == res) { slot = i; buf = b; }
        int32_t idx = (rm && k >= 0 && k < 12) ? *(int32_t*)(rm + 0x1cc + k * 4) : -9;
        uint8_t* e = (rm && idx >= 0 && idx < *(int32_t*)(rm + 0x50)) ? (*(uint8_t***)(rm + 0x58))[idx] : nullptr;
        int32_t* q = e ? (int32_t*)(e + 0x210) : nullptr;
        static int n; if (n++ < 80) Logging.Log("[mk8dx-8p] m3d8 ricostruzione: unit %p kart %d res %p = slot %d buf %d (attivo %d) default=%d | entry %d +230=%d parti %d %d %d pers %d",
            u, k, res, slot, buf, slot >= 0 ? *(int32_t*)(sys + slot * 0x20 + 0x170) : -1, (sys && k >= 0 && k < 12 && res == *(void**)(sys + 0xf8 + k * 8)) ? 1 : 0,
            idx, e ? *(int32_t*)(e + 0x230) : -9, q ? q[0] : -9, q ? q[1] : -9, q ? q[2] : -9, q ? q[3] : -9);
        Orig(u);
    }
};
HOOK_DEFINE_INLINE(KartWorkSizeHook) {                     /* 0x13da28 f9000be8 (dopo il csel 0x13da24): x0 = dimensione heap KartWork */
    static void Callback(exl::hook::InlineCtx* ctx) { if (m3d8::init8) ctx->X[0] = MK8DX_P7_KARTWORK; }
};
HOOK_DEFINE_TRAMPOLINE(MenuKartCfgHook) {                  /* 0x13e59c d10103ff: (dir, n posti, &arg), srotolato su 4 oggetti */
    static void Callback(uint8_t* dir, int32_t n, int32_t* arg) {
        Orig(dir, n, arg);
        int32_t cnt = *(int32_t*)(dir + 0xc0);
        static constexpr int32_t kTipo[4] = { 0, 1, 2, 2 };   /* come 0x13e5d8..0x13e5e8 */
        for (int i = 4; i < cnt && i < 8; i++) {
            uint8_t* o = m3d8::Obj(dir, i); if (!o) continue;
            int32_t t = (n >= 1 && n <= 4) ? kTipo[n - 1] : 2, a = arg ? *arg : 0;
            mk8dx::Fn<void (*)(uint8_t*, int32_t*, int32_t, int32_t*)>(0x147678)(o, &t, i, &a);
        }
        Logging.Log("[mk8dx-8p] m3d8: oggetti menu-kart configurati n=%d modelli=%d locali=%d", n, cnt, menu8::LocalCount());
    }
};
HOOK_DEFINE_INLINE(MenuPresetHook) {                       /* 0x147738 b9403909: x8 = 0x131f450 + k*0x74 (k da {3,4,5,6}[slot] senza limite, 0x14771c) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* o = (uint8_t*)ctx->X[19]; int32_t s = *(int32_t*)(o + 0xc0);
        if (*(int32_t*)(o + 0xc4) != 2 || s < 0 || s >= 8) return;
        if (menu8::LocalCount() > 4) { m3d8::MakePresets(); ctx->X[8] = (uintptr_t)m3d8::preset8[s] - 0x38; }
        else if (s >= 4) ctx->X[8] = mk8dx::Base() + 0x131f450 + (3 + (s & 3)) * 0x74;   /* 1-4P: modelli 4..7 invisibili, preset valido */
    }
};
/* T2 variante A: il viewer dei piedistalli ha 4 voci (v+0x568+i*0x138) e i 6 accessori non controllano l'indice */
#define M3D8_GUARD(N, SIG, ARGS) HOOK_DEFINE_TRAMPOLINE(Vw##N##Hook) { static void Callback SIG { if ((uint32_t)i >= 4) return; Orig ARGS; } };
M3D8_GUARD(Vis,  (uint8_t* v, int32_t i, bool on, int32_t vw), (v, i, on, vw))   /* 0x430cc4 a9bd57f6 */
M3D8_GUARD(Pos,  (uint8_t* v, int32_t i, float* p), (v, i, p))                   /* 0x430d5c 52802708 */
M3D8_GUARD(Rot,  (uint8_t* v, int32_t i, float f), (v, i, f))                    /* 0x430d7c 52802708 */
M3D8_GUARD(Anm,  (uint8_t* v, int32_t i), (v, i))                                /* 0x430d8c f81d0ff5 */
M3D8_GUARD(Set,  (uint8_t* v, int32_t i, int32_t* x), (v, i, x))                 /* 0x430e24 d10083ff */
M3D8_GUARD(Anm2, (uint8_t* v, int32_t i), (v, i))                                /* 0x430e58 f81d0ff5 */
namespace drawreg {
    static volatile bool menu8;   /* anteprime 3D a 8 accese nel menu (m3d8::MenuFrame) */
}
HOOK_DEFINE_TRAMPOLINE(DrawObjHook) {   /* 0x6e2430 fc190fe8 (obj, passi, voci di vista, n voci, R, nodo+0x20, contesto, core): test di vista */
    static void Callback(uint8_t* o, uint32_t pass, uint8_t* ent, uint32_t cnt, uint8_t* R, void* l, void* c, uint32_t core) {
        /* step 7 B (report/step7b_coin.md §9): nel menu a 8 le viste extra 4..7 disegnano solo cio' che ha una vista assegnata (KartUnit,
           piedistalli). Gli oggetti con maschera "tutte le viste" (0xffff, es. il modello moneta globale creato all'avvio da 0x2744b8 -> 0x7f8aac,
           4 viste nell'originale, portato a 8 da ModelCreateHook) restano nelle viste 0..3 come nell'originale. Maschera ridotta solo per il disegno. */
        if (drawreg::menu8 && o && R == proto::R && rlm::sceneType != 3 && *(uint16_t*)(o + 0xc) == 0xffff) {
            *(uint16_t*)(o + 0xc) = 0xff0f; Orig(o, pass, ent, cnt, R, l, c, core); *(uint16_t*)(o + 0xc) = 0xffff; return; }
        Orig(o, pass, ent, cnt, R, l, c, core);
    }
};
namespace m3d8 {
    static void MenuFrame(uint8_t* m) {

        frames++;
        if (MK8DX_LOG) cachediag::Frame();   /* misure delle liste di modelli: solo log */
        if (frames % 600 == 300 && frames < 2000) Logging.Log("[mk8dx-8p] heap: menu f=%lu HeapResident %p allocabile 0x%lx, heap corrente allocabile 0x%lx", frames,
                                                              heapResident, (unsigned long)MaxAlloc(heapResident), (unsigned long)MaxAlloc(CurHeap()));
        uint8_t* d = Dir();
        uint8_t* R = proto::R;
        bool attached = R && proto::layer[0] && *(int32_t*)(R + 0xf0) >= 8 && *(uint8_t**)(proto::layer[0] + 0x1020) == R;
        bool on = On8() && attached;
        uint8_t** sub = *reinterpret_cast<uint8_t***>(m + 0x128);
        if (!sub) return;
        if (MK8DX_SEL8B) sub0 = sub[0];
        if (on != shown) {
            shown = on;
            for (int j = 0; j < 4; j++) if (proto::layer[j]) mk8dx::Fn<proto::EnableFn>(0x6651a8)(proto::layer[j], on);
            if (on) { for (int i = 0; i < 4; i++) if (sub[i]) { memcpy(subRect[i], sub[i] + 0x30, 16); subAsp[i] = *(float*)(*(uint8_t**)(sub[i] + 0x70) + 0xb0); } saved = true; }
            else if (saved) for (int i = 0; i < 4; i++) if (sub[i]) { proto::SetRect(sub[i], subRect[i]); *(float*)(*(uint8_t**)(sub[i] + 0x70) + 0xb0) = subAsp[i]; }
            for (int i = 0; i < 8 && d; i++) if (uint8_t* u = Unit(d, i)) for (int b = 4; b < 8; b++) UnitViewBit(u, b, on && i == b);
            if (uint8_t* v = Viewer()) for (int j = 0; j < 4; j++) for (int b = 4; b < 8; b++) VwVisHook::Orig(v, j, false, b);
            Logging.Log("[mk8dx-8p] m3d8: anteprime 3D a 8 %s (R=%p viste %d, modelli %d)", on ? "ACCESE" : "spente", R, R ? *(int32_t*)(R + 0xf0) : -1,
                        d ? *(int32_t*)(d + 0xc0) : -1);
        }
        drawreg::menu8 = on;
        if (!on) return;
        { static int dumped; if (dumped < 2 && (++lgDump % 300) == 120) { dumped++;   /* diagnosi fantasmi */
            int cnt = *(int32_t*)(R + 0x4228); uint8_t* g0 = *(uint8_t**)(R + 0x4230);
            Logging.Log("[mk8dx-8p] m3d8 diag: R+0xf0=%d gruppi=%d g0=%p", *(int32_t*)(R + 0xf0), cnt, g0);
            for (int i = 0; g0 && i < cnt && i < 8; i++) Logging.Log("[mk8dx-8p] m3d8 diag: gruppo %d +408=%02x +409=%02x +40a=%02x +40b=%02x +40c=%08x", i,
                g0[i * 0x410 + 0x408], g0[i * 0x410 + 0x409], g0[i * 0x410 + 0x40a], g0[i * 0x410 + 0x40b], *(uint32_t*)(g0 + i * 0x410 + 0x40c));
            for (int k = 3; k < 5; k++) { uint8_t* ly = k < 4 ? sub[k] : proto::layer[k - 4]; if (!ly) continue;   /* campi non puntatore */
                for (int o = 0x78; o < 0x240; o += 0x20) { uint32_t* w = (uint32_t*)(ly + o);
                    Logging.Log("[mk8dx-8p] m3d8 diag: L%d +%03x: %08x %08x %08x %08x %08x %08x %08x %08x", k, o, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]); }
                for (int o = 0x400; o < 0x440; o += 0x20) { uint32_t* w = (uint32_t*)(ly + o);
                    Logging.Log("[mk8dx-8p] m3d8 diag: L%d +%03x: %08x %08x %08x %08x %08x %08x %08x %08x", k, o, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]); }
                for (int o = 0xdc0; o < 0xe00; o += 0x20) { uint32_t* w = (uint32_t*)(ly + o);
                    Logging.Log("[mk8dx-8p] m3d8 diag: L%d +%03x: %08x %08x %08x %08x %08x %08x %08x %08x", k, o, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]); } }
            for (int k = 0; k < 8; k++) { uint8_t* ly = k < 4 ? sub[k] : proto::layer[k - 4]; if (!ly) continue;
                uint32_t* w = (uint32_t*)(ly + 0x1070);
                Logging.Log("[mk8dx-8p] m3d8 diag: layer %d %p +84=%d +88=%d +1070: %08x %08x %08x %08x %08x %08x %08x %08x", k, ly, *(int32_t*)(ly + 0x84),
                    *(int32_t*)(ly + 0x88), w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]); }
        } }
        float* full = reinterpret_cast<float*>(m + 0xa38 + 16);
        float sc = *(float*)(m + 0xbdc), W = full[2] * sc, H = full[3] * sc, r[4];
        for (int i = 0; i < 4; i++) if (sub[i]) { Cell(i, W, H, r); proto::SetRect(sub[i], r); }
        uint8_t* c0 = *(uint8_t**)(sub[0] + 0x68); uint8_t* p0 = *(uint8_t**)(sub[0] + 0x70);
        uint8_t* v = Viewer();
        static unsigned long lg;
        /* ogni frame: un cambio di personaggio ricrea il modello con la maschera di default (bit 4..7 accesi) */
        for (int i = 0; i < 8; i++) if (uint8_t* u = Unit(d, i)) for (int b = 4; b < 8; b++) UnitViewBit(u, b, i == b);
        for (int j = 0; j < 4; j++) {
            uint8_t* o = Obj(d, 4 + j);
            uint8_t* c = proto::cam[j]; uint8_t* pj = proto::proj[j];
            if (!o || !c || !pj || !c0 || !p0) continue;
            memcpy(pj, p0, 0xc0); memcpy(c, c0, 0x60);
            memcpy(c + 0x38, o + 0x8c, 12); memcpy(c + 0x44, o + 0x98, 12); memcpy(c + 0x50, o + 0xa4, 12);
            float* up = (float*)(c + 0x50); float l = up[0] * up[0] + up[1] * up[1] + up[2] * up[2];
            if (l > 0) { l = 1.0f / __builtin_sqrtf(l); up[0] *= l; up[1] *= l; up[2] *= l; }
            mk8dx::Fn<void (*)(void*, float)>(0x61bdd0)(pj, *(float*)(o + 0xb0));
            *(float*)(pj + 0xb4) = *(float*)(o + 0xb4); *(float*)(pj + 0xb8) = *(float*)(o + 0xb8);
            Cell(4 + j, W, H, r); proto::SetRect(proto::layer[j], r);
            /* fantasmi (residuo delle viste disegnate prima, prova T1 00:20): l'unica differenza fra Sub3 e Sub4 e' layer+0xde4 bit 0x4
               (0x124 contro 0x120), "flag vista R" di 0x3cd194 da mgr+0x9d6 */
            { uint16_t* de = (uint16_t*)(proto::layer[j] + 0xde4); *de = (*de & ~0x4) | (*(uint16_t*)(sub[0] + 0xde4) & 0x4); }
            mk8dx::Fn<proto::UpdMtxFn>(0x619a08)(c, c + 8);
            if (v) for (int b = 4; b < 8; b++) VwVisHook::Orig(v, j, b == 4 + j && o[0xd4] != 0, b);   /* piedistallo j anche nella vista 4+j (variante A) */
            if ((lg % 600) == 0)
                Logging.Log("[mk8dx-8p] m3d8: vista %d obj=%p slot=%d tipo=%d vis=%d eye=%.1f,%.1f,%.1f fovy=%.3f sh=%.3f,%.3f rect=%.0f,%.0f,%.0f,%.0f | sub%d sh=%.3f,%.3f fovy=%.3f",
                    4 + j, o, *(int32_t*)(o + 0xc0), *(int32_t*)(o + 0xc4), o[0xd4], *(float*)(o + 0x8c), *(float*)(o + 0x90), *(float*)(o + 0x94),
                    *(float*)(o + 0xb0), *(float*)(o + 0xb4), *(float*)(o + 0xb8), r[0], r[1], r[2], r[3], j,
                    *(float*)(*(uint8_t**)(sub[j] + 0x70) + 0xb4), *(float*)(*(uint8_t**)(sub[j] + 0x70) + 0xb8), *(float*)(*(uint8_t**)(sub[j] + 0x70) + 0xa0));
        }
        lg++;
    }
}
namespace p5 {
    struct Set { uint8_t* p3a8; int32_t n39c; uint8_t arr[0x40]; uint8_t okw[0x10]; int32_t c108; uint8_t* p110; uint8_t* cur; uint8_t o178[0x18]; };
    static Set A, B;
    static uint8_t* Obj178(uint8_t* pg) { uint8_t** q = *(uint8_t***)(pg + 0x178); return q ? *q : nullptr; }   /* come 0x4c03e4 */
    static void Save(uint8_t* pg, Set& s) {
        s.p3a8 = *(uint8_t**)(pg + 0x3a8); s.n39c = *(int32_t*)(pg + 0x39c);
        memcpy(s.arr, pg + 0x3c0, 0x40); memcpy(s.okw, pg + 0x450, 0x10);
        s.c108 = *(int32_t*)(pg + 0x108); s.p110 = *(uint8_t**)(pg + 0x110); s.cur = *(uint8_t**)(pg + 0x1b0);
        if (uint8_t* o = Obj178(pg)) memcpy(s.o178, o, 0x18);
    }
    static void Load(uint8_t* pg, const Set& s) {
        *(uint8_t**)(pg + 0x3a8) = s.p3a8; *(int32_t*)(pg + 0x39c) = s.n39c;
        memcpy(pg + 0x3c0, s.arr, 0x40); memcpy(pg + 0x450, s.okw, 0x10);
        *(int32_t*)(pg + 0x108) = s.c108; *(uint8_t**)(pg + 0x110) = s.p110; *(uint8_t**)(pg + 0x1b0) = s.cur;
        if (uint8_t* o = Obj178(pg)) memcpy(o, s.o178, 0x18);
    }
    static void BuildB(uint8_t* pg) {
        uint8_t* nm = p4::Names();
        if (!on || !pg || built == pg) return;
        if (!nm || *(int32_t*)(pg + 0xe8) != 0x31 || *(int32_t*)(pg + 0x3c0) != 4 || !Obj178(pg)) {
            Logging.Log("[mk8dx-8p] p5: set B non costruito: kart=%p id=%x nomi=%p +0x3c0=%d obj178=%p", pg, *(int32_t*)(pg + 0xe8), nm,
                        *(int32_t*)(pg + 0x3c0), Obj178(pg));
            return;
        }
        uint8_t* o = Obj178(pg);
        Save(pg, A);
        int32_t n198 = *(int32_t*)(pg + 0x198);
        *(uint8_t**)(pg + 0x3a8) = nm; *(int32_t*)(pg + 0x39c) = 8;
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x8bafb8)(pg, 32);                 /* cursore B: 8 slot x 4 item (come 0x4c4ed0) */
        *(uint64_t*)(o + 8) = 0; *(int32_t*)o = 0; *(int32_t*)(o + 0x10) = 0;  /* 0x4c03e4 non libera il buffer A */
        building = true; mk8dx::Fn<void (*)(uint8_t*)>(0x4bfdd0)(pg); building = false;
        *(int32_t*)(pg + 0x198) = n198;                                         /* 0x8bb2b8 ha riaggiunto +0x420 */
        for (int k = 0; k < 8; k++) { *(int32_t*)(pg + 0x4a0 + 8 * k) = 0; *(int32_t*)(pg + 0x4a4 + 8 * k) = 0; }
        Save(pg, B); Load(pg, A); built = pg; isB = false;
        Logging.Log("[mk8dx-8p] p5: set B kart costruito: slot=%d sel=%d okw=%d cur=%p (A cur=%p)", *(int32_t*)(B.arr + 0),
                    *(int32_t*)(B.arr + 0x10), *(int32_t*)B.okw, B.cur, A.cur);
    }
    static void Use(uint8_t* pg, bool b) {
        if (built != pg || b == isB) return;
        Save(pg, isB ? B : A); Load(pg, b ? B : A); isB = b;
        Logging.Log("[mk8dx-8p] p5: kart set %c (+0x39c=%d)", b ? 'B' : 'A', *(int32_t*)(pg + 0x39c));
    }
    static uint8_t* PcCtl(int i) { uint8_t* pc = cur8::GfxPage() ? mk8dx::Fn<uint8_t* (*)()>(0x8bf2c8)() : nullptr;
                                   return pc ? *(uint8_t**)(pc + 0x1d0 + (i < 4 ? i : i + 4) * 8) : nullptr; }
    static void Hide3D() { if (m3d8::N8()) { m3d8::Stamp(); return; }   /* step 7 B: con 8 KartUnit le anteprime restano */
                           if (MK8DX_P5_HIDE3D) { for (int i = 0; i < 4; i++) mk8dx::Fn<void (*)(int, bool)>(0x14a7ac)(i, false);
                                                   m3d8::HidePedestals(); } }   /* step 7 B tappa 0: anche i BaseTable */
    static uint8_t* ChangeGrp(uint8_t* pg) {                 /* gruppo 0 della parte radice: come 0x4bb538..0x4bb544 */
        uint8_t* part = pg ? *(uint8_t**)(pg + 0x1d0) : nullptr; uint8_t** a = part ? *(uint8_t***)(part + 0x70) : nullptr;
        return a ? a[0] : nullptr;
    }
    static void Change(uint8_t* pg, float v) { if (uint8_t* g = ChangeGrp(pg)) mk8dx::Fn<void (*)(uint8_t*, int, float)>(0x8ad300)(g, 0, v); }
    static const C kTexQueue[] = {               /* p5fix2: coda texture tc_KP (64 = 4x16) -> 128, heap UIHeap_PartsTex 0x6d0000 -> 0xda0000 */
        {0x40c62c, 0x52a00da4, 0x52a01b44}, {0x40c630, 0x321a03e2, 0x321903e2},
    };
    static const C kFix[] = {                    /* p5fix: clamp del selettore parti (sel+0x1e8 = slot) e cursore grafico finestra varianti */
        {0x476628, 0x7100113f, 0x7100213f}, {0x477400, 0x7100113f, 0x7100213f}, {0x477e38, 0x7100113f, 0x7100213f},
        {0x46c100, 0x7100127f, 0x7100227f},
    };
}
HOOK_DEFINE_INLINE(KartBuildCountHook) {               /* 0x4bfdf4 mov x19,x0: W1 = conteggio di 0x8bb0ac(+0x108/+0x110) */
    static void Callback(exl::hook::InlineCtx* ctx) { if (p5::building) ctx->W[1] = 8; }
};
HOOK_DEFINE_INLINE(P5SlotFixPage) {                    /* 0x4c192c: x8 = cur+i*0x20, cur = [x19+0x1b0] */
    static void Callback(exl::hook::InlineCtx* ctx) { uint8_t* c = *(uint8_t**)(ctx->X[19] + 0x1b0);
        if (cur8::Wide(c) && ctx->X[8] - (uintptr_t)c >= 0x80) ctx->X[8] += 0x80; }
};
P4_SLOTFIX(Ev, 10, 9)                                  /* 0x4c3dd0 */
HOOK_DEFINE_INLINE(PcCreateHook) {                     /* 0x49ad98: [x19, x26] con x26 = x22*8 -> slot 4..7 a +0x210.. */
    static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->X[22] >= 0x3e) ctx->X[26] += 0x20; }   /* i=4..7: 0x1f0.. -> 0x210.. (V = i+4) */
};
HOOK_DEFINE_INLINE(PcIndexHook) {                      /* 0x49ae34 add x8,x0,x8,lsl #3 (cmp 0x49ae2c a #8) */
    static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->W[1] - 4u < 4) ctx->X[8] = ctx->W[1] + 4; }
};
#define P5_PCFIX(N, I) HOOK_DEFINE_INLINE(P5PcFix##N) { static void Callback(exl::hook::InlineCtx* ctx) { \
    if (ctx->X[I] - 4 < 4) ctx->X[0] += 0x20; } };
P5_PCFIX(Cancel, 22) /* 0x4c2ba4 */  P5_PCFIX(Confirm, 21) /* 0x4c2fa0 */
/* ---- step 7f (report/step7f_sblocco_kart.md): pagina Present (0x26 personaggio / 0x27 parti) a 8 regali ------------------------
 * mn_Page_Present_Parts della RomFS 4.0.1 ha 8 parti Gift (3.0.5: 4); il main Switch 1 cicla sulle 8 ma crea i controlli solo per i < 4
 * e nasconde 4..7 (0x4b0ba8). Flusso originale con N = 8: array per regalo [4] -> [8] in coda all'oggetto (0x368 -> 0x5f0):
 *   flag +0x20a[4] -> +0x368[8], controlli +0x218 -> +0x370, riquadri +0x238 -> +0x3b0, effetti Unlock (4 coppie a passo 0x10)
 *   +0x268/+0x2a8/+0x2e8/+0x328 -> +0x3f0/+0x470/+0x4f0/+0x570; conteggi `orr #4` e limiti `cmp #4` -> 8; scrittori esterni: kart 0x4c2658/
 *   0x4c26cc/0x4c2748 (+0x238 per +0x39c pannelli) e regalo personaggio 0x4b8a00. Parti cicli srotolati a 4 -> hook (flag, focus, 0x14b824). */
#ifndef MK8DX_GIFT8
#define MK8DX_GIFT8 1   /* 0 = pagina Present originale a 4 regali (solo per bisezioni) */
#endif
namespace gift8 {
    struct C { uint32_t off, orig, repl; };
    static bool on;
    static constexpr int kFlags = 0x368;
    static const C kTable[] = {     /* 98 parole generate e verificate su main.bin (step7f_data/gen7f.py) */
        {0x409e38, 0x52806d00, 0x5280be00},   /* dimensione 0x368->0x5f0  mov	w0, #0x368              // =872 */
        {0x40a174, 0x52806d00, 0x5280be00},   /* dimensione 0x368->0x5f0  mov	w0, #0x368              // =872 */
        {0x4b0a84, 0x321e03e8, 0x321d03e8},   /* conteggio 4->8           orr	w8, wzr, #0x4 */
        {0x4b0ba8, 0xf10012ff, 0xf10022ff},   /* limite 4->8              cmp	x23, #0x4 */
        {0x4b0c38, 0xf10012ff, 0xf10022ff},   /* limite 4->8              cmp	x23, #0x4 */
        {0x4b0c50, 0xf9010d1c, 0xf901b91c},   /* campo                    str	x28, [x8, #0x218] */
        {0x4b0c90, 0xf9013798, 0xf901fb98},   /* campo                    str	x24, [x28, #0x268] */
        {0x4b0c98, 0xf9413788, 0xf941fb88},   /* campo                    ldr	x8, [x28, #0x268] */
        {0x4b0ce0, 0xf9013b98, 0xf901ff98},   /* campo                    str	x24, [x28, #0x270] */
        {0x4b0ce8, 0xf9413b88, 0xf941ff88},   /* campo                    ldr	x8, [x28, #0x270] */
        {0x4b0d04, 0xf9015798, 0xf9023b98},   /* campo                    str	x24, [x28, #0x2a8] */
        {0x4b0d0c, 0xf9413781, 0xf941fb81},   /* campo                    ldr	x1, [x28, #0x268] */
        {0x4b0d2c, 0xf9413b81, 0xf941ff81},   /* campo                    ldr	x1, [x28, #0x270] */
        {0x4b0d38, 0xf9015b98, 0xf9023f98},   /* campo                    str	x24, [x28, #0x2b0] */
        {0x4b0d54, 0xf9017798, 0xf9027b98},   /* campo                    str	x24, [x28, #0x2e8] */
        {0x4b0d5c, 0xf9413781, 0xf941fb81},   /* campo                    ldr	x1, [x28, #0x268] */
        {0x4b0d7c, 0xf9413b81, 0xf941ff81},   /* campo                    ldr	x1, [x28, #0x270] */
        {0x4b0d88, 0xf9017b98, 0xf9027f98},   /* campo                    str	x24, [x28, #0x2f0] */
        {0x4b0da0, 0xf9413781, 0xf941fb81},   /* campo                    ldr	x1, [x28, #0x268] */
        {0x4b0db4, 0xf9019798, 0xf902bb98},   /* campo                    str	x24, [x28, #0x328] */
        {0x4b0dcc, 0xf9413b81, 0xf941ff81},   /* campo                    ldr	x1, [x28, #0x270] */
        {0x4b0dd8, 0xf9019b98, 0xf902bf98},   /* campo                    str	x24, [x28, #0x330] */
        {0x4b0f8c, 0x91082e68, 0x910da668},   /* campo                    add	x8, x19, #0x20b */
        {0x4b0f90, 0x39082a74, 0x390da274},   /* campo                    strb	w20, [x19, #0x20a] */
        {0x4b0f94, 0x3908367f, 0x390dae7f},   /* campo                    strb	wzr, [x19, #0x20d] */
        {0x4b1154, 0x39082a69, 0x390da269},   /* campo                    strb	w9, [x19, #0x20a] */
        {0x4b1180, 0x39082e69, 0x390da669},   /* campo                    strb	w9, [x19, #0x20b] */
        {0x4b11ac, 0x39083269, 0x390daa69},   /* campo                    strb	w9, [x19, #0x20c] */
        {0x4b11d8, 0x39083668, 0x390dae68},   /* campo                    strb	w8, [x19, #0x20d] */
        {0x4b11e4, 0x321e03e8, 0x321d03e8},   /* conteggio 4->8           orr	w8, wzr, #0x4 */
        {0x4b11f4, 0xf100129f, 0xf100229f},   /* limite 4->8              cmp	x20, #0x4 */
        {0x4b1200, 0x39482908, 0x394da108},   /* campo                    ldrb	w8, [x8, #0x20a] */
        {0x4b1218, 0xf9410f08, 0xf941bb08},   /* campo                    ldr	x8, [x24, #0x218] */
        {0x4b1228, 0xf9410f08, 0xf941bb08},   /* campo                    ldr	x8, [x24, #0x218] */
        {0x4b1234, 0xf94156e0, 0xf9423ae0},   /* campo                    ldr	x0, [x23, #0x2a8] */
        {0x4b1244, 0xf9415ae0, 0xf9423ee0},   /* campo                    ldr	x0, [x23, #0x2b0] */
        {0x4b125c, 0xf9410ee8, 0xf941bae8},   /* campo                    ldr	x8, [x23, #0x218] */
        {0x4b126c, 0xf9410ee8, 0xf941bae8},   /* campo                    ldr	x8, [x23, #0x218] */
        {0x4b1368, 0x7100129f, 0x7100229f},   /* limite 4->8              cmp	w20, #0x4 */
        {0x4b1374, 0xf9410d00, 0xf941b900},   /* campo                    ldr	x0, [x8, #0x218] */
        {0x4b14e8, 0xf9410e60, 0xf941ba60},   /* campo                    ldr	x0, [x19, #0x218] */
        {0x4b1718, 0x321e03e8, 0x321d03e8},   /* conteggio 4->8           orr	w8, wzr, #0x4 */
        {0x4b1734, 0xf10012bf, 0xf10022bf},   /* limite 4->8              cmp	x21, #0x4 */
        {0x4b1740, 0x39482908, 0x394da108},   /* campo                    ldrb	w8, [x8, #0x20a] */
        {0x4b1750, 0xf9411f40, 0xf941db40},   /* campo                    ldr	x0, [x26, #0x238] */
        {0x4b176c, 0xf9411f40, 0xf941db40},   /* campo                    ldr	x0, [x26, #0x238] */
        {0x4b1774, 0xf9411f48, 0xf941db48},   /* campo                    ldr	x8, [x26, #0x238] */
        {0x4b1784, 0xf9410f48, 0xf941bb48},   /* campo                    ldr	x8, [x26, #0x218] */
        {0x4b1798, 0xf9410f48, 0xf941bb48},   /* campo                    ldr	x8, [x26, #0x218] */
        {0x4b17ac, 0xf9415680, 0xf9423a80},   /* campo                    ldr	x0, [x20, #0x2a8] */
        {0x4b17b8, 0x910aa288, 0x9111c288},   /* campo                    add	x8, x20, #0x2a8 */
        {0x4b1808, 0xf9415a80, 0xf9423e80},   /* campo                    ldr	x0, [x20, #0x2b0] */
        {0x4b1814, 0x910ac288, 0x9111e288},   /* campo                    add	x8, x20, #0x2b0 */
        {0x4b1864, 0xf9413680, 0xf941fa80},   /* campo                    ldr	x0, [x20, #0x268] */
        {0x4b1874, 0xf9413a80, 0xf941fe80},   /* campo                    ldr	x0, [x20, #0x270] */
        {0x4b19b0, 0x321e03e8, 0x321d03e8},   /* conteggio 4->8           orr	w8, wzr, #0x4 */
        {0x4b19c4, 0xf100129f, 0xf100229f},   /* limite 4->8              cmp	x20, #0x4 */
        {0x4b19d0, 0x39482908, 0x394da108},   /* campo                    ldrb	w8, [x8, #0x20a] */
        {0x4b19e8, 0xf9410ee0, 0xf941bae0},   /* campo                    ldr	x0, [x23, #0x218] */
        {0x4b19f0, 0xf9410ee8, 0xf941bae8},   /* campo                    ldr	x8, [x23, #0x218] */
        {0x4b1a00, 0xf94156d9, 0xf9423ad9},   /* campo                    ldr	x25, [x22, #0x2a8] */
        {0x4b1aa8, 0xf9415ad9, 0xf9423ed9},   /* campo                    ldr	x25, [x22, #0x2b0] */
        {0x4b1b54, 0xf94176c0, 0xf9427ac0},   /* campo                    ldr	x0, [x22, #0x2e8] */
        {0x4b1b64, 0xf94176c9, 0xf9427ac9},   /* campo                    ldr	x9, [x22, #0x2e8] */
        {0x4b1bac, 0xf9417ac0, 0xf9427ec0},   /* campo                    ldr	x0, [x22, #0x2f0] */
        {0x4b1bbc, 0xf9417ac9, 0xf9427ec9},   /* campo                    ldr	x9, [x22, #0x2f0] */
        {0x4b1c04, 0xf94196c0, 0xf942bac0},   /* campo                    ldr	x0, [x22, #0x328] */
        {0x4b1c14, 0xf94196c9, 0xf942bac9},   /* campo                    ldr	x9, [x22, #0x328] */
        {0x4b1c5c, 0xf9419ac0, 0xf942bec0},   /* campo                    ldr	x0, [x22, #0x330] */
        {0x4b1c6c, 0xf9419ac9, 0xf942bec9},   /* campo                    ldr	x9, [x22, #0x330] */
        {0x4b1cc0, 0x39482a68, 0x394da268},   /* campo                    ldrb	w8, [x19, #0x20a] */
        {0x4b1cd0, 0x39482e68, 0x394da668},   /* campo                    ldrb	w8, [x19, #0x20b] */
        {0x4b1ce0, 0x39483268, 0x394daa68},   /* campo                    ldrb	w8, [x19, #0x20c] */
        {0x4b1cf0, 0x39483668, 0x394dae68},   /* campo                    ldrb	w8, [x19, #0x20d] */
        {0x4b21a8, 0x7100129f, 0x7100229f},   /* limite 4->8              cmp	w20, #0x4 */
        {0x4b21b4, 0xf9410d00, 0xf941b900},   /* campo                    ldr	x0, [x8, #0x218] */
        {0x4b2288, 0x321e03e8, 0x321d03e8},   /* conteggio 4->8           orr	w8, wzr, #0x4 */
        {0x4b22a0, 0xf10012ff, 0xf10022ff},   /* limite 4->8              cmp	x23, #0x4 */
        {0x4b22ac, 0xf9410d00, 0xf941b900},   /* campo                    ldr	x0, [x8, #0x218] */
        {0x4b22b8, 0xf9419795, 0xf942bb95},   /* campo                    ldr	x21, [x28, #0x328] */
        {0x4b233c, 0xf9419b95, 0xf942bf95},   /* campo                    ldr	x21, [x28, #0x330] */
        {0x4b2448, 0x321e03e8, 0x321d03e8},   /* conteggio 4->8           orr	w8, wzr, #0x4 */
        {0x4b2464, 0xf10012df, 0xf10022df},   /* limite 4->8              cmp	x22, #0x4 */
        {0x4b2470, 0xf941577c, 0xf9423b7c},   /* campo                    ldr	x28, [x27, #0x2a8] */
        {0x4b24f0, 0xf9415b7c, 0xf9423f7c},   /* campo                    ldr	x28, [x27, #0x2b0] */
        {0x4b2570, 0xf941777c, 0xf9427b7c},   /* campo                    ldr	x28, [x27, #0x2e8] */
        {0x4b25f0, 0xf9417b7c, 0xf9427f7c},   /* campo                    ldr	x28, [x27, #0x2f0] */
        {0x4b2670, 0xf941977c, 0xf942bb7c},   /* campo                    ldr	x28, [x27, #0x328] */
        {0x4b26f0, 0xf9419b7c, 0xf942bf7c},   /* campo                    ldr	x28, [x27, #0x330] */
        {0x4b2770, 0xf9413760, 0xf941fb60},   /* campo                    ldr	x0, [x27, #0x268] */
        {0x4b2780, 0xf9413b60, 0xf941ff60},   /* campo                    ldr	x0, [x27, #0x270] */
        {0x4b8a00, 0xf9011d09, 0xf901d909},   /* campo                    str	x9, [x8, #0x238] */
        {0x4c2658, 0xf100117f, 0xf100217f},   /* limite 4->8              cmp	x11, #0x4 */
        {0x4c266c, 0xf9011dac, 0xf901d9ac},   /* campo                    str	x12, [x13, #0x238] */
        {0x4c26cc, 0xf100117f, 0xf100217f},   /* limite 4->8              cmp	x11, #0x4 */
        {0x4c26e0, 0xf9011dac, 0xf901d9ac},   /* campo                    str	x12, [x13, #0x238] */
        {0x4c2748, 0xf100117f, 0xf100217f},   /* limite 4->8              cmp	x11, #0x4 */
        {0x4c275c, 0xf9011dac, 0xf901d9ac},   /* campo                    str	x12, [x13, #0x238] */
    };
}
HOOK_DEFINE_TRAMPOLINE(Gift8EnterHook) {               /* 0x4b0e20 onEnter Present (vt+0x258): flag 4..7 a 0 (ramo 0x3fe658 li lascia stare) */
    static void Callback(uint8_t* pg) { memset(pg + gift8::kFlags + 4, 0, 4); Orig(pg); }
};
HOOK_DEFINE_INLINE(Gift8FlagsHook) {                   /* 0x4b11b0 (x8 = selezione, x19 = Present): flag 4..7 come 0x4b111c..0x4b11d8 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* sel = (uint8_t*)ctx->X[8]; uint8_t* pg = (uint8_t*)ctx->X[19]; int n = menu8::LocalCount();
        for (int i = 4; i < 8; i++) {                  /* stato per slot +0xa28[8] (step 5b passo 1); solo slot dei giocatori presenti */
            int32_t st = i < n ? *(int32_t*)(sel + sel8::slotOff + 8 * i) : 0;
            pg[gift8::kFlags + i] = st != 0 && st != 1 && st != 4;
        }
    }
};
HOOK_DEFINE_INLINE(Gift8FocusHook) {                   /* 0x4b140c (x20 = mgr UI, x19 = Present): 0x8bd5f8(mgr, p, pagina) anche per p = 4..N-1 */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!cur8::wideCur) return;                    /* senza cursore a 8 il setFocus originale ridurrebbe p >= 4 al pad 0 */
        int n = menu8::LocalCount(); if (n > 8) n = 8;
        for (int p = 4; p < n; p++) mk8dx::Fn<void (*)(uint8_t*, int, uint8_t*)>(0x8bd5f8)((uint8_t*)ctx->X[20], p, (uint8_t*)ctx->X[19]);
    }
};
HOOK_DEFINE_INLINE(Gift8KartHook) {                    /* 0x4b1d00 (x19 = Present): 0x14b824(i) per i = 4..7 col regalo, come 0x4b1cc0..0x4b1cfc */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* pg = (uint8_t*)ctx->X[19];
        for (int i = 4; i < 8; i++) if (pg[gift8::kFlags + i]) mk8dx::Fn<void (*)(int)>(0x14b824)(i);
    }
};
HOOK_DEFINE_TRAMPOLINE(KartCreateHook) {               /* 0x4c4c84 onCreate kart Multi */
    static void Callback(uint8_t* pg) { Orig(pg); p5::BuildB(pg); }
};
HOOK_DEFINE_TRAMPOLINE(Model3DModeHook) {              /* 0x14a038(model, a, b) -> 0x148b50 legge [x0+0xd7] */
    static void Callback(uint8_t* m, bool a, bool b) { if (m) Orig(m, a, b); }
};
HOOK_DEFINE_TRAMPOLINE(KartExitHook) {                 /* 0x4c3564 vt+0x290 */
    static void Callback(uint8_t* pg) {
        uint8_t* l = *(uint8_t**)(pg + 0x168); int32_t dest = l ? *(int32_t*)(l + 0xc) : -1;
        bool b = p5::isB && p5::built == pg;
        Orig(pg);
        if (!b) return;
        for (int i = 4; i < 8; i++) { if (uint8_t* c = p5::PcCtl(i)) *(uint8_t**)(c + 0xb8) = nullptr;
            uint8_t* pc = mk8dx::Fn<uint8_t* (*)()>(0x8bf2c8)(); if (pc) mk8dx::Fn<void (*)(uint8_t*, int, bool)>(0x49ae1c)(pc, i, false); }
        cur8::SetWide(pg, false, 0);
        if ((dest < 0x2c || dest > 0x2e) && !sel8b::isB) {                     /* avanti: Orig ha chiuso +0x3a8 (= 0x12) e +0x3b0 */
            int32_t id = 0x16; mk8dx::Fn<void (*)(int32_t*)>(0x8b258c)(&id);
            p4::namesOpen = false; p4::MultiNames(*(uint8_t**)(pg + 0x3b8), true);
        }
        Logging.Log("[mk8dx-8p] p5: uscita kart B verso 0x%x", dest);
    }
};
HOOK_DEFINE_TRAMPOLINE(KartPreExitHook) {                /* 0x4c3360 kart vt+0x280: inizio uscita (Change -1 su +0x3a8) */
    static void Callback(uint8_t* pg) {
        Orig(pg);
        if (!p5::isB || p5::built != pg) return;
        uint8_t* l = *(uint8_t**)(pg + 0x168); int32_t dest = l ? *(int32_t*)(l + 0xc) : -1;   /* come 0x8ba5a0 */
        if (dest >= 0x2c && dest <= 0x2e && !sel8b::isB) p5::Change(p5::A.p3a8, -1.0f);      /* anche la 0x16 torna indietro (con il set B
                                                                                                 della selezione la 0x16 non e' aperta) */
    }
};
HOOK_DEFINE_INLINE(CwGfxCursorHook) {                  /* 0x46c10c add x9,x20,x9,lsl #3: cursore grafico della finestra varianti, 4..7 -> +0x210.. */
    static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->X[9] - 4 < 4) ctx->X[9] += 4; }
};
HOOK_DEFINE_INLINE(CwCellGfxHook) {                    /* 0x4553fc / 0x455474 in 0x4552b8(parte, p): cm_Page_Cursor+0x1d0[clamp4(p)] -> 04..07 per p 4..7 */
    static void Callback(exl::hook::InlineCtx* ctx) { if (ctx->W[20] - 4u < 4) ctx->X[8] = ctx->W[20] + 4; }
};
HOOK_DEFINE_TRAMPOLINE(KpLoopHook) {                   /* 0x49ae90 (opzionale): Loop dei KPSlotCursor 4..7 */
    static void Callback(uint8_t* pc) {
        Orig(pc); if (!p5::isB) return;
        for (int k = 0; k < 4; k++) { uint8_t* c = *(uint8_t**)(pc + 0x210 + 8 * k);
            if (c && mk8dx::Fn<bool (*)(uint8_t*)>(0x8adf84)(c + 0x30)) mk8dx::Fn<void (*)(uint8_t*, float)>(0x44375c)(c, *(float*)(pc + 0x1f0)); }
    }
};

/* ---- step 5 S5.4: selezione a due turni (report/step5_plan.md §1, step5_plan_A2.md) --------------------------------
 * Il framework dei cursori e' a 4 giocatori: turno 1 = locali 0..3 con i pad UI 0..3; quando tutti e 4 confermano e
 * N>4, il modulo riporta gli slot 0..3 a "sceglie" (come l'annulla 0x4ba71c) e rimappa 0x8bf158(0..3) sui pad extra
 * (padui::round2): i giocatori 5-8 scelgono con i loro controller e il gioco scrive il race-config dei locali 4..7
 * tramite la catena slot -> pad -> 0x86ebf8(idx) -> entry. Stessa cosa nella pagina kart. */
namespace sel8 {
    static int round;            /* selezione personaggi: 0 = non attiva, 1, 2 */
    static int kartRound;        /* pagina kart: 0, 1, 2 */
    inline int32_t* SlotState(uint8_t* page, int i) { return reinterpret_cast<int32_t*>(page + slotOff + 8 * i); }
    inline bool TwoRounds() { return padui::active8 && menu8::LocalCount() > 4; }
    static uint8_t* SettingsB() { return mk8dx::Fn<uint8_t* (*)()>(0x86f7d4)(); }
    /* log di accettazione: entry, personaggio/parti/tipo e descrittori settingsB (A2 §4.4) */
    static void LogAssigned(const char* tag) {
        uint8_t* rm = race8::RaceMgr(); uint8_t* sb = SettingsB();
        int n = menu8::LocalCount();
        int cnt = *(int32_t*)(rm + 0x50); uint8_t** ent = *(uint8_t***)(rm + 0x58);
        Logging.Log("[mk8dx-8p] A2 %s: N=%d race+0x178=%d sB+0x180=%d +0x184=%d +0x188[0]=%d entry=%d", tag, n,
                    *(int32_t*)(rm + 0x178), *(int32_t*)(sb + 0x180), *(int32_t*)(sb + 0x184), *(int32_t*)(sb + 0x188), cnt);
        for (int j = 0; j < cnt && j < 16; j++) {
            uint8_t* e = ent[j]; if (!e) continue;
            if (*(int32_t*)(e + 0x44) != 0 && *(int32_t*)(e + 0x224) == 4) continue;      /* vuota */
            int32_t d = *(int32_t*)(e + 0x22c);
            uint8_t* ds = (d >= 0 && d < 12) ? sb + 0x30 + d * 0x1c : nullptr;
            Logging.Log("[mk8dx-8p] A2   e%d locale=%d tipo=%d/%d pers=%d var=%u carr=%d gomme=%d ali=%d d=%d | desc pers=%d carr=%d gomme=%d ali=%d tipo=%d",
                        j, *(int32_t*)(e + 0x4c), *(int32_t*)(e + 0x224), *(int32_t*)(e + 0x44), *(int32_t*)(e + 0x21c), e[0x220],
                        *(int32_t*)(e + 0x210), *(int32_t*)(e + 0x214), *(int32_t*)(e + 0x218), d,
                        ds ? *(int32_t*)(ds + 0xc) : -9, ds ? *(int32_t*)(ds + 0x0) : -9, ds ? *(int32_t*)(ds + 0x4) : -9,
                        ds ? *(int32_t*)(ds + 0x8) : -9, ds ? *(int32_t*)(ds + 0x14) : -9);
        }
    }
}
namespace p4 {
    static void RestoreNamesPage() {                 /* (codice spostato da CharselEnterHook, invariato) */
        if (p4::on && p4::reg && p4::regOwner) {                             /* dopo una gara RacePagesSetupHook ha riscritto la voce 0x12 */
            uint8_t* tab = mk8dx::Fn<uint8_t* (*)()>(0x8bcc5c)(); int32_t id = p4::kNamesId;
            if (*(uintptr_t*)(tab + 0x8 + id * 8) != p4::regFac || *(uint8_t**)(tab + 0x448 + id * 8) != p4::regOwner) {
                *(uintptr_t*)(tab + 0x8 + id * 8) = p4::regFac; *(uint8_t**)(tab + 0x448 + id * 8) = p4::regOwner;
                Logging.Log("[mk8dx-8p] p4: voce 0x12 della tabella pagine ripristinata per il menu (era della gara)");
            }
            uint8_t* np = p4::namesPage; uintptr_t vp = *(uintptr_t*)(mk8dx::Base() + 0x13093a0) + 0x10;   /* vptr CharaParts */
            if (np && mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&id) != np && *(uintptr_t*)np == vp && *(int32_t*)(np + 0xe8) == id
                && *(uint8_t**)(np + 0xf0) == p4::regOwner) {
                mk8dx::Fn<void (*)(uint8_t*, int32_t*, uint8_t*)>(0x8bd0c8)(mk8dx::Fn<uint8_t* (*)()>(0x8bdc8c)(), &id, np);   /* mgr+0x30[id] = pagina */
                Logging.Log("[mk8dx-8p] p4: pagina nomi %p rimessa nel gestore pagine (id 0x12)", np);
            }
        }
    }
    static void CalcOne(int n, bool multi) {   /* (codice spostato da CharselEnterHook, invariato) */
        p4::one = MK8DX_SEL1ROUND && p4::on && multi && padui::active8 && cur8::on && cw8::ready && n > 4 && n <= 8 && p4::Names();
        if (p4::on && multi && n > 4 && !p4::one) {                           /* diagnostica: perche' non a turno unico */
            int32_t id = p4::kNamesId; uint8_t* pg = mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&id);
            Logging.Log("[mk8dx-8p] p4: NON turno unico: active8=%d cur8=%d cw8=%d reg=%d 0x8bf0ec(0x12)=%p id=%x vptr=%lx +1e8=%d +1d0=%p",
                        padui::active8, cur8::on, cw8::ready, p4::reg, pg, pg ? *(int32_t*)(pg + 0xe8) : -1,
                        pg ? *(uintptr_t*)pg - mk8dx::Base() : 0, pg ? *(int32_t*)(pg + 0x1e8) : -1, pg ? *(void**)(pg + 0x1d0) : nullptr);
        }
    }
}
/* ---- step 5c opzione B (MK8DX_SEL8B, report/step5c_8pl_plan.md §3 e "Implementazione") ------------------------------------------ */
namespace sel8b {
    struct Set { uint8_t* p728; uint8_t* p738; uint8_t* arr[48]; uint8_t* cur; uint8_t o178[0x18]; };
    static Set A, B;
    static unsigned long uiFrames, f0 = ~0ul;    /* frame del gestore pagine (0x8bd6d8) / frame dell'ultimo ingresso (M2) */
    static int32_t diagId;
    static bool IsSel(const uint8_t* pg) { return pg && *(const int32_t*)(pg + 0xe8) == 0x2e && *(const uintptr_t*)pg == mk8dx::Base() + 0x1278940; }
    static uint8_t* Grp(uint8_t* pg) { uint8_t** q = *(uint8_t***)(pg + 0x178); return q ? *q : nullptr; }   /* come 0x4b5860/0x4b5864 */
    static uint8_t* Page(int32_t id) { return mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&id); }
    static void Save(uint8_t* pg, Set& s) {
        s.p728 = *(uint8_t**)(pg + 0x728); s.p738 = *(uint8_t**)(pg + 0x738); memcpy(s.arr, pg + 0x740, sizeof(s.arr));
        s.cur = *(uint8_t**)(pg + 0x1b0); if (uint8_t* g = Grp(pg)) memcpy(s.o178, g, 0x18);
    }
    static void Load(uint8_t* pg, const Set& s) {
        *(uint8_t**)(pg + 0x728) = s.p728; *(uint8_t**)(pg + 0x738) = s.p738; memcpy(pg + 0x740, s.arr, sizeof(s.arr));
        *(uint8_t**)(pg + 0x1b0) = s.cur; if (uint8_t* g = Grp(pg)) memcpy(g, s.o178, 0x18);
    }
    /* H2: dentro l'onCreate della 0x12 (CharaPartsCreateHook, dopo p5::BuildB). Stessa 0x4b5688 dell'onCreate 0x4bcc84, con +0x728 = 0x12
       e un cursore nuovo (come vt72 0x4bcfe8): 48 L_Chara + B_MP legati alla parte 8pl, gruppo animazioni {0x12, 0x19}. */
    static void BuildB(uint8_t* nm) {
        uint8_t* sel = Page(0x2e);
        if (!IsSel(sel) || built == sel) return;
        uint8_t* g = Grp(sel); uint8_t* a728 = *(uint8_t**)(sel + 0x728);
        if (!nm || p4::Names() != nm || !g || !*(uint8_t**)(g + 8) || !a728 || *(int32_t*)(a728 + 0xe8) != 0x16 || !*(uint8_t**)(sel + 0x740)
            || !*(uint8_t**)(sel + 0x1b0) || *(uint32_t*)(*(uint8_t**)(sel + 0x1b0) + 0xb8) != cur8::kMagic) {
            Logging.Log("[mk8dx-8p] sel8b: set B NON costruito: sel=%p nomi=%p/%p gruppo=%p +0x728=%p +0x740[0]=%p cur=%p", sel, nm, p4::Names(), g, a728,
                        *(void**)(sel + 0x740), *(void**)(sel + 0x1b0));
            return;
        }
        size_t h0 = m3d8::MaxAlloc(m3d8::CurHeap());
        Save(sel, A);
        uint8_t* p9d8 = *(uint8_t**)(sel + 0x9d8); int32_t n198 = *(int32_t*)(sel + 0x198);
        int32_t gc = *(int32_t*)g, gn = *(int32_t*)(g + 0x10);
        *(uint64_t*)(g + 8) = 0; *(int32_t*)g = 0; *(int32_t*)(g + 0x10) = 0;   /* 0x4b586c: niente free dell'array A; 0x8af1c4 riparte da [0] */
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x8bafb8)(sel, 50);                 /* cursore B (0x1a8, 'CUR8' da Cur8CtorHook), come vt72 */
        *(uint8_t**)(sel + 0x728) = nm;
        mk8dx::Fn<void (*)(uint8_t*)>(0x4b5688)(sel);
        *(int32_t*)(sel + 0x198) = n198;                                        /* 0x4b57a0 ha riaggiunto il ricevitore +0x8d0 */
        *(uint8_t**)(sel + 0x9d8) = p9d8;                                       /* 0x4b58ec: nuova alloc 0x20, si tiene quella A */
        Save(sel, B); Load(sel, A); built = sel; isB = false; p4::namesPage = nm;
        uint8_t* ga = *(uint8_t**)(B.o178 + 8);                                 /* voci da 0x20: parte a +0 (0x8af1f0) */
        Logging.Log("[mk8dx-8p] sel8b: set B selezione costruito su 0x12 %p: cur=%p (A %p) L_Chara_00=%p (A %p) B_MP=%p (A %p) gruppo A cap/n=%d/%d B=%d/%d "
                    "parti B {%p,%p} heap 0x%lx -> 0x%lx", nm, B.cur, A.cur, B.arr[0], A.arr[0], B.p738, A.p738, gc, gn, *(int32_t*)B.o178,
                    *(int32_t*)(B.o178 + 0x10), ga ? *(void**)ga : nullptr, ga ? *(void**)(ga + 0x20) : nullptr,
                    (unsigned long)h0, (unsigned long)m3d8::MaxAlloc(m3d8::CurHeap()));
    }
    static bool Use(uint8_t* pg, bool b) {           /* tutto o niente; true = set cambiato */
        if (built != pg || b == isB) return false;
        if (cur8::Wide(*(uint8_t**)(pg + 0x1b0))) cur8::SetWide(pg, false, 0);   /* come all'uscita: il cursore che si lascia torna a 4 */
        Save(pg, isB ? B : A); Load(pg, b ? B : A); isB = b;
        uint8_t* p = *(uint8_t**)(pg + 0x728); uint8_t* c = *(uint8_t**)(pg + 0x1b0);
        Logging.Log("[mk8dx-8p] sel8b: selezione set %c: +0x728=%p (id 0x%x) +0x738=%p cur=%p CUR8=%d", b ? 'B' : 'A', p, p ? *(int32_t*)(p + 0xe8) : -1,
                    *(void**)(pg + 0x738), c, c && *(uint32_t*)(c + 0xb8) == cur8::kMagic);
        return true;
    }
    static void M1(uint8_t* pg, const char* what) {
        int32_t id = *(int32_t*)(pg + 0xe8);
        uint8_t* p = *(uint8_t**)(pg + (id == 0x31 ? 0x3a8 : 0x728)); uint8_t* c = *(uint8_t**)(pg + 0x1b0);
        Logging.Log("[mk8dx-8p] sel8b: M1 enter id=0x%x set=%c +0x%x->0x%x cur=%p wide=%d one=%d N=%d f=%lu (%s)", id,
                    id == 0x31 ? (p5::isB ? 'B' : 'A') : (isB ? 'B' : 'A'), id == 0x31 ? 0x3a8 : 0x728, p ? *(int32_t*)(p + 0xe8) : -1, c,
                    cur8::Wide(c), p4::one, menu8::LocalCount(), uiFrames, what);
        f0 = uiFrames; diagId = id;
    }
    static int PaneVis(uint8_t* page, const char* name) {   /* bit0 di pane+0x58 (0x8ade78); -1 = pagina/pane assente */
        uint8_t* part = page ? *(uint8_t**)(page + 0x1d0) : nullptr; if (!part) return -1;
        hud8::SafeString ss = hud8::Str(name);
        uint8_t* pane = mk8dx::Fn<uint8_t* (*)(uint8_t*, hud8::SafeString*)>(0x8ad8e8)(part + 0x30, &ss);
        return pane ? (pane[0x58] & 1) : -1;
    }
    static void Frame(uint8_t* mgr) {                /* per frame del gestore pagine (Cur8FocusFrameHook) */
        uiFrames++;
        int32_t n = mgr ? *(int32_t*)(mgr + 0xb50) : 0; uint8_t** list = mgr ? *(uint8_t***)(mgr + 0xb58) : nullptr;
        auto in = [&](const uint8_t* pg) { for (int i = 0; pg && list && i < n; i++) if (list[i] == pg) return true; return false; };   /* come 0x8bd298 */
        selOpen = IsSel(selPg) && in(selPg);
        kartOpen = p5::isB && p5::built && in(p5::built);
        openAt = m3d8::frames;
        unsigned long d = uiFrames - f0;
        if (d >= 120) return;
        uint8_t* pg[4] = { Page(0x2e), Page(0x31), Page(0x16), Page(p4::kNamesId) };
        int32_t st[4][3]; for (int k = 0; k < 4; k++) for (int j = 0; j < 3; j++) st[k][j] = pg[k] ? *(int32_t*)(pg[k] + 0x120 + 4 * j) : -1;
        uint8_t* c = pg[0] ? *(uint8_t**)(pg[0] + 0x1b0) : nullptr; uint8_t* s0 = m3d8::sub0; float* r = s0 ? (float*)(s0 + 0x30) : nullptr;
        Logging.Log("[mk8dx-8p] sel8b: M2 [0x%x] f=%lu sel=%d/%d/%d kart=%d/%d/%d 16=%d/%d/%d L%d 12=%d/%d/%d L%d NP00(16)=%d NP04(12)=%d shown=%d open=%d%d "
                    "rect0=%.0f,%.0f,%.0f,%.0f cur+bc=%d set=%c", diagId, d, st[0][0], st[0][1], st[0][2], st[1][0], st[1][1], st[1][2],
                    st[2][0], st[2][1], st[2][2], in(pg[2]), st[3][0], st[3][1], st[3][2], in(pg[3]), PaneVis(pg[2], "N_Player_00"),
                    PaneVis(pg[3], "N_Player_04"), m3d8::shown, selOpen, kartOpen, r ? r[0] : -1.f, r ? r[1] : -1.f, r ? r[2] : -1.f, r ? r[3] : -1.f,
                    c ? *(int32_t*)(c + 0xbc) : -1, isB ? 'B' : 'A');
    }
}
/* H1: 0x8bb3e8(page) = ingresso nello stato 3 (In) di ogni pagina: azzeramenti, stato 3, ripristino cursore +0x1b0 (0x8bb454), enter dei
   controlli, onEnter vt75 (0x8bb4d8). Per la selezione Multi il set (A = 0x16 vanilla, B = 0x12) si sceglie qui, prima del cursore. */
HOOK_DEFINE_TRAMPOLINE(PageEnterHook) {
    static void Callback(uint8_t* pg) {
        if (sel8b::IsSel(pg)) {
            p4::RestoreNamesPage();                              /* prima di p4::Names(): dopo una gara la voce 0x12 e' della gara */
            p4::CalcOne(menu8::LocalCount(), true);
            bool b = p4::one && sel8b::built == pg && sel8b::B.p728 == p4::Names();
            if (p4::one && !b) Logging.Log("[mk8dx-8p] sel8b: set B non disponibile (built=%p sel=%p B+0x728=%p nomi=%p): set A con la 0x12 sovrapposta (passo 4)",
                                           sel8b::built, pg, sel8b::B.p728, p4::Names());
            sel8b::Use(pg, b);
            /* 7e: la pagina kart torna al set A (4 slot) gia' qui se N <= 4: 0x4bb650 chiama 0x4c0490(kart) prima del suo onEnter
               (KartEnterHook); con il set B rimasto da un 8P il ciclo su +0x39c = 8 leggeva il giocatore 4 inesistente (+0x21c su NULL) */
            if (!p4::one && p5::built && p5::isB) { p5::Use(p5::built, false); Logging.Log("[mk8dx-8p] 7e: pagina kart riportata al set A (N=%d)", menu8::LocalCount()); }
            sel8b::selPg = pg; sel8b::entered = pg;
            sel8b::M1(pg, "PageEnterHook");
        } else if (pg && *(int32_t*)(pg + 0xe8) == 0x31 && p5::built == pg) sel8b::M1(pg, "PageEnterHook kart");
        Orig(pg);
    }
};
HOOK_DEFINE_TRAMPOLINE(CharselEnterHook) {               /* 0x4b74a4 (vt77 onEnter): ogni ingresso riparte dal turno 1 */
    static void Callback(uint8_t* page) {
        p6::EnsureLoaded();
        int n = menu8::LocalCount(); bool multi = *(int32_t*)(page + 0xe8) == 0x2e;
        if (MK8DX_SEL8B && sel8b::entered == page) sel8b::entered = nullptr;      /* step 5c: ripristino, p4::one e set gia' fatti in PageEnterHook */
        else {
            p4::RestoreNamesPage();
            p4::CalcOne(n, multi);
            if (MK8DX_SEL8B && multi && sel8b::built == page) {   /* onEnter senza 0x8bb3e8 (non atteso): set scelto qui, cursore ripristinato a mano */
                Logging.Log("[mk8dx-8p] sel8b: onEnter senza PageEnterHook (pagina %p)", page);
                if (sel8b::Use(page, p4::one && sel8b::B.p728 == p4::Names())) mk8dx::Fn<void (*)(uint8_t*)>(0x8afc54)(*(uint8_t**)(page + 0x1b0));
            }
        }
        if (p4::on) {                                                        /* passo 4 */
            if (multi) *(int32_t*)(page + 0x8c0) = p4::one ? 8 : 4;          /* 0x4bcc70: Multi = 4 */
            *(uint8_t**)(page + 0xce8) = p4::one ? p4::Names() : *(uint8_t**)(page + 0x728);
        }
        sel8::round = (!p4::one && sel8::TwoRounds()) ? 1 : 0; padui::round2 = false;
        if (p4::one) cur8::SetWide(page, true, n);                           /* prima di Orig: ciclo cursori 0x4b7af0 a 8 slot */
        Orig(page);
        if (!p4::one) { bool w = cur8::on && padui::active8 && multi && (MK8DX_P3TEST || *(int32_t*)(page + 0x8c0) > 4);
                        cur8::SetWide(page, w, n); }
        if (p4::on && multi) {
            if (MK8DX_SEL8B && sel8b::isB) { if (p5::on) p5::Hide3D(); }   /* step 5c H3: set B, Orig ha aperto [+0x728] = 0x12 (0x4b7d3c) */
            else if (p4::one) p4::OpenNames(page);
            else p4::CloseNames(page);
        }
        if (p4::one) Logging.Log("[mk8dx-8p] p4: selezione a turno unico, N=%d, +0x8c0=8, nomi %p", n, *(uint8_t**)(page + 0xce8));
        {   /* step 5b passo 2: id senza fabbrica / proprietario nella tabella pagine 0x8bcc5c() a ogni ingresso (prima e dopo una gara) */
            uint8_t* tab = mk8dx::Fn<uint8_t* (*)()>(0x8bcc5c)();
            char buf[480]; int n = 0;
            for (int id = 0; id < 0x88 && n < 440 && tab; id++) {
                void* f = *(void**)(tab + 0x8 + id * 8); void* o = *(void**)(tab + 0x448 + id * 8);
                if (!f || !o) n += snprintf(buf + n, sizeof(buf) - n, "%x%s ", id, !f ? (o ? "f" : "") : "o");
            }
            Logging.Log("[mk8dx-8p] P2: tabella pagine all'ingresso selezione, id liberi (f = solo fabbrica nulla, o = solo proprietario nullo): %s", buf);
        }
        if (sel8::round) Logging.Log("[mk8dx-8p] A2: selezione personaggi, turno 1 (N=%d, slot=%d)", menu8::LocalCount(), *(int32_t*)(page + 0x8c0));
    }
};
HOOK_DEFINE_TRAMPOLINE(CharselFrameHook) {               /* 0x4ba77c (vt84/86 per frame) */
    static void Callback(uint8_t* page) {
        { static uint8_t* last; static int fr, n; if (page != last) { last = page; fr = 0; }   /* step 7 B tappa 0 */
          if ((fr == 90 || fr % 900 == 600) && n < 12) { n++; m3d8::Probe(p4::one ? "sel8" : "sel"); } fr++; }
        if (sel8::m2) {                                  /* step 5b passo 1: accessi sfuggiti alla vecchia area +0x904 */
            static bool bad;
            for (int i = 0; i < 0x20 && !bad; i += 4) {
                uint32_t v = *(uint32_t*)(page + 0x904 + i);
                if (v != sel8::kCanary) { bad = true; Logging.Log("[mk8dx-8p] M2: CANARY +0x%x = %08x (accesso sfuggito!) page=%p", 0x904 + i, v, page); }
            }
            static int fr; if ((fr++ % 600) == 0) { int32_t* q = (int32_t*)(page + 0xa28);
                Logging.Log("[mk8dx-8p] M2: +0x8c0=%d stati {%d,%d,%d,%d,%d,%d,%d,%d} p4 one=%d nomi=%p cfg8=%u", *(int32_t*)(page + 0x8c0), q[0], q[2], q[4], q[6], q[8], q[10], q[12], q[14],
                    p4::one, p4::on ? *(void**)(page + 0xce8) : nullptr, cfg8::hits); }
        }
        if (p4::one && cw8::ready) {                     /* p5fix D: finestre varianti degli slot 4..7, bit 1<<slot negli item del loro cursore */
            static int fr; bool lg = (fr++ % 60) == 0;
            for (int j = 0; j < 4; j++) {
                int32_t id = cw8::kIds[j]; uint8_t* cl = mk8dx::Fn<uint8_t* (*)(int32_t*)>(0x8bf0ec)(&id);
                if (!cl || *(int32_t*)(cl + 0xe8) != id) continue;
                uint8_t* c = *(uint8_t**)(cl + 0x1b0); uint32_t bit = 1u << (4 + j);
                int32_t cnt = c ? *(int32_t*)(c + 0x10) : 0; uint8_t** it = c ? *(uint8_t***)(c + 0x18) : nullptr;
                for (int k = 0; k < cnt && it; k++) if (it[k]) *(uint32_t*)(it[k] + 0x58) |= bit;
                if (lg && *(int32_t*)(page + 0xa28 + 8 * (4 + j)) == 5) {
                    uint8_t* part = *(uint8_t**)(cl + 0x1d0);
                    uint8_t* g = cur8::Gfx(4 + j); char nm[] = "cm_L_BtnCursor_00"; nm[16] = '4' + j;
                    uint8_t* gp = cur8::GfxPage(); uint8_t* pane = gp ? mk8dx::Fn<uint8_t* (*)(uint8_t*, const char*)>(0x725ad0)(gp, nm) : nullptr;
                    Logging.Log("[mk8dx-8p] D: P%d gfx=%p g+c0=%d pane+58=%x act=%d draw=%d", 5 + j, g, g ? g[0xc0] : -1, pane ? pane[0x58] : 0xff, cur8::cwAct, cur8::cwDraw);
                    Logging.Log("[mk8dx-8p] D: CharaList 0x%x tipo=%d +120=%d +124=%d +1a8=%x +190=%x +2d1=%d parte+2d0=%d cur=%p slot0 %d/%d item=%d foc=%d sel+9c9=%d",
                        id, *(int32_t*)(cl + 0x2a0), *(int32_t*)(cl + 0x120), *(int32_t*)(cl + 0x124), *(uint32_t*)(cl + 0x1a8), *(uint32_t*)(cl + 0x190),
                        cl[0x2d1], part ? *(int32_t*)(part + 0x2d0) : -1, c, c ? *(int32_t*)(c + 0x28) : -1, c ? *(int32_t*)(c + 0x30) : -1, cnt,
                        cur8::foc[j] == cl, page[0x9c9]);
                }
            }
        }
        if (!sel8::TwoRounds() || sel8::round == 0) { padui::round2 = false; Orig(page); if (p4::one && p5::on) p5::Hide3D(); return; }
        int slots = *(int32_t*)(page + 0x8c0); if (slots > 4) slots = 4;
        int confirmed = 0;
        for (int i = 0; i < slots; i++) if (*sel8::SlotState(page, i) == 7) confirmed++;
        if (confirmed == slots) {
            if (sel8::round == 1) {                      /* turno 2: slot 0..3 di nuovo "sceglie" (come 0x4ba71c), pad 0..3 -> extra */
                sel8::round = 2; padui::round2 = true;
                if (MK8DX_P3TEST) cur8::SetWide(page, false, 0);
                for (int i = 0; i < slots; i++) {
                    if (i == 0) { uint8_t* pf = mk8dx::Fn<uint8_t* (*)()>(0x422108)(); if (pf) pf[0x3d9] = 1; }
                    *sel8::SlotState(page, i) = 3;
                    *(uint32_t*)(page + 0x1a8) |= 1u << i;
                    mk8dx::Fn<void (*)(uint8_t*, int)>(0x8ba814)(page, i);
                    mk8dx::Fn<void (*)(int)>(0x14b508)(i);
                }
                /* il personaggio sotto il cursore (cfg+0x1fb4[i], scritto dal gioco a ogni spostamento) va nell'entry del
                   locale 4+i come fa l'onEnter per gli slot in stato 3 (0x4b840c): senza spostamenti non ci sarebbe scrittura */
                uint8_t* cfg = menu8::Cfg();
                for (int i = 0; i < slots; i++) {
                    int32_t pers = *(int32_t*)(cfg + 0x1fb4 + 4 * i);
                    if (pers >= 0) mk8dx::Fn<void (*)(int, int32_t*)>(0x4203e0)(i, &pers);
                }
                Logging.Log("[mk8dx-8p] A2: turno 2: i giocatori 5-%d scelgono con i pad UI 4..%d", menu8::LocalCount(), menu8::LocalCount() - 1);
                sel8::LogAssigned("fine turno 1");
                Orig(page);
                return;
            }
            /* turno 2 completo: l'avanzamento (0x4ba938 w28 == N) va fatto con N = 4 per questa sola chiamata */
            uint8_t* cfg = menu8::Cfg(); int32_t n = *(int32_t*)(cfg + 0x1fa4);
            *(int32_t*)(cfg + 0x1fa4) = slots;
            Orig(page);
            *(int32_t*)(cfg + 0x1fa4) = n;
            static bool logged; if (!logged) { logged = true; Logging.Log("[mk8dx-8p] A2: turno 2 completo, avanzamento con N=%d ripristinato", n); }
            return;
        }
        Orig(page);
    }
};
HOOK_DEFINE_TRAMPOLINE(CharselStageHook) {               /* 0x4badb0 (vt87): stadio 1 -> 16 entry a tipo 4, poi tipo 0 per gli slot confermati (solo 4 pad) */
    static void Callback(uint8_t* page) {
        int32_t stage = *(int32_t*)(page + 0x124);
        Orig(page);
        if (!sel8::TwoRounds()) return;
        uint8_t* rm = race8::RaceMgr(); int n = menu8::LocalCount(); int fixed = 0;
        for (int l = 0; l < n && l < 8; l++) {
            uint8_t* e = race8::Entry(rm, l);
            if (e && *(int32_t*)(e + 0x224) == 4) {    /* locale umano marcato "vuoto" dal ciclo a 4 pad */
                int32_t zero = 0; mk8dx::Fn<void (*)(int32_t, int32_t*)>(0x42022c)(race8::Locals(rm)[l], &zero); fixed++;
            }
        }
        if (fixed) {
            uint8_t* cfg = menu8::Cfg(); *(int32_t*)(cfg + 0x1434) = n;
            Logging.Log("[mk8dx-8p] A2: stadio %d: tipo 0 ripristinato su %d locali, cfg+0x1434=%d", stage, fixed, n);
            sel8::LogAssigned("dopo vt87");
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(KartEnterHook) {                  /* 0x4c0898 (kart vt77 onEnter): turno 1 con i pad 0..3 */
    static void Callback(uint8_t* page) {
        bool one = p4::one && p5::built == page;                             /* passo 5: kart a 8 slot (set B) */
        p5::Use(page, one);
        padui::round2 = false; sel8::kartRound = (!one && sel8::TwoRounds()) ? 1 : 0;
        if (!one && p4::namesOpen) p4::CloseNames(*(uint8_t**)(page + 0x3b8));   /* passo 4: kart+0x3b8 = pagina selezione */
        if (one) {
            mk8dx::Fn<void (*)(uint8_t*)>(0x8afc54)(*(uint8_t**)(page + 0x1b0));      /* 0x8bb454 aveva ripristinato il cursore A */
            cur8::SetWide(page, true, menu8::LocalCount());                          /* prima del ciclo maschere 0x4c15e0 */
            if (p5::fromSel) { mk8dx::Fn<void (*)(uint8_t*, bool)>(0x4c0490)(page, true);   /* come 0x4bb650, sui selettori B */
                               if (!sel8b::isB) p5::Change(*(uint8_t**)(page + 0x3a8), 1.0f); }   /* = 0x4bb550 (animazione Change), ma sulla
                                                                     0x12; con il set B della selezione l'ha gia' fatto 0x4bb550 sulla 0x12 */
        }
        p5::fromSel = false;
        Orig(page);
        {   /* p5fix §3.3: diagnostica icona carrozzeria (valore cfg presente nella lista del selettore?) */
            uint8_t* cfg = menu8::Cfg(); uint8_t** ss = *(uint8_t***)(page + 0x3d8);
            for (int i = 0; cfg && ss && i < *(int32_t*)(page + 0x39c) && i < 8; i++) { uint8_t* s = ss[i]; if (!s) continue;
                int32_t want = i < 4 ? *(int32_t*)(cfg + 0x1fd4 + 4 * i) : cfg8::sh[8 + i - 4];
                int cnt = *(int32_t*)(s + 0xcc), hit = -1; uint8_t* it = *(uint8_t**)(s + 0xc0);
                for (int k = 0; k < cnt && it; k++) if (*(int32_t*)(it + 16 * k) == want) hit = k;
                Logging.Log("[mk8dx-8p] C: sel%d slot=%d pers=%d cfg=%d match=%d n=%d +324=%d +110=%d +114=%d +118=%d +11c=%d +120=%d,%d", i,
                    *(int32_t*)(s + 0x1e8), *(int32_t*)(s + 0x278), want, hit, cnt, *(int32_t*)(s + 0x324), *(int32_t*)(s + 0x110),
                    *(int32_t*)(s + 0x114), *(int32_t*)(s + 0x118), *(int32_t*)(s + 0x11c), *(int32_t*)(s + 0x120), *(int32_t*)(s + 0x124)); }
        }
        if (one) { for (int i = 4; i < 8; i++) if (uint8_t* c = p5::PcCtl(i)) *(uint8_t**)(c + 0xb8) = page;   /* come 0x49af58 */
                   p5::Hide3D(); Logging.Log("[mk8dx-8p] p5: kart a turno unico, +0x39c=%d +0x190=%x", *(int32_t*)(page + 0x39c), *(uint32_t*)(page + 0x190)); }
        if (sel8::kartRound) { Logging.Log("[mk8dx-8p] A2: pagina kart, turno 1 (+0x39c=%d)", *(int32_t*)(page + 0x39c)); sel8::LogAssigned("kart onEnter"); }
    }
};
namespace sel8 { static int32_t kart184; }
HOOK_DEFINE_TRAMPOLINE(KartFrameHook) {                  /* 0x4c2820 (kart per frame): l'avanzamento e' in coda (0x4c2a44: +0x498=1, +0x184=0x23, 0x8ba5fc) */
    static void Callback(uint8_t* page) {
        sel8::kart184 = *(int32_t*)(page + 0x184);
        uint8_t before = page[0x498];
        Orig(page);
        if (p4::one && p5::isB) p5::Hide3D();
        if (!before && page[0x498]) {                     /* avanzamento verso la pagina successiva */
            padui::round2 = false; sel8::kartRound = 0;
            sel8::LogAssigned(*(int32_t*)(page + 0xe8) == 0x31 ? "kart Multi -> avanti" : "kart -> avanti");
        }
    }
};
namespace pause8 { static void TintCursor0(int frame); }   /* step 7, definito piu' avanti */
HOOK_DEFINE_TRAMPOLINE(PageLinkHook) {                   /* 0x8ba5fc(page, link): transizione; intercetta l'avanzamento del kart al turno 1 */
    static void Callback(uint8_t* page, uint8_t* link) {
        if (page && *(int32_t*)(page + 0xe8) == 0x6f) pause8::TintCursor0(1);   /* step 7: uscita dalla pausa -> cursore di P1 giallo */
        if (page && link == page + 0x1d0 && *(int32_t*)(page + 0xe8) == 0x31 && sel8::kartRound == 1 && sel8::TwoRounds()) {
            page[0x498] = 0; *(int32_t*)(page + 0x184) = sel8::kart184;     /* annulla quanto fatto da 0x4c2a44 */
            sel8::kartRound = 2; padui::round2 = true;
            int slots = *(int32_t*)(page + 0x39c); if (slots > 4) slots = 4;
            for (int i = 0; i < slots; i++) mk8dx::Fn<void (*)(uint8_t*, int, int)>(0x4c2b40)(page, i, 1);   /* come l'annulla (B) da stato 3 */
            for (int i = 0; i < slots; i++) for (int part = 0; part < 3; part++)
                mk8dx::Fn<void (*)(uint8_t*, int, int, int)>(0x4c1bfc)(page, i, part, 1);               /* parti correnti -> locali 4..7 */
            Logging.Log("[mk8dx-8p] A2: kart turno 2: i giocatori 5-%d scelgono il kart", menu8::LocalCount());
            sel8::LogAssigned("kart fine turno 1");
            return;
        }
        Orig(page, link);
    }
};
HOOK_DEFINE_TRAMPOLINE(SetCharaHook) {                   /* 0x4203e0(slot, int* pers): log dei personaggi scritti */
    static void Callback(int slot, int32_t* pers) {
        int e = mk8dx::Fn<int (*)(uint8_t*)>(0x8be03c)(padui::Get(slot));
        Orig(slot, pers);
        if (sel8::TwoRounds() || (MK8DX_P7_MENU3D8 && slot >= 4)) Logging.Log("[mk8dx-8p] A2: personaggio slot %d -> entry %d = %d (turno %d)", slot, e, pers ? *pers : -1, padui::round2 ? 2 : 1);
    }
};

/* ---- step 6 A: gestore kart M (G+0x850): array dei locali a 8 (report/step6_plan_A.md) -----------------------------
 * M+0xa0 e' K*[4] (K = oggetto per kart, +0x258 descrittore, +0x1620 kart, +0x1624 id, +0x1640 locale umano) riempito
 * dalla registrazione 0xac290 con indice M+0x104 senza limite: con 8 locali sfora su +0xc0..+0xd8 (altri campi) e il
 * ciclo per frame 0xaa16c (i < M+0x104) chiama una vtable su NULL -> PC=0. Rilocazione: K*[8] a M+0x3000, s32[8]
 * (ex +0xe0) a M+0x3040, sizeof(M) 0x2978 -> 0x3080 (15 patch) + ctor che inizializza + specchio nel vecchio array. */
namespace kart8 {
    constexpr uintptr_t kA0 = 0x3000, kE0 = 0x3040;
    static bool ok;
}
HOOK_DEFINE_TRAMPOLINE(KartMgrCtorHook) {                /* 0xa8c00 M::M */
    static void Callback(uint8_t* m) {
        Orig(m);
        memset(m + kart8::kA0, 0, 8 * sizeof(void*));
        memset(m + kart8::kE0, 0xff, 8 * sizeof(int32_t));
        Logging.Log("[mk8dx-8p] R0: M=%p creato (0x3080 B)", m);
    }
};
HOOK_DEFINE_TRAMPOLINE(KartRegisterHook) {               /* 0xac290 = K vt+0x20 (K*): registra il kart; se locale M+0x3000[M+0x104++] = K */
    static void Callback(uint8_t* k) {
        Orig(k);
        if (!k[0x1640]) return;
        uint8_t* g = *(uint8_t**)(k + 0x60); if (!g) return;
        uint8_t* m = *(uint8_t**)(g + 0x850); if (!m) return;
        int n = *(int32_t*)(m + 0x104), i = n - 1;
        if (i >= 0 && i < 4) {                           /* specchio vanilla per lettori eventualmente sfuggiti */
            ((uint8_t**)(m + 0xa0))[i] = ((uint8_t**)(m + kart8::kA0))[i];
            ((int32_t*)(m + 0xe0))[i]  = ((int32_t*)(m + kart8::kE0))[i];
        }
        Logging.Log("[mk8dx-8p] R1: M=%p locale %d/%d K=%p kart=%d id=%d", m, i, n, k, *(int32_t*)(k + 0x1620), *(int32_t*)(k + 0x1624));
        if (n > 8) Logging.Log("[mk8dx-8p] R1: ERRORE %d locali > 8", n);
    }
};


/* ---- step 6 C (report/step6_plan_C.md §5A): le viste extra vengono disegnate dentro il job Draw di 3D Sub0 -----------
 * I job propri dei layer extra (ultimi della coda 0, in parallelo al 2D) finivano sopra il 2D a schermo intero in gara;
 * disegnandoli subito dopo Sub0, nello stesso job, ereditano la posizione del Sub0 rispetto al 2D. */
#ifndef MK8DX_DRAW_IN_SUB0
#define MK8DX_DRAW_IN_SUB0 0   /* 6C: superato da 6b E1 (l'ordine GPU non dipende dal job) */
#endif
/* Step 6b E1 (report/step6b_plan.md §3): la GPU riceve i segmenti di un display ordinati per id crescente (heapsort 0x689cec,
 * comparatore 0x684a64 = a.id - b.id) e l'id del job Draw/SubDraw di un layer e' slot*258+0xffff (0x680f8c): i layer extra
 * (slot 14..21) finivano dopo 2D Main/Front/Overlay (10..13). Con le viste extra attive si rimappa l'id per rango. */
#ifndef MK8DX_SLOT_REMAP
#define MK8DX_SLOT_REMAP 1
#endif
namespace order8 {
    static int Rank(int slot) {
        /* step 6d T5 (report/step6d_flash_root.md §7.5): 1 = bande 3D extra (14..17) PRIMA delle 3D Sub0..3 sulla GPU (solo in gara) */
        if (slot >= 0 && slot <= 5)   return slot;               /* 3D Back, 2D Back, 3D Sub0..3 */
        if (slot >= 14 && slot <= 17) return 6 + (slot - 14);    /* 3D Sub4..7 */
        if (slot >= 6 && slot <= 9)   return 10 + (slot - 6);    /* 2D Sub0..3 */
        if (slot >= 18 && slot <= 21) return 14 + (slot - 18);   /* 2D Sub4..7 */
        if (slot >= 10 && slot <= 13) return 18 + (slot - 10);   /* 2D Main, 3D Extra, 2D Front, 2D Overlay */
        return slot;                                             /* 22..31 invariati */
    }
    static int Id(const uint8_t* ly, int id) {
        if (!MK8DX_SLOT_REMAP || !(proto::active || m3d8::shown) || !ly) return id;   /* step 7 B: anche nel menu con le anteprime a 8 */
        int slot = *(const int32_t*)(ly + 0x84);
        if (id != slot * 258 + 0xffff) return id;                /* solo l'id standard del job */
        return Rank(slot) * 258 + 0xffff;
    }
}
HOOK_DEFINE_TRAMPOLINE(LayerDrawJobHook) {               /* 0x683ecc(display, ctx, layer, id) */
    static void Callback(uint8_t* disp, void* ctx, uint8_t* ly, int id) {
        if (MK8DX_DRAW_IN_SUB0 && proto::active)
            for (int j = 0; j < proto::Extra; j++) if (ly == proto::layer[j]) return;       /* job proprio: saltato */
        int nid = order8::Id(ly, id);
        static unsigned long n;
        if (nid != id && *(int32_t*)(ly + 0x84) == 14 && (n++ % 3600) == 0)
            Logging.Log("[mk8dx-8p] ordine: slot %d id %d -> %d", *(int32_t*)(ly + 0x84), id, nid);
        Orig(disp, ctx, ly, nid);
        if (!MK8DX_DRAW_IN_SUB0 || !proto::active || !proto::mgr || !proto::R) return;
        uint8_t* sub0 = (*reinterpret_cast<uint8_t***>(proto::mgr + 0x128))[0];
        if (ly != sub0) return;
        for (int j = 0; j < proto::Used(); j++) {
            uint8_t* ex = proto::layer[j];
            if (*(uint8_t**)(ex + 0x1020) != proto::R || !(ex[0x82] & 1)) continue;
            Orig(disp, ctx, ex, *(int32_t*)(ex + 0x84) * 258 + 0xffff);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(LayerSubDrawJobHook) {            /* 0x684148(display, ctx, layer, id) */
    static void Callback(uint8_t* disp, void* ctx, uint8_t* ly, int id) { Orig(disp, ctx, ly, order8::Id(ly, id)); }
};

/* ---- step 7: pausa a 8 (report/step7_plan.md §1) ------------------------------------------------------------------------
 * P1 layout rc_Page_RacePause_SgOunce (RomFS 4.0.1, parte interna esposta come rc_Pause_Sg_00); P2 pannelli L_SwitchBtns8pl_00..07;
 * P3 frame 2/3 (6P/8P) di G_SingleMultiChange_00; P4 assegnazione dei pulsanti della pausa anche ai giocatori 4..7;
 * P5 scanner tasti 0x8b4038 (pad 0..4 srotolati) esteso ai pad 5..7. */
namespace pause8 {
    static char name8[] = "L_SwitchBtns8pl_00"; static int logged;
    /* step7_plan_cursor.md §4: il cursore della pagina (pause+0x1b0) ha 4 slot (pad clamp a 0 per pad >= 4) e legge i pad 0..3
       in 0x8b0084; il cursore grafico i (cm_Page_Cursor+0x1d0[i]) ha il colore = frame i+1 di ColorChange (4.0.1: 7..10 = P5..P8). */
    static int tint = 1;
    static void TintCursor0(int frame) {                   /* 0x442ca0(obj, n): frame ColorChange del cursore grafico 0 */
        if (frame == tint) return;
        uint8_t* cp = mk8dx::Fn<uint8_t* (*)()>(0x8bf228)();
        uint8_t* g0 = cp ? *(uint8_t**)(cp + 0x1d0) : nullptr;
        if (!g0) return;
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x442ca0)(g0, frame);
        tint = frame;
    }
    static bool active, pollHooked;                        /* blocco pausa a 8 installato / 0x8b0084 agganciato (anche dal passo 3) */
    static int Owner(uint8_t* cur) {                       /* pauser >= 4 della pausa attiva che possiede cur, altrimenti -1 */
        if (!active || !view8::split || !cur) return -1;
        uint8_t* pg = *(uint8_t**)(cur + 0x8);
        if (!pg || *(int32_t*)(pg + 0xe8) != 0x6f || !(*(uint32_t*)(pg + 0x1a8) >> 31)) return -1;
        int p = pg[0x51c];
        return (p >= 4 && p < 8 && p < view8::nLocal) ? p : -1;
    }
}
HOOK_DEFINE_REPLACE(PauseLayoutNameHook) {                 /* 0x53c818: vt13 di Race_Pause (0x6f) */
    static const char* Callback(void*) {
        uint8_t* st = mk8dx::Fn<uint8_t* (*)()>(0x86f7bc)();
        const char* r;
        if ((st[0x24] & 0x10) || *(int32_t*)(st + 8) == 2) r = (const char*)(mk8dx::Base() + 0xeef5f6);   /* rc_Page_RacePause */
        else if (view8::split) r = "rc_Page_RacePause_SgOunce";
        else r = (const char*)(mk8dx::Base() + 0xeef608);                                                    /* rc_Page_RacePause_Sg */
        Logging.Log("[mk8dx-8p] P1: layout pausa %s (split=%d)", r, view8::split);
        return r;
    }
};
HOOK_DEFINE_INLINE(PauseBtnsNameCreateHook) {              /* 0x53d3a8 `stp x28,x8,[sp,#0xb8]`: x8 = nome della parte da creare */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!view8::split) return;
        pause8::name8[17] = '0' + (char)ctx->W[23];
        ctx->X[8] = (uint64_t)pause8::name8;
        if (pause8::logged++ < 8) Logging.Log("[mk8dx-8p] P2: creo %s", pause8::name8);
    }
};
HOOK_DEFINE_INLINE(PauseBtnsNameHideHook) {                /* 0x53d678 `str x8,[sp,#0xc0]`: x8 = nome del pane da nascondere */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!view8::split) return;
        pause8::name8[17] = '0' + (char)ctx->W[23];
        ctx->X[8] = (uint64_t)pause8::name8;
    }
};
HOOK_DEFINE_TRAMPOLINE(PauseSgBindHook) {                  /* 0x51bc10: vt9 del controllo rc_Pause_Sg_00 (gruppi a +0x68/+0x70) */
    static void Callback(uint8_t* ctl) {
        Orig(ctl);
        if (!view8::split || *(int32_t*)(ctl + 0x68) <= 5) return;
        uint8_t* g = ((uint8_t**)*(uintptr_t*)(ctl + 0x70))[5];                     /* G_SingleMultiChange_00 */
        mk8dx::Fn<void (*)(uint8_t*, float, int)>(0x8ad3c4)(g, view8::split == 8 ? 3.0f : 2.0f, 0);
        Logging.Log("[mk8dx-8p] P3: SingleMultiChange frame %d", view8::split == 8 ? 3 : 2);
    }
};
HOOK_DEFINE_INLINE(PauseAssignExtraHook) {                 /* 0x53e404, dopo il ciclo 0..3 di vt75 0x53e0bc; x19 = pagina pausa */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (!view8::split) return;
        uint8_t* p = (uint8_t*)ctx->X[19];
        int pauser = p[0x51c], nb = *(int32_t*)(p + 0x1e8);
        uint8_t** btn = (uint8_t**)*(uintptr_t*)(p + 0x1f0);
        uint8_t* cur = *(uint8_t**)(p + 0x1b0);                                     /* `ldr x0,[x19,#0x1b0]` (0x53e2fc) */
        if (!cur) return;
        for (int i = 4; i < view8::nLocal && i < 8; i++) {
            uint32_t bit = 1u << i;
            if (i == pauser) {
                for (int k = 0; k < nb; k++) *(uint32_t*)(btn[k] + 0x88) |= bit;
                *(uint32_t*)(p + 0x1a8) |= bit;
            } else {                                                                /* niente 0x8afaf0(cur,i): clamp -> slot 0 */
                for (int k = 0; k < nb; k++) *(uint32_t*)(btn[k] + 0x88) &= ~bit;
                if (p[0x51d]) *(uint32_t*)(p + 0x1a8) |= bit; else *(uint32_t*)(p + 0x1a8) &= ~bit;
            }
        }
        if (pauser >= 4 && pauser < 8 && nb > 0)                                    /* slot 0 (gia' rilasciato da P1) al pauser */
            mk8dx::Fn<void (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(cur, pauser, *(int32_t*)(btn[0] + 0x98), 0, 1, 0);
        pause8::TintCursor0(pauser >= 4 && pauser < 8 ? 7 + (pauser - 4) : 1);
        Logging.Log("[mk8dx-8p] P4: pausa di P%d, maschera 0x%x, pulsanti %d, colore cursore %d", pauser + 1,
                    *(uint32_t*)(p + 0x1a8), nb, pause8::tint);
    }
};
HOOK_DEFINE_TRAMPOLINE(CursorPollHook) {                   /* 0x8b0084: 0x8b00e0(cur, 0..3) srotolato -> anche il pauser >= 4 */
    static void Callback(uint8_t* cur) {
        int p = pause8::Owner(cur);
        if (p < 0) {
            Orig(cur);
            if (cur8::Wide(cur) && *(int32_t*)(cur + 0x10))                  /* step 5b passo 3: pad 4..7 (0x8ba634 scarta q >= N) */
                for (int q = 4; q < 8; q++) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b00e0)(cur, q);
            if (int s = cur8::CwSlot(cur); s >= 4) {                              /* passo 7 (b): finestra varianti dei pad 4..7 */
                uint8_t* pg = *(uint8_t**)(cur + 8);
                bool act = *(int32_t*)cur8::Slot(cur, 0) == 2 && cur8::foc[s - 4] == pg && *(int32_t*)(pg + 0x120) == 4
                           && *(int32_t*)(cur + 0x10) && mk8dx::Fn<bool (*)(uint8_t*, int)>(0x8ba634)(pg, s);
                cur8::AsWide(cur, s, [&] {
                    if (act) {                                                   /* stato 2 (attivazione fallita all'apertura): come un tasto di riattivazione */
                        uint8_t* sv = cur8::Slot(cur, s);
                        mk8dx::Fn<bool (*)(uint8_t*, int, int, int, int, int)>(0x8af3f4)(cur, s, *(int32_t*)(sv + 8), 0, 1, 0);
                        static int n; if (n++ < 8) Logging.Log("[mk8dx-8p] p7b: riquadro finestra P%d attivato (stato %d)", s + 1, *(int32_t*)sv);
                    } else mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b00e0)(cur, s);
                });
            }
            if (cur8::Wide(cur)) {                                           /* diagnostica passo 3, ogni 120 frame */
                static int fr; if ((fr++ % 120) == 0) {
                    uint8_t* pg = *(uint8_t**)(cur + 8); uint8_t* mgr = mk8dx::Fn<uint8_t* (*)()>(0x8bdc8c)(); uint8_t* cp = cur8::GfxPage();
                    char nm[] = "cm_L_BtnCursor_00";
                    for (int q = 0; q < 8; q++) {
                        uint8_t* sl = cur8::Slot(cur, q); uint8_t* g = cur8::Gfx(q);
                        uint8_t* f = q < 4 ? (mgr ? *(uint8_t**)(mgr + 0xc68 + 8 * q) : nullptr) : cur8::foc[q - 4];
                        nm[16] = '0' + q; uint8_t* pane = cp ? mk8dx::Fn<uint8_t* (*)(uint8_t*, const char*)>(0x725ad0)(cp, nm) : nullptr;
                        bool in = mk8dx::Fn<bool (*)(uint8_t*, int)>(0x8ba634)(pg, q);
                        Logging.Log("[mk8dx-8p] cur8 diag P%d: stato %d idx %d focus=%d in=%d gfx=%p g+c0=%d ctl+78=%d pane=%p p+58=%x 1a8=%x 190=%x",
                                    q + 1, *(int32_t*)sl, *(int32_t*)(sl + 8), f == pg, in, g, g ? g[0xc0] : -1, g ? g[0x30 + 0x78] : -1,
                                    pane, pane ? pane[0x58] : 0xff, *(uint32_t*)(pg + 0x1a8), *(uint32_t*)(pg + 0x190));
                    }
                }
            }
            return;
        }
        uint32_t* m = (uint32_t*)(*(uint8_t**)(cur + 0x8) + 0x1a8);
        uint32_t save = *m;
        *m = save & ~1u;                                   /* P1 non pilota lo slot 0 condiviso */
        Orig(cur);
        *m = save;
        mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b00e0)(cur, p);
        static int n;
        if ((n++ % 120) == 0) {
            uint8_t* mgr = mk8dx::Fn<uint8_t* (*)()>(0x8bdc8c)(); uint8_t* pg = *(uint8_t**)(cur + 8);
            Logging.Log("[mk8dx-8p] C: pauser P%d slot0 stato %d idx %d focus=%d", p + 1, *(int32_t*)(cur + 0x28), *(int32_t*)(cur + 0x30),
                        mgr ? (int)(*(uint8_t**)(mgr + 0xc68) == pg) : -1);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(BtnScanHook) {                      /* 0x8b4038: scanner tasti, pad 0..4 srotolati -> anche 5..7 */
    static void Callback(uint8_t* s) {
        uint8_t* page = *(uint8_t**)(s + 0x10); uint8_t* ctl = *(uint8_t**)(s + 0x18);
        /* step 7: pulsanti aiuti L_SwitchBtn (controllo a obj+0x30, vtable 0x1285598, giocatore a obj+0xc8): la maschera +0x58
           nasce 0x8000000f e viene ristretta a 1<<i solo per i < 4 -> per i giocatori 5-8 resta 0x80000000 (nessun pad) */
        if (!page && ctl && view8::split && *(uintptr_t*)ctl == mk8dx::Base() + 0x1285598) {
            uint32_t& m = *(uint32_t*)(ctl + 0x58);
            int pl = *(int32_t*)(ctl - 0x30 + 0xc8);
            if (pl >= 4 && pl < 8 && !(m & 0xff)) {
                m |= 1u << pl;
                static int nlog; if (nlog++ < 12) Logging.Log("[mk8dx-8p] P6: pulsante aiuti %p del giocatore %d, maschera -> 0x%x", ctl, pl + 1, m);
            }
        }
        Orig(s);
        if (!padui::active8) return;
        for (int i = 5; i < 8; i++) {
            bool ok = page ? mk8dx::Fn<bool (*)(uint8_t*, int)>(0x8ba634)(page, i)
                           : (ctl && mk8dx::Fn<bool (*)(uint8_t*, int)>(0x8ae0d0)(ctl, i));
            if (ok) mk8dx::Fn<void (*)(uint8_t*, int)>(0x8b4174)(s, i);
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(AidsSaveHook) {                     /* 0x3fe040(cfg+0x1fa4): salvataggio aiuti per i < *field */
    static void Callback(int32_t* field) {
        int32_t n = *field;
        if (n <= 4) { Orig(field); return; }
        *field = 4; Orig(field); *field = n;                                    /* niente scritture a cfg+0x2320..0x234c */
        if (!padui::active8) return;
        for (int i = 4; i < n && i < 8; i++) {
            uint8_t* gp = aid8::GpOf(i); if (!gp) continue;
            uint8_t* kart = *(uint8_t**)(gp + 0x200);
            aid8::code[0][i - 4] = (kart && kart[0x394]) ? 1 : 2;
            aid8::code[1][i - 4] = *(int32_t*)(gp + 0x20c) ? 1 : 2;
            aid8::code[2][i - 4] = gp[0x209] ? 1 : 2;
        }
        Logging.Log("[mk8dx-8p] aid8: salvati aiuti 5-%d nell'ombra (R=%d,%d,%d,%d)", n, aid8::code[2][0], aid8::code[2][1], aid8::code[2][2], aid8::code[2][3]);
        p6::Save("0x3fe040");                                                  /* passo 6: ombre P5-P8 su SD */
    }
};
/* step 7 A: 0x50efa8(lyt, slot) imposta il frame PlayerPos = (slot&1)+2 (pari = HUD a sinistra): con 8 celle 4x2 il lato va per
   colonna (colonne 1-2 a sinistra, 3-4 a destra, riferimento Switch 2) */
HOOK_DEFINE_TRAMPOLINE(RaceViewSideHook) {
    static void Callback(uint8_t* lyt, int32_t slot) {
        if (view8::split == 8 && slot >= 0 && slot < 8) slot = ((slot & 3) < 2) ? 0 : 1;
        Orig(lyt, slot);
    }
};

/* step 7 (confronto Switch 2): disposizione interna del riquadro oggetti. L'onCreate dell'ItemBox 0x510460 sceglie fra N_All_01
   (cerchietto a sinistra) e N_All_02 (speculare) con la parita' del posto locale k della Scene (0x510820..0x51098c, scelta a
   0x510a6c `tbnz w25,#0`); w25 e' riletto solo a 0x510e38 come id ItemTeresa (0x34|k). Con 8 celle il lato va per colonna come
   PlayerPos (RaceViewSideHook); le celle 5-8 non sono nella Scene (w25 = 0). La cella si registra alla costruzione (0x548024,
   X23 = ItemBox, X24 = oggetto viewport, +0x100 = cella). */
namespace ibox {
    static uint8_t* box[8]; static int cell[8]; static int next;
    static uint8_t* who; static uint32_t w25;
    static int CellOf(uint8_t* b) { for (int i = 0; i < 8; i++) if (box[i] == b) return cell[i]; return -1; }
}
HOOK_DEFINE_INLINE(ItemBoxCellHook) {                     /* 0x548024 `add x8,x23,#0x30` (dopo il ctor 0x51035c) */
    static void Callback(exl::hook::InlineCtx* ctx) {
        uint8_t* b = (uint8_t*)ctx->X[23]; uint8_t* vo = (uint8_t*)ctx->X[24];
        if (!b || !vo) return;
        int c = *(int32_t*)(vo + 0x100);
        for (int i = 0; i < 8; i++) if (ibox::box[i] == b) { ibox::cell[i] = c; return; }
        ibox::box[ibox::next] = b; ibox::cell[ibox::next] = c; ibox::next = (ibox::next + 1) & 7;
    }
};
HOOK_DEFINE_INLINE(ItemBoxSideHook) {                     /* 0x510a68 `orr w1,wzr,#1`, prima di 0x510a6c `tbnz w25,#0` */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (view8::split != 8) return;
        uint8_t* b = (uint8_t*)ctx->X[19]; int c = ibox::CellOf(b);
        if (c < 0 || c > 7) return;
        ibox::who = b; ibox::w25 = ctx->W[25];
        ctx->W[25] = ((c & 3) < 2) ? 0 : 1;
        static int logged; if (logged < 8) { logged++; Logging.Log("[mk8dx-8p] ibox: ItemBox %p cella %d k=%u -> lato %u", b, c, ibox::w25, ctx->W[25]); }
    }
};
HOOK_DEFINE_INLINE(ItemBoxSideRestoreHook) {              /* 0x510e38 `orr w3,w25,w8`: id ItemTeresa con il k originale */
    static void Callback(exl::hook::InlineCtx* ctx) {
        if (ibox::who && ibox::who == (uint8_t*)ctx->X[19]) { ctx->W[25] = ibox::w25; ibox::who = nullptr; }
    }
};

/* ==== Diagnostica dei crash su console (6 nov 2026) ====
   Su Switch reale un'eccezione della CPU passa da nn::os::detail::UserExceptionHandler (nnSdk+0x97108) al gestore predefinito
   nnSdk+0x90f38 (x0 = nn::os::UserExceptionInfo*, gia' su uno stack normale) che chiama Abort: il report di Atmosphère mostra solo il
   gestore. Hook su 0x90f38: i campi della UserExceptionInfo vanno nei registri e svcBreak. Nel report:
     X03 = pc, X04 = far, X05 = esr (u32), X06 = lr, X07 = sp, X08 = fp, X09 = descrizione errore,
     X10..X18 = x0..x8 al guasto, X19..X28 = x19..x28 al guasto (x9..x18 non riportati).
   Layout UserExceptionInfo (= ThreadExceptionDump di libnx): +0 desc, +0x10 x0..x28, +0xf8 fp, +0x100 lr, +0x108 sp, +0x110 pc,
   +0x320 pstate, +0x32c esr, +0x330 far. MK8DX_EXCDIAG=0 la spegne. */
#ifndef MK8DX_EXCDIAG
#define MK8DX_EXCDIAG 1
#endif
HOOK_DEFINE_INLINE(ExcDiagHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        register uint64_t info asm("x29") = ctx->X[0];
        if (!info) return;
        asm volatile(
            "ldr x3, [x29, #0x110]\n ldr x4, [x29, #0x330]\n ldr w5, [x29, #0x32c]\n ldr x6, [x29, #0x100]\n"
            "ldr x7, [x29, #0x108]\n ldr x8, [x29, #0xf8]\n ldr w9, [x29, #0x0]\n"
            "ldp x10, x11, [x29, #0x10]\n ldp x12, x13, [x29, #0x20]\n ldp x14, x15, [x29, #0x30]\n ldp x16, x17, [x29, #0x40]\n"
            "ldr x18, [x29, #0x50]\n"
            "ldp x19, x20, [x29, #0xa8]\n ldp x21, x22, [x29, #0xb8]\n ldp x23, x24, [x29, #0xc8]\n ldp x25, x26, [x29, #0xd8]\n"
            "ldp x27, x28, [x29, #0xe8]\n"
            "mov x0, #0\n mov x1, #0\n mov x2, #0\n svc #0x26\n 1: b 1b\n"
            :: "r"(info) : "memory");
    }
};
namespace exl::hook::nx64 { int TrampolinesUsed(); int TrampolinesMax(); }   /* src/exlaunch_patch/hook_impl.cpp */
extern "C" void exl_main(void* x0, void* x1) {
    exl::hook::Initialize();
    if (MK8DX_EXCDIAG) {
        uintptr_t sdk = 0;                                   /* nnSdk: il modulo con UserExceptionHandler (0xd0004807) a +0x97108 */
        for (int i = (int)exl::util::ModuleIndex::Start; i < (int)exl::util::ModuleIndex::End && !sdk; i++) {
            auto idx = (exl::util::ModuleIndex)i;
            if (!exl::util::HasModule(idx)) continue;
            const auto& r = exl::util::GetModuleInfo(idx).m_Total;
            if (r.m_Start && r.m_Size > 0x9710c && *reinterpret_cast<uint32_t*>(r.m_Start + 0x97108) == 0xd0004807
                && *reinterpret_cast<uint32_t*>(r.m_Start + 0x90f38) == 0xb0004849) sdk = r.m_Start;
        }
        if (sdk) ExcDiagHook::InstallAtPtr(sdk + 0x90f38);
        Logging.Log("[mk8dx-8p] diagnostica eccezioni: nnSdk=%p %s", (void*)sdk, sdk ? "hook 0x90f38 installato" : "modulo non trovato, NON installato");
    }
    uintptr_t base = mk8dx::Base();
    uint32_t i1 = *reinterpret_cast<uint32_t*>(base + mk8dx::MenuEventHandler);
    uint32_t i2 = *reinterpret_cast<uint32_t*>(base + mk8dx::SceneInitRace);
    Logging.Log("[mk8dx-8p] step2A loaded: base=%lx insn@4d92e8=%08x insn@7e76b8=%08x", base, i1, i2);
    if (i1 != mk8dx::MenuEventHandlerInsn || i2 != mk8dx::SceneInitRaceInsn) {
        Logging.Log("[mk8dx-8p] versione main non riconosciuta: nessun hook installato");
        return;
    }
    MenuEventHook::InstallAtOffset(mk8dx::MenuEventHandler);
    NvnLoaderHook::InstallAtOffset(mk8dx::NvnProcLoader);
    SceneInitRaceHook::InstallAtOffset(mk8dx::SceneInitRace);
    SceneInitHook::InstallAtOffset(mk8dx::SceneInit);
    RlmSyncHook::InstallAtOffset(mk8dx::RlmSyncFromScene);
    LayerRegisterHook::InstallAtOffset(0x667a94);
    RlmCtorHook::InstallAtOffset(0x3ca44c);
    RlmFrameHook::InstallAtOffset(0x3cc218);
    DirectorUpdateHook::InstallAtOffset(0xc8b60);
    RenderSceneCreateHook::InstallAtOffset(0x6a8f10);
    HeapCreateHook::InstallAtOffset(0x605130);              /* step 7 B: misura degli heap (solo log) */
    KartDirInitHook::InstallAtOffset(0x13d778); SettingsBuildHook::InstallAtOffset(0x869a14);            /* step 7 B: init KartDirector (log; 8 KartUnit con MK8DX_P7_MENU3D8) */
    if (MK8DX_P7_MENU3D8) {                                 /* step 7 B tappe 1-3 (report/step7b_plan.md) */
        {   /* flusso originale con N = 8: giocatori locali di default 8 (0x869180 a 0x4f9e6c/0x4fa260) e limite del conteggio del modo menu 8
               (0x86f074 e copie inline): settings+0x180 = 8, KartUnit/gestori/slot di risorse a 8 creati dal codice del gioco */
            static const struct { uint32_t off, orig, repl; } kFlow8[] = {
                {0x4f9e60, 0x321e03e1, 0x321d03e1},   /* orr w1, wzr, #4 -> #8   0x869180(rm, N) all'avvio */
                {0x86edd4, 0x321e03eb, 0x321d03eb},   /* orr w11, wzr, #4 -> #8  limite modo 5 (0x86edb4) */
                {0x86f080, 0x321e03eb, 0x321d03eb},   /* orr w11, wzr, #4 -> #8  limite modo 5 (0x86f074) */
                {0x86f4ec, 0x321e03eb, 0x321d03eb},   /* orr w11, wzr, #4 -> #8  limite modo 5 (0x86f474) */
                {0x4b865c, 0x71000edf, 0x71001edf},   /* cmp w22, #3 -> #7  ingresso selezione: nasconde le unita' [+0x8c0, 8) (report/step7b_4p_plan.md) */
                {0x4b8674, 0x710012df, 0x710022df},   /* cmp w22, #4 -> #8  (0x14a7ac(i, false)); prima il 4 fisso lasciava visibili 4..7 in 1-4P */
            };
            bool ok = true;
            for (auto& c : kFlow8) if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] m3d8: parola inattesa a %x", c.off); }
#ifndef MK8DX_P7_FLOW8
#define MK8DX_P7_FLOW8 1   /* bisezione 4P: 0 = parole del flusso a 8 non applicate */
#endif
            if (!MK8DX_P7_FLOW8) ok = false;
            if (ok) { exl::patch::RandomAccessPatcher rp; for (auto& c : kFlow8) rp.Write<uint32_t>(c.off, c.repl); }
            Logging.Log("[mk8dx-8p] m3d8: flusso a 8 %s (6 parole)", ok ? "attivo" : "NON attivo");
        }
        DrawObjHook::InstallAtOffset(0x6e2430);
        if (*reinterpret_cast<uint32_t*>(base + 0x6044a8) == 0xa9be4ff4) GfxDeviceCtorHook::InstallAtOffset(0x6044a8);   /* step 6d T11 */
        if (*reinterpret_cast<uint32_t*>(base + 0x6290bc) == 0xd101c3ff) DescRegHook::InstallAtOffset(0x6290bc);   /* step 6d M10 (solo log) */
        KartWorkSizeHook::InstallAtOffset(0x13da28); MenuKartCfgHook::InstallAtOffset(0x13e59c); UnitRebuildDiagHook::InstallAtOffset(0x16eddc);   /* 0x490828: estensione rimossa (chiamata solo all'avvio, non e' l'azzeramento della selezione) */ MenuPresetHook::InstallAtOffset(0x147738);
        VwVisHook::InstallAtOffset(0x430cc4); VwPosHook::InstallAtOffset(0x430d5c); VwRotHook::InstallAtOffset(0x430d7c);
        VwAnmHook::InstallAtOffset(0x430d8c); VwSetHook::InstallAtOffset(0x430e24); VwAnm2Hook::InstallAtOffset(0x430e58);
        Logging.Log("[mk8dx-8p] m3d8: anteprime 3D a 8 installate (KartWork 0x%x)", MK8DX_P7_KARTWORK);
    }
    RlmAttachHook::InstallAtOffset(0x3cce58);
    ModelCreateHook::InstallAtOffset(0x69bb0c);
    Layer3DPrioHook::InstallAtOffset(0x68d474);
    /* step 3: il gioco interroga 8 pad + handheld (P1: resta il set a 9 id del ctor NpadDevice 0x5fbdc8) */
    input::Patch(0x7c54ac, 0x940e3a45, 0xd503201f, "P1 SetSupportedNpadIdType(5 id) -> nop");
    input::Patch(0x7c54b0, 0x321e03e1, 0x321d03e1, "P2 pad interrogati 4 -> 8");
    ControllerMgrCalcHook::InstallAtOffset(0x6192a4);
    /* step 4: 5g-8g senza dialogo, nel ramo Multi 3/4P */
    input::Patch(0x4d9468, 0x54001708, 0x140000b8, "P5a 5g-8g: salto del dialogo");
    input::Patch(0x4d9898, 0x5400048c, 0xd503201f, "P5b 5-8 nel ramo Multi 3/4P (modo 3)");
    PadReadyHook::InstallAtOffset(0x8be248);
    ControllerAppletHook::InstallAtOffset(0x49ea60);
    /* controller di gioco 4..7: capacità lista 7 -> 12, tabelle PIM 7 -> 16, creazione e gestione voci 7..15 */
    bool p3 = input::Patch(0x7e4468, 0x32000be8, 0x52800188, "P3 capacita' lista controller 7 -> 12");
    bool p4 = input::Patch(0x7c5dc0, 0x321e0be0, 0x321a03e0, "P4a PIM+0x98 int[16]")
            & input::Patch(0x7c5dcc, 0x32000be0, 0x321c03e0, "P4b PIM+0xa0 u8[16]")
            & input::Patch(0x7c5ddc, 0x321e0be0, 0x321a03e0, "P4c PIM+0xa8 int[16]");
    if (p3 && p4) { /* H1 senza H3/P4 scriverebbe fuori dalle tabelle */
        NpadInitHook::InstallAtOffset(0x7c53ec);
        PimInitHook::InstallAtOffset(0x7c5d88);
        PimReset1Hook::InstallAtOffset(0x7c58d0);
        PimReset2Hook::InstallAtOffset(0x7c5794);
        PimFrameHook::InstallAtOffset(0x7c4e50);
    } else Logging.Log("[mk8dx-8p] input: patch P3/P4 non applicate, controller 4..7 NON creati");
    /* step 5 B1: mappa slot->entry da +0x18c a +0x19c (tutto o niente), +0x17c a 8 voci */
    {
        struct P { uintptr_t off; uint32_t orig, repl; const char* what; };
        static const P b1[] = {
            {0x868e30, 0xb9018d8b, 0xb9019d8b, "B1a riempimento 1 slot->+0x19c"}, {0x868ee4, 0xb9018d8b, 0xb9019d8b, "B1b riempimento 2"},
            {0x869684, 0xb9018d8b, 0xb9019d8b, "B1c riempimento 3 (0x869180)"},  {0x8699ec, 0xb9018d8b, 0xb9019d8b, "B1d riempimento 4 (0x869760)"},
            {0x86b940, 0xb9018d8b, 0xb9019d8b, "B1e riempimento 5 (0x86b19c)"},  {0x86b0b4, 0xb9018d01, 0xb9019d01, "B1f setter slot coda"},
            {0x88236c, 0xb9818d1a, 0xb9819d1a, "B1g lettore slot 0x8821c0"},     {0x882408, 0xb9818d15, 0xb9819d15, "B1g' lettore slot 0x8823e0"},
            {0x86ec08, 0x71000e7f, 0x71001e7f, "B1i 0x86ebf8 locale<=7"},        {0x86901c, 0x71000d1f, 0x71001d1f, "B1l 0x868fec e+0x4c<=7"},
            {0x86b970, 0x71000d1f, 0x71001d1f, "B1l 0x86b19c e+0x4c<=7"},
        };
        bool ok = true;
        for (const P& p : b1) if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok = false; Logging.Log("[mk8dx-8p] B1: parola inattesa a %lx", p.off); }
        if (ok) {
            for (const P& p : b1) input::Patch(p.off, p.orig, p.repl, p.what);
            SlotSetHook::InstallAtOffset(0x86af68);
            RaceMgrCtorHook::InstallAtOffset(0x8689b8);
            CompactLocalsHook::InstallAtOffset(0x868f00);
            RaceCfgSetupHook::InstallAtOffset(0x869180);
            race8::mapOk = true;
            /* step 7d A: raceMgr a 20 entry (report/step7d_censimento20.md "PATCH DEFINITIVE"): tutte o nessuna */
            static const P p20[] = {
                {0x868b30, 0x321903e0, 0x52801400, "7d lista entry 0x80 -> 0xa0 B"}, {0x868b44, 0x321c03e8, 0x52800288, "7d cap lista 16 -> 20"},
                {0x868cdc, 0x7100429f, 0x7100529f, "7d crea 20 entry"}, {0x868adc, 0xf100429f, 0xf100529f, "7d default 20"},
                {0x86a28c, 0xf100431f, 0xf100531f, "7d registrazione 20"}, {0x86a418, 0xf100429f, 0xf100529f, "7d ripristino 20"},
                {0x869cd8, 0xf100431f, 0xf100531f, "7d conteggio settingsB 20"}, {0x86a834, 0xf100411f, 0xf100511f, "7d slot liberi 20"},
                {0x86ebcc, 0x71003e7f, 0x71004e7f, "7d id->kart id<=19"}, {0x86eae8, 0x71003e7f, 0x71004e7f, "7d kart->GamePlayer id<=19"},
                {0x4e4a20, 0x71003e7f, 0x71004e7f, "7d nome entry id<=19"}, {0x50f258, 0x71003d1f, 0x71004d1f, "7d kart->entry id<=19"},
                {0x4278fc, 0x71003c1f, 0x71004c1f, "7d oggetto->entry id<=19"},
                {0x86af50, 0xf100413f, 0xf100513f, "7d n-esima per tipo"}, {0x869174, 0xf100411f, 0xf100511f, "7d mappa kart"},
                {0x86902c, 0xf100429f, 0xf100529f, "7d 0x868fec"}, {0x869448, 0x710042df, 0x710052df, "7d menu guardia"},
                {0x8694cc, 0x710042df, 0x710052df, "7d menu ciclo"}, {0x86953c, 0xf10042df, 0xf10052df, "7d menu slot"},
                {0x8695fc, 0xf100429f, 0xf100529f, "7d 0x869180 ripristino"}, {0x869754, 0xf100411f, 0xf100511f, "7d 0x869180"},
                {0x869894, 0xf10042df, 0xf10052df, "7d 0x869760 a"}, {0x86996c, 0xf100429f, 0xf100529f, "7d 0x869760 b"},
                {0x869fb8, 0xf100429f, 0xf100529f, "7d 0x869dec modo 4"}, {0x86a7d8, 0xf100429f, 0xf100529f, "7d rilascio GamePlayer"},
                {0x86b980, 0xf10042bf, 0xf10052bf, "7d 0x86b19c entry"}, {0x86b9f8, 0xf10042bf, 0xf10052bf, "7d 0x86b9b0"},
                {0x4a0e84, 0xf100429f, 0xf100529f, "7d risorse per entry"}, {0x53feb8, 0xf100429f, 0xf100529f, "7d UI risultati"},
                {0x3df5d4, 0xf10042bf, 0xf10052bf, "7d 0x3df4f0"}, {0x884b3c, 0xf100429f, 0xf100529f, "7d 0x884b3c"},
                {0x4f9ea4, 0xf100427f, 0xf100527f, "7d 0x4f9ccc"}, {0x4fbf00, 0x710042bf, 0x710052bf, "7d 0x4fbd7c"},
                {0x501cd4, 0xf100427f, 0xf100527f, "7d 0x501c78"},
            };
            bool ok20 = true;
            for (const P& p : p20) if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok20 = false; Logging.Log("[mk8dx-8p] 7d: parola inattesa a %lx", p.off); }
            if (ok20) { for (const P& p : p20) input::Patch(p.off, p.orig, p.repl, p.what); race8::cap20 = true; }
            else Logging.Log("[mk8dx-8p] 7d: raceMgr resta a 16 entry");
        } else Logging.Log("[mk8dx-8p] B1: nessuna patch, raceMgr resta a 4 locali");
    }
    /* step 5 B2: PIM a 8 giocatori (G1-G8); B3: 0x86a594 a 8 (solo con B1 e B2); B4: rilegature per i locali 4..7 */
    if (p3 && p4 && race8::mapOk) {
        bool g2 = input::Patch(0xca3fc, 0x52800500, 0x52800900, "G2 lista figli PIM 0x28 -> 0x48 B")
                & input::Patch(0xca410, 0x528000a8, 0x52800128, "G2 lista figli PIM cap 5 -> 9");
        if (g2) {
            InitPlayersHook::InstallAtOffset(0xca3e0);
            PimBitHook::InstallAtOffset(0x7c5340);
            PimSet8Hook::InstallAtOffset(0x7c6218);
            PimClr8Hook::InstallAtOffset(0x7c62bc);
            PimFlagsHook::InstallAtOffset(0x7c5044);
            PimOrderHook::InstallAtOffset(0x7c60c0);
            BindByLocalHook::InstallAtOffset(0x7ce38c);
            BindByOrderHook::InstallAtOffset(0x7ce8c4);
            AssignGamePlayerHook::InstallAtOffset(0x86a594);
            RebindByIndexHook::InstallAtOffset(0x86a998);
            RebindFlagSetHook::InstallAtOffset(0x86adec);
            RebindFlagClrHook::InstallAtOffset(0x86ad18);
            input::Patch(0x86ac14, 0xf100129f, 0xf100229f, "B4a 0x86aac8 locali < 8");
        } else Logging.Log("[mk8dx-8p] B2: patch G2 non applicate, giocatori 4..7 NON creati");
    } else Logging.Log("[mk8dx-8p] B2/B3 saltati (P3/P4/B1 mancanti)");
    /* step 5 C: pad UI 4..7 (12 x cmp #3 -> #7 nella famiglia pad UI, 4 x GetPad(4) -> GetPad(8) per il tocco), tutto o niente */
    {
        static const uintptr_t cmp3[] = {0x8be040, 0x8be078, 0x8be0cc, 0x8be138, 0x8be1ac, 0x8be25c, 0x8be2fc, 0x8be388, 0x8be400, 0x8be6bc, 0x8be794, 0x8beb90};
        static const uintptr_t touch4[] = {0x4b82a4, 0x4bbcdc, 0x4c17c8, 0x4c3c98};
        bool ok = race8::mapOk;
        for (uintptr_t o : cmp3)   if (*reinterpret_cast<uint32_t*>(base + o) != 0x71000c1f) { ok = false; Logging.Log("[mk8dx-8p] C1: parola inattesa a %lx", o); }
        for (uintptr_t o : touch4) if (*reinterpret_cast<uint32_t*>(base + o) != 0x321e03e0) { ok = false; Logging.Log("[mk8dx-8p] C2: parola inattesa a %lx", o); }
        if (ok) {
            for (uintptr_t o : cmp3)   input::Patch(o, 0x71000c1f, 0x71001c1f, "C1 pad UI idx <= 7");
            for (uintptr_t o : touch4) input::Patch(o, 0x321e03e0, 0x321d03e0, "C2 GetPad(4) tocco -> GetPad(8)");
            PadSetHook::InstallAtOffset(0x8bf040);
            GetPadHook::InstallAtOffset(0x8bf158);
            PadCalcAllHook::InstallAtOffset(0x8bf058);
        } else Logging.Log("[mk8dx-8p] C: pad UI 4..7 NON attivati (B1 mancante o parole inattese)");
    }
    /* step 5 A1-M1: layout 6pl/8pl (solo nome del Page_Lyt e conteggio L_Name; nessuna patch di stringhe) */
    if (*reinterpret_cast<uint32_t*>(base + 0x4be854) == 0xd10043ff && *reinterpret_cast<uint32_t*>(base + 0x4beb04) == 0x91076260) {
        CharaPartsLayoutNameHook::InstallAtOffset(0x4be854);
        CharaPartsSlotCountHook::InstallAtOffset(0x4beb04);
        if (*reinterpret_cast<uint32_t*>(base + 0x4be8d0) == 0xd10303ff) CharaPartsCreateHook::InstallAtOffset(0x4be8d0);
        sel8::a1 = true;
        Logging.Log("[mk8dx-8p] A1: layout CharaParts Multi: MK8DX_LYT8=%d", MK8DX_LYT8);
    } else Logging.Log("[mk8dx-8p] A1: parole inattese a 0x4be854/0x4beb04, layout 6pl/8pl NON attivato");
    {   /* step 5b passo 1: A1-M2 rilocazione +0x904 -> +0xa28 (75 parole + ctor), tutto o niente */
        bool ok = *reinterpret_cast<uint32_t*>(base + 0x4bc844) == 0xa9be4ff4;
        for (const sel8::P& p : sel8::kReloc904)
            if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok = false; Logging.Log("[mk8dx-8p] M2: parola inattesa a %x", p.off); }
        if (ok) {
            CharaSelBaseCtorHook::InstallAtOffset(0x4bc844);  /* prima l'hook, poi le patch */
            exl::patch::RandomAccessPatcher rp;
            for (const sel8::P& p : sel8::kReloc904) rp.Write<uint32_t>(p.off, p.repl);
            rp.Flush();
            sel8::slotOff = 0xa28; sel8::m2 = true;
            Logging.Log("[mk8dx-8p] M2: +0x904 rilocato a +0xa28 (%d parole), oggetto 0xa68", (int)(sizeof(sel8::kReloc904) / sizeof(sel8::P)));
        } else Logging.Log("[mk8dx-8p] M2: rilocazione +0x904 NON attivata");
    }
    if (sel8::m2) {   /* step 5b passo 2: richiede il passo 1 (fabbriche gia' a 0xa68), tutto o niente */
        bool ok = *reinterpret_cast<uint32_t*>(base + 0x8bf0ec) == 0xf81e0ff3 && *reinterpret_cast<uint32_t*>(base + 0x40476c) == 0xf81d0ff5
               && *reinterpret_cast<uint32_t*>(base + 0x4b28d8) == 0xb901ea68 && *reinterpret_cast<uint32_t*>(base + 0x4b7a98) == 0xf940be68;
        for (const sel8::P& p : cw8::kReloc)   if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok = false; Logging.Log("[mk8dx-8p] cw8: parola inattesa a %x", p.off); }
        for (const sel8::P& p : cw8::kFactory) if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok = false; Logging.Log("[mk8dx-8p] cw8: fabbrica inattesa a %x", p.off); }
        if (ok) {
            cw8::active = true;                                      /* prima degli hook: il ctor costruisce la coda */
            CharselLinksHook::InstallAtOffset(0x4b7a98);
            CharaListSlotHook::InstallAtOffset(0x4b28d8);
            PageGetRemapHook::InstallAtOffset(0x8bf0ec);
            MenuPagesSetupHook::InstallAtOffset(0x40476c);
            exl::patch::RandomAccessPatcher rp;
            for (const sel8::P& p : cw8::kReloc)   rp.Write<uint32_t>(p.off, p.repl);
            for (const sel8::P& p : cw8::kFactory) rp.Write<uint32_t>(p.off, p.repl);
            rp.Flush();
            Logging.Log("[mk8dx-8p] cw8: link/code rilocati (%d parole), oggetto 0xce8", (int)(sizeof(cw8::kReloc) / sizeof(sel8::P)));
        } else Logging.Log("[mk8dx-8p] cw8: passo 2 NON attivato");
    }
    /* step 5 S5.4: selezione a due turni (solo con pad UI extra: padui::extra creati da PadSetHook) */
    {
        struct W { uintptr_t off; uint32_t word; };
        static const W w[] = {{0x4b74a4, 0xd10783ff}, {0x4ba77c, 0xd10203ff}, {0x4badb0, 0xd10243ff}, {0x4c0898, 0xd106c3ff}, {0x4c2820, 0xa9bc5ff8}, {0x4c1bfc, 0xd10183ff}};
        bool ok = race8::mapOk;
        for (const W& x : w) if (*reinterpret_cast<uint32_t*>(base + x.off) != x.word) { ok = false; Logging.Log("[mk8dx-8p] S5.4: parola inattesa a %lx", x.off); }
        if (ok) {
            CharselEnterHook::InstallAtOffset(0x4b74a4); sel8::s54 = true;
            CharselFrameHook::InstallAtOffset(0x4ba77c);
            CharselStageHook::InstallAtOffset(0x4badb0);
            KartEnterHook::InstallAtOffset(0x4c0898);
            KartFrameHook::InstallAtOffset(0x4c2820);
            if (*reinterpret_cast<uint32_t*>(base + 0x8ba5fc) == 0xb940e808) PageLinkHook::InstallAtOffset(0x8ba5fc);
            if (*reinterpret_cast<uint32_t*>(base + 0x4203e0) == 0xd10243ff) SetCharaHook::InstallAtOffset(0x4203e0);
        } else Logging.Log("[mk8dx-8p] S5.4: selezione a due turni NON attivata");
    }
    /* step 6 A: array dei locali del gestore kart M a 8 (tutto o niente) */
    {
        struct P { uintptr_t off; uint32_t orig, repl; const char* what; };
        static const P a[] = {
            {0x09c190, 0x52852f00, 0x52861000, "SZ new M 0x2978 -> 0x3080"},
            {0x0ac360, 0x91028289, 0x91400e89, "A1 registrazione M+0xa0 -> +0x3000"},
            {0x0aa1b0, 0x91028276, 0x91400e76, "A2 ciclo 0xaa16c"},
            {0x0aaa24, 0x91028139, 0x91400d39, "A3 0xaa888"},
            {0x0ab4d0, 0x9102800e, 0x91400c0e, "A4 0xab4a8"},
            {0x0aad58, 0xf94052c8, 0xf95802c8, "A5 0xaad58 locale 0"},
            {0x0aad7c, 0xf94052c8, 0xf95802c8, "A6 0xaad7c locale 0"},
            {0x0aae30, 0xf94052c8, 0xf95802c8, "A7 0xaae30 locale 0"},
            {0x0b2070, 0x5280028e, 0x5280c00e, "A8 0xb1f54 base 0x14 -> 0x600"},
            {0x0b2094, 0xd1004dcf, 0xd117fdcf, "A9 0xb1f54 contatore"},
            {0x0ac374, 0x7100111f, 0x7100211f, "E1 clamp +0xe0 4 -> 8"},
            {0x0ac380, 0xb900e109, 0xb9304109, "E2 str +0xe0 -> +0x3040"},
            {0x0aa9e4, 0xf100135f, 0xf100235f, "E3 0xaa888 cmp 4 -> 8"},
            {0x0aa9f0, 0xb980e108, 0xb9b04108, "E4 0xaa888 ldrsw +0xe0 -> +0x3040"},
            {0x0aad9c, 0xb980e2ca, 0xb9b042ca, "E5 0xaad9c ldrsw +0xe0 -> +0x3040"},
        };
        bool ok = *reinterpret_cast<uint32_t*>(base + 0xa8c00) == 0xa9bd57f6 && *reinterpret_cast<uint32_t*>(base + 0xac290) == 0xf81d0ff5;
        for (const P& p : a) if (*reinterpret_cast<uint32_t*>(base + p.off) != p.orig) { ok = false; Logging.Log("[mk8dx-8p] 6A: parola inattesa a %lx", p.off); }
        if (ok) {
            for (const P& p : a) input::Patch(p.off, p.orig, p.repl, p.what);
            KartMgrCtorHook::InstallAtOffset(0xa8c00);
            KartRegisterHook::InstallAtOffset(0xac290);
            kart8::ok = true;
        } else Logging.Log("[mk8dx-8p] 6A: gestore kart NON esteso (gara con >4 locali crasha)");
    }
    /* step 6 C: viste extra disegnate nel job di 3D Sub0 */
    if (*reinterpret_cast<uint32_t*>(base + 0x683ecc) == 0xd10283ff) LayerDrawJobHook::InstallAtOffset(0x683ecc);
    /* step 6b E1: id dei segmenti per rango (ordine GPU) anche per SubDraw */
    if (*reinterpret_cast<uint32_t*>(base + 0x684148) == 0xb94cb008) LayerSubDrawJobHook::InstallAtOffset(0x684148);
    else Logging.Log("[mk8dx-8p] 6b E1: parola inattesa a 684148, SubDraw non rimappato");
    /* step 5 H-A2-0: cicli i<N della selezione personaggi protetti per gli slot 4..7 */
    ProfileToCharaWinHook::InstallAtOffset(0x3fd5a4);
    ProfileToEntriesHook::InstallAtOffset(0x3fdd70);
    if (*reinterpret_cast<uint32_t*>(base + 0x4b7218) == 0xd10243ff) CharselProfileHook::InstallAtOffset(0x4b7218);
    /* step 6 H1: HUD 6P/8P (nomi layout, partizioni); i rect dei layer 2D Sub sono in proto::PerFrame */
    if (*reinterpret_cast<uint32_t*>(base + 0x54798c) == 0xa9bf7bfd && *reinterpret_cast<uint32_t*>(base + 0x548d20) == 0xa9bf7bfd
        && *reinterpret_cast<uint32_t*>(base + 0x536a10) == 0xd10143ff) {
        RaceViewNameHook::InstallAtOffset(0x54798c);
        RaceViewFrontNameHook::InstallAtOffset(0x548d20);
        RaceCmnCreateHook::InstallAtOffset(0x536a10);
        /* H2: pagine HUD per i locali 4..7 */
        if (*reinterpret_cast<uint32_t*>(base + 0x53186c) == 0xa9be4ff4 && *reinterpret_cast<uint32_t*>(base + 0x5326dc) == 0xd10103ff
            && *reinterpret_cast<uint32_t*>(base + 0x406870) == 0xd101c3ff && *reinterpret_cast<uint32_t*>(base + 0x547d5c) == 0x51019514
            && *reinterpret_cast<uint32_t*>(base + 0x86eb84) == 0xf81e0ff3 && *reinterpret_cast<uint32_t*>(base + 0x86eb1c) == 0xf81e0ff3) {
            RacePageCtorHook::InstallAtOffset(0x53186c);
            if (*reinterpret_cast<uint32_t*>(base + 0x533368) == 0xd100c3ff) RaceMapParamHook::InstallAtOffset(0x533368);
            if (*reinterpret_cast<uint32_t*>(base + 0x8b21dc) == 0xd10103ff) PageOpenHook::InstallAtOffset(0x8b21dc);
            RacePagesSetupHook::InstallAtOffset(0x406870);
            RaceViewSlotHook::InstallAtOffset(0x547d5c);
            LocalSlotToVpHook::InstallAtOffset(0x86eb84);
            LocalSlotToKartHook::InstallAtOffset(0x86eb1c);
            /* step 7c (report/step7c_spettatore.md): quinto controller di camera per la cella vuota con 5/7 locali, tutto o niente */
            if (*reinterpret_cast<uint32_t*>(base + 0xcf040) == 0x321e03e1 && *reinterpret_cast<uint32_t*>(base + 0xce29c) == 0xf81c0ff7
                && *reinterpret_cast<uint32_t*>(base + 0xce35c) == 0xf81d0ff5 && *reinterpret_cast<uint32_t*>(base + 0xcc834) == 0xf81c0ff7) {
                CamCtrlCtorHook::InstallAtOffset(0xcc834);
                CamCtrlAttachHook::InstallAtOffset(0xce29c);
                CamCtrlDetachHook::InstallAtOffset(0xce35c);
                if (!(MK8DX_NO7CFIX & 2) && *reinterpret_cast<uint32_t*>(base + 0xcdd18) == 0xd102c3ff) CamCtrlSphereHook::InstallAtOffset(0xcdd18);
                else Logging.Log("[mk8dx-8p] 7c: parola inattesa a 0xcdd18");
                if (MK8DX_NO7CFIX & 1) Logging.Log("[mk8dx-8p] prova: correzioni 7c spente, maschera %d", MK8DX_NO7CFIX);
                else if (*reinterpret_cast<uint32_t*>(base + 0x1088c8) == 0xf9403808 && *reinterpret_cast<uint32_t*>(base + 0x1088fc) == 0x17fffea3)
                    ScreenFxFlagHook::InstallAtOffset(0x1088c8);
                else Logging.Log("[mk8dx-8p] 7c: parole inattese a 0x1088c8");
                if (*reinterpret_cast<uint32_t*>(base + 0x86ca88) == 0x71000d1f && *reinterpret_cast<uint32_t*>(base + 0x86ca84) == 0xb9418408
                    && *reinterpret_cast<uint32_t*>(base + 0x86cbc8) == 0x320007e1 && *reinterpret_cast<uint32_t*>(base + 0x86cbe4) == 0x17fda02c)
                    SpectAutoHook::InstallAtOffset(0x86ca88);
                else Logging.Log("[mk8dx-8p] 7c: parole inattese a 0x86ca88");
                input::Patch(0xcf040, 0x321e03e1, 0x528000a1, "7c P1 0xcf040 controller di camera 4 -> 5");
            } else Logging.Log("[mk8dx-8p] 7c: parole inattese, controller spettatore NON attivato");
            /* step 6b H6b: colore e aggiornamenti per oggetto delle celle 5-8 (tutto o niente) */
            struct W { uintptr_t off; uint32_t word; };
            static const W h6[] = {{0x86a720, 0x7100129f}, {0x5350a8, 0xb9435268}, {0x534848, 0xf9417668}, {0x53467c, 0x91088261},
                                   {0x5345e0, 0xf941ae68}, {0x5349a8, 0xf9401108}, {0x53635c, 0xf81d0ff5}};
            bool ok6 = true;
            for (const W& x : h6) if (*reinterpret_cast<uint32_t*>(base + x.off) != x.word) { ok6 = false; Logging.Log("[mk8dx-8p] H6b: parola inattesa a %lx", x.off); }
            if (ok6) {
                {   /* step 5b passo 8: palette a 8 (report/step5b_plan_p8.md), tutto o niente; altrimenti colori di P1-P4 (0x86a720) */
                    struct W8 { uintptr_t off; uint32_t word; };
                    static const W8 p8[] = {{0x86a770, 0xb9000268}, {0x420df4, 0x320007e8}, {0x420df0, 0xb9400bf4}, {0x420e60, 0x11000694},
                                            {0x513ca8, 0x1e230100}, {0x513c48, 0x7100111f}, {0x513cac, 0x79c3aa68},
                                            {0x520830, 0x7100011f}, {0x520834, 0x1a88b694}, {0x5057a8, 0x71001d1f}, {0x5057b0, 0xf9403a68}};
                    bool okp = MK8DX_PAL8;
                    for (const W8& x : p8) if (*reinterpret_cast<uint32_t*>(base + x.off) != x.word) { okp = false; Logging.Log("[mk8dx-8p] pal8: parola inattesa a %lx", x.off); }
                    if (okp) {
                        PlayerColor8Hook::InstallAtOffset(0x86a770); RowColorP8Hook::InstallAtOffset(0x420df4);
                        RankColorP8Hook::InstallAtOffset(0x513ca8); PauseRowColorP8Hook::InstallAtOffset(0x520830);
                        TotalResultLoopP8Hook::InstallAtOffset(0x5057a8);
                        Logging.Log("[mk8dx-8p] pal8: palette a 8 attiva (P5-P8 rosso, blu, viola, arancio)");
                    } else PlayerColorIdxHook::InstallAtOffset(0x86a720);
                }
                RaceVpFrameHook::InstallAtOffset(0x5350a8);
                RaceFinishReduceHook::InstallAtOffset(0x534848);
                RaceHideObjsHook::InstallAtOffset(0x53467c);
                RaceResetObjsHook::InstallAtOffset(0x5345e0);
                if (*reinterpret_cast<uint32_t*>(base + 0x551a08) == 0x940db5c5 && *reinterpret_cast<uint32_t*>(base + 0x551a0c) == 0x940db83c
                    && *reinterpret_cast<uint32_t*>(base + 0x52682c) == 0x940e623c && *reinterpret_cast<uint32_t*>(base + 0x526830) == 0x940e64b3) {
                    HudHandleCellHook::InstallAtOffset(0x551a0c); HudHandlePartHook::InstallAtOffset(0x526830);
                } else Logging.Log("[mk8dx-8p] 7e: parole inattese a 0x551a0c/0x526830");
                RaceObjCalcHook::InstallAtOffset(0x5349a8);
                RaceAlarmHook::InstallAtOffset(0x53635c);
            } else Logging.Log("[mk8dx-8p] H6b: HUD celle 5-8 NON corretto");
            if (*reinterpret_cast<uint32_t*>(base + 0x10f198) == 0x7100111f && *reinterpret_cast<uint32_t*>(base + 0x7e9b04) == 0xa9ba6ffc
                && *reinterpret_cast<uint32_t*>(base + 0x7e9a68) == 0xb9403808) {
                LakituHideGateHook::InstallAtOffset(0x10f198);
                LakituAssignHook::InstallAtOffset(0x7e9b04);
                if (*reinterpret_cast<uint32_t*>(base + 0x7e9d40) == 0xd10103ff && *reinterpret_cast<uint32_t*>(base + 0x1101f0) == 0x941b66d4
                    && *reinterpret_cast<uint32_t*>(base + 0x110298) == 0xf81d0ff5) LakituCreateHook::InstallAtOffset(0x7e9d40);
                else Logging.Log("[mk8dx-8p] H6b-L3: parole inattese, creazione Lakitu 5-8 NON attivata");
                {
                    static const uintptr_t kLakMode[5] = { 0x7e8bd0, 0x7e8c8c, 0x7e8df0, 0x7e8eb0, 0x7e92b8 };   /* cmp w8,#1 */
                    static const uintptr_t kLakLdr[5] = { 0x7e8bc4, 0x7e8c80, 0x7e8de8, 0x7e8ea0, 0x7e92b4 };    /* ldr w8,[x0,#0xcc] */
                    bool okm = true;
                    for (int i = 0; i < 5; i++)
                        okm = okm && *reinterpret_cast<uint32_t*>(base + kLakMode[i]) == 0x7100051f && *reinterpret_cast<uint32_t*>(base + kLakLdr[i]) == 0xb940cc08;
                    if (okm) {
                        LakituModeHook0::InstallAtOffset(kLakMode[0]); LakituModeHook1::InstallAtOffset(kLakMode[1]);
                        LakituModeHook2::InstallAtOffset(kLakMode[2]); LakituModeHook3::InstallAtOffset(kLakMode[3]);
                        LakituModeHook4::InstallAtOffset(kLakMode[4]);
                    } else Logging.Log("[mk8dx-8p] 6b: parole inattese, posizione Lakitu 8P NON corretta");
                }
            } else Logging.Log("[mk8dx-8p] H6b-L: parole inattese, Lakitu 5-8 NON attivato");
        } else Logging.Log("[mk8dx-8p] H2: parole inattese, HUD celle 5-8 NON attivato");
    } else Logging.Log("[mk8dx-8p] H1: parole inattese, HUD 6P/8P NON attivato");
    if (*reinterpret_cast<uint32_t*>(base + 0x3fe040) == 0xd10283ff) AidsSaveHook::InstallAtOffset(0x3fe040);
    else Logging.Log("[mk8dx-8p] step7: parola inattesa a 3fe040, salvataggio aiuti 5-8 NON protetto");
    if (*reinterpret_cast<uint32_t*>(base + 0x50efa8) == 0xfc1e0fe8) RaceViewSideHook::InstallAtOffset(0x50efa8);
    if (*reinterpret_cast<uint32_t*>(base + 0x548024) == 0x9100c2e8 && *reinterpret_cast<uint32_t*>(base + 0x548020) == 0x97ff20cf
        && *reinterpret_cast<uint32_t*>(base + 0x510a68) == 0x320003e1 && *reinterpret_cast<uint32_t*>(base + 0x510a6c) == 0x370008d9
        && *reinterpret_cast<uint32_t*>(base + 0x510e38) == 0x2a080323) {
        ItemBoxCellHook::InstallAtOffset(0x548024);
        ItemBoxSideHook::InstallAtOffset(0x510a68);
        ItemBoxSideRestoreHook::InstallAtOffset(0x510e38);
    } else Logging.Log("[mk8dx-8p] ibox: parole inattese, lato del riquadro oggetti NON corretto");
    /* step 7: pausa a 8 (tutto o niente) */
    if (*reinterpret_cast<uint32_t*>(base + 0x53c818) == 0xa9bf7bfd && *reinterpret_cast<uint32_t*>(base + 0x53d68c) == 0x710012ff
        && *reinterpret_cast<uint32_t*>(base + 0x53d3a8) == 0xa90ba3fc && *reinterpret_cast<uint32_t*>(base + 0x53d678) == 0xf90063e8
        && *reinterpret_cast<uint32_t*>(base + 0x51bc10) == 0xd10143ff && *reinterpret_cast<uint32_t*>(base + 0x53e404) == 0x320007e8
        && *reinterpret_cast<uint32_t*>(base + 0x8b4038) == 0xf81e0ff3) {
        PauseLayoutNameHook::InstallAtOffset(0x53c818);
        input::Patch(0x53d68c, 0x710012ff, 0x710022ff, "step7 P2 pannelli pausa 4 -> 8");
        PauseBtnsNameCreateHook::InstallAtOffset(0x53d3a8);
        PauseBtnsNameHideHook::InstallAtOffset(0x53d678);
        PauseSgBindHook::InstallAtOffset(0x51bc10);
        PauseAssignExtraHook::InstallAtOffset(0x53e404);
        BtnScanHook::InstallAtOffset(0x8b4038);
        pause8::active = true;
        if (*reinterpret_cast<uint32_t*>(base + 0x8b0084) == 0xf81e0ff3) { CursorPollHook::InstallAtOffset(0x8b0084); pause8::pollHooked = true; }
        else Logging.Log("[mk8dx-8p] step7: parola inattesa a 8b0084, cursore pausa 5-8 NON attivato");
    } else Logging.Log("[mk8dx-8p] step7: parole inattese, pausa a 8 NON attivata");
    {   /* step5b passo 3: selettore cursori a 8 slot virtuali (report/step5b_plan_p3.md) */
        bool ok = true;
        for (const cur8::C& c : cur8::kClampCmp) if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] cur8: parola inattesa a %x", c.off); }
        for (const cur8::C& c : cur8::kWords)    if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] cur8: parola inattesa a %x", c.off); }
        for (const cur8::H& h : cur8::kHookWords) {
            if (h.off == 0x4bbd18 && !MK8DX_P3TEST) continue;
            if (h.off == 0x8b0084 && pause8::pollHooked) continue;                       /* gia' agganciato da CursorPollHook */
            if (*reinterpret_cast<uint32_t*>(base + h.off) != h.word) { ok = false; Logging.Log("[mk8dx-8p] cur8: hook inatteso a %x", h.off); }
        }
        if (ok) {
            exl::patch::RandomAccessPatcher rp;                                          /* 1) parole: cmp #8, dimensioni, cmp x25 #7, nop */
            for (const cur8::C& c : cur8::kClampCmp) rp.Write<uint32_t>(c.off, c.repl);
            for (const cur8::C& c : cur8::kWords)    rp.Write<uint32_t>(c.off, c.repl);
            rp.Flush();                                                                  /* PRIMA degli hook inline sugli stessi indirizzi */
            Cur8ClampF1::InstallAtOffset(0x8ae1e4);  Cur8ClampF2::InstallAtOffset(0x8ae228);  Cur8ClampF3::InstallAtOffset(0x8ae274);
            Cur8ClampF4::InstallAtOffset(0x8ae334);  Cur8ClampF5::InstallAtOffset(0x8ae64c);  Cur8ClampF6::InstallAtOffset(0x8af474);
            Cur8ClampF7::InstallAtOffset(0x8af490);  Cur8ClampF8::InstallAtOffset(0x8afb48);  Cur8ClampF9::InstallAtOffset(0x8afc1c);
            Cur8ClampF10::InstallAtOffset(0x8aff70); Cur8ClampF11::InstallAtOffset(0x8affcc); Cur8ClampF12::InstallAtOffset(0x8b0040);
            Cur8ClampF13::InstallAtOffset(0x8b0108); Cur8ClampF14::InstallAtOffset(0x8b0278); Cur8ClampF15::InstallAtOffset(0x8b08a4);
            Cur8ClampG1::InstallAtOffset(0x8ba7a4);  Cur8ClampG2::InstallAtOffset(0x8ba828);  Cur8ClampG3::InstallAtOffset(0x443590);
            Cur8ClampG4::InstallAtOffset(0x49dfbc);  Cur8ClampG5::InstallAtOffset(0x49abdc);
            Cur8GfxStoreHook::InstallAtOffset(0x49a9c0);                                 /* istruzione spostata = nop */
            Cur8MemA::InstallAtOffset(0x8af5d0); Cur8MemB::InstallAtOffset(0x8af6d8); Cur8MemC::InstallAtOffset(0x8afc28);
            Cur8FocP::InstallAtOffset(0x8b0114); Cur8FocD::InstallAtOffset(0x8b0284);
            Cur8ExitWaitHook::InstallAtOffset(0x8bb900); Cur8IdleWaitHook::InstallAtOffset(0x8bbc2c);
            Cur8DecidedHook::InstallAtOffset(0x8bbaa4);  Cur8PageFocusHook::InstallAtOffset(0x8bb568);
            Cur8CtorHook::InstallAtOffset(0x8af218);     Cur8ResetHook::InstallAtOffset(0x8af308);
            Cur8RestoreHook::InstallAtOffset(0x8afc54);  Cur8ActivateHook::InstallAtOffset(0x8afdd0); CwDecideHook::InstallAtOffset(0x8affa4);
            if (*reinterpret_cast<uint32_t*>(base + 0x49bf0c) == 0xa9417bfd && *reinterpret_cast<uint32_t*>(base + 0x49c1ac) == 0xf94ada68) {
                DialogOpen8Hook::InstallAtOffset(0x49bf0c); DialogClose8Hook::InstallAtOffset(0x49c1ac);
            } else Logging.Log("[mk8dx-8p] cur8: parole inattese a 49bf0c/49c1ac, Dialog resta a 4 pad");
            if (*reinterpret_cast<uint32_t*>(base + 0x8b3c0c) == 0xf9400e60) PageGroupUnload8Hook::InstallAtOffset(0x8b3c0c);
            else Logging.Log("[mk8dx-8p] cur8: parola inattesa a 8b3c0c, scaricamento gruppo pagine a 4 pad");
            Cur8AnyHook::InstallAtOffset(0x8ae2a4);      Cur8DrawHook::InstallAtOffset(0x8b01ec);
            Cur8FocusFrameHook::InstallAtOffset(0x8bd6d8); Cur8SetFocusHook::InstallAtOffset(0x8bd5f8);
            Cur8GfxCreateHook::InstallAtOffset(0x49a8fc); Cur8GfxLoopHook::InstallAtOffset(0x49aa1c);
            Cur8GfxBusyHook::InstallAtOffset(0x49ab04);   Cur8GfxEnableHook::InstallAtOffset(0x49ab68);
            if (!pause8::pollHooked) { CursorPollHook::InstallAtOffset(0x8b0084); pause8::pollHooked = true; }
#if MK8DX_P3TEST
            Cur8TestEventHook::InstallAtOffset(0x4bbd18);
#endif
            for (auto& r : cur8::mem8) for (auto& v : r) v = -1;                    /* nessuna memoria per gli slot 4..7 */
            cur8::on = true;
            Logging.Log("[mk8dx-8p] cur8: passo 3 attivo (cursore 0x1a8, cm_Page_Cursor 0x230, 20 clamp, test=%d)", MK8DX_P3TEST);
        } else Logging.Log("[mk8dx-8p] cur8: passo 3 NON attivato");
    }
    /* step5b passo 4: richiede passi 1-3, A1-M1 (hook 0x4be854/0x4beb04/0x4be8d0) e S5.4 (CharselEnterHook) installati */
    if (MK8DX_SEL1ROUND && !MK8DX_P3TEST && sel8::m2 && cw8::active && cur8::on && sel8::s54 && sel8::a1) {
        bool ok = true;
        for (const p4::C& c : p4::kClamp)    if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] p4: parola inattesa a %x", c.off); }
        for (const p4::C& c : p4::kNames)    if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] p4: parola inattesa a %x", c.off); }
        for (const p4::C& c : p4::kFactory)  if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] p4: fabbrica inattesa a %x", c.off); }
        for (const p4::A& a : p4::kCfgAdd)   if (*reinterpret_cast<uint32_t*>(base + a.off) != a.word) { ok = false; Logging.Log("[mk8dx-8p] p4: add inattesa a %x", a.off); }
        for (const p4::H& h : p4::kHookWords) {                       /* le prime 9 voci sono nuove; le altre sono gia' agganciate */
            if (&h - p4::kHookWords >= 9) break;
            if (*reinterpret_cast<uint32_t*>(base + h.off) != h.word) { ok = false; Logging.Log("[mk8dx-8p] p4: hook inatteso a %x", h.off); }
        }
        if (ok) {
            p4::on = true;                                            /* prima della costruzione del menu: ctor e registrazione */
            exl::patch::RandomAccessPatcher rp;                        /* 1) parole: 50 clamp, 19 +0x728 -> +0xce8, 3 fabbriche 0xcf0 */
            for (const p4::C& c : p4::kClamp)   rp.Write<uint32_t>(c.off, c.repl);
            for (const p4::C& c : p4::kNames)   rp.Write<uint32_t>(c.off, c.repl);
            for (const p4::C& c : p4::kFactory) rp.Write<uint32_t>(c.off, c.repl);
            rp.Flush();
            for (const p4::A& a : p4::kCfgAdd) {                       /* 2) 38 hook inline sulle add della config */
                if (a.base == 8 && a.idx == 9)       Cfg8AddB8I9::InstallAtOffset(a.off);
                else if (a.base == 8 && a.idx == 27) Cfg8AddB8I27::InstallAtOffset(a.off);
                else if (a.base == 8 && a.idx == 28) Cfg8AddB8I28::InstallAtOffset(a.off);
                else if (a.base == 8 && a.idx == 21) Cfg8AddB8I21::InstallAtOffset(a.off);
                else if (a.base == 9 && a.idx == 21) Cfg8AddB9I21::InstallAtOffset(a.off);
            }
            P4SlotFixGift::InstallAtOffset(0x4b1d40); P4SlotFixEnter::InstallAtOffset(0x4b885c);
            P4SlotFixVt93::InstallAtOffset(0x4ba460); P4SlotFixVt93::InstallAtOffset(0x4ba600);
            CharaWinPosHook::InstallAtOffset(0x469760); NamesCreateIdHook::InstallAtOffset(0x4be8fc);
            CharselExitHook::InstallAtOffset(0x4bb6a0); Model3DSlotHook::InstallAtOffset(0x13d424); Model3DReadyHook::InstallAtOffset(0x14899c); if (!MK8DX_P3TEST) SelEventDiagHook::InstallAtOffset(0x4bbd18);
            if (MK8DX_P7_MENU3D8) { CacheDriverHook::InstallAtOffset(0x88e6f4); CacheBodyHook::InstallAtOffset(0x88f5b4); CacheWingHook::InstallAtOffset(0x890374); CacheArmHook::InstallAtOffset(0x890944);
                CacheTireSubHook::InstallAtOffset(0x88ecfc); CacheEmblemHook::InstallAtOffset(0x88f0c0); Cache71f0Hook::InstallAtOffset(0x88fb0c); CacheTireHook::InstallAtOffset(0x88fecc);
                MemBgLoadHook::InstallAtOffset(0x881ce8); MemStableHook::InstallAtOffset(0x3da428); MemCourseHook::InstallAtOffset(0x3d5794); MemRingHook::InstallAtOffset(0x884bd8);
                if (MK8DX_P7_REC16) {   /* step 7 B S1-S3 (report/step7b_records_plan.md §3): liste di modelli a 16 record, tutto o niente */
                    static const struct { uint32_t off, orig, repl; } kRec[] = {
                        {0x3de9f4, 0x52934a00, 0x52969e00},   /* mov w0,#0x9a50 -> #0xb4f0   new di sys */
                        {0x8ac240, 0x52934a00, 0x52969e00},   /* mov w0,#0x9a50 -> #0xb4f0   new di sys */
                        {0x3d57b8, 0x52a04f20, 0x52a06a00},   /* mov w0,#0x2790000 -> #0x3500000 (+movk 0xe000): DriverBodySE 0x350e000 */
                        {0x881d10, 0x52a27600, 0x52a2ea00},   /* mov w0,#0x13b00000 -> #0x17500000: BackgroundLoad */
                        {0x3d57f0, 0x52a1c800, 0x52a20000},   /* S4: mov w0,#0xe400000 -> #0x10000000 (+movk 0x4000): CourseResource 0x10004000 */
                    };
                    static const struct { uint32_t off, word; } kSite[] = {   /* le 8 ricerche sono gia' agganciate sopra (Cache*Hook) */
                        {0x880c8c, 0xd10203ff}, {0x3dc4f0, 0xd100c3ff}, {0x3dc6ec, 0xd100c3ff}, {0x3dc8e8, 0xd10103ff}, {0x3dcb0c, 0xd100c3ff},
                        {0x3dcd08, 0xd10103ff}, {0x3dcf2c, 0xd10103ff}, {0x88e63c, 0xf9683d08}, {0x88f4f4, 0xf97c0d08}, {0x8902b4, 0xf8696908},
                        {0x89087c, 0xf8696908}, {0x88ea40, 0xb9400109}, {0x88f92c, 0xb9400109}, {0x8906ec, 0xb9400109}, {0x890d14, 0xb9400109},
                        {0x88f018, 0xf9002e7f}, {0x88f3f0, 0xf900527f}, {0x88fdc0, 0xf9002a7f}, {0x8901b4, 0xf900527f}, {0x884ae8, 0xaa1f03f4},
                        {0x3dc068, 0x294257f4}, {0x3dc0bc, 0xb9400274}, {0x3dc128, 0xb9400674}, {0x3dc180, 0x29415674}, {0x3dc1e8, 0x29405674},
                        {0x3dc308, 0x910023e0}, {0x884f2c, 0xd10383ff}, {0x8852b8, 0xd10383ff}, {0x881d80, 0xa9be4ff4}, {0x881fa0, 0xa9be4ff4}, {0x89d668, 0xd101c3ff}, {0x89dae4, 0xd101c3ff},
                    };
                    bool ok = true;
                    for (auto& c : kRec) if ((MK8DX_P7_RING9 || c.off != 0x3d57f0) && *reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { ok = false; Logging.Log("[mk8dx-8p] rec16: parola inattesa a %x", c.off); }
                    for (auto& c : kSite) if (*reinterpret_cast<uint32_t*>(base + c.off) != c.word) { ok = false; Logging.Log("[mk8dx-8p] rec16: sito inatteso a %x", c.off); }
                    if (ok) {
                        exl::patch::RandomAccessPatcher rp; for (auto& c : kRec) if ((MK8DX_P7_RING9 || c.off != 0x3d57f0)) rp.Write<uint32_t>(c.off, c.repl);
                        rec16::sizeOk = true;
                        RecCtorHook::InstallAtOffset(0x880c8c);
                        {
                        RecPreDriverHook::InstallAtOffset(0x3dc4f0); RecPreEmblemHook::InstallAtOffset(0x3dc6ec); RecPreBodyHook::InstallAtOffset(0x3dc8e8);
                        RecPreTireHook::InstallAtOffset(0x3dcb0c); RecPreWingHook::InstallAtOffset(0x3dcd08); RecPreArmHook::InstallAtOffset(0x3dcf2c);
                        RecIdxDriverHook::InstallAtOffset(0x88e63c); RecIdxBodyHook::InstallAtOffset(0x88f4f4); RecIdxWingHook::InstallAtOffset(0x8902b4);
                        RecIdxArmHook::InstallAtOffset(0x89087c); RecRelDriverHook::InstallAtOffset(0x88ea40); RecRelBodyHook::InstallAtOffset(0x88f92c);
                        RecRelWingHook::InstallAtOffset(0x8906ec); RecRelArmHook::InstallAtOffset(0x890d14);
                        RecPtrRTireSubHook::InstallAtOffset(0x88f018); RecPtrREmblemHook::InstallAtOffset(0x88f3f0); RecPtrR71f0Hook::InstallAtOffset(0x88fdc0);
                        RecPtrRTireHook::InstallAtOffset(0x8901b4); RecUsedClrHook::InstallAtOffset(0x884ae8);
                        RecUsedDriverHook::InstallAtOffset(0x3dc068); RecUsedEmblemHook::InstallAtOffset(0x3dc0bc); RecUsedBodyHook::InstallAtOffset(0x3dc128);
                        RecUsedTireHook::InstallAtOffset(0x3dc180); RecUsedWingHook::InstallAtOffset(0x3dc1e8); RecUsedArmHook::InstallAtOffset(0x3dc308);
                        RecRingFillBodyHook::InstallAtOffset(0x884f2c); RecRingFillWingHook::InstallAtOffset(0x8852b8);
                        RecRingDownBodyHook::InstallAtOffset(0x881d80); RecRingDownWingHook::InstallAtOffset(0x881fa0);
                        if (MK8DX_P7_RING9) { RingPart0BodyHook::InstallAtOffset(0x89d668); RingPart0WingHook::InstallAtOffset(0x89dae4); }
                        }
                    }
                    Logging.Log("[mk8dx-8p] rec16: liste a 16 record %s (4 parole, 30 hook + 8 ricerche), anello a 9 %s", ok ? "attive" : "NON attive", ok && MK8DX_P7_RING9 ? "attivo" : "spento");
                } } MatFindHook::InstallAtOffset(0x68fffc); MatUseHook::InstallAtOffset(0x11dc0c); BtnPlayerHook::InstallAtOffset(0x43ce4c);
            Logging.Log("[mk8dx-8p] p4: selezione a turno unico pronta (50 clamp, 19 nomi, 38 cfg8, oggetto 0xcf0, pagina nomi 0x%x)", p4::kNamesId);
        } else Logging.Log("[mk8dx-8p] p4: passo 4 NON attivato, selezione a due turni");
    }
    if (p4::on) {   /* step5b passo 5: kart a 8 slot a turno unico (report/step5b_plan_p5.md), tutto o niente */
        bool ok = true;
        auto chk = [&](const p5::C* t, size_t n) { for (size_t i = 0; i < n; i++) if (*reinterpret_cast<uint32_t*>(base + t[i].off) != t[i].orig) { ok = false; Logging.Log("[mk8dx-8p] p5: parola inattesa a %x", t[i].off); } };
        auto put = [&](exl::patch::RandomAccessPatcher& rp, const p5::C* t, size_t n) { for (size_t i = 0; i < n; i++) rp.Write<uint32_t>(t[i].off, t[i].repl); };
#define P5N(a) (sizeof(a) / sizeof(a[0]))
        chk(p5::kClampKart, P5N(p5::kClampKart)); chk(p5::kClampKart2, P5N(p5::kClampKart2)); chk(p5::kReloc460, P5N(p5::kReloc460));
        chk(p5::kFactory, P5N(p5::kFactory)); chk(p5::kPartsCursor, P5N(p5::kPartsCursor));
        chk(p5::kFix, P5N(p5::kFix)); chk(p5::kTexQueue, P5N(p5::kTexQueue));
        for (uint32_t o : {0x476630u, 0x477408u, 0x477e40u}) if (*reinterpret_cast<uint32_t*>(base + o) != 0x8b090908) { ok = false; Logging.Log("[mk8dx-8p] p5fix: add inattesa a %x", o); }
        if (*reinterpret_cast<uint32_t*>(base + 0x4553fc) != 0x8b080e73 || *reinterpret_cast<uint32_t*>(base + 0x455474) != 0x8b080e68) { ok = false; Logging.Log("[mk8dx-8p] p7b: parole inattese a 4553fc/455474"); }
        if (*reinterpret_cast<uint32_t*>(base + 0x46c10c) != 0x8b090e89 || *reinterpret_cast<uint32_t*>(base + 0x4c3360) != 0xd10143ff) { ok = false; Logging.Log("[mk8dx-8p] p5fix: hook inatteso"); }
        for (const p5::H& h : p5::kHookWords) if (*reinterpret_cast<uint32_t*>(base + h.off) != h.word) { ok = false; Logging.Log("[mk8dx-8p] p5: hook inatteso a %x", h.off); }
        if (ok) {
            p5::on = true;
            exl::patch::RandomAccessPatcher rp;
            put(rp, p5::kClampKart, P5N(p5::kClampKart)); put(rp, p5::kClampKart2, P5N(p5::kClampKart2)); put(rp, p5::kReloc460, P5N(p5::kReloc460));
            put(rp, p5::kFactory, P5N(p5::kFactory)); put(rp, p5::kPartsCursor, P5N(p5::kPartsCursor)); put(rp, p5::kFix, P5N(p5::kFix)); put(rp, p5::kTexQueue, P5N(p5::kTexQueue));
            rp.Flush();
            Cfg8AddB8I9::InstallAtOffset(0x476630); Cfg8AddB8I9::InstallAtOffset(0x477408); Cfg8AddB8I9::InstallAtOffset(0x477e40);
            CwGfxCursorHook::InstallAtOffset(0x46c10c); CwCellGfxHook::InstallAtOffset(0x4553fc); CwCellGfxHook::InstallAtOffset(0x455474); KartPreExitHook::InstallAtOffset(0x4c3360);
            KartBuildCountHook::InstallAtOffset(0x4bfdf4);
            P5SlotFixPage::InstallAtOffset(0x4c192c); P4SlotFixEnter::InstallAtOffset(0x4c1b08);
            P4SlotFixGift::InstallAtOffset(0x4c2bd8); P4SlotFixEv::InstallAtOffset(0x4c3dd0); P4SlotFixGift::InstallAtOffset(0x4c3e4c);
            Cfg8AddB8I9::InstallAtOffset(0x4c1dec); Cfg8AddB8I9::InstallAtOffset(0x4c1ea0); Cfg8AddB8I9::InstallAtOffset(0x4c1f54);
            PcCreateHook::InstallAtOffset(0x49ad98); PcIndexHook::InstallAtOffset(0x49ae34);
            P5PcFixCancel::InstallAtOffset(0x4c2ba4); P5PcFixConfirm::InstallAtOffset(0x4c2fa0);
            KartCreateHook::InstallAtOffset(0x4c4c84); KartExitHook::InstallAtOffset(0x4c3564);
            Model3DModeHook::InstallAtOffset(0x14a038); KpLoopHook::InstallAtOffset(0x49ae90);
            Logging.Log("[mk8dx-8p] p5: kart a 8 pronta (%d parole, kart 0x4e0, PartsCursor 0x230)",
                        (int)(P5N(p5::kClampKart) + P5N(p5::kClampKart2) + P5N(p5::kReloc460) + P5N(p5::kFactory) + P5N(p5::kPartsCursor)));
            /* step 7f: pagina Present a 8 regali (tutte le parole o nessuna; serve la rilocazione +0x904 -> +0xa28 per i flag 4..7) */
            bool g8 = sel8::m2 && MK8DX_GIFT8;
            for (const gift8::C& c : gift8::kTable) if (*reinterpret_cast<uint32_t*>(base + c.off) != c.orig) { g8 = false; Logging.Log("[mk8dx-8p] 7f: parola inattesa a %x", c.off); }
            static const p5::H kGiftHooks[] = { {0x4b0e20, 0xd10183ff}, {0x4b11b0, 0xb94a4109}, {0x4b140c, 0x3908267f}, {0x4b1d00, 0x39482268} };
            for (const p5::H& h : kGiftHooks) if (*reinterpret_cast<uint32_t*>(base + h.off) != h.word) { g8 = false; Logging.Log("[mk8dx-8p] 7f: hook inatteso a %x", h.off); }
            if (g8) {
                exl::patch::RandomAccessPatcher rg;
                for (const gift8::C& c : gift8::kTable) rg.Write<uint32_t>(c.off, c.repl);
                rg.Flush();
                Gift8EnterHook::InstallAtOffset(0x4b0e20); Gift8FlagsHook::InstallAtOffset(0x4b11b0);
                Gift8FocusHook::InstallAtOffset(0x4b140c); Gift8KartHook::InstallAtOffset(0x4b1d00);
                gift8::on = true;
                Logging.Log("[mk8dx-8p] 7f: pagina Present a 8 regali (%d parole, oggetto 0x5f0, 4 hook)", (int)(sizeof(gift8::kTable) / sizeof(gift8::C)));
            } else Logging.Log("[mk8dx-8p] 7f: pagina Present resta a 4 regali");
        } else Logging.Log("[mk8dx-8p] p5: passo 5 NON attivato, kart a due turni");
#undef P5N
    }
    if (MK8DX_SEL8B && p4::on) {   /* step 5c opzione B: selezione legata alla 0x12 (set B) scelto in 0x8bb3e8; anteprime 3D legate alla pagina aperta */
        if (*reinterpret_cast<uint32_t*>(base + 0x8bb3e8) == 0xf81c0ff7 && *reinterpret_cast<uint32_t*>(base + 0x4b5688) == 0xd10243ff
            && *reinterpret_cast<uint32_t*>(base + 0x8bafb8) == 0xf81d0ff5) {
            PageEnterHook::InstallAtOffset(0x8bb3e8);
            sel8b::on = true;
            Logging.Log("[mk8dx-8p] sel8b: opzione B attiva (PageEnterHook 0x8bb3e8, set B della selezione sulla 0x12, 3D legato alle pagine aperte)");
        } else Logging.Log("[mk8dx-8p] sel8b: parole inattese a 0x8bb3e8/0x4b5688/0x8bafb8, opzione B NON attiva");
    }
    Logging.Log("[mk8dx-8p] hook installati: menu, nvn loader, scene init (gara/generico), RLM sync, step 5 B");
    Logging.Log("[mk8dx-8p] trampolini usati %d / %d", exl::hook::nx64::TrampolinesUsed(), exl::hook::nx64::TrampolinesMax());
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("Default exception handler called!");
}
