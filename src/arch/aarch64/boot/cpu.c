#include "cpu.h"
#include "arch/intrin.h"
#include "lib/assert.h"
#include "lib/object.h"
#include "mem/stack.h"
#include "mem/vmar.h"
#include "sync/smp.h"
#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Allocate the exception stack of the current core, and switch to running the kernel
 * on SP_EL0 so SP_ELx is always the exception stack (see vectors.S)
 */
static void exception_stack_init(void) {
    stack_allocation_t alloc;
    ASSERT_SUCCESS(stack_alloc(&alloc, SIZE_4KB, false));
    vmar_set_name(alloc.stack_region, "exception-stack-%zu", CPU_LOCAL(cpu_id));

    // we will never free the stack, lock it
    vmar_lock(alloc.stack_region);

    // we no longer need the handle, put it
    OBJECT_PUT(alloc.stack_region);

    // keep running on the current stack through SP_EL0, whichever one
    // we were on, and then load the exception stack into SP_ELx
    asm volatile("mov x9, sp\n"
                 "msr spsel, #0\n"
                 "mov sp, x9\n"
                 "msr spsel, #1\n"
                 "mov sp, %0\n"
                 "msr spsel, #0\n"
                 :
                 : "r"(alloc.stack_bottom)
                 : "x9", "memory");
}

void cpu_init(size_t cpu_id) {
    // Set TPIDR_EL1 to the locals offset, for cpu 0 it is always zero
    // so use that (for easier init sequence)
    ptrdiff_t off = 0;
    if (cpu_id != 0) {
        off = g_cpu_locals_offsets[cpu_id];
    }
    __write_sysreg(tpidr_el1, off);

    // setup the stack for the exceptions
    exception_stack_init();
}
