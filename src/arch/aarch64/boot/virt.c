#include "boot/virt.h"

#include "arch/intrin.h"
#include "arch/virt.h"
#include "boot/limine.h"
#include "lib/assert.h"
#include "mem/mapping.h"
#include "mem/virt.h"
#include "util/defs.h"

[[gnu::section(".limine_requests")]]
static volatile struct limine_paging_mode_request g_paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    // TODO: support for 5lvl paging
    .mode = LIMINE_PAGING_MODE_AARCH64_4LVL,
    .min_mode = LIMINE_PAGING_MODE_AARCH64_4LVL,
    .max_mode = LIMINE_PAGING_MODE_AARCH64_4LVL,
};

void early_virt_init_layout(void) {
    ASSERT(g_paging_mode_request.response != nullptr);

    // figure how many address bits we have (for both kernel and user)
    switch (g_paging_mode_request.response->mode) {
        case LIMINE_PAGING_MODE_AARCH64_4LVL:
            // TTBR1_EL1 covers the top 48 bits, the bootloader's
            // direct map starts right at the bottom of it
            g_kernel_region.base = (void*)0xFFFF000000000000;
            g_kernel_region.page_count = SIZE_256TB / PAGE_SIZE;

            // we save the first 4gb to ensure nothing
            // can be mapped to zero
            g_user_region.base = (void*)SIZE_4GB;
            g_user_region.page_count = (SIZE_256TB - SIZE_4GB) / PAGE_SIZE;
            g_page_table_levels = 4;
            break;

        default:
            ASSERT(0, "Unknown paging mode %lu", g_paging_mode_request.response->mode);
    }

    // both halves are indexed by the same address bits, so each
    // gets its own root, TTBR1_EL1 for the kernel and TTBR0_EL1 for the user
    g_page_table = early_phys_alloc_page();
    g_user_page_table = early_phys_alloc_page();
}

void early_virt_switch_table(void) {
    // make sure the walker sees the tables we built
    __dsb(ishst);

    __write_sysreg(ttbr0_el1, direct_to_phys(g_user_page_table));
    __write_sysreg(ttbr1_el1, direct_to_phys(g_page_table));
    __isb();

    // and drop everything that is left from the bootloader's tables
    asm volatile("tlbi vmalle1" ::: "memory");
    __dsb(nsh);
    __isb();
}