// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file summary_bar_view.h
 * @brief Narrow Compact Summary renderer contract independent of LsmApp.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_SUMMARY_BAR_VIEW_H
#define INFILTRATOR_SYSTEM_MONITOR_SUMMARY_BAR_VIEW_H

#include "monitor_types.h"
#include "presentation_contract.h"

#include <gtk/gtk.h>
#include <stdbool.h>

/** Widget ownership for the Compact Summary renderer. */
typedef struct {
    GtkWidget *frame;
    GtkWidget *values[LSM_SUMMARY_COUNT];
} LsmSummaryBarView;

/**
 * Construct the Compact Summary widget tree without application-global state.
 *
 * @param [out] view Renderer-owned widget references initialised by the call.
 * @return Newly created summary frame, or NULL for invalid input.
 */
GtkWidget *lsm_summary_bar_view_build(LsmSummaryBarView *view);

/**
 * Present one retained monitor snapshot through an existing summary view.
 *
 * @param [in,out] view Constructed Compact Summary renderer state.
 * @param [in] monitor Current platform-neutral monitor snapshot, or NULL.
 * @param [in] network_use_bits Present network throughput in bits when true.
 */
void lsm_summary_bar_view_update(LsmSummaryBarView *view,
                                 const LsmMonitor *monitor,
                                 bool network_use_bits);

#endif
