#pragma once

#include <cpuid.h>
#include <stdint.h>

#define CPUID_VERSION_INFO 0x01

typedef union {
    struct {
        uint32_t SSE3 : 1;
        uint32_t PCLMULQDQ : 1;
        uint32_t DTES64 : 1;
        uint32_t MONITOR : 1;
        uint32_t DS_CPL : 1;
        uint32_t VMX : 1;
        uint32_t SMX : 1;
        uint32_t EIST : 1;
        uint32_t TM2 : 1;
        uint32_t SSSE3 : 1;
        uint32_t L1_CONTEXT_ID : 1;
        uint32_t DEBUG_INTERFACE : 1;
        uint32_t FMA : 1;
        uint32_t CMPXCHG16B : 1;
        uint32_t XTPR_UPDATE_CONTROL : 1;
        uint32_t PERF_CAPABILITIES : 1;
        uint32_t : 1;
        uint32_t PCID : 1;
        uint32_t DCA : 1;
        uint32_t SSE4_1 : 1;
        uint32_t SSE4_2 : 1;
        uint32_t x2APIC : 1;
        uint32_t MOVBE : 1;
        uint32_t POPCNT : 1;
        uint32_t TSC_DEADLINE : 1;
        uint32_t AESNI : 1;
        uint32_t XSAVE : 1;
        uint32_t OSXSAVE : 1;
        uint32_t AVX : 1;
        uint32_t F16C : 1;
        uint32_t RDRAND : 1;
        uint32_t ParaVirtualized : 1;
    };
    uint32_t raw;
} CPUID_VERSION_INFO_ECX;

typedef union {
    struct {
        uint32_t FPU : 1;
        uint32_t VME : 1;
        uint32_t DE : 1;
        uint32_t PSE : 1;
        uint32_t TSC : 1;
        uint32_t MSR : 1;
        uint32_t PAE : 1;
        uint32_t MCE : 1;
        uint32_t CMPXCHG8B : 1;
        uint32_t APIC : 1;
        uint32_t : 1;
        uint32_t SEP : 1;
        uint32_t MTRR : 1;
        uint32_t PGE : 1;
        uint32_t MCA : 1;
        uint32_t CMOV : 1;
        uint32_t PAT : 1;
        uint32_t PSE_36 : 1;
        uint32_t PSN : 1;
        uint32_t CLFLUSH : 1;
        uint32_t : 1;
        uint32_t DS : 1;
        uint32_t ACPI : 1;
        uint32_t MMX : 1;
        uint32_t FXSR : 1;
        uint32_t SSE : 1;
        uint32_t SSE2 : 1;
        uint32_t SELF_SNOOP : 1;
        uint32_t HTT : 1;
        uint32_t TM : 1;
        uint32_t : 1;
        uint32_t PBE : 1;
    };
    uint32_t raw;
} CPUID_VERSION_INFO_EDX;

#define CPUID_MONITOR_MWAIT 0x05

typedef union {
    struct {
        uint32_t SMALLEST_MONITOR_LINE_SIZE : 16;
        uint32_t : 16;
    };
    uint32_t raw;
} CPUID_MONITOR_MWAIT_EAX;

typedef union {
    struct {
        uint32_t LARGEST_MONITOR_LINE_SIZE : 16;
        uint32_t : 16;
    };
    uint32_t raw;
} CPUID_MONITOR_MWAIT_EBX;

typedef union {
    struct {
        uint32_t MONITOR_MWAIT_EXTENSIONS : 1;
        uint32_t INTERRUPT_AS_BREAK_EVENT : 1;
        uint32_t : 30;
    };
    uint32_t raw;
} CPUID_MONITOR_MWAIT_ECX;

typedef union {
    struct {
        uint32_t C0_SUB_STATES : 4;
        uint32_t C1_SUB_STATES : 4;
        uint32_t C2_SUB_STATES : 4;
        uint32_t C3_SUB_STATES : 4;
        uint32_t C4_SUB_STATES : 4;
        uint32_t C5_SUB_STATES : 4;
        uint32_t C6_SUB_STATES : 4;
        uint32_t C7_SUB_STATES : 4;
    };
    uint32_t raw;
} CPUID_MONITOR_MWAIT_EDX;

#define CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS 0x07

#define CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_SUB_LEAF_INFO 0x00

