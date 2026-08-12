// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor.h
 * @brief Lifecycle API for the complete native monitoring snapshot.
 *
 * LsmMonitor is a retained model rather than a disposable sample. Public
 * snapshot fields stay plain C while platform collector resources and cadence
 * state are retained behind its opaque backend-state pointer. The lifecycle is
 * therefore strict:
 * lsm_monitor_init(), zero or more lsm_monitor_update() calls, then
 * lsm_monitor_destroy(). A monitor must not be copied after initialisation.
 *
 * The API is serial and GTK-independent. The owning application thread may
 * present the resulting plain-C fields after each successful update.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_MONITOR_H
#define LINUX_SYSTEM_MONITOR_MONITOR_H

#include "monitor_types.h"

/**
 * Discover devices, initialise collectors and establish rate baselines.
 *
 * Partial optional telemetry is acceptable; the function fails only when the
 * core monitor context cannot be made usable. The caller should pass a zeroed
 * object and must call lsm_monitor_destroy() after any successful return.
 *
 * @param monitor Caller-owned zeroed monitoring model.
 * @return true when core monitoring is ready for updates.
 */
bool lsm_monitor_init(LsmMonitor *monitor);

/**
 * Refresh every due category and calculate rates from prior samples.
 *
 * Fast and slow cadences are coordinated internally. Individual unavailable
 * metrics are cleared or marked unavailable without invalidating unrelated
 * categories.
 *
 * @param monitor Initialised model to update in place.
 * @return true when the update cycle completed; optional field failures do not
 *         by themselves make the whole cycle fail.
 */
bool lsm_monitor_update(LsmMonitor *monitor);

/**
 * Request a device-topology refresh on the next monitoring update.
 *
 * The request is platform-neutral: callers need not know the backend cadence,
 * timestamps or discovery mechanism used to satisfy it.
 *
 * @param monitor Initialised monitoring model; NULL is accepted.
 */
void lsm_monitor_request_topology_refresh(LsmMonitor *monitor);

/**
 * Update aggregate process/thread totals from the shared detailed snapshot.
 *
 * Reusing the Processes snapshot avoids a second procfs walk solely for the CPU
 * page counters.
 *
 * @param monitor Initialised monitoring model.
 * @param processes Current process array, or NULL when @p process_count is zero.
 * @param process_count Number of rows in @p processes.
 */
void lsm_monitor_set_process_totals(LsmMonitor *monitor,
                                    const LsmProcessInfo *processes,
                                    size_t process_count);

/**
 * Release every adapter, descriptor, worker and retained baseline.
 *
 * @param monitor Initialised or partially initialised model; NULL is accepted.
 */
void lsm_monitor_destroy(LsmMonitor *monitor);

#endif
