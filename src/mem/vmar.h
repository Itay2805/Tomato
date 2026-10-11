#pragma once

#include "arch/virt.h"
#include "lib/object.h"
#include "lib/rbtree/rbtree.h"
#include "lib/rbtree/rbtree_types.h"
#include "sync/mutex.h"
#include "sync/rwlock.h"
#include "util/except.h"
#include <stdint.h>

/**
 * The kind of the VMAR
 */
typedef enum vmar_kind : uint8_t {
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

    /**
     * A VMAR that maps to memory, this is essentially a leaf that can have no children
     */
    VMAR_KIND_MAPPING,
} vmar_kind_t;

typedef struct vmar {
    /**
     * The object headers
     */
    object_t object;

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

    /**
     * Was this VMAR unmapped?
     */
    bool unmapped;

    union {
        struct {
            /**
             * The children of the region
             */
            rb_root_linked_t children;
        } region;

        struct {
            /**
             * A lock that protects the page table
             * range of the mapping
             */
            mutex_t page_table_lock;
        } mapping;
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
 *
 * This function will panic if we fail to place it inside of the tree
 */
INIT_CODE void vmar_link(vmar_t* parent, vmar_t* child);

/**
 * Place the child in the parent and link it.
 *
 * see `vmar_allocate` for more details
 *
 * This does not increment the ref-count of the child, so it essentially takes it
 */
err_t vmar_allocate_static(vmar_t* parent, vmar_t* child, uintptr_t offset, size_t page_count);

/**
 * Allocate a new vmar
 *
 * The offset is relative to the start of the parent, if it is VMAR_ANY_OFFSET
 * the child is placed in the lowest gap that can hold it (first-fit).
 *
 * You need to put the child when you are done, this will NOT free it as the tree
 * also has a reference
 */
err_t vmar_allocate(vmar_t* parent, vmar_t** new_child, uintptr_t offset, size_t page_count);

/**
 * Create an anonymous mapping, allocating new memory lazily as required
 */
err_t vmar_map(vmar_t* parent, vmar_t** new_child, uintptr_t offset, size_t page_count);

/**
 * Free a vmar
 *
 * DO NOT CALL MANUALLY
 */
void vmar_free(vmar_t* vmar);

/**
 * Unmap the vmar and all its children, this will not free
 * the vmars but it will disconnect them from each other
 */
void vmar_unmap(vmar_t* vmar);

/**
 * Dump the VMAR tree
 */
void vmar_dump(vmar_t* root);
