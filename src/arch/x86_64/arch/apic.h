#pragma once

#include "sync/smp.h"
#include "util/defs.h"
#include <stdint.h>

/**
 * The APIC id of the local core
 */
DEFINE_CPU_LOCAL(extern, uint32_t, apic_id);

/**
 * Initialize the APIC globally
 */
INIT_CODE void lapic_init(void);

/**
 * Initialize the APIC per core
 */
INIT_CODE void lapic_init_per_core(void);

/**
 * Signal the end of the interrupt being handled
 */
void lapic_eoi(void);

/**
 * Send an IPI to all of the cores but the current one
 */
void lapic_send_ipi_all_excluding_self(uint8_t vector);
