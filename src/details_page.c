// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file details_page.c
 * @brief Technical Details table, filtering, control and CSV recording.
 *
 * The process page supports both a true parent/child tree and a sortable flat
 * list.  The GTK model is rebuilt from one retained process snapshot so search,
 * column selection and view changes do not trigger unnecessary procfs scans.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "details_page.h"
#include "atomic_file.h"
#include "duration_format.h"
#include "refresh_policy.h"
#include "monitor.h"
#include "app_internal.h"
#include "common.h"
#include "history.h"
#include "process_backend.h"
#include "process_inspector.h"
#include "process_scanner.h"
#include "processes_ui.h"
#include "ui_helpers.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    CELL_TEXT,
    CELL_INTEGER,
    CELL_PERCENT,
    CELL_BYTES,
    CELL_RATE,
    CELL_PRIORITY,
    CELL_TIME,
    CELL_DURATION
} ProcessCellFormat;

typedef struct {
    int column;
    const char *key;
    const char *title;
    ProcessCellFormat format;
    gboolean default_visible;
    gboolean expand;
    int minimum_width;
} ProcessColumnSpec;

static const ProcessColumnSpec column_specs[PROC_N_COLUMNS] = {
    {PROC_COL_NAME,             "name",       "Name",             CELL_TEXT,     TRUE,  TRUE,  170},
    {PROC_COL_PID,              "pid",        "PID",              CELL_INTEGER,  TRUE,  FALSE, 70},
    {PROC_COL_PPID,             "ppid",       "Parent PID",       CELL_INTEGER,  FALSE, FALSE, 85},
    {PROC_COL_USER,             "user",       "User",             CELL_TEXT,     TRUE,  FALSE, 95},
    {PROC_COL_STATE,            "status",     "Status",           CELL_TEXT,     TRUE,  FALSE, 90},
    {PROC_COL_CPU,              "cpu",        "CPU",              CELL_PERCENT,  TRUE,  FALSE, 70},
    {PROC_COL_CPU_TIME,         "cpu_time",   "CPU time",         CELL_DURATION, FALSE, FALSE, 95},
    {PROC_COL_MEMORY,           "memory",     "Memory %",         CELL_PERCENT,  TRUE,  FALSE, 80},
    {PROC_COL_RSS,              "rss",        "RAM",              CELL_BYTES,    TRUE,  FALSE, 90},
    {PROC_COL_THREADS,          "threads",    "Threads",          CELL_INTEGER,  FALSE, FALSE, 70},
    {PROC_COL_READ_RATE,        "read_rate",  "Read/s",           CELL_RATE,     TRUE,  FALSE, 90},
    {PROC_COL_WRITE_RATE,       "write_rate", "Write/s",          CELL_RATE,     TRUE,  FALSE, 90},
    {PROC_COL_GPU,              "gpu",        "GPU",              CELL_PERCENT,  TRUE,  FALSE, 70},
    {PROC_COL_GPU_ENGINE,       "gpu_engine", "GPU engine",       CELL_TEXT,     FALSE, FALSE, 105},
    {PROC_COL_GPU_MEMORY,       "gpu_memory", "GPU memory",       CELL_BYTES,    FALSE, FALSE, 105},
    {PROC_COL_READ_TOTAL,       "read_total", "Read total",       CELL_BYTES,    FALSE, FALSE, 95},
    {PROC_COL_WRITE_TOTAL,      "write_total","Written total",    CELL_BYTES,    FALSE, FALSE, 105},
    {PROC_COL_HANDLE_COUNT,         "handles",    "Handles",          CELL_INTEGER,  FALSE, FALSE, 105},
    {PROC_COL_CONTEXT_SWITCHES, "contexts",   "Context switches", CELL_INTEGER,  FALSE, FALSE, 110},
    {PROC_COL_PAGE_FAULTS,      "faults",     "Page faults",      CELL_INTEGER,  FALSE, FALSE, 90},
    {PROC_COL_PRIORITY,             "priority",   "Priority",         CELL_PRIORITY, FALSE, FALSE, 105},
    {PROC_COL_START_TIME,       "started",    "Start time",       CELL_TIME,     FALSE, FALSE, 145},
    {PROC_COL_ELAPSED,          "elapsed",    "Elapsed",          CELL_DURATION, FALSE, FALSE, 100},
    {PROC_COL_EXECUTABLE,       "executable", "Executable",       CELL_TEXT,     FALSE, TRUE,  260},
    {PROC_COL_COMMAND,          "command",    "Command",          CELL_TEXT,     FALSE, TRUE,  280}
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

/* Filtering and rendering operate only on the retained backend snapshot. */
static gboolean process_directly_visible(const LsmApp *app, const LsmProcessInfo *process)
{
    const char *search = gtk_entry_get_text(GTK_ENTRY(app->details.details_search));
    gboolean visible = !search || !*search ||
        lsm_ui_text_matches(process->name, search) ||
        lsm_ui_text_matches(process->user, search) ||
        lsm_ui_text_matches(process->command, search);

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

static void apply_resource_heatmap(LsmApp *app, GtkCellRenderer *renderer,
                                   int column, double value)
{
    if (!app->details.process_heatmap || value <= 0.0 ||
        (column != PROC_COL_CPU && column != PROC_COL_MEMORY &&
         column != PROC_COL_READ_RATE && column != PROC_COL_WRITE_RATE)) {
        g_object_set(renderer, "cell-background-set", FALSE, NULL);
        return;
    }

    double intensity;
    if (column == PROC_COL_CPU || column == PROC_COL_MEMORY)
        intensity = fmin(value / 100.0, 1.0);
    else
        intensity = fmin(log10(value + 1.0) / 9.0, 1.0);

    GdkRGBA colour = {0.96, 0.48, 0.10, 0.08 + 0.30 * intensity};
    g_object_set(renderer,
                 "cell-background-rgba", &colour,
                 "cell-background-set", TRUE,
                 NULL);
}

static void process_cell_data(GtkTreeViewColumn *view_column, GtkCellRenderer *renderer,
                              GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    (void)view_column;
    LsmApp *app = user_data;
    int column = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(renderer), "lsm-column"));
    char text[256] = "";
    double heat_value = 0.0;

    switch (column_specs[column].format) {
        case CELL_TEXT: {
            gchar *value = NULL;
            gtk_tree_model_get(model, iter, column, &value, -1);
            snprintf(text, sizeof(text), "%s", value ? value : "");
            g_free(value);
            break;
        }
        case CELL_INTEGER: {
            if (column == PROC_COL_PID || column == PROC_COL_PPID ||
                column == PROC_COL_CONTEXT_SWITCHES ||
                column == PROC_COL_PAGE_FAULTS) {
                guint64 value = 0;
                gtk_tree_model_get(model, iter, column, &value, -1);
                snprintf(text, sizeof(text), "%llu",
                         (unsigned long long)value);
            } else {
                guint value = 0;
                gtk_tree_model_get(model, iter, column, &value, -1);
                snprintf(text, sizeof(text), "%u", value);
            }
            break;
        }
        case CELL_PERCENT: {
            double value = 0.0;
            gtk_tree_model_get(model, iter, column, &value, -1);
            if (!isfinite(value)) {
                snprintf(text, sizeof(text), "N/A");
                break;
            }
            if (column == PROC_COL_CPU && app->runtime.process_cpu_per_core) {
                const unsigned cores = app->monitor.cpu.logical_cores ?
                    app->monitor.cpu.logical_cores : 1U;
                value *= (double)cores;
            }
            snprintf(text, sizeof(text), "%.1f%%", value);
            heat_value = value;
            break;
        }
        case CELL_BYTES: {
            guint64 value = 0;
            gtk_tree_model_get(model, iter, column, &value, -1);
            if (column == PROC_COL_GPU_MEMORY && value == UINT64_MAX)
                snprintf(text, sizeof(text), "N/A");
            else
                lsm_format_bytes(value, text, sizeof(text));
            break;
        }
        case CELL_RATE: {
            double value = 0.0;
            gtk_tree_model_get(model, iter, column, &value, -1);
            lsm_format_rate(value, text, sizeof(text));
            heat_value = value;
            break;
        }
        case CELL_PRIORITY: {
            gint value = 0;
            gtk_tree_model_get(model, iter, column, &value, -1);
            snprintf(text, sizeof(text), "%s",
                     lsm_process_priority_name((LsmProcessPriority)value));
            break;
        }
        case CELL_TIME: {
            gint64 epoch = 0;
            gtk_tree_model_get(model, iter, column, &epoch, -1);
            if (epoch > 0) {
                time_t timestamp = (time_t)epoch;
                struct tm local;
                localtime_r(&timestamp, &local);
                strftime(text, sizeof(text), "%d/%m/%Y %H:%M:%S", &local);
            } else snprintf(text, sizeof(text), "N/A");
            break;
        }
        case CELL_DURATION: {
            guint64 seconds = 0;
            gtk_tree_model_get(model, iter, column, &seconds, -1);
            lsm_duration_format_clock(seconds, text, sizeof(text));
            break;
        }
    }

    g_object_set(renderer, "text", text, NULL);
    apply_resource_heatmap(app, renderer, column, heat_value);
}

