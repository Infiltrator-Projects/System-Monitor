// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file processes_ui.c
 * @brief Friendly application-grouped Processes page.
 *
 * One backend snapshot feeds both this approachable view and the technical
 * Details page. Related processes are grouped beneath XDG application names;
 * unmatched rows remain visible as background or system processes.
 *
 * Grouping is a projection of immutable snapshot rows, not an alternate source
 * of process truth. Rows retain PID plus backend instance identity so expanding
 * a group or invoking a later action cannot accidentally transfer selection to
 * a recycled PID from a newer snapshot.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "processes_ui.h"

#include "app_internal.h"
#include "application_catalog.h"
#include "common.h"
#include "process_grouping.h"
#include "refresh_policy.h"
#include "ui_helpers.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    GROUPED_COL_DISK_AVAILABLE,
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
    LsmProcessInstanceId instance_id;
    unsigned depth;
} PidDepth;

typedef struct {
    LsmProcessId pid;
    LsmProcessInstanceId instance_id;
} ProcessIdentityCacheKey;

typedef struct {
    ProcessCategory category;
    char key[LSM_NAME_LEN * 2U];
    char name[LSM_NAME_LEN];
    char icon[LSM_NAME_LEN];
    double refreshed_at;
    uint64_t seen_generation;
} ProcessIdentityCacheEntry;

#define LSM_PROCESS_IDENTITY_CACHE_SECONDS 5.0

static guint process_identity_cache_hash(gconstpointer data)
{
    const ProcessIdentityCacheKey *key = data;
    if (!key) return 0U;
    uint64_t hash = LSM_FNV1A64_OFFSET_BASIS;
    hash = lsm_fnv1a64_mix_u64_le(hash, (uint64_t)key->pid);
    hash = lsm_fnv1a64_mix_u64_le(hash, key->instance_id);
    return (guint)(hash ^ (hash >> 32U));
}

static gboolean process_identity_cache_equal(gconstpointer left,
                                             gconstpointer right)
{
    const ProcessIdentityCacheKey *a = left;
    const ProcessIdentityCacheKey *b = right;
    return a && b && a->pid == b->pid &&
           a->instance_id == b->instance_id;
}

static gboolean process_identity_cache_remove_stale(
    gpointer key, gpointer value, gpointer user_data)
{
    (void)key;
    const ProcessIdentityCacheEntry *entry = value;
    const uint64_t *generation = user_data;
    return !entry || !generation ||
           entry->seen_generation != *generation;
}

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
    if (!app) return -1;
    if (app->processes.process_pid_index &&
        pid > 0U && pid <= (LsmProcessId)UINT_MAX) {
        gpointer value = g_hash_table_lookup(
            app->processes.process_pid_index,
            GUINT_TO_POINTER((guint)pid));
        if (value)
            return (ssize_t)(GPOINTER_TO_UINT(value) - 1U);
    }
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++)
        if (app->process.process_snapshot[index].pid == pid) return (ssize_t)index;
    return -1;
}

static GHashTable *refresh_snapshot_pid_index(LsmApp *app)
{
    if (!app) return NULL;
    if (!app->processes.process_pid_index)
        app->processes.process_pid_index =
            g_hash_table_new(g_direct_hash, g_direct_equal);
    GHashTable *index = app->processes.process_pid_index;
    if (!index) return NULL;
    g_hash_table_remove_all(index);
    for (size_t item = 0U; item < app->process.process_snapshot_count; item++) {
        const LsmProcessId pid = app->process.process_snapshot[item].pid;
        if (pid == 0U || pid > (LsmProcessId)UINT_MAX || item >= UINT_MAX)
            continue;
        g_hash_table_insert(index, GUINT_TO_POINTER((guint)pid),
                            GUINT_TO_POINTER((guint)item + 1U));
    }
    return index;
}

static ssize_t indexed_snapshot_index_for_pid(GHashTable *index,
                                              LsmProcessId pid)
{
    if (!index || pid == 0U || pid > (LsmProcessId)UINT_MAX) return -1;
    gpointer value = g_hash_table_lookup(
        index, GUINT_TO_POINTER((guint)pid));
    return value ? (ssize_t)(GPOINTER_TO_UINT(value) - 1U) : -1;
}

