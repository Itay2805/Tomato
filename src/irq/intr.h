#pragma once

#include "util/defs.h"
#include <stdint.h>

INIT_CODE void intr_set_handler(uint8_t vector, void* func);

INIT_CODE void intr_init(void);
