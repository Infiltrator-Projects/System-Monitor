// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file history.c
 * @brief Persistent cumulative application resource history.
 *
 * App History groups short-lived process instances by user and executable
 * identity.  Delta accounting is protected by PID start time so PID reuse
 * cannot transfer counters between unrelated programs.  The history is stored
 * as a compact tab-separated file in the user's configuration directory.
 * Retained identities are bounded by recency so long-running installations do
 * not grow the persistence file without limit.
 *
 * Linux does not expose portable per-process network byte counters.  This page
 * therefore records the reliable cross-kernel values: CPU time, active time,
 * disk reads/writes and peak resident memory.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "history.h"
#include "app_internal.h"
#include "atomic_file.h"
#include "common.h"
#include "duration_format.h"
#include "ui_helpers.h"

#include <infiltratr/core.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char *key;
    char *name;
    char *user;
    char *identity;
    double cpu_seconds;
    double active_seconds;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t peak_rss_bytes;
    int64_t first_seen;
    int64_t last_seen;
} LsmHistoryEntry;

#define LSM_HISTORY_MAX_ENTRIES 4096U

typedef struct {
    uint64_t cpu_time_nanoseconds;
    uint64_t read_bytes;
    uint64_t write_bytes;
    unsigned generation;
} LsmHistorySample;

enum {
    HIST_COL_APP,
    HIST_COL_USER,
    HIST_COL_CPU_SECONDS,
    HIST_COL_ACTIVE_SECONDS,
    HIST_COL_READ_BYTES,
    HIST_COL_WRITE_BYTES,
    HIST_COL_PEAK_RSS,
    HIST_COL_LAST_ACTIVE,
    HIST_COL_IDENTITY,
    HIST_N_COLUMNS
};

/* Persistent-record ownership and formatting helpers. */
static void history_entry_free(gpointer data)
{
    LsmHistoryEntry *entry = data;
    if (!entry) return;
    g_free(entry->key);
    g_free(entry->name);
    g_free(entry->user);
    g_free(entry->identity);
    g_free(entry);
}

static void history_sample_free(gpointer data)
{
    g_free(data);
}

static gboolean remove_history_key(gpointer key, gpointer value,
                                   gpointer user_data)
{
    (void)value;
    const char *candidate = key;
    const char *oldest = user_data;
    return candidate && oldest && strcmp(candidate, oldest) == 0;
}

static gboolean history_remove_oldest(LsmApp *app)
{
    if (!app || !app->history.app_history ||
        app->history.history_entry_count == 0U)
        return FALSE;

    GHashTableIter iterator;
    gpointer key = NULL;
    gpointer value = NULL;
    gpointer oldest_key = NULL;
    int64_t oldest_last_seen = 0;
    int64_t oldest_first_seen = 0;
    gboolean found = FALSE;

    g_hash_table_iter_init(&iterator, app->history.app_history);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        const LsmHistoryEntry *entry = value;
        if (!found || entry->last_seen < oldest_last_seen ||
            (entry->last_seen == oldest_last_seen &&
             entry->first_seen < oldest_first_seen)) {
            oldest_key = key;
            oldest_last_seen = entry->last_seen;
            oldest_first_seen = entry->first_seen;
            found = TRUE;
        }
    }
    if (!found) return FALSE;
    const guint removed = g_hash_table_foreach_remove(
        app->history.app_history, remove_history_key, oldest_key);
    if (removed == 0U) return FALSE;
    app->history.history_entry_count -=
        removed > app->history.history_entry_count
            ? app->history.history_entry_count : removed;
    return TRUE;
}

static gboolean history_trim_to_limit(LsmApp *app)
{
    gboolean changed = FALSE;
    while (app && app->history.app_history &&
           app->history.history_entry_count > LSM_HISTORY_MAX_ENTRIES) {
        if (!history_remove_oldest(app)) break;
        changed = TRUE;
    }
    return changed;
}


