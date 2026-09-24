// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file history.h
 * @brief Public interface for persistent application resource history.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_HISTORY_H
#define INFILTRATOR_SYSTEM_MONITOR_HISTORY_H

#include "monitor_types.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Schedule persistent App History accounting state after the first GTK frame.
 *
 * Tracking is application lifetime state and remains independent of whether
 * the lazily constructed App History tab has ever been opened.
 *
 * @param [in,out] app Application that owns retained history state.
 */
void lsm_history_start(LsmApp *app);
/**
 * Construct the lazily created App History presentation.
 *
 * The retained accounting model is initialised independently; this function
 * creates only the GTK view and binds it to the already-running model.
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
/**
 * Initialise non-visual history state for the deterministic retention fixture.
 *
 * @param [in,out] app Zero-initialised application state owned by the fixture.
 * @param [in] config_dir Temporary configuration directory used by the fixture.
 * @return TRUE when history state and its persistence path were initialised.
 */
gboolean lsm_history_test_init(LsmApp *app, const char *config_dir);
/**
 * Return the number of retained application identities.
 *
 * @param [in] app Initialised fixture application.
 * @return Current retained application-identity count.
 */
guint lsm_history_test_retained_count(const LsmApp *app);
/**
 * Test whether one application-history identity is retained.
 *
 * @param [in] app Initialised fixture application.
 * @param [in] key Complete persisted application-history key.
 * @return TRUE when @p key is present in retained history.
 */
gboolean lsm_history_test_contains(const LsmApp *app, const char *key);
/**
 * Release fixture-owned history state without performing an implicit save.
 *
 * @param [in,out] app Fixture application whose history state is released.
 */
void lsm_history_test_dispose(LsmApp *app);
#endif

#endif
