#pragma once

#include "mem/vmar.h"
#include "util/defs.h"
#include "util/except.h"
#include <stddef.h>

typedef struct stack_allocation {
    /**
     * The top-level stack region
     */
    vmar_t* stack_region;

    /**
     * The bottom of the stack, aka the value
     * you are actually going to use
     */
    void* stack_bottom;
} stack_allocation_t;

/**
 * Allocate a new stack
 */
err_t stack_alloc(stack_allocation_t* allocation, size_t size, bool user);
