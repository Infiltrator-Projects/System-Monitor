// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_platform.h
 * @brief Internal operating-system monitor backend contract.
 *
 * The public monitor lifecycle is intentionally independent of Linux. Exactly
 * one native implementation of this contract is selected by the build. The
 * implementation owns all operating-system discovery, sampling cadence,
 * retained counter baselines and native resource handles while updating only
 * the plain-C LsmMonitor snapshot supplied by the caller.
 *
 * A future platform port therefore replaces this implementation boundary
 * rather than changing monitor.c or presentation code.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_MONITOR_PLATFORM_H
#define LINUX_SYSTEM_MONITOR_MONITOR_PLATFORM_H

#include "monitor_types.h"

/**
 * Initialise the native monitoring implementation and populate initial data.
 *
 * @param [in,out] monitor Caller-owned zeroed snapshot.
 * @return true when the platform backend is usable.
 */
bool lsm_monitor_platform_init(LsmMonitor *monitor);

/**
 * Perform one native sampling cycle.
 *
 * @param [in,out] monitor Initialised snapshot to update.
 * @return true when the platform update cycle completed.
 */
bool lsm_monitor_platform_update(LsmMonitor *monitor);

/**
 * Ask the native backend to rediscover device topology on its next update.
 *
 * @param [in,out] monitor Initialised snapshot; NULL is accepted.
 */
void lsm_monitor_platform_request_topology_refresh(LsmMonitor *monitor);

/**
 * Release every resource owned by the native monitoring implementation.
 *
 * @param [in,out] monitor Initialised or partially initialised snapshot.
 */
void lsm_monitor_platform_destroy(LsmMonitor *monitor);

#endif
