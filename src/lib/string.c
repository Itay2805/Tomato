#include <stddef.h>

void* memcpy(void* dst, const void* src, size_t len) {
    size_t pos = 0;

    while (pos < len) {
        ((char*)dst)[pos] = ((const char*)src)[pos];
        pos++;
    }
    return dst;
}

void* memset(void* dst, int b, size_t len) {
    char* p = dst;

    while (len--) {
        *(p++) = b;
    }
    return dst;
}

void* memmove(void* dst, const void* src, size_t len) {
    size_t dir, pos;

    pos = len;
    dir = -1;

    if (dst < src) {
        pos = -1;
        dir = 1;
    }

    while (len) {
        pos += dir;
        ((char*)dst)[pos] = ((const char*)src)[pos];
        len--;
    }
    return dst;
}
