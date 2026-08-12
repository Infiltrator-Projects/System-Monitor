// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file smbios_memory.h
 * @brief Minimal native SMBIOS Type-17 memory-device reader.
 *
 * The parser reads the kernel-exported DMI table directly.  It deliberately
 * implements only the fields Linux-System-Monitor displays, avoiding a
 * dependency on dmidecode while keeping the privileged surface very small.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_SMBIOS_MEMORY_H
#define LINUX_SYSTEM_MONITOR_SMBIOS_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LSM_SMBIOS_FORM_FACTOR_LEN 64
#define LSM_SMBIOS_MAX_MODULES 32

/** Detailed fields retained for one populated Type-17 record. */
typedef struct {
    char locator[64];
    char bank_locator[64];
    char manufacturer[64];
    char part_number[96];
    char serial_number[64];
    char memory_type[32];
    char form_factor[32];
    uint64_t size_bytes;
    unsigned speed_mhz;
} LsmSmbiosMemoryModule;

/** Aggregated physical-memory information from SMBIOS Memory Device records. */
typedef struct {
    unsigned speed_mhz;   /**< Lowest configured speed among populated slots. */
    unsigned slots_used;  /**< Type-17 records containing an installed module. */
    unsigned slots_total; /**< Total Type-17 memory-device records. */
    char form_factor[LSM_SMBIOS_FORM_FACTOR_LEN]; /**< First known populated form factor. */
    LsmSmbiosMemoryModule modules[LSM_SMBIOS_MAX_MODULES];
    size_t module_count;
} LsmSmbiosMemoryInfo;

/**
 * Parse memory-device records from a raw DMI table.
 *
 * @param path Raw DMI table path, normally /sys/firmware/dmi/tables/DMI.
 * @param info Receives a fully initialised result on success.
 * @param error Optional diagnostic buffer.
 * @param error_size Size of @p error.
 * @return true when at least one valid Type-17 record was found.
 */
bool lsm_smbios_memory_read(const char *path,
                            LsmSmbiosMemoryInfo *info,
                            char *error,
                            size_t error_size);

#endif
