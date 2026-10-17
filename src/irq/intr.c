#include "intr.h"
#include "arch/gdt.h"
#include "arch/intr.h"
#include "arch/intrin.h"
#include "arch/msr.h"
#include "arch/virt.h"
#include "irq/irq.h"
#include "lib/assert.h"
#include "lib/trace.h"
#include "mem/virt.h"
#include "util/defs.h"
#include "util/except.h"
#include <stdint.h>

#define IDT_TYPE_TASK         0x5
#define IDT_TYPE_INTERRUPT_16 0x6
#define IDT_TYPE_TRAP_16      0x7
#define IDT_TYPE_INTERRUPT_32 0xE
#define IDT_TYPE_TRAP_32      0xF

typedef struct [[gnu::packed]] idt_entry {
    uint64_t handler_low : 16;
    uint64_t selector : 16;
    uint64_t ist : 3;
    uint64_t _zero1 : 5;
    uint64_t gate_type : 4;
    uint64_t _zero2 : 1;
    uint64_t ring : 2;
    uint64_t present : 1;
    uint64_t handler_high : 48;
    uint64_t _zero3 : 32;
} idt_entry_t;

typedef struct [[gnu::packed]] idt {
    uint16_t limit;
    idt_entry_t* base;
} idt_t;

/**
 * Swap the GS base if not coming from kernel, otherwise leave as is
 */
