#include "thread.h"

#include "arch/virt.h"
#include "irq/irq.h"
#include "lib/assert.h"
#include "lib/object.h"
#include "lib/snprintf.h"
#include "lib/trace.h"
#include "mem/alloc.h"
#include "mem/stack.h"
#include "mem/vmar.h"
#include "sched.h"
#include "thread/runq.h"
#include <stdarg.h>
#include <stdatomic.h>

/**
 * The saved state when switching between threads
 */
typedef struct [[gnu::packed]] thread_entry_frame {
    // NOTE: must be in-sync with the assembly
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;

    // NOTE: must be in sync with thread_entry_thunk
    uint64_t rdi;
    uint64_t rsi;
} thread_entry_frame_t;

DEFINE_ALLOC(thread_t);

err_t thread_create(thread_t** out, thread_entry_t entry, void* arg, const char* name_fmt, ...) {
    err_t err = SUCCESS;

    // create and setup the thread object
    thread_t* thread = ALLOC(thread_t);
    errdefer FREE(thread_t, thread);
    thread->object.kind = OBJECT_KIND_THREAD;
    thread->object.ref_count = 1;
    thread->state = THREAD_STATE_IDLE;
    va_list args;
    va_start(args, name_fmt);
    kvsnprintf(thread->object.name, sizeof(thread->object.name), name_fmt, args);
    va_end(args);

    // allocate the kernel stack
    stack_allocation_t kernel_stack = {};
    RETHROW(stack_alloc(&kernel_stack, PAGE_SIZE, false));
    errdefer {
        vmar_unmap(kernel_stack.stack_region);
        OBJECT_PUT(kernel_stack.stack_region);
    }
    thread->kernel_stack = kernel_stack.stack_region;

    // setup the initial kernel stack
    void* rsp = kernel_stack.stack_bottom;
    rsp -= sizeof(thread_entry_frame_t);
    thread_entry_frame_t* entry_frame = rsp;
    thread->kernel_rsp = rsp;

    // setup the entry frame
    entry_frame->rip = (uintptr_t)thread_entry_thunk;
    entry_frame->rdi = (uintptr_t)entry;
    entry_frame->rsi = (uintptr_t)arg;

    // we can return the thread
    *out = thread;

    return SUCCESS;
}

void thread_start(thread_t* thread) {
    ASSERT(atomic_exchange(&thread->state, THREAD_STATE_READY) == THREAD_STATE_IDLE);
    run_queue_put(&thread->rqe);
}

noreturn void thread_exit(void) {
    ASSERT(atomic_exchange(&g_current_thread->state, THREAD_STATE_DEAD) == THREAD_STATE_RUNNING);

    irq_disable();
    scheduler_schedule();
    __builtin_unreachable();
}

void thread_yield(void) {
    DEBUG_ASSERT(irq_enabled());

    irq_disable();
    scheduler_schedule();
    irq_enable();
}

void thread_free(thread_t* thread) {
    vmar_unmap(thread->kernel_stack);
    FREE(thread_t, thread);
}

void do_thread_switch(thread_t* to, thread_t* from);

void thread_switch(thread_t* to, thread_t* from) {
    do_thread_switch(to, from);
}

INIT_CODE noreturn void do_thread_bootstrap(thread_t* to);

INIT_CODE noreturn void thread_bootstrap(thread_t* thread) {
    // mark the thread as running
    atomic_store_relaxed(&thread->state, THREAD_STATE_RUNNING);

    // set it as the current thread and jump into it
    g_current_thread = thread;
    do_thread_bootstrap(thread);
}