static GtkTreeViewColumn *add_process_column(LsmApp *app, const ProcessColumnSpec *spec)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set_data(G_OBJECT(renderer), "lsm-column", GINT_TO_POINTER(spec->column));
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    if (spec->format != CELL_TEXT) g_object_set(renderer, "xalign", 1.0, NULL);

    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, spec->title);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer, process_cell_data, app, NULL);
    gtk_tree_view_column_set_sort_column_id(column, spec->column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, spec->expand);
    gtk_tree_view_column_set_min_width(column, spec->minimum_width);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app->details.details_tree), column);
    return column;
}

void lsm_details_save_layout(const LsmApp *app)
{
    if (!app || !app->details.details_tree || !app->details.details_sort_model ||
        g_mkdir_with_parents(app->paths.config_dir, 0700) != 0)
        return;

    gint order[PROC_N_COLUMNS];
    for (int index = 0; index < PROC_N_COLUMNS; index++) order[index] = index;
    GList *columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(app->details.details_tree));
    gint position = 0;
    for (GList *item = columns; item; item = item->next, position++) {
        for (int index = 0; index < PROC_N_COLUMNS; index++) {
            if (item->data == app->details.details_columns[index]) {
                order[index] = position;
                break;
            }
        }
    }
    g_list_free(columns);

    GString *text = g_string_new(NULL);
    g_string_append_printf(text, "layout_version=4\ntree=%d\nheatmap=%d\n",
                           app->details.details_tree_mode ? 1 : 0,
                           app->details.process_heatmap ? 1 : 0);
    for (int i = 0; i < PROC_N_COLUMNS; i++) {
        g_string_append_printf(text, "%s=%d\n", column_specs[i].key,
            gtk_tree_view_column_get_visible(app->details.details_columns[i]) ? 1 : 0);
        g_string_append_printf(text, "width.%s=%d\n", column_specs[i].key,
            gtk_tree_view_column_get_width(app->details.details_columns[i]));
        g_string_append_printf(text, "order.%s=%d\n", column_specs[i].key,
                               order[i]);
    }

    gint sort_column = -1;
    GtkSortType sort_order = GTK_SORT_ASCENDING;
    if (gtk_tree_sortable_get_sort_column_id(
            GTK_TREE_SORTABLE(app->details.details_sort_model),
            &sort_column, &sort_order)) {
        g_string_append_printf(text, "sort_column=%d\nsort_order=%s\n",
            sort_column,
            sort_order == GTK_SORT_DESCENDING ? "descending" : "ascending");
    }
    (void)lsm_atomic_file_write_bytes(app->paths.column_path,
                                      LSM_ATOMIC_FILE_PRIVATE,
                                      text->str, text->len);
    g_string_free(text, TRUE);
}

