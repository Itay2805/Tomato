#pragma once

#include "util/defs.h"

/**
 * The debug console is where the traces go, it is the serial port that the firmware
 * describes in the ACPI DBG2 table, without one the traces go nowhere
 */

/**
 * Find the debug console, called first thing on boot. If it doesn't need to be mapped
 * (port io) it is used right away, otherwise the traces are dropped until it is mapped.
 */
INIT_CODE void debugcon_init(void);

/**
 * Map the debug console into the kernel page tables, called by the early memory
 * init once the kernel region is setup
 */
INIT_CODE void debugcon_early_map(void);

/**
 * Write a single character, the caller is responsible for serializing calls
 */
void debugcon_putc(char c);
