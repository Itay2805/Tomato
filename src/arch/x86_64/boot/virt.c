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
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .min_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .max_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
};

void early_virt_init_layout(void) {
    ASSERT(g_paging_mode_request.response != nullptr);

    // figure how many address bits we have (for both kernel and user)
    switch (g_paging_mode_request.response->mode) {
        case LIMINE_PAGING_MODE_X86_64_4LVL:
            g_kernel_region.base = (void*)0xFFFF800000000000;
            g_kernel_region.page_count = SIZE_128TB / PAGE_SIZE;

            // we save the first 4gb to ensure nothing
            // can be mapped to zero
            g_user_region.base = (void*)SIZE_4GB;

            // we don't allow to map the last canonical page to deal with
            // the issues that come from Intel's SYSRET implementation faulting
            // on a non-canonical rip, this ensures we just can't have that issue
            g_user_region.page_count = ((SIZE_128TB - SIZE_4GB - PAGE_SIZE) / PAGE_SIZE);
            g_page_table_levels = 4;
            break;

        default:
            ASSERT(0, "Unknown paging mode %lu", g_paging_mode_request.response->mode);
    }

    // a single root covers both the kernel and the user halves
    g_page_table = early_phys_alloc_page();
    g_user_page_table = g_page_table;
}

void early_virt_switch_table(void) {
    __writecr3(direct_to_phys(g_page_table));
}