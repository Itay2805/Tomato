/* SPDX-License-Identifier: GPL-2.0 */
#pragma once

#include "list.h"

typedef struct hlist {
    struct hlist_node* first;
} hlist_t;

typedef struct hlist_node {
    struct hlist_node* next;
    struct hlist_node** pprev;
} hlist_node_t;

/**
 * hlist_empty - Is the specified hlist_head structure an empty hlist?
 * @h: Structure to check.
 */
static inline int hlist_empty(const hlist_t* h) {
    return !h->first;
}

static inline void __hlist_del(hlist_node_t* n) {
    hlist_node_t* next = n->next;
    hlist_node_t** pprev = n->pprev;

    *pprev = next;
    if (next)
        next->pprev = pprev;
}

/**
 * hlist_del - Delete the specified hlist_node from its list
 * @n: Node to delete.
 *
 * Note that this function leaves the node in hashed state.  Use
 * hlist_del_init() or similar instead to unhash @n.
 */
static inline void hlist_del(hlist_node_t* n) {
    __hlist_del(n);
    n->next = LIST_POISON1;
    n->pprev = LIST_POISON2;
}

/**
 * hlist_add - add a new entry at the beginning of the hlist
 * @h: hlist head to add it after
 * @n: new entry to be added
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void hlist_add(hlist_t* h, hlist_node_t* n) {
    struct hlist_node* first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

#define hlist_entry(ptr, type, member) container_of(ptr, type, member)
