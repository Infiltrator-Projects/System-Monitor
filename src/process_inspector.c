// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_inspector.c
 * @brief Multi-page graphical Process Inspector implementation.
 *
 * The inspector is deliberately non-modal: the main monitor continues sampling
 * while the window displays live CPU, memory and I/O values. Expensive procfs
 * inventories are refreshed only on explicit user request, while the compact
 * overview and performance graph update once per second from the application's
 * retained process snapshot. This separation avoids turning an inspection
 * window into a second full process scanner.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_inspector.h"
#include "app_internal.h"

#include "common.h"
#include "duration_format.h"
#include "graph.h"
#include "process_backend.h"
#include "process_inspection.h"
#include "ui_helpers.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INSPECTOR_OVERVIEW_LABELS 16U

typedef struct {
    LsmApp *app;
    LsmProcessId pid;
    LsmProcessInstanceId instance_id;
    GtkWidget *window;
    GtkWidget *identity_label;
    GtkWidget *overview_values[INSPECTOR_OVERVIEW_LABELS];
    GtkWidget *cpu_value;
    GtkWidget *memory_value;
    GtkWidget *read_value;
    GtkWidget *write_value;
    GtkWidget *priority_combo;
    GtkListStore *open_files_store;
    GtkListStore *maps_store;
    GtkListStore *threads_store;
    GtkListStore *family_store;
    GtkWidget *open_files_status;
    GtkWidget *maps_status;
    GtkWidget *threads_status;
    GtkWidget *family_status;
    char executable[LSM_PATH_LEN];
    unsigned descriptor_count;
    LsmGraph *performance_graph;
    guint refresh_timer;
} ProcessInspector;

/* Every inspector operation revalidates the process instance token. A
 * recycled process identifier must never redirect an existing inspector to a
 * different process. */
static const LsmProcessInfo *snapshot_process(
    const LsmApp *app, LsmProcessId pid, LsmProcessInstanceId instance_id)
{
    if (!app) return NULL;
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[index];
        if (process->pid == pid && process->instance_id == instance_id)
            return process;
    }
    return NULL;
}

static bool inspector_identity_current(const ProcessInspector *inspector)
{
    return inspector && lsm_process_inspection_identity_matches(
        inspector->pid, inspector->instance_id);
}

static bool require_current_identity(ProcessInspector *inspector,
                                     const char *operation)
{
    if (inspector_identity_current(inspector)) return true;
    lsm_ui_show_error(GTK_WINDOW(inspector->window), operation,
        "The original process has exited or its PID has been reused.");
    return false;
}

static int priority_index(LsmProcessPriority priority)
{
    return (int)priority;
}

static double displayed_cpu(const LsmApp *app, double total_percent)
{
    if (!app || !app->runtime.process_cpu_per_core) return total_percent;
    const unsigned cores = app->monitor.cpu.logical_cores ?
        app->monitor.cpu.logical_cores : 1U;
    return total_percent * (double)cores;
}

static GtkWidget *detail_value(void)
{
    GtkWidget *label = gtk_label_new("N/A");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    return label;
}

static void attach_detail(GtkGrid *grid, int row, const char *caption,
                          GtkWidget **value_out)
{
    GtkWidget *name = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped("<b>%s</b>", caption);
    gtk_label_set_markup(GTK_LABEL(name), markup);
    g_free(markup);
    gtk_widget_set_halign(name, GTK_ALIGN_START);
    GtkWidget *value = detail_value();
    gtk_grid_attach(grid, name, 0, row, 1, 1);
    gtk_grid_attach(grid, value, 1, row, 1, 1);
    *value_out = value;
}

static GtkTreeViewColumn *append_text_column(GtkWidget *tree, const char *title,
                                             int model_column, gboolean expand)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        title, renderer, "text", model_column, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, model_column);
    gtk_tree_view_column_set_expand(column, expand);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    return column;
}

static GtkWidget *tree_page(GtkListStore *store, const char *const *titles,
                            size_t title_count, int expand_column,
                            GtkWidget **status_out)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    GtkWidget *status = gtk_label_new("Not yet refreshed");
    gtk_widget_set_halign(status, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), status, FALSE, FALSE, 0);
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree), TRUE);
    for (size_t index = 0U; index < title_count; index++)
        append_text_column(tree, titles[index], (int)index,
                           (int)index == expand_column);
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_container_add(GTK_CONTAINER(scroller), tree);
    gtk_box_pack_start(GTK_BOX(box), scroller, TRUE, TRUE, 0);
    if (status_out) *status_out = status;
    return box;
}

