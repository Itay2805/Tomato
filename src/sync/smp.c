#include "smp.h"
#include "lib/assert.h"
#include "mem/phys.h"
#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

LATE_RO size_t g_cpu_count = 0;
LATE_RO ptrdiff_t* g_cpu_locals_offsets;
DEFINE_CPU_LOCAL(, size_t, cpu_id);
DEFINE_CPU_LOCAL(, ptrdiff_t, cpu_locals_offset);
