#pragma once

#include "util/defs.h"

/**
 * Initialize the GIC globally
 */
INIT_CODE void gic_init(void);

/**
 * Initialize the GIC CPU interface of the current core
 */
INIT_CODE void gic_init_per_core(void);

/**
 * Acknowledge and handle a pending interrupt
 */
void gic_handle_irq(void);
