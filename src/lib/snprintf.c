#include "snprintf.h"

#include "debug/debug.h"
#include "string.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

typedef struct buf {
    char* buf;
    size_t size;
    size_t len;
} buf_t;

static void buf_put(buf_t* o, char c) {
    if (o->len + 1 < o->size) {
        o->buf[o->len] = c;
    }
    o->len++;
}

static void buf_puts(buf_t* o, const char* s) {
    while (*s) {
        buf_put(o, *s++);
    }
}

static void buf_putnum(buf_t* o, uint64_t v, unsigned base) {
    char tmp[20];
    char* p = tmp + sizeof(tmp);

    do {
        *--p = "0123456789abcdef"[v % base];
        v /= base;
    } while (v);

    while (p < tmp + sizeof(tmp)) {
        buf_put(o, *p++);
    }
}

int kvsnprintf(char* buffer, size_t size, const char* format, va_list args) {
    buf_t o = { buffer, size, 0 };

    for (; *format; format++) {
        if (*format != '%') {
            buf_put(&o, *format);
            continue;
        }

        int wide = 0;
        while (format[1] == 'l' || format[1] == 'z') {
            wide = 1;
            format++;
        }

        switch (*++format) {
            case 'c': {
                buf_put(&o, (char)va_arg(args, int));
            } break;

            case 's': {
                const char* s = va_arg(args, const char*);
                buf_puts(&o, s ? s : "(null)");
            } break;

            case 'd': {
                int64_t v = wide ? va_arg(args, int64_t) : va_arg(args, int);
                uint64_t u = (uint64_t)v;
                if (v < 0) {
                    buf_put(&o, '-');
                    u = -u; /* unsigned negate: INT64_MIN is fine */
                }
                buf_putnum(&o, u, 10);
            } break;

            case 'u': {
                buf_putnum(&o, wide ? va_arg(args, uint64_t) : va_arg(args, unsigned), 10);
            } break;

            case 'x': {
                buf_putnum(&o, wide ? va_arg(args, uint64_t) : va_arg(args, unsigned), 16);
            } break;

            case '%': {
                buf_put(&o, '%');
            } break;

            case '\0':
                goto done;

            default:
                buf_put(&o, '%');
                buf_put(&o, *format);
                break;
        }
    }

done:
    // ensure it ends with null terminator
    if (size) {
        buffer[o.len < size ? o.len : size - 1] = '\0';
    }

    return (int)o.len;
}

int ksnprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = kvsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}
