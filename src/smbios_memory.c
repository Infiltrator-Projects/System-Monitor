// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file smbios_memory.c
 * @brief Native parser for SMBIOS Type-17 physical-memory records.
 *
 * Linux exports the firmware's raw DMI table through sysfs.  Type 17 contains
 * the slot population, form factor and memory-speed fields used by the Memory
 * performance page.  Parsing the narrow subset here removes the previous
 * dmidecode subprocess and its package dependency.
 *
 * The parser is intentionally defensive: every formatted-field access is
 * length checked, the table size is capped, and malformed string areas stop
 * parsing rather than permitting an out-of-bounds walk.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "smbios_memory.h"

#include <infiltratr/endian.h>
#include <infiltratr/posix_io.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LSM_DMI_DEFAULT_PATH "/sys/firmware/dmi/tables/DMI"
#define LSM_DMI_MAX_BYTES (16u * 1024u * 1024u)
#define SMBIOS_TYPE_MEMORY_DEVICE 17u
#define SMBIOS_TYPE_END 127u

static void set_error(char *error, size_t size,
                      const char *message, const char *detail)
{
    if (!error || size == 0) return;
    if (detail && *detail) snprintf(error, size, "%s: %s", message, detail);
    else snprintf(error, size, "%s", message);
}

static const char *form_factor_name(uint8_t value)
{
    static const char *const names[] = {
        "", "Other", "Unknown", "SIMM", "SIP", "Chip", "DIP", "ZIP",
        "Proprietary Card", "DIMM", "TSOP", "Row of Chips", "RIMM",
        "SODIMM", "SRIMM", "FB-DIMM", "Die"
    };
    return value < sizeof(names) / sizeof(names[0]) ? names[value] : "Unknown";
}

static const char *memory_type_name(uint8_t value)
{
    switch (value) {
        case 0x12: return "DDR";
        case 0x13: return "DDR2";
        case 0x18: return "DDR3";
        case 0x1a: return "DDR4";
        case 0x1b: return "LPDDR";
        case 0x1c: return "LPDDR2";
        case 0x1d: return "LPDDR3";
        case 0x1e: return "LPDDR4";
        case 0x20: return "HBM";
        case 0x21: return "HBM2";
        case 0x22: return "DDR5";
        case 0x23: return "LPDDR5";
        case 0x24: return "HBM3";
        default: return "N/A";
    }
}

static uint64_t memory_size_bytes(const uint8_t *record, size_t length)
{
    if (!record || length <= 0x0d) return 0U;
    const uint16_t encoded = infiltratr_load_le16(record + 0x0c);
    if (encoded == 0U || encoded == UINT16_MAX) return 0U;
    if (encoded == 0x7fffU) {
        if (length < 0x20) return 0U;
        return (uint64_t)infiltratr_load_le32(record + 0x1c) * 1024U * 1024U;
    }
    if ((encoded & 0x8000U) != 0U)
        return (uint64_t)(encoded & 0x7fffU) * 1024U;
    return (uint64_t)encoded * 1024U * 1024U;
}

static void copy_smbios_string(const uint8_t *table, size_t table_size,
                               size_t strings_start, size_t strings_end,
                               uint8_t string_index, char *destination,
                               size_t destination_size)
{
    if (!destination || destination_size == 0U) return;
    snprintf(destination, destination_size, "N/A");
    if (!table || string_index == 0U || strings_start >= strings_end ||
        strings_end > table_size)
        return;

    size_t cursor = strings_start;
    unsigned current = 1U;
    while (cursor < strings_end) {
        const size_t start = cursor;
        while (cursor < strings_end && table[cursor] != 0U) cursor++;
        if (current == string_index) {
            size_t length = cursor - start;
            while (length > 0U &&
                   (table[start + length - 1U] == ' ' ||
                    table[start + length - 1U] == '\t'))
                length--;
            if (length >= destination_size) length = destination_size - 1U;
            memcpy(destination, table + start, length);
            destination[length] = '\0';
            if (!destination[0]) snprintf(destination, destination_size, "N/A");
            return;
        }
        if (cursor >= strings_end) break;
        cursor++;
        current++;
    }
}

static bool populated_memory_device(const uint8_t *record, size_t length)
{
    if (length <= 0x0d) return false;
    const uint16_t size = infiltratr_load_le16(record + 0x0c);
    if (size == 0 || size == UINT16_MAX) return false;
    if (size != 0x7fff) return true;
    return length >= 0x20 && infiltratr_load_le32(record + 0x1c) != 0;
}

static unsigned memory_speed(const uint8_t *record, size_t length)
{
    uint32_t rated = 0;
    uint32_t configured = 0;

    if (length >= 0x17) rated = infiltratr_load_le16(record + 0x15);
    if (length >= 0x22) configured = infiltratr_load_le16(record + 0x20);

    /* SMBIOS 3.3 added 32-bit extended speed fields for values that cannot be
     * represented in the original 16-bit locations. */
    if (rated == UINT16_MAX && length >= 0x58)
        rated = infiltratr_load_le32(record + 0x54);
    if (configured == UINT16_MAX && length >= 0x5c)
        configured = infiltratr_load_le32(record + 0x58);

    if (configured == UINT16_MAX) configured = 0;
    if (rated == UINT16_MAX) rated = 0;
    return configured ? (unsigned)configured : (unsigned)rated;
}

