#include "mutex.h"
#include "lib/assert.h"

[[gnu::cold]]
void mutex_lock_slow(mutex_t* mutex) {
    (void)mutex;
    ASSERT(!"mutex_lock_slow");
}

[[gnu::cold]]
void mutex_unlock_slow(mutex_t* mutex) {
    (void)mutex;
    ASSERT(!"mutex_unlock_slow");
}
