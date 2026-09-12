#pragma once

#include "arch/virt.h"
#include "lib/rbtree/rbtree.h"
#include "lib/rbtree/rbtree_types.h"
#include "util/except.h"
#include <stdint.h>

/**
 * The kind of the VMAR
 */
typedef enum vmar_kind {
    /**
     * This VMAR represents a region, where child vmars can be mapped to, a fault on this
     * region means unmapped memory
     */
    VMAR_KIND_REGION,

    /**
     * A VMAR with special semantics, it is used internally by the kernel for things that should
     * never fault and have special semantics for how things are mapped
     */
    VMAR_KIND_SPECIAL,
} vmar_kind_t;

typedef struct vmar {
    /**
     * Node in the children tree of the parent,
     * allows for efficient lookup for address in the vmar
     */
    rb_node_linked_t node;

    /**
     * The base address and count of the vmar
     */
    void* base;
    size_t page_count;

    /**
     * The parent of the vmar, nullptr if this is
     * the top level vmar
     */
    struct vmar* parent;

    /**
     * Debug name
     */
    char* name;

    /**
     * The gap, in pages, between this vmar and the sibling before
     * it (or the start of the parent if it is the first one)
     */
    size_t gap;

    /**
     * The max gap of any node in the subtree rooted at this node,
     * allows for efficient first-fit allocation
     */
    size_t max_gap;

    /**
     * The kind of the region
     */
    vmar_kind_t kind;

    union {
        struct {
            /**
             * The children of the region
             */
            rb_root_linked_t children;
        } region;
    };
} vmar_t;

[[clang::always_inline]]
static inline void* vmar_end(const vmar_t* vmar) {
    return vmar->base + ((vmar->page_count * PAGE_SIZE) - 1);
}

/**
 * Memory permissions
 */
typedef enum vm_perm {
    VM_PERM_RO,
    VM_PERM_RW,
    VM_PERM_RX,
} vm_perm_t;

#define VMAR_ANY_OFFSET ((uintptr_t)-1)

/**
 * Given a filled VMAR (base and page count are set), link it
 * to the parent.
 *
 * The child must be page aligned, fully contained in the parent and
 * must not overlap any of the children already linked to the parent.
 */
err_t vmar_link(vmar_t* parent, vmar_t* child);

/**
 * Place the child in the parent and link it.
 *
 * The offset is relative to the start of the parent, if it is VMAR_ANY_OFFSET
 * the child is placed in the lowest gap that can hold it (first-fit).
 */
err_t vmar_allocate_static(vmar_t* parent, vmar_t* child, uintptr_t offset, size_t page_count);

/**
 * Dump the VMAR tree
 */
void vmar_dump(vmar_t* root);