static void history_cell_data(GtkTreeViewColumn *column, GtkCellRenderer *renderer,
                              GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    (void)column;
    int field = GPOINTER_TO_INT(user_data);
    char text[128] = "";
    if (field == HIST_COL_CPU_SECONDS || field == HIST_COL_ACTIVE_SECONDS) {
        double seconds = 0.0;
        gtk_tree_model_get(model, iter, field, &seconds, -1);
        const uint64_t rounded = seconds > 0.0 ?
            (uint64_t)llround(seconds) : 0U;
        lsm_duration_format_clock(rounded, text, sizeof(text));
    } else if (field == HIST_COL_READ_BYTES || field == HIST_COL_WRITE_BYTES ||
               field == HIST_COL_PEAK_RSS) {
        guint64 bytes = 0;
        gtk_tree_model_get(model, iter, field, &bytes, -1);
        lsm_format_bytes(bytes, text, sizeof(text));
    } else if (field == HIST_COL_LAST_ACTIVE) {
        gint64 epoch = 0;
        gtk_tree_model_get(model, iter, field, &epoch, -1);
        if (epoch > 0) {
            time_t stamp = (time_t)epoch;
            struct tm local;
            localtime_r(&stamp, &local);
            strftime(text, sizeof(text), "%d/%m/%Y %H:%M:%S", &local);
        } else {
            snprintf(text, sizeof(text), "N/A");
        }
    }
    g_object_set(renderer, "text", text, NULL);
}

static GtkTreeViewColumn *history_column(GtkTreeView *tree, const char *title,
                                         int model_column, gboolean expand)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column;
    if (model_column == HIST_COL_APP || model_column == HIST_COL_USER ||
        model_column == HIST_COL_IDENTITY) {
        column = gtk_tree_view_column_new_with_attributes(
            title, renderer, "text", model_column, NULL);
        g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    } else {
        column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(column, title);
        gtk_tree_view_column_pack_start(column, renderer, TRUE);
        gtk_tree_view_column_set_cell_data_func(
            column, renderer, history_cell_data, GINT_TO_POINTER(model_column), NULL);
        g_object_set(renderer, "xalign", 1.0, NULL);
    }
    gtk_tree_view_column_set_sort_column_id(column, model_column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, expand);
    gtk_tree_view_append_column(tree, column);
    return column;
}

static char *sanitise_field(const char *text)
{
    char *copy = g_strdup(text ? text : "");
    for (char *p = copy; *p; p++)
        if (*p == '\t' || *p == '\r' || *p == '\n') *p = ' ';
    return copy;
}

/* Storage format is intentionally simple and atomically replaced on save. */
static void history_load(LsmApp *app)
{
    gchar *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(app->history.history_path, &contents, &length, NULL)) return;

    gchar **lines = g_strsplit(contents, "\n", -1);
    for (gchar **line = lines; *line; line++) {
        if (!**line || **line == '#') continue;
        gchar **fields = g_strsplit(*line, "\t", 11);
        if (g_strv_length(fields) < 11) {
            g_strfreev(fields);
            continue;
        }
        double cpu_seconds = 0.0;
        double active_seconds = 0.0;
        uint64_t read_bytes = 0U;
        uint64_t write_bytes = 0U;
        uint64_t peak_rss_bytes = 0U;
        int64_t first_seen = 0;
        int64_t last_seen = 0;
        if (!infiltratr_parse_double(fields[4], &cpu_seconds) ||
            !infiltratr_parse_double(fields[5], &active_seconds) ||
            !infiltratr_parse_u64(fields[6], 10U, &read_bytes) ||
            !infiltratr_parse_u64(fields[7], 10U, &write_bytes) ||
            !infiltratr_parse_u64(fields[8], 10U, &peak_rss_bytes) ||
            !infiltratr_parse_i64(fields[9], 10U, &first_seen) ||
            !infiltratr_parse_i64(fields[10], 10U, &last_seen)) {
            g_strfreev(fields);
            continue;
        }

        LsmHistoryEntry *entry = g_new0(LsmHistoryEntry, 1);
        entry->key = g_strdup(fields[0]);
        entry->name = g_strdup(fields[1]);
        entry->user = g_strdup(fields[2]);
        entry->identity = g_strdup(fields[3]);
        entry->cpu_seconds = cpu_seconds;
        entry->active_seconds = active_seconds;
        entry->read_bytes = read_bytes;
        entry->write_bytes = write_bytes;
        entry->peak_rss_bytes = peak_rss_bytes;
        entry->first_seen = first_seen;
        entry->last_seen = last_seen;
        const gboolean existed = g_hash_table_contains(
            app->history.app_history, entry->key);
        g_hash_table_replace(app->history.app_history, g_strdup(entry->key), entry);
        if (!existed) app->history.history_entry_count++;
        g_strfreev(fields);
    }
    g_strfreev(lines);
    g_free(contents);
    if (history_trim_to_limit(app))
        app->history.history_dirty = TRUE;
}

