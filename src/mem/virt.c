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
#include "util/atomic.h"
#include "util/defs.h"
#include "util/except.h"
#include <stdint.h>

LATE_RO void* g_page_table = nullptr;
LATE_RO size_t g_page_table_levels = 0;

static _Atomic(uint64_t)* virt_get_next_level(_Atomic(uint64_t)* entry, bool alloc, bool user) {
    uint64_t value = atomic_load_acquire(entry);

    if ((value & IA32_PG_P) == 0) {
        ASSERT(alloc, "Expected page entry to be allocated, but was unallocated");

        void* phys = phys_alloc(PAGE_SIZE);
        if (phys == nullptr) {
            return nullptr;
        }

        uint64_t new_entry = direct_to_phys(phys) | IA32_PG_P;
        new_entry |= IA32_PG_RW;
        if (user)
            new_entry |= IA32_PG_U;

        // attempt to set the new entry, if it failed it means someone else raced and set
        // the entry before us, so use their entry
        if (!atomic_compare_exchange_rel_acq(entry, &value, new_entry)) {
            phys_free(phys, PAGE_SIZE);
        } else {
            value = new_entry;
        }
    }

    ASSERT(value & IA32_PG_P);
    return phys_to_direct(value & PAGING_4K_ADDRESS_MASK);
}

static _Atomic(uint64_t)* virt_get_pte(void* virt, bool alloc) {
    _Atomic(uint64_t)* table = g_page_table;

    bool user = virt_is_user_address(virt);

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

void virt_make_global(void* addr) {
    _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
    ASSERT(pte != nullptr);
    atomic_store_relaxed(pte, atomic_load_relaxed(pte) | IA32_PG_G);
}

err_t virt_reserve_page_tables(void* base, size_t page_count) {
    err_t err = SUCCESS;

    // reserve the memory
    RETHROW(mem_acct_reserve(page_count));
    errdefer mem_acct_unreserve(page_count);

    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        CHECK_ERROR(virt_get_pte(addr, true) != nullptr, OUT_OF_RESOURCES);
    }

    return err;
}

void virt_populate(void* base, size_t page_count) {
    bool user = virt_is_user_address(base);

    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        DEBUG_ASSERT(user == virt_is_user_address(addr));

        _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
        ASSERT(pte != nullptr);

        // ignore an entry that is already populated
        uint64_t value = atomic_load_acquire(pte);
        if (value & IA32_PG_P) {
            continue;
        }

        // NOTE: we assume that the mapping lock is already taken, so we can't race
        //       on allocating this entry
        void* phys = phys_alloc_reserved(PAGE_SIZE);
        ASSERT(phys != nullptr);

        // setup the entry and publish it
        uint64_t entry = direct_to_phys(phys) | IA32_PG_P | IA32_PG_A;
        entry |= IA32_PG_RW | IA32_PG_D;
        entry |= IA32_PG_NX;
        if (user)
            entry |= IA32_PG_U;
        atomic_store_release(pte, entry);
    }
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

    // find the exact mapping, this also takes the mapping lock, ensuring that
    // only we can modify the mapping and populate it
    vmar_t* vmar = vmar_find_mapping(root, addr);
    CHECK_KILL(vmar != nullptr);
    defer {
        mutex_unlock(&vmar->mapping.page_table_lock);
        OBJECT_PUT(vmar);
    }

    // we should never get a protection fault
    CHECK_KILL((error_code & IA32_PF_EC_PROT) == 0);
    CHECK_KILL((error_code & IA32_PF_EC_INSTR) == 0);

    // populate a single page
    virt_populate(addr, 1);

    return SUCCESS;
}

static void tlb_invl_start() {
    // TODO: mark that we start invalidating, maybe
    //       taking some lock
}

static void tlb_invl_queue(void* addr) {
    // might as well flush it normally on our core
    __invlpg(addr);
}

static void tlb_invl_commit() {
    // TODO: tell other cores and wait for them to finish flushing
}

void virt_free(void* base, size_t page_count) {
    // start doing tlb invalidations
    tlb_invl_start();

    // start by going over and invalidating the entire range to be unmapped
    size_t early_unreserve = 0;
    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
        uint64_t value = atomic_load_relaxed(pte);

        // we should never try to free pages marked
        // as global directly, as that means they
        // are still pinned
        ASSERT((value & IA32_PG_G) == 0);

        if (value != 0) {
            // mask all bits but the address bits
            uint64_t address = value & PAGING_4K_ADDRESS_MASK;
            atomic_store_relaxed(pte, address);
            tlb_invl_queue(addr);
        } else {
            // TODO: should we eagerly unreserve for each iteration? or is it
            //       better to gather it like we do?
            early_unreserve++;
        }
    }

    // we can already unreserve some pages to allow
    // others in the system to use that memory
    mem_acct_unreserve(early_unreserve);

    // we are done invalidating the entire page table, commit that it is invalidated
    tlb_invl_commit();

    // now go over the page tables and actually free everything, no core should
    // be able to see these pages anymore
    for (size_t i = 0; i < page_count; i++) {
        void* addr = base + i * PAGE_SIZE;
        _Atomic(uint64_t)* pte = virt_get_pte(addr, false);
        uint64_t value = atomic_load_relaxed(pte);

        // entry can't be accessed anymore, we can return it to the allocator
        if (value != 0) {
            uint64_t address = value & PAGING_4K_ADDRESS_MASK;
            ASSERT(address == value);
            phys_free(phys_to_direct(address), PAGE_SIZE);
            atomic_store_relaxed(pte, 0);
        }
    }
}
