.section .text

/**
 * void do_thread_switch(thread_t* to, thread_t* from);
 */
.global do_thread_switch
do_thread_switch:
    /*
     * save the current state, we save the rbp
     * first to have nice call frames
     */
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /*
     * save the current stack into the from thread
     */
    movq %rsp, (%rsi)

    /**
     * Finally switch to the new real stack
     */
    movq (%rdi), %rsp

    /*
     * restore the register state
     */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    /* restore the ip */
    ret


/**
 * void thread_entry_thunk(void);
 */
.global thread_entry_thunk
thread_entry_thunk:
    popq %rdi
    popq %rsi
    jmp thread_entry_point

.section .text.init

/**
 * void do_thread_bootstrap(thread_t* to);
 */
.global do_thread_bootstrap
do_thread_bootstrap:
    /*
     * restore the stack
     */
    movq (%rdi), %rsp

    /*
     * restore the register state (emulates the
     * normal context switch behaviour)
     */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    /* finally restore the ip */
    ret
