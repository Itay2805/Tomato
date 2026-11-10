#include "arch/intr.h"
#include "arch/gic.h"
#include "arch/intrin.h"
#include "arch/sysreg.h"
#include "debug/trace.h"
#include "irq/irq.h"
#include "lib/assert.h"
#include "mem/virt.h"
#include "util/defs.h"
#include "util/except.h"
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Exception dumping
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Pretty print exception kinds
 */
static const char* const m_exception_kinds[] = {
    "Synchronous (current EL, SP0)",   "IRQ (current EL, SP0)",
    "FIQ (current EL, SP0)",           "SError (current EL, SP0)",
    "Synchronous (current EL, SPx)",   "IRQ (current EL, SPx)",
    "FIQ (current EL, SPx)",           "SError (current EL, SPx)",
    "Synchronous (lower EL, AArch64)", "IRQ (lower EL, AArch64)",
    "FIQ (lower EL, AArch64)",         "SError (lower EL, AArch64)",
    "Synchronous (lower EL, AArch32)", "IRQ (lower EL, AArch32)",
    "FIQ (lower EL, AArch32)",         "SError (lower EL, AArch32)",
    "Kernel stack overflow",
};
static_assert(sizeof(m_exception_kinds) / sizeof(char*) == EXCEPTION_KIND_MAX);

static const char* get_exception_class_name(uint64_t ec) {
    switch (ec) {
        case ESR_EC_UNKNOWN:
            return "Unknown reason";
        case ESR_EC_WFX:
            return "Trapped WFI/WFE";
        case ESR_EC_FP_ASIMD:
            return "Trapped FP/SIMD access";
        case ESR_EC_ILLEGAL_STATE:
            return "Illegal execution state";
        case ESR_EC_SVC64:
            return "SVC";
        case ESR_EC_SYS64:
            return "Trapped system register access";
        case ESR_EC_SVE:
            return "Trapped SVE access";
        case ESR_EC_IABT_LOW:
        case ESR_EC_IABT_CUR:
            return "Instruction Abort";
        case ESR_EC_PC_ALIGN:
            return "PC Alignment Fault";
        case ESR_EC_DABT_LOW:
        case ESR_EC_DABT_CUR:
            return "Data Abort";
        case ESR_EC_SP_ALIGN:
            return "SP Alignment Fault";
        case ESR_EC_FP_EXC64:
            return "Floating-Point Exception";
        case ESR_EC_SERROR:
            return "SError";
        case ESR_EC_BREAKPT_LOW:
        case ESR_EC_BREAKPT_CUR:
            return "Breakpoint";
        case ESR_EC_SOFTSTP_LOW:
        case ESR_EC_SOFTSTP_CUR:
            return "Software Step";
        case ESR_EC_WATCHPT_LOW:
        case ESR_EC_WATCHPT_CUR:
            return "Watchpoint";
        case ESR_EC_BRK64:
            return "BRK instruction";
        default:
            return "<unknown>";
    }
}

static const char* get_fault_status_name(uint64_t fsc) {
    switch (ESR_FSC_TYPE(fsc)) {
        case ESR_FSC_TRANSLATION:
            return "no page found";
        case ESR_FSC_ACCESS_FLAG:
            return "access flag fault";
        case ESR_FSC_PERMISSION:
            return "protection fault";
    }

    switch (fsc) {
        case ESR_FSC_ALIGNMENT:
            return "alignment fault";
        case ESR_FSC_EXTERNAL:
            return "synchronous external abort";
        case ESR_FSC_TLB_CONFLICT:
            return "tlb conflict abort";
        default:
            return "<unknown>";
    }
}

static bool is_abort(uint64_t ec) {
    return ec == ESR_EC_IABT_LOW || ec == ESR_EC_IABT_CUR || ec == ESR_EC_DABT_LOW ||
           ec == ESR_EC_DABT_CUR;
}