static const LsmApplicationEntry *application_for_process(
    const LsmApp *app, size_t process_index, GHashTable *pid_index)
{
    const LsmProcessInfo *process = &app->process.process_snapshot[process_index];
    const LsmApplicationEntry *entry =
        process->cgroup_v2
            ? lsm_application_catalog_lookup_cgroup(
                  app->process.application_catalog, process->cgroup_path)
            : NULL;
    if (!entry)
        entry = lsm_application_catalog_lookup(
            app->process.application_catalog, process->name, process->command);
    LsmProcessId parent = process->ppid;
    for (size_t guard = 0U;
         !entry && parent > 1 && guard < app->process.process_snapshot_count;
         guard++) {
        const ssize_t parent_index = indexed_snapshot_index_for_pid(
            pid_index, parent);
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

typedef struct {
    char *name;
    char *user;
    char *command;
} FoldedProcessText;

static gboolean folded_process_text_init(
    const LsmProcessInfo *process, FoldedProcessText *folded)
{
    if (!process || !folded) return FALSE;
    memset(folded, 0, sizeof(*folded));
    folded->name = g_utf8_casefold(process->name, -1);
    folded->user = g_utf8_casefold(process->user, -1);
    folded->command = g_utf8_casefold(process->command, -1);
    if (folded->name && folded->user && folded->command)
        return TRUE;
    g_free(folded->name);
    g_free(folded->user);
    g_free(folded->command);
    memset(folded, 0, sizeof(*folded));
    return FALSE;
}

static void folded_process_text_clear(FoldedProcessText *folded)
{
    if (!folded) return;
    g_free(folded->name);
    g_free(folded->user);
    g_free(folded->command);
    memset(folded, 0, sizeof(*folded));
}

static gboolean folded_process_contains(
    const FoldedProcessText *folded, const char *needle)
{
    if (!needle || !*needle) return TRUE;
    return folded &&
        ((folded->name && strstr(folded->name, needle)) ||
         (folded->user && strstr(folded->user, needle)) ||
         (folded->command && strstr(folded->command, needle)));
}

static gboolean process_excluded(
    const GPtrArray *folded_filters,
    const FoldedProcessText *folded_process)
{
    if (!folded_filters) return FALSE;
    for (guint index = 0U; index < folded_filters->len; index++) {
        const char *filter = folded_filters->pdata[index];
        if (filter && *filter &&
            folded_process_contains(folded_process, filter))
            return TRUE;
    }
    return FALSE;
}

static gboolean text_matches_folded(const char *text,
                                    const char *folded_needle)
{
    if (!folded_needle || !*folded_needle) return TRUE;
    if (!text) return FALSE;
    char *folded_text = g_utf8_casefold(text, -1);
    const gboolean matches = folded_text &&
        strstr(folded_text, folded_needle) != NULL;
    g_free(folded_text);
    return matches;
}

static gboolean process_matches_search(
    const LsmProcessInfo *process, const char *group_name,
    const char *folded_search, const FoldedProcessText *folded_process)
{
    if (!folded_search || !*folded_search) return TRUE;
    char pid[32];
    snprintf(pid, sizeof(pid), "%llu",
             (unsigned long long)process->pid);
    return text_matches_folded(group_name, folded_search) ||
           folded_process_contains(folded_process, folded_search) ||
           strstr(pid, folded_search) != NULL;
}

static GPtrArray *fold_process_filters(const LsmApp *app)
{
    if (!app || !app->process.filters || app->process.filters->len == 0U)
        return NULL;
    GPtrArray *folded =
        g_ptr_array_new_with_free_func(g_free);
    if (!folded) return NULL;
    for (guint index = 0U; index < app->process.filters->len; index++) {
        const char *filter =
            g_ptr_array_index(app->process.filters, index);
        char *value = g_utf8_casefold(filter ? filter : "", -1);
        if (!value) {
            g_ptr_array_free(folded, TRUE);
            return NULL;
        }
        g_ptr_array_add(folded, value);
    }
    return folded;
}

static gboolean cgroup_path_has_slice(const char *path,
                                      const char *slice)
{
    if (!path || !*path || !slice || !*slice) return FALSE;
    const size_t slice_length = strlen(slice);
    const char *cursor = path;
    while ((cursor = strstr(cursor, slice))) {
        const gboolean left_boundary =
            cursor == path || cursor[-1] == '/';
        const char after = cursor[slice_length];
        const gboolean right_boundary = after == '\0' || after == '/';
        if (left_boundary && right_boundary) return TRUE;
        cursor += slice_length;
    }
    return FALSE;
}

static void process_identity_uncached(
                             const LsmApp *app, size_t process_index,
                             GHashTable *pid_index,
                             ProcessCategory *category, char *key,
                             size_t key_size, char *name, size_t name_size,
                             char *icon, size_t icon_size)
{
    const LsmProcessInfo *process = &app->process.process_snapshot[process_index];
    const LsmApplicationEntry *entry =
        application_for_process(app, process_index, pid_index);
    if (entry) {
        *category = PROCESS_CATEGORY_APPLICATION;
        snprintf(key, key_size, "app:%s", entry->id);
        lsm_copy_string(name, name_size, entry->name);
        lsm_copy_string(icon, icon_size, entry->icon);
        return;
    }

    if (process->cgroup_v2 &&
        cgroup_path_has_slice(process->cgroup_path, "app.slice")) {
        *category = PROCESS_CATEGORY_APPLICATION;
        snprintf(key, key_size, "cgroup-app:%s", process->name);
    } else if (process->cgroup_v2 &&
               cgroup_path_has_slice(process->cgroup_path,
                                     "background.slice")) {
        *category = PROCESS_CATEGORY_BACKGROUND;
        snprintf(key, key_size, "background:%s", process->name);
    } else {
        *category = process->owned_by_current_user
            ? PROCESS_CATEGORY_BACKGROUND : PROCESS_CATEGORY_SYSTEM;
        snprintf(key, key_size, "%s:%s",
                 *category == PROCESS_CATEGORY_BACKGROUND
                     ? "background" : "system",
                 process->name);
    }
    lsm_copy_string(name, name_size, process->name);
    lsm_copy_string(icon, icon_size, category_icon(*category));
}

static void process_identity(LsmApp *app, size_t process_index,
                             GHashTable *pid_index,
                             ProcessCategory *category, char *key,
                             size_t key_size, char *name, size_t name_size,
                             char *icon, size_t icon_size)
{
    if (!app || process_index >= app->process.process_snapshot_count ||
        !category || !key || key_size == 0U ||
        !name || name_size == 0U || !icon || icon_size == 0U)
        return;

    if (!app->processes.process_identity_cache)
        app->processes.process_identity_cache = g_hash_table_new_full(
            process_identity_cache_hash, process_identity_cache_equal,
            g_free, g_free);

    const LsmProcessInfo *process =
        &app->process.process_snapshot[process_index];
    const ProcessIdentityCacheKey lookup = {
        .pid = process->pid,
        .instance_id = process->instance_id
    };
    ProcessIdentityCacheEntry *cached =
        app->processes.process_identity_cache
            ? g_hash_table_lookup(
                  app->processes.process_identity_cache, &lookup)
            : NULL;
    const double now = lsm_monotonic_seconds();
    const gboolean fresh =
        cached && now > 0.0 && cached->refreshed_at > 0.0 &&
        now >= cached->refreshed_at &&
        now - cached->refreshed_at < LSM_PROCESS_IDENTITY_CACHE_SECONDS;

    if (!fresh) {
        ProcessCategory resolved_category = PROCESS_CATEGORY_BACKGROUND;
        char resolved_key[LSM_NAME_LEN * 2U] = "";
        char resolved_name[LSM_NAME_LEN] = "";
        char resolved_icon[LSM_NAME_LEN] = "";
        process_identity_uncached(
            app, process_index, pid_index, &resolved_category,
            resolved_key, sizeof(resolved_key),
            resolved_name, sizeof(resolved_name),
            resolved_icon, sizeof(resolved_icon));

        if (!cached && app->processes.process_identity_cache) {
            ProcessIdentityCacheKey *stored_key =
                g_new0(ProcessIdentityCacheKey, 1U);
            ProcessIdentityCacheEntry *stored =
                g_new0(ProcessIdentityCacheEntry, 1U);
            if (stored_key && stored) {
                *stored_key = lookup;
                g_hash_table_insert(
                    app->processes.process_identity_cache,
                    stored_key, stored);
                cached = stored;
            } else {
                g_free(stored_key);
                g_free(stored);
            }
        }
        if (cached) {
            cached->category = resolved_category;
            lsm_copy_string(
                cached->key, sizeof(cached->key), resolved_key);
            lsm_copy_string(
                cached->name, sizeof(cached->name), resolved_name);
            lsm_copy_string(
                cached->icon, sizeof(cached->icon), resolved_icon);
            cached->refreshed_at = now > 0.0 ? now : 0.0;
        } else {
            *category = resolved_category;
            lsm_copy_string(key, key_size, resolved_key);
            lsm_copy_string(name, name_size, resolved_name);
            lsm_copy_string(icon, icon_size, resolved_icon);
            return;
        }
    }

    cached->seen_generation = app->process.process_snapshot_generation;
    *category = cached->category;
    lsm_copy_string(key, key_size, cached->key);
    lsm_copy_string(name, name_size, cached->name);
    lsm_copy_string(icon, icon_size, cached->icon);
}

static void group_destroy(gpointer data)
{
    ProcessGroup *group = data;
    if (!group) return;
    free(group->indices);
    free(group);
}

static void group_cache_remove_fast(GPtrArray *cache, guint index)
{
    if (!cache || index >= cache->len) return;
    const guint last = cache->len - 1U;
    if (index != last) {
        gpointer removed = cache->pdata[index];
        cache->pdata[index] = cache->pdata[last];
        cache->pdata[last] = removed;
    }
    g_ptr_array_set_size(cache, (gint)last);
}

static gboolean group_append(ProcessGroup *group, size_t process_index,
                             const LsmProcessInfo *process)
{
    size_t required = 0U;
    if (!group || !process ||
        !lsm_size_add_checked(group->count, 1U, &required) ||
        !lsm_array_reserve((void **)&group->indices, &group->capacity,
                           sizeof(*group->indices), required, 4U))
        return FALSE;
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
    return lsm_ascii_compare_ci(a->name, b->name);
}

static GPtrArray *collect_groups(LsmApp *app)
{
    if (!app) return NULL;
    GPtrArray *groups = g_ptr_array_new();
    if (!groups) return NULL;

    GHashTable *pid_index = refresh_snapshot_pid_index(app);
    if (!app->processes.process_group_cache)
        app->processes.process_group_cache =
            g_ptr_array_new_with_free_func(group_destroy);
    if (!app->processes.process_group_index)
        app->processes.process_group_index =
            g_hash_table_new(g_str_hash, g_str_equal);
    GPtrArray *group_cache = app->processes.process_group_cache;
    GHashTable *group_index = app->processes.process_group_index;
    if (!pid_index || !group_cache || !group_index) {
        g_ptr_array_free(groups, TRUE);
        return NULL;
    }

    g_hash_table_remove_all(group_index);
    for (guint index = group_cache->len; index > 0U; index--) {
        ProcessGroup *group =
            g_ptr_array_index(group_cache, index - 1U);
        if (group && group->count == 0U)
            group_cache_remove_fast(group_cache, index - 1U);
    }
    for (guint index = 0U; index < group_cache->len; index++) {
        ProcessGroup *group = g_ptr_array_index(group_cache, index);
        group->count = 0U;
        memset(&group->metrics, 0, sizeof(group->metrics));
        g_hash_table_insert(group_index, group->key, group);
    }

    const char *search = gtk_entry_get_text(
        GTK_ENTRY(app->processes.processes_search));
    char *folded_search = search && *search
        ? g_utf8_casefold(search, -1) : NULL;
    GPtrArray *folded_filters = fold_process_filters(app);
    if ((search && *search && !folded_search) ||
        (app->process.filters && app->process.filters->len > 0U &&
         !folded_filters)) {
        g_free(folded_search);
        if (folded_filters) g_ptr_array_free(folded_filters, TRUE);
        g_ptr_array_free(groups, TRUE);
        return NULL;
    }

    for (size_t index = 0U;
         index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process =
            &app->process.process_snapshot[index];
        FoldedProcessText folded_process = {0};
        const gboolean need_folded =
            (folded_search && *folded_search) || folded_filters != NULL;
        if (need_folded &&
            !folded_process_text_init(process, &folded_process)) {
            g_free(folded_search);
            if (folded_filters)
                g_ptr_array_free(folded_filters, TRUE);
            g_ptr_array_free(groups, TRUE);
            return NULL;
        }
        if (process_excluded(folded_filters, &folded_process)) {
            folded_process_text_clear(&folded_process);
            continue;
        }

        ProcessCategory category = PROCESS_CATEGORY_BACKGROUND;
        char key[LSM_NAME_LEN * 2U];
        char name[LSM_NAME_LEN];
        char icon[LSM_NAME_LEN];
        process_identity(app, index, pid_index, &category, key,
                         sizeof(key), name, sizeof(name),
                         icon, sizeof(icon));
        if (!process_matches_search(
                process, name, folded_search, &folded_process)) {
            folded_process_text_clear(&folded_process);
            continue;
        }
        folded_process_text_clear(&folded_process);

        ProcessGroup *group = g_hash_table_lookup(group_index, key);
        if (!group) {
            group = calloc(1U, sizeof(*group));
            if (!group) {
                g_free(folded_search);
                if (folded_filters)
                    g_ptr_array_free(folded_filters, TRUE);
                g_ptr_array_free(groups, TRUE);
                return NULL;
            }
            group->category = category;
            lsm_copy_string(group->key, sizeof(group->key), key);
            lsm_copy_string(group->name, sizeof(group->name), name);
            lsm_copy_string(group->icon, sizeof(group->icon), icon);
            g_ptr_array_add(group_cache, group);
            g_hash_table_insert(group_index, group->key, group);
        }
        if (group->count == 0U) {
            group->category = category;
            lsm_copy_string(group->name, sizeof(group->name), name);
            lsm_copy_string(group->icon, sizeof(group->icon), icon);
            g_ptr_array_add(groups, group);
        }
        if (!group_append(group, index, process)) {
            g_free(folded_search);
            if (folded_filters)
                g_ptr_array_free(folded_filters, TRUE);
            g_ptr_array_free(groups, TRUE);
            return NULL;
        }
    }

    g_free(folded_search);
    if (folded_filters) g_ptr_array_free(folded_filters, TRUE);
    if (app->processes.process_identity_cache) {
        uint64_t generation =
            app->process.process_snapshot_generation;
        g_hash_table_foreach_remove(
            app->processes.process_identity_cache,
            process_identity_cache_remove_stale,
            &generation);
    }
    g_ptr_array_sort(groups, compare_groups);
    return groups;
}

static uint64_t grouped_structure_signature(
    const LsmApp *app, const GPtrArray *groups)
{
    uint64_t xor_value = UINT64_C(0x243f6a8885a308d3);
    uint64_t sum_value = groups
        ? (uint64_t)groups->len * UINT64_C(0x9e3779b185ebca87)
        : 0U;
    if (!app || !groups) return xor_value ^ sum_value;

    for (guint group_index = 0U; group_index < groups->len; group_index++) {
        const ProcessGroup *group = g_ptr_array_index(groups, group_index);
        uint64_t group_hash = LSM_FNV1A64_OFFSET_BASIS;
        group_hash = lsm_fnv1a64_mix_byte(
            group_hash, (unsigned char)group->category);
        group_hash = lsm_fnv1a64_mix_text(group_hash, group->key);
        group_hash = lsm_fnv1a64_mix_text(group_hash, group->name);
        group_hash = lsm_fnv1a64_mix_text(group_hash, group->icon);
        uint64_t child_xor = 0U;
        uint64_t child_sum = 0U;
        for (size_t child = 0U; child < group->count; child++) {
            const LsmProcessInfo *process =
                &app->process.process_snapshot[group->indices[child]];
            uint64_t identity =
                ((uint64_t)process->pid * UINT64_C(0x94d049bb133111eb)) ^
                process->instance_id;
            const unsigned shift =
                (unsigned)(process->pid % 63U) + 1U;
            identity = (identity << shift) |
                       (identity >> (64U - shift));
            child_xor ^= identity;
            child_sum += identity * UINT64_C(0xbf58476d1ce4e5b9);
        }
        group_hash ^= child_xor;
        group_hash += child_sum;
        xor_value ^= group_hash;
        sum_value += group_hash * UINT64_C(0x94d049bb133111eb);
    }
    return xor_value ^ sum_value;
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
        lsm_copy_string(text, sizeof(text), value ? value : "");
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
        lsm_copy_string(text, sizeof(text), value && *value ? value : "N/A");
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
        gboolean available = FALSE;
        double value = 0.0;
        gtk_tree_model_get(model, iter,
                           GROUPED_COL_DISK, &value,
                           GROUPED_COL_DISK_AVAILABLE, &available, -1);
        if (!available) {
            snprintf(text, sizeof(text), "N/A");
        } else {
            lsm_format_rate(value, text, sizeof(text));
            heat_value = fmin(log10(value + 1.0) / 9.0 * 100.0, 100.0);
        }
    }
    g_object_set(renderer, "text", text, NULL);
    if (!lsm_process_heatmap_enabled(app) || heat_value <= 0.0) {
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

static void set_process_child_row(LsmApp *app,
                                  const ProcessGroup *group,
                                  const LsmProcessInfo *process,
                                  GtkTreeIter *iter)
{
    const double disk = process->read_bytes_per_sec +
                        process->write_bytes_per_sec;
    gtk_tree_store_set(app->processes.processes_store, iter,
        GROUPED_COL_ICON, group->icon,
        GROUPED_COL_NAME, process->name,
        GROUPED_COL_STATUS, process->state,
        GROUPED_COL_CPU, process->cpu_percent,
        GROUPED_COL_MEMORY, process->rss_bytes,
        GROUPED_COL_DISK, isfinite(disk) && disk > 0.0 ? disk : 0.0,
        GROUPED_COL_DISK_AVAILABLE, process->io_rate_available,
        GROUPED_COL_GPU, process->gpu_percent,
        GROUPED_COL_GPU_ENGINE,
            process->gpu_available && process->gpu_engine[0]
                ? process->gpu_engine : "N/A",
        GROUPED_COL_GPU_AVAILABLE, process->gpu_available,
        GROUPED_COL_PID, process->pid,
        GROUPED_COL_KIND, PROCESS_ROW_PROCESS,
        GROUPED_COL_KEY, group->key,
        -1);
}

static void set_group_row(LsmApp *app, const ProcessGroup *group,
                          GtkTreeIter *iter)
{
    char name[LSM_NAME_LEN + 32U];
    if (group->count > 1U)
        snprintf(name, sizeof(name), "%s (%zu)", group->name, group->count);
    else
        lsm_copy_string(name, sizeof(name), group->name);
    const char *status = group->metrics.all_stopped ? "Suspended" :
                         group->metrics.all_efficient
                         ? "Efficiency mode" : "";
    const LsmProcessInfo *representative =
        &app->process.process_snapshot[group->indices[0]];
    gtk_tree_store_set(app->processes.processes_store, iter,
        GROUPED_COL_ICON, group->icon,
        GROUPED_COL_NAME, name,
        GROUPED_COL_STATUS, status,
        GROUPED_COL_CPU, group->metrics.cpu_percent,
        GROUPED_COL_MEMORY, group->metrics.memory_bytes,
        GROUPED_COL_DISK, group->metrics.disk_bytes_per_sec,
        GROUPED_COL_DISK_AVAILABLE, group->metrics.disk_available,
        GROUPED_COL_GPU, group->metrics.gpu_percent,
        GROUPED_COL_GPU_ENGINE,
            group->metrics.gpu_available && group->metrics.gpu_engine[0]
                ? group->metrics.gpu_engine : "N/A",
        GROUPED_COL_GPU_AVAILABLE, group->metrics.gpu_available,
        GROUPED_COL_PID, representative->pid,
        GROUPED_COL_KIND, PROCESS_ROW_GROUP,
        GROUPED_COL_KEY, group->key,
        -1);
}

static void append_process_child(LsmApp *app, const ProcessGroup *group,
                                 size_t group_index, GtkTreeIter *parent,
                                 LsmProcessId desired_pid,
                                 LsmProcessInstanceId desired_instance_id)
{
    const LsmProcessInfo *process =
        &app->process.process_snapshot[group->indices[group_index]];
    GtkTreeIter child;
    gtk_tree_store_append(app->processes.processes_store, &child, parent);
    set_process_child_row(app, group, process, &child);
    if (desired_pid == process->pid &&
        desired_instance_id == process->instance_id)
        select_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                    GTK_TREE_MODEL(app->processes.processes_store), &child);
}

static void append_group(LsmApp *app, const ProcessGroup *group,
                         GtkTreeIter *category, GHashTable *expanded,
                         LsmProcessId desired_pid,
                         LsmProcessInstanceId desired_instance_id,
                         const char *desired_group)
{
    GtkTreeIter group_iter;
    gtk_tree_store_append(app->processes.processes_store, &group_iter, category);
    set_group_row(app, group, &group_iter);

    if (group->count > 1U) {
        for (size_t index = 0U; index < group->count; index++)
            append_process_child(app, group, index, &group_iter,
                                 desired_pid, desired_instance_id);
        if (g_hash_table_contains(expanded, group->key))
            expand_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                        GTK_TREE_MODEL(app->processes.processes_store), &group_iter);
    }
    char name[LSM_NAME_LEN + 32U];
    if (group->count > 1U)
        snprintf(name, sizeof(name), "%s (%zu)", group->name, group->count);
    else
        lsm_copy_string(name, sizeof(name), group->name);
    const LsmProcessInfo *representative =
        &app->process.process_snapshot[group->indices[0]];
    if (desired_group && *desired_group &&
        strcmp(desired_group, name) == 0)
        select_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                    GTK_TREE_MODEL(app->processes.processes_store), &group_iter);
    else if (group->count == 1U &&
             desired_pid == representative->pid &&
             desired_instance_id == representative->instance_id)
        select_iter(GTK_TREE_VIEW(app->processes.processes_tree),
                    GTK_TREE_MODEL(app->processes.processes_store), &group_iter);
}

