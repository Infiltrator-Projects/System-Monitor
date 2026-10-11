// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file details_page.c
 * @brief Technical Details table, filtering, control and CSV recording.
 *
 * The process page supports both a true parent/child tree and a sortable flat
 * list. The GTK model is rebuilt from one retained process snapshot so search,
 * column selection and view changes do not trigger unnecessary procfs scans.
 * Shared process sampling and cross-page coordination belong to
 * process_workspace.c.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "details_page.h"
#include "details_layout.h"
#include "atomic_file.h"
#include "app_internal.h"
#include "common.h"
#include "process_inspector.h"
#include "process_table_ui.h"
#include "ui_helpers.h"

#include <infiltratr/config.h>
#include <infiltratr/format.h>

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const LsmProcessTableColumnSpec lsm_details_column_specs[PROC_N_COLUMNS] = {
    {PROC_COL_NAME,             "name",       "Name",              LSM_PROCESS_TABLE_TEXT,         TRUE,  TRUE,  170, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_PID,              "pid",        "PID",               LSM_PROCESS_TABLE_UINT64,       TRUE,  FALSE, 70,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_PPID,             "ppid",       "Parent PID",        LSM_PROCESS_TABLE_UINT64,       FALSE, FALSE, 85,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_USER,             "user",       "User",              LSM_PROCESS_TABLE_TEXT,         TRUE,  FALSE, 95,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_STATE,            "status",     "Status",            LSM_PROCESS_TABLE_TEXT,         TRUE,  FALSE, 90,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_CPU,              "cpu",        "CPU",               LSM_PROCESS_TABLE_CPU_PERCENT,  TRUE,  FALSE, 70,  LSM_PROCESS_TABLE_HEAT_PERCENT},
    {PROC_COL_CPU_TIME,         "cpu_time",   "CPU time",          LSM_PROCESS_TABLE_DURATION,     FALSE, FALSE, 95,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_MEMORY,           "memory",     "Memory %",          LSM_PROCESS_TABLE_PERCENT,      TRUE,  FALSE, 80,  LSM_PROCESS_TABLE_HEAT_PERCENT},
    {PROC_COL_RSS,              "rss",        "Resident memory",   LSM_PROCESS_TABLE_BYTES,        TRUE,  FALSE, 110, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_VIRTUAL_MEMORY,   "virtual",    "Virtual memory",    LSM_PROCESS_TABLE_BYTES,        FALSE, FALSE, 110, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_WRITABLE_MEMORY,  "writable",   "Writable memory",   LSM_PROCESS_TABLE_BYTES,        FALSE, FALSE, 115, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_SHARED_MEMORY,    "shared",     "Shared memory",     LSM_PROCESS_TABLE_BYTES,        FALSE, FALSE, 110, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_THREADS,          "threads",    "Threads",           LSM_PROCESS_TABLE_UNSIGNED,     FALSE, FALSE, 70,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_READ_RATE,        "read_rate",  "Read/s",            LSM_PROCESS_TABLE_RATE,         TRUE,  FALSE, 90,  LSM_PROCESS_TABLE_HEAT_RATE},
    {PROC_COL_WRITE_RATE,       "write_rate", "Write/s",           LSM_PROCESS_TABLE_RATE,         TRUE,  FALSE, 90,  LSM_PROCESS_TABLE_HEAT_RATE},
    {PROC_COL_GPU,              "gpu",        "GPU",               LSM_PROCESS_TABLE_PERCENT,      TRUE,  FALSE, 70,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_GPU_ENGINE,       "gpu_engine", "GPU engine",        LSM_PROCESS_TABLE_TEXT,         FALSE, FALSE, 105, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_GPU_MEMORY,       "gpu_memory", "GPU memory",        LSM_PROCESS_TABLE_BYTES,        FALSE, FALSE, 105, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_READ_TOTAL,       "read_total", "Read total",        LSM_PROCESS_TABLE_BYTES,        FALSE, FALSE, 95,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_WRITE_TOTAL,      "write_total","Written total",     LSM_PROCESS_TABLE_BYTES,        FALSE, FALSE, 105, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_HANDLE_COUNT,     "handles",    "Handles",           LSM_PROCESS_TABLE_HANDLE_COUNT, FALSE, FALSE, 105, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_CONTEXT_SWITCHES, "contexts",   "Context switches",  LSM_PROCESS_TABLE_UINT64,       FALSE, FALSE, 110, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_PAGE_FAULTS,      "faults",     "Page faults",       LSM_PROCESS_TABLE_UINT64,       FALSE, FALSE, 90,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_PRIORITY,         "priority",   "Priority",          LSM_PROCESS_TABLE_PRIORITY,     FALSE, FALSE, 105, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_NICE,             "nice",       "Nice",              LSM_PROCESS_TABLE_SIGNED,       FALSE, FALSE, 65,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_SECURITY_CONTEXT, "security",   "Security context",  LSM_PROCESS_TABLE_TEXT,         FALSE, TRUE,  180, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_WCHAN,            "wchan",      "Waiting channel",   LSM_PROCESS_TABLE_TEXT,         FALSE, TRUE,  145, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_CGROUP,           "cgroup",     "Control group",     LSM_PROCESS_TABLE_TEXT,         FALSE, TRUE,  220, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_UNIT,             "unit",       "Unit",              LSM_PROCESS_TABLE_TEXT,         FALSE, TRUE,  170, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_SESSION,          "session",    "Session",           LSM_PROCESS_TABLE_TEXT,         FALSE, FALSE, 85,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_SEAT,             "seat",       "Seat",              LSM_PROCESS_TABLE_TEXT,         FALSE, FALSE, 85,  LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_OWNER,            "owner",      "Owner",             LSM_PROCESS_TABLE_TEXT,         FALSE, FALSE, 105, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_START_TIME,       "started",    "Start time",        LSM_PROCESS_TABLE_TIME,         FALSE, FALSE, 145, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_ELAPSED,          "elapsed",    "Elapsed",           LSM_PROCESS_TABLE_ELAPSED,      FALSE, FALSE, 100, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_EXECUTABLE,       "executable", "Executable",        LSM_PROCESS_TABLE_TEXT,         FALSE, TRUE,  260, LSM_PROCESS_TABLE_HEAT_NONE},
    {PROC_COL_COMMAND,          "command",    "Command",            LSM_PROCESS_TABLE_TEXT,         FALSE, TRUE,  280, LSM_PROCESS_TABLE_HEAT_NONE}
};

