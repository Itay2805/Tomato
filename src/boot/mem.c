#include "mem.h"
#include "arch/intrin.h"
#include "arch/virt.h"
#include "lib/assert.h"
#include "lib/string.h"
#include "lib/trace.h"
#include "limine.h"
#include "mem/mapping.h"
#include "mem/phys.h"
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

uintptr_t g_early_alloc_top = 0;
static size_t m_early_alloc_size = 0;

static void early_alloc_find_next_entry() {
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
        if (entry->base < g_early_alloc_top) {
            continue;
        }

        g_early_alloc_top = entry->base;
        m_early_alloc_size = entry->length;
        ASSERT((g_early_alloc_top % PAGE_SIZE) == 0);
        ASSERT((m_early_alloc_size % PAGE_SIZE) == 0);
        return;
    }

    ASSERT(!"Ran out of memory during boot");
}

void* early_phys_alloc_page() {
    if (m_early_alloc_size == 0) {
        early_alloc_find_next_entry();
    }

    uintptr_t phys = g_early_alloc_top;
    g_early_alloc_top += PAGE_SIZE;
    m_early_alloc_size -= PAGE_SIZE;

    void* ptr = phys_to_direct(phys);
    memset(ptr, 0, PAGE_SIZE);
    return ptr;
}

//
// Setup early regions
//

static int init_kernel_region() {
    // figure how many address bits we have (for both kernel and user)
    switch (g_paging_mode_request.response->mode) {
#ifdef __x86_64__
        case LIMINE_PAGING_MODE_X86_64_4LVL:
            g_kernel_region.base = (void*)0xFFFF800000000000;
            g_kernel_region.page_count = SIZE_128TB / PAGE_SIZE;
            g_user_region.base = 0;
            g_user_region.page_count = SIZE_128TB / PAGE_SIZE;
            return 4;
#else
    #error Unknown arch
#endif
        default:
            ASSERT(0, "Unknown paging mode %lu", g_paging_mode_request.response->mode);
    }
}

static void init_direct_map() {
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

    vmar_link(&g_kernel_region, &g_direct_map);
}

//
// Generic early utilities for page table manipulation
//

static uint64_t* early_virt_get_next_level(uint64_t* entry) {
    if ((*entry & IA32_PG_P) == 0) {
        void* phys = early_phys_alloc_page();
        *entry = direct_to_phys(phys) | IA32_PG_P | IA32_PG_RW;
    }
    return phys_to_direct(*entry & PAGING_4K_ADDRESS_MASK);
}

