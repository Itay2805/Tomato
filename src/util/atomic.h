#pragma once

#include <stdatomic.h>

#define atomic_load_relaxed(object) atomic_load_explicit(object, memory_order_relaxed)
#define atomic_load_acquire(object) atomic_load_explicit(object, memory_order_acquire)

#define atomic_store_relaxed(object, desired)                                                      \
    atomic_store_explicit(object, desired, memory_order_relaxed)
#define atomic_store_release(object, desired)                                                      \
    atomic_store_explicit(object, desired, memory_order_release)

#define atomic_fetch_add_relaxed(object, operand)                                                  \
    atomic_fetch_add_explicit(object, operand, memory_order_relaxed)
#define atomic_fetch_add_acquire(object, operand)                                                  \
    atomic_fetch_add_explicit(object, operand, memory_order_acquire)

#define atomic_fetch_sub_relaxed(object, operand)                                                  \
    atomic_fetch_sub_explicit(object, operand, memory_order_relaxed)
#define atomic_fetch_sub_release(object, operand)                                                  \
    atomic_fetch_sub_explicit(object, operand, memory_order_release)

#define atomic_fence_acquire() atomic_thread_fence(memory_order_acquire)

#define atomic_compare_exchange_relaxed(object, expected, desired)                                 \
    atomic_compare_exchange_strong_explicit(object, expected, desired, memory_order_relaxed,       \
                                            memory_order_relaxed)

#define atomic_compare_exchange_release(object, expected, desired)                                 \
    atomic_compare_exchange_strong_explicit(object, expected, desired, memory_order_release,       \
                                            memory_order_relaxed)

#define atomic_compare_exchange_rel_acq(object, expected, desired)                                 \
    atomic_compare_exchange_strong_explicit(object, expected, desired, memory_order_release,       \
                                            memory_order_acquire)