typedef struct {
    LsmApp *app;
    const LsmProcessInfo *processes;
    size_t count;
    gboolean *visible;
    gboolean *inserted;
    unsigned char *state;
    GtkTreeIter *iters;
    GHashTable *pid_to_index;
} ProcessBuildContext;

static void sync_process_table_presentation(LsmApp *app)
{
    if (!app || !app->details.details_tree) return;
    const LsmProcessTablePresentation presentation = {
        .cpu_percent_per_core = app->runtime.process_cpu_per_core,
        .logical_cores = app->monitor.cpu.logical_cores,
        .heatmap = app->details.process_heatmap
    };
    lsm_process_table_set_presentation(
        app->details.details_tree, &presentation);
}

static gboolean process_directly_visible(const LsmApp *app,
                                         const LsmProcessInfo *process)
{
    gboolean scope_visible = TRUE;
    if (app->details.details_scope == PROCESS_SCOPE_MINE)
        scope_visible = process->owned_by_current_user;
    else if (app->details.details_scope == PROCESS_SCOPE_ACTIVE)
        scope_visible =
            strcmp(process->state, "Sleeping") != 0 &&
            strcmp(process->state, "Stopped") != 0 &&
            strcmp(process->state, "Zombie") != 0 &&
            strcmp(process->state, "Idle") != 0 &&
            strcmp(process->state, "Dead") != 0;

    char pid_text[32];
    snprintf(pid_text, sizeof(pid_text), "%llu",
             (unsigned long long)process->pid);
    const char *search = gtk_entry_get_text(GTK_ENTRY(app->details.details_search));
    gboolean visible = scope_visible && (!search || !*search ||
        lsm_ui_text_matches(process->name, search) ||
        lsm_ui_text_matches(process->user, search) ||
        lsm_ui_text_matches(process->command, search) ||
        lsm_ui_text_matches(pid_text, search));

    if (visible) {
        for (guint i = 0; i < app->process.filters->len; i++) {
            const char *filter = g_ptr_array_index(app->process.filters, i);
            if (lsm_ui_text_matches(process->name, filter) ||
                lsm_ui_text_matches(process->user, filter) ||
                lsm_ui_text_matches(process->command, filter)) {
                visible = FALSE;
                break;
            }
        }
    }
    return visible;
}