static gboolean update_group_children(LsmApp *app, const ProcessGroup *group,
                                      GHashTable *pid_index,
                                      GtkTreeIter *group_iter)
{
    GtkTreeModel *model = GTK_TREE_MODEL(app->processes.processes_store);
    GtkTreeIter child;
    const gboolean has_children =
        gtk_tree_model_iter_children(model, &child, group_iter);
    if (group->count <= 1U) return !has_children;
    if (!has_children) return FALSE;

    size_t updated = 0U;
    do {
        guint64 pid = 0U;
        gtk_tree_model_get(model, &child, GROUPED_COL_PID, &pid, -1);
        if (pid == 0U || pid > UINT_MAX) return FALSE;
        gpointer value = g_hash_table_lookup(
            pid_index, GUINT_TO_POINTER((guint)pid));
        if (!value) return FALSE;
        const size_t process_index =
            (size_t)(GPOINTER_TO_UINT(value) - 1U);
        set_process_child_row(
            app, group, &app->process.process_snapshot[process_index], &child);
        updated++;
    } while (gtk_tree_model_iter_next(model, &child));
    return updated == group->count;
}

static gboolean update_grouped_model_values(LsmApp *app, GPtrArray *groups)
{
    if (!app || !groups || !app->processes.processes_store)
        return FALSE;
    GHashTable *group_index = app->processes.process_group_index;
    GHashTable *pid_index = app->processes.process_pid_index;
    if (!group_index || !pid_index) return FALSE;

    GtkTreeModel *model = GTK_TREE_MODEL(app->processes.processes_store);
    GtkTreeIter category;
    gboolean valid_category =
        gtk_tree_model_get_iter_first(model, &category);
    size_t updated_groups = 0U;
    while (valid_category) {
        gint kind = PROCESS_ROW_PROCESS;
        gtk_tree_model_get(
            model, &category, GROUPED_COL_KIND, &kind, -1);
        if (kind != PROCESS_ROW_CATEGORY) return FALSE;

        GtkTreeIter group_iter;
        gboolean valid_group =
            gtk_tree_model_iter_children(model, &group_iter, &category);
        while (valid_group) {
            char *key = NULL;
            gtk_tree_model_get(
                model, &group_iter, GROUPED_COL_KEY, &key, -1);
            ProcessGroup *group =
                key ? g_hash_table_lookup(group_index, key) : NULL;
            g_free(key);
            if (!group) return FALSE;

            set_group_row(app, group, &group_iter);
            if (!update_group_children(
                    app, group, pid_index, &group_iter))
                return FALSE;
            updated_groups++;
            valid_group =
                gtk_tree_model_iter_next(model, &group_iter);
        }
        valid_category =
            gtk_tree_model_iter_next(model, &category);
    }
    return updated_groups == groups->len;
}

