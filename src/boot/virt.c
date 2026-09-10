#include "virt.h"

#include "arch/intrin.h"
#include "arch/virt.h"
#include "lib/assert.h"
#include "lib/string.h"
#include "lib/trace.h"
#include "limine.h"
#include "mem/mapping.h"
#include "mem/vmar.h"
#include "util/defs.h"

#include <stdint.h>

[[gnu::section(".limine_requests")]]
static volatile struct limine_paging_mode_request g_paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
#ifdef __x86_64__
    // TODO: support for 5lvl paging
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .min_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .max_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
#else
    #error Unknown arch
#endif
};

[[gnu::section(".limine_requests")]]
static volatile struct limine_hhdm_request g_hhdm_request = { .id = LIMINE_HHDM_REQUEST_ID,
                                                              .revision = 0,
                                                              .response = nullptr };

[[gnu::section(".limine_requests")]]
static volatile struct limine_executable_address_request g_executable_address = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID, .revision = 0, .response = nullptr
};

[[gnu::section(".limine_requests")]]
static volatile struct limine_memmap_request m_memmap_request = { .id = LIMINE_MEMMAP_REQUEST_ID,
                                                                  .revision = 0,
                                                                  .response = nullptr };

static uintptr_t m_early_alloc_top = 0;
static size_t m_early_alloc_size = 0;

static void early_alloc_find_next_entry(void) {
    struct limine_memmap_response* response = m_memmap_request.response;
    ASSERT(response != NULL);

    // we assume this is sorted by address
    for (size_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];
        // must be a usable entry
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        // must be above the current top (we assume this is called only
        // once we are out of memory to allocate)
        if (entry->base < m_early_alloc_top) {
            continue;
        }

        m_early_alloc_top = entry->base;
        m_early_alloc_size = entry->length;
        ASSERT((m_early_alloc_top % PAGE_SIZE) == 0);
        ASSERT((m_early_alloc_size % PAGE_SIZE) == 0);
        return;
    }

    ASSERT(!"Ran out of memory during boot");
}

void* early_phys_alloc_page(void) {
    if (m_early_alloc_size == 0) {
        early_alloc_find_next_entry();
    }

    uintptr_t phys = m_early_alloc_top;
    m_early_alloc_top += PAGE_SIZE;
    m_early_alloc_size -= PAGE_SIZE;

    void* ptr = phys_to_direct(phys);
    memset(ptr, 0, PAGE_SIZE);
    return ptr;
}

//
// Setup early regions
//

static int init_kernel_region(void) {
    ASSERT(g_paging_mode_request.response != nullptr);

    // figure how many address bits we have (for both kernel and user)
    switch (g_paging_mode_request.response->mode) {
#ifdef __x86_64__
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
            return 4;
#else
    #error Unknown arch
#endif
        default:
            ASSERT(0, "Unknown paging mode %lu", g_paging_mode_request.response->mode);
    }
}

static void init_direct_map(void) {
    // set the direct map base and length
    ASSERT(g_hhdm_request.response != NULL);
    ASSERT(m_memmap_request.response != NULL);

    // find the top most address that we need to store in kernel
    uintptr_t top_address = 0;
    for (int64_t i = m_memmap_request.response->entry_count - 1; i >= 0; i--) {
        struct limine_memmap_entry* entry = m_memmap_request.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            top_address = entry->base + entry->length;
            break;
        }
    }

    // set the region, we will link it later
    g_direct_map.base = (void*)g_hhdm_request.response->offset;
    g_direct_map.page_count = top_address / PAGE_SIZE;
}

//
// Generic early utilities for page table manipulation
//

static void* m_page_table = nullptr;
static size_t m_page_table_levels = 0;

static uint64_t* early_virt_get_next_level(uint64_t* entry) {
    if ((*entry & IA32_PG_P) == 0) {
        void* phys = early_phys_alloc_page();
        *entry = direct_to_phys(phys) | IA32_PG_P | IA32_PG_RW;
    }
    return phys_to_direct(*entry & PAGING_4K_ADDRESS_MASK);
}

static uint64_t* early_virt_get_pte(void* virt) {
    uint64_t* table = m_page_table;
    for (int i = m_page_table_levels - 1; i >= 0; i--) {
        size_t shift = PAGING_INDEX_BITS * i + PAGE_SHIFT;
        size_t index = ((uintptr_t)virt >> shift) & PAGING_INDEX_MASK;

        // get the pte, if its the exact level we want return it
        uint64_t* pte = &table[index];
        if (i == 0) {
            return pte;
        }

        // get next table
        table = early_virt_get_next_level(pte);
    }

    __builtin_unreachable();
}

static void early_virt_map(void* virt, uint64_t phys, size_t num_pages, uint64_t flags) {
    for (; num_pages != 0; num_pages--, virt += PAGE_SIZE, phys += PAGE_SIZE) {
        uint64_t* pte = early_virt_get_pte(virt);
        ASSERT(*pte == 0);
        *pte = phys | IA32_PG_P | IA32_PG_A | flags;
    }
}

static void early_map_vmar(vmar_t* vmar, vm_perm_t perms) {
    ASSERT(g_executable_address.response != nullptr);
    uint64_t phys_base = g_executable_address.response->physical_base;
    void* virt_base = (void*)g_executable_address.response->virtual_base;
    uint64_t phys_addr = (vmar->base - virt_base) + phys_base;

    uint64_t flags = IA32_PG_G;
    if (perms == VM_PERM_RW) {
        flags |= IA32_PG_D | IA32_PG_RW;
    }
    if (perms != VM_PERM_RX) {
        flags |= IA32_PG_NX;
    }
    early_virt_map(vmar->base, phys_addr, vmar->page_count, flags);
}

static void early_map_kernel(void) {
    early_map_vmar(&g_kernel_limine_requests_mapping, VM_PERM_RO);
    early_map_vmar(&g_kernel_text_mapping, VM_PERM_RX);
    early_map_vmar(&g_kernel_rodata_mapping, VM_PERM_RO);
    early_map_vmar(&g_kernel_data_mapping, VM_PERM_RW);
}

static void early_map_direct_map(void) {
    for (size_t i = 0; i < m_memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = m_memmap_request.response->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE &&
            entry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            continue;
        }
        early_virt_map(phys_to_direct(entry->base), entry->base, entry->length / PAGE_SIZE,
                       IA32_PG_RW | IA32_PG_D | IA32_PG_NX);
    }
}

void early_virt_init(void) {
    // initialize the basic regions
    m_page_table_levels = init_kernel_region();
    init_direct_map();

    // map the kernel and direct map, don't link
    // them to the vmar tree yet, because we want
    // to avoid code that will use kasan
    m_page_table = early_phys_alloc_page();
    early_map_kernel();
    early_map_direct_map();
    // TODO: create kasan range
    __writecr3(direct_to_phys(m_page_table));

    // we now have kasan and can finalize the early
    // memory init

    // now with the new page table finialize the
    vmar_link(&g_kernel_region, &g_direct_map);
    vmar_link(&g_kernel_region, &g_kernel_limine_requests_mapping);
    vmar_link(&g_kernel_region, &g_kernel_text_mapping);
    vmar_link(&g_kernel_region, &g_kernel_rodata_mapping);
    vmar_link(&g_kernel_region, &g_kernel_data_mapping);
}
