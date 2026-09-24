// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pci_names_data.c
 * @brief Generated binary-search tables for normalized PCI identity facts.
 *
 * Names are deduplicated into bounded chunks so the generated translation unit
 * stays within conservative C implementation limits. Numeric vendor/device
 * keys remain sorted for O(log n) lookup.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "pci_names_data.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t id;
    uint32_t name_ref;
} LsmPciVendorRecord;

typedef struct {
    uint32_t key;
    uint32_t name_ref;
} LsmPciDeviceRecord;



static const char *const lsm_pci_name_chunks[] = {

};

static const LsmPciVendorRecord lsm_pci_vendor_records[] = {

};

static const LsmPciDeviceRecord lsm_pci_device_records[] = {

};

static const char *name_at(uint32_t reference)
{
    const size_t chunk = (size_t)(reference >> 16U);
    const size_t offset = (size_t)(reference & 0xffffU);
    if (chunk >= sizeof(lsm_pci_name_chunks) / sizeof(lsm_pci_name_chunks[0]))
        return NULL;
    return lsm_pci_name_chunks[chunk] + offset;
}

const char *lsm_pci_data_vendor_name(uint16_t vendor_id)
{
    size_t low = 0U;
    size_t high =
        sizeof(lsm_pci_vendor_records) / sizeof(lsm_pci_vendor_records[0]);
    while (low < high) {
        const size_t middle = low + (high - low) / 2U;
        const uint16_t candidate = lsm_pci_vendor_records[middle].id;
        if (candidate < vendor_id)
            low = middle + 1U;
        else
            high = middle;
    }
    if (low >= sizeof(lsm_pci_vendor_records) /
                   sizeof(lsm_pci_vendor_records[0]) ||
        lsm_pci_vendor_records[low].id != vendor_id)
        return NULL;
    return name_at(lsm_pci_vendor_records[low].name_ref);
}

const char *lsm_pci_data_device_name(uint16_t vendor_id, uint16_t device_id)
{
    const uint32_t key = ((uint32_t)vendor_id << 16U) | (uint32_t)device_id;
    size_t low = 0U;
    size_t high =
        sizeof(lsm_pci_device_records) / sizeof(lsm_pci_device_records[0]);
    while (low < high) {
        const size_t middle = low + (high - low) / 2U;
        const uint32_t candidate = lsm_pci_device_records[middle].key;
        if (candidate < key)
            low = middle + 1U;
        else
            high = middle;
    }
    if (low >= sizeof(lsm_pci_device_records) /
                   sizeof(lsm_pci_device_records[0]) ||
        lsm_pci_device_records[low].key != key)
        return NULL;
    return name_at(lsm_pci_device_records[low].name_ref);
}
