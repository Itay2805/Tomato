#pragma once

#include <stdatomic.h>

#define atomic_load_relaxed(object) atomic_load_explicit(object, memory_order_relaxed)

#define atomic_fetch_add_relaxed(object, operand)                                                  \
    atomic_fetch_add_explicit(object, operand, memory_order_relaxed)

#define atomic_fetch_sub_relaxed(object, operand)                                                  \
    atomic_fetch_sub_explicit(object, operand, memory_order_relaxed)