static void process_columns_load(LsmApp *app)
{
    gboolean visible[PROC_N_COLUMNS];
    gint widths[PROC_N_COLUMNS];
    gint order[PROC_N_COLUMNS];
    gint sort_column = -1;
    GtkSortType sort_order = GTK_SORT_ASCENDING;
    int layout_version = 0;

    app->details.details_tree_mode = TRUE;
    app->details.process_heatmap = TRUE;
    for (int i = 0; i < PROC_N_COLUMNS; i++) {
        visible[i] = column_specs[i].default_visible;
        widths[i] = column_specs[i].minimum_width;
        order[i] = i;
    }

    gchar *content = NULL;
    gsize length = 0;
    if (g_file_get_contents(app->paths.column_path, &content, &length, NULL)) {
        gchar **lines = g_strsplit(content, "\n", -1);
        for (gchar **line = lines; *line; line++) {
            char *equals = strchr(*line, '=');
            if (!equals) continue;
            *equals++ = '\0';
            gboolean enabled = atoi(equals) != 0;
            if (strcmp(*line, "layout_version") == 0)
                layout_version = atoi(equals);
            else if (strcmp(*line, "tree") == 0)
                app->details.details_tree_mode = enabled;
            else if (strcmp(*line, "heatmap") == 0) app->details.process_heatmap = enabled;
            else if (strcmp(*line, "sort_column") == 0) {
                const int parsed = atoi(equals);
                if (parsed >= 0 && parsed < PROC_N_COLUMNS)
                    sort_column = parsed;
            } else if (strcmp(*line, "sort_order") == 0) {
                sort_order = strcmp(equals, "descending") == 0
                    ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
            }
            else {
                for (int i = 0; i < PROC_N_COLUMNS; i++) {
                    char width_key[64];
                    char order_key[64];
                    snprintf(width_key, sizeof(width_key), "width.%s",
                             column_specs[i].key);
                    snprintf(order_key, sizeof(order_key), "order.%s",
                             column_specs[i].key);
                    if (strcmp(*line, column_specs[i].key) == 0) {
                        visible[i] = enabled;
                        break;
                    }
                    if (strcmp(*line, width_key) == 0) {
                        const int parsed = atoi(equals);
                        if (parsed >= 40 && parsed <= 2000) widths[i] = parsed;
                        break;
                    }
                    if (strcmp(*line, order_key) == 0) {
                        const int parsed = atoi(equals);
                        if (parsed >= 0 && parsed < PROC_N_COLUMNS)
                            order[i] = parsed;
                        break;
                    }
                }
            }
        }
        g_strfreev(lines);
        g_free(content);
    }
    if (layout_version < 2) {
        for (int i = 0; i < PROC_N_COLUMNS; i++)
            visible[i] = column_specs[i].default_visible;
    }
    if (layout_version < 3) {
        const int previous_memory_column = 6;
        const int previous_read_total_column = 11;
        if (sort_column >= previous_read_total_column)
            sort_column += 3;
        else if (sort_column >= previous_memory_column)
            sort_column += 1;
        for (int i = 0; i < PROC_N_COLUMNS; i++) order[i] = i;
    }
    if (layout_version < 4) {
        if (sort_column >= PROC_COL_GPU_ENGINE) sort_column++;
        for (int i = 0; i < PROC_N_COLUMNS; i++) order[i] = i;
    }
    for (int i = 0; i < PROC_N_COLUMNS; i++) {
        gtk_tree_view_column_set_visible(app->details.details_columns[i], visible[i]);
        gtk_tree_view_column_set_sizing(app->details.details_columns[i],
                                        GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width(app->details.details_columns[i], widths[i]);
    }
    GtkTreeViewColumn *previous = NULL;
    for (int position = 0; position < PROC_N_COLUMNS; position++) {
        for (int index = 0; index < PROC_N_COLUMNS; index++) {
            if (order[index] != position) continue;
            gtk_tree_view_move_column_after(GTK_TREE_VIEW(app->details.details_tree),
                                            app->details.details_columns[index],
                                            previous);
            previous = app->details.details_columns[index];
            break;
        }
    }
    if (sort_column >= 0)
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(app->details.details_sort_model),
            sort_column, sort_order);

    /* The tree expander is rendered in the first visible column. Keeping Name
     * visible prevents a saved configuration from producing an unusable tree. */
    gtk_tree_view_column_set_visible(app->details.details_columns[PROC_COL_NAME], TRUE);
}

