// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_presentation_context.h
 * @brief Narrow private-state bridge for presentation-facing coordinators.
 *
 * Presentation modules need small, stable views of application-owned state,
 * not the complete private LsmApp layout. This contract keeps coordinators,
 * leaf dialogs and diagnostic exporters independent of unrelated subsystem
 * storage while leaving ownership in the application composition root.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_PRESENTATION_CONTEXT_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_PRESENTATION_CONTEXT_H

#include "app.h"
#include "app_config.h"
#include "monitor_types.h"

#include <glib.h>
#include <stdbool.h>
#include <stddef.h>

/** Widget/runtime facts permitted to the top-level keyboard command router. */
typedef struct {
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *processes_tree;
    GtkWidget *details_tree;
    GtkWidget *performance_stack;
    GtkWidget *pause_menu_item;
    gboolean paused;
} LsmKeyboardControlView;

/** Runtime facts permitted to the top-level page descriptor registry. */
typedef struct {
    LsmTabIndex active_tab;
    guint filesystem_update_interval_ms;
    gboolean shutting_down;
    gboolean shell_shown;
} LsmPageRegistryControlView;

/** Retained plain-C data permitted to the diagnostic snapshot exporter. */
typedef struct {
    const LsmMonitor *monitor;
    const LsmProcessInfo *processes;
    size_t process_count;
} LsmSystemSnapshotView;

/**
 * Copy keyboard-routing state out of the private application object.
 * @param app Application owning the presentation state.
 * @param view Destination for the narrow keyboard view.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_app_keyboard_control_view(const LsmApp *app,
                                   LsmKeyboardControlView *view);

/**
 * Copy page-registry runtime state out of the private application object.
 * @param app Application owning runtime state.
 * @param view Destination for the narrow registry view.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_app_page_registry_control_view(const LsmApp *app,
                                        LsmPageRegistryControlView *view);

/**
 * Return the main presentation window without exposing the shell layout.
 * @param app Application owning the shell.
 * @return Main window widget, or NULL when unavailable.
 */
GtkWidget *lsm_app_presentation_window(const LsmApp *app);

/**
 * Return the currently open Help window, if any.
 * @param app Application owning Help-window lifetime.
 * @return Help window widget, or NULL when no Help window is open.
 */
GtkWidget *lsm_app_help_window(const LsmApp *app);

/**
 * Update the application-owned Help-window reference.
 * @param app Application owning Help-window lifetime.
 * @param window Current Help window, or NULL after destruction.
 */
void lsm_app_help_set_window(LsmApp *app, GtkWidget *window);

/**
 * Copy the completed monitor and process data needed by snapshot export.
 * @param app Application owning the retained completed snapshots.
 * @param view Destination for the narrow diagnostic-export view.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_app_system_snapshot_view(const LsmApp *app,
                                  LsmSystemSnapshotView *view);

/**
 * Query whether one top-level page has been constructed.
 * @param app Application owning page lifetime state.
 * @param page Stable page identity.
 * @return true only when the page identity is valid and already built.
 */
bool lsm_app_page_registry_page_built(const LsmApp *app, LsmTabIndex page);

/**
 * Return the search entry owned by a searchable page.
 * @param app Application owning page widgets.
 * @param page Stable page identity.
 * @return Search widget, or NULL when the page has no search surface.
 */
GtkWidget *lsm_app_page_registry_search_target(const LsmApp *app,
                                               LsmTabIndex page);

/**
 * Return the top-level notebook used by page navigation.
 * @param app Application owning the shell.
 * @return Notebook widget, or NULL when unavailable.
 */
GtkWidget *lsm_app_page_registry_notebook(const LsmApp *app);

/**
 * Return the top-level application window.
 * @param app Application owning the shell.
 * @return Window widget, or NULL when unavailable.
 */
GtkWidget *lsm_app_page_registry_window(const LsmApp *app);

/**
 * Update active and last-page runtime identity together.
 * @param app Application owning navigation state.
 * @param page New active page.
 */
void lsm_app_page_registry_set_active(LsmApp *app, LsmTabIndex page);

/**
 * Ensure one page has been lazily constructed.
 * @param app Application owning page state.
 * @param page Page that must exist before entry policy runs.
 */
void lsm_app_page_registry_ensure_built(LsmApp *app, LsmTabIndex page);

/**
 * Restore one page's retained vertical scroll position.
 * @param app Application owning page scroll state.
 * @param page Page whose scroller should be restored.
 */
void lsm_app_page_registry_restore_scroll(LsmApp *app, LsmTabIndex page);

/**
 * Synchronise Overview-integrated menu and summary chrome.
 * @param app Application owning shell and runtime presentation state.
 */
void lsm_app_page_registry_sync_overview_chrome(LsmApp *app);

#endif
