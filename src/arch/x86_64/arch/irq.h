#pragma once

#include "arch/intrin.h"
#include "util/defs.h"

/**
 * Interrupt control helpers, see irq/irq.h
 */

static inline void irq_enable(void) {
    asm("sti" ::: "memory");
}

static inline void irq_disable(void) {
    asm("cli" ::: "memory");
}

static inline bool irq_enabled(void) {
    return __builtin_ia32_readeflags_u64() & RFLAGS_IF;
}

static inline void irq_wait(void) {
    // we need the sti to come right before the hlt to make sure we
    // atomically hlt and enable interrupts
    asm volatile("sti\n"
                 "hlt\n"
                 "cli\n" ::
                     : "memory");
}
