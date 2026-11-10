#include "acpi.h"

#include "acpi/tables.h"
#include "boot/virt.h"
#include "limine.h"

#include <stddef.h>
#include <stdint.h>

[[gnu::section(".limine_requests")]]
static volatile struct limine_rsdp_request m_rsdp_request = { .id = LIMINE_RSDP_REQUEST_ID,
                                                              .revision = 0,
                                                              .response = nullptr };

static bool acpi_checksum_valid(const void* ptr, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += ((const uint8_t*)ptr)[i];
    }
    return sum == 0;
}

static bool acpi_signature_equals(const char* signature, const char* expected, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (signature[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

static acpi_sdt_header_t* acpi_check_table(uint64_t phys, const char* signature) {
    acpi_sdt_header_t* header = early_phys_to_direct(phys);
    if (!acpi_signature_equals(header->signature, signature, sizeof(header->signature))) {
        return nullptr;
    }

    if (header->length < sizeof(*header) || !acpi_checksum_valid(header, header->length)) {
        return nullptr;
    }

    return header;
}

void* early_acpi_find_table(const char* signature) {
    if (m_rsdp_request.response == nullptr) {
        return nullptr;
    }

    // base revision 4 and up gives us the address in the direct map
    acpi_rsdp_t* rsdp = m_rsdp_request.response->address;
    if (!acpi_signature_equals(rsdp->signature, ACPI_RSDP_SIGNATURE, sizeof(rsdp->signature)) ||
        !acpi_checksum_valid(rsdp, ACPI_RSDP_V1_LENGTH)) {
        return nullptr;
    }

    // prefer the XSDT, the RSDT is all there is on ACPI 1.0
    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        acpi_xsdt_t* xsdt = (acpi_xsdt_t*)acpi_check_table(rsdp->xsdt_address, "XSDT");
        if (xsdt == nullptr) {
            return nullptr;
        }

        size_t count = (xsdt->header.length - sizeof(xsdt->header)) / sizeof(uint64_t);
        for (size_t i = 0; i < count; i++) {
            acpi_sdt_header_t* table = acpi_check_table(xsdt->entries[i], signature);
            if (table != nullptr) {
                return table;
            }
        }
    } else {
        acpi_rsdt_t* rsdt = (acpi_rsdt_t*)acpi_check_table(rsdp->rsdt_address, "RSDT");
        if (rsdt == nullptr) {
            return nullptr;
        }

        size_t count = (rsdt->header.length - sizeof(rsdt->header)) / sizeof(uint32_t);
        for (size_t i = 0; i < count; i++) {
            acpi_sdt_header_t* table = acpi_check_table(rsdt->entries[i], signature);
            if (table != nullptr) {
                return table;
            }
        }
    }

    return nullptr;
}