typedef union {
    struct {
        uint32_t FSGSBASE : 1;
        uint32_t TSC_ADJUST : 1;
        uint32_t SGX : 1;
        uint32_t BMI1 : 1;
        uint32_t HLE : 1;
        uint32_t AVX2 : 1;
        uint32_t FDP_EXCPTN_ONLY : 1;
        uint32_t SMEP : 1;
        uint32_t BMI2 : 1;
        uint32_t ENH_REP_MOVSB_STOSB : 1;
        uint32_t INVPCID : 1;
        uint32_t RTM : 1;
        uint32_t RDT_M : 1;
        uint32_t FCS_FDS_DEPRECATION : 1;
        uint32_t MPX : 1;
        uint32_t RDT_A : 1;
        uint32_t AVX512F : 1;
        uint32_t AVX512DQ : 1;
        uint32_t RDSEED : 1;
        uint32_t ADX : 1;
        uint32_t SMAP : 1;
        uint32_t AVX512_IFMA : 1;
        uint32_t : 1;
        uint32_t CLFLUSHOPT : 1;
        uint32_t CLWB : 1;
        uint32_t INTEL_PROC_TRACE : 1;
        uint32_t AVX512PF : 1;
        uint32_t AVX512ER : 1;
        uint32_t AVX512CD : 1;
        uint32_t SHA : 1;
        uint32_t AVX512BW : 1;
        uint32_t AVX512VL : 1;
    };
    uint32_t raw;
} CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_EBX;

typedef union {
    struct {
        uint32_t PREFETCHWT1 : 1;
        uint32_t AVX512_VBMI : 1;
        uint32_t UMIP : 1;
        uint32_t PKU : 1;
        uint32_t OSPKE : 1;
        uint32_t WAITPKG : 1;
        uint32_t AVX512_VBMI2 : 1;
        uint32_t CET_SS : 1;
        uint32_t GFNI : 1;
        uint32_t VAES : 1;
        uint32_t VPCLMULQDQ : 1;
        uint32_t AVX512_VNNI : 1;
        uint32_t AVX512_BITALG : 1;
        uint32_t TME_EN : 1;
        uint32_t AVX512_VPOPCNTDQ : 1;
        uint32_t : 1;
        uint32_t LA57 : 1;
        uint32_t MPX_MAWAU : 5;
        uint32_t RDPID : 1;
        uint32_t KEY_LOCKER : 1;
        uint32_t BUS_LOCK_DETECT : 1;
        uint32_t CLDEMOTE : 1;
        uint32_t : 1;
        uint32_t MOVDIRI : 1;
        uint32_t MOVDIR64B : 1;
        uint32_t ENQCMD : 1;
        uint32_t SGX_LC : 1;
        uint32_t PKS : 1;
    };
    uint32_t raw;
} CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_ECX;

typedef union {
    struct {
        uint32_t : 1;
        uint32_t SGX_KEYS : 1;
        uint32_t AVX512_4VNNIW : 1;
        uint32_t AVX512_4FMAPS : 1;
        uint32_t FAST_SHORT_REP_MOVSB : 1;
        uint32_t UINTR : 1;
        uint32_t : 2;
        uint32_t AVX512_VP2INTERSECT : 1;
        uint32_t MCU_OPT_CTRL : 1;
        uint32_t MD_CLEAR : 1;
        uint32_t RTM_ALWAYS_ABORT : 1;
        uint32_t : 1;
        uint32_t RTM_FORCE_ABORT : 1;
        uint32_t SERIALIZE : 1;
        uint32_t HYBRID : 1;
        uint32_t TSXLDTRK : 1;
        uint32_t : 1;
        uint32_t PCONFIG : 1;
        uint32_t ARCH_LBRS : 1;
        uint32_t CET_IBT : 1;
        uint32_t : 1;
        uint32_t AMX_BF16 : 1;
        uint32_t AVX512_FP16 : 1;
        uint32_t AMX_TILE : 1;
        uint32_t AMX_INT8 : 1;
        uint32_t IBRS_IBPB : 1;
        uint32_t SPEC_CTRL_ST_PREDICTORS : 1;
        uint32_t L1D_FLUSH_INTERFACE : 1;
        uint32_t ARCH_CAPABILITIES : 1;
        uint32_t CORE_CAPABILITIES : 1;
        uint32_t SPEC_CTRL_SSBD : 1;
    };
    uint32_t raw;
} CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_EDX;

#define CPUID_EXTENDED_STATE 0x0D

#define CPUID_EXTENDED_STATE_MAIN_LEAF 0x00

typedef union {
    struct {
        uint32_t x87 : 1;
        uint32_t SSE : 1;
        uint32_t AVX : 1;
        uint32_t MPX_BNDREGS : 1;
        uint32_t MPX_BNDCSR : 1;
        uint32_t AVX512_OPMASK : 1;
        uint32_t AVX512_ZMM_HI256 : 1;
        uint32_t AVX512_HI16_ZMM : 1;
        uint32_t : 1;
        uint32_t PKRU : 1;
        uint32_t : 7;
        uint32_t AMX_TILECFG : 1;
        uint32_t AMX_TILEDATA : 1;
        uint32_t : 13;
    };
    uint32_t raw;
} CPUID_EXTENDED_STATE_MAIN_LEAF_EAX;

#define CPUID_EXTENDED_STATE_SUB_LEAF 0x01

