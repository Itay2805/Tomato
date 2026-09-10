#include "alloc.h"

#include "entry.h"
#include "lib/assert.h"
#include "lib/string.h"
#include "lib/trace.h"
#include "limine.h"
#include "mem/mapping.h"
#include <stddef.h>
#include <stdint.h>

static uintptr_t m_early_alloc_top = 0;
static size_t m_early_alloc_size = 0;

static void early_alloc_find_next_entry() {
    struct limine_memmap_response* response = g_memmap_request.response;
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

void* early_phys_alloc_page() {
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
