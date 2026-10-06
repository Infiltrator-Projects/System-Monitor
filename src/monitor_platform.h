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
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_MONITOR_PLATFORM_H
#define INFILTRATOR_SYSTEM_MONITOR_MONITOR_PLATFORM_H

#include "monitor_types.h"

/**
 * Initialise the native monitoring implementation and populate initial data.
 *
 * @param [in,out] monitor Caller-owned zeroed snapshot.
 * @return true when the platform backend is usable.
 */
bool lsm_monitor_platform_init(LsmMonitor *monitor);

/**
 * Request native sampling and publish completed data without requiring I/O
 * to finish synchronously. Linux may retain the preceding public snapshot.
 *
 * @param [in,out] monitor Initialised snapshot to update.
 * @return true when the backend accepted the update request.
 */
bool lsm_monitor_platform_update(LsmMonitor *monitor);

/**
 * Ask the native backend to rediscover device topology on its next update.
 *
 * @param [in,out] monitor Initialised snapshot; NULL is accepted.
 */
void lsm_monitor_platform_request_topology_refresh(LsmMonitor *monitor);

/**
 * Release caller ownership of the native monitoring implementation. A backend
 * may defer cleanup to an outstanding worker with independent lifetime.
 *
 * @param [in,out] monitor Initialised or partially initialised snapshot.
 */
void lsm_monitor_platform_destroy(LsmMonitor *monitor);

#endif
