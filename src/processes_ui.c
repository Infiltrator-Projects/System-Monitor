// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file processes_ui.c
 * @brief Friendly application-grouped Processes page.
 *
 * One backend snapshot feeds both this approachable view and the technical
 * Details page. Related processes are grouped beneath XDG application names;
 * unmatched rows remain visible as background or system processes.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "processes_ui.h"

#include "app.h"
#include "application_catalog.h"
#include "common.h"
#include "details_page.h"
#include "process_grouping.h"
#include "refresh_policy.h"
#include "ui_helpers.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef enum {
    PROCESS_CATEGORY_APPLICATION,
    PROCESS_CATEGORY_BACKGROUND,
    PROCESS_CATEGORY_SYSTEM,
    PROCESS_CATEGORY_COUNT
} ProcessCategory;

typedef enum {
    PROCESS_ROW_CATEGORY,
    PROCESS_ROW_GROUP,
    PROCESS_ROW_PROCESS
} ProcessRowKind;

enum {
    GROUPED_COL_ICON,
    GROUPED_COL_NAME,
    GROUPED_COL_STATUS,
    GROUPED_COL_CPU,
    GROUPED_COL_MEMORY,
    GROUPED_COL_DISK,
    GROUPED_COL_GPU,
    GROUPED_COL_GPU_ENGINE,
    GROUPED_COL_GPU_AVAILABLE,
    GROUPED_COL_PID,
    GROUPED_COL_KIND,
    GROUPED_COL_KEY,
    GROUPED_N_COLUMNS
};

typedef struct {
    ProcessCategory category;
    char key[LSM_NAME_LEN * 2U];
    char name[LSM_NAME_LEN];
    char icon[LSM_NAME_LEN];
    size_t *indices;
    size_t count;
    size_t capacity;
    LsmProcessGroupMetrics metrics;
} ProcessGroup;

typedef struct {
    LsmProcessId pid;
    unsigned depth;
} PidDepth;

/* Group construction is independent of GTK row lifetime: build semantic
 * groups from the retained process snapshot first, then render them. */
static const char *category_name(ProcessCategory category)
{
    switch (category) {
        case PROCESS_CATEGORY_APPLICATION: return "Applications";
        case PROCESS_CATEGORY_BACKGROUND: return "Background processes";
        case PROCESS_CATEGORY_SYSTEM: return "System processes";
        case PROCESS_CATEGORY_COUNT: return "Processes";
    }
    return "Processes";
}

static const char *category_icon(ProcessCategory category)
{
    switch (category) {
        case PROCESS_CATEGORY_APPLICATION: return "applications-other";
        case PROCESS_CATEGORY_BACKGROUND: return "system-run";
        case PROCESS_CATEGORY_SYSTEM: return "computer";
        case PROCESS_CATEGORY_COUNT: return "application-x-executable";
    }
    return "application-x-executable";
}

static ssize_t snapshot_index_for_pid(const LsmApp *app, LsmProcessId pid)
{
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++)
        if (app->process.process_snapshot[index].pid == pid) return (ssize_t)index;
    return -1;
}

static const LsmApplicationEntry *application_for_process(
    const LsmApp *app, size_t process_index)
{
    const LsmProcessInfo *process = &app->process.process_snapshot[process_index];
    const LsmApplicationEntry *entry = lsm_application_catalog_lookup(
        app->process.application_catalog, process->name, process->command);
    LsmProcessId parent = process->ppid;
    for (size_t guard = 0U;
         !entry && parent > 1 && guard < app->process.process_snapshot_count;
         guard++) {
        const ssize_t parent_index = snapshot_index_for_pid(app, parent);
        if (parent_index < 0) break;
        const LsmProcessInfo *ancestor =
            &app->process.process_snapshot[(size_t)parent_index];
        entry = lsm_application_catalog_lookup(
            app->process.application_catalog, ancestor->name, ancestor->command);
        if (ancestor->ppid == parent) break;
        parent = ancestor->ppid;
    }
    return entry;
}

