#pragma once

#include "boot/limine.h"

/**
 * The flags of the MP request, there are none on aarch64
 */
#define LIMINE_MP_REQUEST_ARCH_FLAGS 0

/**
 * Check if the given cpu of the MP response is the BSP
 */
static inline bool limine_mp_is_bsp(struct limine_mp_response* mp, struct limine_mp_info* info) {
    return info->mpidr == mp->bsp_mpidr;
}
