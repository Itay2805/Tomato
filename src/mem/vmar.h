#pragma once

#include "arch/virt.h"
#include "lib/list.h"
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
    rb_node_t node;

    /**
     * Entry in the ordered linked list of children
     * Allows for efficient iteration over the children
     */
    list_entry_t entry;

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
     * The kind of the region
     */
    vmar_kind_t kind;

    union {
        struct {
            /**
             * The children of the region, as a tree
             */
            rb_root_t children_tree;

            /**
             * The children of the region, as a list
             */
            list_t children_list;
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

#define VMAR_ANY_OFFSET ((void*)-1)

/**
 * Given a filled VMAR (base and page count are set), link it to the parent.
 *
 * The child must be fully contained in the parent and must not overlap
 * any of the children already linked to the parent.
 *
 * Returns the base of the child on success, nullptr if the child could not
 * be placed (invalid range, outside of the parent, or overlapping a sibling).
 */
err_t vmar_link(vmar_t* parent_vmar, vmar_t* child_vmar);

/**
 * Dump the VMAR tree
 */
void vmar_dump(vmar_t* root);
