#include "alloc.h"
#include "arch/virt.h"
#include "lib/assert.h"
#include "lib/list.h"
#include "lib/string.h"
#include "lib/trace.h"
#include "mem/phys.h"
#include "mem/virt.h"
#include "sync/mutex.h"
#include <stddefer.h>

static inline mem_alloc_slab_t* object_to_slab(void* p) {
    return ALIGN_DOWN(p, PAGE_SIZE);
}

static mem_alloc_slab_t* slab_create(mem_alloc_t* alloc, const mem_alloc_meta_t* meta) {
    mem_alloc_slab_t* slab = phys_alloc(PAGE_SIZE);
    if (slab == nullptr) {
        return nullptr;
    }

    // we assume most slabs don't get freed so we can
    // mark it as global to save on tlb invalidations
    virt_make_global(slab);

    // setup the metadata
    slab->alloc = alloc;
    slab->free = LIST_INIT(slab->free);

    // link all the objects into the linked list
    void* area = (void*)slab + ALIGN_UP(sizeof(mem_alloc_slab_t), meta->object_align);
    for (int i = 0; i < meta->objects_per_slab; i++) {
        list_entry_t* node = area + meta->object_stride * i;
        list_add(&slab->free, node);

        // we fill the area between this object and the next object with 0xBB
        // just to make it a bit easier to catch overflows on the object
        memset((void*)node + meta->object_size, 0xBB, meta->object_stride - meta->object_size);
    }

    return slab;
}

void* mem_alloc(mem_alloc_t* alloc, const mem_alloc_meta_t* meta) {
    mutex_guard(&alloc->lock);

    // choose a slab to use, prefer partial slabs
    mem_alloc_slab_t* slab;
    if (!list_empty(&alloc->partial)) {
        slab = list_first_entry(&alloc->partial, mem_alloc_slab_t, link);

    } else if (!list_empty(&alloc->empty)) {
        slab = list_first_entry(&alloc->empty, mem_alloc_slab_t, link);

        // move to partial list
        list_del(&slab->link);
        list_add(&alloc->partial, &slab->link);

    } else {
        slab = slab_create(alloc, meta);
        if (slab == nullptr) {
            return nullptr;
        }
        list_add(&alloc->partial, &slab->link);
    }

    ASSERT(!list_empty(&slab->free));
    list_entry_t* object = slab->free.next;

    // remove from the list and clear the object so it can be
    // zeroed, the free path handles the zero of the entire object
    list_del(object);
    *object = (list_entry_t){};

    slab->in_use++;

    // if slab becomes full, move to the full list
    if (slab->in_use == meta->objects_per_slab) {
        list_del(&slab->link);
        list_add(&alloc->full, &slab->link);
    }

    return object;
}

void mem_free(mem_alloc_t* alloc, const mem_alloc_meta_t* meta, void* ptr) {
    if (ptr == nullptr) {
        return;
    }

    mutex_guard(&alloc->lock);

    // get the slab, and ensure it matches the alloc we are freeing from
    mem_alloc_slab_t* slab = object_to_slab(ptr);
    ASSERT(slab->alloc == alloc);

    // add to the freelist of the slab
    memset(ptr, 0, meta->object_size);
    list_entry_t* object = ptr;
    list_add_tail(&slab->free, object);

    // decrease the use count
    ASSERT(slab->in_use != 0);
    const bool was_full = (slab->in_use == meta->objects_per_slab);
    slab->in_use--;

    // if no active objects move to empty
    // if was full, move to partial
    if (slab->in_use == 0) {
        list_del(&slab->link);
        list_add_tail(&alloc->empty, &slab->link);
    } else if (was_full) {
        list_del(&slab->link);
        list_add_tail(&alloc->partial, &slab->link);
    }
}
