#pragma once

/**
 * Find an ACPI table by its signature, returns null if there is no such table or
 * its checksum is wrong.
 *
 * The tables are only mapped by the bootloader's direct map, so this is only valid
 * before switching to our page tables.
 */
void* early_acpi_find_table(const char* signature);
