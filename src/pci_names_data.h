// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pci_names_data.h
 * @brief Generated lookup surface for System Monitor's normalized PCI facts.
 *
 * The generated representation deliberately exposes lookup operations rather
 * than the source registry's storage layout. Normal runtime lookup therefore
 * does not parse, scan or reproduce an external PCI database format.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PCI_NAMES_DATA_H
#define INFILTRATOR_SYSTEM_MONITOR_PCI_NAMES_DATA_H

#include <stdint.h>

/** Return the normalized vendor name for @p vendor_id, or NULL when unknown. */
const char *lsm_pci_data_vendor_name(uint16_t vendor_id);

/** Return the normalized direct-device name for an exact vendor/device pair. */
const char *lsm_pci_data_device_name(uint16_t vendor_id, uint16_t device_id);

#endif
