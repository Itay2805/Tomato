#pragma once

#include <stdint.h>

/**
 * Define a new cpu local
 */
#define DEFINE_CPU_LOCAL(storage, type, name)                                                      \
    [[gnu::section("cpu_locals")]]                                                                 \
    storage type __cpulocal_##name;

/**
 * The stack pointer, only used as a fake memory input
 */
register uintptr_t __current_stack_pointer asm("sp");

/**
 * Get the offset of the cpu locals of the current core
 */
static inline uintptr_t cpu_locals_current_offset(void) {
    uintptr_t offset;
    // not volatile so the compiler can reuse the offset, the fake read from the stack makes
    // it read it again after anything that may clobber memory (barriers, calls), so it never
    // survives switching threads
    asm("mrs %0, tpidr_el1" : "=r"(offset) : "Q"(*(const uintptr_t*)__current_stack_pointer));
    return offset;
}

/**
 * Get absolute pointer to a cpu local from the current cpu
 */
#define CPU_LOCAL_PTR(name)                                                                        \
    ((typeof_unqual(__cpulocal_##name)*)((uintptr_t)&__cpulocal_##name +                           \
                                         cpu_locals_current_offset()))

/**
 * Get a deref to the cpu local
 */
#define CPU_LOCAL(name) (*CPU_LOCAL_PTR(name))

/**
 * Get absolute pointer to a cpu local of another core
 */
#define CPU_LOCAL_PTR_OF(name, index)                                                              \
    ((typeof_unqual(__cpulocal_##name)*)((uintptr_t)&__cpulocal_##name +                           \
                                         g_cpu_locals_offsets[index]))