static unsigned process_scan_flags(const LsmApp *app)
{
    unsigned flags = LSM_PROCESS_SCAN_NONE;
    const gint current = gtk_notebook_get_current_page(
        GTK_NOTEBOOK(app->shell.notebook));

    if (current == LSM_TAB_DETAILS) {
        if (gtk_tree_view_column_get_visible(
                app->details.details_columns[PROC_COL_EXECUTABLE]))
            flags |= LSM_PROCESS_SCAN_EXECUTABLE;
        if (gtk_tree_view_column_get_visible(
                app->details.details_columns[PROC_COL_HANDLE_COUNT]))
            flags |= LSM_PROCESS_SCAN_HANDLE_COUNT;
    }

    if (current == LSM_TAB_PROCESSES ||
        (current == LSM_TAB_DETAILS &&
         (gtk_tree_view_column_get_visible(app->details.details_columns[PROC_COL_GPU]) ||
          gtk_tree_view_column_get_visible(
              app->details.details_columns[PROC_COL_GPU_ENGINE]) ||
          gtk_tree_view_column_get_visible(
              app->details.details_columns[PROC_COL_GPU_MEMORY]))))
        flags |= LSM_PROCESS_SCAN_GPU;
    return flags;
}

void lsm_details_show_columns(LsmApp *app)
{
    if (!app || !app->details.details_tree) return;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Select process columns",
        GTK_WINDOW(app->shell.window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 650, 430);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget *description = gtk_label_new(
        "Choose the fields displayed in the process table. Executable paths, "
        "handle counts and graphics counters are collected only while "
        "their columns are in use.");
    gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), description, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    GtkWidget *checks[PROC_N_COLUMNS];
    for (int i = 0; i < PROC_N_COLUMNS; i++) {
        checks[i] = gtk_check_button_new_with_label(column_specs[i].title);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checks[i]),
            gtk_tree_view_column_get_visible(app->details.details_columns[i]));
        if (i == PROC_COL_NAME) gtk_widget_set_sensitive(checks[i], FALSE);
        gtk_grid_attach(GTK_GRID(grid), checks[i], i % 3, i / 3, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 12);

    GtkWidget *heatmap = gtk_check_button_new_with_label(
        "Highlight CPU, memory and disk-I/O usage");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(heatmap), app->details.process_heatmap);
    gtk_box_pack_start(GTK_BOX(content), heatmap, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        for (int i = 0; i < PROC_N_COLUMNS; i++)
            gtk_tree_view_column_set_visible(app->details.details_columns[i],
                i == PROC_COL_NAME || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checks[i])));
        app->details.process_heatmap = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(heatmap));
        lsm_details_save_layout(app);
        gtk_widget_queue_draw(app->details.details_tree);
        (void)lsm_processes_update(app);
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
        PROC_COL_THREADS, process->threads,
        PROC_COL_READ_RATE, process->read_bytes_per_sec,
        PROC_COL_WRITE_RATE, process->write_bytes_per_sec,
        PROC_COL_GPU, process->gpu_available ? process->gpu_percent : NAN,
        PROC_COL_GPU_ENGINE, process->gpu_available && process->gpu_engine[0]
            ? process->gpu_engine : "N/A",
        PROC_COL_GPU_MEMORY, process->gpu_memory_available
            ? process->gpu_memory_bytes : UINT64_MAX,
        PROC_COL_READ_TOTAL, process->read_bytes,
        PROC_COL_WRITE_TOTAL, process->write_bytes,
        PROC_COL_HANDLE_COUNT, process->handle_count,
        PROC_COL_CONTEXT_SWITCHES, process->context_switches,
        PROC_COL_PAGE_FAULTS, process->page_faults,
        PROC_COL_PRIORITY, (gint)process->priority,
        PROC_COL_START_TIME, process->start_time_epoch,
        PROC_COL_ELAPSED, process->elapsed_seconds,
        PROC_COL_EXECUTABLE, process->executable,
        PROC_COL_COMMAND, process->command,
        -1);
}

