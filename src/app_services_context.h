// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_services_context.h
 * @brief Narrow application bridge for Services-page orchestration.
 *
 * Services owns its feature state directly. This bridge exposes only that
 * state plus the small amount of shell/runtime policy needed for refresh and
 * action completion, avoiding access to the complete private LsmApp layout.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_SERVICES_CONTEXT_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_SERVICES_CONTEXT_H

#include "app.h"
#include "services_state.h"

#include <stdbool.h>

/** Runtime and shell facts permitted to Services orchestration. */
typedef struct {
    GtkWidget *window;
    GtkWidget *notebook;
    gboolean paused;
    gboolean shutting_down;
} LsmServicesControlView;

/**
 * Return the feature-owned Services state.
 * @param app Application owning Services state.
 * @return Mutable Services state, or NULL when app is NULL.
 */
LsmServicesState *lsm_app_services_state(LsmApp *app);

/**
 * Copy the shell/runtime facts needed by Services callbacks.
 * @param app Application owning shell and runtime state.
 * @param view Destination receiving the narrow Services control view.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_app_services_control_view(const LsmApp *app,
                                   LsmServicesControlView *view);

/**
 * Register the Services page scroller with retained page-scroll state.
 * @param app Application owning top-level page state.
 * @param scroller Services page scrolled window.
 */
void lsm_app_services_set_scroller(LsmApp *app, GtkWidget *scroller);

#endif
