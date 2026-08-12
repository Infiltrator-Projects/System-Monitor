// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pci_names_data.h
 * @brief Embedded PCI vendor and device name database.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PCI_NAMES_DATA_H
#define LINUX_SYSTEM_MONITOR_PCI_NAMES_DATA_H

#include <stddef.h>

/** Read-only chunks containing the generated compact PCI database. */
extern const char *const lsm_pci_names_chunks[];
/** Byte length of each generated PCI database chunk. */
extern const size_t lsm_pci_names_chunk_sizes[];
/** Number of generated PCI database chunks. */
extern const size_t lsm_pci_names_chunk_count;

#endif
