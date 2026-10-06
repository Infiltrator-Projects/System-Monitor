// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file summary_bar_adapter.c
 * @brief LsmApp adapter for the narrow Compact Summary renderer contract.
 *
 * This is intentionally the only Compact Summary translation unit allowed to
 * know the complete application layout. The renderer in summary_bar.c accepts
 * only its own widget state plus the monitor snapshot it needs to present.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "summary_bar.h"
#include "summary_bar_view.h"

#include "app_internal.h"

static void summary_view_from_app(const LsmApp *app, LsmSummaryBarView *view)
{
    if (!app || !view) return;
    view->frame = app->shell.summary_bar;
    view->values[LSM_SUMMARY_CPU] = app->shell.summary_cpu;
    view->values[LSM_SUMMARY_MEMORY] = app->shell.summary_memory;
    view->values[LSM_SUMMARY_DISK] = app->shell.summary_disk;
    view->values[LSM_SUMMARY_NETWORK] = app->shell.summary_network;
    view->values[LSM_SUMMARY_GPU] = app->shell.summary_gpu;
}

GtkWidget *lsm_summary_bar_build(LsmApp *app)
{
    if (!app) return NULL;

    LsmSummaryBarView view;
    GtkWidget *frame = lsm_summary_bar_view_build(&view);
    if (!frame) return NULL;

    app->shell.summary_bar = view.frame;
    app->shell.summary_cpu = view.values[LSM_SUMMARY_CPU];
    app->shell.summary_memory = view.values[LSM_SUMMARY_MEMORY];
    app->shell.summary_disk = view.values[LSM_SUMMARY_DISK];
    app->shell.summary_network = view.values[LSM_SUMMARY_NETWORK];
    app->shell.summary_gpu = view.values[LSM_SUMMARY_GPU];

    lsm_summary_bar_view_update(
        &view, &app->monitor, app->runtime.network_use_bits);
    return frame;
}

void lsm_summary_bar_update(LsmApp *app)
{
    if (!app || !app->shell.summary_bar) return;

    LsmSummaryBarView view;
    summary_view_from_app(app, &view);
    lsm_summary_bar_view_update(
        &view, &app->monitor, app->runtime.network_use_bits);
}