static gboolean process_excluded(const LsmApp *app,
                                 const LsmProcessInfo *process)
{
    if (!app->process.filters) return FALSE;
    for (guint index = 0U; index < app->process.filters->len; index++) {
        const char *filter = g_ptr_array_index(app->process.filters, index);
        if (lsm_ui_text_matches(process->name, filter) ||
            lsm_ui_text_matches(process->user, filter) ||
            lsm_ui_text_matches(process->command, filter))
            return TRUE;
    }
    return FALSE;
}

static gboolean process_matches_search(const LsmProcessInfo *process,
                                       const char *group_name,
                                       const char *search)
{
    if (!search || !*search) return TRUE;
    char pid[32];
    snprintf(pid, sizeof(pid), "%llu",
             (unsigned long long)process->pid);
    return lsm_ui_text_matches(group_name, search) ||
           lsm_ui_text_matches(process->name, search) ||
           lsm_ui_text_matches(process->user, search) ||
           lsm_ui_text_matches(process->command, search) ||
           lsm_ui_text_matches(pid, search);
}

static void process_identity(const LsmApp *app, size_t process_index,
                             ProcessCategory *category, char *key,
                             size_t key_size, char *name, size_t name_size,
                             char *icon, size_t icon_size)
{
    const LsmProcessInfo *process = &app->process.process_snapshot[process_index];
    const LsmApplicationEntry *entry =
        application_for_process(app, process_index);
    if (entry) {
        *category = PROCESS_CATEGORY_APPLICATION;
        snprintf(key, key_size, "app:%s", entry->id);
        g_strlcpy(name, entry->name, name_size);
        g_strlcpy(icon, entry->icon, icon_size);
        return;
    }

    *category = process->owned_by_current_user
        ? PROCESS_CATEGORY_BACKGROUND : PROCESS_CATEGORY_SYSTEM;
    snprintf(key, key_size, "%s:%s",
             *category == PROCESS_CATEGORY_BACKGROUND ? "background" : "system",
             process->name);
    g_strlcpy(name, process->name, name_size);
    g_strlcpy(icon, category_icon(*category), icon_size);
}

static void group_destroy(gpointer data)
{
    ProcessGroup *group = data;
    if (!group) return;
    free(group->indices);
    free(group);
}

static ProcessGroup *find_group(GPtrArray *groups, const char *key)
{
    for (guint index = 0U; index < groups->len; index++) {
        ProcessGroup *group = g_ptr_array_index(groups, index);
        if (strcmp(group->key, key) == 0) return group;
    }
    return NULL;
}

static gboolean group_append(ProcessGroup *group, size_t process_index,
                             const LsmProcessInfo *process)
{
    if (group->count == group->capacity) {
        const size_t next = group->capacity ? group->capacity * 2U : 4U;
        size_t *grown = realloc(group->indices, next * sizeof(*grown));
        if (!grown) return FALSE;
        group->indices = grown;
        group->capacity = next;
    }
    group->indices[group->count++] = process_index;
    lsm_process_group_metrics_add(&group->metrics, process);
    return TRUE;
}

static gint compare_groups(gconstpointer left, gconstpointer right)
{
    const ProcessGroup *a = *(ProcessGroup *const *)left;
    const ProcessGroup *b = *(ProcessGroup *const *)right;
    if (a->category != b->category)
        return a->category < b->category ? -1 : 1;
    return strcasecmp(a->name, b->name);
}

static GPtrArray *collect_groups(LsmApp *app)
{
    GPtrArray *groups =
        g_ptr_array_new_with_free_func(group_destroy);
    if (!groups) return NULL;
    const char *search = gtk_entry_get_text(GTK_ENTRY(app->processes.processes_search));

    for (size_t index = 0U; index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[index];
        if (process_excluded(app, process)) continue;
        ProcessCategory category = PROCESS_CATEGORY_BACKGROUND;
        char key[LSM_NAME_LEN * 2U];
        char name[LSM_NAME_LEN];
        char icon[LSM_NAME_LEN];
        process_identity(app, index, &category, key, sizeof(key),
                         name, sizeof(name), icon, sizeof(icon));
        if (!process_matches_search(process, name, search)) continue;

        ProcessGroup *group = find_group(groups, key);
        gboolean new_group = FALSE;
        if (!group) {
            group = calloc(1U, sizeof(*group));
            if (!group) continue;
            new_group = TRUE;
            group->category = category;
            g_strlcpy(group->key, key, sizeof(group->key));
            g_strlcpy(group->name, name, sizeof(group->name));
            g_strlcpy(group->icon, icon, sizeof(group->icon));
            g_ptr_array_add(groups, group);
        }
        if (!group_append(group, index, process) && new_group)
            g_ptr_array_set_size(groups, (gint)groups->len - 1);
    }
    g_ptr_array_sort(groups, compare_groups);
    return groups;
}

