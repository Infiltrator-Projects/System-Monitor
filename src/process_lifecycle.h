// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_lifecycle.h
 * @brief Process-domain construction, background startup and teardown.
 *
 * The application composition root owns ordering only. Process scanner,
 * filters, application-catalog enrichment and retained process-domain storage
 * are initialised and released through this feature boundary.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_LIFECYCLE_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_LIFECYCLE_H

#include <stdbool.h>

typedef struct LsmApp LsmApp;

/**
 * Initialise process-domain resources required before the shell is built.
 * @param app Application receiving process scanner and persisted filters.
 * @return true when all required process resources were created.
 */
bool lsm_process_lifecycle_init(LsmApp *app);

/**
 * Start process-domain work that requires the completed application shell.
 * @param app Application whose scanner callback and catalogue load are started.
 */
void lsm_process_lifecycle_start(LsmApp *app);

/**
 * Quiesce and release all process-domain resources owned by the application.
 * @param app Application whose process scanner, snapshots and filters are freed.
 */
void lsm_process_lifecycle_destroy(LsmApp *app);

#endif
