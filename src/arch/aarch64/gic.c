#include "arch/gic.h"
#include "arch/idreg.h"
#include "arch/intrin.h"
#include "arch/sysreg.h"
#include "debug/trace.h"
#include "lib/assert.h"
#include "util/defs.h"
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GICv3 CPU interface driver
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ICC_SRE_SRE BIT0

#define ICC_CTLR_EOIMODE BIT1

#define ICC_IAR_INTID_MASK 0xFFFFFF

/**
 * INTIDs 1020-1023 are special, they mean there is no interrupt for us
 */
#define GIC_INTID_SPECIAL_FIRST 1020
#define GIC_INTID_SPECIAL_LAST  1023

INIT_CODE void gic_init(void) {
    // check the cpu has the system register interface at all, otherwise
    // even accessing ICC_SRE_EL1 is undefined
    ID_AA64PFR0_EL1 pfr0 = { .raw = __read_sysreg(id_aa64pfr0_el1) };
    ASSERT(pfr0.GIC != 0, "TODO: GICv2 support");

    TRACE("gic: using the GICv3 system register interface");

    // perform the per-core init
    gic_init_per_core();
}

INIT_CODE void gic_init_per_core(void) {
    // ensure we are using the system registers across all cores, VHE doesn't
    // redirect ICC_SRE_EL1, so at EL2 the one that applies to us is ICC_SRE_EL2
    if (__read_sysreg(currentel) == CURRENT_EL_EL2) {
        __write_sysreg(icc_sre_el2, __read_sysreg(icc_sre_el2) | ICC_SRE_SRE);
        __isb();
        ASSERT(__read_sysreg(icc_sre_el2) & ICC_SRE_SRE, "TODO: GICv2 support");
    } else {
        __write_sysreg(icc_sre_el1, __read_sysreg(icc_sre_el1) | ICC_SRE_SRE);
        __isb();
        ASSERT(__read_sysreg(icc_sre_el1) & ICC_SRE_SRE, "TODO: GICv2 support");
    }

    // don't mask any priority, and use all the priority bits for preemption
    __write_sysreg(icc_pmr_el1, 0xFF);
    __write_sysreg(icc_bpr1_el1, 0);

    // writing the EOI both drops the priority and deactivates the interrupt
    uint64_t ctlr = __read_sysreg(icc_ctlr_el1);
    __write_sysreg(icc_ctlr_el1, ctlr & ~ICC_CTLR_EOIMODE);

    // and enable the non-secure group 1 interrupts, those are delivered to us as IRQs
    __write_sysreg(icc_igrpen1_el1, 1);
    __isb();

    // TODO: wake up the redistributor and enable the distributor, this needs their
    //       addresses from the device tree or the MADT
    // TODO: set the timer register
}

void gic_handle_irq(void) {
    uint32_t intid = __read_sysreg(icc_iar1_el1) & ICC_IAR_INTID_MASK;
    if (intid >= GIC_INTID_SPECIAL_FIRST && intid <= GIC_INTID_SPECIAL_LAST) {
        WARN("gic: got spurious interrupt, ignoring");
        return;
    }

    // nothing routes interrupts to us yet
    WARN("gic: got unexpected interrupt %u, ignoring", intid);
    __write_sysreg(icc_eoir1_el1, intid);
}
