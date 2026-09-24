// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pci_names.c
 * @brief In-process lookup against System Monitor's normalized PCI registry.
 *
 * Normal operation performs binary search over project-generated numeric
 * tables, so identity resolution has no runtime pciutils, lspci, lshw or
 * external names-database dependency. Developers may set LSM_PCI_DB_PATH to
 * exercise an alternate flat registry before the embedded fallback.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "pci_names.h"

#include "common.h"
#include "pci_names_data.h"

#include <infiltratr/core.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LSM_PCI_NAME_LEN 256
#define LSM_PCI_CACHE_SIZE 64

typedef struct {
    char vendor_id[5];
    char device_id[5];
    char vendor[LSM_PCI_NAME_LEN];
    char product[LSM_PCI_NAME_LEN];
    bool found;
} LsmPciCacheEntry;

static LsmPciCacheEntry cache[LSM_PCI_CACHE_SIZE];
static size_t cache_count;

static bool normalise_id(const char *input, char output[5], uint16_t *numeric)
{
    if (!input || !numeric) return false;
    while (lsm_ascii_is_space((unsigned char)*input)) input++;
    if (input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) input += 2;
    for (size_t index = 0U; index < 4U; index++) {
        if (!lsm_ascii_is_xdigit((unsigned char)input[index])) return false;
        output[index] = (char)lsm_ascii_to_upper((unsigned char)input[index]);
    }
    input += 4;
    while (lsm_ascii_is_space((unsigned char)*input)) input++;
    if (*input != '\0') return false;
    output[4] = '\0';

    uint64_t parsed = 0U;
    if (!infiltratr_parse_u64(output, 16U, &parsed) || parsed > UINT16_MAX)
        return false;
    *numeric = (uint16_t)parsed;
    return true;
}

static LsmPciCacheEntry *find_cached(const char vendor_id[5],
                                     const char device_id[5])
{
    for (size_t index = 0U; index < cache_count; index++) {
        if (strcmp(cache[index].vendor_id, vendor_id) == 0 &&
            strcmp(cache[index].device_id, device_id) == 0)
            return &cache[index];
    }
    return NULL;
}

static LsmPciCacheEntry *new_cache_entry(const char vendor_id[5],
                                         const char device_id[5])
{
    size_t index;
    if (cache_count < LSM_PCI_CACHE_SIZE) {
        index = cache_count++;
    } else {
        /* Hardware identities are static for a normal desktop session.
         * Reuse the oldest small-cache slot after unusually broad probing. */
        memmove(&cache[0], &cache[1],
                sizeof(cache[0]) * (LSM_PCI_CACHE_SIZE - 1U));
        index = LSM_PCI_CACHE_SIZE - 1U;
    }
    memset(&cache[index], 0, sizeof(cache[index]));
    lsm_copy_string(cache[index].vendor_id, sizeof(cache[index].vendor_id),
                    vendor_id);
    lsm_copy_string(cache[index].device_id, sizeof(cache[index].device_id),
                    device_id);
    return &cache[index];
}

/* Flat developer override grammar:
 * V<TAB>VVVV<TAB>vendor-name
 * D<TAB>VVVV:DDDD<TAB>vendor-name<TAB>device-name */
static void search_database_file(FILE *database,
                                 const char vendor_id[5],
                                 const char device_id[5],
                                 LsmPciCacheEntry *entry)
{
    char expected_key[10];
    (void)snprintf(expected_key, sizeof(expected_key), "%s:%s",
                   vendor_id, device_id);

    char line[1024];
    while (fgets(line, sizeof(line), database)) {
        lsm_trim_line_end(line);
        if (!line[0] || line[0] == '#') continue;

        char *save = NULL;
        char *kind = strtok_r(line, "\t", &save);
        char *key = strtok_r(NULL, "\t", &save);
        if (!kind || !key) continue;

        if (strcmp(kind, "V") == 0 &&
            lsm_ascii_equal_ci(key, vendor_id)) {
            char *vendor = save;
            if (vendor && *vendor && !entry->vendor[0])
                lsm_copy_string(entry->vendor, sizeof(entry->vendor), vendor);
        } else if (strcmp(kind, "D") == 0 &&
                   lsm_ascii_equal_ci(key, expected_key)) {
            char *vendor = strtok_r(NULL, "\t", &save);
            char *product = save;
            if (vendor && *vendor && !entry->vendor[0])
                lsm_copy_string(entry->vendor, sizeof(entry->vendor), vendor);
            if (product && *product && !entry->product[0])
                lsm_copy_string(entry->product, sizeof(entry->product), product);
        }
        if (entry->vendor[0] && entry->product[0]) break;
    }
}

static void populate_entry(LsmPciCacheEntry *entry,
                           uint16_t vendor_id, uint16_t device_id)
{
    const char *override = getenv("LSM_PCI_DB_PATH");
    if (override && *override) {
        FILE *database = fopen(override, "r");
        if (database) {
            search_database_file(database, entry->vendor_id, entry->device_id,
                                 entry);
            fclose(database);
        }
    }

    if (!entry->vendor[0]) {
        const char *vendor = lsm_pci_data_vendor_name(vendor_id);
        if (vendor)
            lsm_copy_string(entry->vendor, sizeof(entry->vendor), vendor);
    }
    if (!entry->product[0]) {
        const char *product = lsm_pci_data_device_name(vendor_id, device_id);
        if (product)
            lsm_copy_string(entry->product, sizeof(entry->product), product);
    }
    entry->found = entry->vendor[0] != '\0' || entry->product[0] != '\0';
}

bool lsm_pci_names_lookup(const char *vendor_id,
                          const char *device_id,
                          char *vendor,
                          size_t vendor_size,
                          char *product,
                          size_t product_size)
{
    if (vendor && vendor_size > 0U) vendor[0] = '\0';
    if (product && product_size > 0U) product[0] = '\0';

    char normal_vendor[5];
    char normal_device[5];
    uint16_t numeric_vendor = 0U;
    uint16_t numeric_device = 0U;
    if (!normalise_id(vendor_id, normal_vendor, &numeric_vendor) ||
        !normalise_id(device_id, normal_device, &numeric_device))
        return false;

    LsmPciCacheEntry *entry = find_cached(normal_vendor, normal_device);
    if (!entry) {
        entry = new_cache_entry(normal_vendor, normal_device);
        populate_entry(entry, numeric_vendor, numeric_device);
    }

    lsm_copy_string(vendor, vendor_size, entry->vendor);
    lsm_copy_string(product, product_size, entry->product);
    return entry->found;
}
