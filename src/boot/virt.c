#include "virt.h"
#include "arch/virt.h"
#include "boot/alloc.h"
#include "entry.h"
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
    ASSERT(g_memmap_request.response != NULL);

    // find the top most address that we need to store in kernel
    uintptr_t top_address = 0;
    for (size_t i = 0; i < g_memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = g_memmap_request.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            top_address = entry->base + entry->length;
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
                           vm_perm_t perms) {
    for (; num_pages != 0; num_pages--, virt += PAGE_SIZE, phys += PAGE_SIZE) {
        uint64_t* pte = early_virt_get_pte(table, levels, virt);
        ASSERT(*pte == 0);

        uint64_t entry = phys | IA32_PG_P | IA32_PG_A;

        if (perms != VM_PERM_RX)
            entry |= IA32_PG_NX;

        if (perms == VM_PERM_RW)
            entry |= IA32_PG_RW | IA32_PG_D;

        *pte = entry;
    }
}

static const char* const g_memmap_type_strs[] = {
    [LIMINE_MEMMAP_USABLE] = "Usable",
    [LIMINE_MEMMAP_RESERVED] = "Reserved",
    [LIMINE_MEMMAP_ACPI_RECLAIMABLE] = "ACPI Reclaimable",
    [LIMINE_MEMMAP_ACPI_NVS] = "ACPI NVS",
    [LIMINE_MEMMAP_BAD_MEMORY] = "Bad Memory",
    [LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE] = "Bootloader Reclaimable",
    [LIMINE_MEMMAP_EXECUTABLE_AND_MODULES] = "Executable and Modules",
    [LIMINE_MEMMAP_FRAMEBUFFER] = "Framebuffer",
    [LIMINE_MEMMAP_RESERVED_MAPPED] = "Reserved (Mapped)",
};

static void create_direct_map(void* table, int levels) {
    TRACE("Memory map:");
    for (size_t i = 0; i < g_memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = g_memmap_request.response->entries[i];

        TRACE("\t%016lx-%016lx: %s", entry->base, entry->base + (entry->length - 1),
              g_memmap_type_strs[entry->type]);

        if (entry->type != LIMINE_MEMMAP_USABLE &&
            entry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            continue;
        }

        early_virt_map(table, levels, phys_to_direct(entry->base), entry->base,
                       entry->length / PAGE_SIZE, VM_PERM_RW);
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

    early_virt_map(table, levels, vmar->base, phys_addr, vmar->page_count, perms);
}

static void create_kernel_mapping(void* table, int levels) {
    TRACE("Mapping kernel");
    early_map_vmar(table, levels, &g_kernel_text_mapping, VM_PERM_RX);
    early_map_vmar(table, levels, &g_kernel_rodata_mapping, VM_PERM_RO);
    early_map_vmar(table, levels, &g_kernel_data_mapping, VM_PERM_RW);
}

void init_early_virt(void) {
    // start by initializing some of the basic structs
    int levels = init_kernel_region();
    init_direct_map();

    // continue by creating the page table and everything
    // we need to finish early booting
    void* table = early_phys_alloc_page();
    create_direct_map(table, levels);
    create_kernel_mapping(table, levels);

    // switch to new page table
    __writecr3(direct_to_phys(table));

    vmar_dump(&g_kernel_region);
}
