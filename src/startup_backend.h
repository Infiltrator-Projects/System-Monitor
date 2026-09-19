// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file startup_backend.h
 * @brief Platform-neutral startup-application inventory and control contract.
 *
 * Presentation receives plain startup records while native discovery, override
 * semantics and durable writes remain in the selected platform backend.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_STARTUP_BACKEND_H
#define LINUX_SYSTEM_MONITOR_STARTUP_BACKEND_H

#include "monitor_types.h"

#include <gio/gio.h>
#include <stdbool.h>
#include <stddef.h>

/** One startup application supplied by the active platform backend. */
typedef struct {
    char id[LSM_NAME_LEN];
    char source_identity[LSM_PATH_LEN];
    char origin_identity[LSM_PATH_LEN];
    char name[LSM_NAME_LEN];
    char command[1024];
    char description[512];
    bool user_entry;
    bool enabled;
} LsmStartupEntry;

/**
 * Collect the effective startup-application inventory.
 *
 * @param [out] out_entries Receives a caller-owned array.
 * @param [out] out_count Receives the number of records in @p out_entries.
 * @return true when the inventory was collected, including an empty inventory.
 */
bool lsm_startup_backend_collect(LsmStartupEntry **out_entries,
                                 size_t *out_count);

/**
 * Change one startup entry using the platform's reversible override semantics.
 *
 * @param [in] id Stable startup-entry identity.
 * @param [in] source_identity Opaque source token returned by the backend.
 * @param [in] origin_identity Opaque origin token returned by the backend.
 * @param [in] enabled Desired effective state.
 * @param [out] error Optional native failure detail.
 * @return true when the durable state was updated.
 */
bool lsm_startup_backend_set_enabled(const char *id,
                                     const char *source_identity,
                                     const char *origin_identity,
                                     bool enabled,
                                     GError **error);

/**
 * Build a URI for the location containing one startup entry.
 *
 * @param [in] source_identity Opaque source token returned by the backend.
 * @return Newly allocated URI for presentation, or NULL on failure.
 */
char *lsm_startup_backend_location_uri(const char *source_identity);

/**
 * Release an inventory returned by lsm_startup_backend_collect().
 *
 * @param [in,out] entries Array to release, or NULL.
 */
void lsm_startup_backend_free(LsmStartupEntry *entries);

#endif
