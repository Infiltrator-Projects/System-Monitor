// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file task_launcher.c
 * @brief User-triggered graphical task launcher without shell interpretation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "task_launcher.h"

#include "app.h"
#include "ui_helpers.h"

#include <gtk/gtk.h>

void lsm_task_launcher_show(LsmApp *app)
{
    if (!app || !app->shell.window) return;
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Run new task", GTK_WINDOW(app->shell.window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Run", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 560, 190);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);

    GtkWidget *description = gtk_label_new(
        "Enter the program and any arguments. The command is launched directly "
        "as your user account and is not interpreted by a shell.");
    gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), description, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry),
                                   "Program and arguments");
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 10);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *command = gtk_entry_get_text(GTK_ENTRY(entry));
        gint argument_count = 0;
        gchar **arguments = NULL;
        GError *error = NULL;
        if (!command || !*command ||
            !g_shell_parse_argv(command, &argument_count, &arguments, &error) ||
            argument_count <= 0 || !arguments || !arguments[0]) {
            lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                "Unable to run the task", "%s",
                error ? error->message :
                "Enter the name of a program to run.");
        } else if (!g_spawn_async(
                       g_get_home_dir(), arguments, NULL,
                       G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
            lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                "Unable to run the task", "%s",
                error ? error->message : "The program could not be started.");
        }
        if (error) g_error_free(error);
        g_strfreev(arguments);
    }
    gtk_widget_destroy(dialog);
}
