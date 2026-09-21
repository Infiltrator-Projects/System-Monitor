// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pressure.h
 * @brief Linux pressure-stall parsing and bounded procfs collection.
 *
 * Pressure Stall Information is a Linux kernel interface that reports the
 * share of wall time in which runnable work is delayed by CPU, memory or I/O
 * contention. This module keeps the parser independent of GTK and publishes
 * only the plain monitor-model representation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PRESSURE_H
#define LINUX_SYSTEM_MONITOR_PRESSURE_H

#include "monitor_types.h"

#include <stdbool.h>

/**
 * Parse one complete PSI text record.
 *
 * The mandatory "some" line must contain avg10, avg60, avg300 and total.
 * The optional "full" line uses the same grammar. Unknown future key/value
 * fields are ignored, while malformed known fields reject their line.
 *
 * @param [in] text NUL-terminated procfs PSI contents.
 * @param [out] pressure Parsed pressure snapshot, cleared on failure.
 * @return true when a complete "some" line was parsed.
 */
bool lsm_pressure_parse(const char *text, LsmPressureInfo *pressure);

/**
 * Read and parse one PSI procfs file.
 *
 * @param [in] path Path such as /proc/pressure/cpu.
 * @param [out] pressure Parsed pressure snapshot, cleared when unavailable.
 * @return true when PSI data was read and parsed.
 */
bool lsm_pressure_read(const char *path, LsmPressureInfo *pressure);

#endif
