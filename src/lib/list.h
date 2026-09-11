/* SPDX-License-Identifier: GPL-2.0 */
#pragma once

#include "util/defs.h"
#include <stddef.h>

#define LIST_POISON1 ((void*)0x100)
#define LIST_POISON2 ((void*)0x122)

typedef struct list_entry {
    struct list_entry* next;
    struct list_entry* prev;
} list_entry_t;

typedef list_entry_t list_t;

#define LIST_INIT(name) { &(name), &(name) }

[[gnu::cold, noreturn]]
void __list_add_valid_or_report(list_entry_t* new, list_entry_t* prev, list_entry_t* next);

[[clang::always_inline]]
static inline void __list_add_valid(list_entry_t* new, list_entry_t* prev, list_entry_t* next) {
    if (LIKELY(next->prev == prev && prev->next == next && new != prev&& new != next)) {
        return;
    }

    __list_add_valid_or_report(new, prev, next);
}

[[gnu::cold, noreturn]]
void __list_del_entry_valid_or_report(list_entry_t* entry);

[[clang::always_inline]]
static inline void __list_del_entry_valid(list_entry_t* entry) {
    list_entry_t* prev = entry->prev;
    list_entry_t* next = entry->next;

    if (LIKELY(prev->next == entry && next->prev == entry))
        return;

    __list_del_entry_valid_or_report(entry);
}

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 *
 * Must be inlined to ensure it can be safely called
 * with initdata arguments.
 */
