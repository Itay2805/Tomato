#pragma once

#include "lib/list.h"
#include "sync/mutex.h"
#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

/**
 * The metadata of the allocator, in a read-only region
 */
typedef struct mem_alloc_meta {
    /**
     * The objects in each slab
     */
    uint16_t objects_per_slab;

    /**
     * The stride of each object
     */
    uint16_t object_stride;

    /**
     * The size of each object
     */
    uint16_t object_size;

    /**
     * The object's alignment
     */
    uint16_t object_align;
} mem_alloc_meta_t;

/**
 * The actual allocator data
 */
typedef struct mem_alloc {
    /**
     * Lock to protect the allocator
     */
    mutex_t lock;

    /**
     * List of slabs with available objects
     */
    list_t partial;

    /**
     * List of full slabs
     */
    list_t full;

    /**
     * List of empty slabs, available as cache
     */
    list_t empty;
} mem_alloc_t;

#define DEFINE_ALLOC(type)                                                                         \
    static_assert(sizeof(type) <= UINT16_MAX);                                                     \
    static_assert(alignof(type) <= UINT16_MAX);                                                    \
    static constexpr size_t m_##type##_alloc_stride =                                              \
        ALIGN_UP(MAX(sizeof(void*), sizeof(type)), alignof(type));                                 \
    static_assert(m_##type##_alloc_stride <= UINT16_MAX);                                          \
    static_assert(m_##type##_alloc_stride >= sizeof(type));                                        \
    static constexpr mem_alloc_meta_t m_##type##_alloc_meta = {                                    \
        .object_size = sizeof(type),                                                               \
        .object_align = alignof(type),                                                             \
        .object_stride = m_##type##_alloc_stride,                                                  \
    };                                                                                             \
    static mem_alloc_t m_##type##_alloc = {                                                        \
        .partial = LIST_INIT(m_##type##_alloc.partial),                                            \
        .full = LIST_INIT(m_##type##_alloc.full),                                                  \
        .empty = LIST_INIT(m_##type##_alloc.empty),                                                \
    };

void* mem_alloc(mem_alloc_t* alloc, const mem_alloc_meta_t* meta);
void mem_free(mem_alloc_t* alloc, const mem_alloc_meta_t* meta, void* ptr);

/**
 * Allocate object of the given type
 */
#define ALLOC(type) ((type*)mem_alloc(&m_##type##_alloc, &m_##type##_alloc_meta))

/**
 * Free object of the given type
 */
#define FREE(type, ptr)                                                                            \
    do {                                                                                           \
        type* value__ = ptr;                                                                       \
        mem_free(&m_##type##_alloc, &m_##type##_alloc_meta, value__);                              \
    } while (0)