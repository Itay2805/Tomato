#pragma once

#include <stdarg.h>
#include <stddef.h>

/**
 * A small, allocation-free formatter for kernel and embedded use.
 *
 * Supported conversions are %% %c %s %d %i %u %x %X and %p. Integer
 * conversions accept an optional `l`, a field width, and zero padding.
 * Everything else, including floating point and precision, is omitted.
 *
 * The return value is the number of characters that would have been written,
 * excluding the terminator. The output is always terminated when size is not
 * zero. A null buffer is valid only when size is zero. Results longer than
 * INT_MAX return -1.
 */
[[gnu::format(printf, 3, 0)]]
int kvsnprintf(char* buffer, size_t size, const char* format, va_list arguments);

[[gnu::format(printf, 3, 4)]]
int ksnprintf(char* buffer, size_t size, const char* format, ...);
