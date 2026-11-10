#pragma once

#include "boot/limine.h"

/**
 * The flags of the MP request, we only support x2APIC
 */
#define LIMINE_MP_REQUEST_ARCH_FLAGS 0

/**
 * Check if the given cpu of the MP response is the BSP
 */
static inline bool limine_mp_is_bsp(struct limine_mp_response* mp, struct limine_mp_info* info) {
    return info->lapic_id == mp->bsp_lapic_id;
}
