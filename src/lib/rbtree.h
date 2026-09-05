/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>


  linux/include/linux/rbtree.h

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...

  See Documentation/core-api/rbtree.rst for documentation and samples.
*/

#pragma once

#include "container_of.h"
#include "rbtree_types.h"
#include <stddef.h>

#define rb_parent(r) ((rb_node_t*)((r)->__rb_parent_color & ~3))

#define rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root) (READ_ONCE((root)->rb_node) == nullptr)

/* 'empty' nodes are nodes that are known not to be inserted in an rbtree */
#define RB_EMPTY_NODE(node) ((node)->__rb_parent_color == (unsigned long)(node))
#define RB_CLEAR_NODE(node) ((node)->__rb_parent_color = (unsigned long)(node))

#define RB_EMPTY_LINKED_NODE(lnode) RB_EMPTY_NODE(&(lnode)->node)
#define RB_CLEAR_LINKED_NODE(lnode)                                                                \
    ({                                                                                             \
        RB_CLEAR_NODE(&(lnode)->node);                                                             \
        (lnode)->prev = (lnode)->next = nullptr;                                                   \
    })

void rb_insert_color(rb_node_t* node, rb_root_t* root);
void rb_erase(rb_node_t* node, rb_root_t* root);
bool rb_erase_linked(rb_node_linked_t* node, rb_root_linked_t* root);

/* Find logical next and previous nodes in a tree */
rb_node_t* rb_next(const rb_node_t* node);
rb_node_t* rb_prev(const rb_node_t* node);

/*
 * This function returns the first node (in sort order) of the tree.
 */
static inline rb_node_t* rb_first(const rb_root_t* root) {
    rb_node_t* n;

    n = root->rb_node;
    if (!n)
        return nullptr;
    while (n->rb_left)
        n = n->rb_left;
    return n;
}

/*
 * This function returns the last node (in sort order) of the tree.
 */
static inline rb_node_t* rb_last(const rb_root_t* root) {
    rb_node_t* n;

    n = root->rb_node;
    if (!n)
        return nullptr;
    while (n->rb_right)
        n = n->rb_right;
    return n;
}

/* Postorder iteration - always visit the parent after its children */
rb_node_t* rb_first_postorder(const rb_root_t* root);
rb_node_t* rb_next_postorder(const rb_node_t* node);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
void rb_replace_node(rb_node_t* victim, rb_node_t* new, rb_root_t* root);
void rb_replace_node_rcu(rb_node_t* victim, rb_node_t* new, rb_root_t* root);

static inline void rb_link_node(rb_node_t* node, rb_node_t* parent, rb_node_t** rb_link) {
    node->__rb_parent_color = (unsigned long)parent;
    node->rb_left = node->rb_right = nullptr;

    *rb_link = node;
}

#define rb_entry_safe(ptr, type, member)                                                           \
    ({                                                                                             \
        typeof(ptr) ____ptr = (ptr);                                                               \
        ____ptr ? rb_entry(____ptr, type, member) : nullptr;                                       \
    })

/**
 * rbtree_postorder_for_each_entry_safe - iterate in post-order over rb_root of
 * given type allowing the backing memory of @pos to be invalidated
 *
 * @pos:	the 'type *' to use as a loop cursor.
 * @n:		another 'type *' to use as temporary storage
 * @root:	'rb_root *' of the rbtree.
 * @field:	the name of the rb_node field within 'type'.
 *
 * rbtree_postorder_for_each_entry_safe() provides a similar guarantee as
 * list_for_each_entry_safe() and allows the iteration to continue independent
 * of changes to @pos by the body of the loop.
 *
 * Note, however, that it cannot handle other modifications that re-order the
 * rbtree it is iterating over. This includes calling rb_erase() on @pos, as
 * rb_erase() may rebalance the tree, causing us to miss some nodes.
 */
#define rbtree_postorder_for_each_entry_safe(pos, n, root, field)                                  \
    for (pos = rb_entry_safe(rb_first_postorder(root), typeof(*pos), field);                       \
         pos && ({                                                                                 \
             n = rb_entry_safe(rb_next_postorder(&pos->field), typeof(*pos), field);               \
             1;                                                                                    \
         });                                                                                       \
         pos = n)

/* Same as rb_first(), but O(1) */
#define rb_first_cached(root) (root)->rb_leftmost

