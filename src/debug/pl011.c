#include "arch/intrin.h"
#include "debug/uart.h"
#include "util/defs.h"
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// PL011 UART driver
////////////////////////////////////////////////////////////////////////////////////////////////////

#define PL011_DR 0x000
#define PL011_FR 0x018
#define PL011_CR 0x030

#define PL011_FR_TXFF BIT5

#define PL011_CR_UARTEN BIT0
#define PL011_CR_TXE    BIT8

static void pl011_putc(const uart_t* uart, char c) {
    while (uart_read(uart, PL011_FR) & PL011_FR_TXFF) {
        cpu_relax();
    }
    uart_write(uart, PL011_DR, (uint8_t)c);
}

INIT_CODE static void pl011_init_common(uart_t* uart) {
    // the registers are at their byte offsets, and the
    // SBSA UART only allows accessing them as 32 bits
    uart->access_size = sizeof(uint32_t);
    uart->reg_shift = 0;
    uart->putc = pl011_putc;
}

INIT_CODE void pl011_init(uart_t* uart) {
    pl011_init_common(uart);

    // make sure we can transmit, the firmware already configured the rest
    uart_write(uart, PL011_CR, uart_read(uart, PL011_CR) | PL011_CR_UARTEN | PL011_CR_TXE);
}

INIT_CODE void sbsa_uart_init(uart_t* uart) {
    // there is no control register, the firmware enabled it
    pl011_init_common(uart);
}
