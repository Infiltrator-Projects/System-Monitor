// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file summary_bar_adapter.c
 * @brief LsmApp adapter for the narrow Compact Summary renderer contract.
 *
 * The adapter consumes only the presentation context exposed by the
 * application bridge. The renderer in summary_bar.c remains independent of
 * the complete private application layout.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "summary_bar.h"
#include "summary_bar_view.h"

#include "app_presentation_context.h"

GtkWidget *lsm_summary_bar_build(LsmApp *app)
{
    if (!app) return NULL;

    LsmSummaryBarView view;
    GtkWidget *frame = lsm_summary_bar_view_build(&view);
    if (!frame) return NULL;

    lsm_app_summary_bar_store_view(app, &view);

    LsmSummaryBarContext context;
    if (lsm_app_summary_bar_context(app, &context))
        lsm_summary_bar_view_update(
            &context.view, context.monitor, context.network_use_bits);
    return frame;
}

void lsm_summary_bar_update(LsmApp *app)
{
    LsmSummaryBarContext context;
    if (!lsm_app_summary_bar_context(app, &context) || !context.view.frame)
        return;

    lsm_summary_bar_view_update(
        &context.view, context.monitor, context.network_use_bits);
}
