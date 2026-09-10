#pragma once

#include "defs.h"
#include "lib/cpp_magic.h"
#include "lib/trace.h"
#include "tomato/err.h"
#include "util/defs.h"

typedef struct err {
    tomato_err_t code;
} err_t;

#define SUCCESS ((err_t){ .code = TOMATO_SUCCESS })

#define IS_ERROR(err) ((err).code != TOMATO_SUCCESS)

#define CHECK(expr, code, ...)                                                                     \
    do {                                                                                           \
        if (UNLIKELY(!(expr))) {                                                                   \
            err = ((err_t){ TOMATO_ERROR_##code });                                                \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__));                                         \
            ERROR("Check failed at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__);                 \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

#define RETHROW(expr)                                                                              \
    do {                                                                                           \
        err_t err__ = expr;                                                                        \
        if (UNLIKELY(IS_ERROR(err__))) {                                                           \
            err = err__;                                                                           \
            ERROR("\trethrown at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__);                   \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_FAIL(code, ...) CHECK(0, code, ##__VA_ARGS__)
