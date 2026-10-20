#pragma once

#include "sync/smp.h"
#include "util/defs.h"
#include <stdint.h>

/**
 * The APIC id of the local core
 */
CPU_LOCAL(extern, uint32_t, g_apic_id);

/**
 * Initialize the APIC globally
 */
INIT_CODE void lapic_init(void);

/**
 * Initialize the APIC per core
 */
INIT_CODE void lapic_init_per_core(void);
