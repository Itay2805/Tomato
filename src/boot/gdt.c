#include "gdt.h"
#include "arch/gdt.h"
#include "arch/msr.h"
#include "lib/assert.h"
#include "lib/object.h"
#include "lib/snprintf.h"
#include "mem/phys.h"
#include "mem/stack.h"
#include "mem/vmar.h"
#include "sync/smp.h"
#include "sync/spinlock.h"
#include "util/defs.h"
#include <stddef.h>
#include <stdint.h>

typedef struct [[gnu::packed]] tss64 {
    uint32_t : 32;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t : 64;
    uint64_t ist[7];
    uint64_t : 64;
    uint16_t : 16;
    uint16_t iopb_offset;
} tss64_t;
static_assert(sizeof(tss64_t) == 104);

typedef struct [[gnu::packed]] gdt {
    uint16_t size;
    gdt_entries_t* entries;
} gdt_t;

/**
 * TSS allocation on initialization, the lock ensures we don't try to modify the
 * global's gdt's tss from multiple cpus at the same time
 */
static irq_spinlock_t m_tss_lock;

/**
 * The TSS of the current CPU
 */
CPU_LOCAL(static, tss64_t, m_tss);

static const char* const m_ist_name[] = {
    "ist-df-%zu", "ist-nmi-%zu", "ist-db-%zu", "ist-mce-%zu", "ist-cp-%zu",
};
static_assert(sizeof(m_ist_name) / sizeof(char*) == TSS_IST_MAX - 1);

static void tss_init() {
    // get the tss address
    tss64_t* tss = CPU_LOCAL_PTR(m_tss);
    tss->iopb_offset = sizeof(*tss);

    // allocate the special interrupt stacks
    for (int i = 0; i < TSS_IST_MAX - 1; i++) {
        stack_allocation_t alloc;
        ASSERT_SUCCESS(stack_alloc(&alloc, SIZE_4KB, false));
        vmar_set_name(alloc.stack_region, m_ist_name[i], g_cpu_id);

        // we will never free the stack, lock it
        vmar_lock(alloc.stack_region);

        // we no longer need the handle, put it
        OBJECT_PUT(alloc.stack_region);

        // and save it
        tss->ist[i] = (uintptr_t)alloc.stack_bottom;
    }

    // load the tss
    irq_spinlock_guard(&m_tss_lock);
    uintptr_t base = (uintptr_t)tss;
    uint32_t limit = sizeof(*tss);
    g_gdt_entries.tss = (tss64_entry_t){
        .length = limit & 0xFFFF,
        .low = base & 0xFFFF,
        .mid = (base >> 16) & 0xFF,
        .high = (base >> 24) & 0xFF,
        .upper32 = base >> 32,
        // ltr marks the descriptor as busy, so we set it back to available every time
        .flags1 = 0b10001001,
        .flags2 = (limit >> 16) & 0xF,
    };
    asm volatile("ltr %w0" : : "r"((uint16_t)GDT_TSS) : "memory");
}

void gdt_init(size_t cpu_id) {
    // load the gdt and all the segments
    gdt_t gdt = {
        .size = sizeof(gdt_entries_t) - 1,
        .entries = &g_gdt_entries,
    };
    asm volatile("lgdt %0\n"
                 "movq %%rsp, %%rax\n"
                 "pushq %2\n"
                 "pushq %%rax\n"
                 "pushfq\n"
                 "pushq %1\n"
                 "lea 1f(%%rip), %%rax\n"
                 "pushq %%rax\n"
                 "iretq\n"
                 "1:\n"
                 "movw %2, %%ax\n"
                 "movw %%ax, %%ds\n"
                 "movw %%ax, %%es\n"
                 "movw %%ax, %%fs\n"
                 "movw %%ax, %%gs\n"
                 :
                 : "m"(gdt), "i"(GDT_KERNEL_CODE), "i"(GDT_KERNEL_DATA)
                 : "memory", "rax");

    // Set the GS base to the be locals offset, for cpu 0 it is always zero
    // so use that (for easier init sequence)
    ptrdiff_t off = 0;
    if (cpu_id != 0) {
        off = g_cpu_locals_offsets[cpu_id];
    }
    __wrmsr(MSR_IA32_GS_BASE, off);

    // load the current tss
    tss_init();
}
