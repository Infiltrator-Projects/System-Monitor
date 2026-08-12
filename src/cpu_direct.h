// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file cpu_direct.h
 * @brief Direct processor identity and topology queries.
 *
 * On x86, immutable processor information is read with CPUID rather than
 * through procfs or sysfs. Other architectures return false so the portable
 * kernel-interface fallback remains available.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_CPU_DIRECT_H
#define LINUX_SYSTEM_MONITOR_CPU_DIRECT_H

#include <stdbool.h>

#include "monitor_types.h"

/**
 * Populate immutable CPU identity, topology and cache information directly.
 *
 * CPUID is used where the architecture permits it; portable kernel interfaces
 * provide conservative fallbacks on other architectures.
 *
 * @param [in,out] cpu CPU snapshot receiving static fields.
 * @return true when at least a usable processor identity/topology was obtained.
 */
bool lsm_cpu_direct_read_static(LsmCpuInfo *cpu);

#endif