unsigned lsm_details_process_scan_flags(const LsmApp *app)
{
    if (!app || !app->runtime.page_built[LSM_TAB_DETAILS] ||
        !app->details.details_tree)
        return 0U;

    unsigned flags = 0U;
    if (gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_HANDLE_COUNT]))
        flags |= LSM_PROCESS_SCAN_HANDLE_COUNT;
    if (gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_WRITABLE_MEMORY]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_SECURITY_CONTEXT]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_WCHAN]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_CGROUP]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_UNIT]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_SESSION]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_SEAT]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_OWNER]))
        flags |= LSM_PROCESS_SCAN_TECHNICAL | LSM_PROCESS_SCAN_CGROUP;
    if (gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_GPU]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_GPU_ENGINE]) ||
        gtk_tree_view_column_get_visible(
            app->details.details_columns[PROC_COL_GPU_MEMORY]))
        flags |= LSM_PROCESS_SCAN_GPU;
    return flags;
}

void lsm_details_show_columns(LsmApp *app)
{
    if (!app || !app->details.details_tree) return;
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Select process columns", GTK_WINDOW(app->shell.window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_ACCEPT, NULL);
    lsm_ui_set_workarea_default_size(GTK_WINDOW(dialog), 650, 430);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget *description = gtk_label_new(
        "Choose the fields displayed in the process table. Executable identity "
        "is retained for App History; handle counts, technical fields and "
        "graphics counters are collected only while their columns are in use.");
    gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), description, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    GtkWidget *checks[PROC_N_COLUMNS];
    for (int i = 0; i < PROC_N_COLUMNS; i++) {
        checks[i] = gtk_check_button_new_with_label(lsm_details_column_specs[i].title);
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(checks[i]),
            gtk_tree_view_column_get_visible(app->details.details_columns[i]));
        if (i == PROC_COL_NAME) gtk_widget_set_sensitive(checks[i], FALSE);
        gtk_grid_attach(GTK_GRID(grid), checks[i], i % 3, i / 3, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 12);

    GtkWidget *heatmap = gtk_check_button_new_with_label(
        "Highlight CPU, memory and disk-I/O usage");
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(heatmap), app->details.process_heatmap);
    gtk_box_pack_start(GTK_BOX(content), heatmap, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        for (int i = 0; i < PROC_N_COLUMNS; i++)
            gtk_tree_view_column_set_visible(
                app->details.details_columns[i],
                i == PROC_COL_NAME ||
                    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checks[i])));
        app->details.process_heatmap =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(heatmap));
        sync_process_table_presentation(app);
        lsm_details_save_layout(app);
        gtk_widget_queue_draw(app->details.details_tree);
        (void)lsm_process_workspace_update(app);
    }
    gtk_widget_destroy(dialog);
}

