#pragma once

#include "mem/pte.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Allocate a zeroed page before the physical allocator is ready, returns its direct map address
 */
void* early_phys_alloc_page(void);

/**
 * Get the address of physical memory in the bootloader's direct map, unlike ours it also
 * covers the executable and the ACPI tables, so only valid before switching to our page tables
 */
void* early_phys_to_direct(uint64_t phys);

void early_virt_init(void);

/**
 * Map a range into the early page tables, only valid during early_virt_init, before the
 * rest of the memory is given to the physical allocator
 */
void early_virt_map(void* virt, uint64_t phys, size_t num_pages, pte_flags_t flags);

/**
 * Setup the address space layout for the paging mode the bootloader gave us: the kernel and
 * user regions, the amount of page table levels and the root page tables.
 *
 * Implemented by the arch.
 */
void early_virt_init_layout(void);

/**
 * Switch the current core to the kernel page tables.
 *
 * Implemented by the arch.
 */
void early_virt_switch_table(void);
