#include "mem/tlb.h"
#include "arch/apic.h"
#include "arch/intr.h"
#include "arch/intrin.h"
#include "sync/smp.h"
#include "util/defs.h"
#include <stdatomic.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TLB shootdown
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The part each core takes in the shootdowns
 */
typedef struct tlb_shootdown_cpu {
    /**
     * The core can take the IPI, until then the shootdowns don't wait for it
     */
    atomic_bool online;

    /**
     * The core still has to apply the shootdown in progress
     */
    atomic_bool pending;
} tlb_shootdown_cpu_t;

DEFINE_CPU_LOCAL(static, tlb_shootdown_cpu_t, tlb_shootdown);

/**
 * The shootdown in progress, there is only one at a time
 */
static atomic_flag m_shootdown_lock = ATOMIC_FLAG_INIT;
static const tlb_invl_t* m_shootdown_invl = nullptr;
static atomic_size_t m_shootdown_remaining = 0;

/**
 * Apply the shootdown in progress if it is waiting on the current core
 */
static void tlb_shootdown_poll(void) {
    tlb_shootdown_cpu_t* cpu = CPU_LOCAL_PTR(tlb_shootdown);
    if (!atomic_exchange_explicit(&cpu->pending, false, memory_order_acquire)) {
        return;
    }

    tlb_invl_apply(m_shootdown_invl);
    atomic_fetch_sub_explicit(&m_shootdown_remaining, 1, memory_order_release);
}

[[gnu::interrupt()]]
static void tlb_shootdown_handler(interrupt_frame_t* frame) {
    swapgs(frame);
    tlb_shootdown_poll();
    lapic_eoi();
    swapgs(frame);
}

void tlb_shootdown_ipi(const tlb_invl_t* invl) {
    // only one shootdown at a time, while waiting for our turn we must still take part in
    // the one in progress, interrupts are disabled so we won't get its IPI
    while (atomic_flag_test_and_set_explicit(&m_shootdown_lock, memory_order_acquire)) {
        tlb_shootdown_poll();
        cpu_relax();
    }

    // the locked instruction orders the entries we changed before checking which cores are
    // online, a core that comes online after that invalidates everything on its own
    m_shootdown_invl = invl;

    size_t self = CPU_LOCAL(cpu_id);
    size_t targets = 0;
    for (size_t i = 0; i < g_cpu_count; i++) {
        tlb_shootdown_cpu_t* cpu = CPU_LOCAL_PTR_OF(tlb_shootdown, i);
        if (i == self || !atomic_load_explicit(&cpu->online, memory_order_acquire)) {
            continue;
        }

        // count it before it can see the shootdown, so it can't finish before being counted
        atomic_fetch_add_explicit(&m_shootdown_remaining, 1, memory_order_relaxed);
        atomic_store_explicit(&cpu->pending, true, memory_order_release);
        targets++;
    }

    if (targets != 0) {
        lapic_send_ipi_all_excluding_self(INTR_VECTOR_TLB_SHOOTDOWN);
        while (atomic_load_explicit(&m_shootdown_remaining, memory_order_acquire) != 0) {
            cpu_relax();
        }
    }

    atomic_flag_clear_explicit(&m_shootdown_lock, memory_order_release);
}

INIT_CODE void tlb_init(void) {
    intr_set_handler(INTR_VECTOR_TLB_SHOOTDOWN, tlb_shootdown_handler);

    // perform the per-core init
    tlb_init_per_core();
}

INIT_CODE void tlb_init_per_core(void) {
    // from now on the shootdowns wait for us, and anything we
    // cached before that might already be stale
    atomic_store_explicit(&CPU_LOCAL_PTR(tlb_shootdown)->online, true, memory_order_seq_cst);
    tlb_invalidate_nonglobal();
}
