#pragma once

#include <stddef.h>

void tss_alloc(size_t cpu_count);

void gdt_init();
