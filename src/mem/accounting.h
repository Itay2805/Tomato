#pragma once

#include "util/except.h"
#include <stddef.h>

/**
 * Reserve pages that are going to be allocated later with phys_alloc_reserved,
 * fails if there are not enough available pages
 */
err_t mem_acct_reserve(size_t page_count);

/**
 * Give back reserved pages that are never going to be allocated
 */
void mem_acct_unreserve(size_t page_count);

/**
 * Add new pages to the total, after they were added to the allocator
 */
void mem_acct_add_total(size_t page_count);

/**
 * Add pages that are already in use to the total as committed, for memory that
 * was allocated before the allocator was up and never went through it
 */
void mem_acct_add_committed(size_t page_count);

/**
 * Commit available pages, fails if there are not enough of them
 */
bool mem_acct_commit(size_t page_count);

/**
 * Commit pages that were reserved, can't fail
 */
void mem_acct_commit_reserved(size_t page_count);

/**
 * Uncommit pages, after they were returned to the allocator
 */
void mem_acct_uncommit(size_t page_count);

/**
 * Uncommit pages back into the reservation they came from, after
 * they were returned to the allocator
 */
void mem_acct_uncommit_to_reserved(size_t page_count);

void mem_acct_dump(void);
