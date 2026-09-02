// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file history.h
 * @brief Public interface for persistent application resource history.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_HISTORY_H
#define LINUX_SYSTEM_MONITOR_HISTORY_H

#include "monitor_types.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the persistent App History tab and load saved accounting state.
 *
 * @param [in,out] app Application that owns history models and hash tables.
 * @param [in] container Empty GTK container receiving the history view.
 */
void lsm_history_build(LsmApp *app, GtkWidget *container);
/**
 * Accumulate one process snapshot into per-application historical totals.
 *
 * @param [in,out] app Application containing retained history state.
 * @param [in] processes Current process rows.
 * @param [in] count Number of rows in @p processes.
 */
void lsm_app_history_ingest(LsmApp *app,
                            const LsmProcessInfo *processes,
                            size_t count);
/**
 * Rebuild the visible App History model from retained cumulative state.
 *
 * @param [in,out] app Application whose history view is updated.
 */
void lsm_history_refresh(LsmApp *app);
/**
 * Persist application history atomically beneath the user's config directory.
 *
 * @param [in] app Application containing state to save.
 */
void lsm_history_save(LsmApp *app);
/**
 * Flush pending history and release tab-owned state.
 *
 * @param [in,out] app Application being shut down.
 */
void lsm_history_destroy(LsmApp *app);

#ifdef LSM_HISTORY_TEST_API
gboolean lsm_history_test_init(LsmApp *app, const char *config_dir);
guint lsm_history_test_retained_count(const LsmApp *app);
gboolean lsm_history_test_contains(const LsmApp *app, const char *key);
void lsm_history_test_dispose(LsmApp *app);
#endif

#endif
