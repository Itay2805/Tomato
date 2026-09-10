#include "trace.h"
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

    // TODO: something more generic/cross platform, how to abstract this?
    irq_state_t state = irq_spinlock_acquire(&m_trace_lock);
    char* p = buffer;
    while (*p) {
        if (*p == '\t') {
            __outbyte(0xE9, ' ');
            __outbyte(0xE9, ' ');
            __outbyte(0xE9, ' ');
            __outbyte(0xE9, ' ');
        } else {
            __outbyte(0xE9, *p);
        }
        p++;
    }
    irq_spinlock_release(&m_trace_lock, state);
}
