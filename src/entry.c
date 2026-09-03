#include "lib/assert.h"
#include "lib/trace.h"
#include "limine.h"

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

void _start() {
    TRACE("Tomato!");
    bootloader_sanity();

    for (;;)
        asm("hlt");
}
