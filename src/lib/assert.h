#pragma once

#include "cpp_magic.h"
#include "defs.h"
#include "trace.h"

#define ASSERT(expr, ...)                                                                          \
    do {                                                                                           \
        if (UNLIKELY(!(expr))) {                                                                   \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__));                                         \
            ERROR("Assertion `%s` failed at %s:%d", #expr, __FILE__, __LINE__);                    \
            for (;;)                                                                               \
                ;                                                                                  \
        }                                                                                          \
    } while (0)
