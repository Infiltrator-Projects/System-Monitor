// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pci_names.c
 * @brief In-process lookup against the embedded PCI identity index.
 *
 * Normal operation searches a compact table compiled into the executable, so
 * adapter identity resolution has no runtime data-file, pciutils, lspci or
 * lshw dependency.  Developers may set LSM_PCI_DB_PATH to test an alternate
 * TSV table before the embedded fallback is consulted.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "pci_names.h"

#include "common.h"
#include "pci_names_data.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

static bool normalise_id(const char *input, char output[5])
{
    if (!input) return false;
    while (isspace((unsigned char)*input)) input++;
    if (input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) input += 2;

    for (size_t index = 0U; index < 4U; index++) {
        if (!isxdigit((unsigned char)input[index])) return false;
        output[index] = (char)toupper((unsigned char)input[index]);
    }
    input += 4;
    while (isspace((unsigned char)*input)) input++;
    if (*input != '\0') return false;
    output[4] = '\0';
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
        /* Hardware identities are effectively static for a desktop session.
         * If more than 64 unique devices are queried, reuse the oldest slot. */
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

static void search_database_file(FILE *database,
                                 const char vendor_id[5],
                                 const char device_id[5],
                                 LsmPciCacheEntry *entry)
{
    char line[1024];
    while (fgets(line, sizeof(line), database)) {
        lsm_trim_line_end(line);
        if (!line[0] || line[0] == '#') continue;

        char *save = NULL;
        char *kind = strtok_r(line, "\t", &save);
        char *row_vendor = strtok_r(NULL, "\t", &save);
        if (!kind || !row_vendor || strcasecmp(row_vendor, vendor_id) != 0)
            continue;

        if (strcmp(kind, "V") == 0) {
            char *name = save;
            if (name && *name && !entry->vendor[0])
                lsm_copy_string(entry->vendor, sizeof(entry->vendor), name);
        } else if (strcmp(kind, "D") == 0) {
            char *row_device = strtok_r(NULL, "\t", &save);
            char *name = save;
            if (row_device && name && *name &&
                strcasecmp(row_device, device_id) == 0 && !entry->product[0])
                lsm_copy_string(entry->product, sizeof(entry->product), name);
        }

        if (entry->vendor[0] && entry->product[0]) break;
    }
}

static void copy_embedded_name(char *destination, size_t destination_size,
                               const char *start, const char *end)
{
    if (!destination || destination_size == 0U || !start || !end || end <= start)
        return;
    while (end > start && (end[-1] == '\r' || end[-1] == '\n')) end--;
    size_t length = (size_t)(end - start);
    if (length >= destination_size) length = destination_size - 1U;
    memcpy(destination, start, length);
    destination[length] = '\0';
}

static void search_embedded_chunk(const char *data, size_t data_size,
                                  const char vendor_id[5],
                                  const char device_id[5],
                                  LsmPciCacheEntry *entry)
{
    const char *cursor = data;
    const char *const end = data + data_size;
    while (cursor < end) {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        if (!line_end) line_end = end;
        const size_t length = (size_t)(line_end - cursor);

        if (length >= 7U && cursor[0] == 'V' && cursor[1] == '\t' &&
            memcmp(cursor + 2, vendor_id, 4U) == 0 && cursor[6] == '\t' &&
            !entry->vendor[0]) {
            copy_embedded_name(entry->vendor, sizeof(entry->vendor),
                               cursor + 7, line_end);
        } else if (length >= 12U && cursor[0] == 'D' && cursor[1] == '\t' &&
                   memcmp(cursor + 2, vendor_id, 4U) == 0 && cursor[6] == '\t' &&
                   memcmp(cursor + 7, device_id, 4U) == 0 && cursor[11] == '\t' &&
                   !entry->product[0]) {
            copy_embedded_name(entry->product, sizeof(entry->product),
                               cursor + 12, line_end);
        }
        cursor = line_end < end ? line_end + 1 : end;
    }
}

static void search_embedded_database(const char vendor_id[5],
                                     const char device_id[5],
                                     LsmPciCacheEntry *entry)
{
    for (size_t index = 0U; index < lsm_pci_names_chunk_count; index++) {
        search_embedded_chunk(lsm_pci_names_chunks[index],
                              lsm_pci_names_chunk_sizes[index],
                              vendor_id, device_id, entry);
        if (entry->vendor[0] && entry->product[0]) break;
    }
}

static void populate_entry(LsmPciCacheEntry *entry)
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
    if (!entry->vendor[0] || !entry->product[0])
        search_embedded_database(entry->vendor_id, entry->device_id, entry);
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
    if (!normalise_id(vendor_id, normal_vendor) ||
        !normalise_id(device_id, normal_device))
        return false;

    LsmPciCacheEntry *entry = find_cached(normal_vendor, normal_device);
    if (!entry) {
        entry = new_cache_entry(normal_vendor, normal_device);
        populate_entry(entry);
    }

    lsm_copy_string(vendor, vendor_size, entry->vendor);
    lsm_copy_string(product, product_size, entry->product);
    return entry->found;
}
