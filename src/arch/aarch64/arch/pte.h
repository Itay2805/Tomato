#pragma once

#include "arch/intrin.h"
#include "arch/virt.h"
#include "mem/pte.h"
#include "mem/tlb.h"
#include "util/defs.h"
#include <stdatomic.h>
#include <stdint.h>

static inline bool pte_is_present(uint64_t pte) {
    return pte & AARCH64_PG_VALID;
}

static inline uint64_t pte_address(uint64_t pte) {
    return pte & PAGING_4K_ADDRESS_MASK;
}

static inline bool pte_is_global(uint64_t pte) {
    return pte_is_present(pte) && !(pte & AARCH64_PG_NG);
}

static inline uint64_t pte_table(uint64_t phys, bool user) {
    uint64_t pte = phys | AARCH64_PG_VALID | AARCH64_PG_TABLE;
    if (user) {
        // the kernel never runs user code
        pte |= AARCH64_PG_PXN_TABLE;
    } else {
        // and the user never touches kernel memory
        pte |= AARCH64_PG_UXN_TABLE | AARCH64_PG_AP_TABLE_NO_EL0;
    }
    return pte;
}

static inline uint64_t pte_page(uint64_t phys, pte_flags_t flags) {
    uint64_t pte = phys | AARCH64_PG_VALID | AARCH64_PG_PAGE | AARCH64_PG_AF;

    if (flags & PTE_UNCACHED) {
        // device memory, the shareability is ignored and it must never be executed
        if (flags & PTE_DEVICE) {
            pte |= AARCH64_PG_ATTR(MAIR_ATTR_DEVICE_nGnRE);
        } else {
            pte |= AARCH64_PG_ATTR(MAIR_ATTR_DEVICE_nGnRnE);
        }
        flags &= ~PTE_EXEC;
    } else if (flags & PTE_DEVICE) {
        pte |= AARCH64_PG_ATTR(MAIR_ATTR_NORMAL_NC) | AARCH64_PG_SH_INNER;
    } else {
        pte |= AARCH64_PG_ATTR(MAIR_ATTR_NORMAL) | AARCH64_PG_SH_INNER;
    }

    if (!(flags & PTE_WRITE))
        pte |= AARCH64_PG_AP_RO;

    if (flags & PTE_USER) {
        pte |= AARCH64_PG_AP_EL0 | AARCH64_PG_PXN;
        if (!(flags & PTE_EXEC))
            pte |= AARCH64_PG_UXN;
    } else {
        pte |= AARCH64_PG_UXN;
        if (!(flags & PTE_EXEC))
            pte |= AARCH64_PG_PXN;
    }

    if (!(flags & PTE_GLOBAL))
        pte |= AARCH64_PG_NG;

    return pte;
}

static inline void pte_barrier(void) {
    // the walker is not ordered by normal barriers, make sure the new entries
    // reached it before anything uses them
    __dsb(ishst);
    __isb();
}
static inline void pte_make_global(_Atomic(uint64_t)* pte, void* addr) {
    // going from non-global to global needs a break-before-make, otherwise the TLB may
    // end up with both versions of the entry, so the page can't be used meanwhile
    uint64_t value = atomic_load_explicit(pte, memory_order_relaxed);
    atomic_store_explicit(pte, 0, memory_order_relaxed);
    tlb_invalidate_page(addr);
    tlb_invalidate_sync();
    atomic_store_explicit(pte, value & ~AARCH64_PG_NG, memory_order_relaxed);
    pte_barrier();
}
