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
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PERFORMANCE_SELECTION_H
#define LINUX_SYSTEM_MONITOR_PERFORMANCE_SELECTION_H

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

#endif
