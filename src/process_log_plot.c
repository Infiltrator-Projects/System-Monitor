// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_log_plot.c
 * @brief Asynchronous recorded-process CSV parsing and graph presentation.
 *
 * The menu layer dispatches this action but does not own file parsing, worker
 * lifetime or graph-window construction.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_log_plot.h"

#include "app_presentation_context.h"
#include "app_runtime_context.h"
#include "common.h"
#include "graph.h"
#include "process_recorder.h"
#include "ui_helpers.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void graph_window_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    lsm_graph_free(user_data);
}

/** Immutable file request passed to the process-log parser worker. */
typedef struct {
    char *filename; /**< Owned path selected by the user. */
} ProcessLogPlotRequest;

/** Bounded process-log samples returned to the GTK main context. */
typedef struct {
    char *filename; /**< Owned source path used as the graph caption. */
    char *error_message; /**< Owned parse/open error, or NULL on success. */
    double cpu[LSM_HISTORY_LENGTH]; /**< Circular CPU sample storage. */
    double memory[LSM_HISTORY_LENGTH]; /**< Circular memory sample storage. */
    size_t count; /**< Number of retained samples, bounded by history length. */
    size_t next; /**< Circular index that will receive the next sample. */
} ProcessLogPlotResult;

static void process_log_plot_request_free(gpointer data)
{
    ProcessLogPlotRequest *request = data;
    if (!request) return;
    g_free(request->filename);
    g_free(request);
}

static void process_log_plot_result_free(gpointer data)
{
    ProcessLogPlotResult *result = data;
    if (!result) return;
    g_free(result->filename);
    g_free(result->error_message);
    g_free(result);
}

static void process_log_plot_worker(GTask *task, gpointer source_object,
                                    gpointer task_data,
                                    GCancellable *cancellable)
{
    (void)source_object;
    (void)cancellable;
    const ProcessLogPlotRequest *request = task_data;
    ProcessLogPlotResult *result = g_new0(ProcessLogPlotResult, 1U);
    result->filename = g_strdup(request->filename);

    FILE *file = fopen(request->filename, "r");
    if (!file) {
        result->error_message = g_strdup(g_strerror(errno ? errno : EIO));
        g_task_return_pointer(task, result, process_log_plot_result_free);
        return;
    }

    char line[2048];
    bool header = true;
    while (fgets(line, sizeof(line), file)) {
        if (header) {
            header = false;
            if (!lsm_process_recorder_header_matches(line)) {
                result->error_message =
                    g_strdup("Not a supported System Monitor process recording.");
                break;
            }
            continue;
        }
        if (strchr(line, '\n') == NULL) {
            /* Discard a long/interrupted physical record in full. Its later
             * fgets fragments must never become independent graph samples. */
            while (fgets(line, sizeof(line), file) &&
                   strchr(line, '\n') == NULL) {}
            continue;
        }
        double cpu = 0.0;
        double memory = 0.0;
        if (!lsm_process_recorder_parse_metrics(line, &cpu, &memory))
            continue;
        result->cpu[result->next] = cpu;
        result->memory[result->next] = memory;
        result->next = (result->next + 1U) % LSM_HISTORY_LENGTH;
        if (result->count < LSM_HISTORY_LENGTH) result->count++;
    }
    if (ferror(file) && !result->error_message)
        result->error_message = g_strdup(g_strerror(errno ? errno : EIO));
    if (fclose(file) != 0 && !result->error_message)
        result->error_message = g_strdup(g_strerror(errno ? errno : EIO));
    if (result->count == 0U && !result->error_message)
        result->error_message =
            g_strdup("The process recording contains no valid samples.");
    g_task_return_pointer(task, result, process_log_plot_result_free);
}

static void process_log_plot_complete(GObject *source_object,
                                      GAsyncResult *async_result,
                                      gpointer user_data)
{
    (void)user_data;
    ProcessLogPlotResult *result =
        g_task_propagate_pointer(G_TASK(async_result), NULL);
    LsmApp *app = source_object
        ? g_object_get_data(source_object, "lsm-app") : NULL;
    LsmRuntimeControlView runtime;
    if (!result || !app ||
        !lsm_app_runtime_control_view(app, &runtime) || runtime.shutting_down) {
        process_log_plot_result_free(result);
        return;
    }

    GtkWidget *parent = lsm_app_presentation_window(app);
    if (!parent) {
        process_log_plot_result_free(result);
        return;
    }
    if (result->error_message) {
        lsm_ui_show_error(GTK_WINDOW(parent),
                          "Unable to plot process log", "%s",
                          result->error_message);
        process_log_plot_result_free(result);
        return;
    }

    /* A single process can consume several logical cores, so recorded CPU
     * percentages legitimately exceed 100. Preserve a 100% minimum for the
     * memory trace, then grow the shared axis in complete 100% increments. */
    LsmGraph *graph = lsm_graph_new(TRUE, FALSE, 0.0, -1, 390);
    if (!graph) {
        lsm_ui_show_error(GTK_WINDOW(parent), "Unable to plot process log",
                          "Unable to allocate the process graph.");
        process_log_plot_result_free(result);
        return;
    }
    lsm_graph_set_dynamic_scale(graph, 100.0, 100.0);
    const size_t first = result->count == LSM_HISTORY_LENGTH
        ? result->next : 0U;
    for (size_t offset = 0U; offset < result->count; offset++) {
        const size_t index = (first + offset) % LSM_HISTORY_LENGTH;
        lsm_graph_push(graph, result->cpu[index], result->memory[index], TRUE);
    }

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), LSM_PROGRAM_NAME " Process Log");
    lsm_ui_set_workarea_default_size(GTK_WINDOW(window), 820, 500);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(parent));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    GtkWidget *label = gtk_label_new(result->filename);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    GtkWidget *legend = gtk_label_new(NULL);
    char legend_markup[128];
    snprintf(legend_markup, sizeof(legend_markup),
             "<b>CPU %%</b> and <b>Memory %%</b> — most recent %zu %s",
             result->count, result->count == 1U ? "sample" : "samples");
    gtk_label_set_markup(GTK_LABEL(legend), legend_markup);
    gtk_widget_set_halign(legend, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), legend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), graph->area, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    g_signal_connect(window, "destroy", G_CALLBACK(graph_window_destroy), graph);
    gtk_widget_show_all(window);
    process_log_plot_result_free(result);
}

/* Log parsing is worker-owned; GTK only constructs the completed graph. */
void lsm_process_log_plot_show(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    if (!app) return;
    GtkWidget *parent = lsm_app_presentation_window(app);
    if (!parent) return;

    GtkWidget *chooser = gtk_file_chooser_dialog_new("Plot process log",
        GTK_WINDOW(parent), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);
    char log_dir[LSM_PATH_LEN];
    char home[LSM_PATH_LEN];
    if (lsm_home_directory(home, sizeof(home)) &&
        lsm_join_path(log_dir, sizeof(log_dir), home, LSM_LOG_DIRECTORY))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), log_dir);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "System Monitor CSV logs");
    gtk_file_filter_add_pattern(filter, "*.csv");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(chooser);
        return;
    }
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    gtk_widget_destroy(chooser);
    if (!filename) return;

    ProcessLogPlotRequest *request = g_new0(ProcessLogPlotRequest, 1U);
    request->filename = filename;
    GTask *task = g_task_new(G_OBJECT(parent), NULL,
                             process_log_plot_complete, NULL);
    g_task_set_task_data(task, request, process_log_plot_request_free);
    g_task_run_in_thread(task, process_log_plot_worker);
    g_object_unref(task);
}
