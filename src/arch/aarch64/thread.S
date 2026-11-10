.section .text

/*
 * Restore the callee saved registers from the stack, the layout
 * is thread_entry_frame_t
 */
.macro RESTORE_CALLEE_SAVED
    ldp x19, x20, [sp, #16 * 0]
    ldp x21, x22, [sp, #16 * 1]
    ldp x23, x24, [sp, #16 * 2]
    ldp x25, x26, [sp, #16 * 3]
    ldp x27, x28, [sp, #16 * 4]
    ldp x29, x30, [sp, #16 * 5]
    add sp, sp, #16 * 6
.endm

/**
 * void do_thread_switch(thread_t* to, thread_t* from);
 */
.global do_thread_switch
do_thread_switch:
    /*
     * save the current state, the frame pointer
     * and return address are on top for nice call frames
     */
    sub sp, sp, #16 * 6
    stp x19, x20, [sp, #16 * 0]
    stp x21, x22, [sp, #16 * 1]
    stp x23, x24, [sp, #16 * 2]
    stp x25, x26, [sp, #16 * 3]
    stp x27, x28, [sp, #16 * 4]
    stp x29, x30, [sp, #16 * 5]

    /*
     * save the current stack into the from thread
     */
    mov x9, sp
    str x9, [x1]

    /**
     * Finally switch to the new real stack
     */
    ldr x9, [x0]
    mov sp, x9

    /*
     * restore the register state
     */
    RESTORE_CALLEE_SAVED

    /* restore the ip */
    ret


/**
 * void thread_entry_thunk(void);
 */
.global thread_entry_thunk
thread_entry_thunk:
    mov x0, x19
    mov x1, x20
    b thread_entry_point

.section .init.text, "ax", %progbits

/**
 * void do_thread_bootstrap(thread_t* to);
 */
.global do_thread_bootstrap
do_thread_bootstrap:
    /*
     * restore the stack
     */
    ldr x9, [x0]
    mov sp, x9

    /*
     * restore the register state (emulates the
     * normal context switch behaviour)
     */
    RESTORE_CALLEE_SAVED

    /* finally restore the ip */
    ret