/* Cell-data callbacks format already-aggregated values. They do not rescan
 * processes, keeping scrolling/sorting detached from collection cost. */
static void grouped_cell_data(GtkTreeViewColumn *column,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model, GtkTreeIter *iter,
                              gpointer user_data)
{
    (void)column;
    LsmApp *app = user_data;
    const int model_column = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(renderer), "lsm-column"));
    gint kind = PROCESS_ROW_PROCESS;
    gtk_tree_model_get(model, iter, GROUPED_COL_KIND, &kind, -1);
    if (kind == PROCESS_ROW_CATEGORY) {
        g_object_set(renderer, "text", "", "cell-background-set", FALSE, NULL);
        return;
    }

    char text[128];
    double heat_value = 0.0;
    if (model_column == GROUPED_COL_STATUS) {
        char *value = NULL;
        gtk_tree_model_get(model, iter, model_column, &value, -1);
        snprintf(text, sizeof(text), "%s", value ? value : "");
        g_free(value);
    } else if (model_column == GROUPED_COL_CPU) {
        double value = 0.0;
        gtk_tree_model_get(model, iter, model_column, &value, -1);
        if (app->runtime.process_cpu_per_core) {
            const unsigned cores = app->monitor.cpu.logical_cores
                ? app->monitor.cpu.logical_cores : 1U;
            value *= (double)cores;
        } else value = fmin(value, 100.0);
        snprintf(text, sizeof(text), "%.1f%%", value);
        heat_value = value;
    } else if (model_column == GROUPED_COL_MEMORY) {
        guint64 value = 0U;
        gtk_tree_model_get(model, iter, model_column, &value, -1);
        lsm_format_bytes(value, text, sizeof(text));
        heat_value = app->monitor.memory.total_bytes > 0U
            ? 100.0 * (double)value /
              (double)app->monitor.memory.total_bytes : 0.0;
    } else if (model_column == GROUPED_COL_GPU_ENGINE) {
        char *value = NULL;
        gtk_tree_model_get(model, iter, model_column, &value, -1);
        snprintf(text, sizeof(text), "%s", value && *value ? value : "N/A");
        g_free(value);
    } else if (model_column == GROUPED_COL_GPU) {
        gboolean available = FALSE;
        double value = 0.0;
        gtk_tree_model_get(model, iter,
                           GROUPED_COL_GPU, &value,
                           GROUPED_COL_GPU_AVAILABLE, &available, -1);
        if (!available) {
            snprintf(text, sizeof(text), "N/A");
        } else {
            value = fmin(fmax(value, 0.0), 100.0);
            snprintf(text, sizeof(text), "%.1f%%", value);
            heat_value = value;
        }
    } else {
        double value = 0.0;
        gtk_tree_model_get(model, iter, model_column, &value, -1);
        lsm_format_rate(value, text, sizeof(text));
        heat_value = fmin(log10(value + 1.0) / 9.0 * 100.0, 100.0);
    }
    g_object_set(renderer, "text", text, NULL);
    if (!app->details.process_heatmap || heat_value <= 0.0) {
        g_object_set(renderer, "cell-background-set", FALSE, NULL);
    } else {
        const double intensity = fmin(heat_value / 100.0, 1.0);
        GdkRGBA colour = {0.96, 0.48, 0.10, 0.08 + 0.30 * intensity};
        g_object_set(renderer, "cell-background-rgba", &colour,
                     "cell-background-set", TRUE, NULL);
    }
}