static uint64_t process_signature_text(uint64_t hash, const char *text)
{
    const unsigned char *cursor =
        (const unsigned char *)(text ? text : "");
    while (*cursor) {
        hash ^= *cursor++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
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
        uint64_t row = UINT64_C(14695981039346656037);
        row ^= (uint64_t)process->pid;
        row *= UINT64_C(1099511628211);
        row ^= process->instance_id;
        row *= UINT64_C(1099511628211);
        row ^= (uint64_t)process->ppid;
        row *= UINT64_C(1099511628211);
        row = process_signature_text(row, process->name);
        row = process_signature_text(row, process->user);
        row = process_signature_text(row, process->command);
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
        const size_t index =
            (size_t)(GPOINTER_TO_UINT(value) - 1U);
        set_process_row(app->details.details_store, &iter,
                        &app->process.process_snapshot[index]);
        if (!update_details_branch(app, pid_index, &iter))
            return FALSE;
        valid = gtk_tree_model_iter_next(model, &iter);
    }
    return TRUE;
}

static gboolean update_details_model_values(LsmApp *app)
{
    if (!app || !app->details.details_store) return FALSE;
    GHashTable *pid_index =
        g_hash_table_new(g_direct_hash, g_direct_equal);
    if (!pid_index) return FALSE;
    for (size_t index = 0U;
         index < app->process.process_snapshot_count; index++) {
        const LsmProcessId pid =
            app->process.process_snapshot[index].pid;
        if (pid == 0U || pid > UINT_MAX || index >= UINT_MAX) continue;
        g_hash_table_insert(pid_index,
                            GUINT_TO_POINTER((guint)pid),
                            GUINT_TO_POINTER((guint)index + 1U));
    }
    const gboolean okay = update_details_branch(app, pid_index, NULL);
    g_hash_table_destroy(pid_index);
    return okay;
}

static ssize_t process_index_for_pid(ProcessBuildContext *context, LsmProcessId pid)
{
    gpointer value = g_hash_table_lookup(context->pid_to_index, GINT_TO_POINTER(pid));
    return value ? (ssize_t)(GPOINTER_TO_UINT(value) - 1u) : -1;
}

