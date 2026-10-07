// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_shell.h
 * @brief Internal global-window chrome and navigation coordination API.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_SHELL_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_SHELL_H

#include "app.h"

/**
 * Apply the selected appearance mode.
 *
 * Follow-system mode keeps the host GTK palette authoritative. Day and Night
 * use the semantic palettes supplied by Infiltratr Common. The implementation
 * is isolated in app_theme.c rather than shell navigation code.
 *
 * @param [in,out] app Active application context.
 */
void lsm_app_shell_apply_theme(LsmApp *app);

/**
 * Build the branded System Settings-style client-side title bar.
 *
 * The header is installed as the real GTK window titlebar so window dragging,
 * title-bar double-click maximise/restore and native window-manager geometry
 * semantics remain intact while the product chrome is visually consistent.
 *
 * @param [in,out] app Active application context with a constructed window.
 * @return GTK header bar owned by the window after installation.
 */
GtkWidget *lsm_app_shell_build_header(LsmApp *app);

/**
 * Build the persistent graphical primary navigation rail.
 *
 * The rail replaces the notebook's visible text tabs while retaining the
 * notebook as the stable internal page container. Performance resource classes
 * are promoted into first-class destinations without changing device identity.
 *
 * @param [in,out] app Active application context.
 * @return GTK widget owning the primary navigation rail.
 */
GtkWidget *lsm_app_shell_build_navigation(LsmApp *app);

/**
 * Synchronise the graphical rail with the active tab/resource.
 *
 * @param [in,out] app Active application context.
 */
void lsm_app_shell_sync_navigation(LsmApp *app);

/**
 * Apply compact-summary visibility and window geometry policy.
 *
 * @param [in,out] app Active application context.
 */
void lsm_app_shell_apply_compact_summary(LsmApp *app);

/**
 * Persist one page's current vertical scroll position in runtime state.
 *
 * @param [in,out] app Active application context.
 * @param [in] page Current tab index to snapshot.
 */
void lsm_app_shell_save_page_scroll(LsmApp *app, gint page);

/**
 * Cancel deferred shell-only callbacks during shutdown and release theme state.
 *
 * @param [in,out] app Application context being shut down.
 */
void lsm_app_shell_cancel_pending(LsmApp *app);

#endif