/* Construction is separated from population so expensive inventories can be
 * refreshed explicitly without rebuilding the inspector window. */
static GtkWidget *build_overview_page(ProcessInspector *inspector)
{
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget *grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    static const char *captions[INSPECTOR_OVERVIEW_LABELS] = {
        "Name", "PID", "Parent PID", "User", "State", "Executable",
        "Command", "Started", "Elapsed", "Threads", "Handles",
        "Priority", "CPU time", "GPU", "GPU engine", "GPU memory"
    };
    for (size_t index = 0U; index < G_N_ELEMENTS(captions); index++)
        attach_detail(GTK_GRID(grid), (int)index, captions[index],
                      &inspector->overview_values[index]);
    gtk_container_add(GTK_CONTAINER(scroller), grid);
    return scroller;
}

static GtkWidget *metric_block(const char *caption, GtkWidget **value_out)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *value = gtk_label_new("0%");
    GtkWidget *name = gtk_label_new(caption);
    gtk_widget_set_halign(value, GTK_ALIGN_START);
    gtk_widget_set_halign(name, GTK_ALIGN_START);
    char *markup = g_markup_printf_escaped("<span size=\"xx-large\"><b>%s</b></span>",
                                           "0%");
    gtk_label_set_markup(GTK_LABEL(value), markup);
    g_free(markup);
    gtk_box_pack_start(GTK_BOX(box), value, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), name, FALSE, FALSE, 0);
    *value_out = value;
    return box;
}

static GtkWidget *build_performance_page(ProcessInspector *inspector)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    GtkWidget *metrics = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(metrics), 36);
    gtk_grid_set_row_spacing(GTK_GRID(metrics), 8);
    gtk_grid_attach(GTK_GRID(metrics), metric_block("CPU", &inspector->cpu_value), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics), metric_block("Memory", &inspector->memory_value), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics), metric_block("Read", &inspector->read_value), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics), metric_block("Write", &inspector->write_value), 3, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), metrics, FALSE, FALSE, 0);
    inspector->performance_graph = lsm_graph_new(TRUE, TRUE, 100.0,
                                                 -1, 360);
    gtk_box_pack_start(GTK_BOX(box), inspector->performance_graph->area,
                       TRUE, TRUE, 0);
    GtkWidget *legend = gtk_label_new(
        "CPU (total computer capacity) and memory percentage — recent history");
    gtk_widget_set_halign(legend, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), legend, FALSE, FALSE, 0);
    return box;
}

/* Inventory reads are deliberately on-demand. They can be large and may race
 * with process churn, so each category reports partial/unavailable results
 * without invalidating the one-second summary. */
static void populate_open_files(ProcessInspector *inspector)
{
    gtk_list_store_clear(inspector->open_files_store);
    LsmOpenFileInfo *items = NULL;
    const size_t count = lsm_process_inspection_open_files(inspector->pid, &items);
    for (size_t index = 0U; index < count; index++) {
        GtkTreeIter iterator;
        char descriptor[32];
        snprintf(descriptor, sizeof(descriptor), "%d", items[index].descriptor);
        gtk_list_store_append(inspector->open_files_store, &iterator);
        gtk_list_store_set(inspector->open_files_store, &iterator,
                           0, descriptor, 1, items[index].kind,
                           2, items[index].target, -1);
    }
    if (count != 0U)
        lsm_ui_set_label_text(inspector->open_files_status,
                              "%zu open descriptor%s", count,
                              count == 1U ? "" : "s");
    else
        lsm_ui_set_label_text(inspector->open_files_status,
            "No readable descriptors — the process may have exited or access may be restricted");
    lsm_process_inspection_free(items);
}

