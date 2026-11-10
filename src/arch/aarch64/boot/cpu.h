#pragma once

#include <stddef.h>

/**
 * Setup the cpu locals and the exception stack of the current core
 */
void cpu_init(size_t cpu_id);
