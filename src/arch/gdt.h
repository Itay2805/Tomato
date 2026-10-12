#pragma once

#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

#define GDT_KERNEL_CODE offsetof(gdt_entries_t, kernel_code)
#define GDT_KERNEL_DATA offsetof(gdt_entries_t, kernel_data)
#define GDT_USER_DATA   offsetof(gdt_entries_t, user_data)
#define GDT_USER_CODE   offsetof(gdt_entries_t, user_code)
#define GDT_TSS         offsetof(gdt_entries_t, tss)

typedef struct [[gnu::packed]] gdt64_entry {
    uint16_t limit;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} gdt64_entry_t;

typedef struct [[gnu::packed]] tss64_entry {
    uint16_t length;
    uint16_t low;
    uint8_t mid;
    uint8_t flags1;
    uint8_t flags2;
    uint8_t high;
    uint32_t upper32;
    uint32_t reserved;
} tss64_entry_t;

typedef struct [[gnu::packed]] gdt_entries {
    // first cacheline
    gdt64_entry_t null;
    gdt64_entry_t _reserved0;
    gdt64_entry_t _reserved1;
    gdt64_entry_t _reserved2;

    // second cacheline
    gdt64_entry_t kernel_code;
    gdt64_entry_t kernel_data;
    gdt64_entry_t user_data;
    gdt64_entry_t user_code;

    // third cacheline
    tss64_entry_t tss;
} gdt_entries_t;

/**
 * The interrupt stack allocation, mostly to deal with
 * unmaskable interrupts
 */
typedef enum tss_ist {
    TSS_IST_DF = 0,
    TSS_IST_NMI,
    TSS_IST_DB,
    TSS_IST_MCE,
    TSS_IST_CP,
    TSS_IST_MAX,
} tss_ist_t;
static_assert(TSS_IST_MAX <= 7);

/**
 * The GDT entries of the kernel.
 */
LATE_RO extern gdt_entries_t g_gdt_entries;
