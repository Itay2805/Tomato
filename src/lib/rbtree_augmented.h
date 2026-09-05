/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  (C) 2002  David Woodhouse <dwmw2@infradead.org>
  (C) 2012  Michel Lespinasse <walken@google.com>


  linux/include/linux/rbtree_augmented.h
*/

#pragma once

#include "rbtree.h"

/*
 * Please note - only rb_augment_callbacks_t and the prototypes for
 * rb_insert_augmented() and rb_erase_augmented() are intended to be public.
 * The rest are implementation details you are not expected to depend on.
 *
 * See Documentation/core-api/rbtree.rst for documentation and samples.
 */

typedef struct rb_augment_callbacks {
    void (*propagate)(rb_node_t* node, rb_node_t* stop);
    void (*copy)(rb_node_t* old, rb_node_t* new);
    void (*rotate)(rb_node_t* old, rb_node_t* new);
} rb_augment_callbacks_t;

extern void __rb_insert_augmented(rb_node_t* node, rb_root_t* root,
                                  void (*augment_rotate)(rb_node_t* old, rb_node_t* new));

/*
 * Fixup the rbtree and update the augmented information when rebalancing.
 *
 * On insertion, the user must update the augmented information on the path
 * leading to the inserted node, then call rb_link_node() as usual and
 * rb_insert_augmented() instead of the usual rb_insert_color() call.
 * If rb_insert_augmented() rebalances the rbtree, it will callback into
 * a user provided function to update the augmented information on the
 * affected subtrees.
 */
static inline void rb_insert_augmented(rb_node_t* node, rb_root_t* root,
                                       const rb_augment_callbacks_t* augment) {
    __rb_insert_augmented(node, root, augment->rotate);
}

static inline void rb_insert_augmented_cached(rb_node_t* node, rb_root_cached_t* root, bool newleft,
                                              const rb_augment_callbacks_t* augment) {
    if (newleft)
        root->rb_leftmost = node;
    rb_insert_augmented(node, &root->rb_root, augment);
}

[[clang::always_inline]]
static inline rb_node_t* rb_add_augmented_cached(rb_node_t* node, rb_root_cached_t* tree,
                                                 bool (*less)(rb_node_t*, const rb_node_t*),
                                                 const rb_augment_callbacks_t* augment) {
    rb_node_t** link = &tree->rb_root.rb_node;
    rb_node_t* parent = nullptr;
    bool leftmost = true;

    while (*link) {
        parent = *link;
        if (less(node, parent)) {
            link = &parent->rb_left;
        } else {
            link = &parent->rb_right;
            leftmost = false;
        }
    }

    rb_link_node(node, parent, link);
    augment->propagate(parent, nullptr); /* suboptimal */
    rb_insert_augmented_cached(node, tree, leftmost, augment);

    return leftmost ? node : nullptr;
}

/*
 * Template for declaring augmented rbtree callbacks (generic case)
 *
 * RBSTATIC:    'static' or empty
 * RBNAME:      name of the rb_augment_callbacks structure
 * RBSTRUCT:    type_t of the tree nodes
 * RBFIELD:     name of rb_node_t field within RBSTRUCT
 * RBAUGMENTED: name of field within RBholding_t data for subtree
 * RBCOMPUTE:   name of function that recomputes the RBAUGMENTED data
 */

