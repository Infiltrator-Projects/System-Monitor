// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_runtime.h
 * @brief Internal refresh-cadence and timer coordination API.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_APP_RUNTIME_H
#define LINUX_SYSTEM_MONITOR_APP_RUNTIME_H

#include "app.h"

/** Refresh the shared process snapshot when its effective cadence is due. */
gboolean lsm_app_refresh_processes_if_due(LsmApp *app, gboolean force);
/** Refresh all user-visible inventories once, preserving pause state. */
void lsm_app_refresh_all(LsmApp *app);
/** Start application-owned recurring refresh sources. */
void lsm_app_runtime_start(LsmApp *app);
/** Stop and clear application-owned recurring refresh sources. */
void lsm_app_runtime_stop(LsmApp *app);

#endif
