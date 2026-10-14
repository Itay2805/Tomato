#pragma once

#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

#define CPU_LOCAL(storage, type, name)                                                             \
    [[gnu::section("cpu_locals")]]                                                                 \
    storage type __seg_gs name;

/**
 * How many cores we have
 */
LATE_RO extern size_t g_cpu_count;

/**
 * The array of bases of all cores
 */
LATE_RO extern ptrdiff_t* g_cpu_locals_offsets;

/**
 * The base of the cpu locals of the current thread
 */
CPU_LOCAL(extern, ptrdiff_t, g_cpu_locals_offset);

/**
 * Get absolute pointer to a cpu local from the current cpu
 */
#define CPU_LOCAL_PTR(name)                                                                        \
    ({                                                                                             \
        __seg_gs typeof_unqual(name)* var__ = &name;                                               \
        (typeof_unqual(name)*)(g_cpu_locals_offset + (uintptr_t)var__);                            \
    })

/**
 * Get absolute pointer to a cpu local of another core
 */
#define CPU_LOCAL_PTR_OF(name, index)                                                              \
    ({                                                                                             \
        __seg_gs typeof_unqual(name)* var__ = &name;                                               \
        (typeof_unqual(name)*)(g_cpu_locals_offsets[index] + (uintptr_t)var__);                    \
    })
