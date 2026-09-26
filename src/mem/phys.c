#include "phys.h"
#include "lib/assert.h"
#include "lib/hlist.h"
#include "lib/string.h"
#include "mem/mapping.h"
#include "sync/mutex.h"
#include "util/except.h"

typedef struct buddy_free_page {
    hlist_node_t node;
    uint8_t level;
} buddy_free_page_t;

// the header is stored inside of the free block, so even the smallest block must fit it
static_assert(sizeof(buddy_free_page_t) <= PHYS_BUDDY_MIN_SIZE);

// the level is stored in a byte in the header
static_assert(PHYS_BUDDY_MAX_LEVEL <= UINT8_MAX + 1);

/**
 * All the different levels of the buddy allocator
 */
static hlist_t m_phys_buddy_levels[PHYS_BUDDY_MAX_LEVEL] = {};

/**
 * Protects the buddy allocator lists
 */
static mutex_t m_phys_buddy_lock = {};

static int get_level_by_size(size_t size) {
    if (size > PHYS_BUDDY_MAX_SIZE) {
        return -1;
    }

    if (size < PHYS_BUDDY_MIN_SIZE) {
        size = PHYS_BUDDY_MIN_SIZE;
    }

    // the order of the size rounded up to the next power of two
    int order = 64 - __builtin_clzll(size - 1);
    return order - PHYS_BUDDY_MIN_ORDER;
}

static bool buddy_is_block_free(void* ptr) {
    uintptr_t addr = direct_to_phys(ptr) >> PHYS_BUDDY_MIN_ORDER;
    size_t index = addr / 8;
    size_t shift = addr % 8;
    uint8_t* const buddy_bitmap = g_buddy_bitmap_mapping.base;
    return (buddy_bitmap[index] >> shift) & 1;
}

static void buddy_set_block_allocated(void* ptr) {
    uintptr_t addr = direct_to_phys(ptr) >> PHYS_BUDDY_MIN_ORDER;
    size_t index = addr / 8;
    size_t shift = addr % 8;
    uint8_t* const buddy_bitmap = g_buddy_bitmap_mapping.base;
    buddy_bitmap[index] &= ~(1U << shift);
}

static void buddy_set_block_free(void* ptr) {
    uintptr_t addr = direct_to_phys(ptr) >> PHYS_BUDDY_MIN_ORDER;
    size_t index = addr / 8;
    size_t shift = addr % 8;
    uint8_t* const buddy_bitmap = g_buddy_bitmap_mapping.base;
    buddy_bitmap[index] |= 1U << shift;
}

void* phys_alloc(size_t size) {
    int level = get_level_by_size(size);
    if (level < 0) {
        ERROR("memory: too much memory requested (0x%lx bytes)", size);
        return NULL;
    }

    mutex_guard(&m_phys_buddy_lock);

    // search for the freelist that has the closest level to what we want
    int block_at_level = level;
    while (block_at_level < PHYS_BUDDY_MAX_LEVEL &&
           hlist_empty(&m_phys_buddy_levels[block_at_level])) {
        block_at_level++;
    }

    if (block_at_level == PHYS_BUDDY_MAX_LEVEL) {
        // TODO: attempt to reclaim as much memory from different places on the system, and only
        //       if we still have no memory then return null
        return NULL;
    }

    hlist_t* freelist = &m_phys_buddy_levels[block_at_level];
    buddy_free_page_t* page = hlist_entry(freelist->first, buddy_free_page_t, node);
    ASSERT(page->level == block_at_level);

    // we want to zero the free page header
    // so the entire block ends up being zeroed
    // (we already zero on free)
    hlist_del(&page->node);
    *page = (buddy_free_page_t){};

    // split the blocks until we reach
    // the requested level
    void* block = page;
    while (block_at_level > level) {
        // we need the size to split it
        size_t block_size = 1ULL << (block_at_level + PHYS_BUDDY_MIN_ORDER);
        block_at_level--;

        // add the upper part of the page to the bottom freelist
        buddy_free_page_t* upper = block + block_size / 2;
        upper->level = block_at_level;
        hlist_add(&m_phys_buddy_levels[block_at_level], &upper->node);
        buddy_set_block_free(upper);
    }

    // mark our block as allocated
    buddy_set_block_allocated(block);

    return block;
}

static void phys_free_internal(void* ptr, int level, bool check_allocated) {
    // sanity check
    ASSERT(((uintptr_t)ptr % (1UL << (level + PHYS_BUDDY_MIN_ORDER))) == 0);

    mutex_guard(&m_phys_buddy_lock);

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

        // remove it from the freelist, and zero its header since it
        // ends up in the middle of the merged block
        hlist_del(&neighbor->node);
        *neighbor = (buddy_free_page_t){};

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
    hlist_add(&m_phys_buddy_levels[level], &block->node);
    buddy_set_block_free(block);
}

void phys_free(void* ptr, size_t size) {
    if (ptr == NULL) {
        return;
    }

    int level = get_level_by_size(size);
    ASSERT(level >= 0);

    // zero it before freeing
    size_t block_size = 1ULL << (level + PHYS_BUDDY_MIN_ORDER);
    memset(ptr, 0, block_size);

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
    // zero the entire region
    memset(start, 0, end - start);

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