static void swapgs(interrupt_frame_t* frame) {
    if (frame->cs != GDT_KERNEL_CODE) {
        asm("swapgs");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////but
// Exception dumping
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef union [[gnu::packed]] selector_error_code {
    struct [[gnu::packed]] {
        uint32_t e : 1;
        uint32_t tbl : 2;
        uint32_t index : 13;
    };
    uint32_t packed;
} selector_error_code_t;

/**
 * Pretty print exception names
 */
static const char* const m_exception_names[] = {
    "#DE - Division Error",
    "#DB - Debug",
    "Non-maskable Interrupt",
    "#BP - Breakpoint",
    "#OF - Overflow",
    "#BR - Bound Range Exceeded",
    "#UD - Invalid Opcode",
    "#NM - Device Not Available",
    "#DF - Double Fault",
    "Coprocessor Segment Overrun",
    "#TS - Invalid TSS",
    "#NP - Segment Not Present",
    "#SS - Stack-Segment Fault",
    "#GP - General Protection Fault",
    "#PF - Page Fault",
    "Reserved",
    "#MF - x87 Floating-Point Exception",
    "#AC - Alignment Check",
    "#MC - Machine Check",
    "#XM/#XF - SIMD Floating-Point Exception",
    "#VE - Virtualization Exception",
    "#CP - Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "#HV - Hypervisor Injection Exception",
    "#VC - VMM Communication Exception",
    "#SX - Security Exception",
    "Reserved",
};

static const char* get_segment_name(uint64_t segment) {
    switch (segment & ~0b111) {
        case GDT_KERNEL_CODE:
            return "kernel-code";
        case GDT_USER_CODE:
            return "user-code";
        case GDT_KERNEL_DATA:
            return "kernel-data";
        case GDT_USER_DATA:
            return "user-data";
        default:
            return "<unknown>";
    }
}

static void panic_exception_handler(interrupt_frame_t* frame, uint64_t error, uint8_t exception) {
    irq_disable();

    // reset the spinlock so we can print
    ERROR("");
    ERROR("**************************************************************");
    ERROR("Exception occurred: %s (%u)", m_exception_names[exception], exception);
    ERROR("**************************************************************");
    ERROR("");

    if (exception == EXCEPT_IA32_PAGE_FAULT) {
        const char* prot = (error & IA32_PF_EC_PROT) ? "protection fault" : "no page found";
        const char* access = (error & IA32_PF_EC_WRITE) ? "write access" : "read access";
        const char* user = (error & IA32_PF_EC_USER) ? "user-mode access" : "kernel-mode access";
        ERROR("page fault: %s, %s, %s", prot, access, user);

        if (error & IA32_PF_EC_RSVD)
            ERROR("page fault: use of reserved bit detected");
        if (error & IA32_PF_EC_INSTR)
            ERROR("page fault: fault was an instruction fetch");
        if (error & IA32_PF_EC_PK)
            ERROR("page fault: protections keys block access");
        if (error & IA32_PF_EC_SHSTK)
            ERROR("page fault: shadow stack access fault");
        if (error & IA32_PF_EC_SGX)
            ERROR("page fault: SGX MMU page-fault");
        ERROR("");

    } else if (exception == EXCEPT_IA32_GP_FAULT && error != 0) {
        selector_error_code_t selector = (selector_error_code_t){ .packed = error };
        static const char* const table[] = { "GDT", "IDT", "LDT", "IDT" };
        ERROR("Accessing %s[%d]", table[selector.tbl], selector.index);
        ERROR("");
    }

    // now trace the entire state that we have
    ERROR("RIP=%016lx RSP=%016lx RFL=%08lx [%c%c%c%c%c%c%c]", frame->rip, frame->rsp, frame->rflags,
          frame->rflags & RFLAGS_DF ? 'D' : '-', frame->rflags & RFLAGS_OF ? 'O' : '-',
          frame->rflags & RFLAGS_SF ? 'S' : '-', frame->rflags & RFLAGS_ZF ? 'Z' : '-',
          frame->rflags & RFLAGS_AC ? 'A' : '-', frame->rflags & RFLAGS_PF ? 'P' : '-',
          frame->rflags & RFLAGS_CF ? 'C' : '-');
    ERROR("CS =%04lx DPL=%ld [%s]", frame->cs, frame->cs & 0b111, get_segment_name(frame->cs));
    ERROR("SS =%04lx DPL=%ld [%s]", frame->ss, frame->ss & 0b111, get_segment_name(frame->ss));
    ERROR("FS =%016lx", __rdmsr(MSR_IA32_FS_BASE));
    ERROR("GS =%016lx", (frame->cs == GDT_KERNEL_CODE) ? __rdmsr(MSR_IA32_GS_BASE)
                                                       : __rdmsr(MSR_IA32_KERNEL_GS_BASE));
    ERROR("CR0=%08lx CR2=%016lx CR3=%016lx CR4=%08lx", __readcr0(), __readcr2(), __readcr3(),
          __readcr4());
    ERROR("");

    // half and catch fire
    ASSERT(!"Got exception!");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////but
// Exception handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void common_exception_handler(interrupt_frame_t* frame, uint64_t error, uint8_t exception) {
    swapgs(frame);
    panic_exception_handler(frame, error, exception);
}

[[gnu::interrupt()]]
static void page_fault_handler(interrupt_frame_t* frame, uint64_t error_code) {
    swapgs(frame);
    void* fault_address = (void*)__readcr2();
    if (frame->rflags & RFLAGS_IF) {
        irq_enable();
    }

    if (UNLIKELY(IS_ERROR(virt_page_fault_handler(fault_address, error_code)))) {
        panic_exception_handler(frame, error_code, EXCEPT_IA32_PAGE_FAULT);
    }

    irq_disable();
    swapgs(frame);
}

#define EXCEPTION_STUB(num)                                                                        \
    [[gnu::interrupt()]]                                                                           \
    static void exception_handler_##num(interrupt_frame_t* frame) {                                \
        common_exception_handler(frame, 0, num);                                                   \
    }

#define EXCEPTION_ERROR_STUB(num)                                                                  \
    [[gnu::interrupt()]]                                                                           \
    static void exception_handler_##num(interrupt_frame_t* frame, uint64_t error) {                \
        common_exception_handler(frame, error, num);                                               \
    }

EXCEPTION_STUB(0x00);
EXCEPTION_STUB(0x01);
EXCEPTION_STUB(0x02);
EXCEPTION_STUB(0x03);
EXCEPTION_STUB(0x04);
EXCEPTION_STUB(0x05);
EXCEPTION_STUB(0x06);
EXCEPTION_STUB(0x07);
EXCEPTION_ERROR_STUB(0x08);
EXCEPTION_STUB(0x09);
EXCEPTION_ERROR_STUB(0x0A);
EXCEPTION_ERROR_STUB(0x0B);
EXCEPTION_ERROR_STUB(0x0C);
EXCEPTION_ERROR_STUB(0x0D);
EXCEPTION_STUB(0x0F);
EXCEPTION_STUB(0x10);
EXCEPTION_ERROR_STUB(0x11);
EXCEPTION_STUB(0x12);
EXCEPTION_STUB(0x13);
EXCEPTION_STUB(0x14);
EXCEPTION_ERROR_STUB(0x15);
EXCEPTION_STUB(0x16);
EXCEPTION_STUB(0x17);
EXCEPTION_STUB(0x18);
EXCEPTION_STUB(0x19);
EXCEPTION_STUB(0x1A);
EXCEPTION_STUB(0x1B);
EXCEPTION_STUB(0x1C);
EXCEPTION_ERROR_STUB(0x1D);
EXCEPTION_ERROR_STUB(0x1E);
EXCEPTION_STUB(0x1F);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////but
// IDT setup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * All interrupt handler entries
 */
