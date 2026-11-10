#pragma once

#include "arch/intrin.h"
#include "lib/assert.h"
#include "util/defs.h"
#include <stdint.h>

/**
 * Page size is 4kb
 */
#define PAGE_SHIFT 12
#define PAGE_MASK  ((1 << PAGE_SHIFT) - 1)
#define PAGE_SIZE  (1 << PAGE_SHIFT)

/**
 * Translation table descriptor bits, for the 4kb granule
 */
#define AARCH64_PG_VALID           BIT0
#define AARCH64_PG_TABLE           BIT1 // a table at levels 0-2
#define AARCH64_PG_PAGE            BIT1 // a page at level 3
#define AARCH64_PG_ATTR(index)     ((uint64_t)(index) << 2)
#define AARCH64_PG_AP_EL0          BIT6 // AP[1]
#define AARCH64_PG_AP_RO           BIT7 // AP[2]
#define AARCH64_PG_SH_INNER        (BIT8 | BIT9)
#define AARCH64_PG_AF              BIT10
#define AARCH64_PG_NG              BIT11
#define AARCH64_PG_PXN             BIT53
#define AARCH64_PG_UXN             BIT54
#define AARCH64_PG_PXN_TABLE       BIT59
#define AARCH64_PG_UXN_TABLE       BIT60
#define AARCH64_PG_AP_TABLE_NO_EL0 BIT61

/**
 * The memory types in MAIR_EL1, the bootloader guarantees the first two (normal
 * write-back memory and the framebuffer type) and we add the rest of the caching modes
 */
#define MAIR_ATTR_NORMAL        0
#define MAIR_ATTR_FRAMEBUFFER   1
#define MAIR_ATTR_DEVICE_nGnRnE 2
#define MAIR_ATTR_DEVICE_nGnRE  3
#define MAIR_ATTR_NORMAL_NC     4

#define MAIR_NORMAL_WB       0xFF
#define MAIR_NORMAL_NC       0x44
#define MAIR_DEVICE_nGnRnE   0x00
#define MAIR_DEVICE_nGnRE    0x04
#define MAIR_ATTR_SHIFT(idx) ((idx) * 8)

/**
 * Masking for page table addresses
 */
#define PAGING_4K_ADDRESS_MASK 0x0000FFFFFFFFF000ull

/**
 * Mask for the index of a page
 */
#define PAGING_INDEX_BITS 9
#define PAGING_INDEX_MASK ((1 << PAGING_INDEX_BITS) - 1)

/**
 * Add our memory types to MAIR_EL1, while keeping the ones of the bootloader
 */
static inline void mair_init(void) {
    uint64_t mair = __read_sysreg(mair_el1);
    mair &= ~(0xFFull << MAIR_ATTR_SHIFT(MAIR_ATTR_DEVICE_nGnRnE));
    mair &= ~(0xFFull << MAIR_ATTR_SHIFT(MAIR_ATTR_DEVICE_nGnRE));
    mair &= ~(0xFFull << MAIR_ATTR_SHIFT(MAIR_ATTR_NORMAL_NC));
    mair |= (uint64_t)MAIR_DEVICE_nGnRnE << MAIR_ATTR_SHIFT(MAIR_ATTR_DEVICE_nGnRnE);
    mair |= (uint64_t)MAIR_DEVICE_nGnRE << MAIR_ATTR_SHIFT(MAIR_ATTR_DEVICE_nGnRE);
    mair |= (uint64_t)MAIR_NORMAL_NC << MAIR_ATTR_SHIFT(MAIR_ATTR_NORMAL_NC);
    __write_sysreg(mair_el1, mair);
    __isb();
}
