// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ui_helpers.c
 * @brief Shared GTK presentation helpers.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "ui_helpers.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


gboolean lsm_ui_text_needs_update(const char *current, const char *next)
{
    if (!current) current = "";
    if (!next) next = "";
    return strcmp(current, next) != 0;
}

gboolean lsm_ui_set_label_text(GtkWidget *label, const char *format, ...)
{
    if (!label || !format) return FALSE;

    char stack_text[192];
    va_list arguments;
    va_start(arguments, format);
    va_list measurement;
    va_copy(measurement, arguments);
    const int required = vsnprintf(stack_text, sizeof(stack_text), format, arguments);
    va_end(arguments);

    char *allocated = NULL;
    const char *next = stack_text;
    if (required < 0) {
        va_end(measurement);
        return FALSE;
    }
    if ((size_t)required >= sizeof(stack_text)) {
        allocated = malloc((size_t)required + 1U);
        if (!allocated) {
            va_end(measurement);
            return FALSE;
        }
        (void)vsnprintf(allocated, (size_t)required + 1U, format, measurement);
        next = allocated;
    }
    va_end(measurement);

    const char *current = gtk_label_get_text(GTK_LABEL(label));
    const gboolean changed = lsm_ui_text_needs_update(current, next);
    if (changed) gtk_label_set_text(GTK_LABEL(label), next);
    free(allocated);
    return changed;
}


void lsm_ui_show_error(GtkWindow *parent, const char *title,
                       const char *format, ...)
{
    if (!format) return;

    va_list arguments;
    va_start(arguments, format);
    char *message = g_strdup_vprintf(format, arguments);
    va_end(arguments);

    GtkWidget *dialog = gtk_message_dialog_new(
        parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
        "%s", title ? title : "Error");
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog), "%s", message ? message : "");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(message);
}

GdkRGBA lsm_ui_background_colour(GtkWidget *widget)
{
    GdkRGBA background = {0.17, 0.17, 0.19, 1.0};
    if (!widget) return background;

    GtkStyleContext *style = gtk_widget_get_style_context(widget);
    GdkRGBA theme_background;
    if (gtk_style_context_lookup_color(style, "theme_bg_color", &theme_background) ||
        gtk_style_context_lookup_color(style, "window_bg_color", &theme_background))
        background = theme_background;

    const double luminance = 0.2126 * background.red +
                             0.7152 * background.green +
                             0.0722 * background.blue;
    /* Some dark themes publish an almost-black DrawingArea colour. Graphs use
     * the surrounding panel tone so grid lines remain distinguishable. */
    if (luminance < 0.08) {
        background.red = 0.17;
        background.green = 0.17;
        background.blue = 0.19;
        background.alpha = 1.0;
    }
    return background;
}

gboolean lsm_ui_text_matches(const char *text, const char *needle)
{
    if (!needle || !*needle) return TRUE;
    if (!text) return FALSE;

    char *folded_text = g_utf8_casefold(text, -1);
    char *folded_needle = g_utf8_casefold(needle, -1);
    const gboolean matches =
        folded_text && folded_needle && strstr(folded_text, folded_needle) != NULL;
    g_free(folded_text);
    g_free(folded_needle);
    return matches;
}
