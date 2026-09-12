#pragma once

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
 * Page table bits
 */
#define IA32_PG_P      BIT0
#define IA32_PG_RW     BIT1
#define IA32_PG_U      BIT2
#define IA32_PG_WT     BIT3
#define IA32_PG_CD     BIT4
#define IA32_PG_A      BIT5
#define IA32_PG_D      BIT6
#define IA32_PG_PS     BIT7
#define IA32_PG_G      BIT8
#define IA32_PG_PAT_2M BIT12
#define IA32_PG_PAT_4K IA32_PG_PS
#define IA32_PG_PMNT   BIT62
#define IA32_PG_NX     BIT63

/**
 * Page table caching bits (according to Limine PAT)
 */
#define IA32_PG_CACHE_WB    (0)
#define IA32_PG_CACHE_WT    (IA32_PG_WT)
#define IA32_PG_CACHE_UCM   (IA32_PG_CD)
#define IA32_PG_CACHE_UC    (IA32_PG_CD | IA32_PG_WT)
#define IA32_PG_CACHE_WP_4K (IA32_PG_PAT_4K)
#define IA32_PG_CACHE_WC_4K (IA32_PG_PAT_4K | IA32_PG_WT)
#define IA32_PG_CACHE_WP_2M (IA32_PG_PAT_2M)
#define IA32_PG_CACHE_WC_2M (IA32_PG_PAT_2M | IA32_PG_WT)

/**
 * Masking for page table addresses
 */
#define PAGING_4K_ADDRESS_MASK 0x000FFFFFFFFFF000ull
#define PAGING_2M_ADDRESS_MASK 0x000FFFFFFFE00000ull
#define PAGING_1G_ADDRESS_MASK 0x000FFFFFC0000000ull

/**
 * Mask for the index of a page
 */
#define PAGING_INDEX_BITS 9
#define PAGING_INDEX_MASK ((1 << PAGING_INDEX_BITS) - 1)