static inline void rb_insert_color_cached(rb_node_t* node, rb_root_cached_t* root, bool leftmost) {
    if (leftmost)
        root->rb_leftmost = node;
    rb_insert_color(node, &root->rb_root);
}

static inline rb_node_t* rb_erase_cached(rb_node_t* node, rb_root_cached_t* root) {
    rb_node_t* leftmost = nullptr;

    if (root->rb_leftmost == node)
        leftmost = root->rb_leftmost = rb_next(node);

    rb_erase(node, &root->rb_root);

    return leftmost;
}

static inline void rb_replace_node_cached(rb_node_t* victim, rb_node_t* new,
                                          rb_root_cached_t* root) {
    if (root->rb_leftmost == victim)
        root->rb_leftmost = new;
    rb_replace_node(victim, new, &root->rb_root);
}

/*
 * The below helper functions use 2 operators with 3 different
 * calling conventions. The operators are related like:
 *
 *	comp(a->key,b) < 0  := less(a,b)
 *	comp(a->key,b) > 0  := less(b,a)
 *	comp(a->key,b) == 0 := !less(a,b) && !less(b,a)
 *
 * If these operators define a partial order on the elements we make no
 * guarantee on which of the elements matching the key is found. See
 * rb_find().
 *
 * The reason for this is to allow the find() interface without requiring an
 * on-stack dummy object, which might not be feasible due to object size.
 */

/**
 * rb_add_cached() - insert @node into the leftmost cached tree @tree
 * @node: node to insert
 * @tree: leftmost cached tree to insert @node into
 * @less: operator defining the (partial) node order
 *
 * Returns @node when it is the new leftmost, or NULL.
 */
[[clang::always_inline]]
static inline rb_node_t* rb_add_cached(rb_node_t* node, rb_root_cached_t* tree,
                                       bool (*less)(rb_node_t*, const rb_node_t*)) {
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
    rb_insert_color_cached(node, tree, leftmost);

    return leftmost ? node : nullptr;
}

[[clang::always_inline]]
static inline void __rb_add(rb_node_t* node, rb_root_t* tree,
                            bool (*less)(rb_node_t*, const rb_node_t*),
                            void (*linkop)(rb_node_t*, rb_node_t*, rb_node_t**)) {
    rb_node_t** link = &tree->rb_node;
    rb_node_t* parent = nullptr;

    while (*link) {
        parent = *link;
        if (less(node, parent))
            link = &parent->rb_left;
        else
            link = &parent->rb_right;
    }

    linkop(node, parent, link);
    rb_link_node(node, parent, link);
    rb_insert_color(node, tree);
}

#define __node_2_linked_node(_n) rb_entry((_n), rb_node_linked_t, node)

static inline void rb_link_linked_node(rb_node_t* node, rb_node_t* parent, rb_node_t** link) {
    if (!parent)
        return;

    rb_node_linked_t* nnew = __node_2_linked_node(node);
    rb_node_linked_t* npar = __node_2_linked_node(parent);

    if (link == &parent->rb_left) {
        nnew->prev = npar->prev;
        nnew->next = npar;
        npar->prev = nnew;
        if (nnew->prev)
            nnew->prev->next = nnew;
    } else {
        nnew->next = npar->next;
        nnew->prev = npar;
        npar->next = nnew;
        if (nnew->next)
            nnew->next->prev = nnew;
    }
}

/**
 * rb_add_linked() - insert @node into the leftmost linked tree @tree
 * @node: node to insert
 * @tree: linked tree to insert @node into
 * @less: operator defining the (partial) node order
 *
 * Returns @true when @node is the new leftmost, @false otherwise.
 */
[[clang::always_inline]]
static inline bool rb_add_linked(rb_node_linked_t* node, rb_root_linked_t* tree,
                                 bool (*less)(rb_node_t*, const rb_node_t*)) {
    __rb_add(&node->node, &tree->rb_root, less, rb_link_linked_node);
    if (!node->prev)
        tree->rb_leftmost = node;
    return !node->prev;
}

/* Empty linkop function which is optimized away by the compiler */
[[clang::always_inline]]
static inline void rb_link_noop(rb_node_t* n, rb_node_t* p, rb_node_t** l) {
    (void)n;
    (void)p;
    (void)l;
}

/**
 * rb_add() - insert @node into @tree
 * @node: node to insert
 * @tree: tree to insert @node into
 * @less: operator defining the (partial) node order
 */
