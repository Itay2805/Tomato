#pragma once

#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __x86_64__

    /**
     * Define a new cpu local
     */
    #define DEFINE_CPU_LOCAL(storage, type, name)                                                  \
        [[gnu::section("cpu_locals")]]                                                             \
        storage type __seg_gs __cpulocal_##name;

    /**
     * Get a deref to the cpu local
     */
    #define CPU_LOCAL(name) __cpulocal_##name

    /**
     * Get absolute pointer to a cpu local from the current cpu
     */
    #define CPU_LOCAL_PTR(name)                                                                    \
        ({                                                                                         \
            __seg_gs typeof_unqual(CPU_LOCAL(name))* var__ = &CPU_LOCAL(name);                     \
            (typeof_unqual(CPU_LOCAL(name))*)(CPU_LOCAL(cpu_locals_offset) + (uintptr_t)var__);    \
        })

    /**
     * Get absolute pointer to a cpu local of another core
     */
    #define CPU_LOCAL_PTR_OF(name, index)                                                          \
        ({                                                                                         \
            __seg_gs typeof_unqual(CPU_LOCAL(name))* var__ = &CPU_LOCAL(name);                     \
            (typeof_unqual(CPU_LOCAL(name))*)(g_cpu_locals_offsets[index] + (uintptr_t)var__);     \
        })

#elifdef __aarch64__

// TODO: this

#else
    #error Unknown arch
#endif

/**
 * How many cores we have
 */
LATE_RO extern size_t g_cpu_count;

/**
 * The array of bases of all cores
 */
LATE_RO extern ptrdiff_t* g_cpu_locals_offsets;

/**
 * The current cpu id
 */
DEFINE_CPU_LOCAL(extern, size_t, cpu_id);

/**
 * The base of the cpu locals of the current thread
 */
DEFINE_CPU_LOCAL(extern, ptrdiff_t, cpu_locals_offset);
