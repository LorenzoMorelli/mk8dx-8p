#pragma once

#include "common.hpp"

#define EXL_MODULE_NAME "mk8dx-8p"

#define EXL_DEBUG
#define EXL_USE_FAKEHEAP

namespace exl::setting {
    /* How large the fake .bss heap will be. */
    constexpr size_t HeapSize = 0x5000;

    /* How large the JIT area will be for hooks. */
    constexpr size_t JitSize = 0x20000;   /* mk8dx-8p: 655 trampolini (0x10000 = 327, ~299 usati) */

    /* How large the area will be inline hook pool. */
    constexpr size_t InlinePoolSize = 0x8000;

    /* How large the formatting buffer should be for logging. The buffer will be on the stack. */
    constexpr size_t LogBufferSize = 512;

    static_assert(ALIGN_UP(JitSize, PAGE_SIZE) == JitSize, "");
    static_assert(ALIGN_UP(InlinePoolSize, PAGE_SIZE) == InlinePoolSize, "");
}
