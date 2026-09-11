#include "rwlock.h"
#include "lib/assert.h"

[[gnu::cold]]
void rwlock_lock_exclusive_slow(rwlock_t* lock) {
    (void)lock;
    ASSERT(!"TODO: rwlock_lock_exclusive_slow");
}

[[gnu::cold]]
void rwlock_unlock_exclusive_slow(rwlock_t* lock) {
    (void)lock;
    ASSERT(!"TODO: rwlock_unlock_exclusive_slow");
}

[[gnu::cold]]
void rwlock_try_lock_shared_slow(rwlock_t* lock) {
    (void)lock;
    ASSERT(!"TODO: rwlock_try_lock_shared_slow");
}

[[gnu::cold]]
void rwlock_unlock_shared_slow(rwlock_t* lock) {
    (void)lock;
    ASSERT(!"TODO: rwlock_unlock_shared_slow");
}