static void set_process_row(GtkTreeStore *store, GtkTreeIter *iter,
                            const LsmProcessInfo *process)
{
    gtk_tree_store_set(store, iter,
        PROC_COL_NAME, process->name,
        PROC_COL_PID, process->pid,
        PROC_COL_PPID, process->ppid,
        PROC_COL_USER, process->user,
        PROC_COL_STATE, process->state,
        PROC_COL_CPU, process->cpu_percent,
        PROC_COL_CPU_TIME, process->cpu_time_seconds,
        PROC_COL_MEMORY, process->memory_percent,
        PROC_COL_RSS, process->rss_bytes,
        PROC_COL_VIRTUAL_MEMORY, process->virtual_memory_available
            ? process->virtual_memory_bytes : UINT64_MAX,
        PROC_COL_WRITABLE_MEMORY, process->writable_memory_available
            ? process->writable_memory_bytes : UINT64_MAX,
        PROC_COL_SHARED_MEMORY, process->shared_memory_available
            ? process->shared_memory_bytes : UINT64_MAX,
        PROC_COL_THREADS, process->threads,
        PROC_COL_READ_RATE, process->io_rate_available
            ? process->read_bytes_per_sec : NAN,
        PROC_COL_WRITE_RATE, process->io_rate_available
            ? process->write_bytes_per_sec : NAN,
        PROC_COL_GPU, process->gpu_available ? process->gpu_percent : NAN,
        PROC_COL_GPU_ENGINE, process->gpu_available && process->gpu_engine[0]
            ? process->gpu_engine : "N/A",
        PROC_COL_GPU_MEMORY, process->gpu_memory_available
            ? process->gpu_memory_bytes : UINT64_MAX,
        PROC_COL_READ_TOTAL, process->io_totals_available
            ? process->read_bytes : UINT64_MAX,
        PROC_COL_WRITE_TOTAL, process->io_totals_available
            ? process->write_bytes : UINT64_MAX,
        PROC_COL_HANDLE_COUNT, process->handle_count_available
            ? process->handle_count : UINT_MAX,
        PROC_COL_CONTEXT_SWITCHES, process->context_switches,
        PROC_COL_PAGE_FAULTS, process->page_faults,
        PROC_COL_PRIORITY, (gint)process->priority,
        PROC_COL_NICE, process->nice_value,
        PROC_COL_SECURITY_CONTEXT, process->security_context[0]
            ? process->security_context : "N/A",
        PROC_COL_WCHAN, process->waiting_channel[0]
            ? process->waiting_channel : "N/A",
        PROC_COL_CGROUP, process->cgroup_path[0]
            ? process->cgroup_path : "N/A",
        PROC_COL_UNIT, process->unit[0] ? process->unit : "N/A",
        PROC_COL_SESSION, process->session[0] ? process->session : "N/A",
        PROC_COL_SEAT, process->seat[0] ? process->seat : "N/A",
        PROC_COL_OWNER, process->owner[0] ? process->owner : "N/A",
        PROC_COL_START_TIME, process->start_time_epoch,
        PROC_COL_ELAPSED, process->elapsed_seconds,
        PROC_COL_EXECUTABLE, process->executable,
        PROC_COL_COMMAND, process->command,
        -1);
}

static uint64_t process_structure_signature(const LsmApp *app)
{
    uint64_t xor_value = app && app->details.details_tree_mode
        ? UINT64_C(0x6a09e667f3bcc909)
        : UINT64_C(0xbb67ae8584caa73b);
    uint64_t sum_value = app
        ? (uint64_t)app->process.process_snapshot_count *
          UINT64_C(0x9e3779b185ebca87) : 0U;
    if (!app) return xor_value;

    for (size_t index = 0U;
         index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process =
            &app->process.process_snapshot[index];
        uint64_t row = LSM_FNV1A64_OFFSET_BASIS;
        row = lsm_fnv1a64_mix_u64_le(row, (uint64_t)process->pid);
        row = lsm_fnv1a64_mix_u64_le(row, process->instance_id);
        row = lsm_fnv1a64_mix_u64_le(row, (uint64_t)process->ppid);
        const unsigned shift = (unsigned)(process->pid % 63U) + 1U;
        const uint64_t rotated =
            (row << shift) | (row >> (64U - shift));
        xor_value ^= rotated;
        sum_value += row * UINT64_C(0x94d049bb133111eb);
    }
    return xor_value ^ sum_value;
}

static gboolean update_details_branch(LsmApp *app, GHashTable *pid_index,
                                      GtkTreeIter *parent)
{
    GtkTreeModel *model = GTK_TREE_MODEL(app->details.details_store);
    GtkTreeIter iter;
    gboolean valid = parent
        ? gtk_tree_model_iter_children(model, &iter, parent)
        : gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        guint64 pid = 0U;
        gtk_tree_model_get(model, &iter, PROC_COL_PID, &pid, -1);
        if (pid == 0U || pid > UINT_MAX) return FALSE;
        gpointer value = g_hash_table_lookup(
            pid_index, GUINT_TO_POINTER((guint)pid));
        if (!value) return FALSE;
        const size_t index = (size_t)(GPOINTER_TO_UINT(value) - 1U);
        set_process_row(app->details.details_store, &iter,
                        &app->process.process_snapshot[index]);
        if (!update_details_branch(app, pid_index, &iter)) return FALSE;
        valid = gtk_tree_model_iter_next(model, &iter);
    }
    return TRUE;
}

