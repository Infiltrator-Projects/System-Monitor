// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pci_names.h
 * @brief In-process PCI vendor and device-name lookup.
 *
 * Linux-System-Monitor uses this module directly from its normal backend.
 * No helper executable, lspci, pciutils or lshw process is involved.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PCI_NAMES_H
#define LINUX_SYSTEM_MONITOR_PCI_NAMES_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Resolve numeric PCI identifiers through the compiled-in names database.
 *
 * @param [in] vendor_id Four-digit hexadecimal vendor identifier.
 * @param [in] device_id Four-digit hexadecimal device identifier.
 * @param [out] vendor Destination for the vendor name.
 * @param [in] vendor_size Capacity of @p vendor in bytes.
 * @param [out] product Destination for the device/product name.
 * @param [in] product_size Capacity of @p product in bytes.
 * @return true when at least the vendor identifier was found.
 */
bool lsm_pci_names_lookup(const char *vendor_id,
                          const char *device_id,
                          char *vendor,
                          size_t vendor_size,
                          char *product,
                          size_t product_size);

#endif