[[clang::always_inline]]
static inline void rb_add(rb_node_t* node, rb_root_t* tree,
                          bool (*less)(rb_node_t*, const rb_node_t*)) {
    __rb_add(node, tree, less, rb_link_noop);
}

/**
 * rb_find_add_cached() - find equivalent @node in @tree, or add @node
 * @node: node to look-for / insert
 * @tree: tree to search / modify
 * @cmp: operator defining the node order
 *
 * Returns the rb_node matching @node, or NULL when no match is found and @node
 * is inserted.
 */
[[clang::always_inline]]
static inline rb_node_t* rb_find_add_cached(rb_node_t* node, rb_root_cached_t* tree,
                                            int (*cmp)(const rb_node_t* new,
                                                       const rb_node_t* exist)) {
    bool leftmost = true;
    rb_node_t** link = &tree->rb_root.rb_node;
    rb_node_t* parent = nullptr;
    int c;

    while (*link) {
        parent = *link;
        c = cmp(node, parent);

        if (c < 0) {
            link = &parent->rb_left;
        } else if (c > 0) {
            link = &parent->rb_right;
            leftmost = false;
        } else {
            return parent;
        }
    }

    rb_link_node(node, parent, link);
    rb_insert_color_cached(node, tree, leftmost);
    return nullptr;
}

/**
 * rb_find_add() - find equivalent @node in @tree, or add @node
 * @node: node to look-for / insert
 * @tree: tree to search / modify
 * @cmp: operator defining the node order
 *
 * Returns the rb_node matching @node, or NULL when no match is found and @node
 * is inserted.
 */
[[clang::always_inline]]
static inline rb_node_t* rb_find_add(rb_node_t* node, rb_root_t* tree,
                                     int (*cmp)(rb_node_t*, const rb_node_t*)) {
    rb_node_t** link = &tree->rb_node;
    rb_node_t* parent = nullptr;
    int c;

    while (*link) {
        parent = *link;
        c = cmp(node, parent);

        if (c < 0)
            link = &parent->rb_left;
        else if (c > 0)
            link = &parent->rb_right;
        else
            return parent;
    }

    rb_link_node(node, parent, link);
    rb_insert_color(node, tree);
    return nullptr;
}

/**
 * rb_find() - find @key in tree @tree
 * @key: key to match
 * @tree: tree to search
 * @cmp: operator defining the node order
 *
 * Returns the rb_node matching @key or NULL.
 */
[[clang::always_inline]]
static inline rb_node_t* rb_find(const void* key, const rb_root_t* tree,
                                 int (*cmp)(const void* key, const rb_node_t*)) {
    rb_node_t* node = tree->rb_node;

    while (node) {
        int c = cmp(key, node);

        if (c < 0)
            node = node->rb_left;
        else if (c > 0)
            node = node->rb_right;
        else
            return node;
    }

    return nullptr;
}

/**
 * rb_find_first() - find the first @key in @tree
 * @key: key to match
 * @tree: tree to search
 * @cmp: operator defining node order
 *
 * Returns the leftmost node matching @key, or NULL.
 */
[[clang::always_inline]]
static inline rb_node_t* rb_find_first(const void* key, const rb_root_t* tree,
                                       int (*cmp)(const void* key, const rb_node_t*)) {
    rb_node_t* node = tree->rb_node;
    rb_node_t* match = nullptr;

    while (node) {
        int c = cmp(key, node);

        if (c <= 0) {
            if (!c)
                match = node;
            node = node->rb_left;
        } else if (c > 0) {
            node = node->rb_right;
        }
    }

    return match;
}

/**
 * rb_next_match() - find the next @key in @tree
 * @key: key to match
 * @node: tree to search
 * @cmp: operator defining node order
 *
 * Returns the next node matching @key, or NULL.
 */
[[clang::always_inline]]
static inline rb_node_t* rb_next_match(const void* key, rb_node_t* node,
                                       int (*cmp)(const void* key, const rb_node_t*)) {
    node = rb_next(node);
    if (node && cmp(key, node))
        node = nullptr;
    return node;
}

/**
 * rb_for_each() - iterates a subtree matching @key
 * @node: iterator
 * @key: key to match
 * @tree: tree to search
 * @cmp: operator defining node order
 */
#define rb_for_each(node, key, tree, cmp)                                                          \
    for ((node) = rb_find_first((key), (tree), (cmp)); (node);                                     \
         (node) = rb_next_match((key), (node), (cmp)))