[[clang::always_inline]]
static inline void __list_add(list_entry_t* new, list_entry_t* prev, list_entry_t* next) {
    __list_add_valid(new, prev, next);
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 *
 * Must be inlined to ensure it can be safely called
 * with initdata arguments.
 */
[[clang::always_inline]]
static inline void list_add(list_t* head, list_entry_t* new) {
    __list_add(new, head, head->next);
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(list_t* head, list_entry_t* new) {
    __list_add(new, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(list_entry_t* prev, list_entry_t* next) {
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(list_entry_t* entry) {
    __list_del_entry_valid(entry);
    __list_del(entry->prev, entry->next);
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(list_entry_t* entry) {
    __list_del_entry(entry);
    entry->next = LIST_POISON1;
    entry->prev = LIST_POISON2;
}

/**
 * list_is_first -- tests whether @list is the first entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_first(const list_entry_t* list, const list_entry_t* head) {
    return list->prev == head;
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_last(const list_entry_t* list, const list_entry_t* head) {
    return list->next == head;
}

/**
 * list_is_head - tests whether @list is the list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_head(const list_entry_t* list, const list_entry_t* head) {
    return list == head;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const list_entry_t* head) {
    return head->next == head;
}

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &list_entry_t pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)

/**
 * list_last_entry - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)

/**
 * list_first_entry_or_null - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note that if the list is empty, it returns NULL.
 */
#define list_first_entry_or_null(ptr, type, member)                                                \
    ({                                                                                             \
        list_entry_t* head__ = (ptr);                                                              \
        list_entry_t* pos__ = head__->next;                                                        \
        pos__ != head__ ? list_entry(pos__, type, member) : NULL;                                  \
    })

/**
 * list_last_entry_or_null - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note that if the list is empty, it returns NULL.
 */
#define list_last_entry_or_null(ptr, type, member)                                                 \
    ({                                                                                             \
        list_entry_t* head__ = (ptr);                                                              \
        list_entry_t* pos__ = head__->prev;                                                        \
        pos__ != head__ ? list_entry(pos__, type, member) : NULL;                                  \
    })

/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_next_entry_circular - get the next element in list
 * @pos:	the type * to cursor.
 * @head:	the list head to take the element from.
 * @member:	the name of the list_head within the struct.
 *
 * Wraparound if pos is the last element (return the first element).
 * Note, that list is expected to be not empty.
 */
#define list_next_entry_circular(pos, head, member)                                                \
    (list_is_last(&(pos)->member, head) ? list_first_entry(head, typeof(*(pos)), member)           \
                                        : list_next_entry(pos, member))

/**
 * list_prev_entry - get the prev element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_prev_entry(pos, member) list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * list_prev_entry_circular - get the prev element in list
 * @pos:	the type * to cursor.
 * @head:	the list head to take the element from.
 * @member:	the name of the list_head within the struct.
 *
 * Wraparound if pos is the first element (return the last element).
 * Note, that list is expected to be not empty.
 */
#define list_prev_entry_circular(pos, head, member)                                                \
    (list_is_first(&(pos)->member, head) ? list_last_entry(head, typeof(*(pos)), member)           \
                                         : list_prev_entry(pos, member))

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &list_entry_t to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head)                                                                   \
    for (pos = (head)->next; !list_is_head(pos, (head)); pos = pos->next)

/**
 * list_for_each_continue - continue iteration over a list
 * @pos:	the &list_entry_t to use as a loop cursor.
 * @head:	the head for your list.
 *
 * Continue to iterate over a list, continuing after the current position.
 */
#define list_for_each_continue(pos, head)                                                          \
    for (pos = pos->next; !list_is_head(pos, (head)); pos = pos->next)

/**
 * list_for_each_prev	-	iterate over a list backwards
 * @pos:	the &list_entry_t to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head)                                                              \
    for (pos = (head)->prev; !list_is_head(pos, (head)); pos = pos->prev)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:	the &list_entry_t to use as a loop cursor.
 * @n:		another &list_entry_t to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head)                                                           \
    for (pos = (head)->next, n = pos->next; !list_is_head(pos, (head)); pos = n, n = pos->next)

/**
 * list_for_each_prev_safe - iterate over a list backwards safe against removal of list entry
 * @pos:	the &list_entry_t to use as a loop cursor.
 * @n:		another &list_entry_t to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_prev_safe(pos, n, head)                                                      \
    for (pos = (head)->prev, n = pos->prev; !list_is_head(pos, (head)); pos = n, n = pos->prev)

/**
 * list_count_nodes - count nodes in the list
 * @head:	the head for your list.
 */
static inline size_t list_count_nodes(list_entry_t* head) {
    list_entry_t* pos;
    size_t count = 0;

    list_for_each(pos, head) count++;

    return count;
}

/**
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:	the type * to cursor
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member) list_is_head(&pos->member, (head))

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member)                                                     \
    for (pos = list_first_entry(head, typeof(*pos), member);                                       \
         !list_entry_is_head(pos, head, member); pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)                                             \
    for (pos = list_last_entry(head, typeof(*pos), member);                                        \
         !list_entry_is_head(pos, head, member); pos = list_prev_entry(pos, member))

/**
 * list_prepare_entry - prepare a pos entry for use in list_for_each_entry_continue()
 * @pos:	the type * to use as a start point
 * @head:	the head of the list
 * @member:	the name of the list_head within the struct.
 *
 * Prepares a pos entry for use as a start point in list_for_each_entry_continue().
 */
#define list_prepare_entry(pos, head, member) ((pos) ?: list_entry(head, typeof(*pos), member))

/**
 * list_for_each_entry_continue - continue iteration over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 */
#define list_for_each_entry_continue(pos, head, member)                                            \
    for (pos = list_next_entry(pos, member); !list_entry_is_head(pos, head, member);               \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_continue_reverse - iterate backwards from the given point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Start to iterate over list of given type backwards, continuing after
 * the current position.
 */
#define list_for_each_entry_continue_reverse(pos, head, member)                                    \
    for (pos = list_prev_entry(pos, member); !list_entry_is_head(pos, head, member);               \
         pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_from - iterate over list of given type from the current point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate over list of given type, continuing from current position.
 */
#define list_for_each_entry_from(pos, head, member)                                                \
    for (; !list_entry_is_head(pos, head, member); pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_from_reverse - iterate backwards over list of given type
 *                                    from the current point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate backwards over list of given type, continuing from current position.
 */
#define list_for_each_entry_from_reverse(pos, head, member)                                        \
    for (; !list_entry_is_head(pos, head, member); pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)                                             \
    for (pos = list_first_entry(head, typeof(*pos), member), n = list_next_entry(pos, member);     \
         !list_entry_is_head(pos, head, member); pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_continue - continue list iteration safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate over list of given type, continuing after current point,
 * safe against removal of list entry.
 */
#define list_for_each_entry_safe_continue(pos, n, head, member)                                    \
    for (pos = list_next_entry(pos, member), n = list_next_entry(pos, member);                     \
         !list_entry_is_head(pos, head, member); pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_from - iterate over list from current point safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate over list of given type from current point, safe against
 * removal of list entry.
 */
#define list_for_each_entry_safe_from(pos, n, head, member)                                        \
    for (n = list_next_entry(pos, member); !list_entry_is_head(pos, head, member);                 \
         pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_reverse - iterate backwards over list safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate backwards over list of given type, safe against removal
 * of list entry.
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member)                                     \
    for (pos = list_last_entry(head, typeof(*pos), member), n = list_prev_entry(pos, member);      \
         !list_entry_is_head(pos, head, member); pos = n, n = list_prev_entry(n, member))

/**
 * list_safe_reset_next - reset a stale list_for_each_entry_safe loop
 * @pos:	the loop cursor used in the list_for_each_entry_safe loop
 * @n:		temporary storage used in list_for_each_entry_safe
 * @member:	the name of the list_head within the struct.
 *
 * list_safe_reset_next is not safe to use in general if the list may be
 * modified concurrently (eg. the lock is dropped in the loop body). An
 * exception to this is if the cursor element (pos) is pinned in the list,
 * and list_safe_reset_next is called after re-taking the lock and before
 * completing the current iteration of the loop body.
 */
#define list_safe_reset_next(pos, n, member) n = list_next_entry(pos, member)
