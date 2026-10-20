#pragma once

#include <stdint.h>

#define EXCEPT_IA32_DIVIDE_ERROR       0
#define EXCEPT_IA32_DEBUG              1
#define EXCEPT_IA32_NMI                2
#define EXCEPT_IA32_BREAKPOINT         3
#define EXCEPT_IA32_OVERFLOW           4
#define EXCEPT_IA32_BOUND              5
#define EXCEPT_IA32_INVALID_OPCODE     6
#define EXCEPT_IA32_DOUBLE_FAULT       8
#define EXCEPT_IA32_INVALID_TSS        10
#define EXCEPT_IA32_SEG_NOT_PRESENT    11
#define EXCEPT_IA32_STACK_FAULT        12
#define EXCEPT_IA32_GP_FAULT           13
#define EXCEPT_IA32_PAGE_FAULT         14
#define EXCEPT_IA32_FP_ERROR           16
#define EXCEPT_IA32_ALIGNMENT_CHECK    17
#define EXCEPT_IA32_MACHINE_CHECK      18
#define EXCEPT_IA32_SIMD               19
#define EXCEPT_IA32_CONTROL_PROTECTION 21

// #CP error-code "type" field (low 15 bits of the error code).
#define IA32_CP_EC_TYPE_MASK    0x7FFF
#define IA32_CP_EC_NEAR_RET     1
#define IA32_CP_EC_FAR_RET_IRET 2
#define IA32_CP_EC_ENDBRANCH    3
#define IA32_CP_EC_RSTORSSP     4
#define IA32_CP_EC_SETSSBSY     5
#define IA32_CP_EC_ENCL         BIT15

// reserved vectors
#define INTR_VECTOR_SPURIOUS 0xFF

typedef struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;
