// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_selection.c
 * @brief Re-entrancy guard for Performance side-pane selection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_selection.h"

#include <string.h>

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

bool lsm_performance_selection_matches(
    const char *saved_stack_name,
    const char *saved_identity,
    const char *candidate_stack_name,
    const char *candidate_identity,
    bool same_type)
{
    if (!candidate_stack_name || !*candidate_stack_name) return false;

    if (saved_stack_name && *saved_stack_name &&
        strcmp(saved_stack_name, candidate_stack_name) == 0)
        return true;

    return same_type &&
           saved_identity && *saved_identity &&
           candidate_identity && *candidate_identity &&
           strcmp(saved_identity, candidate_identity) == 0;
}
