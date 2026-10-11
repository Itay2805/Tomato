#include "virt.h"
#include "arch/virt.h"
#include "lib/assert.h"
#include "mem/accounting.h"
#include "mem/mapping.h"
#include "mem/phys.h"
#include "sync/rwlock.h"
#include "sync/spinlock.h"
#include <stdint.h>

LATE_RO void* g_page_table = nullptr;
LATE_RO size_t g_page_table_levels = 0;

/**
 * This protects modifications and iterations to the page table, but not to the leaf nodes, the leaf
 * nodes are protected by VMAR mapping lock
 */
static rwlock_t m_page_table_lock;

static uint64_t* virt_get_next_level(uint64_t* entry, bool alloc) {
    if ((*entry & IA32_PG_P) == 0) {
        ASSERT(alloc, "Expected page entry to be allocated, but was unallocated");

        void* phys = phys_alloc(PAGE_SIZE);
        if (phys == nullptr) {
            return nullptr;
        }

        *entry = direct_to_phys(phys) | IA32_PG_P | IA32_PG_RW;
    }

    return phys_to_direct(*entry & PAGING_4K_ADDRESS_MASK);
}

static uint64_t* virt_get_pte(void* virt, bool alloc) {
    uint64_t* table = g_page_table;

    for (int i = g_page_table_levels - 1; i >= 0; i--) {
        size_t shift = PAGING_INDEX_BITS * i + PAGE_SHIFT;
        size_t index = ((uintptr_t)virt >> shift) & PAGING_INDEX_MASK;

        // get the pte, if its the exact level we want return it
        uint64_t* pte = &table[index];
        if (i == 0) {
            return pte;
        }

        // get next table
        table = virt_get_next_level(pte, alloc);
        if (table == nullptr) {
            return nullptr;
        }
    }

    __builtin_unreachable();
}

void virt_make_global(void* addr) {
    rwlock_shared_guard(&m_page_table_lock);

    uint64_t* pte = virt_get_pte(addr, false);
    ASSERT(pte != nullptr);
    ASSERT((*pte & IA32_PG_G) == 0);
    *pte |= IA32_PG_G;
}

err_t virt_reserve_page_tables(void* base, size_t page_count) {
    rwlock_exclusive_guard(&m_page_table_lock);

    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        CHECK_ERROR(virt_get_pte(addr, true) != nullptr, OUT_OF_RESOURCES);
    }

    return SUCCESS;
}

void virt_free(void* base, size_t page_count) {
    // this function does not modify the non-leaf entries, it just
    // walks them, so take the shared lock
    rwlock_shared_guard(&m_page_table_lock);

    // start by going over and invalidating the entire range to be unmapped
    size_t early_unreserve = 0;
    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        uint64_t* pte = virt_get_pte(addr, false);

        // we should never try to free pages marked
        // as global directly, as that means they
        // are still pinned
        ASSERT((*pte & IA32_PG_G) == 0);

        if (*pte != 0) {
            // mask all bits but the address bits
            *pte &= PAGING_4K_ADDRESS_MASK;

            // we want to keep the present bit cleared
            // so the page is invalid, but we want a non
            // address bit in case the page allocated for this
            // is the 0 page, which could happen
            *pte |= BIT1;

            // TODO: gather TLB invalidation
        } else {
            // TODO: should we eagerly unreserve for each iteration? or is it
            //       better to gather it like we do?
            early_unreserve++;
        }
    }

    // we can already unreserve some pages to allow
    // others in the system to use that memory
    mem_acct_unreserve(early_unreserve);

    // TODO: flush TLB invalidations

    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        uint64_t* pte = virt_get_pte(addr, false);

        // entry can't be accessed anymore, we can return it to the allocator
        if (*pte != 0) {
            uint64_t address = *pte & PAGING_4K_ADDRESS_MASK;
            phys_free(phys_to_direct(address), PAGE_SIZE);

            *pte = 0;
        }
    }
}
