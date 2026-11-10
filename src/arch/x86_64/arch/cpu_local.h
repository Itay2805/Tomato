#pragma once

#include <stdint.h>

/**
 * The cpu locals are accessed through the GS segment, its base is set to the offset
 * of the locals of the current core from the ones linked into the kernel
 */

/**
 * Define a new cpu local
 */
#define DEFINE_CPU_LOCAL(storage, type, name)                                                      \
    [[gnu::section("cpu_locals")]]                                                                 \
    storage type __seg_gs __cpulocal_##name;

/**
 * Get a deref to the cpu local
 */
#define CPU_LOCAL(name) __cpulocal_##name

/**
 * Get absolute pointer to a cpu local from the current cpu
 */
#define CPU_LOCAL_PTR(name)                                                                        \
    ({                                                                                             \
        __seg_gs typeof_unqual(CPU_LOCAL(name))* var__ = &CPU_LOCAL(name);                         \
        (typeof_unqual(CPU_LOCAL(name))*)(CPU_LOCAL(cpu_locals_offset) + (uintptr_t)var__);        \
    })

/**
 * Get absolute pointer to a cpu local of another core
 */
#define CPU_LOCAL_PTR_OF(name, index)                                                              \
    ({                                                                                             \
        __seg_gs typeof_unqual(CPU_LOCAL(name))* var__ = &CPU_LOCAL(name);                         \
        (typeof_unqual(CPU_LOCAL(name))*)(g_cpu_locals_offsets[index] + (uintptr_t)var__);         \
    })
