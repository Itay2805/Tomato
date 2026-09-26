#pragma once

#include <stdint.h>

/**
 * What each index of the random request should map into
 * we trust the bootloader to provide the next random it
 * can give us
 */
typedef enum random_index {
    RANDOM_INDEX_STACK_CHK_GUARD,
    RANDOM_INDEX_MAX,
} random_index_t;

uint64_t boot_random_get(random_index_t index);
