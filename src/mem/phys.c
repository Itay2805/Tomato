#include "phys.h"
#include "lib/assert.h"
#include "lib/hlist.h"
#include "lib/string.h"
#include "mem/accounting.h"
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
 * The level of a block that is exactly a page, anything below it is a piece
 * of a split page, and the page can't be handed out until all of its pieces
 * merge back, so the accounting treats the whole page as committed
 */
#define PHYS_BUDDY_PAGE_LEVEL (PAGE_SHIFT - PHYS_BUDDY_MIN_ORDER)
static_assert(PHYS_BUDDY_PAGE_LEVEL < PHYS_BUDDY_MAX_LEVEL);

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

/**
 * The level of a block of pages that matches the size exactly, -1
 * if there is no such block, used for the reserved allocations
 */
static int get_page_level_by_size(size_t size) {
    int level = get_level_by_size(size);
    if (level < PHYS_BUDDY_PAGE_LEVEL || size != (1ULL << (level + PHYS_BUDDY_MIN_ORDER))) {
        return -1;
    }
    return level;
}

/**
 * The amount of pages in a block of the page level or above
 */
static size_t get_page_count_by_level(int level) {
    return 1ULL << (level - PHYS_BUDDY_PAGE_LEVEL);
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

static void* phys_alloc_internal(int level, bool reserved) {
    mutex_guard(&m_phys_buddy_lock);

    // search for the freelist that has the closest level to what we want
    int block_at_level = level;
    while (block_at_level < PHYS_BUDDY_MAX_LEVEL &&
           hlist_empty(&m_phys_buddy_levels[block_at_level])) {
        block_at_level++;
    }

    if (block_at_level == PHYS_BUDDY_MAX_LEVEL) {
        // a reserved page always has a free page waiting for it,
        // if it doesn't then the accounting is broken

        // TODO: attempt to reclaim as much memory from different places on the system, and only
        //       if we still have no memory then return null

        ASSERT(!reserved || level != PHYS_BUDDY_PAGE_LEVEL,
               "memory: no free page for a reserved allocation");
        return NULL;
    }

    // commit the pages before we take them, a block of pages commits all of them, a
    // piece of a page only commits a page if it has to split one, otherwise it comes
    // from a page that was already split and is already committed
    if (level >= PHYS_BUDDY_PAGE_LEVEL) {
        size_t page_count = get_page_count_by_level(level);
        if (reserved) {
            mem_acct_commit_reserved(page_count);
        } else if (!mem_acct_commit(page_count)) {
            return NULL;
        }
    } else if (block_at_level >= PHYS_BUDDY_PAGE_LEVEL) {
        if (!mem_acct_commit(1)) {
            return NULL;
        }
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

void* phys_alloc(size_t size) {
    int level = get_level_by_size(size);
    if (level < 0) {
        ERROR("memory: too much memory requested (0x%lx bytes)", size);
        return NULL;
    }

    return phys_alloc_internal(level, false);
}

void* phys_alloc_reserved(size_t size) {
    int level = get_page_level_by_size(size);
    ASSERT(level >= 0, "memory: reserved allocation of 0x%lx bytes is not a block of pages", size);
    return phys_alloc_internal(level, true);
}

/**
 * Returns the level of the block after merging it
 */
static int phys_free_internal(void* ptr, int level, bool check_allocated) {
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

    return level;
}

static void phys_free_block(void* ptr, int level, bool reserved) {
    // zero it before freeing
    size_t block_size = 1ULL << (level + PHYS_BUDDY_MIN_ORDER);
    memset(ptr, 0, block_size);

    int merged_level = phys_free_internal(ptr, level, true);

    // the pages are back in the allocator so we can uncommit them, a piece of
    // a page only uncommits the page once all of its pieces merged back into it
    if (level >= PHYS_BUDDY_PAGE_LEVEL) {
        size_t page_count = get_page_count_by_level(level);
        if (reserved) {
            mem_acct_uncommit_to_reserved(page_count);
        } else {
            mem_acct_uncommit(page_count);
        }
    } else if (merged_level >= PHYS_BUDDY_PAGE_LEVEL) {
        mem_acct_uncommit(1);
    }
}

void phys_free(void* ptr, size_t size) {
    if (ptr == NULL) {
        return;
    }

    int level = get_level_by_size(size);
    ASSERT(level >= 0);

    phys_free_block(ptr, level, false);
}

void phys_free_reserved(void* ptr, size_t size) {
    if (ptr == NULL) {
        return;
    }

    int level = get_page_level_by_size(size);
    ASSERT(level >= 0, "memory: reserved free of 0x%lx bytes is not a block of pages", size);

    phys_free_block(ptr, level, true);
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
    // the accounting is in pages, if we allowed a piece of a page in here it could
    // later merge into a whole page that was never counted in the total
    ASSERT(((uintptr_t)start & PAGE_MASK) == 0 && ((uintptr_t)end & PAGE_MASK) == 0);
    size_t page_count = (end - start) / PAGE_SIZE;

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

    // only now that the pages are in the allocator can they be handed out
    mem_acct_add_total(page_count);
}
