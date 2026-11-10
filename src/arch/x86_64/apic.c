#include "arch/apic.h"
#include "arch/intr.h"
#include "arch/msr.h"
#include "debug/trace.h"
#include "irq/irq.h"
#include "lib/assert.h"
#include "mem/mapping.h"
#include "mem/vmar.h"
#include "util/except.h"
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LAPIC driver
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define XAPIC_ID_OFFSET                         0x20
#define XAPIC_VERSION_OFFSET                    0x30
#define XAPIC_EOI_OFFSET                        0x0b0
#define XAPIC_ICR_DFR_OFFSET                    0x0e0
#define XAPIC_SPURIOUS_VECTOR_OFFSET            0x0f0
#define XAPIC_ICR_LOW_OFFSET                    0x300
#define XAPIC_ICR_HIGH_OFFSET                   0x310
#define XAPIC_LVT_TIMER_OFFSET                  0x320
#define XAPIC_LVT_LINT0_OFFSET                  0x350
#define XAPIC_LVT_LINT1_OFFSET                  0x360
#define XAPIC_TIMER_INIT_COUNT_OFFSET           0x380
#define XAPIC_TIMER_CURRENT_COUNT_OFFSET        0x390
#define XAPIC_TIMER_DIVIDE_CONFIGURATION_OFFSET 0x3E0

#define X2APIC_MSR_BASE_ADDRESS 0x800
#define X2APIC_MSR_ICR_ADDRESS  0x830

#define LOCAL_APIC_DELIVERY_MODE_FIXED           0
#define LOCAL_APIC_DELIVERY_MODE_LOWEST_PRIORITY 1
#define LOCAL_APIC_DELIVERY_MODE_SMI             2
#define LOCAL_APIC_DELIVERY_MODE_NMI             4
#define LOCAL_APIC_DELIVERY_MODE_INIT            5
#define LOCAL_APIC_DELIVERY_MODE_STARTUP         6
#define LOCAL_APIC_DELIVERY_MODE_EXTINT          7

#define LOCAL_APIC_DESTINATION_SHORTHAND_NO_SHORTHAND       0
#define LOCAL_APIC_DESTINATION_SHORTHAND_SELF               1
#define LOCAL_APIC_DESTINATION_SHORTHAND_ALL_INCLUDING_SELF 2
#define LOCAL_APIC_DESTINATION_SHORTHAND_ALL_EXCLUDING_SELF 3

typedef union [[gnu::packed]] LOCAL_APIC_SVR {
    struct [[gnu::packed]] {
        uint32_t spurious_vector : 8;
        uint32_t software_enable : 1;
        uint32_t focus_processor_checking : 1;
        uint32_t : 2;
        uint32_t eoi_broadcast_suppression : 1;
        uint32_t : 19;
    };
    uint32_t packed;
} LOCAL_APIC_SVR;

typedef union [[gnu::packed]] LOCAL_APIC_DCR {
    struct [[gnu::packed]] {
        uint32_t divide_value_1 : 2;
        uint32_t : 1;
        uint32_t divide_value_2 : 1;
        uint32_t : 28;
    };
    uint32_t packed;
} LOCAL_APIC_DCR;

typedef union [[gnu::packed]] LOCAL_APIC_LVT_LINT {
    struct [[gnu::packed]] {
        uint32_t vector : 8;
        uint32_t delivery_mode : 3;
        uint32_t : 1;
        uint32_t delivery_status : 1;
        uint32_t input_pin_polarity : 1;
        uint32_t remote_irr : 1;
        uint32_t trigger_mode : 1;
        uint32_t mask : 1;
        uint32_t : 15;
    };
    uint32_t packed;
} LOCAL_APIC_LVT_LINT;

typedef union [[gnu::packed]] LOCAL_APIC_LVT_TIMER {
    struct [[gnu::packed]] {
        uint32_t vector : 8;
        uint32_t : 4;
        uint32_t delivery_status : 1;
        uint32_t : 3;
        uint32_t mask : 1;
        uint32_t timer_mode : 2;
        uint32_t : 13;
    };
    uint32_t packed;
} LOCAL_APIC_LVT_TIMER;

typedef union [[gnu::packed]] LOCAL_APIC_ICR_LOW {
    struct [[gnu::packed]] {
        uint32_t vector : 8;
        uint32_t delivery_mode : 3;
        uint32_t destination_mode : 1;
        uint32_t delivery_status : 1;
        uint32_t : 1;
        uint32_t level : 1;
        uint32_t trigger_mode : 1;
        uint32_t : 2;
        uint32_t destination_shorthand : 2;
        uint32_t : 12;
    };
    uint32_t packed;
} LOCAL_APIC_ICR_LOW;

typedef union [[gnu::packed]] LOCAL_APIC_ICR_HIGH {
    struct [[gnu::packed]] {
        uint32_t : 24;
        uint32_t destination : 8;
    };
    uint32_t packed;
} LOCAL_APIC_ICR_HIGH;

/**
 * Are we using x2APIC mode
 */
LATE_RO static bool m_x2apic_mode = false;

/**
 * The xAPIC base, when using xAPIC mode
 */
LATE_RO static uint8_t* m_xapic_base = NULL;

