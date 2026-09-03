#pragma once

#include "arch/intrin.h"
#include <stddef.h>

static inline bool irq_enabled() {
    return __builtin_ia32_readeflags_u64() & (1 << 9);
}

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
