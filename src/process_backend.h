// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_backend.h
 * @brief Platform-neutral process enumeration, accounting and control contract.
 *
 * The application owns one opaque backend context. Native implementations map
 * operating-system process identifiers, scheduler controls and instance-reuse
 * protection into the common process model without exposing those mechanisms
 * to presentation code.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_BACKEND_H
#define LINUX_SYSTEM_MONITOR_PROCESS_BACKEND_H

#include "process_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Opaque state retained across process samples. */
typedef struct LsmProcessBackend LsmProcessBackend;

/**
 * Allocate an independent retained-sample context.
 *
 * @return Newly allocated context, or NULL when allocation or platform
 *         initialisation fails.
 */
LsmProcessBackend *lsm_process_backend_create(void);

/**
 * Release a context and all retained process and identity caches.
 *
 * @param backend Context returned by lsm_process_backend_create(), or NULL.
 */
void lsm_process_backend_destroy(LsmProcessBackend *backend);

/**
 * Calculate one process's share of total computer CPU capacity.
 *
 * @param [in] process_delta Process CPU-accounting units since the prior sample.
 * @param [in] system_delta Aggregate machine CPU-accounting units in that sample.
 * @return Percentage of total computer capacity in the range 0..100.
 */
double lsm_process_cpu_total_percent(uint64_t process_delta,
                                     uint64_t system_delta);

/**
 * Enumerate processes and calculate per-process CPU and I/O rates.
 *
 * @param backend Retained-sample context owned by the caller.
 * @param out_processes Receives the caller-owned process array.
 * @param scan_flags Bitwise OR of LSM_PROCESS_SCAN_* enrichment flags.
 * @return Number of valid rows in @p out_processes; zero on failure or no rows.
 */
size_t lsm_process_scan(LsmProcessBackend *backend,
                        LsmProcessInfo **out_processes,
                        unsigned scan_flags);

/**
 * Verify that a process identifier still denotes the expected instance.
 *
 * @param pid Process identifier captured in a process snapshot.
 * @param instance_id Opaque instance token captured with @p pid.
 * @return true only while both values still identify the same process.
 */
bool lsm_process_identity_matches(LsmProcessId pid,
                                  LsmProcessInstanceId instance_id);

/**
 * Populate expensive optional fields for one existing process row.
 *
 * The row's PID and opaque instance token are checked before and after the
 * optional reads so PID recycling cannot attach another process's details.
 *
 * @param pid Process identifier represented by @p process.
 * @param process Row to enrich in place.
 * @param scan_flags Bitwise OR of supported LSM_PROCESS_SCAN_* enrichment flags.
 * @return true only when the same process instance survives the enrichment.
 */
bool lsm_process_enrich(LsmProcessId pid, LsmProcessInfo *process,
                        unsigned scan_flags);

/**
 * Set a user-facing scheduler priority.
 *
 * @param pid Target process identifier.
 * @param instance_id Opaque instance token captured with @p pid.
 * @param priority Platform-neutral priority requested by the user.
 * @return true when the native scheduler accepted the translated priority.
 */
bool lsm_process_set_priority(LsmProcessId pid,
                              LsmProcessInstanceId instance_id,
                              LsmProcessPriority priority);

/**
 * Apply or remove the application's efficiency scheduling policy.
 *
 * @param pid Target process identifier.
 * @param instance_id Opaque instance token captured with @p pid.
 * @param enabled true to enable the lower-resource policy; false for defaults.
 * @return true when at least one supported native efficiency control succeeded.
 */
bool lsm_process_set_efficiency(LsmProcessId pid,
                                LsmProcessInstanceId instance_id,
                                bool enabled);

/**
 * Read the process CPU-affinity mask into a caller-owned boolean array.
 *
 * @param pid Target process identifier.
 * @param instance_id Opaque instance token captured with @p pid.
 * @param enabled Receives one boolean per reported processor.
 * @param capacity Number of elements available in @p enabled.
 * @return Number of populated entries; zero on invalid input or backend failure.
 */
size_t lsm_process_affinity_get(LsmProcessId pid,
                                LsmProcessInstanceId instance_id,
                                bool *enabled, size_t capacity);

/**
 * Replace the process CPU-affinity mask from a boolean array.
 *
 * @param pid Target process identifier.
 * @param instance_id Opaque instance token captured with @p pid.
 * @param enabled Desired per-processor membership mask.
 * @param count Number of entries in @p enabled.
 * @return true when the active platform accepted the affinity mask.
 */
bool lsm_process_affinity_set(LsmProcessId pid,
                              LsmProcessInstanceId instance_id,
                              const bool *enabled, size_t count);

/**
 * Apply a control action to a process and its current descendants.
 *
 * Descendants are handled deepest-first where the native platform can provide
 * that relationship, reducing parent respawn races during tree termination.
 *
 * @param root_pid Root process identifier.
 * @param root_instance_id Opaque instance token captured with @p root_pid.
 * @param action Platform-neutral control action.
 * @return true when every extant target accepted the requested action.
 */
bool lsm_process_control_tree(LsmProcessId root_pid,
                              LsmProcessInstanceId root_instance_id,
                              LsmProcessControl action);

/**
 * Apply one control action to a process.
 *
 * @param pid Target process identifier.
 * @param instance_id Opaque instance token captured with @p pid.
 * @param action Platform-neutral control action.
 * @return true when the active platform accepted the requested action.
 */
bool lsm_process_control(LsmProcessId pid,
                         LsmProcessInstanceId instance_id,
                         LsmProcessControl action);

/**
 * Format the native failure from the immediately preceding process operation.
 *
 * @param buffer Destination for a human-readable message.
 * @param size Capacity of @p buffer in bytes.
 */
void lsm_process_error_message(char *buffer, size_t size);

/**
 * Free an array returned by lsm_process_scan().
 *
 * @param processes Array to release, or NULL.
 */
void lsm_process_list_free(LsmProcessInfo *processes);

#endif