static void grouped_name_data(GtkTreeViewColumn *column,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model, GtkTreeIter *iter,
                              gpointer user_data)
{
    (void)column;
    (void)user_data;
    char *name = NULL;
    gint kind = PROCESS_ROW_PROCESS;
    gtk_tree_model_get(model, iter,
                       GROUPED_COL_NAME, &name,
                       GROUPED_COL_KIND, &kind, -1);
    g_object_set(renderer, "text", name ? name : "",
                 "weight", kind == PROCESS_ROW_CATEGORY
                    ? PANGO_WEIGHT_BOLD : 400,
                 "weight-set", TRUE, NULL);
    g_free(name);
}

static void grouped_icon_data(GtkTreeViewColumn *column,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model, GtkTreeIter *iter,
                              gpointer user_data)
{
    (void)column;
    (void)user_data;
    char *icon = NULL;
    gtk_tree_model_get(model, iter, GROUPED_COL_ICON, &icon, -1);
    g_object_set(renderer, "icon-name", icon && *icon ? icon : NULL, NULL);
    g_free(icon);
}

static GtkTreeViewColumn *add_name_column(LsmApp *app)
{
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Name");
    GtkCellRenderer *icon = gtk_cell_renderer_pixbuf_new();
    GtkCellRenderer *text = gtk_cell_renderer_text_new();
    g_object_set(text, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_pack_start(column, icon, FALSE);
    gtk_tree_view_column_pack_start(column, text, TRUE);
    gtk_tree_view_column_set_cell_data_func(
        column, icon, grouped_icon_data, app, NULL);
    gtk_tree_view_column_set_cell_data_func(
        column, text, grouped_name_data, app, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 260);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app->processes.processes_tree), column);
    return column;
}

static GtkTreeViewColumn *add_resource_column(
    LsmApp *app, const char *title, int model_column, int width)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    if (model_column != GROUPED_COL_STATUS &&
        model_column != GROUPED_COL_GPU_ENGINE)
        g_object_set(renderer, "xalign", 1.0, NULL);
    g_object_set_data(G_OBJECT(renderer), "lsm-column",
                      GINT_TO_POINTER(model_column));
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, title);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(
        column, renderer, grouped_cell_data, app, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, width);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app->processes.processes_tree), column);
    return column;
}

static void select_iter(GtkTreeView *tree, GtkTreeModel *model,
                        GtkTreeIter *iter)
{
    GtkTreePath *path = gtk_tree_model_get_path(model, iter);
    if (!path) return;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree);
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_view_scroll_to_cell(tree, path, NULL, FALSE, 0.0, 0.0);
    gtk_tree_path_free(path);
}

static void expand_iter(GtkTreeView *tree, GtkTreeModel *model,
                        GtkTreeIter *iter)
{
    GtkTreePath *path = gtk_tree_model_get_path(model, iter);
    if (!path) return;
    gtk_tree_view_expand_row(tree, path, FALSE);
    gtk_tree_path_free(path);
}

static void collect_expanded_group(GtkTreeView *tree, GtkTreePath *path,
                                   gpointer user_data)
{
    GHashTable *expanded = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tree);
    if (!gtk_tree_model_get_iter(model, &iter, path)) return;
    gint kind = PROCESS_ROW_PROCESS;
    char *key = NULL;
    gtk_tree_model_get(model, &iter,
                       GROUPED_COL_KIND, &kind,
                       GROUPED_COL_KEY, &key, -1);
    if (kind == PROCESS_ROW_GROUP && key && *key)
        g_hash_table_add(expanded, key);
    else
        g_free(key);
}

static void append_process_child(LsmApp *app, const ProcessGroup *group,
                                 size_t group_index, GtkTreeIter *parent,
                                 LsmProcessId desired_pid)
{
    const LsmProcessInfo *process =
        &app->process.process_snapshot[group->indices[group_index]];
    GtkTreeIter child;
    const double disk = process->read_bytes_per_sec +
                        process->write_bytes_per_sec;
    gtk_tree_store_append(app->processes.processes_store, &child, parent);
    gtk_tree_store_set(app->processes.processes_store, &child,
        GROUPED_COL_ICON, group->icon,
        GROUPED_COL_NAME, process->name,
        GROUPED_COL_STATUS, process->state,
        GROUPED_COL_CPU, process->cpu_percent,
        GROUPED_COL_MEMORY, process->rss_bytes,
        GROUPED_COL_DISK, isfinite(disk) && disk > 0.0 ? disk : 0.0,
        GROUPED_COL_GPU, process->gpu_percent,
        GROUPED_COL_GPU_ENGINE,
            process->gpu_available && process->gpu_engine[0]
                ? process->gpu_engine : "N/A",
        GROUPED_COL_GPU_AVAILABLE, process->gpu_available,
        GROUPED_COL_PID, process->pid,
        GROUPED_COL_KIND, PROCESS_ROW_PROCESS,
        GROUPED_COL_KEY, group->key,
        -1);
    if (desired_pid == process->pid)
        select_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                    GTK_TREE_MODEL(app->processes.processes_store), &child);
}