static gboolean update_details_model_values(LsmApp *app)
{
    if (!app || !app->details.details_store) return FALSE;
    GHashTable *pid_index = g_hash_table_new(g_direct_hash, g_direct_equal);
    if (!pid_index) return FALSE;
    for (size_t index = 0U;
         index < app->process.process_snapshot_count; index++) {
        const LsmProcessId pid = app->process.process_snapshot[index].pid;
        if (pid == 0U || pid > UINT_MAX || index >= UINT_MAX) continue;
        g_hash_table_insert(pid_index,
                            GUINT_TO_POINTER((guint)pid),
                            GUINT_TO_POINTER((guint)index + 1U));
    }
    const gboolean okay = update_details_branch(app, pid_index, NULL);
    g_hash_table_destroy(pid_index);
    return okay;
}

static ssize_t process_index_for_pid(ProcessBuildContext *context,
                                     LsmProcessId pid)
{
    if (!context || pid == 0U || pid > (LsmProcessId)UINT_MAX) return -1;
    gpointer value = g_hash_table_lookup(
        context->pid_to_index, GUINT_TO_POINTER((guint)pid));
    return value ? (ssize_t)(GPOINTER_TO_UINT(value) - 1U) : -1;
}

static void append_process_recursive(ProcessBuildContext *context, size_t index)
{
    if (context->inserted[index] || !context->visible[index]) return;
    if (context->state[index] == 1) context->state[index] = 0;
    context->state[index] = 1;

    GtkTreeIter *parent = NULL;
    if (context->app->details.details_tree_mode) {
        ssize_t parent_index = process_index_for_pid(
            context, context->processes[index].ppid);
        if (parent_index >= 0 && (size_t)parent_index != index &&
            context->visible[parent_index] &&
            context->state[parent_index] != 1) {
            append_process_recursive(context, (size_t)parent_index);
            if (context->inserted[parent_index])
                parent = &context->iters[parent_index];
        }
    }

    gtk_tree_store_append(context->app->details.details_store,
                          &context->iters[index], parent);
    set_process_row(context->app->details.details_store,
                    &context->iters[index], &context->processes[index]);
    context->inserted[index] = TRUE;
    context->state[index] = 2;
}

static void collect_expanded_pid(GtkTreeView *tree, GtkTreePath *path,
                                 gpointer user_data)
{
    GHashTable *expanded = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tree);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        guint64 pid = 0U;
        gtk_tree_model_get(model, &iter, PROC_COL_PID, &pid, -1);
        if (pid > 0U && pid <= UINT_MAX)
            g_hash_table_add(expanded, GUINT_TO_POINTER((guint)pid));
    }
}

static void select_sorted_path(LsmApp *app, GtkTreeIter *child_iter,
                               gboolean expand)
{
    GtkTreePath *child_path = gtk_tree_model_get_path(
        GTK_TREE_MODEL(app->details.details_store), child_iter);
    GtkTreePath *sorted_path = gtk_tree_model_sort_convert_child_path_to_path(
        GTK_TREE_MODEL_SORT(app->details.details_sort_model), child_path);
    if (sorted_path) {
        if (expand)
            gtk_tree_view_expand_row(
                GTK_TREE_VIEW(app->details.details_tree), sorted_path, FALSE);
        else {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(
                GTK_TREE_VIEW(app->details.details_tree));
            gtk_tree_selection_select_path(selection, sorted_path);
            gtk_tree_view_scroll_to_cell(
                GTK_TREE_VIEW(app->details.details_tree), sorted_path,
                NULL, FALSE, 0.0, 0.0);
        }
        gtk_tree_path_free(sorted_path);
    }
    gtk_tree_path_free(child_path);
}

