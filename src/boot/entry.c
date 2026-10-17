#include "arch/virt.h"
#include "boot/arch.h"
#include "boot/random.h"
#include "boot/virt.h"
#include "lib/assert.h"
#include "lib/object.h"
#include "lib/snprintf.h"
#include "lib/sp.h"
#include "lib/trace.h"
#include "limine.h"
#include "mem/mapping.h"
#include "mem/phys.h"
#include "mem/vmar.h"
#include "sync/smp.h"
#include "util/defs.h"

#include <immintrin.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/////////////////////////////////////////////////////////////////////////
// Limine requests
/////////////////////////////////////////////////////////////////////////

/**
 * We use the latest limine revision that is currently available (6)
 */
[[gnu::used, gnu::section(".limine_requests")]]
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

/**
 * The start and end markers of limine requests
 */
[[gnu::used, gnu::section(".limine_requests_start")]]
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

[[gnu::used, gnu::section(".limine_requests_end")]]
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

[[gnu::section(".limine_requests")]]
static volatile struct limine_bootloader_info_request g_bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0, .response = nullptr
};

/////////////////////////////////////////////////////////////////////////
// SMP entry point
/////////////////////////////////////////////////////////////////////////

[[gnu::section(".limine_requests")]]
static volatile struct limine_mp_request m_mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
#ifdef __x86_64__
    .flags = LIMINE_MP_REQUEST_X86_64_X2APIC,
#endif
};

static atomic_size_t m_core_init_finished = 0;

static void core_entry(struct limine_mp_info* info) {
    size_t cpu_id = info->extra_argument;
    TRACE("\tCPU #%zu", cpu_id);

    // perform the arch cpu init, this will also fixup the cpu locals
    // so we can use them, so we want to call this as early as we can
    arch_init_per_core(cpu_id);

    // TODO: rest of init

    // mark that we finished running the core
    m_core_init_finished++;
    for (;;)
        asm("hlt");
}

static void start_cores() {
    if (m_mp_request.response == nullptr) {
        return;
    }

    size_t cpu_id = 1; // id 0 is reserved for the BSP

    TRACE("CPU Startup:");
    for (size_t i = 0; i < g_cpu_count; i++) {
        struct limine_mp_info* info = m_mp_request.response->cpus[i];
        info->extra_argument = cpu_id++;

        if (info->lapic_id == m_mp_request.response->bsp_lapic_id) {
            // this is the current core, nothing to do
            TRACE("\tCPU #%ld (BSP)", info->extra_argument);
            m_core_init_finished++;
        } else {
            _Atomic(limine_goto_address)* goto_address = (void*)&info->goto_address;
            atomic_store_explicit(goto_address, core_entry, memory_order_release);
        }
    }

    // wait until all the cores are done initializing
    while (m_core_init_finished != g_cpu_count)
        ;
}

/////////////////////////////////////////////////////////////////////////
// Per-cpu locals allocation
/////////////////////////////////////////////////////////////////////////

extern char __start_cpu_locals[];
extern char __stop_cpu_locals[];

static void alloc_cpu_locals() {
    // this should be small enough that using the buddy should be fine
    g_cpu_locals_offsets = phys_alloc(g_cpu_count * sizeof(void*));
    ASSERT(g_cpu_locals_offsets != nullptr);

    // the locals of the first core live in the kernel itself, so zero offset,
    // we allocate the rest elsewhere
    g_cpu_locals_offsets[0] = 0;
    g_cpu_locals_offset = 0;

    size_t locals_page_count = DIV_ROUND_UP(__stop_cpu_locals - __start_cpu_locals, PAGE_SIZE);
    for (size_t i = 1; i < g_cpu_count; i++) {
        vmar_t* vmar = nullptr;
        ASSERT_SUCCESS(vmar_map(&g_kernel_region, &vmar, VMAR_ANY_OFFSET, locals_page_count));
        ksnprintf(vmar->name, sizeof(vmar->name), "cpu-locals-%zu", i);

        // set the offset and local offset of the core
        ptrdiff_t offset = vmar->base - (void*)__start_cpu_locals;
        g_cpu_locals_offsets[i] = offset;
        *CPU_LOCAL_PTR_OF(g_cpu_locals_offset, i) = offset;
    }
}

/////////////////////////////////////////////////////////////////////////
// The kernel entry point
/////////////////////////////////////////////////////////////////////////

static void bootloader_sanity() {
    if (LIMINE_LOADED_BASE_REVISION_VALID(limine_base_revision)) {
        TRACE("Bootloader has loaded us using base revision %lu",
              LIMINE_LOADED_BASE_REVISION(limine_base_revision));
    }

    // make sure we got the requested bootloader revision
    ASSERT(LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision),
           "Limine base revision not supported");

    if (g_bootloader_info_request.response != nullptr) {
        TRACE("Bootloader: %s (%s)", g_bootloader_info_request.response->name,
              g_bootloader_info_request.response->version);
    }
}

[[clang::no_stack_protector]]
static void init_stack_guard() {
    uint64_t value = boot_random_get(RANDOM_INDEX_STACK_CHK_GUARD);
    value &= ~(uintptr_t)0xFF; // ensure string operations don't corrupt too much
    __stack_chk_guard = value;
}

void _start() {
#ifdef __DEBUG__
    TRACE("Tomato! (DEBUG)");
#else
    TRACE("Tomato!");
#endif
    bootloader_sanity();

    // setup the stack guard as soon as possible
    init_stack_guard();

    // make sure we have all the required cpu features
    arch_early_init();

    // early setup, this leaves us with our own page
    // tables and a working physical memory allocator
    // so we can continue
    early_virt_init();

    // get the core count, if we don't have the mp request
    // fallback to one core
    if (m_mp_request.response != nullptr) {
        g_cpu_count = m_mp_request.response->cpu_count;
        ASSERT(g_cpu_count >= 1);
    } else {
        g_cpu_count = 1;
    }

    // finalize arch init, this can be called before alloc locals
    // before we assume the first locals are part of the kernel
    arch_init();

    // allocate all of the cpu locals
    alloc_cpu_locals();

    // and now we can start the rest of the system
    start_cores();

    TRACE("LOADED");

    for (;;)
        asm("hlt");
}
