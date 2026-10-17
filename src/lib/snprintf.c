#include "snprintf.h"

#include "mem/phys.h"
#include "string.h"
#include <stddef.h>
#include <stdint.h>

static size_t strlen(const char* str) {
    size_t len;
    for (len = 0; str[len]; len++)
        ;
    return len;
}

static int u64toa_r(uint64_t in, char* buffer) {
    int digits = 0;

    int pos = 19;
    do {
        int dig = 0;
        unsigned long long lim = 0;
        for (dig = 0, lim = 1; dig < pos; dig++) {
            lim *= 10;
        }

        if (digits || in >= lim || !pos) {
            for (dig = 0; in >= lim; dig++) {
                in -= lim;
            }

            buffer[digits++] = '0' + dig;
        }
    } while (pos--);

    buffer[digits] = 0;
    return digits;
}

static int i64toa_r(int64_t in, char* buffer) {
    char* ptr = buffer;
    int len = 0;

    if (in < 0) {
        in = -(uint64_t)in;
        *(ptr++) = '-';
        len++;
    }
    len += u64toa_r(in, ptr);
    return len;
}

static int u64toh_r(uint64_t in, char* buffer) {
    signed char pos = 60;

    int digits = 0;
    do {
        int dig = (in >> pos) & 0xF;
        if (dig > 9)
            dig += 'a' - '0' - 10;
        pos -= 4;
        if (dig || digits || pos < 0) {
            buffer[digits++] = '0' + dig;
        }
    } while (pos >= 0);

    buffer[digits] = 0;
    return digits;
}

void kvsnprintf(char* out, size_t n, const char* fmt, va_list args) {
    size_t size = n;
    char* buf = out;

    char c;
    unsigned long long v;
    size_t len;
    char tmpbuf[21];
    const char* outstr;

    size_t offset = 0;
    size_t lpref = 0;
    size_t written = 0;
    bool escape = false;

    while (1) {
        size_t width = 0;
        char pad = ' ';

        c = fmt[offset++];

        if (escape) {
            // we're in an escape sequence, offset == 1
            escape = false;

            // pad with zero instead of space
            if (c == '0') {
                pad = '0';
                c = fmt[offset++];
            }

            // width
            while (c >= '0' && c <= '9') {
                width *= 10;
                width += c - '0';

                c = fmt[offset++];
            }

            // modifiers or final 0
            while (c == 'l') {
                lpref++;
                c = fmt[offset++];
            }

            // size_t is a long, so z is the same as a single l
            static_assert(sizeof(size_t) == sizeof(long));
            if (c == 'z' && lpref == 0) {
                lpref = 1;
                c = fmt[offset++];
            }

            if (c == 'c' || c == 'd' || c == 'u' || c == 'x' || c == 'p') {
                char* out = tmpbuf;

                if (c == 'p') {
                    v = va_arg(args, unsigned long);
                } else if (lpref) {
                    if (lpref > 1) {
                        v = va_arg(args, unsigned long long);
                    } else {
                        v = va_arg(args, unsigned long);
                    }
                } else {
                    v = va_arg(args, unsigned int);
                }

                if (c == 'd') {
                    // sign-extend the value
                    if (lpref == 0) {
                        v = (long long)(int)v;
                    } else if (lpref == 1) {
                        v = (long long)(long)v;
                    }
                }

                switch (c) {
                    case 'c':
                        out[0] = v;
                        out[1] = 0;
                        break;
                    case 'd':
                        i64toa_r(v, out);
                        break;
                    case 'u':
                        u64toa_r(v, out);
                        break;
                    case 'p':
                        *(out++) = '0';
                        *(out++) = 'x';
                    default: // 'x' and 'p' above
                        u64toh_r(v, out);
                        break;
                }
                outstr = tmpbuf;

            } else if (c == 's') {
                outstr = va_arg(args, char*);
                if (outstr == nullptr) {
                    outstr = "(null)";
                }

            } else if (c == '%') {
                // queue it verbatim
                continue;

            } else {
                // unknown conversion, print it verbatim instead of eating the text
                // after it, the % is right before the escape sequence
                outstr = fmt - 1;
                len = c == 0 ? offset : offset + 1;
                width = 0;
                goto flush_str;
            }

            len = strlen(outstr);
            goto flush_str;
        }

        // not an escape sequence
        if (c == 0 || c == '%') {
            // flush pending data on escape or end
            escape = true;
            lpref = 0;
            outstr = fmt;
            len = offset - 1;

        flush_str:
            // pad up to the width and then copy the string, whatever doesn't
            // fit in the buffer is dropped but still counted in written
            size_t pad_len = width > len ? width - len : 0;
            size_t p = pad_len < n ? pad_len : n;
            memset(out, pad, p);
            out += p;
            n -= p;

            size_t w = len < n ? len : n;
            memcpy(out, outstr, w);
            out += w;
            n -= w;

            written += pad_len + len;

            if (c == 0)
                break;

            fmt += offset;
            offset = 0;
            continue;
        }

        // literal char, just queue it
    }

    // ensure it always ends with a null terminator
    if (size != 0) {
        buf[written < size ? written : size - 1] = '\0';
    }
}

void ksnprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    kvsnprintf(buffer, size, format, args);
    va_end(args);
}