/* Model rebuilds preserve the user-visible selection and expanded groups by
 * stable process/group identity rather than by transient GtkTreePath values. */
static void rebuild_grouped_model(LsmApp *app, GPtrArray *groups)
{
    const LsmProcessId desired_pid = app->process.selected_pid;
    const LsmProcessInstanceId desired_instance_id =
        app->process.selected_instance_id;
    char desired_group[LSM_NAME_LEN];
    lsm_copy_string(desired_group, sizeof(desired_group), app->process.selected_group_name);
    GHashTable *expanded = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
    gtk_tree_view_map_expanded_rows(
        GTK_TREE_VIEW(app->processes.processes_tree), collect_expanded_group, expanded);

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
            GROUPED_COL_DISK_AVAILABLE, FALSE,
            GROUPED_COL_GPU_ENGINE, "",
            GROUPED_COL_PID, 0,
            GROUPED_COL_KIND, PROCESS_ROW_CATEGORY,
            GROUPED_COL_KEY, "",
            -1);
        for (guint index = 0U; index < groups->len; index++) {
            const ProcessGroup *group = g_ptr_array_index(groups, index);
            if (group->category == category)
                append_group(app, group, &category_iter, expanded,
                             desired_pid, desired_instance_id,
                             desired_group);
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
    if (!app || !app->processes.processes_model_dirty ||
        !app->processes.processes_store)
        return;
    GPtrArray *groups = collect_groups(app);
    if (!groups) return;

    const uint64_t signature =
        grouped_structure_signature(app, groups);
    if (!app->processes.processes_structure_valid ||
        app->processes.processes_structure_signature != signature ||
        !update_grouped_model_values(app, groups)) {
        rebuild_grouped_model(app, groups);
        app->processes.processes_structure_signature = signature;
        app->processes.processes_structure_valid = TRUE;
    }
    g_ptr_array_free(groups, TRUE);
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
                const ssize_t snapshot_index = snapshot_index_for_pid(
                    app, (LsmProcessId)pid);
                if (snapshot_index < 0) continue;
                ordered[index].pid = (LsmProcessId)pid;
                ordered[index].instance_id = app->process.process_snapshot[
                    (size_t)snapshot_index].instance_id;
                ordered[index].depth = process_depth(app,
                                                     (LsmProcessId)pid);
                index++;
            }
        } while (gtk_tree_model_iter_next(model, &child));
    }
    qsort(ordered, index, sizeof(*ordered), compare_pid_depth);
    app->process.selected_group_pids = calloc(
        index, sizeof(*app->process.selected_group_pids));
    app->process.selected_group_instance_ids = calloc(
        index, sizeof(*app->process.selected_group_instance_ids));
    if (app->process.selected_group_pids &&
        app->process.selected_group_instance_ids) {
        for (size_t item = 0U; item < index; item++) {
            app->process.selected_group_pids[item] = ordered[item].pid;
            app->process.selected_group_instance_ids[item] =
                ordered[item].instance_id;
        }
        app->process.selected_group_count = index;
        lsm_copy_string(app->process.selected_group_name, sizeof(app->process.selected_group_name),
                        name);
    } else {
        free(app->process.selected_group_pids);
        free(app->process.selected_group_instance_ids);
        app->process.selected_group_pids = NULL;
        app->process.selected_group_instance_ids = NULL;
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
    lsm_process_selection_set(app, pid);
    const gboolean valid = app->process.selected_pid > 1;
    gtk_widget_set_sensitive(app->processes.processes_end_button, valid);
    gtk_widget_set_sensitive(app->processes.processes_inspect_button,
                             app->process.selected_pid > 0);
    lsm_process_record_action_sync(
        app, valid, app->process.selected_group_count > 0U);
    g_free(name);
}

static void grouped_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    LsmApp *app = user_data;
    app->processes.processes_structure_valid = FALSE;
    app->processes.processes_model_dirty = TRUE;
    lsm_processes_present_snapshot(app);
}

static void grouped_end_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_processes_end_selected(user_data);
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
        G_TYPE_UINT64, G_TYPE_DOUBLE, G_TYPE_BOOLEAN, G_TYPE_DOUBLE,
        G_TYPE_STRING, G_TYPE_BOOLEAN,
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
    if (app->processes.processes_store)
        g_object_unref(app->processes.processes_store);
    app->processes.processes_store = NULL;
    if (app->processes.process_pid_index)
        g_hash_table_destroy(app->processes.process_pid_index);
    app->processes.process_pid_index = NULL;
    if (app->processes.process_group_index)
        g_hash_table_destroy(app->processes.process_group_index);
    app->processes.process_group_index = NULL;
    if (app->processes.process_group_cache)
        g_ptr_array_free(app->processes.process_group_cache, TRUE);
    app->processes.process_group_cache = NULL;
    if (app->processes.process_identity_cache)
        g_hash_table_destroy(app->processes.process_identity_cache);
    app->processes.process_identity_cache = NULL;
}
