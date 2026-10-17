#pragma once

#include "util/except.h"
#include <stddef.h>
#include <stdint.h>

LATE_RO extern void* g_page_table;
LATE_RO extern size_t g_page_table_levels;

bool virt_is_user_address(void* addr);

/**
 * Mark the page of the address as a global page
 */
void virt_make_global(void* addr);

/**
 * Allocate the non-leaf entries in the range of base to page count, ensuring that we
 * don't need to allocate anything in order to map a new page, this is mostly done to
 * more easily deal with ensuring we don't over commit, the amount of need to allocate
 * is small enough that it should not be too expensive performance wise
 *
 * NOTE: this does not look at the vmars at all
 */
err_t virt_reserve_page_tables(void* base, size_t page_count);

/**
 * Populate the given range with pages
 *
 * NOTE: this does not look at the vmars at all
 */
void virt_populate(void* addr, size_t page_count);

/**
 * Handle a page fault that happened, returns true if handled successfully, false otherwise
 */
err_t virt_page_fault_handler(void* addr, uint64_t error_code);

/**
 * Free the allocated region
 */
void virt_free(void* base, size_t page_count);
