#pragma once

#include "util/defs.h"
#include <stdatomic.h>

#define MUTEX_LOCKED_BIT BIT0

typedef struct mutex {
    atomic_size_t state;
} mutex_t;

[[gnu::cold]]
void mutex_lock_slow(mutex_t* mutex);

[[gnu::cold]]
void mutex_unlock_slow(mutex_t* mutex);

static inline void mutex_lock(mutex_t* mutex) {
    size_t zero = 0;
    if (!atomic_compare_exchange_weak_explicit(&mutex->state, &zero, MUTEX_LOCKED_BIT,
                                               memory_order_acquire, memory_order_relaxed)) {
        mutex_lock_slow(mutex);
    }
}

static inline void mutex_unlock(mutex_t* mutex) {
    size_t locked = MUTEX_LOCKED_BIT;
    if (atomic_compare_exchange_strong_explicit(&mutex->state, &locked, 0, memory_order_release,
                                                memory_order_relaxed)) {
        return;
    }
    mutex_unlock_slow(mutex);
}
