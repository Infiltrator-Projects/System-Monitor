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
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_MONITOR_H
#define INFILTRATOR_SYSTEM_MONITOR_MONITOR_H

#include "monitor_types.h"

/**
 * Initialise collectors and start discovery and rate-baseline collection.
 *
 * Partial optional telemetry is acceptable; the function fails only when the
 * core monitor context cannot be made usable. The caller should pass a zeroed
 * object and must call lsm_monitor_destroy() after any successful return.
 *
 * @param monitor Caller-owned zeroed monitoring model.
 * @return true when core monitoring accepts updates. Linux device discovery
 *         continues asynchronously; this does not guarantee a full sample.
 */
bool lsm_monitor_init(LsmMonitor *monitor);

/**
 * Request sampling and publish the latest completed snapshot, if one exists.
 *
 * Fast and slow cadences are coordinated internally. Each metric's availability
 * and baseline contract governs failures without invalidating unrelated data.
 *
 * Linux queues work without waiting for native I/O. A successful call may
 * retain the preceding public snapshot while the worker is still collecting.
 *
 * @param monitor Initialised model to update in place.
 * @return true when the backend accepted the update; optional field failures
 *         do not by themselves make the whole cycle fail.
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
 * Relinquish ownership of adapters, workers and retained baselines.
 *
 * Linux waits briefly for its sampler, then lets an outstanding worker own
 * cleanup. The caller may release the public model immediately on return;
 * blocked native resources may outlive it until collection finishes or exit.
 *
 * @param monitor Initialised or partially initialised model; NULL is accepted.
 */
void lsm_monitor_destroy(LsmMonitor *monitor);

#endif