/**
 * Used to validate all cores have the same address
 * TODO: should we enforce it?
 */
INIT_DATA static uint64_t m_xapic_base_phys = 0;

DEFINE_CPU_LOCAL(, uint32_t, apic_id);

static uint32_t lapic_read(size_t offset) {
    if (m_x2apic_mode) {
        return __rdmsr((offset >> 4) + X2APIC_MSR_BASE_ADDRESS);
    } else {
        return *((volatile uint32_t*)(m_xapic_base + offset));
    }
}

static void lapic_write(size_t offset, uint32_t value) {
    if (m_x2apic_mode) {
        __wrmsr((offset >> 4) + X2APIC_MSR_BASE_ADDRESS, value);
    } else {
        *((volatile uint32_t*)(m_xapic_base + offset)) = value;
    }
}

[[gnu::interrupt()]]
static void spurious_interrupt_handler(interrupt_frame_t* frame) {
    (void)frame;
    WARN("apic: got spurious interrupt, ignoring");
}

INIT_CODE void lapic_init(void) {
    // check the apic state
    MSR_IA32_APIC_BASE_REGISTER apic_base = { .packed = __rdmsr(MSR_IA32_APIC_BASE) };
    ASSERT(apic_base.en);

    if (apic_base.extd) {
        m_x2apic_mode = true;
        TRACE("apic: using x2apic");

    } else {
        m_x2apic_mode = false;
        TRACE("apic: using xapic");

        // get the address
        m_xapic_base_phys = apic_base.apic_base << 12;

        // make sure the apic is mapped properly, according to the spec the
        // range should be marked as "Strong Uncacheable"
        vmar_t* xapic_vmar = nullptr;
        ASSERT_SUCCESS(vmar_map_phys(&g_kernel_region, &xapic_vmar, VMAR_ANY_OFFSET, 1,
                                     m_xapic_base_phys, VM_CACHE_UNCACHED_DEVICE));
        vmar_set_name(xapic_vmar, "apic");
        vmar_lock(xapic_vmar);
        ASSERT_SUCCESS(vmar_populate(xapic_vmar));
        m_xapic_base = xapic_vmar->base;
        OBJECT_PUT(xapic_vmar);
    }

    // set the spurious interrupt handler
    intr_set_handler(INTR_VECTOR_SPURIOUS, spurious_interrupt_handler);

    // perform the per-core init
    lapic_init_per_core();
}

INIT_CODE void lapic_init_per_core(void) {
    MSR_IA32_APIC_BASE_REGISTER apic_base = { .packed = __rdmsr(MSR_IA32_APIC_BASE) };
    ASSERT(apic_base.en);

    if (apic_base.extd) {
        // ensure we are using x2apic across all cores
        ASSERT(m_x2apic_mode);

    } else {
        // ensure we are using xapic across all cores, and that
        // the apic address is the same for all of them
        ASSERT(!m_x2apic_mode);

        // validate the address is the same
        uintptr_t addr = apic_base.apic_base << 12;
        ASSERT(addr == m_xapic_base_phys, "Invalid APIC base 0x%lx != 0x%lx", addr,
               m_xapic_base_phys);
    }

    // set the spurious vector
    LOCAL_APIC_SVR svr = {
        .spurious_vector = INTR_VECTOR_SPURIOUS,
        .software_enable = 1,
    };
    lapic_write(XAPIC_SPURIOUS_VECTOR_OFFSET, svr.packed);

    // TODO: set the timer register
}

void lapic_eoi(void) {
    lapic_write(XAPIC_EOI_OFFSET, 0);
}

static void lapic_send_ipi(uint32_t icr_low, uint32_t apic_id) {
    if (m_x2apic_mode) {
        uint64_t value = ((uint64_t)apic_id << 32) | icr_low;
        __wrmsr(X2APIC_MSR_ICR_ADDRESS, value);
    } else {
        // disable interrupts while we perform this to make
        // sure we can't have two people racing
        irq_state_t state = irq_save();

        // wait for delivery to be clear
        LOCAL_APIC_ICR_LOW icr_low_reg = {};
        do {
            icr_low_reg.packed = lapic_read(XAPIC_ICR_LOW_OFFSET);
        } while (icr_low_reg.delivery_status != 0);

        LOCAL_APIC_ICR_HIGH icr_high_reg = { .destination = apic_id };

        // we need to write the high and then the low, the low triggers the ipi
        lapic_write(XAPIC_ICR_HIGH_OFFSET, icr_high_reg.packed);
        lapic_write(XAPIC_ICR_LOW_OFFSET, icr_low);

        // we won't wait for the dispatch to finish, just let it happen, we will
        // poll it anyways if someone is trying to send something afterwards
        irq_restore(state);
    }
}

void lapic_send_ipi_all_excluding_self(uint8_t vector) {
    LOCAL_APIC_ICR_LOW icr = {
        .vector = vector,
        .delivery_mode = LOCAL_APIC_DELIVERY_MODE_FIXED,
        .level = 1,
        .destination_shorthand = LOCAL_APIC_DESTINATION_SHORTHAND_ALL_EXCLUDING_SELF,
    };
    // the destination is ignored when using a shorthand
    lapic_send_ipi(icr.packed, 0);
}