static void append_group(LsmApp *app, const ProcessGroup *group,
                         GtkTreeIter *category, GHashTable *expanded,
                         LsmProcessId desired_pid, const char *desired_group)
{
    GtkTreeIter group_iter;
    char name[LSM_NAME_LEN + 32U];
    if (group->count > 1U)
        snprintf(name, sizeof(name), "%s (%zu)", group->name, group->count);
    else
        g_strlcpy(name, group->name, sizeof(name));
    const char *status = group->metrics.all_stopped ? "Suspended" :
                         group->metrics.all_efficient
                         ? "Efficiency mode" : "";
    const LsmProcessId representative =
        app->process.process_snapshot[group->indices[0]].pid;
    gtk_tree_store_append(app->processes.processes_store, &group_iter, category);
    gtk_tree_store_set(app->processes.processes_store, &group_iter,
        GROUPED_COL_ICON, group->icon,
        GROUPED_COL_NAME, name,
        GROUPED_COL_STATUS, status,
        GROUPED_COL_CPU, group->metrics.cpu_percent,
        GROUPED_COL_MEMORY, group->metrics.memory_bytes,
        GROUPED_COL_DISK, group->metrics.disk_bytes_per_sec,
        GROUPED_COL_GPU, group->metrics.gpu_percent,
        GROUPED_COL_GPU_ENGINE,
            group->metrics.gpu_available && group->metrics.gpu_engine[0]
                ? group->metrics.gpu_engine : "N/A",
        GROUPED_COL_GPU_AVAILABLE, group->metrics.gpu_available,
        GROUPED_COL_PID, representative,
        GROUPED_COL_KIND, PROCESS_ROW_GROUP,
        GROUPED_COL_KEY, group->key,
        -1);

    if (group->count > 1U) {
        for (size_t index = 0U; index < group->count; index++)
            append_process_child(app, group, index, &group_iter, desired_pid);
        if (g_hash_table_contains(expanded, group->key))
            expand_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                        GTK_TREE_MODEL(app->processes.processes_store), &group_iter);
    }
    if (desired_group && *desired_group &&
        strcmp(desired_group, name) == 0)
        select_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                    GTK_TREE_MODEL(app->processes.processes_store), &group_iter);
    else if (group->count == 1U && desired_pid == representative)
        select_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                    GTK_TREE_MODEL(app->processes.processes_store), &group_iter);
}

/* Model rebuilds preserve the user-visible selection and expanded groups by
 * stable process/group identity rather than by transient GtkTreePath values. */
