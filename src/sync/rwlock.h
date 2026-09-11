#pragma once

#include "util/defs.h"
#include <stdatomic.h>

#define RWLOCK_PARKED_BIT        BIT0
#define RWLOCK_WRITER_PARKED_BIT BIT1
#define RWLOCK_WRITER_BIT        BIT2
#define RWLOCK_READERS_MASK      (~(BIT0 | BIT1 | BIT2))
#define RWLOCK_ONE_READER        BIT3

typedef struct rwlock {
    atomic_size_t state;
} rwlock_t;

[[gnu::cold]]
void rwlock_lock_exclusive_slow(rwlock_t* lock);

[[gnu::cold]]
void rwlock_unlock_exclusive_slow(rwlock_t* lock);

static inline void rwlock_lock_exclusive(rwlock_t* lock) {
    size_t zero = 0;
    if (!atomic_compare_exchange_weak_explicit(&lock->state, &zero, RWLOCK_WRITER_BIT,
                                               memory_order_acquire, memory_order_relaxed)) {
        rwlock_lock_exclusive_slow(lock);
    }
}

static inline void rwlock_unlock_exclusive(rwlock_t* lock) {
    size_t locked = RWLOCK_WRITER_BIT;
    if (atomic_compare_exchange_weak_explicit(&lock->state, &locked, 0, memory_order_release,
                                              memory_order_relaxed)) {
        return;
    }
    rwlock_unlock_exclusive_slow(lock);
}

[[clang::always_inline]]
static inline bool rwlock_try_lock_shared_fast(rwlock_t* lock) {
    size_t state = atomic_load_explicit(&lock->state, memory_order_relaxed);

    // We can't allow grabbing a shared lock if there is a writer, even if
    // the writer is still waiting for the remaining readers to exit.
    if ((state & RWLOCK_WRITER_BIT) != 0) {
        return false;
    }

    size_t new_state;
    if (!__builtin_add_overflow(state, RWLOCK_ONE_READER, &new_state)) {
        return atomic_compare_exchange_weak_explicit(&lock->state, &state, new_state,
                                                     memory_order_acquire, memory_order_relaxed);
    } else {
        return false;
    }
}

[[gnu::cold]]
void rwlock_try_lock_shared_slow(rwlock_t* lock);

[[gnu::cold]]
void rwlock_unlock_shared_slow(rwlock_t* lock);

static inline void rwlock_lock_shared(rwlock_t* lock) {
    if (!rwlock_try_lock_shared_fast(lock)) {
        rwlock_try_lock_shared_slow(lock);
    }
}

static inline void rwlock_unlock_shared(rwlock_t* lock) {
    size_t state = atomic_fetch_sub_explicit(&lock->state, RWLOCK_ONE_READER, memory_order_release);
    if ((state & (RWLOCK_READERS_MASK | RWLOCK_WRITER_PARKED_BIT)) ==
        (RWLOCK_ONE_READER | RWLOCK_WRITER_PARKED_BIT)) {
        rwlock_unlock_shared_slow(lock);
    }
}

[[clang::always_inline]]
static inline void rwlock_shared_cleanup(rwlock_t** lock) {
    rwlock_unlock_shared(*lock);
}
