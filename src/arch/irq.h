#pragma once

#include "arch/intrin.h"
#include <stddef.h>

typedef struct irq_state {
    bool enabled;
} irq_state_t;

static inline irq_state_t irq_save() {
    bool enabled = irq_enabled();
    irq_disable();
    return (irq_state_t){ .enabled = enabled };
}

static inline void irq_restore(irq_state_t state) {
    if (state.enabled) {
        irq_enable();
    }
}