static void populate_maps(ProcessInspector *inspector)
{
    gtk_list_store_clear(inspector->maps_store);
    LsmMemoryMapInfo *items = NULL;
    const size_t count = lsm_process_inspection_memory_maps(inspector->pid, &items);
    for (size_t index = 0U; index < count; index++) {
        GtkTreeIter iterator;
        char range[64], offset[32], inode[32], size[64];
        snprintf(range, sizeof(range), "%llx–%llx",
                 (unsigned long long)items[index].start_address,
                 (unsigned long long)items[index].end_address);
        snprintf(offset, sizeof(offset), "0x%llx",
                 (unsigned long long)items[index].file_offset);
        snprintf(inode, sizeof(inode), "%llu",
                 (unsigned long long)items[index].inode);
        lsm_format_bytes(items[index].end_address - items[index].start_address,
                         size, sizeof(size));
        gtk_list_store_append(inspector->maps_store, &iterator);
        gtk_list_store_set(inspector->maps_store, &iterator,
                           0, range, 1, size, 2, items[index].permissions,
                           3, offset, 4, items[index].device, 5, inode,
                           6, items[index].path, -1);
    }
    if (count != 0U)
        lsm_ui_set_label_text(inspector->maps_status,
                              "%zu virtual-memory area%s", count,
                              count == 1U ? "" : "s");
    else
        lsm_ui_set_label_text(inspector->maps_status,
            "Memory map unavailable — the process may have exited or access may be restricted");
    lsm_process_inspection_free(items);
}

static void populate_threads(ProcessInspector *inspector)
{
    gtk_list_store_clear(inspector->threads_store);
    LsmThreadInfo *items = NULL;
    const size_t count = lsm_process_inspection_threads(inspector->pid, &items);
    for (size_t index = 0U; index < count; index++) {
        GtkTreeIter iterator;
        char tid[32];
        snprintf(tid, sizeof(tid), "%llu",
                 (unsigned long long)items[index].tid);
        gtk_list_store_append(inspector->threads_store, &iterator);
        gtk_list_store_set(inspector->threads_store, &iterator,
                           0, tid, 1, items[index].name, 2, items[index].state, -1);
    }
    if (count != 0U)
        lsm_ui_set_label_text(inspector->threads_status, "%zu thread%s", count,
                              count == 1U ? "" : "s");
    else
        lsm_ui_set_label_text(inspector->threads_status,
                              "Thread information unavailable");
    lsm_process_inspection_free(items);
}

static void populate_family(ProcessInspector *inspector)
{
    gtk_list_store_clear(inspector->family_store);
    const LsmProcessInfo *selected = snapshot_process(inspector->app, inspector->pid,
                                                      inspector->instance_id);
    if (!selected) {
        lsm_ui_set_label_text(inspector->family_status,
                              "Process family unavailable — the process has exited");
        return;
    }
    size_t child_count = 0U;
    for (size_t index = 0U; index < inspector->app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process = &inspector->app->process.process_snapshot[index];
        const char *relationship = NULL;
        if (process->pid == selected->ppid) relationship = "Parent";
        else if (process->ppid == selected->pid) {
            relationship = "Child";
            child_count++;
        } else if (process->pid == selected->pid) relationship = "Selected";
        if (!relationship) continue;
        GtkTreeIter iterator;
        char pid[32], cpu[32], memory[32];
        snprintf(pid, sizeof(pid), "%llu",
                 (unsigned long long)process->pid);
        snprintf(cpu, sizeof(cpu), "%.1f%%",
                 displayed_cpu(inspector->app, process->cpu_percent));
        snprintf(memory, sizeof(memory), "%.1f%%", process->memory_percent);
        gtk_list_store_append(inspector->family_store, &iterator);
        gtk_list_store_set(inspector->family_store, &iterator,
                           0, relationship, 1, pid, 2, process->name,
                           3, process->state, 4, cpu, 5, memory, -1);
    }
    lsm_ui_set_label_text(inspector->family_status,
        "Selected process, parent and %zu immediate child%s", child_count,
        child_count == 1U ? "" : "ren");
}

