#include "virt.h"

#include "arch/virt.h"
#include "debug/debugcon.h"
#include "debug/trace.h"
#include "lib/assert.h"
#include "lib/string.h"
#include "limine.h"
#include "mem/accounting.h"
#include "mem/mapping.h"
#include "mem/phys.h"
#include "mem/pte.h"
#include "mem/virt.h"
#include "mem/vmar.h"
#include "util/defs.h"

#include <stdint.h>

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
static size_t m_early_alloc_page_count = 0;

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
    m_early_alloc_page_count++;

    void* ptr = phys_to_direct(phys);
    memset(ptr, 0, PAGE_SIZE);
    return ptr;
}

//
// Setup early regions
//

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

static uint64_t* early_virt_get_next_level(uint64_t* entry) {
    if (!pte_is_present(*entry)) {
        void* phys = early_phys_alloc_page();
        *entry = pte_table(direct_to_phys(phys), false);
    }
    return phys_to_direct(pte_address(*entry));
}

static uint64_t* early_virt_get_pte(void* virt) {
    uint64_t* table = g_page_table;
    for (int i = g_page_table_levels - 1; i >= 0; i--) {
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

void early_virt_map(void* virt, uint64_t phys, size_t num_pages, pte_flags_t flags) {
    for (; num_pages != 0; num_pages--, virt += PAGE_SIZE, phys += PAGE_SIZE) {
        uint64_t* pte = early_virt_get_pte(virt);
        ASSERT(*pte == 0);
        *pte = pte_page(phys, flags);
    }
}

//
// Early access to the bootloader's direct map
//

void* early_phys_to_direct(uint64_t phys) {
    // we use the same offset as the bootloader, but this is used before we set up our direct map
    ASSERT(g_hhdm_request.response != nullptr);
    return (void*)(g_hhdm_request.response->offset + phys);
}

static void early_map_vmar(vmar_t* vmar, vm_perm_t perms) {
    ASSERT(g_executable_address.response != nullptr);
    uint64_t phys_base = g_executable_address.response->physical_base;
    void* virt_base = (void*)g_executable_address.response->virtual_base;
    uint64_t phys_addr = (vmar->base - virt_base) + phys_base;

    pte_flags_t flags = PTE_GLOBAL;
    if (perms == VM_PERM_RW) {
        flags |= PTE_WRITE;
    }
    if (perms == VM_PERM_RX) {
        flags |= PTE_EXEC;
    }
    early_virt_map(vmar->base, phys_addr, vmar->page_count, flags);
}

static void early_map_kernel(void) {
    early_map_vmar(&g_kernel_limine_requests_mapping, VM_PERM_RO);
    early_map_vmar(&g_kernel_init_text_mapping, VM_PERM_RX);
    early_map_vmar(&g_kernel_init_rodata_mapping, VM_PERM_RO);
    early_map_vmar(&g_kernel_init_data_mapping, VM_PERM_RW);
    early_map_vmar(&g_kernel_text_mapping, VM_PERM_RX);
    early_map_vmar(&g_kernel_rodata_mapping, VM_PERM_RO);
    early_map_vmar(&g_kernel_late_rodata_mapping, VM_PERM_RW);
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
                       PTE_WRITE);
    }
}

static void early_create_buddy_bitmap(void) {
    // the bitmap has a bit per min sized block, and the buddy of a block is always
    // within the same max sized block, so cover up to the end of the last max sized block
    uintptr_t top = ALIGN_UP(g_direct_map.page_count * PAGE_SIZE, PHYS_BUDDY_MAX_SIZE);
    size_t bitmap_page_count =
        DIV_ROUND_UP(DIV_ROUND_UP(top >> PHYS_BUDDY_MIN_ORDER, 8), PAGE_SIZE);
    ASSERT_SUCCESS(vmar_allocate_static(&g_kernel_region, &g_buddy_bitmap_mapping, VMAR_ANY_OFFSET,
                                        bitmap_page_count));

    struct limine_memmap_response* response = m_memmap_request.response;
    for (size_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE &&
            entry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            continue;
        }

        // calculate the bitmap range that we need to allocate, we cover entire max sized
        // blocks so merging with a buddy outside the entry never touches an unmapped page
        uintptr_t start = ALIGN_DOWN(entry->base, PHYS_BUDDY_MAX_SIZE);
        uintptr_t end = ALIGN_UP(entry->base + entry->length, PHYS_BUDDY_MAX_SIZE);
        size_t bitmap_start = ALIGN_DOWN((start >> PHYS_BUDDY_MIN_ORDER) / 8, PAGE_SIZE);
        size_t bitmap_end = ALIGN_UP(DIV_ROUND_UP(end >> PHYS_BUDDY_MIN_ORDER, 8), PAGE_SIZE);

        // map the entire bitmap right now
        void* bitmap_ptr = g_buddy_bitmap_mapping.base + bitmap_start;
        void* bitmap_end_ptr = g_buddy_bitmap_mapping.base + bitmap_end;
        for (; bitmap_ptr < bitmap_end_ptr; bitmap_ptr += PAGE_SIZE) {
            uint64_t* pte = early_virt_get_pte(bitmap_ptr);
            if (pte_is_present(*pte)) {
                continue;
            }

            // allocate and map the page, we mark it as RW and global (it never gets unmapped)
            *pte = pte_page(direct_to_phys(early_phys_alloc_page()), PTE_WRITE | PTE_GLOBAL);
        }
    }

    // we are about to fill the bitmap
    pte_barrier();
}

static void early_phys_add_memory(void) {
    struct limine_memmap_response* response = m_memmap_request.response;
    for (size_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uintptr_t base = entry->base;
        uintptr_t end = base + entry->length;

        if (end < m_early_alloc_top) {
            continue;
        }

        if (base < m_early_alloc_top) {
            base = m_early_alloc_top;
        }

        phys_add_memory(phys_to_direct(base), phys_to_direct(end));
    }

    // the rest of the memory now belongs to the physical allocator, so the
    // early allocations are final and can be accounted as committed
    mem_acct_add_committed(m_early_alloc_page_count);
}

void early_virt_init(void) {
    // initialize the basic regions and the root tables
    init_direct_map();
    early_virt_init_layout();

    // map the kernel and direct map, don't link
    // them to the vmar tree yet, because we want
    // to avoid code that will use kasan
    early_map_kernel();
    early_map_direct_map();
    // TODO: create kasan range
    early_virt_switch_table();

    // we now have kasan and can finalize the early
    // memory init

    // now with the new page table finialize the
    vmar_link(&g_kernel_region, &g_direct_map);
    vmar_link(&g_kernel_region, &g_kernel_limine_requests_mapping);
    vmar_link(&g_kernel_region, &g_kernel_init_text_mapping);
    vmar_link(&g_kernel_region, &g_kernel_init_rodata_mapping);
    vmar_link(&g_kernel_region, &g_kernel_init_data_mapping);
    vmar_link(&g_kernel_region, &g_kernel_text_mapping);
    vmar_link(&g_kernel_region, &g_kernel_rodata_mapping);
    vmar_link(&g_kernel_region, &g_kernel_late_rodata_mapping);
    vmar_link(&g_kernel_region, &g_kernel_data_mapping);

    // the debug console can be used once it is mapped
    debugcon_early_map();

    // now initialize the buddy allocator bitmap
    // and add all physical memory to it
    // yes this is technically not part of virt init but its close enough
    early_create_buddy_bitmap();
    early_phys_add_memory();
}
