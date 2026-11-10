#include "boot/arch.h"
#include "arch/gic.h"
#include "arch/idreg.h"
#include "arch/intr.h"
#include "arch/intrin.h"
#include "arch/sysreg.h"
#include "arch/virt.h"
#include "boot/virt.h"
#include "cpu.h"
#include "lib/assert.h"
#include "util/defs.h"
#include <stdint.h>

static void arch_validate_required_cpu_features(void) {
    {
        ID_AA64ISAR0_EL1 isar0 = { .raw = __read_sysreg(id_aa64isar0_el1) };

        // armv8.2-a
        ASSERT(isar0.ATOMIC >= 2, "Missing LSE support");
        ASSERT(isar0.CRC32 >= 1, "Missing CRC32 support");
    }

    {
        ID_AA64MMFR0_EL1 mmfr0 = { .raw = __read_sysreg(id_aa64mmfr0_el1) };
        ID_AA64MMFR1_EL1 mmfr1 = { .raw = __read_sysreg(id_aa64mmfr1_el1) };

        // Features unconditionally used by the kernel
        ASSERT(mmfr0.TGRAN4 >= 0, "Missing 4KB granule support");
        ASSERT(mmfr1.PAN >= 1, "Missing PAN support");
    }
}

void arch_early_init(void) {
    arch_validate_required_cpu_features();

    // our memory types, the device mappings in our page tables need them
    mair_init();

    // PAN is our SMAP, set it on every exception entry and turn it on
    uint64_t sctlr = __read_sysreg(sctlr_el1);
    sctlr &= ~SCTLR_SPAN;
    __write_sysreg(sctlr_el1, sctlr);
    __isb();
    asm volatile("msr pan, #1" ::: "memory");

    // until the core has its own cpu locals, point it to the ones
    // in the kernel (which is what the BSP uses)
    __write_sysreg(tpidr_el1, 0);
}

void arch_init(void) {
    // setup the cpu locals and the exception stack for the current core
    cpu_init(0);
    intr_init();
    gic_init();
}

void arch_init_per_core(size_t cpu_id) {
    // setup all the cpu features first
    arch_early_init();

    // switch to the real page table now
    early_virt_switch_table();

    // set the cpu locals and the exception stack
    cpu_init(cpu_id);
    intr_init();
    gic_init_per_core();
}