static void refresh_inventories(ProcessInspector *inspector)
{
    if (!inspector_identity_current(inspector)) {
        gtk_list_store_clear(inspector->open_files_store);
        gtk_list_store_clear(inspector->maps_store);
        gtk_list_store_clear(inspector->threads_store);
        gtk_list_store_clear(inspector->family_store);
        lsm_ui_set_label_text(inspector->open_files_status,
                              "Process exited or PID was reused");
        lsm_ui_set_label_text(inspector->maps_status,
                              "Process exited or PID was reused");
        lsm_ui_set_label_text(inspector->threads_status,
                              "Process exited or PID was reused");
        lsm_ui_set_label_text(inspector->family_status,
                              "Process exited or PID was reused");
        return;
    }
    const LsmProcessInfo *snapshot = snapshot_process(inspector->app,
        inspector->pid, inspector->instance_id);
    inspector->executable[0] = '\0';
    inspector->descriptor_count = 0U;
    if (snapshot) {
        LsmProcessInfo details = *snapshot;
        if (lsm_process_enrich(details.pid, &details,
                LSM_PROCESS_SCAN_EXECUTABLE |
                LSM_PROCESS_SCAN_HANDLE_COUNT)) {
            lsm_copy_string(
                inspector->executable, sizeof(inspector->executable),
                details.executable);
            inspector->descriptor_count = details.handle_count;
        }
    }
    populate_open_files(inspector);
    populate_maps(inspector);
    populate_threads(inspector);
    populate_family(inspector);
}

static void set_large_metric(GtkWidget *label, const char *text)
{
    char *markup = g_markup_printf_escaped(
        "<span size=\"xx-large\"><b>%s</b></span>", text);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
}

static gboolean inspector_update(gpointer user_data)
{
    ProcessInspector *inspector = user_data;
    const LsmProcessInfo *snapshot = snapshot_process(inspector->app,
        inspector->pid, inspector->instance_id);
    if (!snapshot || !inspector_identity_current(inspector)) {
        lsm_ui_set_label_text(inspector->identity_label,
                              "Process %llu has exited",
                              (unsigned long long)inspector->pid);
        return G_SOURCE_CONTINUE;
    }

    LsmProcessInfo process = *snapshot;
    lsm_copy_string(process.executable, sizeof(process.executable),
                    inspector->executable);
    process.handle_count = inspector->descriptor_count;
    char pid[32], ppid[32], started[64], elapsed[64], threads[32], handles[32];
    char priority[64], cpu[32], memory[32], read_rate[64], write_rate[64];
    char cpu_time[64], gpu[32], gpu_memory[64];
    snprintf(pid, sizeof(pid), "%llu",
             (unsigned long long)process.pid);
    snprintf(ppid, sizeof(ppid), "%llu",
             (unsigned long long)process.ppid);
    if (process.start_time_epoch > 0) {
        const time_t value = (time_t)process.start_time_epoch;
        struct tm local;
        localtime_r(&value, &local);
        strftime(started, sizeof(started), "%d/%m/%Y %H:%M:%S", &local);
    } else {
        snprintf(started, sizeof(started), "N/A");
    }
    lsm_duration_format_clock(process.elapsed_seconds, elapsed,
                              sizeof(elapsed));
    snprintf(threads, sizeof(threads), "%u", process.threads);
    snprintf(handles, sizeof(handles), "%u", process.handle_count);
    snprintf(priority, sizeof(priority), "%s",
             lsm_process_priority_name(process.priority));
    lsm_duration_format_clock(process.cpu_time_seconds, cpu_time,
                              sizeof(cpu_time));
    if (process.gpu_available)
        snprintf(gpu, sizeof(gpu), "%.1f%%", process.gpu_percent);
    else
        snprintf(gpu, sizeof(gpu), "N/A");
    if (process.gpu_memory_available)
        lsm_format_bytes(process.gpu_memory_bytes,
                         gpu_memory, sizeof(gpu_memory));
    else
        snprintf(gpu_memory, sizeof(gpu_memory), "N/A");
    const char *values[INSPECTOR_OVERVIEW_LABELS] = {
        process.name, pid, ppid, process.user, process.state,
        process.executable, process.command, started, elapsed, threads, handles,
        priority, cpu_time, gpu,
        process.gpu_available && process.gpu_engine[0]
            ? process.gpu_engine : "N/A",
        gpu_memory
    };
    for (size_t index = 0U; index < G_N_ELEMENTS(values); index++)
        lsm_ui_set_label_text(inspector->overview_values[index], "%s",
                              values[index] && *values[index] ? values[index] : "N/A");

    const double cpu_value = displayed_cpu(inspector->app, process.cpu_percent);
    snprintf(cpu, sizeof(cpu), "%.1f%%", cpu_value);
    snprintf(memory, sizeof(memory), "%.1f%%", process.memory_percent);
    lsm_format_rate(process.read_bytes_per_sec, read_rate, sizeof(read_rate));
    lsm_format_rate(process.write_bytes_per_sec, write_rate, sizeof(write_rate));
    set_large_metric(inspector->cpu_value, cpu);
    set_large_metric(inspector->memory_value, memory);
    set_large_metric(inspector->read_value, read_rate);
    set_large_metric(inspector->write_value, write_rate);
    lsm_graph_push(inspector->performance_graph, process.cpu_percent,
                   process.memory_percent, inspector->app->runtime.newer_on_right);
    lsm_ui_set_label_text(inspector->identity_label,
                          "%s — PID %llu — %s", process.name,
                          (unsigned long long)process.pid,
                          process.user);
    return G_SOURCE_CONTINUE;
}

