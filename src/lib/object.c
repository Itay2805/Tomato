#include "object.h"
#include "lib/assert.h"
#include "lib/container_of.h"
#include "lib/trace.h"
#include "mem/vmar.h"

#include "thread/thread.h"
#include "util/atomic.h"

void object_free(object_t* object) {
    atomic_fence_acquire();

    switch (object->kind) {
        case OBJECT_KIND_VMAR: {
            vmar_free(container_of(object, vmar_t, object));
        } break;

        case OBJECT_KIND_THREAD: {
            thread_free(container_of(object, thread_t, object));
        } break;

        default:
            ASSERT(0, "Unknown object kind %d", object->kind);
    }
}
