#pragma once

#include "arch/cpu_local.h"
#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

/**
 * How many cores we have
 */
LATE_RO extern size_t g_cpu_count;

/**
 * The array of bases of all cores
 */
LATE_RO extern ptrdiff_t* g_cpu_locals_offsets;

/**
 * The current cpu id
 */
DEFINE_CPU_LOCAL(extern, size_t, cpu_id);

/**
 * The base of the cpu locals of the current thread
 */
DEFINE_CPU_LOCAL(extern, ptrdiff_t, cpu_locals_offset);
