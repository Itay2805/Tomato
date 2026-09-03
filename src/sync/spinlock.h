#pragma once

#include "arch/irq.h"
#include <stdatomic.h>
#include <stddef.h>

typedef struct irq_spinlock {
    atomic_flag flag;
} irq_spinlock_t;

#define INIT_IRQ_SPINLOCK ((irq_spinlock_t){ .flag = ATOMIC_FLAG_INIT })

irq_state_t irq_spinlock_acquire(irq_spinlock_t* lock);
void irq_spinlock_release(irq_spinlock_t* lock, irq_state_t state);
