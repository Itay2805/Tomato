#pragma once

#include <stddef.h>

/**
 * Early per-arch initialization
 */
void arch_early_init(void);

/**
 * Arch initialization
 */
void arch_init(void);

/**
 * Arch init per-core, not needed on the BSP
 */
void arch_init_per_core(size_t cpu_id);