LATE_RO static idt_entry_t m_idt_entries[256];

/**
 * Set a single idt entry
 */
INIT_CODE static void intr_set_handler_ist(uint8_t vector, void* func, tss_ist_t ist) {
    m_idt_entries[vector].handler_low = (uint16_t)((uintptr_t)func & 0xFFFF);
    m_idt_entries[vector].handler_high = (uint64_t)((uintptr_t)func >> 16);
    m_idt_entries[vector].gate_type = IDT_TYPE_INTERRUPT_32;
    m_idt_entries[vector].selector = GDT_KERNEL_CODE;
    m_idt_entries[vector].present = 1;
    m_idt_entries[vector].ist = ist;
}

INIT_CODE void intr_set_handler(uint8_t vector, void* func) {
    intr_set_handler_ist(vector, func, TSS_IST_NONE);
}

INIT_CODE void intr_init(void) {
    // set all of the exception handlers
    intr_set_handler(EXCEPT_IA32_DIVIDE_ERROR, exception_handler_0x00);
    intr_set_handler_ist(EXCEPT_IA32_DEBUG, exception_handler_0x01, TSS_IST_DB);
    intr_set_handler_ist(EXCEPT_IA32_NMI, exception_handler_0x02, TSS_IST_NMI);
    intr_set_handler(EXCEPT_IA32_BREAKPOINT, exception_handler_0x03);
    intr_set_handler(EXCEPT_IA32_OVERFLOW, exception_handler_0x04);
    intr_set_handler(EXCEPT_IA32_BOUND, exception_handler_0x05);
    intr_set_handler(EXCEPT_IA32_INVALID_OPCODE, exception_handler_0x06);
    intr_set_handler(0x07, exception_handler_0x07);
    intr_set_handler_ist(EXCEPT_IA32_DOUBLE_FAULT, exception_handler_0x08, TSS_IST_DF);
    intr_set_handler(0x09, exception_handler_0x09);
    intr_set_handler(EXCEPT_IA32_INVALID_TSS, exception_handler_0x0A);
    intr_set_handler(EXCEPT_IA32_SEG_NOT_PRESENT, exception_handler_0x0B);
    intr_set_handler(EXCEPT_IA32_STACK_FAULT, exception_handler_0x0C);
    intr_set_handler(EXCEPT_IA32_GP_FAULT, exception_handler_0x0D);
    intr_set_handler(EXCEPT_IA32_PAGE_FAULT, page_fault_handler);
    intr_set_handler(0x0F, exception_handler_0x0F);
    intr_set_handler(EXCEPT_IA32_FP_ERROR, exception_handler_0x10);
    intr_set_handler(EXCEPT_IA32_ALIGNMENT_CHECK, exception_handler_0x11);
    intr_set_handler_ist(EXCEPT_IA32_MACHINE_CHECK, exception_handler_0x12, TSS_IST_MCE);
    intr_set_handler(EXCEPT_IA32_SIMD, exception_handler_0x13);
    intr_set_handler(0x14, exception_handler_0x14);
    intr_set_handler_ist(EXCEPT_IA32_CONTROL_PROTECTION, exception_handler_0x15, TSS_IST_CP);
    intr_set_handler(0x16, exception_handler_0x16);
    intr_set_handler(0x17, exception_handler_0x17);
    intr_set_handler(0x18, exception_handler_0x18);
    intr_set_handler(0x19, exception_handler_0x19);
    intr_set_handler(0x1A, exception_handler_0x1A);
    intr_set_handler(0x1B, exception_handler_0x1B);
    intr_set_handler(0x1C, exception_handler_0x1C);
    intr_set_handler(0x1D, exception_handler_0x1D);
    intr_set_handler(0x1E, exception_handler_0x1E);
    intr_set_handler(0x1F, exception_handler_0x1F);

    // load it
    idt_t idt = {
        .limit = sizeof(m_idt_entries) - 1,
        .base = m_idt_entries,
    };
    asm volatile("lidt %0" : : "m"(idt));
}
