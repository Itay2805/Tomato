#pragma once

#include "arch/virt.h"
#include <stddef.h>

/**
 * How many levels of buddy we are holding
 */
#define PHYS_BUDDY_MAX_LEVEL 10

/**
 * The minimum size that the physical allocator can allocate, this is
 * a page size, for smaller than page allocation use the normal memory
 * allocator
 */
#define PHYS_BUDDY_MIN_ORDER PAGE_SHIFT
#define PHYS_BUDDY_MIN_SIZE  (1UL << PHYS_BUDDY_MIN_ORDER)

/**
 * The maximum size the buddy can allocate, this is also the max alignment
 * we can have because every page is aligned to its own size
 */
#define PHYS_BUDDY_MAX_SIZE (1ULL << ((PHYS_BUDDY_MAX_LEVEL + PHYS_BUDDY_MIN_ORDER) - 1))

/**
 * Allocate physical memory
 */
void* phys_alloc(size_t size);

/**
 * Free physical memory
 */
void phys_free(void* ptr, size_t size);

/**
 * Add more memory to the physical memory allcoator
 */
void phys_add_memory(void* start, void* end);