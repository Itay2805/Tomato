#pragma once

#include "util/defs.h"
#include <stdint.h>

/**
 * Arch independent flags of a leaf page table entry, the arch turns
 * them into real entries
 *
 * if only device is set then assume write-combining
 */
typedef enum pte_flags {
    PTE_WRITE = BIT0,
    PTE_EXEC = BIT1,
    PTE_USER = BIT2,
    PTE_GLOBAL = BIT3,
    PTE_DEVICE = BIT4,
    PTE_UNCACHED = BIT5,
} pte_flags_t;

/**
 * Page table entry helpers, every arch implements them in its arch/pte.h
 */

/**
 * A leaf entry mapping a single page
 */
static inline uint64_t pte_page(uint64_t phys, pte_flags_t flags);

/**
 * A non-leaf entry pointing to the next level table
 */
static inline uint64_t pte_table(uint64_t phys, bool user);

/**
 * The entry is used by the page table walker
 */
static inline bool pte_is_present(uint64_t pte);

/**
 * The physical address of the entry
 */
static inline uint64_t pte_address(uint64_t pte);

/**
 * Global pages survive flushing all the non-global translations
 */
static inline bool pte_is_global(uint64_t pte);

/**
 * Make a present entry global, the page must not be in use
 */
static inline void pte_make_global(_Atomic(uint64_t)* pte, void* addr);

/**
 * Make new entries visible to the walker, needed before using them
 */
static inline void pte_barrier(void);

// the implementation needs pte_flags_t, so it comes last
#include "arch/pte.h"