typedef union {
    struct {
        uint32_t XSAVEOPT : 1;
        uint32_t XSAVEC : 1;
        uint32_t XGETBV1 : 1;
        uint32_t XSAVES : 1;
        uint32_t XFD : 1;
        uint32_t : 27;
    };
    uint32_t raw;
} CPUID_EXTENDED_STATE_SUB_LEAF_EAX;

typedef union {
    struct {
        uint32_t : 8;
        uint32_t PT : 1;
        uint32_t : 1;
        uint32_t PASID : 1;
        uint32_t CET_U : 1;
        uint32_t CET_S : 1;
        uint32_t HDC : 1;
        uint32_t UINTR : 1;
        uint32_t LBR : 1;
        uint32_t HWP : 1;
        uint32_t : 15;
    };
    uint32_t raw;
} CPUID_EXTENDED_STATE_SUB_LEAF_ECX;

#define CPUID_TIME_STAMP_COUNTER 0x15

#define CPUID_EXTENDED_CPU_SIG 0x80000001

typedef union {
    struct {
        uint32_t LAHF_SAHF_64 : 1;
        uint32_t : 4;
        uint32_t LZCNT : 1;
        uint32_t : 2;
        uint32_t PREFETCHW : 1;
        uint32_t : 23;
    };
    uint32_t raw;
} CPUID_EXTENDED_CPU_SIG_ECX;

typedef union {
    struct {
        uint32_t : 11;
        uint32_t SYSCALL_SYSRET_64 : 1;
        uint32_t : 8;
        uint32_t EXECUTE_DIS : 1;
        uint32_t : 5;
        uint32_t PAGE_1GB : 1;
        uint32_t RDTSCP : 1;
        uint32_t : 1;
        uint32_t INTEL64 : 1;
        uint32_t : 2;
    };
    uint32_t raw;
} CPUID_EXTENDED_CPU_SIG_EDX;

#define CPUID_EXTENDED_TIME_STAMP_COUNTER 0x80000007

typedef union {
    struct {
        uint32_t : 8;
        uint32_t TSC_INVARIANT : 1;
        uint32_t : 23;
    };
    uint32_t raw;
} CPUID_EXTENDED_TIME_STAMP_COUNTER_EDX;

#define CPUID_VIR_PHY_ADDRESS_SIZE 0x80000008

typedef union {
    struct {
        uint32_t PHYS_ADDR_SIZE : 8;
        uint32_t LIN_ADDR_SIZE : 8;
        uint32_t GUEST_PHYS_ADDR_SIZE : 8;
        uint32_t : 8;
    };
    uint32_t raw;
} CPUID_VIR_PHY_ADDRESS_SIZE_EAX;

#define KVM_CPUID_SIGNATURE 0x40000000

#define KVM_CPUID_FEATURES 0x40000001
typedef union {
    struct {
        uint32_t CLOCKSOURCE : 1;
        uint32_t NOP_IO_DELAY : 1;
        uint32_t MMU_OP : 1;
        uint32_t CLOCKSOURCE2 : 1;
        uint32_t ASYNC_PF : 1;
        uint32_t STEAL_TIME : 1;
        uint32_t PV_EOI : 1;
        uint32_t PV_UNHALT : 1;
        uint32_t PV_TLB_FLUSH : 1;
        uint32_t ASYNC_PF_VMEXIT : 1;
        uint32_t PV_SEND_IPI : 1;
        uint32_t POLL_CONTROL : 1;
        uint32_t PV_SCHED_YIELD : 1;
        uint32_t ASYNC_PF_INT : 1;
        uint32_t MSI_EXT_DEST_ID : 1;
        uint32_t HC_MAP_GPA_RANGE : 1;
        uint32_t MIGRATION_CONTROL : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t CLOCKSOURCE_STABLE_BIT : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
        uint32_t : 1;
    };
    uint32_t raw;
} CPUID_KVM_FEATURES_EAX;

static inline bool cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx,
                         uint32_t* edx) {
    uint32_t a, b, c, d;
    if (!__get_cpuid(leaf, &a, &b, &c, &d)) {
        return false;
    }

    if (eax != nullptr)
        *eax = a;
    if (ebx != nullptr)
        *ebx = b;
    if (ecx != nullptr)
        *ecx = c;
    if (edx != nullptr)
        *edx = d;

    return true;
}

static inline bool cpuid_count(uint32_t leaf, uint32_t count, uint32_t* eax, uint32_t* ebx,
                               uint32_t* ecx, uint32_t* edx) {
    uint32_t a, b, c, d;
    if (!__get_cpuid_count(leaf, count, &a, &b, &c, &d)) {
        return false;
    }

    if (eax != nullptr)
        *eax = a;
    if (ebx != nullptr)
        *ebx = b;
    if (ecx != nullptr)
        *ecx = c;
    if (edx != nullptr)
        *edx = d;

    return true;
}
