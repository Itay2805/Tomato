#pragma once

#include "arch/virt.h"
#include "lib/assert.h"
#include "vmar.h"
#include <stdint.h>

// The higher half region
extern vmar_t g_kernel_region;

// The direct map + buddy allocator bitmap
extern vmar_t g_direct_map;
extern vmar_t g_buddy_bitmap_mapping;

/**
 * The buddy allocator keeps a byte of metadata per physical page in the buddy
 * bitmap. Blocks go from a single page (order 0) up to 2^BUDDY_MAX_ORDER pages,
 * and the max order is chosen so the metadata of a max order block is exactly
 * one page of the bitmap: merging two buddies never touches the metadata of a
 * different bitmap page.
 *
 * That allows the bitmap to be sparse, only the bitmap pages that cover actual
 * memory are backed, the rest of it is left unmapped.
 */
#define BUDDY_MAX_ORDER PAGE_SHIFT

/**
 * The amount of physical memory covered by a single page of the bitmap
 */
#define BUDDY_BITMAP_PAGE_COVERAGE ((size_t)PAGE_SIZE << BUDDY_MAX_ORDER)

// The kernel sections
extern vmar_t g_kernel_text_mapping;
extern vmar_t g_kernel_rodata_mapping;
extern vmar_t g_kernel_data_mapping;

// The user region
extern vmar_t g_user_region;
extern vmar_t g_null_region;

static inline void* phys_to_direct(uint64_t phys) {
    DEBUG_ASSERT(phys <= g_direct_map.page_count * PAGE_SIZE);
    return (void*)(g_direct_map.base + phys);
}

static inline uint64_t direct_to_phys(void* ptr) {
    DEBUG_ASSERT(g_direct_map.base <= ptr);
    uint64_t phys = (uintptr_t)ptr - (uintptr_t)g_direct_map.base;
    DEBUG_ASSERT(phys <= g_direct_map.page_count * PAGE_SIZE);
    return phys;
}
