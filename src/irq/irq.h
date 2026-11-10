#pragma once

#include "util/defs.h"
#include <stddef.h>

/**
 * Interrupt control of the current core, every arch implements it in its arch/irq.h
 */

static inline void irq_enable(void);

static inline void irq_disable(void);

/**
 * Are interrupts enabled on the current core
 */
static inline bool irq_enabled(void);

/**
 * Enable interrupts, wait for one to arrive and disable them again
 */
static inline void irq_wait(void);

typedef struct irq_state {
    bool enabled;
} irq_state_t;

static inline irq_state_t irq_save(void) {
    bool enabled = irq_enabled();
    irq_disable();
    return (irq_state_t){ .enabled = enabled };
}

static inline void irq_restore(irq_state_t state) {
    if (state.enabled) {
        irq_enable();
    }
}

#include "arch/irq.h"
