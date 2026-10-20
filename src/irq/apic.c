#include "apic.h"
#include "arch/intr.h"
#include "arch/msr.h"
#include "irq/intr.h"
#include "irq/irq.h"
#include "lib/assert.h"
#include "lib/trace.h"
#include "mem/mapping.h"
#include "util/except.h"

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

// /**
//  * The xAPIC base, when using xAPIC mode
//  */
// LATE_RO static uint8_t* m_xapic_base = NULL;

CPU_LOCAL(, uint32_t, g_apic_id);

// static uint32_t lapic_read(size_t offset) {
//     if (m_x2apic_mode) {
//         __asm__("" ::: "memory");
//         return __rdmsr((offset >> 4) + X2APIC_MSR_BASE_ADDRESS);
//     } else {
//         ASSERT(!"TODO: xapic support");
//         // return *((uint32_t*)(m_xapic_base + offset));
//     }
// }

static void lapic_write(size_t offset, uint32_t value) {
    if (m_x2apic_mode) {
        __asm__("" ::: "memory");
        __wrmsr((offset >> 4) + X2APIC_MSR_BASE_ADDRESS, value);
    } else {
        ASSERT(!"TODO: xapic support");
        // *((uint32_t*)(m_xapic_base + offset)) = value;
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

        // make sure the apic is mapped properly, according to the spec the
        // range should be marked as "Strong Uncacheable"
        ASSERT(!"TODO: xapic support");
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
        ASSERT(!"TODO: xapic support");
        // CHECK(m_xapic_base == phys_to_direct(apic_base.apic_base << 12));
    }

    // set the spurious vector
    LOCAL_APIC_SVR svr = {
        .spurious_vector = INTR_VECTOR_SPURIOUS,
        .software_enable = 1,
    };
    lapic_write(XAPIC_SPURIOUS_VECTOR_OFFSET, svr.packed);

    // TODO: set the timer register
}