static int history_save_checked(LsmApp *app)
{
    if (!app || !app->history.app_history || !app->history.history_dirty)
        return 0;
    if (g_mkdir_with_parents(app->paths.config_dir, 0700) != 0)
        return errno ? errno : EIO;

    GString *output = g_string_new("# Linux-System-Monitor App History v1\n");
    GHashTableIter iterator;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iterator, app->history.app_history);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        (void)key;
        LsmHistoryEntry *entry = value;
        char *safe_key = sanitise_field(entry->key);
        char *safe_name = sanitise_field(entry->name);
        char *safe_user = sanitise_field(entry->user);
        char *safe_identity = sanitise_field(entry->identity);
        g_string_append_printf(output,
            "%s\t%s\t%s\t%s\t%.6f\t%.6f\t%llu\t%llu\t%llu\t%lld\t%lld\n",
            safe_key, safe_name, safe_user, safe_identity,
            entry->cpu_seconds, entry->active_seconds,
            (unsigned long long)entry->read_bytes,
            (unsigned long long)entry->write_bytes,
            (unsigned long long)entry->peak_rss_bytes,
            (long long)entry->first_seen, (long long)entry->last_seen);
        g_free(safe_key);
        g_free(safe_name);
        g_free(safe_user);
        g_free(safe_identity);
    }

    const int failure = lsm_atomic_file_write_bytes(
        app->history.history_path, LSM_ATOMIC_FILE_PRIVATE,
        output->str, output->len);
    g_string_free(output, TRUE);
    if (failure == 0) app->history.history_dirty = FALSE;
    return failure;
}

void lsm_history_save(LsmApp *app)
{
    if (!app) return;
    const int failure = history_save_checked(app);
    if (failure == 0) {
        app->history.history_save_error_reported = FALSE;
        return;
    }

    if (app->history.history_save_error_reported) return;
    app->history.history_save_error_reported = TRUE;
    if (app->shell.window && !app->runtime.shutting_down) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Unable to save application history", "%s",
                          g_strerror(failure));
    } else {
        fprintf(stderr, "Linux System Monitor: unable to save application history: %s\n",
                g_strerror(failure));
    }
}

static gboolean history_save_timer(gpointer user_data)
{
    lsm_history_save(user_data);
    return G_SOURCE_CONTINUE;
}

/* Executable identity plus user groups process instances into applications. */
static void history_identity(const LsmProcessInfo *process,
                             char *key, size_t key_size,
                             char *identity, size_t identity_size)
{
    char executable[LSM_PATH_LEN] = "";
    if (process->command[0] && process->command[0] != '[') {
        size_t length = strcspn(process->command, " \t");
        if (length >= sizeof(executable)) length = sizeof(executable) - 1;
        memcpy(executable, process->command, length);
        executable[length] = '\0';
    }
    if (!executable[0]) snprintf(executable, sizeof(executable), "%s", process->name);
    snprintf(identity, identity_size, "%s", executable);
    snprintf(key, key_size, "%s|%s",
             process->account_identity, executable);
}

static LsmHistoryEntry *history_entry_get(LsmApp *app, const LsmProcessInfo *process,
                                           const char *key, const char *identity)
{
    LsmHistoryEntry *entry = g_hash_table_lookup(app->history.app_history, key);
    if (entry) return entry;

    if (app->history.history_entry_count >= LSM_HISTORY_MAX_ENTRIES) {
        if (history_remove_oldest(app))
            app->history.history_dirty = TRUE;
    }

    entry = g_new0(LsmHistoryEntry, 1);
    entry->key = g_strdup(key);
    entry->name = g_strdup(process->name);
    entry->user = g_strdup(process->user);
    entry->identity = g_strdup(identity);
    entry->first_seen = entry->last_seen = (int64_t)time(NULL);
    g_hash_table_insert(app->history.app_history, g_strdup(key), entry);
    app->history.history_entry_count++;
    return entry;
}

static gboolean remove_stale_sample(gpointer key, gpointer value, gpointer user_data)
{
    (void)key;
    const LsmHistorySample *sample = value;
    unsigned current = GPOINTER_TO_UINT(user_data);
    return sample->generation + 2 < current;
}