static void panic_exception_handler(interrupt_frame_t* frame, uint64_t kind) {
    irq_disable();

    uint64_t ec = ESR_EC(frame->esr);
    uint64_t iss = ESR_ISS(frame->esr);

    ERROR("");
    ERROR("**************************************************************");
    ERROR("Exception occurred: %s (%lu)", m_exception_kinds[kind], kind);
    ERROR("Exception class: %s (%lx)", get_exception_class_name(ec), ec);
    ERROR("**************************************************************");
    ERROR("");

    if (kind == EXCEPTION_KIND_STACK_OVERFLOW) {
        ERROR("stack overflow: no room for the exception frame on the thread stack");
        ERROR("");

    } else if (is_abort(ec)) {
        uint64_t fsc = iss & ESR_ISS_ABT_FSC_MASK;
        const char* access = "read access";
        if (ec == ESR_EC_IABT_LOW || ec == ESR_EC_IABT_CUR) {
            access = "instruction fetch";
        } else if (iss & ESR_ISS_ABT_WNR) {
            access = "write access";
        }
        const char* user = (ec == ESR_EC_IABT_LOW || ec == ESR_EC_DABT_LOW) ? "user-mode access"
                                                                            : "kernel-mode access";
        ERROR("page fault: %s (level %lu), %s, %s", get_fault_status_name(fsc), ESR_FSC_LEVEL(fsc),
              access, user);

        if (iss & ESR_ISS_ABT_FNV)
            ERROR("page fault: the fault address is not valid");
        ERROR("");
    }

    // now trace the entire state that we have
    uint64_t spsr = frame->spsr;
    ERROR("PC =%016lx SP =%016lx PSTATE=%08lx [%c%c%c%c %c%c%c%c] EL%lu%c", frame->elr, frame->sp,
          spsr, spsr & SPSR_N ? 'N' : '-', spsr & SPSR_Z ? 'Z' : '-', spsr & SPSR_C ? 'C' : '-',
          spsr & SPSR_V ? 'V' : '-', spsr & SPSR_D ? 'D' : '-', spsr & SPSR_A ? 'A' : '-',
          spsr & SPSR_I ? 'I' : '-', spsr & SPSR_F ? 'F' : '-', (spsr & SPSR_M_MASK) >> 2,
          spsr & SPSR_M_SP ? 'h' : 't');
    ERROR("ESR=%016lx FAR=%016lx", frame->esr, frame->far);
    for (int i = 0; i < 28; i += 4) {
        ERROR("X%d%s=%016lx X%d%s=%016lx X%d%s=%016lx X%d%s=%016lx", i, i < 10 ? " " : "",
              frame->x[i], i + 1, i + 1 < 10 ? " " : "", frame->x[i + 1], i + 2,
              i + 2 < 10 ? " " : "", frame->x[i + 2], i + 3, i + 3 < 10 ? " " : "",
              frame->x[i + 3]);
    }
    ERROR("X28=%016lx X29=%016lx X30=%016lx", frame->x[28], frame->x[29], frame->x[30]);
    ERROR("SCTLR=%016lx TCR=%016lx TPIDR=%016lx", __read_sysreg(sctlr_el1), __read_sysreg(tcr_el1),
          __read_sysreg(tpidr_el1));
    ERROR("TTBR0=%016lx TTBR1=%016lx", __read_sysreg(ttbr0_el1), __read_sysreg(ttbr1_el1));
    ERROR("");

    // half and catch fire
    ASSERT(!"Got exception!");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Exception handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void page_fault_handler(interrupt_frame_t* frame) {
    uint64_t ec = ESR_EC(frame->esr);
    uint64_t iss = ESR_ISS(frame->esr);
    uint64_t fsc = iss & ESR_ISS_ABT_FSC_MASK;

    // only these can popup in normal operation, if anything
    // else happened then we should really panic
    virt_fault_flags_t flags = 0;
    switch (ESR_FSC_TYPE(fsc)) {
        case ESR_FSC_TRANSLATION:
            break;

        case ESR_FSC_ACCESS_FLAG:
        case ESR_FSC_PERMISSION:
            flags |= VIRT_FAULT_PRESENT;
            break;

        default:
            panic_exception_handler(frame, EXCEPTION_KIND_CUR_SP0_SYNC);
    }

    if (UNLIKELY(iss & ESR_ISS_ABT_FNV)) {
        panic_exception_handler(frame, EXCEPTION_KIND_CUR_SP0_SYNC);
    }

    if (ec == ESR_EC_IABT_CUR) {
        flags |= VIRT_FAULT_INSTR;
    } else if ((iss & ESR_ISS_ABT_WNR) && !(iss & ESR_ISS_ABT_CM)) {
        // cache maintenance reports as a write, but it only needs a read
        flags |= VIRT_FAULT_WRITE;
    }

    if (!(frame->spsr & SPSR_I)) {
        irq_enable();
    }

    if (UNLIKELY(IS_ERROR(virt_page_fault_handler((void*)frame->far, flags)))) {
        panic_exception_handler(frame, EXCEPTION_KIND_CUR_SP0_SYNC);
    }

    irq_disable();
}

/**
 * Synchronous exceptions of the kernel, called on the thread stack by the vectors
 */
void exception_sync_handler(interrupt_frame_t* frame) {
    switch (ESR_EC(frame->esr)) {
        case ESR_EC_IABT_CUR:
        case ESR_EC_DABT_CUR:
            page_fault_handler(frame);
            break;

        default:
            panic_exception_handler(frame, EXCEPTION_KIND_CUR_SP0_SYNC);
    }
}

/**
 * Interrupts of the kernel, called on the thread stack by the vectors
 */
void exception_irq_handler(interrupt_frame_t* frame) {
    (void)frame;
    gic_handle_irq();
}

/**
 * Exceptions we can't recover from, called on the exception stack by the vectors
 */
[[noreturn]]
void exception_fatal_handler(interrupt_frame_t* frame, uint64_t kind) {
    panic_exception_handler(frame, kind);
    __builtin_unreachable();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vector setup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern char exception_vectors[];

INIT_CODE void intr_init(void) {
    __write_sysreg(vbar_el1, exception_vectors);
    __isb();

    // SErrors are always fatal, let them in now that we can report them
    asm volatile("msr daifclr, #4" ::: "memory");
}
