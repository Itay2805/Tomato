#pragma once

#include <stdint.h>

typedef enum tomato_err : uint32_t {
    TOMATO_SUCCESS,
    TOMATO_ERROR_INTERNAL_ERROR,
    TOMATO_ERROR_INVALID_ARGUMENT,
    TOMATO_ERROR_OUT_OF_RANGE,
    TOMATO_ERROR_OVERLAPS,
} tomato_err_t;
