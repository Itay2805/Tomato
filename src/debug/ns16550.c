#include "arch/intrin.h"
#include "debug/uart.h"
#include "util/defs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// 16550 UART driver
////////////////////////////////////////////////////////////////////////////////////////////////////

#define NS16550_THR 0
#define NS16550_LCR 3
#define NS16550_LSR 5

#define NS16550_LCR_DLAB BIT7

#define NS16550_LSR_THRE BIT5

static void ns16550_putc(const uart_t* uart, char c) {
    while (!(uart_read(uart, NS16550_LSR) & NS16550_LSR_THRE)) {
        cpu_relax();
    }
    uart_write(uart, NS16550_THR, (uint8_t)c);
}

INIT_CODE void ns16550_init(uart_t* uart) {
    // the registers are spaced by the access size
    uart->reg_shift = __builtin_ctz(uart->access_size);

    // the divisor latch hides the transmit register, make sure it is not
    // selected, the firmware already configured the rest of the line
    uart_write(uart, NS16550_LCR, uart_read(uart, NS16550_LCR) & ~NS16550_LCR_DLAB);

    uart->putc = ns16550_putc;
}