static void rebuild_grouped_model(LsmApp *app)
{
    const LsmProcessId desired_pid = app->process.selected_pid;
    char desired_group[LSM_NAME_LEN];
    g_strlcpy(desired_group, app->process.selected_group_name,
              sizeof(desired_group));
    GHashTable *expanded = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
    gtk_tree_view_map_expanded_rows(
        GTK_TREE_VIEW(app->processes.processes_tree), collect_expanded_group, expanded);

    GPtrArray *groups = collect_groups(app);
    gtk_tree_store_clear(app->processes.processes_store);
    if (!groups) {
        g_hash_table_destroy(expanded);
        return;
    }

    size_t category_groups[PROCESS_CATEGORY_COUNT] = {0U};
    size_t category_processes[PROCESS_CATEGORY_COUNT] = {0U};
    for (guint index = 0U; index < groups->len; index++) {
        const ProcessGroup *group = g_ptr_array_index(groups, index);
        category_groups[group->category]++;
        category_processes[group->category] += group->count;
    }

    for (int category_value = PROCESS_CATEGORY_APPLICATION;
         category_value < PROCESS_CATEGORY_COUNT; category_value++) {
        const ProcessCategory category = (ProcessCategory)category_value;
        if (category_groups[category] == 0U) continue;
        GtkTreeIter category_iter;
        char title[LSM_NAME_LEN];
        snprintf(title, sizeof(title), "%s (%zu)",
                 category_name(category), category_groups[category]);
        gtk_tree_store_append(app->processes.processes_store, &category_iter, NULL);
        gtk_tree_store_set(app->processes.processes_store, &category_iter,
            GROUPED_COL_ICON, category_icon(category),
            GROUPED_COL_NAME, title,
            GROUPED_COL_STATUS, "",
            GROUPED_COL_CPU, 0.0,
            GROUPED_COL_MEMORY, (guint64)0U,
            GROUPED_COL_DISK, 0.0,
            GROUPED_COL_GPU_ENGINE, "",
            GROUPED_COL_PID, 0,
            GROUPED_COL_KIND, PROCESS_ROW_CATEGORY,
            GROUPED_COL_KEY, "",
            -1);
        for (guint index = 0U; index < groups->len; index++) {
            const ProcessGroup *group = g_ptr_array_index(groups, index);
            if (group->category == category)
                append_group(app, group, &category_iter, expanded,
                             desired_pid, desired_group);
        }
        expand_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                    GTK_TREE_MODEL(app->processes.processes_store), &category_iter);
    }

    size_t visible_processes = 0U;
    for (int category = PROCESS_CATEGORY_APPLICATION;
         category < PROCESS_CATEGORY_COUNT; category++)
        visible_processes += category_processes[category];
    lsm_ui_set_label_text(app->processes.processes_count_label,
        "Apps: %zu   Background: %zu   System: %zu   Processes: %zu",
        category_groups[PROCESS_CATEGORY_APPLICATION],
        category_groups[PROCESS_CATEGORY_BACKGROUND],
        category_groups[PROCESS_CATEGORY_SYSTEM], visible_processes);

    g_ptr_array_free(groups, TRUE);
    g_hash_table_destroy(expanded);
}

gboolean lsm_processes_page_visible(const LsmApp *app)
{
    if (!app || !app->shell.notebook) return TRUE;
    const gint current =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->shell.notebook));
    return current >= 0 && lsm_refresh_page_should_present(
        (unsigned)current, (unsigned)LSM_TAB_PROCESSES, TRUE);
}

void lsm_processes_present_snapshot(LsmApp *app)
{
    if (!app || !app->processes.processes_model_dirty || !app->processes.processes_store)
        return;
    rebuild_grouped_model(app);
    app->processes.processes_model_dirty = FALSE;
}

/* Process-tree actions are resolved against the retained snapshot before the
 * backend is asked to act, avoiding UI-order dependence for grouped rows. */
static unsigned process_depth(const LsmApp *app, LsmProcessId pid)
{
    unsigned depth = 0U;
    for (size_t guard = 0U;
         pid > 1 && guard < app->process.process_snapshot_count; guard++) {
        const ssize_t index = snapshot_index_for_pid(app, pid);
        if (index < 0) break;
        const LsmProcessId parent = app->process.process_snapshot[(size_t)index].ppid;
        if (parent <= 0 || parent == pid) break;
        depth++;
        pid = parent;
    }
    return depth;
}

static int compare_pid_depth(const void *left, const void *right)
{
    const PidDepth *a = left;
    const PidDepth *b = right;
    if (a->depth != b->depth) return a->depth > b->depth ? -1 : 1;
    return a->pid > b->pid ? -1 : a->pid < b->pid ? 1 : 0;
}

