#pragma once

#include <stdint.h>

///////////////////////////////////////////////////////////////
// Arch specific intrinsics
///////////////////////////////////////////////////////////////

#define __read_sysreg(reg)                                                                         \
    ({                                                                                             \
        uint64_t value__;                                                                          \
        asm volatile("mrs %0, " #reg : "=r"(value__));                                             \
        value__;                                                                                   \
    })

#define __write_sysreg(reg, value)                                                                 \
    asm volatile("msr " #reg ", %0" : : "r"((uint64_t)(value)) : "memory")

#define __dsb(option) asm volatile("dsb " #option ::: "memory")

static inline void __isb(void) {
    asm volatile("isb" ::: "memory");
}

/**
 * Translate the virtual address for a write at the current EL, returns the PAR_EL1 value
 *
 * NOTE: the exception entry uses PAR_EL1 as well, so this must be called with interrupts
 *       disabled
 */
static inline uint64_t __at_s1e1w(const void* addr) {
    asm volatile("at s1e1w, %0" : : "r"(addr) : "memory");
    __isb();
    return __read_sysreg(par_el1);
}

static inline void cpu_relax(void) {
    asm volatile("yield" ::: "memory");
}
