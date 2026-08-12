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

static void set_bold(GtkWidget *label)
{
    PangoAttrList *attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(label), attributes);
    pango_attr_list_unref(attributes);
}

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

GtkWidget *lsm_ui_make_page_header(const char *title, const char *subtitle,
                                   GtkWidget **title_out,
                                   GtkWidget **subtitle_out)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *title_label = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", title ? title : "");
    gtk_label_set_markup(GTK_LABEL(title_label), markup);
    g_free(markup);
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);

    GtkWidget *subtitle_label = gtk_label_new(subtitle ? subtitle : "");
    gtk_widget_set_halign(subtitle_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(subtitle_label), PANGO_ELLIPSIZE_END);

    gtk_box_pack_start(GTK_BOX(box), title_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), subtitle_label, FALSE, FALSE, 0);

    if (title_out) *title_out = title_label;
    if (subtitle_out) *subtitle_out = subtitle_label;
    return box;
}

GtkWidget *lsm_ui_make_value_grid(const char *const *names, size_t count,
                                  GtkWidget **values_out)
{
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);
    gtk_widget_set_halign(grid, GTK_ALIGN_START);

    for (size_t index = 0; index < count; index++) {
        GtkWidget *name = gtk_label_new(names[index] ? names[index] : "");
        gtk_widget_set_halign(name, GTK_ALIGN_START);

        GtkWidget *value = gtk_label_new("N/A");
        gtk_widget_set_halign(value, GTK_ALIGN_START);
        set_bold(value);

        const int column_group = (int)(index / 6);
        const int row = (int)(index % 6);
        gtk_grid_attach(GTK_GRID(grid), name, column_group * 2, row, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), value, column_group * 2 + 1, row, 1, 1);
        if (values_out) values_out[index] = value;
    }
    return grid;
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