static void select_group_processes(LsmApp *app, GtkTreeModel *model,
                                   GtkTreeIter *group, const char *name)
{
    GtkTreeIter child;
    size_t count = 0U;
    if (gtk_tree_model_iter_children(model, &child, group)) {
        do {
            guint64 pid = 0U;
            gtk_tree_model_get(model, &child, GROUPED_COL_PID, &pid, -1);
            if (pid > 1) count++;
        } while (gtk_tree_model_iter_next(model, &child));
    }
    if (count <= 1U) return;

    PidDepth *ordered = calloc(count, sizeof(*ordered));
    if (!ordered) return;
    size_t index = 0U;
    if (gtk_tree_model_iter_children(model, &child, group)) {
        do {
            guint64 pid = 0U;
            gtk_tree_model_get(model, &child, GROUPED_COL_PID, &pid, -1);
            if (pid > 1 && index < count) {
                ordered[index].pid = (LsmProcessId)pid;
                ordered[index].depth = process_depth(app,
                                                     (LsmProcessId)pid);
                index++;
            }
        } while (gtk_tree_model_iter_next(model, &child));
    }
    qsort(ordered, index, sizeof(*ordered), compare_pid_depth);
    app->process.selected_group_pids = calloc(index, sizeof(*app->process.selected_group_pids));
    if (app->process.selected_group_pids) {
        for (size_t item = 0U; item < index; item++)
            app->process.selected_group_pids[item] = ordered[item].pid;
        app->process.selected_group_count = index;
        g_strlcpy(app->process.selected_group_name, name,
                  sizeof(app->process.selected_group_name));
    }
    free(ordered);
}

static void grouped_selection_changed(GtkTreeSelection *selection,
                                      gpointer user_data)
{
    LsmApp *app = user_data;
    lsm_process_group_selection_clear(app);
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    guint64 pid = 0U;
    gint kind = PROCESS_ROW_CATEGORY;
    char *name = NULL;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter,
                           GROUPED_COL_PID, &pid,
                           GROUPED_COL_KIND, &kind,
                           GROUPED_COL_NAME, &name, -1);
        if (kind == PROCESS_ROW_GROUP)
            select_group_processes(app, model, &iter, name ? name : "");
    }
    app->process.selected_pid = pid > 0U ? (LsmProcessId)pid : 0U;
    const gboolean valid = app->process.selected_pid > 1;
    gtk_widget_set_sensitive(app->processes.processes_end_button, valid);
    gtk_widget_set_sensitive(app->processes.processes_inspect_button,
                             app->process.selected_pid > 0);
    if (app->details.process_record_menu_item)
        gtk_widget_set_sensitive(app->details.process_record_menu_item,
            (valid && app->process.selected_group_count == 0U) ||
            app->process.record_file != NULL);
    g_free(name);
}

static void grouped_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    LsmApp *app = user_data;
    app->processes.processes_model_dirty = TRUE;
    lsm_processes_present_snapshot(app);
}

static void grouped_end_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_processes_end_selected(user_data);
}

void lsm_processes_go_to_details(LsmApp *app)
{
    if (!app || app->process.selected_pid <= 0) return;
    gtk_entry_set_text(GTK_ENTRY(app->details.details_search), "");
    app->details.details_model_dirty = TRUE;
    gtk_notebook_set_current_page(
        GTK_NOTEBOOK(app->shell.notebook), LSM_TAB_DETAILS);
    lsm_details_present_snapshot(app);
}

static void grouped_details_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_processes_go_to_details(user_data);
}

static gboolean grouped_button_press(GtkWidget *widget,
                                     GdkEventButton *event,
                                     gpointer user_data)
{
    LsmApp *app = user_data;
    if (event->type != GDK_BUTTON_PRESS || event->button != 3U)
        return FALSE;
    GtkTreePath *path = NULL;
    if (!gtk_tree_view_get_path_at_pos(
            GTK_TREE_VIEW(widget), (gint)event->x, (gint)event->y,
            &path, NULL, NULL, NULL))
        return FALSE;
    GtkTreeSelection *selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);
    if (app->process.selected_pid <= 0) return TRUE;
    GtkWidget *menu = lsm_process_actions_menu(app, FALSE);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    return TRUE;
}

static void grouped_row_activated(GtkTreeView *tree, GtkTreePath *path,
                                  GtkTreeViewColumn *column,
                                  gpointer user_data)
{
    (void)tree;
    (void)path;
    (void)column;
    LsmApp *app = user_data;
    if (app->process.selected_group_count <= 1U)
        lsm_processes_go_to_details(app);
}

/* Page construction owns GTK objects only; collection remains in the shared
 * process backend used by Processes, Details and inspection workflows. */
