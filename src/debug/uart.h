#pragma once

#include "arch/io.h"
#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

/**
 * A UART we can transmit on, the firmware is expected to have already
 * configured the line (baud rate and so on)
 */
typedef struct uart {
    /**
     * Transmit a single character, waits until there is room for it
     */
    void (*putc)(const struct uart* uart, char c);

    /**
     * Where the registers are, either a port or a virtual address
     */
    uintptr_t base;
    bool port_io;

    /**
     * The size of the register accesses in bytes, and the shift from
     * the register to its offset from the base
     */
    uint8_t access_size;
    uint8_t reg_shift;
} uart_t;

static inline uint32_t uart_read(const uart_t* uart, size_t reg) {
    uintptr_t addr = uart->base + (reg << uart->reg_shift);
    if (ARCH_HAS_PORT_IO && uart->port_io) {
        return port_read8(addr);
    }

    switch (uart->access_size) {
        case 1:
            return *(volatile uint8_t*)addr;
        case 2:
            return *(volatile uint16_t*)addr;
        default:
            return *(volatile uint32_t*)addr;
    }
}

static inline void uart_write(const uart_t* uart, size_t reg, uint32_t value) {
    uintptr_t addr = uart->base + (reg << uart->reg_shift);
    if (ARCH_HAS_PORT_IO && uart->port_io) {
        port_write8(addr, value);
        return;
    }

    switch (uart->access_size) {
        case 1:
            *(volatile uint8_t*)addr = value;
            break;
        case 2:
            *(volatile uint16_t*)addr = value;
            break;
        default:
            *(volatile uint32_t*)addr = value;
            break;
    }
}

/**
 * Setup a UART whose registers are already set, and set its putc
 */
INIT_CODE void ns16550_init(uart_t* uart);
INIT_CODE void pl011_init(uart_t* uart);

/**
 * The SBSA generic UART is a subset of the PL011
 */
INIT_CODE void sbsa_uart_init(uart_t* uart);
