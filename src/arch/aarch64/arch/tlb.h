#pragma once

#include "arch/intrin.h"
#include "arch/virt.h"
#include "mem/tlb.h"
#include "util/defs.h"
#include <stdint.h>

/**
 * TLB primitives, see mem/tlb.h
 *
 * We use the inner shareable (is) variants, which the hardware broadcasts to all of
 * the cores, so there is never a need to tell the other cores about them
 */

static inline void tlb_invalidate_page(void* addr) {
    // the entry must be updated before the invalidation
    __dsb(ishst);
    asm volatile("tlbi vaae1is, %0"
                 :
                 : "r"(((uintptr_t)addr >> PAGE_SHIFT) & (BIT44 - 1))
                 : "memory");
}

static inline void tlb_invalidate_nonglobal(void) {
    // everything we map that is not global is tagged with ASID 0
    __dsb(ishst);
    asm volatile("tlbi aside1is, %0" : : "r"(0ull) : "memory");
}

static inline void tlb_invalidate_all(void) {
    __dsb(ishst);
    asm volatile("tlbi vmalle1is" ::: "memory");
}

static inline void tlb_invalidate_sync(void) {
    __dsb(ish);
    __isb();
}

static inline void tlb_shootdown(const tlb_invl_t* invl) {
    // the hardware already invalidated on all of the cores
    (void)invl;
}
