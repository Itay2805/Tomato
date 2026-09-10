#pragma once

#include <stdarg.h>
#include <stddef.h>

[[gnu::format(printf, 3, 0)]]
void kvsnprintf(char* buffer, size_t size, const char* format, va_list arguments);

[[gnu::format(printf, 3, 4)]]
void ksnprintf(char* buffer, size_t size, const char* format, ...);
