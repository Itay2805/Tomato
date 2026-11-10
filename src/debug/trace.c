#include "debug/trace.h"
#include "debug/debugcon.h"
#include "lib/snprintf.h"
#include "sync/spinlock.h"
#include <stdarg.h>

static irq_spinlock_t m_trace_lock = INIT_IRQ_SPINLOCK;

void _trace(const char* fmt, ...) {
    // format it on the stack for fun and profit
    char buffer[512];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    irq_spinlock_guard(&m_trace_lock);

    char* p = buffer;
    while (*p) {
        if (*p == '\t') {
            debugcon_putc(' ');
            debugcon_putc(' ');
            debugcon_putc(' ');
            debugcon_putc(' ');
        } else {
            debugcon_putc(*p);
        }
        p++;
    }
}
