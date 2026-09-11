#pragma once

#include <stdint.h>

extern uintptr_t g_early_alloc_top;

/**
 * Allocate a single page from the boot allocator
 */
void* early_phys_alloc_page();
