#include "mapping.h"
#include "arch/virt.h"
#include "lib/object.h"
#include "lib/rbtree/rbtree_types.h"
#include "mem/vmar.h"
#include "util/defs.h"
#include <stdint.h>

vmar_t g_kernel_region = {
    .object = INIT_OBJECT(OBJECT_KIND_VMAR, "kernel"),
    .kind = VMAR_KIND_REGION,
    .region = {
        .children = RB_ROOT_LINKED,
    },
};

vmar_t g_direct_map = {
    .object = INIT_OBJECT(OBJECT_KIND_VMAR, "direct-map"),
    .kind = VMAR_KIND_SPECIAL,
    .locked = true,
};

vmar_t g_buddy_bitmap_mapping = {
    .object = INIT_OBJECT(OBJECT_KIND_VMAR, "buddy-bitmap"),
    .kind = VMAR_KIND_SPECIAL,
    .locked = true,
};

#define KERNEL_REGION(_name)                                                                       \
    extern char __##_name##_start[];                                                               \
    extern char __##_name##_page_count[];                                                          \
    vmar_t g_kernel_##_name##_mapping = {                                                          \
        .object = INIT_OBJECT(OBJECT_KIND_VMAR, "kernel " #_name),                                 \
        .base = __##_name##_start,                                                                 \
        .page_count = (uintptr_t)__##_name##_page_count,                                           \
        .locked = true,                                                                            \
        .kind = VMAR_KIND_SPECIAL,                                                                 \
    };

KERNEL_REGION(limine_requests);

KERNEL_REGION(init_text);
KERNEL_REGION(init_rodata);
KERNEL_REGION(init_data);

KERNEL_REGION(text);
KERNEL_REGION(rodata);
KERNEL_REGION(late_rodata);
KERNEL_REGION(data);

vmar_t g_user_region = {
    .object = INIT_OBJECT(OBJECT_KIND_VMAR, "user"),
    .kind = VMAR_KIND_REGION,
    .region = {
        .children = RB_ROOT_LINKED,
    },
};
