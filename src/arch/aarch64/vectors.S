#include "arch/intr.h"

/*
 * The kernel runs with SP_EL0 selected (EL1t, or EL2t with VHE), which leaves SP_ELx
 * to be a per-cpu exception stack that is always valid when an exception is taken,
 * like the IST stacks on x86-64 but for all the exceptions.
 *
 * The exceptions we can handle (sync and irq from the kernel) move over to the stack
 * of the interrupted thread once we know the frame fits on it, so the handlers can
 * block or switch threads. Everything else is fatal and stays on the exception stack.
 */

/*
 * Save the interrupted state into the frame at sp, x0 and x1 must
 * still hold the interrupted values
 */
.macro SAVE_FRAME, use_sp_el0
    stp x0, x1, [sp, #16 * 0]
    stp x2, x3, [sp, #16 * 1]
    stp x4, x5, [sp, #16 * 2]
    stp x6, x7, [sp, #16 * 3]
    stp x8, x9, [sp, #16 * 4]
    stp x10, x11, [sp, #16 * 5]
    stp x12, x13, [sp, #16 * 6]
    stp x14, x15, [sp, #16 * 7]
    stp x16, x17, [sp, #16 * 8]
    stp x18, x19, [sp, #16 * 9]
    stp x20, x21, [sp, #16 * 10]
    stp x22, x23, [sp, #16 * 11]
    stp x24, x25, [sp, #16 * 12]
    stp x26, x27, [sp, #16 * 13]
    stp x28, x29, [sp, #16 * 14]

    /* the interrupted stack pointer */
.if \use_sp_el0
    mrs x0, sp_el0
.else
    add x0, sp, #INTERRUPT_FRAME_SIZE
.endif
    stp x30, x0, [sp, #16 * 15]

    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #16 * 16]
    mrs x0, esr_el1
    mrs x1, far_el1
    stp x0, x1, [sp, #16 * 17]
.endm

/*
 * Restore the interrupted state from the frame at sp and return to it
 */
.macro RESTORE_FRAME
    ldp x0, x1, [sp, #16 * 16]
    msr elr_el1, x0
    msr spsr_el1, x1

    ldp x0, x1, [sp, #16 * 0]
    ldp x2, x3, [sp, #16 * 1]
    ldp x4, x5, [sp, #16 * 2]
    ldp x6, x7, [sp, #16 * 3]
    ldp x8, x9, [sp, #16 * 4]
    ldp x10, x11, [sp, #16 * 5]
    ldp x12, x13, [sp, #16 * 6]
    ldp x14, x15, [sp, #16 * 7]
    ldp x16, x17, [sp, #16 * 8]
    ldp x18, x19, [sp, #16 * 9]
    ldp x20, x21, [sp, #16 * 10]
    ldp x22, x23, [sp, #16 * 11]
    ldp x24, x25, [sp, #16 * 12]
    ldp x26, x27, [sp, #16 * 13]
    ldp x28, x29, [sp, #16 * 14]
    ldr x30, [sp, #16 * 15]
    add sp, sp, #INTERRUPT_FRAME_SIZE

    eret
.endm

/*
 * An exception we can't recover from, report it from the exception stack
 */
.macro FATAL_ENTRY, kind, use_sp_el0
    sub sp, sp, #INTERRUPT_FRAME_SIZE
    SAVE_FRAME \use_sp_el0
    mov x0, sp
    mov x1, #\kind
    bl exception_fatal_handler
    b .
.endm

/*
 * An exception of the kernel we can handle, move to the thread stack and call the handler
 */
.macro THREAD_ENTRY, handler
    /*
     * We are still on the exception stack, make sure the thread stack is
     * aligned and has room for the frame
     */
    stp x0, x1, [sp, #-16]!
    mrs x0, sp_el0
    tst x0, #0xF
    b.ne 1f
    sub x0, x0, #INTERRUPT_FRAME_SIZE
    at s1e1w, x0
    isb
    mrs x1, par_el1
    tbnz x1, #0, 1f

    /*
     * It does, switch over to it, this leaves the exception stack empty
     */
    ldp x0, x1, [sp], #16
    msr spsel, #0
    sub sp, sp, #INTERRUPT_FRAME_SIZE
    SAVE_FRAME 0

    mov x0, sp
    bl \handler

    RESTORE_FRAME

1:
    /*
     * It doesn't, most likely the thread overflowed its stack
     */
    ldp x0, x1, [sp], #16
    FATAL_ENTRY EXCEPTION_KIND_STACK_OVERFLOW, 1
.endm

.section .text

/**
 * The exception vector table, each entry is 128 bytes
 */
.balign 2048
.global exception_vectors
exception_vectors:
    /* current EL with SP_EL0, this is where the kernel runs */
.balign 128
    b cur_sp0_sync
.balign 128
    b cur_sp0_irq
.balign 128
    b cur_sp0_fiq
.balign 128
    b cur_sp0_serror

    /* current EL with SP_ELx, nested exceptions while on the exception stack */
.balign 128
    b cur_spx_sync
.balign 128
    b cur_spx_irq
.balign 128
    b cur_spx_fiq
.balign 128
    b cur_spx_serror

    /* lower EL using AArch64, there is no usermode yet */
.balign 128
    b low64_sync
.balign 128
    b low64_irq
.balign 128
    b low64_fiq
.balign 128
    b low64_serror

    /* lower EL using AArch32, never supported */
.balign 128
    b low32_sync
.balign 128
    b low32_irq
.balign 128
    b low32_fiq
.balign 128
    b low32_serror

cur_sp0_sync:
    THREAD_ENTRY exception_sync_handler
cur_sp0_irq:
    THREAD_ENTRY exception_irq_handler
cur_sp0_fiq:
    FATAL_ENTRY EXCEPTION_KIND_CUR_SP0_FIQ, 1
cur_sp0_serror:
    FATAL_ENTRY EXCEPTION_KIND_CUR_SP0_SERROR, 1

cur_spx_sync:
    FATAL_ENTRY EXCEPTION_KIND_CUR_SPX_SYNC, 0
cur_spx_irq:
    FATAL_ENTRY EXCEPTION_KIND_CUR_SPX_IRQ, 0
cur_spx_fiq:
    FATAL_ENTRY EXCEPTION_KIND_CUR_SPX_FIQ, 0
cur_spx_serror:
    FATAL_ENTRY EXCEPTION_KIND_CUR_SPX_SERROR, 0

low64_sync:
    FATAL_ENTRY EXCEPTION_KIND_LOW64_SYNC, 1
low64_irq:
    FATAL_ENTRY EXCEPTION_KIND_LOW64_IRQ, 1
low64_fiq:
    FATAL_ENTRY EXCEPTION_KIND_LOW64_FIQ, 1
low64_serror:
    FATAL_ENTRY EXCEPTION_KIND_LOW64_SERROR, 1

low32_sync:
    FATAL_ENTRY EXCEPTION_KIND_LOW32_SYNC, 1
low32_irq:
    FATAL_ENTRY EXCEPTION_KIND_LOW32_IRQ, 1
low32_fiq:
    FATAL_ENTRY EXCEPTION_KIND_LOW32_FIQ, 1
low32_serror:
    FATAL_ENTRY EXCEPTION_KIND_LOW32_SERROR, 1
