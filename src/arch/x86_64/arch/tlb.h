#pragma once

#include "arch/intrin.h"
#include "mem/tlb.h"
#include "util/defs.h"

/**
 * TLB primitives, see mem/tlb.h
 *
 * The invalidations only act on the current core, the other cores
 * are told about them with an IPI
 */

static inline void tlb_invalidate_page(void* addr) {
    __invlpg(addr);
}

static inline void tlb_invalidate_nonglobal(void) {
    // everything we map that is not global is tagged with PCID 0
    __invpcid(INVPCID_TYPE_SINGLE_CONTEXT, 0, nullptr);
}

static inline void tlb_invalidate_all(void) {
    __invpcid(INVPCID_TYPE_ALL_CONTEXT_WITH_GLOBALS, 0, nullptr);
}

static inline void tlb_invalidate_sync(void) {
    // the invalidations are done once the instructions return
}

/**
 * Send the batch to all of the other cores and wait for them to apply it
 */
void tlb_shootdown_ipi(const tlb_invl_t* invl);

static inline void tlb_shootdown(const tlb_invl_t* invl) {
    tlb_shootdown_ipi(invl);
}

/**
 * Setup the shootdown IPI, and take part in the shootdowns from the current core
 */
INIT_CODE void tlb_init(void);

/**
 * Take part in the shootdowns from the current core
 */
INIT_CODE void tlb_init_per_core(void);