static void inspector_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    ProcessInspector *inspector = user_data;
    if (inspector->refresh_timer) g_source_remove(inspector->refresh_timer);
    if (inspector->performance_graph) lsm_graph_free(inspector->performance_graph);
    if (inspector->open_files_store) g_object_unref(inspector->open_files_store);
    if (inspector->maps_store) g_object_unref(inspector->maps_store);
    if (inspector->threads_store) g_object_unref(inspector->threads_store);
    if (inspector->family_store) g_object_unref(inspector->family_store);
    g_free(inspector);
}

static void refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    ProcessInspector *inspector = user_data;
    refresh_inventories(inspector);
    (void)inspector_update(inspector);
}

/* Controls express neutral actions; the selected platform backend translates
 * them to its native process-control mechanism. */
static void apply_priority(GtkButton *button, gpointer user_data)
{
    (void)button;
    ProcessInspector *inspector = user_data;
    if (!require_current_identity(inspector, "Unable to change process priority"))
        return;
    const int selected = gtk_combo_box_get_active(
        GTK_COMBO_BOX(inspector->priority_combo));
    if (selected < (int)LSM_PROCESS_PRIORITY_HIGH ||
        selected > (int)LSM_PROCESS_PRIORITY_LOW) return;
    if (!lsm_process_set_priority(
            inspector->pid, inspector->instance_id,
            (LsmProcessPriority)selected)) {
        char error[160];
        lsm_process_error_message(error, sizeof(error));
        lsm_ui_show_error(GTK_WINDOW(inspector->window),
                          "Unable to change process priority", "%s", error);
    }
}

static void affinity_apply(ProcessInspector *inspector)
{
    if (!require_current_identity(inspector, "Unable to read CPU affinity"))
        return;
    bool enabled[LSM_MAX_CPUS] = {0};
    const size_t count = lsm_process_affinity_get(
        inspector->pid, inspector->instance_id, enabled, LSM_MAX_CPUS);
    if (!count) {
        char error[160];
        lsm_process_error_message(error, sizeof(error));
        lsm_ui_show_error(GTK_WINDOW(inspector->window),
                          "Unable to read CPU affinity", "%s", error);
        return;
    }
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Set CPU affinity",
        GTK_WINDOW(inspector->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 620, 480);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    GtkWidget *description = gtk_label_new(
        "Select the logical processors on which this process may run.");
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), description, FALSE, FALSE, 0);
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroller, TRUE);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_container_add(GTK_CONTAINER(scroller), grid);
    gtk_box_pack_start(GTK_BOX(content), scroller, TRUE, TRUE, 8);
    GtkWidget **checks = g_new0(GtkWidget *, count);
    for (size_t index = 0U; index < count; index++) {
        char label[32];
        snprintf(label, sizeof(label), "CPU %zu", index);
        checks[index] = gtk_check_button_new_with_label(label);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checks[index]),
                                     enabled[index]);
        gtk_grid_attach(GTK_GRID(grid), checks[index], (int)(index % 6U),
                        (int)(index / 6U), 1, 1);
    }
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        bool any = false;
        for (size_t index = 0U; index < count; index++) {
            enabled[index] = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(checks[index]));
            any = any || enabled[index];
        }
        if (!any)
            lsm_ui_show_error(GTK_WINDOW(inspector->window),
                              "Unable to set CPU affinity",
                              "At least one CPU must be selected.");
        else if (!require_current_identity(inspector,
                     "Unable to set CPU affinity")) {
            /* The error dialog is displayed by require_current_identity(). */
        } else if (!lsm_process_affinity_set(
                       inspector->pid, inspector->instance_id,
                       enabled, count))
            {
            char error[160];
            lsm_process_error_message(error, sizeof(error));
            lsm_ui_show_error(GTK_WINDOW(inspector->window),
                              "Unable to set CPU affinity", "%s", error);
        }
    }
    g_free(checks);
    gtk_widget_destroy(dialog);
}

