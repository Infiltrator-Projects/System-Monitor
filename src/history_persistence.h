// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file history_persistence.h
 * @brief Durable App History snapshot serialization and publication.
 *
 * This module owns immutable save requests, generation ordering and atomic
 * publication. The History model supplies plain records and remains unaware of
 * persistence-worker locks, serialization buffers and stale-write suppression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_HISTORY_PERSISTENCE_H
#define INFILTRATOR_SYSTEM_MONITOR_HISTORY_PERSISTENCE_H

#include "process_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct LsmHistorySaveCoordinator;
typedef struct LsmHistorySaveRequest LsmHistorySaveRequest;

/** Plain immutable record projected from the in-memory History model. */
typedef struct {
    const char *key;
    const char *name;
    const char *user;
    const char *identity;
    double cpu_seconds;
    double active_seconds;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t peak_rss_bytes;
    int64_t first_seen;
    int64_t last_seen;
} LsmHistoryPersistenceRecord;

/** Create generation-ordering state shared by detached save requests. */
struct LsmHistorySaveCoordinator *
lsm_history_persistence_coordinator_create(void);

/** Retain a coordinator for one immutable worker request. */
void lsm_history_persistence_coordinator_retain(
    struct LsmHistorySaveCoordinator *coordinator);

/** Release one coordinator reference. */
void lsm_history_persistence_coordinator_release(
    struct LsmHistorySaveCoordinator *coordinator);

/** Record the newest generation whose persistence has been scheduled. */
void lsm_history_persistence_note_scheduled(
    struct LsmHistorySaveCoordinator *coordinator, unsigned generation);

/** Return the newest generation durably published by this coordinator. */
unsigned lsm_history_persistence_latest_written(
    const struct LsmHistorySaveCoordinator *coordinator);

/**
 * Snapshot model records into a self-contained save request.
 *
 * Strings are copied and normalised for the tab-separated on-disk contract;
 * callers may release or mutate their source records immediately after return.
 */
LsmHistorySaveRequest *lsm_history_persistence_request_create(
    struct LsmHistorySaveCoordinator *coordinator,
    const char *config_dir, const char *path,
    const LsmHistoryPersistenceRecord *records, size_t count,
    unsigned generation);

/** Release an immutable save request. NULL is accepted. */
void lsm_history_persistence_request_free(LsmHistorySaveRequest *request);

/** Return the generation captured by a save request. */
unsigned lsm_history_persistence_request_generation(
    const LsmHistorySaveRequest *request);

/**
 * Serialize and atomically publish one immutable request.
 *
 * Older or duplicate generations are deliberately skipped without error.
 * @param request Immutable request produced by request_create().
 * @param written Receives true only when this call performed the durable write.
 * @return zero on success/intentional skip, otherwise an errno-style failure.
 */
int lsm_history_persistence_request_write(
    LsmHistorySaveRequest *request, bool *written);

#endif
