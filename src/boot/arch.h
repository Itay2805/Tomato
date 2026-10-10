#pragma once

/**
 * Early per-arch initialization
 */
void arch_early_init();

/**
 * Arch initialization
 */
void arch_init();

/**
 * Arch init per-core, not needed on the BSP
 */
void arch_init_per_core();
