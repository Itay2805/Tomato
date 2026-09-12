#pragma once

#include <stdint.h>

#define MSR_IA32_UMWAIT_CONTROL 0xE1

#define MSR_IA32_STAR  0xC0000081
#define MSR_IA32_LSTAR 0xC0000082
#define MSR_IA32_CSTAR 0xC0000083
#define MSR_IA32_FMASK 0xC0000084

#define MSR_IA32_FS_BASE        0xC0000100
#define MSR_IA32_GS_BASE        0xC0000101
#define MSR_IA32_KERNEL_GS_BASE 0xC0000102

#define MSR_IA32_TSC_AUX 0xC0000103

#define MSR_IA32_APIC_BASE 0x0000001B

typedef union {
    struct {
        uint64_t : 8;
        uint64_t bsp : 1;
        uint64_t : 1;
        uint64_t extd : 1;
        uint64_t en : 1;
        uint64_t apic_base : 52;
    };
    uint64_t packed;
} MSR_IA32_APIC_BASE_REGISTER;

#define MSR_IA32_EFER 0xC0000080

typedef union {
    struct {
        uint32_t sce : 1;
        uint32_t : 7;
        uint32_t lme : 1;
        uint32_t : 1;
        uint32_t lma : 1;
        uint32_t nxe : 1;
        uint32_t : 20;
        uint32_t : 32;
    };
    uint32_t packed;
} MSR_IA32_EFER_REGISTER;

#define MSR_IA32_U_CET 0x6A0
#define MSR_IA32_S_CET 0x6A2

typedef union {
    struct {
        uint64_t SH_STK_EN : 1;
        uint64_t WR_SHSTK_EN : 1;
        uint64_t ENDBR_EN : 1;
        uint64_t LEG_IW_EN : 1;
        uint64_t NO_TRACK_EN : 1;
        uint64_t SUPPRESS_DIS : 1;
        uint64_t : 4;
        uint64_t SUPPRESS : 1;
        uint64_t TRACKER : 1;
        uint64_t EB_LEG_BITMAP_BASE : 52;
    };
    uint64_t raw;
} MSR_IA32_CET_REGISTER;

#define MSR_IA32_PL0_SSP 0x6A4
#define MSR_IA32_PL1_SSP 0x6A5
#define MSR_IA32_PL2_SSP 0x6A6
#define MSR_IA32_PL3_SSP 0x6A7

#define MSR_IA32_INTERRUPT_SSP_TABLE_ADDR 0x6A8

#define MSR_IA32_TSC_DEADLINE 0x6E0

#define MSR_KVM_WALL_CLOCK_NEW    0x4b564d00
#define MSR_KVM_SYSTEM_TIME_NEW   0x4b564d01
#define MSR_KVM_WALL_CLOCK        0x11
#define MSR_KVM_SYSTEM_TIME       0x12
#define MSR_KVM_ASYNC_PF_EN       0x4b564d02
#define MSR_KVM_STEAL_TIME        0x4b564d03
#define MSR_KVM_EOI_EN            0x4b564d04
#define MSR_KVM_POLL_CONTROL      0x4b564d05
#define MSR_KVM_ASYNC_PF_INT      0x4b564d06
#define MSR_KVM_ASYNC_PF_ACK      0x4b564d07
#define MSR_KVM_MIGRATION_CONTROL 0x4b564d08

static inline void __wrmsr(uint32_t index, uint64_t value) {
    uint32_t low_data = value;
    uint32_t high_data = value >> 32;
    __asm__ __volatile__("wrmsr" : : "c"(index), "a"(low_data), "d"(high_data));
}

static inline uint64_t __rdmsr(uint32_t index) {
    uint32_t low_data;
    uint32_t high_data;
    __asm__ __volatile__("rdmsr" : "=a"(low_data), "=d"(high_data) : "c"(index));
    return low_data | ((uint64_t)high_data << 32);
}