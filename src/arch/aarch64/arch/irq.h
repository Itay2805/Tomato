#pragma once

#include "arch/intrin.h"
#include "arch/sysreg.h"
#include "util/defs.h"

/**
 * Interrupt control helpers, see irq/irq.h
 */

static inline void irq_enable(void) {
    asm volatile("msr daifclr, #2" ::: "memory");
}

static inline void irq_disable(void) {
    asm volatile("msr daifset, #2" ::: "memory");
}

static inline bool irq_enabled(void) {
    return !(__read_sysreg(daif) & DAIF_I);
}

static inline void irq_wait(void) {
    // wfi wakes up on a pending interrupt even if it is masked, so we can wait
    // with interrupts masked and only unmask them to take it, the isb makes sure
    // it is taken before we mask them again
    asm volatile("wfi\n"
                 "msr daifclr, #2\n"
                 "isb\n"
                 "msr daifset, #2\n" ::
                     : "memory");
}