/* Delta accounting validates PID start time before accepting cumulative data. */
void lsm_app_history_ingest(LsmApp *app, const LsmProcessInfo *processes, size_t count)
{
    if (!app || !app->history.app_history || !app->history.app_history_samples || !processes) return;

    app->history.history_generation++;
    const double now_mono = lsm_monotonic_seconds();
    double elapsed = app->history.history_last_sample > 0.0 ? now_mono - app->history.history_last_sample : 0.0;
    if (elapsed < 0.0 || elapsed > 60.0) elapsed = 0.0;
    app->history.history_last_sample = now_mono;
    const int64_t now_epoch = (int64_t)time(NULL);

    GHashTable *seen_apps = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GHashTable *rss_totals = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    for (size_t i = 0; i < count; i++) {
        const LsmProcessInfo *process = &processes[i];
        char app_key[LSM_PATH_LEN + 64], identity[LSM_PATH_LEN];
        history_identity(process, app_key, sizeof(app_key), identity, sizeof(identity));
        LsmHistoryEntry *entry = history_entry_get(app, process, app_key, identity);

        char sample_key[96];
        snprintf(sample_key, sizeof(sample_key), "%llu:%llu",
                 (unsigned long long)process->pid,
                 (unsigned long long)process->instance_id);
        LsmHistorySample *sample = g_hash_table_lookup(app->history.app_history_samples, sample_key);
        uint64_t cpu_delta = 0, read_delta = 0, write_delta = 0;
        if (sample) {
            if (process->cpu_time_nanoseconds >= sample->cpu_time_nanoseconds)
                cpu_delta = process->cpu_time_nanoseconds -
                            sample->cpu_time_nanoseconds;
            if (process->read_bytes >= sample->read_bytes)
                read_delta = process->read_bytes - sample->read_bytes;
            if (process->write_bytes >= sample->write_bytes)
                write_delta = process->write_bytes - sample->write_bytes;
        } else {
            sample = g_new0(LsmHistorySample, 1);
            g_hash_table_insert(app->history.app_history_samples, g_strdup(sample_key), sample);
        }
        sample->cpu_time_nanoseconds = process->cpu_time_nanoseconds;
        sample->read_bytes = process->read_bytes;
        sample->write_bytes = process->write_bytes;
        sample->generation = app->history.history_generation;

        entry->cpu_seconds += (double)cpu_delta / 1000000000.0;
        entry->read_bytes = lsm_u64_add_saturating(
            entry->read_bytes, read_delta);
        entry->write_bytes = lsm_u64_add_saturating(
            entry->write_bytes, write_delta);
        if (cpu_delta || read_delta || write_delta || process->cpu_percent > 0.05)
            entry->last_seen = now_epoch;

        if (!g_hash_table_contains(seen_apps, app_key)) {
            entry->active_seconds += elapsed;
            g_hash_table_add(seen_apps, g_strdup(app_key));
        }

        uint64_t *rss = g_hash_table_lookup(rss_totals, app_key);
        if (!rss) {
            rss = g_new0(uint64_t, 1);
            g_hash_table_insert(rss_totals, g_strdup(app_key), rss);
        }
        *rss = lsm_u64_add_saturating(*rss, process->rss_bytes);
    }

    GHashTableIter iterator;
    gpointer key, value;
    g_hash_table_iter_init(&iterator, rss_totals);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        LsmHistoryEntry *entry = g_hash_table_lookup(app->history.app_history, key);
        uint64_t rss = *(uint64_t *)value;
        if (entry && rss > entry->peak_rss_bytes) entry->peak_rss_bytes = rss;
    }

    g_hash_table_foreach_remove(app->history.app_history_samples, remove_stale_sample,
                                GUINT_TO_POINTER(app->history.history_generation));
    g_hash_table_destroy(seen_apps);
    g_hash_table_destroy(rss_totals);
    if (count > 0U) app->history.history_dirty = TRUE;

    if (app->history.history_tree && gtk_notebook_get_current_page(GTK_NOTEBOOK(app->shell.notebook)) == 2)
        lsm_history_refresh(app);
}

void lsm_history_refresh(LsmApp *app)
{
    if (!app || !app->history.history_store || !app->history.app_history) return;
    gtk_list_store_clear(app->history.history_store);
    const char *search = app->history.history_search
        ? gtk_entry_get_text(GTK_ENTRY(app->history.history_search)) : "";
    guint shown = 0;

    GHashTableIter iterator;
    gpointer key, value;
    g_hash_table_iter_init(&iterator, app->history.app_history);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        (void)key;
        LsmHistoryEntry *entry = value;
        if (*search && !lsm_ui_text_matches(entry->name, search) &&
            !lsm_ui_text_matches(entry->user, search) &&
            !lsm_ui_text_matches(entry->identity, search)) continue;
        GtkTreeIter row;
        gtk_list_store_append(app->history.history_store, &row);
        gtk_list_store_set(app->history.history_store, &row,
            HIST_COL_APP, entry->name,
            HIST_COL_USER, entry->user,
            HIST_COL_CPU_SECONDS, entry->cpu_seconds,
            HIST_COL_ACTIVE_SECONDS, entry->active_seconds,
            HIST_COL_READ_BYTES, entry->read_bytes,
            HIST_COL_WRITE_BYTES, entry->write_bytes,
            HIST_COL_PEAK_RSS, entry->peak_rss_bytes,
            HIST_COL_LAST_ACTIVE, entry->last_seen,
            HIST_COL_IDENTITY, entry->identity,
            -1);
        shown++;
    }
    lsm_ui_set_label_text(app->history.history_count_label, "%u applications", shown);
}

