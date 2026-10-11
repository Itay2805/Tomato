#include "vmar.h"
#include "arch/virt.h"
#include "lib/assert.h"
#include "lib/container_of.h"
#include "lib/object.h"
#include "lib/rbtree/rbtree.h"
#include "lib/rbtree/rbtree_augmented.h"
#include "lib/rbtree/rbtree_types.h"
#include "lib/string.h"
#include "lib/trace.h"
#include "mem/accounting.h"
#include "mem/alloc.h"
#include "mem/virt.h"
#include "sync/rwlock.h"
#include "tomato/err.h"
#include "util/except.h"
#include <stddef.h>
#include <stdint.h>

/**
 * An RWLock protecting all VMAR modifications
 * TODO: maybe only have a per address-space lock instead?
 */
static rwlock_t m_vmar_lock = {};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get the vmar holding the given tree node
 */
#define vmar_of_rb(rb) rb_entry(rb, vmar_t, node.node)

/**
 * Get the vmar holding the given linked node, nullptr if there is none
 */
[[clang::always_inline]]
static inline vmar_t* vmar_of_linked(rb_node_linked_t* lnode) {
    return lnode != nullptr ? container_of(lnode, vmar_t, node) : nullptr;
}

/**
 * The siblings right before and after the vmar in address order
 */
[[clang::always_inline]]
static inline vmar_t* vmar_prev(vmar_t* vmar) {
    return vmar_of_linked(vmar->node.prev);
}

[[clang::always_inline]]
static inline vmar_t* vmar_next(vmar_t* vmar) {
    return vmar_of_linked(vmar->node.next);
}

/**
 * All the range math is done in page numbers rather than addresses, it makes
 * the numbers small enough that none of it can overflow, even for a region
 * that ends at the very top of the address space.
 */
[[clang::always_inline]]
static inline size_t vmar_first_page(const vmar_t* vmar) {
    return (uintptr_t)vmar->base >> PAGE_SHIFT;
}

/**
 * The page right after the last page of the vmar (exclusive end)
 */