static void append_process_recursive(ProcessBuildContext *context, size_t index)
{
    if (context->inserted[index] || !context->visible[index]) return;
    if (context->state[index] == 1) {
        /* Corrupt or transient parent cycles are displayed as roots rather than
         * preventing the process list from being rendered. */
        context->state[index] = 0;
    }
    context->state[index] = 1;

    GtkTreeIter *parent = NULL;
    if (context->app->details.details_tree_mode) {
        ssize_t parent_index = process_index_for_pid(context, context->processes[index].ppid);
        if (parent_index >= 0 && (size_t)parent_index != index &&
            context->visible[parent_index] && context->state[parent_index] != 1) {
            append_process_recursive(context, (size_t)parent_index);
            if (context->inserted[parent_index]) parent = &context->iters[parent_index];
        }
    }

    gtk_tree_store_append(context->app->details.details_store, &context->iters[index], parent);
    set_process_row(context->app->details.details_store, &context->iters[index], &context->processes[index]);
    context->inserted[index] = TRUE;
    context->state[index] = 2;
}

static void collect_expanded_pid(GtkTreeView *tree, GtkTreePath *path, gpointer user_data)
{
    GHashTable *expanded = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tree);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gint pid = 0;
        gtk_tree_model_get(model, &iter, PROC_COL_PID, &pid, -1);
        if (pid > 0) g_hash_table_add(expanded, GINT_TO_POINTER(pid));
    }
}

static void select_sorted_path(LsmApp *app, GtkTreeIter *child_iter, gboolean expand)
{
    GtkTreePath *child_path = gtk_tree_model_get_path(GTK_TREE_MODEL(app->details.details_store), child_iter);
    GtkTreePath *sorted_path = gtk_tree_model_sort_convert_child_path_to_path(
        GTK_TREE_MODEL_SORT(app->details.details_sort_model), child_path);
    if (sorted_path) {
        if (expand) gtk_tree_view_expand_row(GTK_TREE_VIEW(app->details.details_tree), sorted_path, FALSE);
        else {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->details.details_tree));
            gtk_tree_selection_select_path(selection, sorted_path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(app->details.details_tree), sorted_path,
                                         NULL, FALSE, 0.0, 0.0);
        }
        gtk_tree_path_free(sorted_path);
    }
    gtk_tree_path_free(child_path);
}

/* Rebuild while preserving selection by process identity and expansion by PID. */
static void rebuild_details_model(LsmApp *app)
{
    size_t count = app->process.process_snapshot_count;
    LsmProcessId desired_pid = app->process.selected_pid;
    const LsmProcessInstanceId desired_instance_id =
        app->process.selected_instance_id;
    GHashTable *expanded = g_hash_table_new(g_direct_hash, g_direct_equal);
    if (app->details.details_tree_mode && app->details.details_tree_initialized)
        gtk_tree_view_map_expanded_rows(GTK_TREE_VIEW(app->details.details_tree),
                                        collect_expanded_pid, expanded);

    gboolean *direct = g_new0(gboolean, count);
    gboolean *visible = g_new0(gboolean, count);
    gboolean *inserted = g_new0(gboolean, count);
    unsigned char *state = g_new0(unsigned char, count);
    GtkTreeIter *iters = g_new0(GtkTreeIter, count);
    GHashTable *pid_to_index = g_hash_table_new(g_direct_hash, g_direct_equal);

    for (size_t i = 0; i < count; i++) {
        g_hash_table_insert(pid_to_index, GINT_TO_POINTER(app->process.process_snapshot[i].pid),
                            GUINT_TO_POINTER((guint)i + 1u));
        direct[i] = process_directly_visible(app, &app->process.process_snapshot[i]);
        visible[i] = direct[i];
    }

    /* A matching descendant keeps each ancestor visible so the result remains
     * reachable in tree mode.  Ancestors are context rows; excluded children
     * remain excluded unless another visible descendant needs them. */
    if (app->details.details_tree_mode) {
        for (size_t i = 0; i < count; i++) {
            if (!direct[i]) continue;
            LsmProcessId parent = app->process.process_snapshot[i].ppid;
            for (size_t guard = 0; parent > 0 && guard < count; guard++) {
                gpointer value = g_hash_table_lookup(pid_to_index, GINT_TO_POINTER(parent));
                if (!value) break;
                size_t parent_index = GPOINTER_TO_UINT(value) - 1u;
                visible[parent_index] = TRUE;
                LsmProcessId next_parent = app->process.process_snapshot[parent_index].ppid;
                if (next_parent == parent) break;
                parent = next_parent;
            }
        }
    }

    gtk_tree_store_clear(app->details.details_store);
    ProcessBuildContext context = {
        .app = app, .processes = app->process.process_snapshot, .count = count,
        .visible = visible, .inserted = inserted, .state = state,
        .iters = iters, .pid_to_index = pid_to_index
    };
    size_t shown = 0;
    for (size_t i = 0; i < count; i++) {
        if (visible[i]) {
            append_process_recursive(&context, i);
            shown++;
        }
    }

    const char *active_search = gtk_entry_get_text(GTK_ENTRY(app->details.details_search));
    gboolean searching = active_search && *active_search;
    for (size_t i = 0; i < count; i++) {
        if (!inserted[i]) continue;
        if (desired_pid > 0 &&
            app->process.process_snapshot[i].pid == desired_pid &&
            app->process.process_snapshot[i].instance_id ==
                desired_instance_id)
            select_sorted_path(app, &iters[i], FALSE);
        if (app->details.details_tree_mode &&
            (searching ||
             g_hash_table_contains(expanded, GINT_TO_POINTER(app->process.process_snapshot[i].pid)) ||
             (visible[i] && !direct[i])))
            select_sorted_path(app, &iters[i], TRUE);
    }

    if (app->details.details_tree_mode && !app->details.details_tree_initialized) {
        /* Expanding only root rows exposes the useful first level without
         * opening thousands of descendants or making startup expensive. */
        for (size_t i = 0; i < count; i++) {
            if (!inserted[i]) continue;
            ssize_t parent = process_index_for_pid(&context, app->process.process_snapshot[i].ppid);
            if (parent < 0 || !visible[parent]) select_sorted_path(app, &iters[i], TRUE);
        }
        app->details.details_tree_initialized = TRUE;
    }

    lsm_ui_set_label_text(app->details.details_count_label,
        shown == count ? "Processes: %zu" : "Processes: %zu  Showing: %zu", count, shown);

    g_hash_table_destroy(expanded);
    g_hash_table_destroy(pid_to_index);
    g_free(direct);
    g_free(visible);
    g_free(inserted);
    g_free(state);
    g_free(iters);
}

