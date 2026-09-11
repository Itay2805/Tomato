#include "phys.h"
#include "lib/assert.h"
#include "lib/list.h"
#include "mem/mapping.h"
#include "sync/mutex.h"
#include "util/except.h"

typedef struct buddy_free_page {
    list_entry_t entry;
    uint8_t level;
} buddy_free_page_t;

typedef struct buddy_level {
    list_t freelist;
} buddy_level_t;

static buddy_level_t m_phys_buddy_levels[PHYS_BUDDY_MAX_LEVEL] = {
    [0] = { .freelist = LIST_INIT(m_phys_buddy_levels[0].freelist) },
    [1] = { .freelist = LIST_INIT(m_phys_buddy_levels[1].freelist) },
    [2] = { .freelist = LIST_INIT(m_phys_buddy_levels[2].freelist) },
    [3] = { .freelist = LIST_INIT(m_phys_buddy_levels[3].freelist) },
    [4] = { .freelist = LIST_INIT(m_phys_buddy_levels[4].freelist) },
    [5] = { .freelist = LIST_INIT(m_phys_buddy_levels[5].freelist) },
    [6] = { .freelist = LIST_INIT(m_phys_buddy_levels[6].freelist) },
    [7] = { .freelist = LIST_INIT(m_phys_buddy_levels[7].freelist) },
    [8] = { .freelist = LIST_INIT(m_phys_buddy_levels[8].freelist) },
    [9] = { .freelist = LIST_INIT(m_phys_buddy_levels[9].freelist) },
};

static mutex_t m_phys_buddy_lock = {};

static int get_level_by_size(size_t size) {
    if (size > PHYS_BUDDY_MAX_SIZE) {
        return -1;
    }

    if (size < PHYS_BUDDY_MIN_SIZE) {
        size = PHYS_BUDDY_MIN_SIZE;
    }

    size = 1 << (32 - __builtin_clz(size - 1));
    int level = 32 - __builtin_clz(size) - 1;
    return level - PHYS_BUDDY_MIN_ORDER;
}

static bool buddy_is_block_free(void* ptr) {
    uintptr_t addr = direct_to_phys(ptr);
    size_t index = (addr / PAGE_SIZE) / 8;
    size_t shift = (addr / PAGE_SIZE) % 8;
    uint8_t* const buddy_bitmap = g_buddy_bitmap_mapping.base;
    return (buddy_bitmap[index] >> shift) & 1;
}

static void buddy_set_block_allocated(void* ptr) {
    uintptr_t addr = direct_to_phys(ptr);
    size_t index = (addr / PAGE_SIZE) / 8;
    size_t shift = (addr / PAGE_SIZE) % 8;
    uint8_t* const buddy_bitmap = g_buddy_bitmap_mapping.base;
    buddy_bitmap[index] &= ~(1U << shift);
}

static void buddy_set_block_free(void* ptr) {
    uintptr_t addr = direct_to_phys(ptr);
    size_t index = (addr / PAGE_SIZE) / 8;
    size_t shift = (addr / PAGE_SIZE) % 8;
    uint8_t* const buddy_bitmap = g_buddy_bitmap_mapping.base;
    buddy_bitmap[index] |= 1U << shift;
}

void* phys_alloc(size_t size) {
    int level = get_level_by_size(size);
    if (level < 0) {
        ERROR("memory: too much memory requested (0x%lx bytes)", size);
        return NULL;
    }

    mutex_lock(&m_phys_buddy_lock);
    defer mutex_unlock(&m_phys_buddy_lock);

    // search for a free page in the freelists that has the closest level to what we want
    int block_at_level = 0;
    void* block = NULL;
    for (block_at_level = level; block_at_level < PHYS_BUDDY_MAX_LEVEL; block_at_level++) {
        list_t* freelist = &m_phys_buddy_levels[block_at_level].freelist;
        if (!list_empty(freelist)) {
            buddy_free_page_t* page = list_first_entry(freelist, buddy_free_page_t, entry);
            ASSERT(page->level == block_at_level);
            list_del(&page->entry);
            block = page;
            break;
        }
    }

    if (block != NULL) {
        // split the blocks until we reach
        // the requested level
        while (block_at_level > level) {
            // we need the size to split it
            size_t block_size = 1ULL << (block_at_level + PHYS_BUDDY_MIN_ORDER);
            block_at_level--;

            // add the upper part of the page to the bottom freelist
            buddy_free_page_t* upper = block + block_size / 2;
            upper->level = block_at_level;
            list_add(&m_phys_buddy_levels[block_at_level].freelist, &upper->entry);
            buddy_set_block_free(upper);
        }

        // mark our block as allocated
        buddy_set_block_allocated(block);
    }

    return block;
}

static void phys_free_internal(void* ptr, int level, bool check_allocated) {
    // sanity check
    ASSERT(((uintptr_t)ptr % (1UL << (level + PHYS_BUDDY_MIN_ORDER))) == 0);

    mutex_lock(&m_phys_buddy_lock);
    defer mutex_unlock(&m_phys_buddy_lock);

    // mark the block as free right away
    if (check_allocated) {
        ASSERT(!buddy_is_block_free(ptr));
        buddy_set_block_free(ptr);
    }

    // go up the levels and search for other free blocks
    // that we can merge with
    while (level < (PHYS_BUDDY_MAX_LEVEL - 1)) {
        size_t block_size = 1UL << (level + PHYS_BUDDY_MIN_ORDER);

        buddy_free_page_t* neighbor = (void*)((uintptr_t)ptr ^ block_size);

        // we can only merge with a free block
        if (!buddy_is_block_free(neighbor)) {
            break;
        }

        // we can only merge with a block that is the
        // same level as us
        if (neighbor->level != level) {
            break;
        }

        // remove it from the freelist
        list_del(&neighbor->entry);

        // if the neighbor is from the bottom
        // then merge with it from the bottom
        if (ptr > (void*)neighbor) {
            ptr = neighbor;
        }

        // next level please
        level++;
    }

    // we merged it as much as we can, add to the freelist
    buddy_free_page_t* block = ptr;
    block->level = level;
    list_add(&m_phys_buddy_levels[level].freelist, &block->entry);
    buddy_set_block_free(block);
}

void phys_free(void* ptr, size_t size) {
    if (ptr == NULL) {
        return;
    }

    int level = get_level_by_size(size);
    ASSERT(level >= 0);

    phys_free_internal(ptr, level, true);
}

//----------------------------------------------------------------------------------------------------------------------
// Buddy initialization
//----------------------------------------------------------------------------------------------------------------------

static int get_best_level_for_block(void* start, void* end) {
    uintptr_t addr = (uintptr_t)start;
    uintptr_t addr_end = (uintptr_t)end;

    for (int i = PHYS_BUDDY_MAX_LEVEL - 1; i >= 0; i--) {
        // check we have enough space for this level
        size_t size = 1ULL << (i + PHYS_BUDDY_MIN_ORDER);
        if (addr + size > addr_end) {
            continue;
        }

        // check the alignment matches
        size_t alignment = size - 1;
        if ((addr & alignment) == 0) {
            return i;
        }
    }

    return -1;
}

void phys_add_memory(void* start, void* end) {
    while (start < end) {
        // get the best level that fits the block
        int level = get_best_level_for_block(start, end);
        ASSERT(level >= 0);

        // free it, the logic should just work
        phys_free_internal(start, level, false);

        // next block
        size_t block_size = 1ULL << (level + PHYS_BUDDY_MIN_ORDER);
        start += block_size;
    }
}