#define RB_DECLARE_CALLBACKS(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD, RBAUGMENTED, RBCOMPUTE)          \
    static inline void RBNAME##_propagate(rb_node_t* rb, rb_node_t* stop) {                        \
        while (rb != stop) {                                                                       \
            RBSTRUCT* node = rb_entry(rb, RBSTRUCT, RBFIELD);                                      \
            if (RBCOMPUTE(node, true))                                                             \
                break;                                                                             \
            rb = rb_parent(&node->RBFIELD);                                                        \
        }                                                                                          \
    }                                                                                              \
    static inline void RBNAME##_copy(rb_node_t* rb_old, rb_node_t* rb_new) {                       \
        RBSTRUCT* old = rb_entry(rb_old, RBSTRUCT, RBFIELD);                                       \
        RBSTRUCT* new = rb_entry(rb_new, RBSTRUCT, RBFIELD);                                       \
        new->RBAUGMENTED = old->RBAUGMENTED;                                                       \
    }                                                                                              \
    static void RBNAME##_rotate(rb_node_t* rb_old, rb_node_t* rb_new) {                            \
        RBSTRUCT* old = rb_entry(rb_old, RBSTRUCT, RBFIELD);                                       \
        RBSTRUCT* new = rb_entry(rb_new, RBSTRUCT, RBFIELD);                                       \
        new->RBAUGMENTED = old->RBAUGMENTED;                                                       \
        RBCOMPUTE(old, false);                                                                     \
    }                                                                                              \
    RBSTATIC const rb_augment_callbacks_t RBNAME = { .propagate = RBNAME##_propagate,              \
                                                     .copy = RBNAME##_copy,                        \
                                                     .rotate = RBNAME##_rotate };

/*
 * Template for declaring augmented rbtree callbacks,
 * computing RBAUGMENTED scalar as max(RBCOMPUTE(node)) for all subtree nodes.
 *
 * RBSTATIC:    'static' or empty
 * RBNAME:      name of the rb_augment_callbacks structure
 * RBSTRUCT:    type_t of the tree nodes
 * RBFIELD:     name of rb_node_t field within RBSTRUCT
 * RBTYPE:      type of the RBAUGMENTED field
 * RBAUGMENTED: name of RBTYPE field within RBholding_t data for subtree
 * RBCOMPUTE:   name of function that returns the per-node RBTYPE scalar
 */

#define RB_DECLARE_CALLBACKS_MAX(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD, RBTYPE, RBAUGMENTED,         \
                                 RBCOMPUTE)                                                        \
    static inline bool RBNAME##_compute_max(RBSTRUCT* node, bool exit) {                           \
        RBSTRUCT* child;                                                                           \
        RBTYPE max = RBCOMPUTE(node);                                                              \
        if (node->RBFIELD.rb_left) {                                                               \
            child = rb_entry(node->RBFIELD.rb_left, RBSTRUCT, RBFIELD);                            \
            if (child->RBAUGMENTED > max)                                                          \
                max = child->RBAUGMENTED;                                                          \
        }                                                                                          \
        if (node->RBFIELD.rb_right) {                                                              \
            child = rb_entry(node->RBFIELD.rb_right, RBSTRUCT, RBFIELD);                           \
            if (child->RBAUGMENTED > max)                                                          \
                max = child->RBAUGMENTED;                                                          \
        }                                                                                          \
        if (exit && node->RBAUGMENTED == max)                                                      \
            return true;                                                                           \
        node->RBAUGMENTED = max;                                                                   \
        return false;                                                                              \
    }                                                                                              \
    RB_DECLARE_CALLBACKS(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD, RBAUGMENTED, RBNAME##_compute_max)

#define RB_RED   0
#define RB_BLACK 1

#define __rb_parent(pc) ((rb_node_t*)(pc & ~3))

#define __rb_color(pc)    ((pc) & 1)
#define __rb_is_black(pc) __rb_color(pc)
#define __rb_is_red(pc)   (!__rb_color(pc))
#define rb_color(rb)      __rb_color((rb)->__rb_parent_color)
#define rb_is_red(rb)     __rb_is_red((rb)->__rb_parent_color)
#define rb_is_black(rb)   __rb_is_black((rb)->__rb_parent_color)

static inline void rb_set_parent(rb_node_t* rb, rb_node_t* p) {
    rb->__rb_parent_color = rb_color(rb) + (unsigned long)p;
}

static inline void rb_set_parent_color(rb_node_t* rb, rb_node_t* p, int color) {
    rb->__rb_parent_color = (unsigned long)p + color;
}

static inline void __rb_change_child(rb_node_t* old, rb_node_t* new, rb_node_t* parent,
                                     rb_root_t* root) {
    if (parent) {
        if (parent->rb_left == old)
            parent->rb_left = new;
        else
            parent->rb_right = new;
    } else
        root->rb_node = new;
}

extern void __rb_erase_color(rb_node_t* parent, rb_root_t* root,
                             void (*augment_rotate)(rb_node_t* old, rb_node_t* new));

[[clang::always_inline]]
static inline rb_node_t* __rb_erase_augmented(rb_node_t* node, rb_root_t* root,
                                              const rb_augment_callbacks_t* augment) {
    rb_node_t* child = node->rb_right;
    rb_node_t* tmp = node->rb_left;
    rb_node_t *parent, *rebalance;
    unsigned long pc;

    if (!tmp) {
        /*
         * Case 1: node to erase has no more than 1 child (easy!)
         *
         * Note that if there is one child it must be red due to 5)
         * and node must be black due to 4). We adjust colors locally
         * so as to bypass __rb_erase_color() later on.
         */
        pc = node->__rb_parent_color;
        parent = __rb_parent(pc);
        __rb_change_child(node, child, parent, root);
        if (child) {
            child->__rb_parent_color = pc;
            rebalance = nullptr;
        } else
            rebalance = __rb_is_black(pc) ? parent : nullptr;
        tmp = parent;
    } else if (!child) {
        /* Still case 1, but this time the child is node->rb_left */
        tmp->__rb_parent_color = pc = node->__rb_parent_color;
        parent = __rb_parent(pc);
        __rb_change_child(node, tmp, parent, root);
        rebalance = nullptr;
        tmp = parent;
    } else {
        rb_node_t *successor = child, *child2;

        tmp = child->rb_left;
        if (!tmp) {
            /*
             * Case 2: node's successor is its right child
             *
             *    (n)          (s)
             *    / \          / \
             *  (x) (s)  ->  (x) (c)
             *        \
             *        (c)
             */
            parent = successor;
            child2 = successor->rb_right;

            augment->copy(node, successor);
        } else {
            /*
             * Case 3: node's successor is leftmost under
             * node's right child subtree
             *
             *    (n)          (s)
             *    / \          / \
             *  (x) (y)  ->  (x) (y)
             *      /            /
             *    (p)          (p)
             *    /            /
             *  (s)          (c)
             *    \
             *    (c)
             */
            do {
                parent = successor;
                successor = tmp;
                tmp = tmp->rb_left;
            } while (tmp);
            child2 = successor->rb_right;
            parent->rb_left = child2;
            successor->rb_right = child;
            rb_set_parent(child, successor);

            augment->copy(node, successor);
            augment->propagate(parent, successor);
        }

        tmp = node->rb_left;
        successor->rb_left = tmp;
        rb_set_parent(tmp, successor);

        pc = node->__rb_parent_color;
        tmp = __rb_parent(pc);
        __rb_change_child(node, successor, tmp, root);

        if (child2) {
            rb_set_parent_color(child2, parent, RB_BLACK);
            rebalance = nullptr;
        } else {
            rebalance = rb_is_black(successor) ? parent : nullptr;
        }
        successor->__rb_parent_color = pc;
        tmp = successor;
    }

    augment->propagate(tmp, nullptr);
    return rebalance;
}

[[clang::always_inline]]
static inline void rb_erase_augmented(rb_node_t* node, rb_root_t* root,
                                      const rb_augment_callbacks_t* augment) {
    rb_node_t* rebalance = __rb_erase_augmented(node, root, augment);
    if (rebalance)
        __rb_erase_color(rebalance, root, augment->rotate);
}

[[clang::always_inline]]
static inline void rb_erase_augmented_cached(rb_node_t* node, rb_root_cached_t* root,
                                             const rb_augment_callbacks_t* augment) {
    if (root->rb_leftmost == node)
        root->rb_leftmost = rb_next(node);
    rb_erase_augmented(node, &root->rb_root, augment);
}