static gboolean details_page_visible(const LsmApp *app)
{
    if (!app || !app->shell.notebook) return TRUE;
    const gint current = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->shell.notebook));
    return current >= 0 && lsm_refresh_page_should_present(
        (unsigned)current, (unsigned)LSM_TAB_DETAILS, TRUE);
}

void lsm_details_present_snapshot(LsmApp *app)
{
    if (!app || !app->details.details_model_dirty ||
        !app->details.details_store)
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

static void selected_process_changed(GtkTreeSelection *selection, gpointer user_data)
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
        if (app->details.process_record_menu_item)
            gtk_widget_set_sensitive(app->details.process_record_menu_item,
                                     valid || app->process.recorder != NULL);
    } else {
        lsm_process_selection_set(app, 0U);
        gtk_widget_set_sensitive(app->details.details_end_button, FALSE);
        gtk_widget_set_sensitive(app->details.details_inspect_button, FALSE);
        if (!app->process.recorder && app->details.process_record_menu_item)
            gtk_widget_set_sensitive(app->details.process_record_menu_item, FALSE);
    }
}

static gboolean process_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    LsmApp *app = user_data;
    if (event->type != GDK_BUTTON_PRESS || event->button != 3) return FALSE;
    GtkTreePath *path = NULL;
    if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), (gint)event->x, (gint)event->y,
                                       &path, NULL, NULL, NULL)) return FALSE;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);
    GtkWidget *menu = lsm_process_actions_menu(app, TRUE);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    return TRUE;
}