static void rebuild_details_model(LsmApp *app)
{
    size_t count = app->process.process_snapshot_count;
    LsmProcessId desired_pid = app->process.selected_pid;
    const LsmProcessInstanceId desired_instance_id =
        app->process.selected_instance_id;
    GHashTable *expanded = g_hash_table_new(g_direct_hash, g_direct_equal);
    if (app->details.details_tree_mode && app->details.details_tree_initialized)
        gtk_tree_view_map_expanded_rows(
            GTK_TREE_VIEW(app->details.details_tree),
            collect_expanded_pid, expanded);

    gboolean *direct = g_new0(gboolean, count);
    gboolean *visible = g_new0(gboolean, count);
    gboolean *inserted = g_new0(gboolean, count);
    unsigned char *state = g_new0(unsigned char, count);
    GtkTreeIter *iters = g_new0(GtkTreeIter, count);
    GHashTable *pid_to_index = g_hash_table_new(g_direct_hash, g_direct_equal);

    for (size_t i = 0; i < count; i++) {
        const LsmProcessId pid = app->process.process_snapshot[i].pid;
        if (pid > 0U && pid <= (LsmProcessId)UINT_MAX && i < UINT_MAX)
            g_hash_table_insert(pid_to_index,
                                GUINT_TO_POINTER((guint)pid),
                                GUINT_TO_POINTER((guint)i + 1U));
        direct[i] = process_directly_visible(
            app, &app->process.process_snapshot[i]);
        visible[i] = direct[i];
    }

    if (app->details.details_tree_mode) {
        for (size_t i = 0; i < count; i++) {
            if (!direct[i]) continue;
            LsmProcessId parent = app->process.process_snapshot[i].ppid;
            for (size_t guard = 0; parent > 0 && guard < count; guard++) {
                if (parent > (LsmProcessId)UINT_MAX) break;
                gpointer value = g_hash_table_lookup(
                    pid_to_index, GUINT_TO_POINTER((guint)parent));
                if (!value) break;
                size_t parent_index = GPOINTER_TO_UINT(value) - 1U;
                visible[parent_index] = TRUE;
                LsmProcessId next_parent =
                    app->process.process_snapshot[parent_index].ppid;
                if (next_parent == parent) break;
                parent = next_parent;
            }
        }
    }

    gtk_tree_store_clear(app->details.details_store);
    ProcessBuildContext context = {
        .app = app,
        .processes = app->process.process_snapshot,
        .count = count,
        .visible = visible,
        .inserted = inserted,
        .state = state,
        .iters = iters,
        .pid_to_index = pid_to_index
    };
    size_t shown = 0;
    for (size_t i = 0; i < count; i++) {
        if (visible[i]) {
            append_process_recursive(&context, i);
            shown++;
        }
    }

    const char *active_search = gtk_entry_get_text(
        GTK_ENTRY(app->details.details_search));
    gboolean searching = active_search && *active_search;
    for (size_t i = 0; i < count; i++) {
        if (!inserted[i]) continue;
        if (desired_pid > 0 &&
            app->process.process_snapshot[i].pid == desired_pid &&
            app->process.process_snapshot[i].instance_id == desired_instance_id)
            select_sorted_path(app, &iters[i], FALSE);
        if (app->details.details_tree_mode &&
            (searching ||
             (app->process.process_snapshot[i].pid <=
                  (LsmProcessId)UINT_MAX &&
              g_hash_table_contains(
                  expanded,
                  GUINT_TO_POINTER(
                      (guint)app->process.process_snapshot[i].pid))) ||
             (visible[i] && !direct[i])))
            select_sorted_path(app, &iters[i], TRUE);
    }

    if (app->details.details_tree_mode &&
        !app->details.details_tree_initialized) {
        for (size_t i = 0; i < count; i++) {
            if (!inserted[i]) continue;
            ssize_t parent = process_index_for_pid(
                &context, app->process.process_snapshot[i].ppid);
            if (parent < 0 || !visible[parent])
                select_sorted_path(app, &iters[i], TRUE);
        }
        app->details.details_tree_initialized = TRUE;
    }

    lsm_ui_set_label_text(app->details.details_count_label,
        shown == count ? "Processes: %zu" : "Processes: %zu  Showing: %zu",
        count, shown);

    g_hash_table_destroy(expanded);
    g_hash_table_destroy(pid_to_index);
    g_free(direct);
    g_free(visible);
    g_free(inserted);
    g_free(state);
    g_free(iters);
}

void lsm_details_present_snapshot(LsmApp *app)
{
    if (!app) return;
    sync_process_table_presentation(app);
    if (!app->details.details_model_dirty || !app->details.details_store)
        return;
    const uint64_t signature = process_structure_signature(app);
    if (!app->details.details_structure_valid ||
        app->details.details_structure_signature != signature ||
        !update_details_model_values(app)) {
        rebuild_details_model(app);
        app->details.details_structure_signature = signature;
        app->details.details_structure_valid = TRUE;
    }
    app->details.details_model_dirty = FALSE;
}

static void refilter_processes(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    LsmApp *app = user_data;
    app->details.details_structure_valid = FALSE;
    app->details.details_model_dirty = TRUE;
    lsm_details_present_snapshot(app);
}

