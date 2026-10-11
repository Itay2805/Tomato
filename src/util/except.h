#pragma once

#include "defs.h"
#include "lib/cpp_magic.h"
#include "lib/trace.h"
#include "tomato/err.h"

#include <stddefer.h>

typedef struct err {
    tomato_err_t code;
} err_t;

/**
 * The success value
 */
#define SUCCESS ((err_t){ .code = TOMATO_SUCCESS })

/**
 * Check if there was an error
 */
#define IS_ERROR(err) ((err).code != TOMATO_SUCCESS)

/**
 * Defer into an error path
 */
#define errdefer defer if (IS_ERROR(err))

/**
 * We use this as a way to detect if there is an `err_t err` declared
 * in the function or not
 */
struct rethrow_no_err_ {};
extern const struct rethrow_no_err_ err;

#define ERROR_VARIABLE                                                                             \
    err_t* err__ = _Generic(&err,                                                                  \
        err_t*: &err,                                  /* local err exists */                      \
        const struct rethrow_no_err_*: &(err_t){ 0 }); /* none */

/**
 * If the expression fails return an error
 */
#define CHECK_ERROR(expr, code, ...)                                                               \
    do {                                                                                           \
        if (UNLIKELY(!(expr))) {                                                                   \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__));                                         \
            ERROR("Check failed at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__);                 \
            ERROR_VARIABLE;                                                                        \
            *err__ = ((err_t){ TOMATO_ERROR_##code });                                             \
            return ((err_t){ TOMATO_ERROR_##code });                                               \
        }                                                                                          \
    } while (0)

#define CHECK(expr, ...)     CHECK_ERROR(expr, INTERNAL_ERROR, ##__VA_ARGS__)
#define CHECK_ARG(expr, ...) CHECK_ERROR(expr, INVALID_ARGUMENT, ##__VA_ARGS__)

#define RETHROW(expr)                                                                              \
    do {                                                                                           \
        err_t err___ = expr;                                                                       \
        if (UNLIKELY(IS_ERROR(err___))) {                                                          \
            ERROR("\trethrown at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__);                   \
            ERROR_VARIABLE;                                                                        \
            *err__ = err___;                                                                       \
            return err___;                                                                         \
        }                                                                                          \
    } while (0)

#define CHECK_FAIL(code, ...) CHECK_ERROR(0, code, ##__VA_ARGS__)
