// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file memory_hardware.h
 * @brief Native unprivileged RAM-module discovery.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_MEMORY_HARDWARE_H
#define LINUX_SYSTEM_MONITOR_MEMORY_HARDWARE_H

#include "monitor_types.h"
#include "smbios_memory.h"

#include <stdbool.h>

/**
 * Read SMBIOS memory-device information directly under current credentials.
 *
 * @param [in,out] memory Memory snapshot receiving slot and module details.
 * @return true when a valid SMBIOS table was parsed and applied.
 */
bool lsm_memory_hardware_read_direct(LsmMemoryInfo *memory);
/**
 * Apply a parsed SMBIOS summary to the public memory snapshot.
 *
 * @param [in,out] memory Destination snapshot.
 * @param [in] source Parsed module/slot summary.
 */
void lsm_memory_hardware_apply(LsmMemoryInfo *memory,
                               const LsmSmbiosMemoryInfo *source);

#endif
