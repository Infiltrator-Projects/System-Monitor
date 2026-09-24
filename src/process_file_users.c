// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_file_users.c
 * @brief Asynchronous exact-file process-owner search and presentation.
 *
 * File ownership discovery is explicit user work, but the /proc walk runs on a
 * worker because cost grows with process and open-resource count. GTK receives
 * only the completed plain-data result.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_file_users.h"

#include "app_internal.h"
#include "process_inspection.h"
#include "process_table_ui.h"
#include "ui_helpers.h"

#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char *path;
} FileUsersRequest;

typedef struct {
    char *path;
    LsmFileUserInfo *items;
    size_t count;
} FileUsersResult;

static void show_file_users_results(GtkWindow *parent, const char *path,
                                    LsmFileUserInfo *items, size_t count)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Processes using file", parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 820, 520);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    GtkWidget *description = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped(
        "<b>%zu matching descriptor%s</b>\n%s",
        count, count == 1U ? "" : "s", path);
    gtk_label_set_markup(GTK_LABEL(description), markup);
    g_free(markup);
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_label_set_selectable(GTK_LABEL(description), TRUE);
    gtk_box_pack_start(GTK_BOX(content), description, FALSE, FALSE, 0);

    GtkListStore *store = gtk_list_store_new(
        4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    for (size_t index = 0U; index < count; index++) {
        GtkTreeIter iterator;
        char pid[32];
        char descriptor[32];
        snprintf(pid, sizeof(pid), "%llu",
                 (unsigned long long)items[index].pid);
        snprintf(descriptor, sizeof(descriptor), "%d",
                 items[index].descriptor);
        gtk_list_store_append(store, &iterator);
        gtk_list_store_set(store, &iterator,
                           0, pid,
                           1, items[index].process_name,
                           2, descriptor,
                           3, items[index].target,
                           -1);
    }

    static const char *titles[] = {"PID", "Process", "FD", "Target"};
    GtkWidget *result_status = NULL;
    GtkWidget *tree = lsm_process_table_page(
        store, titles, G_N_ELEMENTS(titles), 3, &result_status);
    lsm_ui_set_label_text(result_status, "%zu matching descriptor%s",
                          count, count == 1U ? "" : "s");
    gtk_box_pack_start(GTK_BOX(content), tree, TRUE, TRUE, 8);
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_object_unref(store);
}

static void file_users_result_free(gpointer data)
{
    FileUsersResult *result = data;
    if (!result) return;
    g_free(result->path);
    lsm_process_inspection_free(result->items);
    g_free(result);
}

static void file_users_request_free(gpointer data)
{
    FileUsersRequest *request = data;
    if (!request) return;
    g_free(request->path);
    g_free(request);
}

static void file_users_worker(GTask *task, gpointer source_object,
                              gpointer task_data, GCancellable *cancellable)
{
    (void)source_object;
    (void)cancellable;
    const FileUsersRequest *request = task_data;
    FileUsersResult *result = g_new0(FileUsersResult, 1U);
    result->path = g_strdup(request->path);
    result->count = lsm_process_inspection_find_file_users(
        request->path, &result->items);
    g_task_return_pointer(task, result, file_users_result_free);
}

static void file_users_complete(GObject *source_object,
                                GAsyncResult *async_result,
                                gpointer user_data)
{
    (void)user_data;
    FileUsersResult *result =
        g_task_propagate_pointer(G_TASK(async_result), NULL);
    if (result && source_object &&
        gtk_widget_get_mapped((GtkWidget *)source_object))
        show_file_users_results(GTK_WINDOW(source_object), result->path,
                                result->items, result->count);
    file_users_result_free(result);
}

void lsm_process_file_users_show(LsmApp *app)
{
    if (!app) return;
    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Find processes using a file", GTK_WINDOW(app->shell.window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Search", GTK_RESPONSE_ACCEPT,
        NULL);
    if (gtk_dialog_run(GTK_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(chooser);
        return;
    }

    char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    gtk_widget_destroy(chooser);
    if (!path) return;

    FileUsersRequest *request = g_new0(FileUsersRequest, 1U);
    request->path = path;
    GTask *task = g_task_new(G_OBJECT(app->shell.window), NULL,
                             file_users_complete, NULL);
    g_task_set_task_data(task, request, file_users_request_free);
    g_task_run_in_thread(task, file_users_worker);
    g_object_unref(task);
}
