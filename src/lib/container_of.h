/* SPDX-License-Identifier: GPL-2.0 */
#pragma once

/* Are two types/vars the same type (ignoring qualifiers)? */
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#define typeof_member(T, m) typeof(((T*)0)->m)

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 * WARNING: any const qualifier of @ptr is lost.
 * Do not use container_of() in new code.
 */
#define container_of(ptr, type, member)                                                            \
    ({                                                                                             \
        static_assert(__same_type(*(ptr), typeof_member(type, member)) ||                          \
                          __same_type(*(ptr), void),                                               \
                      "pointer type mismatch in container_of()");                                  \
        (type*)((void*)(ptr) - offsetof(type, member));                                            \
    })

/**
 * container_of_const - cast a member of a structure out to the containing
 *			structure and preserve the const-ness of the pointer
 * @ptr:		the pointer to the member
 * @type:		the type of the container struct this is embedded in.
 * @member:		the name of the member within the struct.
 *
 * Always prefer container_of_const() instead of container_of() in new code.
 */
#define container_of_const(ptr, type, member)                                                      \
    _Generic(ptr,                                                                                  \
        const typeof(*(ptr))*: ((const type*)container_of(ptr, type, member)),                     \
        default: ((type*)container_of(ptr, type, member)))
