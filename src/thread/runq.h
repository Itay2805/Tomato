#pragma once

#include "lib/list.h"

typedef struct run_queue_entry {
    list_entry_t link;
} run_queue_entry_t;

void run_queue_init(void);

void run_queue_put(run_queue_entry_t* thread);

run_queue_entry_t* run_queue_get(void);