/* GTK construction and user actions. */
static void history_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    lsm_history_refresh(user_data);
}

static void history_reset(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->shell.window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "Reset all application history?");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
        "CPU time, active time, disk activity and peak-memory totals will be cleared.");
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL,
                           "Reset", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        g_hash_table_remove_all(app->history.app_history);
        g_hash_table_remove_all(app->history.app_history_samples);
        app->history.history_entry_count = 0U;
        app->history.history_dirty = TRUE;
        lsm_history_save(app);
        lsm_history_refresh(app);
    }
    gtk_widget_destroy(dialog);
}

void lsm_history_build(LsmApp *app, GtkWidget *container)
{
    app->history.app_history = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, history_entry_free);
    app->history.app_history_samples = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, history_sample_free);
    char *path = g_build_filename(app->paths.config_dir, "app-history.tsv", NULL);
    g_strlcpy(app->history.history_path, path, sizeof(app->history.history_path));
    g_free(path);
    history_load(app);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 8);
    app->history.history_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->history.history_search), "Search application history");
    gtk_widget_set_hexpand(app->history.history_search, TRUE);
    app->history.history_count_label = gtk_label_new("0 applications");
    app->history.history_reset_button = gtk_button_new_with_label("Reset history");
    gtk_box_pack_start(GTK_BOX(toolbar), app->history.history_search, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->history.history_reset_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), toolbar, FALSE, FALSE, 0);

    app->history.history_store = gtk_list_store_new(HIST_N_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE,
        G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_INT64, G_TYPE_STRING);
    app->history.history_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->history.history_store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(app->history.history_tree), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(app->history.history_tree), FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "Application", HIST_COL_APP, FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "User", HIST_COL_USER, FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "CPU time", HIST_COL_CPU_SECONDS, FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "Active time", HIST_COL_ACTIVE_SECONDS, FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "Disk read", HIST_COL_READ_BYTES, FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "Disk written", HIST_COL_WRITE_BYTES, FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "Peak memory", HIST_COL_PEAK_RSS, FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "Last active", HIST_COL_LAST_ACTIVE, FALSE);
    history_column(GTK_TREE_VIEW(app->history.history_tree), "Identity", HIST_COL_IDENTITY, TRUE);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(app->history.history_store),
                                         HIST_COL_CPU_SECONDS, GTK_SORT_DESCENDING);

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    app->runtime.page_scrollers[LSM_TAB_APP_HISTORY] = scroller;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroller), app->history.history_tree);
    gtk_box_pack_start(GTK_BOX(container), scroller, TRUE, TRUE, 0);
    gtk_widget_set_halign(app->history.history_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->history.history_count_label, 8);
    gtk_widget_set_margin_bottom(app->history.history_count_label, 6);
    gtk_box_pack_start(GTK_BOX(container), app->history.history_count_label,
                       FALSE, FALSE, 0);

    g_signal_connect(app->history.history_search, "changed", G_CALLBACK(history_search_changed), app);
    g_signal_connect(app->history.history_reset_button, "clicked", G_CALLBACK(history_reset), app);
    app->history.history_save_timer = g_timeout_add_seconds(30, history_save_timer, app);
    lsm_history_refresh(app);
}

void lsm_history_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->history.history_save_timer) {
        g_source_remove(app->history.history_save_timer);
        app->history.history_save_timer = 0;
    }
    lsm_history_save(app);
    if (app->history.app_history) g_hash_table_destroy(app->history.app_history);
    if (app->history.app_history_samples) g_hash_table_destroy(app->history.app_history_samples);
    if (app->history.history_store) g_object_unref(app->history.history_store);
    app->history.app_history = NULL;
    app->history.app_history_samples = NULL;
    app->history.history_store = NULL;
    app->history.history_entry_count = 0U;
}
