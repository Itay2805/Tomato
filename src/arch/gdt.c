#include "gdt.h"
#include "util/defs.h"

LATE_RO gdt_entries_t g_gdt_entries = {
    .kernel_code = {   // kernel code
        .limit          = 0x0000,
        .base_low       = 0x0000,
        .base_mid       = 0x00,
        .access         = 0b10011011,
        .granularity    = 0b00100000,
        .base_high      = 0x00
    },
    .kernel_data = {   // kernel data
        .limit          = 0x0000,
        .base_low       = 0x0000,
        .base_mid       = 0x00,
        .access         = 0b10010011,
        .granularity    = 0b00000000,
        .base_high      = 0x00
    },
    .user_data = {   // user data
        .limit          = 0x0000,
        .base_low       = 0x0000,
        .base_mid       = 0x00,
        .access         = 0b11110011,
        .granularity    = 0b00000000,
        .base_high      = 0x00
    },
    .user_code = {   // user code
        .limit          = 0x0000,
        .base_low       = 0x0000,
        .base_mid       = 0x00,
        .access         = 0b11111011,
        .granularity    = 0b00100000,
        .base_high      = 0x00
    },
    .tss = {   // TSS
        .length         = 0,
        // Will be filled by the init function
        .low            = 0,
        .mid            = 0,
        .high           = 0,
        .upper32        = 0,
        .flags1         = 0b10001001,
        .flags2         = 0b00000000,
        .reserved       = 0
    }
};