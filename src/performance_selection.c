// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_selection.c
 * @brief Re-entrancy guard for Performance side-pane selection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_selection.h"

bool lsm_performance_selection_begin(LsmPerformanceSelection *selection)
{
    if (!selection || selection->active) return false;
    selection->active = true;
    return true;
}

void lsm_performance_selection_end(LsmPerformanceSelection *selection)
{
    if (selection) selection->active = false;
}

bool lsm_performance_selection_active(
    const LsmPerformanceSelection *selection)
{
    return selection && selection->active;
}
