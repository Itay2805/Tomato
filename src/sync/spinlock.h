#pragma once

#include "arch/irq.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stddefer.h>

typedef struct irq_spinlock {
    atomic_flag flag;
} irq_spinlock_t;

#define INIT_IRQ_SPINLOCK ((irq_spinlock_t){ .flag = ATOMIC_FLAG_INIT })

#define irq_spinlock_guard(lock)                                                                   \
    irq_state_t __irq_state_##__LINE__ = irq_spinlock_acquire(lock);                               \
    defer irq_spinlock_release(lock, __irq_state_##__LINE__);

static inline irq_state_t irq_spinlock_acquire(irq_spinlock_t* lock) {
    irq_state_t state = irq_save();
    while (atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire))
        ;
    return state;
}

static inline void irq_spinlock_release(irq_spinlock_t* lock, irq_state_t state) {
    atomic_flag_clear_explicit(&lock->flag, memory_order_release);
    irq_restore(state);
}
