#include "object.h"
#include "lib/assert.h"
#include "lib/trace.h"
#include "mem/vmar.h"

#include <stdatomic.h>

void object_free(object_t* object) {
    atomic_thread_fence(memory_order_acquire);

    switch (object->kind) {
        case OBJECT_KIND_VMAR: {
            vmar_free((vmar_t*)object);
        } break;

        default:
            ASSERT(0, "Unknown object kind %d", object->kind);
    }
}
