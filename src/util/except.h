#pragma once

#include "defs.h"
#include "lib/cpp_magic.h"
#include "lib/trace.h"
#include "tomato/err.h"
#include "util/defs.h"

#include <stddefer.h>

typedef struct err {
    tomato_err_t code;
} err_t;

#define SUCCESS ((err_t){ .code = TOMATO_SUCCESS })

#define IS_ERROR(err) ((err).code != TOMATO_SUCCESS)

#define CHECK_ERROR(expr, code, ...)                                                               \
    do {                                                                                           \
        if (UNLIKELY(!(expr))) {                                                                   \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__));                                         \
            ERROR("Check failed at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__);                 \
            return ((err_t){ TOMATO_ERROR_##code });                                               \
        }                                                                                          \
    } while (0)

#define CHECK(expr, ...)     CHECK_ERROR(expr, INTERNAL_ERROR, ##__VA_ARGS__)
#define CHECK_ARG(expr, ...) CHECK_ERROR(expr, INVALID_ARGUMENT, ##__VA_ARGS__)

#define RETHROW(expr)                                                                              \
    do {                                                                                           \
        err_t err__ = expr;                                                                        \
        if (UNLIKELY(IS_ERROR(err__))) {                                                           \
            ERROR("\trethrown at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__);                   \
            return err__;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_FAIL(code, ...) CHECK(0, code, ##__VA_ARGS__)
