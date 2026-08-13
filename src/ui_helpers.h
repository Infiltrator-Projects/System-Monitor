// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ui_helpers.h
 * @brief Shared GTK presentation helpers used by multiple tabs.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_UI_HELPERS_H
#define LINUX_SYSTEM_MONITOR_UI_HELPERS_H

#include <gtk/gtk.h>
#include <stddef.h>

/**
 * Compare current and proposed widget text under NULL-as-empty semantics.
 *
 * @param [in] current Existing label text, or NULL.
 * @param [in] next Proposed label text, or NULL.
 * @return TRUE only when a GTK property update is required.
 */
gboolean lsm_ui_text_needs_update(const char *current, const char *next);
/**
 * Format and conditionally apply label text.
 *
 * Formatting uses stack storage on the normal path. GTK is notified only when
 * the final text differs, avoiding redundant allocation, layout and redraw.
 *
 * @param [in,out] label GtkLabel-compatible widget.
 * @param [in] format printf-style format string followed by its arguments.
 * @return TRUE when the widget text changed; FALSE for invalid input or an
 *         unchanged formatted value.
 */
gboolean lsm_ui_set_label_text(GtkWidget *label, const char *format, ...)
    G_GNUC_PRINTF(2, 3);
/**
 * Present a modal, formatted error message through the GUI.
 *
 * @param [in] parent Optional transient parent window.
 * @param [in] title Dialog title.
 * @param [in] format printf-style message format followed by its arguments.
 */
void lsm_ui_show_error(GtkWindow *parent, const char *title,
                       const char *format, ...) G_GNUC_PRINTF(3, 4);
/**
 * Resolve the effective themed background colour for a widget.
 *
 * @param [in] widget Widget whose style context is queried.
 * @return Resolved RGBA colour, with an opaque neutral fallback on failure.
 */
GdkRGBA lsm_ui_background_colour(GtkWidget *widget);
/**
 * Perform the application's case-insensitive substring filter comparison.
 *
 * @param [in] text Candidate text, or NULL.
 * @param [in] needle Search text, or NULL/empty to match all candidates.
 * @return TRUE when @p needle is empty or occurs in @p text.
 */
gboolean lsm_ui_text_matches(const char *text, const char *needle);

#endif
