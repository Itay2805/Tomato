#pragma once

#include "cpp_magic.h"
#include "trace.h"
#include "util/defs.h"

#define ASSERT(expr, ...)                                                                          \
    do {                                                                                           \
        if (UNLIKELY(!(expr))) {                                                                   \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__));                                         \
            ERROR("Assertion `%s` failed at %s:%d", #expr, __FILE__, __LINE__);                    \
            for (;;)                                                                               \
                ;                                                                                  \
        }                                                                                          \
    } while (0)

#define ASSERT_SUCCESS(expr, ...) ASSERT(!IS_ERROR(expr), ##__VA_ARGS__)

#ifdef __DEBUG__
    #define DEBUG_ASSERT ASSERT
#else
    #define DEBUG_ASSERT(...)
#endif
