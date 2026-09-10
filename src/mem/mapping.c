#include "mapping.h"
#include "lib/rbtree/rbtree_types.h"
#include "mem/vmar.h"
#include "util/defs.h"
#include <stdint.h>

vmar_t g_kernel_region = {
    .name = "kernel",
    .kind = VMAR_KIND_REGION,
    .region = {
        .children = RB_ROOT_LINKED,
    },
};

vmar_t g_direct_map = {
    .name = "direct-map",
    .kind = VMAR_KIND_SPECIAL,
};

#define KERNEL_REGION(_name)                                                                       \
    extern char __##_name##_start[];                                                               \
    extern char __##_name##_page_count[];                                                          \
    vmar_t g_kernel_##_name##_mapping = {                                                          \
        .name = "kernel " #_name,                                                                  \
        .base = __##_name##_start,                                                                 \
        .page_count = (uintptr_t)__##_name##_page_count,                                           \
        .kind = VMAR_KIND_SPECIAL,                                                                 \
    };

KERNEL_REGION(limine_requests);

KERNEL_REGION(text);
KERNEL_REGION(rodata);
KERNEL_REGION(data);

vmar_t g_user_region = {
    .name = "user",
    .kind = VMAR_KIND_REGION,
    .region = {
        .children = RB_ROOT_LINKED,
    },
};
