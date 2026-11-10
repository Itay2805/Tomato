#pragma once

#include <stdint.h>

/**
 * The layout of the ACPI tables we use, all of them are packed and
 * can be at any alignment
 */

////////////////////////////////////////////////////////////////////////////////////////////////////
// Root tables
////////////////////////////////////////////////////////////////////////////////////////////////////

#define ACPI_RSDP_SIGNATURE "RSD PTR "

typedef struct [[gnu::packed]] acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;

    // revision 2 and up
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} acpi_rsdp_t;

/**
 * The part of the RSDP that the checksum covers, the rest is
 * covered by the extended checksum
 */
#define ACPI_RSDP_V1_LENGTH 20

typedef struct [[gnu::packed]] acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_header_t;

typedef struct [[gnu::packed]] acpi_rsdt {
    acpi_sdt_header_t header;
    uint32_t entries[];
} acpi_rsdt_t;

typedef struct [[gnu::packed]] acpi_xsdt {
    acpi_sdt_header_t header;
    uint64_t entries[];
} acpi_xsdt_t;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic Address Structure
////////////////////////////////////////////////////////////////////////////////////////////////////

#define ACPI_GAS_SYSTEM_MEMORY 0
#define ACPI_GAS_SYSTEM_IO     1

#define ACPI_GAS_ACCESS_UNDEFINED 0
#define ACPI_GAS_ACCESS_BYTE      1
#define ACPI_GAS_ACCESS_WORD      2
#define ACPI_GAS_ACCESS_DWORD     3
#define ACPI_GAS_ACCESS_QWORD     4

typedef struct [[gnu::packed]] acpi_gas {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t access_size;
    uint64_t address;
} acpi_gas_t;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug Port Table 2
////////////////////////////////////////////////////////////////////////////////////////////////////

#define ACPI_DBG2_SIGNATURE "DBG2"

typedef struct [[gnu::packed]] acpi_dbg2 {
    acpi_sdt_header_t header;
    uint32_t device_info_offset;
    uint32_t device_info_count;
} acpi_dbg2_t;

#define ACPI_DBG2_PORT_SERIAL 0x8000

#define ACPI_DBG2_SERIAL_16550        0x0000
#define ACPI_DBG2_SERIAL_16550_SUBSET 0x0001
#define ACPI_DBG2_SERIAL_PL011        0x0003
#define ACPI_DBG2_SERIAL_SBSA_32BIT   0x000D
#define ACPI_DBG2_SERIAL_SBSA         0x000E
#define ACPI_DBG2_SERIAL_16550_GAS    0x0012

/**
 * All the offsets are from the start of the device, it is followed by the base
 * address registers (acpi_gas_t) and their sizes (uint32_t)
 */
typedef struct [[gnu::packed]] acpi_dbg2_device {
    uint8_t revision;
    uint16_t length;
    uint8_t register_count;
    uint16_t namespace_string_length;
    uint16_t namespace_string_offset;
    uint16_t oem_data_length;
    uint16_t oem_data_offset;
    uint16_t port_type;
    uint16_t port_subtype;
    uint16_t reserved;
    uint16_t base_address_offset;
    uint16_t address_size_offset;
} acpi_dbg2_device_t;
