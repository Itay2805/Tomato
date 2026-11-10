#include "virt.h"
#include "arch/virt.h"
#include "debug/trace.h"
#include "irq/irq.h"
#include "lib/assert.h"
#include "lib/object.h"
#include "mem/accounting.h"
#include "mem/mapping.h"
#include "mem/phys.h"
#include "mem/pte.h"
#include "mem/tlb.h"
#include "mem/vmar.h"
#include "sync/mutex.h"
#include "sync/rwlock.h"
#include "sync/spinlock.h"
#include "util/atomic.h"
#include "util/defs.h"
#include "util/except.h"
#include <stdint.h>

LATE_RO void* g_page_table = nullptr;
LATE_RO void* g_user_page_table = nullptr;
LATE_RO size_t g_page_table_levels = 0;

static _Atomic(uint64_t)* virt_get_next_level(_Atomic(uint64_t)* entry, bool alloc, bool user) {
    uint64_t value = atomic_load_acquire(entry);

    if (!pte_is_present(value)) {
        ASSERT(alloc, "Expected page entry to be allocated, but was unallocated");

        void* phys = phys_alloc(PAGE_SIZE);
        if (phys == nullptr) {
            return nullptr;
        }

        uint64_t new_entry = pte_table(direct_to_phys(phys), user);

        // attempt to set the new entry, if it failed it means someone else raced and set
        // the entry before us, so use their entry
        if (!atomic_compare_exchange_rel_acq(entry, &value, new_entry)) {
            phys_free(phys, PAGE_SIZE);
        } else {
            value = new_entry;
        }
    }

    ASSERT(pte_is_present(value));
    return phys_to_direct(pte_address(value));
}

static _Atomic(uint64_t)* virt_get_pte(void* virt, bool alloc) {
    bool user = virt_is_user_address(virt);
    _Atomic(uint64_t)* table = user ? g_user_page_table : g_page_table;

    for (int i = g_page_table_levels - 1; i >= 0; i--) {
        size_t shift = PAGING_INDEX_BITS * i + PAGE_SHIFT;
        size_t index = ((uintptr_t)virt >> shift) & PAGING_INDEX_MASK;

        // get the pte, if its the exact level we want return it
        _Atomic(uint64_t)* pte = &table[index];
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

bool virt_is_user_address(void* addr) {
    return addr <= vmar_end(&g_user_region);
}

void virt_make_global(void* base, size_t page_count) {
    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
        ASSERT(pte != nullptr);
        uint64_t value = atomic_load_relaxed(pte);
        if (pte_is_present(value) && !pte_is_global(value)) {
            pte_make_global(pte, addr);
        }
    }
}

err_t virt_reserve_page_tables(void* base, size_t page_count) {
    err_t err = SUCCESS;

    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        CHECK_ERROR(virt_get_pte(addr, true) != nullptr, OUT_OF_RESOURCES);
    }

    return err;
}

/**
 * The pages that back a mapping are only accessible through it, so they
 * are taken out of the direct map for as long as they are mapped
 */
static void virt_direct_map_remove(tlb_invl_t* invl, void* page) {
    _Atomic(uint64_t)* pte = virt_get_pte(page, false);
    uint64_t value = atomic_load_relaxed(pte);
    ASSERT(pte_is_present(value) && !pte_is_global(value));
    atomic_store_relaxed(pte, 0);
    tlb_invl_queue(invl, page, pte_is_global(value));
}

static void virt_direct_map_restore(void* page) {
    _Atomic(uint64_t)* pte = virt_get_pte(page, false);
    ASSERT(atomic_load_relaxed(pte) == 0);
    atomic_store_relaxed(pte, pte_page(direct_to_phys(page), PTE_WRITE));

    // the allocator uses it through the direct map right away
    pte_barrier();
}

static pte_flags_t vmar_pte_flags(vmar_t* vmar) {
    // populate the entire vmar
    pte_flags_t flags = PTE_WRITE;

    // if locked marked as global, it will never get unmapped
    if (vmar->locked) {
        flags |= PTE_GLOBAL;
    }

    // if a phys mapping and we need device caching then
    // set it as such
    if (vmar->kind == VMAR_KIND_PHYS_MAPPING || vmar->kind == VMAR_KIND_MAPPING) {
        switch (vmar->mapping.cache) {
            case VM_CACHE_CACHED:
                break;

            case VM_CACHE_UNCACHED_DEVICE:
                flags |= PTE_DEVICE;
                // fallthrough

            case VM_CACHE_UNCACHED:
                flags |= PTE_UNCACHED;
                break;

            case VM_CACHE_WRITE_COMBINING:
                flags |= PTE_DEVICE;
                break;

            default:
                ASSERT(!"Invalid caching mode");
        }
    }

    return flags;
}

