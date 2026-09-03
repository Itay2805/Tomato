#include "spinlock.h"
#include "arch/irq.h"
#include <stdatomic.h>

irq_state_t irq_spinlock_acquire(irq_spinlock_t* lock) {
    irq_state_t state = irq_save();
    while (atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire))
        ;
    return state;
}

void irq_spinlock_release(irq_spinlock_t* lock, irq_state_t state) {
    atomic_flag_clear_explicit(&lock->flag, memory_order_release);
    irq_restore(state);
}
