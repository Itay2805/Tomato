#include "arch.h"
#include "arch/cpuid.h"
#include "arch/intrin.h"
#include "arch/msr.h"
#include "boot/gdt.h"
#include "boot/virt.h"
#include "irq/intr.h"
#include "lib/assert.h"
#include "mem/phys.h"
#include "util/defs.h"
#include <stdint.h>

#define CR0_PG BIT31
#define CR0_CD BIT30
#define CR0_NW BIT29
#define CR0_AM BIT18
#define CR0_WP BIT16
#define CR0_NE BIT5
#define CR0_ET BIT4
#define CR0_TS BIT3
#define CR0_EM BIT2
#define CR0_MP BIT1
#define CR0_PE BIT0

#define CR4_VME        BIT0
#define CR4_PVI        BIT1
#define CR4_TSD        BIT2
#define CR4_DE         BIT3
#define CR4_PSE        BIT4
#define CR4_PAE        BIT5
#define CR4_MCE        BIT6
#define CR4_PGE        BIT7
#define CR4_PCE        BIT8
#define CR4_OSFXSR     BIT9
#define CR4_OSXMMEXCPT BIT10
#define CR4_UMIP       BIT11
#define CR4_LA57       BIT12
#define CR4_VMXE       BIT13
#define CR4_SMXE       BIT14
#define CR4_FSGSBASE   BIT16
#define CR4_PCIDE      BIT17
#define CR4_OSXSAVE    BIT18
#define CR4_KL         BIT19
#define CR4_SMEP       BIT20
#define CR4_SMAP       BIT21
#define CR4_PKE        BIT22
#define CR4_CET        BIT23
#define CR4_PKS        BIT24
#define CR4_UINTR      BIT25

static void arch_validate_required_cpu_features(void) {
    {
        CPUID_VERSION_INFO_ECX ecx;
        ASSERT(cpuid(CPUID_VERSION_INFO, nullptr, nullptr, &ecx.raw, nullptr));

        // x86-64-v3
        ASSERT(ecx.XSAVE, "Missing XSAVE support");
        ASSERT(ecx.SSE3, "Missing SSE3 support");
        ASSERT(ecx.SSSE3, "Missing SSSE3 support");
        ASSERT(ecx.FMA, "Missing FMA support");
        ASSERT(ecx.CMPXCHG16B, "Missing CMPXCHG16B support");
        ASSERT(ecx.SSE4_1, "Missing SSE4_1 support");
        ASSERT(ecx.SSE4_2, "Missing SSE4_2 support");
        ASSERT(ecx.MOVBE, "Missing MOVBE support");
        ASSERT(ecx.POPCNT, "Missing POPCNT support");
        ASSERT(ecx.AVX, "Missing AVX support");
        ASSERT(ecx.F16C, "Missing F16C support");
    }

    {
        CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_EBX ebx;
        CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_ECX ecx;
        ASSERT(cpuid_count(CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS,
                           CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_SUB_LEAF_INFO, nullptr, &ebx.raw,
                           &ecx.raw, nullptr));

        // x86-64-v3
        ASSERT(ebx.BMI1, "Missing BMI1 support");
        ASSERT(ebx.AVX2, "Missing AVX2 support");
        ASSERT(ebx.BMI2, "Missing BMI2 support");

        // Features unconditionally used by the kernel
        ASSERT(ebx.SMAP, "Missing SMAP support");
        ASSERT(ebx.SMEP, "Missing SMEP support");
        ASSERT(ebx.INVPCID, "Missing INVPCID support");
        ASSERT(ecx.UMIP, "Missing UMIP support");
    }

    {
        CPUID_EXTENDED_CPU_SIG_ECX ecx;
        CPUID_EXTENDED_CPU_SIG_EDX edx;
        ASSERT(cpuid(CPUID_EXTENDED_CPU_SIG, nullptr, nullptr, &ecx.raw, &edx.raw));

        // x86-64-v3
        ASSERT(ecx.LAHF_SAHF_64, "Missing LAHF_SAHF_64 support");
        ASSERT(ecx.LZCNT, "Missing LZCNT support");

        // Features unconditionally used by the kernel
        ASSERT(edx.SYSCALL_SYSRET_64, "Missing SYSCALL_SYSRET_64 support");
        ASSERT(edx.EXECUTE_DIS, "Missing EXECUTE_DIS support");
    }
}

void arch_early_init(void) {
    arch_validate_required_cpu_features();

    __writecr0(CR0_PG | CR0_PE | CR0_MP | CR0_WP | CR0_NE);

    uint32_t cr4 = 0;
    cr4 |= CR4_PAE;
    cr4 |= CR4_PGE;
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    cr4 |= CR4_OSXSAVE;
    cr4 |= CR4_SMAP | CR4_SMEP;
    cr4 |= CR4_UMIP;
    __writecr4(cr4);

    MSR_IA32_EFER_REGISTER efer = { .packed = __rdmsr(MSR_IA32_EFER) };
    efer.nxe = 1;
    efer.sce = 1;
    __wrmsr(MSR_IA32_EFER, efer.packed);
}

void arch_init(void) {
    // setup the gdt for the current core
    gdt_init();
    intr_init();
}

void arch_init_per_core(void) {
    // setup all the cpu features first
    arch_early_init();

    // switch to the real page table now
    early_virt_switch_table();

    // set the gdt
    gdt_init();
    intr_init();
}
