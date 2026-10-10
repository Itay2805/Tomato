#include "boot/arch.h"
#include "boot/random.h"
#include "boot/virt.h"
#include "lib/assert.h"
#include "lib/sp.h"
#include "lib/trace.h"
#include "limine.h"

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
    arch_init_per_core();

    // let the BSP know we are done
    m_core_init_finished++;

    for (;;)
        asm("hlt");
}

static void start_cores(void) {
    struct limine_mp_response* mp = m_mp_request.response;
    if (mp == nullptr) {
        return;
    }

    size_t cpu_id = 1; // id 0 is reserved for the BSP

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

    // wait until all the other cores are done initializing
    while (m_core_init_finished != mp->cpu_count - 1)
        ;
}

/////////////////////////////////////////////////////////////////////////
// The kernel entry point
/////////////////////////////////////////////////////////////////////////

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

    // finalize arch init
    arch_init();

    // start up the cores
    start_cores();

    TRACE("DONE!");

    // TODO: continue the rest of the init
    for (;;)
        asm("hlt");
}
