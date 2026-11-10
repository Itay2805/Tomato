#include "debugcon.h"

#include "acpi/tables.h"
#include "arch/io.h"
#include "boot/acpi.h"
#include "boot/virt.h"
#include "debug/uart.h"
#include "lib/assert.h"
#include "lib/object.h"
#include "lib/string.h"
#include "mem/mapping.h"
#include "mem/pte.h"
#include "mem/vmar.h"
#include "util/defs.h"

#include <stddef.h>
#include <stdint.h>

/**
 * The UART we use as the debug console, it has no putc until it can be used
 */
LATE_RO static uart_t m_uart = {};

/**
 * A memory mapped UART can only be setup once debugcon_early_map maps it
 */
INIT_DATA static void (*m_pending_init)(uart_t* uart) = nullptr;
INIT_DATA static uint64_t m_pending_phys = 0;
INIT_DATA static size_t m_pending_size = 0;

static vmar_t m_debugcon_mapping = {
    .object = INIT_OBJECT(OBJECT_KIND_VMAR, "debugcon"),
    .kind = VMAR_KIND_SPECIAL,
    .locked = true,
};

/**
 * Setup the UART from a DBG2 device, returns false if we can't use it
 */
INIT_CODE static bool debugcon_init_device(acpi_dbg2_device_t* device) {
    if (device->port_type != ACPI_DBG2_PORT_SERIAL) {
        return false;
    }

    void (*init)(uart_t* uart);
    bool allow_port_io = false;
    switch (device->port_subtype) {
        case ACPI_DBG2_SERIAL_16550:
        case ACPI_DBG2_SERIAL_16550_SUBSET:
        case ACPI_DBG2_SERIAL_16550_GAS:
            init = ns16550_init;
            allow_port_io = true;
            break;

        case ACPI_DBG2_SERIAL_PL011:
            init = pl011_init;
            break;

        case ACPI_DBG2_SERIAL_SBSA_32BIT:
        case ACPI_DBG2_SERIAL_SBSA:
            init = sbsa_uart_init;
            break;

        default:
            return false;
    }

    // all of the registers of a UART are in the first range
    if (device->register_count == 0 ||
        device->base_address_offset + sizeof(acpi_gas_t) > device->length ||
        device->address_size_offset + sizeof(uint32_t) > device->length) {
        return false;
    }

    acpi_gas_t* gas = (void*)device + device->base_address_offset;
    uint32_t size;
    memcpy(&size, (void*)device + device->address_size_offset, sizeof(size));

    switch (gas->address_space_id) {
        case ACPI_GAS_SYSTEM_IO:
            if (!ARCH_HAS_PORT_IO || !allow_port_io || gas->address > UINT16_MAX) {
                return false;
            }

            // nothing to map, so it can be used right away
            m_uart.base = gas->address;
            m_uart.port_io = true;
            m_uart.access_size = 1;
            init(&m_uart);
            return true;

        case ACPI_GAS_SYSTEM_MEMORY:
            switch (gas->access_size) {
                case ACPI_GAS_ACCESS_UNDEFINED:
                case ACPI_GAS_ACCESS_BYTE:
                    m_uart.access_size = 1;
                    break;
                case ACPI_GAS_ACCESS_WORD:
                    m_uart.access_size = 2;
                    break;
                case ACPI_GAS_ACCESS_DWORD:
                    m_uart.access_size = 4;
                    break;
                default:
                    return false;
            }

            m_pending_init = init;
            m_pending_phys = gas->address;
            m_pending_size = MAX(size, 1);
            return true;

        default:
            return false;
    }
}

INIT_CODE void debugcon_init(void) {
    acpi_dbg2_t* dbg2 = early_acpi_find_table(ACPI_DBG2_SIGNATURE);
    if (dbg2 == nullptr) {
        return;
    }

    // use the first device we can drive
    size_t offset = dbg2->device_info_offset;
    for (size_t i = 0; i < dbg2->device_info_count; i++) {
        if (offset + sizeof(acpi_dbg2_device_t) > dbg2->header.length) {
            break;
        }

        acpi_dbg2_device_t* device = (void*)dbg2 + offset;
        if (device->length < sizeof(*device) || offset + device->length > dbg2->header.length) {
            break;
        }

        if (debugcon_init_device(device)) {
            break;
        }

        offset += device->length;
    }
}

INIT_CODE void debugcon_early_map(void) {
    if (m_pending_init == nullptr) {
        return;
    }

    uint64_t start = ALIGN_DOWN(m_pending_phys, PAGE_SIZE);
    size_t page_count = DIV_ROUND_UP(m_pending_phys + m_pending_size - start, PAGE_SIZE);
    ASSERT_SUCCESS(
        vmar_allocate_static(&g_kernel_region, &m_debugcon_mapping, VMAR_ANY_OFFSET, page_count));
    early_virt_map(m_debugcon_mapping.base, start, page_count,
                   PTE_WRITE | PTE_DEVICE | PTE_UNCACHED | PTE_GLOBAL);
    pte_barrier();

    m_uart.base = (uintptr_t)m_debugcon_mapping.base + (m_pending_phys - start);
    m_pending_init(&m_uart);
}

void debugcon_putc(char c) {
    if (m_uart.putc == nullptr) {
        return;
    }

    m_uart.putc(&m_uart, c);
}
