#include "virt.h"
#include "arch/irq.h"
#include "arch/virt.h"
#include "lib/assert.h"
#include "lib/object.h"
#include "lib/trace.h"
#include "mem/accounting.h"
#include "mem/mapping.h"
#include "mem/phys.h"
#include "mem/vmar.h"
#include "sync/mutex.h"
#include "sync/rwlock.h"
#include "sync/spinlock.h"
#include "util/defs.h"
#include "util/except.h"
#include <stdint.h>

LATE_RO void* g_page_table = nullptr;
LATE_RO size_t g_page_table_levels = 0;

/**
 * This protects modifications and iterations to the page table, but not to the leaf nodes, the leaf
 * nodes are protected by VMAR mapping lock
 */
static rwlock_t m_page_table_lock;

static uint64_t* virt_get_next_level(uint64_t* entry, bool alloc, bool user) {
    if ((*entry & IA32_PG_P) == 0) {
        ASSERT(alloc, "Expected page entry to be allocated, but was unallocated");

        void* phys = phys_alloc(PAGE_SIZE);
        if (phys == nullptr) {
            return nullptr;
        }

        // TODO: once we have support for mapping the usermode
        //       elf and setting up protections we should conditionally
        //       set the RW/NX bits, to make it a bit safer
        uint64_t new_entry = direct_to_phys(phys) | IA32_PG_P;
        new_entry |= IA32_PG_RW;
        new_entry |= IA32_PG_NX;
        if (user)
            new_entry |= IA32_PG_U;
        *entry = new_entry;
    }

    return phys_to_direct(*entry & PAGING_4K_ADDRESS_MASK);
}

static uint64_t* virt_get_pte(void* virt, bool alloc) {
    uint64_t* table = g_page_table;

    bool user = virt <= vmar_end(&g_user_region);

    for (int i = g_page_table_levels - 1; i >= 0; i--) {
        size_t shift = PAGING_INDEX_BITS * i + PAGE_SHIFT;
        size_t index = ((uintptr_t)virt >> shift) & PAGING_INDEX_MASK;

        // get the pte, if its the exact level we want return it
        uint64_t* pte = &table[index];
        if (i == 0) {
            return pte;
        }

        // get next table
        table = virt_get_next_level(pte, alloc, user);
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
    err_t err = SUCCESS;

    // reserve the memory
    RETHROW(mem_acct_reserve(page_count));
    errdefer mem_acct_unreserve(page_count);

    rwlock_exclusive_guard(&m_page_table_lock);

    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        CHECK_ERROR(virt_get_pte(addr, true) != nullptr, OUT_OF_RESOURCES);
    }

    return err;
}

#define CHECK_KILL(expr)                                                                           \
    do {                                                                                           \
        if (UNLIKELY(!(expr))) {                                                                   \
            if ((error_code & IA32_PF_EC_USER)) {                                                  \
                ASSERT(!"kill the current thread");                                                \
            } else {                                                                               \
                CHECK_FAIL(INTERNAL_ERROR);                                                        \
            }                                                                                      \
        }                                                                                          \
    } while (0)

err_t virt_page_fault_handler(void* addr, uint64_t error_code) {
    // check the flags can popup in normal operation, if they
    // shouldn't then we should really panic
    uint64_t valid_flags = IA32_PF_EC_USER | IA32_PF_EC_WRITE | IA32_PF_EC_PROT | IA32_PF_EC_INSTR;
    CHECK((error_code & valid_flags) == error_code);

    // find the correct root to use
    vmar_t* root = nullptr;
    if (LIKELY(error_code & IA32_PF_EC_USER) || addr < g_kernel_region.base) {
        // either a user access or an access below the kernel region
        root = &g_user_region;
    } else {
        // a kernel region access
        root = &g_kernel_region;
    }

    // find the exact mapping
    vmar_t* vmar = vmar_find_mapping(root, addr);
    CHECK_KILL(vmar != nullptr);
    defer OBJECT_PUT(vmar);
    CHECK(vmar->kind == VMAR_KIND_MAPPING, "Expected a mapping vmar");
    defer mutex_unlock(&vmar->mapping.page_table_lock);

    // TODO: should we maybe have a lock per pte or something?

    // we should never get a protection fault
    CHECK_KILL((error_code & IA32_PF_EC_PROT) == 0);
    CHECK_KILL((error_code & IA32_PF_EC_INSTR) == 0);

    // get the pte, it should never be null
    uint64_t* pte = virt_get_pte(addr, false);
    CHECK(pte != nullptr, "Failed to get a reserved PTE");

    // if page is already present, then we raced
    if (*pte & IA32_PG_P) {
        return SUCCESS;
    }

    // allocate the page, must be available from reserve
    void* page = phys_alloc_reserved(PAGE_SIZE);
    CHECK(page != nullptr, "Failed to allocate a reserved page");

    // TODO: check the protections
    uint64_t new_pte = IA32_PG_P | IA32_PG_A | direct_to_phys(page);
    new_pte |= IA32_PG_RW | IA32_PG_D;
    new_pte |= IA32_PG_NX;
    if (root == &g_user_region) {
        new_pte |= IA32_PG_U;
    }
    *pte = new_pte;

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