static bool read_table(const char *path, uint8_t **bytes_out, size_t *size_out,
                       char *error, size_t error_size)
{
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        set_error(error, error_size, "Unable to open DMI table", strerror(errno));
        return false;
    }

    struct stat status;
    if (fstat(descriptor, &status) != 0) {
        set_error(error, error_size, "Unable to inspect DMI table", strerror(errno));
        close(descriptor);
        return false;
    }
    if (status.st_size <= 0 || (uint64_t)status.st_size > LSM_DMI_MAX_BYTES) {
        set_error(error, error_size, "DMI table has an invalid size", NULL);
        close(descriptor);
        return false;
    }

    const size_t size = (size_t)status.st_size;
    uint8_t *bytes = malloc(size);
    if (!bytes) {
        set_error(error, error_size, "Unable to allocate DMI buffer", "out of memory");
        close(descriptor);
        return false;
    }

    if (infiltratr_read_full(descriptor, bytes, size) != 0) {
        const int saved_errno = errno;
        set_error(error, error_size, "Unable to read DMI table",
                  strerror(saved_errno));
        free(bytes);
        (void)close(descriptor);
        errno = saved_errno;
        return false;
    }
    if (close(descriptor) != 0) {
        const int saved_errno = errno;
        set_error(error, error_size, "Unable to close DMI table",
                  strerror(saved_errno));
        free(bytes);
        errno = saved_errno;
        return false;
    }
    *bytes_out = bytes;
    *size_out = size;
    return true;
}

bool lsm_smbios_memory_read(const char *path,
                            LsmSmbiosMemoryInfo *info,
                            char *error,
                            size_t error_size)
{
    if (error && error_size) error[0] = '\0';
    if (!info) {
        set_error(error, error_size, "No result structure supplied", NULL);
        return false;
    }
    memset(info, 0, sizeof(*info));
    snprintf(info->form_factor, sizeof(info->form_factor), "N/A");
    if (!path || !*path) path = LSM_DMI_DEFAULT_PATH;

    uint8_t *table = NULL;
    size_t table_size = 0;
    if (!read_table(path, &table, &table_size, error, error_size)) return false;

    size_t offset = 0;
    while (offset + 4 <= table_size) {
        const uint8_t type = table[offset];
        const uint8_t length = table[offset + 1];
        if (length < 4 || offset + length > table_size) break;

        const size_t strings_start = offset + length;
        size_t next = strings_start;
        while (next + 1U < table_size &&
               (table[next] != 0U || table[next + 1U] != 0U))
            next++;
        if (next + 1U >= table_size) break;

        if (type == SMBIOS_TYPE_MEMORY_DEVICE) {
            info->slots_total++;
            const uint8_t *record = table + offset;
            if (populated_memory_device(record, length)) {
                info->slots_used++;
                const unsigned speed = memory_speed(record, length);
                if (speed > 0 && (info->speed_mhz == 0 || speed < info->speed_mhz))
                    info->speed_mhz = speed;

                if (strcmp(info->form_factor, "N/A") == 0 && length > 0x0e) {
                    const char *name = form_factor_name(record[0x0e]);
                    if (strcmp(name, "Unknown") != 0 && name[0] != '\0')
                        snprintf(info->form_factor, sizeof(info->form_factor), "%s", name);
                }

                if (info->module_count < LSM_SMBIOS_MAX_MODULES) {
                    LsmSmbiosMemoryModule *module =
                        &info->modules[info->module_count++];
                    memset(module, 0, sizeof(*module));
                    module->size_bytes = memory_size_bytes(record, length);
                    module->speed_mhz = speed;
                    snprintf(module->form_factor, sizeof(module->form_factor),
                             "%s", length > 0x0e
                                      ? form_factor_name(record[0x0e]) : "N/A");
                    snprintf(module->memory_type, sizeof(module->memory_type),
                             "%s", length > 0x12
                                      ? memory_type_name(record[0x12]) : "N/A");
                    copy_smbios_string(table, table_size, strings_start, next,
                        length > 0x10 ? record[0x10] : 0U,
                        module->locator, sizeof(module->locator));
                    copy_smbios_string(table, table_size, strings_start, next,
                        length > 0x11 ? record[0x11] : 0U,
                        module->bank_locator, sizeof(module->bank_locator));
                    copy_smbios_string(table, table_size, strings_start, next,
                        length > 0x17 ? record[0x17] : 0U,
                        module->manufacturer, sizeof(module->manufacturer));
                    copy_smbios_string(table, table_size, strings_start, next,
                        length > 0x18 ? record[0x18] : 0U,
                        module->serial_number, sizeof(module->serial_number));
                    copy_smbios_string(table, table_size, strings_start, next,
                        length > 0x1a ? record[0x1a] : 0U,
                        module->part_number, sizeof(module->part_number));
                }
            }
        }

        if (type == SMBIOS_TYPE_END) break;

        offset = next + 2;
    }

    free(table);
    if (info->slots_total == 0) {
        set_error(error, error_size, "No SMBIOS memory-device records found", NULL);
        return false;
    }
    return true;
}