static void selected_process_changed(GtkTreeSelection *selection,
                                     gpointer user_data)
{
    LsmApp *app = user_data;
    lsm_process_group_selection_clear(app);
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        guint64 pid = 0U;
        gtk_tree_model_get(model, &iter, PROC_COL_PID, &pid, -1);
        lsm_process_selection_set(app, pid);
        gboolean valid = pid > 1;
        gtk_widget_set_sensitive(app->details.details_end_button, valid);
        gtk_widget_set_sensitive(app->details.details_inspect_button, pid > 0);
        lsm_process_record_action_sync(app, valid, FALSE);
    } else {
        lsm_process_selection_set(app, 0U);
        gtk_widget_set_sensitive(app->details.details_end_button, FALSE);
        gtk_widget_set_sensitive(app->details.details_inspect_button, FALSE);
        lsm_process_record_action_sync(app, FALSE, FALSE);
    }
}

static gboolean process_button_press(GtkWidget *widget,
                                     GdkEventButton *event,
                                     gpointer user_data)
{
    LsmApp *app = user_data;
    if (event->type != GDK_BUTTON_PRESS || event->button != 3) return FALSE;
    GtkTreePath *path = NULL;
    if (!gtk_tree_view_get_path_at_pos(
            GTK_TREE_VIEW(widget), (gint)event->x, (gint)event->y,
            &path, NULL, NULL, NULL))
        return FALSE;
    GtkTreeSelection *selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);
    GtkWidget *menu = lsm_process_actions_menu(app, TRUE);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    return TRUE;
}

static void process_row_activated(GtkTreeView *tree, GtkTreePath *path,
                                  GtkTreeViewColumn *column,
                                  gpointer user_data)
{
    (void)tree;
    (void)path;
    (void)column;
    LsmApp *app = user_data;
    lsm_process_inspector_show(app, app->process.selected_pid,
                               app->process.selected_instance_id);
}

static void kill_selected_process(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_processes_end_selected(user_data);
}

static void details_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_processes_show_selected_details(user_data);
}

static void process_scope_changed(GtkComboBox *combo, gpointer user_data)
{
    LsmApp *app = user_data;
    const gint scope = gtk_combo_box_get_active(combo);
    if (scope < PROCESS_SCOPE_ALL || scope > PROCESS_SCOPE_MINE ||
        scope == app->details.details_scope)
        return;
    app->details.details_scope = scope;
    app->details.details_structure_valid = FALSE;
    app->details.details_model_dirty = TRUE;
    lsm_details_save_layout(app);
    lsm_details_present_snapshot(app);
}

static void process_view_changed(GtkComboBox *combo, gpointer user_data)
{
    LsmApp *app = user_data;
    gboolean tree_mode = gtk_combo_box_get_active(combo) == 0;
    if (tree_mode != app->details.details_tree_mode) {
        app->details.details_tree_mode = tree_mode;
        if (tree_mode) app->details.details_tree_initialized = FALSE;
        app->details.details_structure_valid = FALSE;
        app->details.details_model_dirty = TRUE;
        lsm_details_save_layout(app);
        lsm_details_present_snapshot(app);
    }
}

