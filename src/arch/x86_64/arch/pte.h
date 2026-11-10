#pragma once

#include "arch/intrin.h"
#include "arch/virt.h"
#include "lib/assert.h"
#include "mem/pte.h"
#include "mem/vmar.h"
#include <stdatomic.h>
#include <stdint.h>

static inline bool pte_is_present(uint64_t pte) {
    return pte & IA32_PG_P;
}

static inline uint64_t pte_address(uint64_t pte) {
    return pte & PAGING_4K_ADDRESS_MASK;
}

static inline bool pte_is_global(uint64_t pte) {
    return pte & IA32_PG_G;
}

static inline uint64_t pte_table(uint64_t phys, bool user) {
    uint64_t pte = phys | IA32_PG_P | IA32_PG_RW;
    if (user)
        pte |= IA32_PG_U;
    return pte;
}

static inline uint64_t pte_page(uint64_t phys, pte_flags_t flags) {
    uint64_t pte = phys | IA32_PG_P | IA32_PG_A;

    if (flags & PTE_WRITE)
        pte |= IA32_PG_RW | IA32_PG_D;

    if (!(flags & PTE_EXEC))
        pte |= IA32_PG_NX;

    if (flags & PTE_USER)
        pte |= IA32_PG_U;

    if (flags & PTE_GLOBAL)
        pte |= IA32_PG_G;

    if (flags & PTE_UNCACHED) {
        if (flags & PTE_DEVICE) {
            pte |= IA32_PG_CACHE_UCM;
        } else {
            pte |= IA32_PG_CACHE_UC;
        }
    } else if (flags & PTE_DEVICE) {
        pte |= IA32_PG_CACHE_WC_4K;
    }

    return pte;
}

static inline void pte_barrier(void) {
    // the page walker sees our stores in order
    asm volatile("" ::: "memory");
}
static inline void pte_make_global(_Atomic(uint64_t)* pte, void* addr) {
    (void)addr;
    atomic_store_explicit(pte, atomic_load_explicit(pte, memory_order_relaxed) | IA32_PG_G,
                          memory_order_relaxed);
}
