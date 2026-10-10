#include "gdt.h"
#include "arch/gdt.h"
#include "lib/assert.h"
#include "mem/phys.h"
#include "sync/smp.h"
#include "sync/spinlock.h"
#include "util/defs.h"

/**
 * Every TSS gets its own cacheline, rsp0 is written on every context
 * switch and we don't want the cores to fight over the same line
 */
#define TSS_STRIDE ALIGN_UP(sizeof(tss64_t), 64)

/**
 * A bit for every port, plus a byte with all bits set at the end because
 * the cpu always reads two bytes of the bitmap when checking a port
 */
#define IOPB_SIZE (SIZE_64KB / 8 + 1)

/**
 * The iopb is at a 16 bit offset from the TSS, so all the TSSs that
 * share an iopb must be in the 64kb that come right before it
 */
#define TSS_PER_IOPB_MAX (UINT16_MAX / TSS_STRIDE)

// the descriptor limit has to cover the iopb, and it only has 20 bits
static_assert(UINT16_MAX + IOPB_SIZE - 1 <= 0xFFFFF);

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
 * All the TSSs are in one allocation, split into groups where each
 * group is a run of TSSs followed by the iopb they share
 */
static void* m_tss_base = nullptr;
static size_t m_tss_per_group = 0;
static size_t m_tss_group_size = 0;

/**
 * TSS allocation on initialization, the lock ensures we don't try to modify the
 * global's gdt's tss from multiple cpus at the same time
 */
static irq_spinlock_t m_tss_lock;
static size_t m_tss_next = 0;

static tss64_t* tss_get(size_t index) {
    size_t group = index / m_tss_per_group;
    size_t slot = index % m_tss_per_group;
    return m_tss_base + group * m_tss_group_size + slot * TSS_STRIDE;
}

void tss_alloc() {
    ASSERT(g_cpu_count > 0);

    // use as few groups as we can, and spread the cores evenly
    // between them so each group is as small as possible
    size_t group_count = DIV_ROUND_UP(g_cpu_count, TSS_PER_IOPB_MAX);
    size_t per_group = DIV_ROUND_UP(g_cpu_count, group_count);

    // the next group must start aligned to the stride as well
    size_t group_size = ALIGN_UP(per_group * TSS_STRIDE + IOPB_SIZE, TSS_STRIDE);
    size_t total_size = group_count * group_size;

    void* base = phys_alloc(total_size);
    ASSERT(base != NULL, "gdt: failed to allocate %lu TSSs (0x%lx bytes)", g_cpu_count, total_size);

    for (size_t i = 0; i < group_count; i++) {
        void* group = base + i * group_size;
        uint8_t* iopb = group + per_group * TSS_STRIDE;

        // the memory is already zeroed, which lets usermode access all
        // the ports, we only need to set the byte at the end
        iopb[IOPB_SIZE - 1] = 0xFF;

        for (size_t j = 0; j < per_group; j++) {
            tss64_t* tss = group + j * TSS_STRIDE;
            tss->iopb_offset = (void*)iopb - (void*)tss;
        }
    }

    m_tss_base = base;
    m_tss_per_group = per_group;
    m_tss_group_size = group_size;
}

static void tss_init() {
    irq_spinlock_guard(&m_tss_lock);

    ASSERT(m_tss_next < g_cpu_count, "gdt: no TSS left for this core");
    tss64_t* tss = tss_get(m_tss_next++);

    // the limit must cover the iopb, which is past 16 bits for most of the TSSs
    uintptr_t base = (uintptr_t)tss;
    uint32_t limit = tss->iopb_offset + IOPB_SIZE - 1;
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

void gdt_init() {
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

    // init the tss for the core
    tss_init();
}
