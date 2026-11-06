#pragma once

#include <atomic>
#include <utility>
#include <lib/log/svc_logger.hpp>
#include <common.hpp>

/* Specify logger implementations here. */
inline exl::log::LoggerMgr<
    exl::log::SvcLogger
> LoggingImpl;

/* 5 nov 2026: Logging.Log e' chiamato da piu' thread (principale, rendering, caricamento). Lo snprintf di newlib condivide un solo
   stato globale (_impure_ptr): con "%f" _dtoa_r usa la freelist dei bigint (_Balloc/_Bfree), che senza lock si corrompeva
   (letture a 0x300000 in _Balloc, viste con lo stub GDB di Eden; su Switch reale crash). Spinlock attorno a ogni riga.
   5 nov sera: l'attesa con svcSleepThread(0) cede il core solo ai thread di PARI priorita': MainThread in attesa e Prepare Thread
   (priorita' piu' bassa, stesso core) fermo dentro snprintf col lock -> stallo al titolo (visto con gdb). Ora l'attesa e' un vero sonno. */
/* 6 nov 2026: MK8DX_LOG=0 (build distribuibile) toglie le righe di log: niente formattazione ne' attesa sul lock (gli argomenti
   restano valutati come prima: stesso comportamento). Le build per Eden e quella "diagnostica" usano MK8DX_LOG=1. */
#ifndef MK8DX_LOG
#define MK8DX_LOG 1
#endif
struct LockedLogging {
    static constexpr int64_t kWaitNs = 100000;   /* 100 us */
    std::atomic_flag busy = ATOMIC_FLAG_INIT;
    template<typename... Args>
    void Log(Args&&... args) {
        if constexpr (!MK8DX_LOG) { ((void)args, ...); return; }
        while (busy.test_and_set(std::memory_order_acquire)) svcSleepThread(kWaitNs);   /* sonno vero: puo' girare anche chi ha priorita' piu' bassa */
        LoggingImpl.Log(std::forward<Args>(args)...);
        busy.clear(std::memory_order_release);
    }
    template<typename... Args>
    void VLog(Args&&... args) {
        if constexpr (!MK8DX_LOG) { ((void)args, ...); return; }
        while (busy.test_and_set(std::memory_order_acquire)) svcSleepThread(kWaitNs);
        LoggingImpl.VLog(std::forward<Args>(args)...);
        busy.clear(std::memory_order_release);
    }
    bool IsEnabled() const { return MK8DX_LOG && LoggingImpl.IsEnabled(); }
};
inline LockedLogging Logging;
