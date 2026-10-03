// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file gtk_typography_chrome.c
 * @brief Check real GTK font resolution and titlebar colours from a DEB payload.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_internal.h"
#include "app_shell.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *find_named(GtkWidget *widget, const char *name)
{
    if (strcmp(gtk_widget_get_name(widget), name) == 0) return widget;
    if (!GTK_IS_CONTAINER(widget)) return NULL;
    GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget *found = NULL;
    for (GList *item = children; item && !found; item = item->next)
        found = find_named(GTK_WIDGET(item->data), name);
    g_list_free(children);
    return found;
}

static void check_font(GtkWidget *widget, const char *family)
{
    if (!widget) exit(EXIT_FAILURE);
    PangoFontDescription *requested = NULL;
    gtk_style_context_get(gtk_widget_get_style_context(widget),
                          GTK_STATE_FLAG_NORMAL, "font", &requested, NULL);
    PangoFont *font = pango_context_load_font(
        gtk_widget_get_pango_context(widget), requested);
    if (!font) exit(EXIT_FAILURE);
    PangoFontDescription *actual = pango_font_describe(font);
    const char *resolved = pango_font_description_get_family(actual);
    if (!resolved || strcmp(resolved, family) != 0) {
        fprintf(stderr, "GTK substituted %s for %s\n",
                resolved ? resolved : "(missing)", family);
        exit(EXIT_FAILURE);
    }
    pango_font_description_free(actual);
    pango_font_description_free(requested);
    g_object_unref(font);
}

static void check_colour(GtkWidget *widget, GtkStateFlags state, uint32_t rgb)
{
    GdkRGBA colour;
    gtk_style_context_get_color(gtk_widget_get_style_context(widget), state, &colour);
    const double red = (double)((rgb >> 16U) & 255U) / 255.0;
    const double green = (double)((rgb >> 8U) & 255U) / 255.0;
    const double blue = (double)(rgb & 255U) / 255.0;
    if (fabs(colour.red - red) > 0.01 || fabs(colour.green - green) > 0.01 ||
        fabs(colour.blue - blue) > 0.01 || colour.alpha < 0.99) {
        fprintf(stderr, "Titlebar state %u colour %.3f/%.3f/%.3f expected #%06X\n",
                (unsigned int)state, colour.red, colour.green, colour.blue,
                (unsigned int)rgb);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    g_object_set(gtk_settings_get_default(), "gtk-enable-animations", FALSE, NULL);
    LsmApp *app = g_new0(LsmApp, 1);
    app->shell.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *header = lsm_app_shell_build_header(app);
    gtk_window_set_titlebar(GTK_WINDOW(app->shell.window), header);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *body = gtk_label_new("Body text 0123456789");
    GtkWidget *title = gtk_label_new("CPU");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "lsm-performance-title");
    gtk_container_add(GTK_CONTAINER(app->shell.window), box);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), body, FALSE, FALSE, 0);
    gtk_widget_show_all(app->shell.window);

    const InfiltratrTypography *typography = infiltratr_typography();
    const InfiltratrThemeMode modes[] = {INFILTRATR_THEME_DAY, INFILTRATR_THEME_NIGHT};
    const GtkStateFlags states[] = {
        GTK_STATE_FLAG_NORMAL, GTK_STATE_FLAG_PRELIGHT,
        GTK_STATE_FLAG_ACTIVE, GTK_STATE_FLAG_BACKDROP
    };
    const char *keys[] = {
        "lsm-minimize-button", "lsm-maximize-button", "lsm-close-button"
    };
    for (size_t mode = 0U; mode < G_N_ELEMENTS(modes); mode++) {
        app->runtime.theme_mode = modes[mode];
        lsm_app_shell_apply_theme(app);
        while (gtk_events_pending()) gtk_main_iteration();
        check_font(body, typography->ui_family);
        check_font(title, typography->brand_family);
        check_font(find_named(header, "lsm-header-brand-title"), typography->brand_family);
        const InfiltratrThemePalette *palette = infiltratr_theme_resolve(modes[mode], FALSE);
        for (size_t key = 0U; key < G_N_ELEMENTS(keys); key++) {
            GtkWidget *button = g_object_get_data(G_OBJECT(app->shell.window), keys[key]);
            GtkWidget *image = gtk_bin_get_child(GTK_BIN(button));
            for (size_t state = 0U; state < G_N_ELEMENTS(states); state++) {
                const uint32_t expected = key == 2U && states[state] == GTK_STATE_FLAG_PRELIGHT
                    ? 0xFFFFFFU : palette->title_rgb;
                gtk_widget_set_state_flags(button, states[state], TRUE);
                gtk_widget_set_state_flags(image, states[state], TRUE);
                while (gtk_events_pending()) gtk_main_iteration();
                check_colour(button, states[state], expected);
                check_colour(image, states[state], expected);
            }
        }
    }
    gtk_widget_destroy(app->shell.window);
    g_object_unref(app->shell.theme_provider);
    g_free(app);
    puts("DEB fonts resolve to MB Corpo and titlebar icons follow Day/Night in all tested states.");
    return EXIT_SUCCESS;
}
