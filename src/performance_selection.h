// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_selection.h
 * @brief Re-entrancy guard for Performance side-pane selection.
 *
 * GTK toggle-button state changes emit the same clicked signal used for direct
 * navigation. Updating the remaining buttons from that signal therefore needs
 * an explicit transaction boundary to prevent recursively alternating pages.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_SELECTION_H
#define INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_SELECTION_H

#include <stdbool.h>

/** State for one atomic Performance side-pane selection transaction. */
typedef struct {
    bool active;
} LsmPerformanceSelection;

/**
 * Begin a Performance selection transaction.
 *
 * @param [in,out] selection Selection state owned by the application.
 * @return true when the caller owns the transaction; false for a re-entrant
 *         callback or invalid state pointer.
 */
bool lsm_performance_selection_begin(LsmPerformanceSelection *selection);

/**
 * End a Performance selection transaction.
 *
 * @param [in,out] selection Selection state owned by the application.
 */
void lsm_performance_selection_end(LsmPerformanceSelection *selection);

/**
 * Report whether Performance selection is already being updated.
 *
 * @param [in] selection Selection state owned by the application.
 * @return true during an active transaction, otherwise false.
 */
bool lsm_performance_selection_active(
    const LsmPerformanceSelection *selection);

/**
 * Match a retained Performance selection against a rebuilt candidate page.
 *
 * Exact stack identity wins. When topology discovery promotes or regenerates
 * that presentation identity, a candidate of the same resource type may still
 * match through its semantic device identity.
 *
 * @param [in] saved_stack_name Previously visible GtkStack child name.
 * @param [in] saved_identity Stable semantic identity retained by the page.
 * @param [in] candidate_stack_name Candidate GtkStack child name.
 * @param [in] candidate_identity Candidate semantic device identity.
 * @param [in] same_type Whether saved and candidate pages represent the same
 *            Performance resource type.
 * @return true when the candidate represents the retained selection.
 */
bool lsm_performance_selection_matches(
    const char *saved_stack_name,
    const char *saved_identity,
    const char *candidate_stack_name,
    const char *candidate_identity,
    bool same_type);

#endif
