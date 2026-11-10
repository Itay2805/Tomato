#include "tlb.h"
#include "irq/irq.h"
#include <stddef.h>

void tlb_invl_start(tlb_invl_t* invl) {
    invl->count = 0;
    invl->global = false;
}

void tlb_invl_queue(tlb_invl_t* invl, void* addr, bool global) {
    // past the max everything is invalidated, so there is no need to remember it
    if (invl->count < TLB_INVL_MAX_PAGES) {
        invl->pages[invl->count] = addr;
    }
    invl->count++;
    invl->global |= global;
}

void tlb_invl_apply(const tlb_invl_t* invl) {
    if (invl->count > TLB_INVL_MAX_PAGES) {
        if (invl->global) {
            tlb_invalidate_all();
        } else {
            tlb_invalidate_nonglobal();
        }
    } else {
        for (size_t i = 0; i < invl->count; i++) {
            tlb_invalidate_page(invl->pages[i]);
        }
    }
    tlb_invalidate_sync();
}

void tlb_invl_commit(tlb_invl_t* invl) {
    if (invl->count == 0) {
        return;
    }

    // we invalidate on the current core and the shootdown does it on the rest,
    // so we can't move to another core in between
    irq_state_t state = irq_save();
    tlb_invl_apply(invl);
    tlb_shootdown(invl);
    irq_restore(state);
}