void virt_populate(void* base, size_t page_count, vmar_t* vmar) {
    ASSERT(((uintptr_t)base % PAGE_SIZE) == 0);

    pte_flags_t flags = vmar_pte_flags(vmar);
    if (virt_is_user_address(base)) {
        flags |= PTE_USER;
    }

    // the new pages hold nothing until we return, so they can be
    // in both places until the direct map removal is committed
    tlb_invl_t invl;
    tlb_invl_start(&invl);

    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        DEBUG_ASSERT(user == virt_is_user_address(addr));

        _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
        ASSERT(pte != nullptr);

        // ignore an entry that is already populated
        uint64_t value = atomic_load_acquire(pte);
        if (pte_is_present(value)) {
            continue;
        }

        // NOTE: we assume that the mapping lock is already taken, so we can't race
        //       on allocating this entry
        uint64_t phys_addr;
        if (vmar->kind == VMAR_KIND_MAPPING) {
            // allocate a new page
            void* phys = phys_alloc_reserved(PAGE_SIZE);
            ASSERT(phys != nullptr);
            virt_direct_map_remove(&invl, phys);
            phys_addr = direct_to_phys(phys);

        } else if (vmar->kind == VMAR_KIND_PHYS_MAPPING) {
            // offset from the base phys page
            size_t offset = (addr - vmar->base);
            phys_addr = vmar->mapping.phys_addr + offset;

        } else {
            ASSERT(!"Invalid vmar kind");
        }

        // setup the entry and publish it
        atomic_store_release(pte, pte_page(phys_addr, flags));
    }

    // no core can see the new pages through the direct map once this returns
    tlb_invl_commit(&invl);

    // make sure the new pages can be used right away
    pte_barrier();
}

#define CHECK_KILL(expr)                                                                           \
    do {                                                                                           \
        if (UNLIKELY(!(expr))) {                                                                   \
            if ((flags & VIRT_FAULT_USER)) {                                                       \
                ASSERT(!"kill the current thread");                                                \
            } else {                                                                               \
                CHECK_FAIL(INTERNAL_ERROR);                                                        \
            }                                                                                      \
        }                                                                                          \
    } while (0)

err_t virt_page_fault_handler(void* addr, virt_fault_flags_t flags) {
    // find the correct root to use
    vmar_t* root = nullptr;
    if (LIKELY(flags & VIRT_FAULT_USER) || addr < g_kernel_region.base) {
        // either a user access or an access below the kernel region
        root = &g_user_region;
    } else {
        // a kernel region access
        root = &g_kernel_region;
    }

    // find the exact mapping, this also takes the mapping lock, ensuring that
    // only we can modify the mapping and populate it
    vmar_t* vmar = vmar_find_mapping(root, addr);
    CHECK_KILL(vmar != nullptr);
    defer {
        mutex_unlock(&vmar->mapping.page_table_lock);
        OBJECT_PUT(vmar);
    }

    // we should never get a protection fault
    CHECK_KILL((flags & VIRT_FAULT_PRESENT) == 0);
    CHECK_KILL((flags & VIRT_FAULT_INSTR) == 0);

    // populate a single page
    virt_populate(addr, 1, vmar);

    return SUCCESS;
}

void virt_free(void* base, size_t page_count) {
    // start doing tlb invalidations
    tlb_invl_t invl;
    tlb_invl_start(&invl);

    // start by going over and invalidating the entire range to be unmapped
    size_t early_unreserve = 0;
    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
        uint64_t value = atomic_load_relaxed(pte);

        // we should never try to free pages marked
        // as global directly, as that means they
        // are still pinned
        ASSERT(!pte_is_global(value));

        if (value != 0) {
            // mask all bits but the address bits
            uint64_t address = pte_address(value);
            atomic_store_relaxed(pte, address);
            tlb_invl_queue(&invl, addr, pte_is_global(value));
        } else {
            // we perform a single unreserve for all the non-allocated pages
            // to save on atomic ops
            early_unreserve++;
        }
    }

    // we can already unreserve some pages to allow
    // others in the system to use that memory
    mem_acct_unreserve(early_unreserve);

    // we are done invalidating the entire page table, commit that it is invalidated
    tlb_invl_commit(&invl);

    // now go over the page tables and actually free everything, no core should
    // be able to see these pages anymore
    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
        uint64_t value = atomic_load_relaxed(pte);

        // entry can't be accessed anymore, we can return it to the allocator, which
        // needs it back in the direct map
        if (value != 0) {
            uint64_t address = pte_address(value);
            ASSERT(address == value);
            virt_direct_map_restore(phys_to_direct(address));
            phys_free(phys_to_direct(address), PAGE_SIZE);
            atomic_store_relaxed(pte, 0);
        }
    }
}

void virt_unmap(void* base, size_t page_count) {
    // start doing tlb invalidations
    tlb_invl_t invl;
    tlb_invl_start(&invl);

    // just unmap the entire range
    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
        uint64_t value = atomic_load_relaxed(pte);

        // we should never try to unmap pages marked
        // as global directly, as that means they
        // are still pinned
        ASSERT(!pte_is_global(value));

        if (value != 0) {
            // just remove the pte
            atomic_store_relaxed(pte, 0);
            tlb_invl_queue(&invl, addr, pte_is_global(value));
        }
    }

    // we are done invalidating the entire page table, commit that it is invalidated
    tlb_invl_commit(&invl);
}
