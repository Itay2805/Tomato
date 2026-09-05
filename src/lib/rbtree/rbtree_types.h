/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

typedef struct rb_node {
    unsigned long __rb_parent_color;
    struct rb_node* rb_right;
    struct rb_node* rb_left;
} __attribute__((aligned(sizeof(long)))) rb_node_t;
/* The alignment might seem pointless, but allegedly CRIS needs it */

typedef struct rb_node_linked {
    rb_node_t node;
    struct rb_node_linked* prev;
    struct rb_node_linked* next;
} rb_node_linked_t;

typedef struct rb_root {
    rb_node_t* rb_node;
} rb_root_t;

/*
 * Leftmost-cached rbtrees.
 *
 * We do not cache the rightmost node based on footprint
 * size vs number of potential users that could benefit
 * from O(1) rb_last(). Just not worth it, users that want
 * this feature can always implement the logic explicitly.
 * Furthermore, users that want to cache both pointers may
 * find it a bit asymmetric, but that's ok.
 */
typedef struct rb_root_cached {
    struct rb_root rb_root;
    struct rb_node* rb_leftmost;
} rb_root_cached_t;

/*
 * Leftmost tree with links. This would allow a trivial rb_rightmost update,
 * but that has been omitted due to the lack of users.
 */
typedef struct rb_root_linked {
    struct rb_root rb_root;
    struct rb_node_linked* rb_leftmost;
} rb_root_linked_t;

#define RB_ROOT                                                                                    \
    (rb_root_t) {                                                                                  \
        nullptr,                                                                                   \
    }

#define RB_ROOT_CACHED                                                                             \
    (rb_root_cached_t) {                                                                           \
        {                                                                                          \
            nullptr,                                                                               \
        },                                                                                         \
            nullptr                                                                                \
    }

#define RB_ROOT_LINKED                                                                             \
    (rb_root_linked_t) {                                                                           \
        {                                                                                          \
            nullptr,                                                                               \
        },                                                                                         \
            nullptr                                                                                \
    }