void lsm_processes_build(LsmApp *app, GtkWidget *container)
{
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 8);
    gtk_container_add(GTK_CONTAINER(container), outer);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app->processes.processes_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->processes.processes_search),
        "Search application, process, owner, command, or PID");
    gtk_widget_set_hexpand(app->processes.processes_search, TRUE);
    app->processes.processes_inspect_button =
        gtk_button_new_with_label("Go to details");
    app->processes.processes_end_button = gtk_button_new_with_label("End task");
    gtk_widget_set_tooltip_text(app->processes.processes_search,
        "Filter by application, process, owner, command, or PID (Ctrl+F)");
    gtk_widget_set_tooltip_text(app->processes.processes_inspect_button,
        "Open the selected process in the technical Details page (Enter)");
    gtk_widget_set_tooltip_text(app->processes.processes_end_button,
        "Ask the selected process or application group to exit (Delete)");
    gtk_widget_set_sensitive(app->processes.processes_inspect_button, FALSE);
    gtk_widget_set_sensitive(app->processes.processes_end_button, FALSE);
    gtk_box_pack_start(GTK_BOX(toolbar), app->processes.processes_search,
                       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->processes.processes_inspect_button,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->processes.processes_end_button,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), toolbar, FALSE, FALSE, 0);

    app->processes.processes_store = gtk_tree_store_new(GROUPED_N_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE,
        G_TYPE_UINT64, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_STRING,
        G_TYPE_BOOLEAN,
        G_TYPE_UINT64, G_TYPE_INT, G_TYPE_STRING);
    app->processes.processes_tree = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(app->processes.processes_store));
    gtk_widget_set_tooltip_text(app->processes.processes_tree,
        "Right-click for process actions; Ctrl+C copies the selected row");
    gtk_tree_view_set_headers_clickable(
        GTK_TREE_VIEW(app->processes.processes_tree), FALSE);
    gtk_tree_view_set_enable_search(
        GTK_TREE_VIEW(app->processes.processes_tree), FALSE);
    gtk_tree_view_set_enable_tree_lines(
        GTK_TREE_VIEW(app->processes.processes_tree), TRUE);
    gtk_tree_view_set_show_expanders(
        GTK_TREE_VIEW(app->processes.processes_tree), TRUE);
    (void)add_name_column(app);
    (void)add_resource_column(app, "Status", GROUPED_COL_STATUS, 120);
    (void)add_resource_column(app, "CPU", GROUPED_COL_CPU, 85);
    (void)add_resource_column(app, "Memory", GROUPED_COL_MEMORY, 100);
    (void)add_resource_column(app, "Disk", GROUPED_COL_DISK, 95);
    (void)add_resource_column(app, "GPU", GROUPED_COL_GPU, 80);
    (void)add_resource_column(app, "GPU engine", GROUPED_COL_GPU_ENGINE, 105);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(app->processes.processes_tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(grouped_selection_changed), app);
    g_signal_connect(app->processes.processes_search, "changed",
                     G_CALLBACK(grouped_search_changed), app);
    g_signal_connect(app->processes.processes_inspect_button, "clicked",
                     G_CALLBACK(grouped_details_clicked), app);
    g_signal_connect(app->processes.processes_end_button, "clicked",
                     G_CALLBACK(grouped_end_clicked), app);
    g_signal_connect(app->processes.processes_tree, "button-press-event",
                     G_CALLBACK(grouped_button_press), app);
    g_signal_connect(app->processes.processes_tree, "row-activated",
                     G_CALLBACK(grouped_row_activated), app);

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    app->runtime.page_scrollers[LSM_TAB_PROCESSES] = scroller;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_container_add(GTK_CONTAINER(scroller), app->processes.processes_tree);
    gtk_box_pack_start(GTK_BOX(outer), scroller, TRUE, TRUE, 0);

    app->processes.processes_count_label = gtk_label_new(
        "Apps: 0   Background: 0   System: 0   Processes: 0");
    gtk_widget_set_halign(app->processes.processes_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->processes.processes_count_label, 4);
    gtk_box_pack_start(GTK_BOX(outer), app->processes.processes_count_label,
                       FALSE, FALSE, 0);
}

void lsm_processes_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->processes.processes_store) g_object_unref(app->processes.processes_store);
    app->processes.processes_store = NULL;
}