static void process_row_activated(GtkTreeView *tree, GtkTreePath *path,
                                  GtkTreeViewColumn *column, gpointer user_data)
{
    (void)tree; (void)path; (void)column;
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

/* Public lifecycle. The update callback produces the shared process snapshot
 * consumed by Processes, Performance, Users and App History. */
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
    app->details.details_view_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->details.details_view_combo), "Process tree");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->details.details_view_combo), "Flat list");
    app->details.details_count_label = gtk_label_new("Processes: 0");
    app->details.details_inspect_button = gtk_button_new_with_label("Details");
    gtk_widget_set_sensitive(app->details.details_inspect_button, FALSE);
    app->details.details_end_button = gtk_button_new_with_label("End process");
    gtk_widget_set_tooltip_text(app->details.details_search,
        "Filter the technical process table (Ctrl+F)");
    gtk_widget_set_tooltip_text(app->details.details_view_combo,
        "Choose a parent/child tree or a sortable flat process list");
    gtk_widget_set_tooltip_text(app->details.details_inspect_button,
        "Inspect identity, resources, files and graphics for this process");
    gtk_widget_set_tooltip_text(app->details.details_end_button,
        "Ask the selected process to exit (Delete)");
    gtk_widget_set_sensitive(app->details.details_end_button, FALSE);

    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_search, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_view_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_inspect_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->details.details_end_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), toolbar, FALSE, FALSE, 0);

    app->details.details_store = gtk_tree_store_new(PROC_N_COLUMNS,
        G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_DOUBLE, G_TYPE_UINT64, G_TYPE_DOUBLE, G_TYPE_UINT64,
        G_TYPE_UINT, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE,
        G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT,
        G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_INT, G_TYPE_INT64,
        G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_STRING);
    app->details.details_sort_model = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(app->details.details_store));
    app->details.details_tree = gtk_tree_view_new_with_model(app->details.details_sort_model);
    gtk_widget_set_tooltip_text(app->details.details_tree,
        "Click headings to sort; right-click for actions; Ctrl+C copies the selected row");
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(app->details.details_tree), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(app->details.details_tree), FALSE);
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(app->details.details_tree), TRUE);
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(app->details.details_tree), TRUE);

    for (int i = 0; i < PROC_N_COLUMNS; i++)
        app->details.details_columns[i] = add_process_column(app, &column_specs[i]);
    process_columns_load(app);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->details.details_view_combo),
                             app->details.details_tree_mode ? 0 : 1);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->details.details_tree));
    g_signal_connect(selection, "changed", G_CALLBACK(selected_process_changed), app);
    g_signal_connect(app->details.details_search, "changed", G_CALLBACK(refilter_processes), app);
    g_signal_connect(app->details.details_view_combo, "changed", G_CALLBACK(process_view_changed), app);
    g_signal_connect(app->details.details_inspect_button, "clicked", G_CALLBACK(details_clicked), app);
    g_signal_connect(app->details.details_end_button, "clicked", G_CALLBACK(kill_selected_process), app);
    g_signal_connect(app->details.details_tree, "button-press-event", G_CALLBACK(process_button_press), app);
    g_signal_connect(app->details.details_tree, "row-activated", G_CALLBACK(process_row_activated), app);

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_container_add(GTK_CONTAINER(scroller), app->details.details_tree);
    app->runtime.page_scrollers[LSM_TAB_DETAILS] = scroller;
    gtk_box_pack_start(GTK_BOX(outer), scroller, TRUE, TRUE, 0);
    gtk_widget_set_halign(app->details.details_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->details.details_count_label, 4);
    gtk_box_pack_start(GTK_BOX(outer), app->details.details_count_label,
                       FALSE, FALSE, 0);
    if (app->details.process_record_menu_item)
        gtk_widget_set_sensitive(app->details.process_record_menu_item, FALSE);
}

static void append_record_if_needed(LsmApp *app,
                                    const LsmProcessInfo *processes,
                                    size_t count)
{
    if (!app->process.recorder || app->process.recording_pid <= 1 ||
        app->process.recording_instance_id == 0U)
        return;
    const LsmProcessInfo *found = NULL;
    for (size_t index = 0U; index < count; index++) {
        if (processes[index].pid == app->process.recording_pid &&
            processes[index].instance_id ==
                app->process.recording_instance_id) {
            found = &processes[index];
            break;
        }
    }
    if (!found) {
        lsm_process_record_stop(app);
        return;
    }
    (void)lsm_process_record_append(app, found);
}

gboolean lsm_processes_update(gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || app->runtime.paused || !app->process_scanner)
        return G_SOURCE_CONTINUE;

    LsmProcessInfo *processes = NULL;
    size_t count = 0U;
    const gboolean have_snapshot = lsm_process_scanner_take(
        app->process_scanner, &processes, &count);
    (void)lsm_process_scanner_request(
        app->process_scanner, process_scan_flags(app));
    if (!have_snapshot) return G_SOURCE_CONTINUE;

    lsm_monitor_set_process_totals(&app->monitor, processes, count);
    append_record_if_needed(app, processes, count);
    lsm_app_history_ingest(app, processes, count);

    lsm_process_list_free(app->process.process_snapshot);
    app->process.process_snapshot = processes;
    app->process.process_snapshot_count = count;
    app->processes.processes_model_dirty = TRUE;
    app->details.details_model_dirty = TRUE;
    if (lsm_processes_page_visible(app))
        lsm_processes_present_snapshot(app);
    if (details_page_visible(app)) lsm_details_present_snapshot(app);
    return G_SOURCE_CONTINUE;
}

void lsm_details_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->details.details_sort_model) g_object_unref(app->details.details_sort_model);
    if (app->details.details_store) g_object_unref(app->details.details_store);
    app->details.details_sort_model = NULL;
    app->details.details_store = NULL;
    app->details.details_structure_valid = FALSE;
}