[[clang::always_inline]]
static inline size_t vmar_end_page(const vmar_t* vmar) {
    return vmar_first_page(vmar) + vmar->page_count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gap tracking
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Each child knows the gap right before it, and the tree is augmented with
 * the largest such gap in each subtree, so a first-fit search can skip any
 * subtree that has nothing big enough.
 *
 * The gap after the last child belongs to no node and is handled
 * separately by the search.
 */
[[clang::always_inline]]
static inline size_t vmar_gap_of(vmar_t* vmar) {
    return vmar->gap;
}

RB_DECLARE_CALLBACKS_MAX(static, vmar_gap_callbacks, vmar_t, node.node, size_t, max_gap,
                         vmar_gap_of)

/**
 * Recompute the gap between the vmar and the sibling before it (or the start of
 * the parent if it is the first one) and propagate the new max up the tree.
 *
 * Must be called whenever the predecessor of a vmar changes.
 */
static void vmar_gap_update(vmar_t* vmar) {
    vmar_t* prev = vmar_prev(vmar);
    size_t gap_start = prev != nullptr ? vmar_end_page(prev) : vmar_first_page(vmar->parent);
    vmar->gap = vmar_first_page(vmar) - gap_start;
    vmar_gap_callbacks_propagate(&vmar->node.node, nullptr);
}

/**
 * The gap after the last child belongs to no node so the tree does not track
 * it, if there are no children at all the entire region is the gap.
 *
 * Returns the size of the gap and its first page.
 */
static size_t vmar_tail_gap(vmar_t* parent, size_t* out_page) {
    rb_node_t* last = rb_last(&parent->region.children.rb_root);
    size_t start = last != nullptr ? vmar_end_page(vmar_of_rb(last)) : vmar_first_page(parent);
    *out_page = start;
    return vmar_end_page(parent) - start;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Linking
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Orders the children by address, two children that overlap compare as equal
 */
static int vmar_overlap_cmp(rb_node_t* new, const rb_node_t* exist) {
    vmar_t* a = vmar_of_rb(new);
    vmar_t* b = vmar_of_rb(exist);

    if (vmar_end_page(a) <= vmar_first_page(b)) {
        return -1;
    } else if (vmar_first_page(a) >= vmar_end_page(b)) {
        return 1;
    } else {
        return 0;
    }
}

static err_t vmar_link_internal(vmar_t* parent, vmar_t* child) {
    CHECK_ARG(parent->kind == VMAR_KIND_REGION);
    CHECK_ARG(child->parent == nullptr);
    CHECK_ARG(((uintptr_t)child->base & PAGE_MASK) == 0);

    // must be fully contained within the parent
    size_t end;
    CHECK_ERROR(!__builtin_add_overflow(vmar_first_page(child), child->page_count, &end),
                OUT_OF_RANGE);
    CHECK_ERROR(vmar_first_page(child) >= vmar_first_page(parent), OUT_OF_RANGE);
    CHECK_ERROR(end <= vmar_end_page(parent), OUT_OF_RANGE);

    // insert into the tree, until we know who comes before us we contribute
    // no gap at all, which keeps the augmented data consistent while the
    // tree is being rebalanced
    child->gap = 0;
    child->max_gap = 0;
    CHECK_ERROR(rb_find_add_augmented_linked(&child->node, &parent->region.children,
                                             vmar_overlap_cmp, &vmar_gap_callbacks) == nullptr,
                OVERLAPS);
    child->parent = parent;

    // now that we are in place both our own gap and the gap of the
    // sibling after us (which is now measured against us) are known
    vmar_gap_update(child);
    vmar_t* next = vmar_next(child);
    if (next != nullptr) {
        vmar_gap_update(next);
    }

    return SUCCESS;
}

err_t vmar_link(vmar_t* parent, vmar_t* child) {
    rwlock_exclusive_guard(&m_vmar_lock);
    return vmar_link_internal(parent, child);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Allocation
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The first-fit search wants the lowest child with a big enough gap before
 * it, the max gap of each subtree tells it which subtrees to skip
 */
static bool vmar_gap_fits(const void* key, const rb_node_t* rb) {
    return vmar_of_rb(rb)->gap >= *(const size_t*)key;
}

static bool vmar_max_gap_fits(const void* key, const rb_node_t* rb) {
    return vmar_of_rb(rb)->max_gap >= *(const size_t*)key;
}

static bool vmar_find_gap(vmar_t* parent, size_t page_count, size_t* out_page) {
    rb_root_t* root = &parent->region.children.rb_root;

    // the lowest child with a big enough gap before it
    rb_node_t* rb = rb_find_first_fit(&page_count, root, vmar_gap_fits, vmar_max_gap_fits);
    if (rb != nullptr) {
        vmar_t* vmar = vmar_of_rb(rb);
        *out_page = vmar_first_page(vmar) - vmar->gap;
        return true;
    }

    // otherwise it may fit after the last child
    size_t gap_start;
    if (vmar_tail_gap(parent, &gap_start) >= page_count) {
        *out_page = gap_start;
        return true;
    }

    return false;
}

err_t vmar_allocate_static(vmar_t* parent, vmar_t* child, uintptr_t offset, size_t page_count) {
    CHECK_ARG(parent->kind == VMAR_KIND_REGION);
    CHECK_ARG(page_count != 0);

    rwlock_exclusive_guard(&m_vmar_lock);

    uintptr_t base;
    if (offset == VMAR_ANY_OFFSET) {
        // first fit, the lowest gap that can hold the child
        size_t page;
        CHECK_ERROR(vmar_find_gap(parent, page_count, &page), OUT_OF_RESOURCES);
        base = page << PAGE_SHIFT;
    } else {
        // at a fixed offset from the start of the parent, linking
        // will verify the range is actually inside of it
        CHECK_ERROR(!__builtin_add_overflow((uintptr_t)parent->base, offset, &base), OUT_OF_RANGE);
    }

    child->base = (void*)base;
    child->page_count = page_count;
    RETHROW(vmar_link_internal(parent, child));

    return SUCCESS;
}

DEFINE_ALLOC(vmar_t);

err_t vmar_allocate(vmar_t* parent, vmar_t** new_child, uintptr_t offset, size_t page_count) {
    CHECK_ARG(parent->kind == VMAR_KIND_REGION);
    CHECK_ARG(page_count != 0);

    // allocate the new entry
    vmar_t* child = ALLOC(vmar_t);
    CHECK_ERROR(child != nullptr, OUT_OF_RESOURCES);
    child->object = INIT_OBJECT(OBJECT_KIND_VMAR);
    OBJECT_GET(child); // we return the object
    child->kind = VMAR_KIND_REGION;
    child->region.children = RB_ROOT_LINKED;
    defer if (child != nullptr) vmar_free(child);

    // actually place it
    RETHROW(vmar_allocate_static(parent, child, offset, page_count));

    // move it to the caller
    *new_child = MOVE(child);

    return SUCCESS;
}

err_t vmar_map(vmar_t* parent, vmar_t** new_child, uintptr_t offset, size_t page_count) {
    CHECK_ARG(parent->kind == VMAR_KIND_REGION);
    CHECK_ARG(page_count != 0);

    // start by reserving pages for the mapping
    RETHROW(mem_acct_reserve(page_count));
    defer if (page_count != 0) mem_acct_unreserve(page_count);

    // allocate the new entry
    vmar_t* child = ALLOC(vmar_t);
    CHECK_ERROR(child != nullptr, OUT_OF_RESOURCES);
    child->object = INIT_OBJECT(OBJECT_KIND_VMAR);
    OBJECT_GET(child); // we return the object
    child->kind = VMAR_KIND_MAPPING;
    defer if (child != nullptr) vmar_free(child);

    // actually place it
    RETHROW(vmar_allocate_static(parent, child, offset, page_count));

    // now that we know where we want to be, reserve the
    // page table region, if this fails we are going to
    // properly free everything
    RETHROW(virt_reserve_page_tables(child->base, child->page_count));

    page_count = 0;
    *new_child = MOVE(child);

    return SUCCESS;
}

/**
 * Perform the actual unmap logic, assumes we are
 * under the vmar exclusive lock.
 *
 * this does NOT release the last reference
 */
static void vmar_unmap_internal(vmar_t* vmar) {
    // make sure not unmapped already
    ASSERT(!vmar->unmapped);

    // get the parent and validate it
    vmar_t* parent = vmar->parent;
    ASSERT(parent != nullptr);
    ASSERT(parent->kind == VMAR_KIND_REGION);

    if (vmar->kind == VMAR_KIND_REGION) {
        // we are going to unmap all of the children, and then put the reference, which will
        // optionally free the vmar itself (or not if there is a parent)
        rb_node_linked_t* it = rb_first_linked(&vmar->region.children);
        while (it != nullptr) {

            // get the child and the next one already
            vmar_t* child = vmar_of_linked(it);
            it = it->next;

            // unmap the entry, this will remove the link
            vmar_unmap_internal(child);

            // put the child ref, potentially freeing it
            OBJECT_PUT(child);
        }

    } else if (vmar->kind == VMAR_KIND_MAPPING) {
        // take the lock for the mapping, so no one can access
        // it at the same time as we are modifying it
        rwlock_exclusive_guard(&vmar->mapping.page_table_lock);

        virt_free(vmar->base, vmar->page_count);

    } else if (vmar->kind == VMAR_KIND_SPECIAL) {
        ASSERT(!"Can't free special mappings");

    } else {
        ASSERT(!"Unknown vmar kind!");
    }

    // remove from the tree
    vmar_t* next = vmar_next(vmar);
    rb_erase_augmented_linked(&vmar->node, &parent->region.children, &vmar_gap_callbacks);
    if (next != nullptr) {
        vmar_gap_update(next);
    }

    // and finally clear everything to make sure the objects are
    // known to be unmapped
    vmar->parent = nullptr;
    vmar->unmapped = true;
}

void vmar_unmap(vmar_t* vmar) {
    // we are about the modify the vmar tree, lock
    // the vmar exclusively
    rwlock_exclusive_guard(&m_vmar_lock);

    // ignore if already unmapped
    if (vmar->unmapped) {
        return;
    }

    // unmap it and remove the reference, assuming
    // the caller holds a reference
    vmar_unmap_internal(vmar);
    OBJECT_PUT(vmar);
}

void vmar_free(vmar_t* vmar) {
    // must be mapped before reaching this point
    ASSERT(vmar->unmapped);
    FREE(vmar_t, vmar);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Dumping
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The glyphs used to draw the tree, a child is drawn after a branch
 * and its descendants are indented by the matching indent so the guide
 * line continues down only while there are more siblings to come.
 */
#define DUMP_BRANCH      "├── "
#define DUMP_BRANCH_LAST "└── "
#define DUMP_INDENT      "│   "
#define DUMP_INDENT_LAST "    "

/**
 * The deepest nesting we draw guide lines for, anything past that
 * is printed at the same indentation as its parent instead of
 * overflowing the prefix buffer.
 */
#define DUMP_MAX_DEPTH   32
#define DUMP_PREFIX_SIZE (DUMP_MAX_DEPTH * (sizeof(DUMP_INDENT) - 1) + 1)

typedef struct dump_glyphs {
    const char* branch;
    const char* indent;
    size_t indent_len;
} dump_glyphs_t;

static const dump_glyphs_t m_dump_glyphs = {
    .branch = DUMP_BRANCH,
    .indent = DUMP_INDENT,
    .indent_len = sizeof(DUMP_INDENT) - 1,
};

static const dump_glyphs_t m_dump_glyphs_last = {
    .branch = DUMP_BRANCH_LAST,
    .indent = DUMP_INDENT_LAST,
    .indent_len = sizeof(DUMP_INDENT_LAST) - 1,
};

static const char* vmar_kind_str(vmar_kind_t kind) {
    switch (kind) {
        case VMAR_KIND_REGION:
            return "region";
        case VMAR_KIND_SPECIAL:
            return "special";
        case VMAR_KIND_MAPPING:
            return "mapping";
        default:
            return "unknown";
    }
}

static void vmar_dump_line(vmar_t* vmar, const char* prefix, const char* branch) {
    const char* name = vmar->name;
    if (name == nullptr) {
        name = "<anonymous>";
    }

    TRACE("%s%s%016lx-%016lx: %s [%s]", prefix, branch, (uintptr_t)vmar->base,
          (uintptr_t)vmar_end(vmar), name, vmar_kind_str(vmar->kind));
}

static void vmar_dump_gap(size_t first_page, size_t page_count, const char* prefix,
                          const char* branch) {
    uintptr_t start = first_page << PAGE_SHIFT;
    TRACE("%s%s%016lx-%016lx: <gap> [%lu pages]", prefix, branch, start,
          start + ((page_count << PAGE_SHIFT) - 1), page_count);
}

/**
 * Print all the children of the given vmar, the prefix is the already
 * drawn guide lines of the ancestors and is extended in place while
 * descending, so it must have room for DUMP_PREFIX_SIZE bytes.
 */
static void vmar_dump_children(vmar_t* vmar, char* prefix, size_t prefix_len) {
    // only regions have children
    if (vmar->kind != VMAR_KIND_REGION) {
        return;
    }

    // the free space after the last child comes out as the last entry, so
    // the last child only closes the tree if there is none
    size_t tail_page;
    size_t tail_gap = vmar_tail_gap(vmar, &tail_page);

    // walk the siblings from the leftmost, the links
    // are kept in address order
    for (rb_node_linked_t* it = rb_first_linked(&vmar->region.children); it != nullptr;
         it = it->next) {
        vmar_t* child = vmar_of_linked(it);

        // the free space before the child
        if (child->gap != 0) {
            vmar_dump_gap(vmar_first_page(child) - child->gap, child->gap, prefix, DUMP_BRANCH);
        }

        bool last = it->next == nullptr && tail_gap == 0;
        const dump_glyphs_t* glyphs = last ? &m_dump_glyphs_last : &m_dump_glyphs;
        vmar_dump_line(child, prefix, glyphs->branch);

        // extend the prefix for the grandchildren, if there is
        // no more room just print them flat under the child
        size_t child_len = prefix_len + glyphs->indent_len;
        if (child_len < DUMP_PREFIX_SIZE) {
            memcpy(prefix + prefix_len, glyphs->indent, glyphs->indent_len + 1);
            vmar_dump_children(child, prefix, child_len);
            prefix[prefix_len] = '\0';
        } else {
            vmar_dump_children(child, prefix, prefix_len);
        }
    }

    if (tail_gap != 0) {
        vmar_dump_gap(tail_page, tail_gap, prefix, DUMP_BRANCH_LAST);
    }
}

void vmar_dump(vmar_t* root) {
    rwlock_shared_guard(&m_vmar_lock);

    char prefix[DUMP_PREFIX_SIZE] = "";
    vmar_dump_line(root, prefix, "");
    vmar_dump_children(root, prefix, 0);
}
