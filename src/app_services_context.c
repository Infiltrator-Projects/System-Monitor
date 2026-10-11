// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_services_context.c
 * @brief Private LsmApp bridge for Services-page orchestration.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_services_context.h"

#include "app_config.h"
#include "app_internal.h"

LsmServicesState *lsm_app_services_state(LsmApp *app)
{
    return app ? &app->services : NULL;
}

bool lsm_app_services_control_view(const LsmApp *app,
                                   LsmServicesControlView *view)
{
    if (!app || !view) return false;
    view->window = app->shell.window;
    view->notebook = app->shell.notebook;
    view->paused = app->runtime.paused;
    view->shutting_down = app->runtime.shutting_down;
    return true;
}

void lsm_app_services_set_scroller(LsmApp *app, GtkWidget *scroller)
{
    if (!app) return;
    app->runtime.page_scrollers[LSM_TAB_SERVICES] = scroller;
}
