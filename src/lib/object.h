#pragma once

#include "lib/assert.h"
#include <stdatomic.h>
typedef enum object_kind {
    OBJECT_KIND_VMAR,
} object_kind_t;

typedef struct kobject {
    /**
     * The type of object we have
     */
    object_kind_t kind;

    /**
     * The ref count of the object
     */
    atomic_size_t ref_count;
} object_t;

#define INIT_OBJECT(KIND) ((object_t){ .kind = KIND, .ref_count = 1 })

static inline void object_get(object_t* object) {
    atomic_fetch_add_explicit(&object->ref_count, 1, memory_order_acquire);
}

void object_free(object_t* object);

static inline void object_put(object_t* object) {
    size_t ref_count = atomic_fetch_sub_explicit(&object->ref_count, 1, memory_order_release);
    ASSERT(ref_count >= 1);

    if (ref_count == 1) {
        object_free(object);
    }
}

#define OBJECT_GET(obj)                                                                            \
    ({                                                                                             \
        typeof(obj) obj__ = obj;                                                                   \
        object_get(&obj__->object);                                                                \
        obj__;                                                                                     \
    })

#define OBJECT_PUT(obj)                                                                            \
    ({                                                                                             \
        typeof(obj) obj__ = obj;                                                                   \
        object_put(&obj__->object);                                                                \
        obj__;                                                                                     \
    })