static void affinity_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    affinity_apply(user_data);
}

static void end_process_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    ProcessInspector *inspector = user_data;
    if (!require_current_identity(inspector, "Unable to end process")) return;
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(inspector->window),
        GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
        "End process %llu?", (unsigned long long)inspector->pid);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
        "The process will be asked to shut down cleanly and save its state.");
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL,
                           "End process", GTK_RESPONSE_ACCEPT, NULL);
    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response == GTK_RESPONSE_ACCEPT) {
        if (!require_current_identity(inspector, "Unable to end process"))
            return;
        if (!lsm_process_control(inspector->pid, inspector->instance_id,
                                 LSM_PROCESS_CONTROL_TERMINATE)) {
            char error[160];
            lsm_process_error_message(error, sizeof(error));
            lsm_ui_show_error(GTK_WINDOW(inspector->window),
                              "Unable to end the process", "%s", error);
        }
    }
}

static GtkWidget *build_control_bar(ProcessInspector *inspector)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *refresh = gtk_button_new_with_label("Refresh inspection");
    GtkWidget *affinity = gtk_button_new_with_label("CPU affinity…");
    GtkWidget *end = gtk_button_new_with_label("End process");
    inspector->priority_combo = gtk_combo_box_text_new();
    static const char *priorities[] = {
        "High", "Above normal", "Normal", "Below normal", "Low"
    };
    for (size_t index = 0U; index < G_N_ELEMENTS(priorities); index++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(inspector->priority_combo),
                                       priorities[index]);
    const LsmProcessInfo *process = snapshot_process(inspector->app,
        inspector->pid, inspector->instance_id);
    gtk_combo_box_set_active(GTK_COMBO_BOX(inspector->priority_combo),
        process ? priority_index(process->priority) :
                  (int)LSM_PROCESS_PRIORITY_NORMAL);
    GtkWidget *apply = gtk_button_new_with_label("Apply priority");
    gtk_box_pack_start(GTK_BOX(bar), refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), inspector->priority_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), apply, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), affinity, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bar), end, FALSE, FALSE, 0);
    g_signal_connect(refresh, "clicked", G_CALLBACK(refresh_clicked), inspector);
    g_signal_connect(apply, "clicked", G_CALLBACK(apply_priority), inspector);
    g_signal_connect(affinity, "clicked", G_CALLBACK(affinity_clicked), inspector);
    g_signal_connect(end, "clicked", G_CALLBACK(end_process_clicked), inspector);
    return bar;
}