void lsm_details_build(LsmApp *app, GtkWidget *container)
{
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 8);
    gtk_container_add(GTK_CONTAINER(container), outer);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app->details.details_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->details.details_search),
        "Search process name, owner, or command");
    gtk_widget_set_hexpand(app->details.details_search, TRUE);
    app->details.details_scope_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(
        GTK_COMBO_BOX_TEXT(app->details.details_scope_combo), "All processes");
    gtk_combo_box_text_append_text(
        GTK_COMBO_BOX_TEXT(app->details.details_scope_combo), "Active processes");
    gtk_combo_box_text_append_text(
        GTK_COMBO_BOX_TEXT(app->details.details_scope_combo), "My processes");
    app->details.details_view_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(
        GTK_COMBO_BOX_TEXT(app->details.details_view_combo), "Process tree");
    gtk_combo_box_text_append_text(
        GTK_COMBO_BOX_TEXT(app->details.details_view_combo), "Flat list");
    app->details.details_count_label = gtk_label_new("Processes: 0");
    app->details.details_inspect_button = gtk_button_new_with_label("Details");
    gtk_widget_set_sensitive(app->details.details_inspect_button, FALSE);
    app->details.details_end_button = gtk_button_new_with_label("End process");
    gtk_widget_set_tooltip_text(app->details.details_search,
        "Filter the technical process table (Ctrl+F)");
    gtk_widget_set_tooltip_text(app->details.details_scope_combo,
        "Show all processes, only active work, or processes owned by your account");
    gtk_widget_set_tooltip_text(app->details.details_view_combo,
        "Choose a parent/child tree or a sortable flat process list");
    gtk_widget_set_tooltip_text(app->details.details_inspect_button,
        "Inspect identity, resources, files and graphics for this process");
    gtk_widget_set_tooltip_text(app->details.details_end_button,
        "Ask the selected process to exit (Delete)");
    gtk_widget_set_sensitive(app->details.details_end_button, FALSE);

    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_search,
                       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_scope_combo,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_view_combo,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_inspect_button,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_end_button,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), toolbar, FALSE, FALSE, 0);

    app->details.details_store = gtk_tree_store_new(PROC_N_COLUMNS,
        G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_UINT64, G_TYPE_DOUBLE,
        G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT64,
        G_TYPE_UINT, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE,
        G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT64,
        G_TYPE_UINT, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_INT,
        G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_INT64, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_STRING);
    app->details.details_sort_model = gtk_tree_model_sort_new_with_model(
        GTK_TREE_MODEL(app->details.details_store));
    app->details.details_tree = gtk_tree_view_new_with_model(
        app->details.details_sort_model);
    gtk_widget_set_tooltip_text(app->details.details_tree,
        "Click headings to sort; right-click for actions; Ctrl+C copies the selected row");
    gtk_tree_view_set_headers_clickable(
        GTK_TREE_VIEW(app->details.details_tree), TRUE);
    gtk_tree_view_set_enable_search(
        GTK_TREE_VIEW(app->details.details_tree), FALSE);
    gtk_tree_view_set_enable_tree_lines(
        GTK_TREE_VIEW(app->details.details_tree), TRUE);
    gtk_tree_view_set_show_expanders(
        GTK_TREE_VIEW(app->details.details_tree), TRUE);

    for (int i = 0; i < PROC_N_COLUMNS; i++)
        app->details.details_columns[i] =
            lsm_process_table_append_process_column(
                app->details.details_tree, &lsm_details_column_specs[i]);
    lsm_details_layout_load(app);
    sync_process_table_presentation(app);
    gtk_combo_box_set_active(
        GTK_COMBO_BOX(app->details.details_scope_combo),
        app->details.details_scope);
    gtk_combo_box_set_active(
        GTK_COMBO_BOX(app->details.details_view_combo),
        app->details.details_tree_mode ? 0 : 1);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(app->details.details_tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(selected_process_changed), app);
    g_signal_connect(app->details.details_search, "changed",
                     G_CALLBACK(refilter_processes), app);
    g_signal_connect(app->details.details_scope_combo, "changed",
                     G_CALLBACK(process_scope_changed), app);
    g_signal_connect(app->details.details_view_combo, "changed",
                     G_CALLBACK(process_view_changed), app);
    g_signal_connect(app->details.details_inspect_button, "clicked",
                     G_CALLBACK(details_clicked), app);
    g_signal_connect(app->details.details_end_button, "clicked",
                     G_CALLBACK(kill_selected_process), app);
    g_signal_connect(app->details.details_tree, "button-press-event",
                     G_CALLBACK(process_button_press), app);
    g_signal_connect(app->details.details_tree, "row-activated",
                     G_CALLBACK(process_row_activated), app);

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_container_add(GTK_CONTAINER(scroller), app->details.details_tree);
    app->runtime.page_scrollers[LSM_TAB_DETAILS] = scroller;
    gtk_box_pack_start(GTK_BOX(outer), scroller, TRUE, TRUE, 0);
    gtk_widget_set_halign(app->details.details_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->details.details_count_label, 4);
    gtk_box_pack_start(GTK_BOX(outer), app->details.details_count_label,
                       FALSE, FALSE, 0);
    lsm_process_record_action_sync(app, FALSE, FALSE);
}

void lsm_details_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->details.details_sort_model)
        g_object_unref(app->details.details_sort_model);
    if (app->details.details_store)
        g_object_unref(app->details.details_store);
    app->details.details_sort_model = NULL;
    app->details.details_store = NULL;
    app->details.details_structure_valid = FALSE;
}
