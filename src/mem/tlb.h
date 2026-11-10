#pragma once

#include <stddef.h>

/**
 * TLB invalidation, the same api works whether the hardware broadcasts the invalidations
 * to all of the cores (aarch64), or each core has to be told to invalidate on its own with
 * an IPI (x86-64).
 *
 * The invalidations are batched, a page is queued once its entry is changed, and any core
 * may keep using the old translation until the batch is committed, so whatever it pointed
 * to must not be reused before that.
 *
 * NOTE: committing waits for the other cores to invalidate, so it must not be done while
 *       holding a lock that other cores may wait on with interrupts disabled
 */

/**
 * Past this many pages the batch invalidates all of the non-global pages instead, or
 * all of the pages if any of the queued pages is global
 */
#define TLB_INVL_MAX_PAGES 32

typedef struct tlb_invl {
    void* pages[TLB_INVL_MAX_PAGES];
    size_t count;

    /**
     * A global page was queued, so invalidating all of the
     * non-global pages is not enough
     */
    bool global;
} tlb_invl_t;

/**
 * Start a new batch
 */
void tlb_invl_start(tlb_invl_t* invl);

/**
 * Queue a page whose entry was changed, global tells if the old entry was global
 */
void tlb_invl_queue(tlb_invl_t* invl, void* addr, bool global);

/**
 * Invalidate the queued pages on all of the cores, returns once they are all done
 */
void tlb_invl_commit(tlb_invl_t* invl);

/**
 * Invalidate the queued pages on the current core, for the arch's shootdown
 */
void tlb_invl_apply(const tlb_invl_t* invl);

/**
 * TLB primitives, every arch implements them in its arch/tlb.h, they act on the current
 * core, or on all of the cores when the hardware broadcasts the invalidations
 */

/**
 * Start invalidating a single page
 */
static inline void tlb_invalidate_page(void* addr);

/**
 * Start invalidating all of the non-global pages
 */
static inline void tlb_invalidate_nonglobal(void);

/**
 * Start invalidating all of the pages, including the global ones
 */
static inline void tlb_invalidate_all(void);

/**
 * Wait for all the started invalidations to finish
 */
static inline void tlb_invalidate_sync(void);

/**
 * Get all of the other cores to apply the batch and wait for them, there is nothing
 * to do when the hardware broadcasts the invalidations
 */
static inline void tlb_shootdown(const tlb_invl_t* invl);

// the implementation needs tlb_invl_t, so it comes last
#include "arch/tlb.h"
