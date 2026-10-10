#pragma once

#include "arch/virt.h"
#include <stddef.h>

/**
 * How many levels of buddy we are holding, 64 bytes up to 2mb
 */
#define PHYS_BUDDY_MAX_LEVEL 16

/**
 * The minimum size that the physical allocator can allocate, this is 64 bytes
 * so small physically contiguous structures don't need a full page
 */
#define PHYS_BUDDY_MIN_ORDER 6
#define PHYS_BUDDY_MIN_SIZE  (1UL << PHYS_BUDDY_MIN_ORDER)

/**
 * The maximum size the buddy can allocate, this is also the max alignment
 * we can have because every page is aligned to its own size
 */
#define PHYS_BUDDY_MAX_SIZE (1ULL << ((PHYS_BUDDY_MAX_LEVEL + PHYS_BUDDY_MIN_ORDER) - 1))

/**
 * Allocate physical memory, a block of a page or more commits all of its pages, a
 * smaller block only commits a page when it has to split a new one.
 *
 * Returns NULL if there is no free block or not enough available pages
 */
void* phys_alloc(size_t size);

/**
 * Allocate physical memory using pages that were reserved with mem_acct_reserve,
 * the size must be exactly a block of pages so it is clear how many pages are
 * taken from the reservation.
 *
 * A single page always succeeds, bigger blocks can still fail because of
 * fragmentation, in which case the reservation is left untouched.
 */
void* phys_alloc_reserved(size_t size);

/**
 * Free physical memory
 */
void phys_free(void* ptr, size_t size);

/**
 * Free physical memory, putting its pages back into the reservation instead of
 * making them available, so the owner can allocate them again later.
 *
 * The size must be exactly a block of pages
 */
void phys_free_reserved(void* ptr, size_t size);

/**
 * Add more memory to the physical memory allcoator, the range
 * must be page aligned since it is accounted in pages
 */
void phys_add_memory(void* start, void* end);