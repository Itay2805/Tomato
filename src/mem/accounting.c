#include "accounting.h"
#include "lib/assert.h"
#include "lib/trace.h"
#include "util/except.h"
#include <stdatomic.h>

/**
 * The pages that are neither committed nor reserved, this is the only counter that
 * decides if a reservation or a commit can go through, it can never go below zero
 * which is what keeps committed + reserved <= total.
 */
static atomic_size_t m_available_pages = 0;

/**
 * The total amount of pages in the system available to the os
 */
static atomic_size_t m_total_pages = 0;

/**
 * The total amount of commited pages
 */
static atomic_size_t m_committed_pages = 0;

/**
 * The total amount of reserved pages
 */
static atomic_size_t m_reserved_pages = 0;

/**
 * Take pages from the available count, if there are not
 * enough of them it is left untouched
 */
static bool mem_acct_take_available(size_t page_count) {
    size_t available = atomic_load_explicit(&m_available_pages, memory_order_relaxed);
    do {
        if (available < page_count) {
            return false;
        }
    } while (!atomic_compare_exchange_weak_explicit(&m_available_pages, &available,
                                                    available - page_count, memory_order_acquire,
                                                    memory_order_relaxed));
    return true;
}

static void checked_atomic_sub(atomic_size_t* counter, size_t page_count) {
    size_t old = atomic_fetch_sub_explicit(counter, page_count, memory_order_relaxed);
    ASSERT(old >= page_count, "memory: accounting underflow (%lu - %lu)", old, page_count);
}

err_t mem_acct_reserve(size_t page_count) {
    CHECK_ERROR(mem_acct_take_available(page_count), OUT_OF_RESOURCES);
    atomic_fetch_add_explicit(&m_reserved_pages, page_count, memory_order_relaxed);
    return SUCCESS;
}

void mem_acct_unreserve(size_t page_count) {
    checked_atomic_sub(&m_reserved_pages, page_count);
    atomic_fetch_add_explicit(&m_available_pages, page_count, memory_order_relaxed);
}

void mem_acct_add_total(size_t page_count) {
    atomic_fetch_add_explicit(&m_total_pages, page_count, memory_order_relaxed);
    atomic_fetch_add_explicit(&m_available_pages, page_count, memory_order_relaxed);
}

void mem_acct_add_committed(size_t page_count) {
    atomic_fetch_add_explicit(&m_total_pages, page_count, memory_order_relaxed);
    atomic_fetch_add_explicit(&m_committed_pages, page_count, memory_order_relaxed);
}

bool mem_acct_commit(size_t page_count) {
    if (!mem_acct_take_available(page_count)) {
        return false;
    }
    atomic_fetch_add_explicit(&m_committed_pages, page_count, memory_order_relaxed);
    return true;
}

void mem_acct_commit_reserved(size_t page_count) {
    checked_atomic_sub(&m_reserved_pages, page_count);
    atomic_fetch_add_explicit(&m_committed_pages, page_count, memory_order_relaxed);
}

void mem_acct_uncommit(size_t page_count) {
    checked_atomic_sub(&m_committed_pages, page_count);
    atomic_fetch_add_explicit(&m_available_pages, page_count, memory_order_relaxed);
}

void mem_acct_uncommit_to_reserved(size_t page_count) {
    checked_atomic_sub(&m_committed_pages, page_count);
    atomic_fetch_add_explicit(&m_reserved_pages, page_count, memory_order_relaxed);
}

void mem_acct_dump(void) {
    TRACE("Memory accounting:");
    TRACE("\tAvailable pages: %lu", m_available_pages);
    TRACE("\tTotal pages: %lu", m_total_pages);
    TRACE("\tCommited pages: %lu", m_committed_pages);
    TRACE("\tReserved pages: %lu", m_reserved_pages);
}