void lsm_process_inspector_show(LsmApp *app, LsmProcessId pid,
                                LsmProcessInstanceId instance_id)
{
    if (!app || pid < 1 || instance_id == 0U) return;
    const LsmProcessInfo *process = NULL;
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++)
        if (app->process.process_snapshot[index].pid == pid &&
            app->process.process_snapshot[index].instance_id == instance_id) {
            process = &app->process.process_snapshot[index];
            break;
        }
    if (!process) return;

    ProcessInspector *inspector = g_new0(ProcessInspector, 1U);
    inspector->app = app;
    inspector->pid = pid;
    inspector->instance_id = instance_id;
    inspector->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    char title[256];
    snprintf(title, sizeof(title), "%s — Process Inspector", process->name);
    gtk_window_set_title(GTK_WINDOW(inspector->window), title);
    gtk_window_set_default_size(GTK_WINDOW(inspector->window), 980, 720);
    gtk_window_set_transient_for(GTK_WINDOW(inspector->window),
                                 GTK_WINDOW(app->shell.window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(inspector->window), TRUE);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 10);
    gtk_container_add(GTK_CONTAINER(inspector->window), outer);
    inspector->identity_label = gtk_label_new(NULL);
    gtk_widget_set_halign(inspector->identity_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(outer), inspector->identity_label,
                       FALSE, FALSE, 0);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(outer), notebook, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        build_overview_page(inspector), gtk_label_new("Overview"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        build_performance_page(inspector), gtk_label_new("Performance"));

    inspector->open_files_store = gtk_list_store_new(3,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    static const char *file_titles[] = {"FD", "Type", "Target"};
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        tree_page(inspector->open_files_store, file_titles,
                  G_N_ELEMENTS(file_titles), 2, &inspector->open_files_status),
        gtk_label_new("Open Files"));

    inspector->maps_store = gtk_list_store_new(7,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    static const char *map_titles[] = {
        "Address", "Size", "Permissions", "Offset", "Device", "Inode", "Path"
    };
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        tree_page(inspector->maps_store, map_titles,
                  G_N_ELEMENTS(map_titles), 6, &inspector->maps_status),
        gtk_label_new("Memory Map"));

    inspector->threads_store = gtk_list_store_new(3,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    static const char *thread_titles[] = {"TID", "Name", "State"};
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        tree_page(inspector->threads_store, thread_titles,
                  G_N_ELEMENTS(thread_titles), 1, &inspector->threads_status),
        gtk_label_new("Threads"));

    inspector->family_store = gtk_list_store_new(6,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING);
    static const char *family_titles[] = {
        "Relationship", "PID", "Name", "State", "CPU", "Memory"
    };
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        tree_page(inspector->family_store, family_titles,
                  G_N_ELEMENTS(family_titles), 2, &inspector->family_status),
        gtk_label_new("Process Family"));

    gtk_box_pack_start(GTK_BOX(outer), build_control_bar(inspector),
                       FALSE, FALSE, 0);
    g_signal_connect(inspector->window, "destroy",
                     G_CALLBACK(inspector_destroy), inspector);
    refresh_inventories(inspector);
    (void)inspector_update(inspector);
    inspector->refresh_timer = g_timeout_add_seconds(1U, inspector_update,
                                                     inspector);
    gtk_widget_show_all(inspector->window);
}

/* Exact-file ownership is an explicit diagnostic operation rather than a
 * background scan because its cost grows with process and open-resource count. */
static void show_file_users_results(LsmApp *app, const char *path,
                                    LsmFileUserInfo *items, size_t count)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Processes using file",
        GTK_WINDOW(app->shell.window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 820, 520);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    GtkWidget *description = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped(
        "<b>%zu matching descriptor%s</b>\n%s", count, count == 1U ? "" : "s",
        path);
    gtk_label_set_markup(GTK_LABEL(description), markup);
    g_free(markup);
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_label_set_selectable(GTK_LABEL(description), TRUE);
    gtk_box_pack_start(GTK_BOX(content), description, FALSE, FALSE, 0);
    GtkListStore *store = gtk_list_store_new(4,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    for (size_t index = 0U; index < count; index++) {
        GtkTreeIter iterator;
        char pid[32], descriptor[32];
        snprintf(pid, sizeof(pid), "%llu",
                 (unsigned long long)items[index].pid);
        snprintf(descriptor, sizeof(descriptor), "%d", items[index].descriptor);
        gtk_list_store_append(store, &iterator);
        gtk_list_store_set(store, &iterator, 0, pid, 1, items[index].process_name,
                           2, descriptor, 3, items[index].target, -1);
    }
    static const char *titles[] = {"PID", "Process", "FD", "Target"};
    GtkWidget *result_status = NULL;
    GtkWidget *tree = tree_page(store, titles, G_N_ELEMENTS(titles), 3,
                                &result_status);
    lsm_ui_set_label_text(result_status, "%zu matching descriptor%s", count,
                          count == 1U ? "" : "s");
    gtk_box_pack_start(GTK_BOX(content), tree, TRUE, TRUE, 8);
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_object_unref(store);
}

void lsm_process_file_users_show(LsmApp *app)
{
    if (!app) return;
    GtkWidget *chooser = gtk_file_chooser_dialog_new("Find processes using a file",
        GTK_WINDOW(app->shell.window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, "Search", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(chooser);
        return;
    }
    char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    gtk_widget_destroy(chooser);
    if (!path) return;
    LsmFileUserInfo *items = NULL;
    const size_t count = lsm_process_inspection_find_file_users(path, &items);
    show_file_users_results(app, path, items, count);
    lsm_process_inspection_free(items);
    g_free(path);
}