static uint64_t* early_virt_get_pte(uint64_t* table, int levels, void* virt) {
    for (int i = levels - 1; i >= 0; i--) {
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

static void early_virt_map(uint64_t* table, int levels, void* virt, uint64_t phys, size_t num_pages,
                           uint64_t flags) {
    for (; num_pages != 0; num_pages--, virt += PAGE_SIZE, phys += PAGE_SIZE) {
        uint64_t* pte = early_virt_get_pte(table, levels, virt);
        ASSERT(*pte == 0);
        *pte = phys | IA32_PG_P | IA32_PG_A | flags;
    }
}

static void early_map_vmar(void* table, int levels, vmar_t* vmar, vm_perm_t perms) {
    ASSERT(g_executable_address.response != nullptr);
    uint64_t phys_base = g_executable_address.response->physical_base;
    void* virt_base = (void*)g_executable_address.response->virtual_base;
    uint64_t phys_addr = (vmar->base - virt_base) + phys_base;

    char w = perms == VM_PERM_RW ? 'w' : '-';
    char x = perms == VM_PERM_RX ? 'x' : '-';
    TRACE("\t%p-%p: %lu pages [r%c%c]", vmar->base, vmar->base + (vmar->page_count * PAGE_SIZE) - 1,
          vmar->page_count, w, x);

    vmar_link(&g_kernel_region, vmar);

    uint64_t flags = IA32_PG_G;
    if (perms == VM_PERM_RW) {
        flags |= IA32_PG_D | IA32_PG_RW;
    }
    if (perms != VM_PERM_RX) {
        flags |= IA32_PG_NX;
    }
    early_virt_map(table, levels, vmar->base, phys_addr, vmar->page_count, flags);
}

static void early_map_kernel(void* table, int levels) {
    TRACE("Mapping kernel");
    early_map_vmar(table, levels, &g_kernel_text_mapping, VM_PERM_RX);
    early_map_vmar(table, levels, &g_kernel_rodata_mapping, VM_PERM_RO);
    early_map_vmar(table, levels, &g_kernel_data_mapping, VM_PERM_RW);
}

static void early_map_direct_map(void* table, int levels) {
    for (size_t i = 0; i < m_memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = m_memmap_request.response->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE &&
            entry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            continue;
        }
        early_virt_map(table, levels, phys_to_direct(entry->base), entry->base,
                       entry->length / PAGE_SIZE, IA32_PG_RW | IA32_PG_D | IA32_PG_NX);
    }
}

static void early_map_buddy_bitmap(void* table, int levels) {
    size_t bitmap_page_count = DIV_ROUND_UP(DIV_ROUND_UP(g_direct_map.page_count, 8), PAGE_SIZE);
    ASSERT_SUCCESS(vmar_allocate_static(&g_kernel_region, &g_buddy_bitmap_mapping, VMAR_ANY_OFFSET,
                                        bitmap_page_count));

    struct limine_memmap_response* response = m_memmap_request.response;
    for (size_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE &&
            entry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            continue;
        }

        // calculate the bitmap range that we need to allocate
        size_t bitmap_start = ALIGN_DOWN((entry->base / PAGE_SIZE) / 8, PAGE_SIZE);
        size_t bitmap_size = ALIGN_UP(DIV_ROUND_UP(entry->length / PAGE_SIZE, 8), PAGE_SIZE);

        // map the entire bitmap right now
        void* bitmap_ptr = g_buddy_bitmap_mapping.base + bitmap_start;
        void* bitmap_end = g_buddy_bitmap_mapping.base + bitmap_start + bitmap_size;
        for (; bitmap_ptr < bitmap_end; bitmap_ptr += PAGE_SIZE) {
            uint64_t* pte = early_virt_get_pte(table, levels, bitmap_ptr);
            if (*pte & IA32_PG_P) {
                continue;
            }

            // allocate and map the page, we mark it as RW, global (it never gets unmapped)
            // and as both dirty and accessed because we don't care for that information
            uint64_t entry = direct_to_phys(early_phys_alloc_page());
            entry |= IA32_PG_P | IA32_PG_RW | IA32_PG_NX;
            entry |= IA32_PG_G;
            entry |= IA32_PG_D | IA32_PG_A;
            *pte = entry;
        }
    }
}

static void early_phys_add_memory() {
    struct limine_memmap_response* response = m_memmap_request.response;
    for (size_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uintptr_t base = entry->base;
        uintptr_t end = base + entry->length;

        if (end < g_early_alloc_top) {
            continue;
        }

        if (base < g_early_alloc_top) {
            base = g_early_alloc_top;
        }

        phys_add_memory(phys_to_direct(base), phys_to_direct(end));
    }
}

void init_early_mem(void) {
    // start by initializing some of the basic structs
    int levels = init_kernel_region();
    init_direct_map();

    // continue by creating the page table and everything
    // we need to finish early booting
    void* table = early_phys_alloc_page();
    early_map_kernel(table, levels);
    early_map_direct_map(table, levels);
    early_map_buddy_bitmap(table, levels);

    // switch to new page table
    __writecr3(direct_to_phys(table));

    // finish up by setting the allocator
    early_phys_add_memory();

    vmar_dump(&g_kernel_region);
}
