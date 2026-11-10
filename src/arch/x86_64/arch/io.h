#pragma once

#include "arch/intrin.h"
#include <stdint.h>

/**
 * Devices can also be in the port IO space (the system IO address space of ACPI)
 */
#define ARCH_HAS_PORT_IO true

static inline uint8_t port_read8(uint16_t port) {
    return __inbyte(port);
}

static inline void port_write8(uint16_t port, uint8_t value) {
    __outbyte(port, value);
}
