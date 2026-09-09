#include "list.h"
#include "lib/assert.h"

[[gnu::cold, noreturn]]
void __list_add_valid_or_report(list_entry_t* new, list_entry_t* prev, list_entry_t* next) {
    ASSERT(next->prev == prev,
           "list_add corruption. next->prev should be prev (%px), but was %px. (next=%px).\n", prev,
           next->prev, next);

    ASSERT(prev->next == next,
           "list_add corruption. prev->next should be next (%px), but was %px. (prev=%px).\n", next,
           prev->next, prev);

    ASSERT(new != prev && new != next, "list_add double add: new=%px, prev=%px, next=%px.\n", new,
           prev, next);

    __builtin_trap();
}

[[gnu::cold, noreturn]]
void __list_del_entry_valid_or_report(list_entry_t* entry) {
    list_entry_t* prev = entry->prev;
    list_entry_t* next = entry->next;

    ASSERT(prev->next != entry,
           "list_del corruption. prev->next should be %px, but was %px. (prev=%px)\n", entry,
           prev->next, prev);

    ASSERT(next->prev != entry,
           "list_del corruption. next->prev should be %px, but was %px. (next=%px)\n", entry,
           next->prev, next);

    __builtin_trap();
}
