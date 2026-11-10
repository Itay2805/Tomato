#pragma once

/**
 * The size of interrupt_frame_t, shared with the assembly
 */
#define INTERRUPT_FRAME_SIZE (16 * 18)

/**
 * The kinds of exceptions, the first 16 are in the order of the vector table
 */
#define EXCEPTION_KIND_CUR_SP0_SYNC   0
#define EXCEPTION_KIND_CUR_SP0_IRQ    1
#define EXCEPTION_KIND_CUR_SP0_FIQ    2
#define EXCEPTION_KIND_CUR_SP0_SERROR 3
#define EXCEPTION_KIND_CUR_SPX_SYNC   4
#define EXCEPTION_KIND_CUR_SPX_IRQ    5
#define EXCEPTION_KIND_CUR_SPX_FIQ    6
#define EXCEPTION_KIND_CUR_SPX_SERROR 7
#define EXCEPTION_KIND_LOW64_SYNC     8
#define EXCEPTION_KIND_LOW64_IRQ      9
#define EXCEPTION_KIND_LOW64_FIQ      10
#define EXCEPTION_KIND_LOW64_SERROR   11
#define EXCEPTION_KIND_LOW32_SYNC     12
#define EXCEPTION_KIND_LOW32_IRQ      13
#define EXCEPTION_KIND_LOW32_FIQ      14
#define EXCEPTION_KIND_LOW32_SERROR   15
#define EXCEPTION_KIND_STACK_OVERFLOW 16
#define EXCEPTION_KIND_MAX            17

#ifndef __ASSEMBLER__

    #include "util/defs.h"
    #include <stdint.h>

typedef struct interrupt_frame {
    // NOTE: must be in-sync with the assembly
    uint64_t x[31];
    uint64_t sp;
    uint64_t elr;
    uint64_t spsr;
    uint64_t esr;
    uint64_t far;
} interrupt_frame_t;
static_assert(sizeof(interrupt_frame_t) == INTERRUPT_FRAME_SIZE);

/**
 * Install the exception vectors on the current core, the exception
 * stack must already be setup
 */
INIT_CODE void intr_init(void);

#endif
