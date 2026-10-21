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
#include "thread/runq.h"
#include "thread/sched.h"
#include "thread/thread.h"
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

static void core_entry(struct limine_mp_info* info) {
    size_t cpu_id = info->extra_argument;
    TRACE("\tCPU #%zu", cpu_id);
    arch_init_per_core(cpu_id);
    sched_start_per_core();
}

static void start_cores(void) {
    struct limine_mp_response* mp = m_mp_request.response;
    if (mp == nullptr) {
        return;
    }

    // set the total cpu count
    size_t cpu_id = 1; // id 0 is reserved for the BSP

    // we start the cores one by one because we don't want any mutexes to trigger
    // the slow path before we get into a threaded environment
    TRACE("CPU Startup:");
    for (size_t i = 0; i < mp->cpu_count; i++) {
        struct limine_mp_info* info = mp->cpus[i];

        if (info->lapic_id == mp->bsp_lapic_id) {
            info->extra_argument = 0;

            // this is the current core, nothing to do
            TRACE("\tCPU #%ld (BSP)", info->extra_argument);
        } else {
            info->extra_argument = cpu_id++;

            _Atomic(limine_goto_address)* goto_address = (void*)&info->goto_address;
            atomic_store_explicit(goto_address, core_entry, memory_order_release);
        }
    }

    // wait for all cores to start scheduling
    sched_wait();
}

/////////////////////////////////////////////////////////////////////////
// Per-cpu locals allocation
/////////////////////////////////////////////////////////////////////////

extern char __start_cpu_locals[];
extern char __stop_cpu_locals[];

static void alloc_cpu_locals(void) {
    // get the core count, if we don't have the mp
    // response fallback to one core
    size_t cpu_count = 1;
    if (m_mp_request.response != nullptr) {
        cpu_count = m_mp_request.response->cpu_count;
        ASSERT(cpu_count >= 1);
    }

    // publish the cpu count
    g_cpu_count = cpu_count;

    // this should be small enough that using the buddy should be fine
    g_cpu_locals_offsets = phys_alloc(cpu_count * sizeof(void*));
    ASSERT(g_cpu_locals_offsets != nullptr);

    // the locals of the first core live in the kernel itself, so zero offset,
    // we allocate the rest elsewhere
    g_cpu_locals_offsets[0] = 0;
    g_cpu_locals_offset = 0;
    g_cpu_id = 0;

    size_t locals_page_count = DIV_ROUND_UP(__stop_cpu_locals - __start_cpu_locals, PAGE_SIZE);
    for (size_t i = 1; i < cpu_count; i++) {
        vmar_t* vmar = nullptr;
        ASSERT_SUCCESS(vmar_map(&g_kernel_region, &vmar, VMAR_ANY_OFFSET, locals_page_count));
        ASSERT_SUCCESS(vmar_populate(vmar));
        vmar_lock(vmar);
        vmar_set_name(vmar, "cpu-locals-%zu", i);

        // set the offset and local offset of the core
        ptrdiff_t offset = vmar->base - (void*)__start_cpu_locals;
        g_cpu_locals_offsets[i] = offset;
        *CPU_LOCAL_PTR_OF(g_cpu_locals_offset, i) = offset;
        *CPU_LOCAL_PTR_OF(g_cpu_id, i) = i;

        OBJECT_PUT(vmar);
    }
}

/////////////////////////////////////////////////////////////////////////
// The kernel entry point
/////////////////////////////////////////////////////////////////////////

static void kernel_init_thread(void* arg) {
    (void)arg;

    // start up the cores
    start_cores();

    TRACE("DONE!");

    // TODO: continue the rest of the init
}

static void bootloader_sanity(void) {
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
static void init_stack_guard(void) {
    uint64_t value = boot_random_get(RANDOM_INDEX_STACK_CHK_GUARD);
    value &= ~(uintptr_t)0xFF; // ensure string operations don't corrupt too much
    __stack_chk_guard = value;
}

void _start(void) {
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

    // allocate all of the cpu locals
    alloc_cpu_locals();

    // finalize arch init
    arch_init();

    // need to start by initializing the run queue so we can
    // actually queue a thread to run
    sched_init();

    // setup the init thread
    thread_t* thread = nullptr;
    ASSERT_SUCCESS(thread_create(&thread, kernel_init_thread, nullptr, "init"));
    thread_start(thread);

    // and start the scheduler on the BSP
    sched_start_per_core();
}
