// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file service_backend.h
 * @brief Platform-neutral service inventory and control contract.
 *
 * The GTK Services page consumes plain service records and generic actions.
 * Native service-manager protocols remain in the selected platform backend.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_SERVICE_BACKEND_H
#define INFILTRATOR_SYSTEM_MONITOR_SERVICE_BACKEND_H

#include "monitor_types.h"

#include <gio/gio.h>
#include <stdbool.h>
#include <stddef.h>

/** One service record supplied by the active platform backend. */
typedef struct {
    char name[LSM_NAME_LEN];
    char description[256];
    char active[32];
    char substate[64];
    char startup[64];
} LsmServiceEntry;

/** Generic service actions implemented by each platform backend. */
typedef enum {
    LSM_SERVICE_ACTION_START,
    LSM_SERVICE_ACTION_STOP,
    LSM_SERVICE_ACTION_RESTART,
    LSM_SERVICE_ACTION_ENABLE,
    LSM_SERVICE_ACTION_DISABLE
} LsmServiceAction;

/**
 * Collect the current service inventory.
 *
 * @param [out] out_entries Receives a caller-owned array, possibly NULL when
 *             the service manager reports no entries.
 * @param [out] out_count Receives the number of entries in @p out_entries.
 * @param [in] cancellable Optional cancellation token for a worker call.
 * @param [out] error Optional native failure detail.
 * @return true when the service manager was queried successfully.
 */
bool lsm_service_backend_collect(LsmServiceEntry **out_entries,
                                 size_t *out_count,
                                 GCancellable *cancellable,
                                 GError **error);

/**
 * Apply one generic service action.
 *
 * @param [in] name Native service identity returned by the inventory.
 * @param [in] action Generic operation requested by the user.
 * @param [in] cancellable Optional cancellation token for a worker call.
 * @param [out] error Optional native failure detail.
 * @return true only when the service manager accepted the complete operation.
 */
bool lsm_service_backend_action(const char *name,
                                LsmServiceAction action,
                                GCancellable *cancellable,
                                GError **error);

/**
 * Interpret a backend startup-state string for presentation controls.
 *
 * @param [in] state Startup state returned in LsmServiceEntry.
 * @return true when the state means the service is enabled or linked.
 */
bool lsm_service_backend_state_is_enabled(const char *state);

/**
 * Release an inventory returned by lsm_service_backend_collect().
 *
 * @param [in,out] entries Array to release, or NULL.
 */
void lsm_service_backend_free(LsmServiceEntry *entries);

#endif
