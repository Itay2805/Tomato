#pragma once

#include "mem/pte.h"
#include "mem/vmar.h"
#include "util/except.h"
#include <stddef.h>
#include <stdint.h>

/**
 * The root page tables of the kernel and user halves, on x86-64 both halves live in the same
 * root, while on aarch64 each half has its own root (TTBR1_EL1 and TTBR0_EL1)
 */
LATE_RO extern void* g_page_table;
LATE_RO extern void* g_user_page_table;
LATE_RO extern size_t g_page_table_levels;

/**
 * Arch independent description of a page fault, the arch's exception
 * handler translates its own fault information into these
 */
typedef enum virt_fault_flags {
    /**
     * The page was present, this is a protection violation
     */
    VIRT_FAULT_PRESENT = BIT0,
    VIRT_FAULT_WRITE = BIT1,
    VIRT_FAULT_USER = BIT2,
    VIRT_FAULT_INSTR = BIT3,
} virt_fault_flags_t;

bool virt_is_user_address(void* addr);

/**
 * Mark the page of the address as a global page
 */
void virt_make_global(void* addr, size_t page_count);

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
 * Populate the given range with pages, the pages are taken out of the
 * direct map until they are freed
 *
 * NOTE: this does not look at the vmars at all
 */
void virt_populate(void* addr, size_t page_count, vmar_t* vmar);

/**
 * Handle a page fault that happened, returns true if handled successfully, false otherwise
 */
err_t virt_page_fault_handler(void* addr, virt_fault_flags_t flags);

/**
 * Free the allocated region
 */
void virt_free(void* base, size_t page_count);

/**
 * Unmap pages, not freeing them
 */
void virt_unmap(void* base, size_t page_count);
