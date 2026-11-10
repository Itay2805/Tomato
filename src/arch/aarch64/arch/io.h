#pragma once

#include <stdint.h>

/**
 * There is no port IO, devices are only ever memory mapped, so nothing may call these
 */
#define ARCH_HAS_PORT_IO false

static inline uint8_t port_read8(uint16_t port) {
    (void)port;
    __builtin_trap();
}

static inline void port_write8(uint16_t port, uint8_t value) {
    (void)port;
    (void)value;
    __builtin_trap();
}
