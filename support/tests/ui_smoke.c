// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ui_smoke.c
 * @brief Consolidated UI and preferences regression smoke suite.
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stddef.h>
#include <stdio.h>

int smoke_case_dbus_models(void);
int smoke_case_preferences(void);
int smoke_case_startup(void);
int smoke_case_ui_update(void);
int smoke_case_task_manager_layout(void);

/* ---- dbus_models ---- */
#define main smoke_case_dbus_models
#define sample_units lsm_test_dbus_models_sample_units
#define sample_unit_files lsm_test_dbus_models_sample_unit_files
#define sample_sessions lsm_test_dbus_models_sample_sessions
#define sample_properties lsm_test_dbus_models_sample_properties
#define manager_call_on_bus lsm_test_dbus_models_manager_call_on_bus
#define service_find lsm_test_dbus_models_service_find
#define service_get lsm_test_dbus_models_service_get
#define service_compare lsm_test_dbus_models_service_compare
#define merge_loaded_units lsm_test_dbus_models_merge_loaded_units
#define merge_unit_files lsm_test_dbus_models_merge_unit_files
#define collect_services lsm_test_dbus_models_collect_services
#define login_manager_call_on_bus lsm_test_dbus_models_login_manager_call_on_bus
#define session_properties_on_bus lsm_test_dbus_models_session_properties_on_bus
#define property_string lsm_test_dbus_models_property_string
#define property_boolean lsm_test_dbus_models_property_boolean
#define property_uint32 lsm_test_dbus_models_property_uint32
#define property_uint64 lsm_test_dbus_models_property_uint64
#define fill_account_identity lsm_test_dbus_models_fill_account_identity
#define parse_session_list lsm_test_dbus_models_parse_session_list
#define collect_sessions lsm_test_dbus_models_collect_sessions
/**
 * @file dbus_models_smoke.c
 * @brief Synthetic systemd and logind D-Bus model tests.
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#include "../src/service_backend_linux.c"
#include "../src/user_backend_linux.c"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static GVariant *sample_units(void)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a(ssssssouso)"));
    g_variant_builder_add(builder, "(ssssssouso)",
                          "alpha.service", "Alpha Service", "loaded", "active", "running", "",
                          "/org/freedesktop/systemd1/unit/alpha_2eservice", 0u, "", "/");
    g_variant_builder_add(builder, "(ssssssouso)",
                          "not-a-service.mount", "Ignored Mount", "loaded", "active", "mounted", "",
                          "/org/freedesktop/systemd1/unit/not_2da_2dservice_2emount", 0u, "", "/");
    GVariant *reply = g_variant_new("(@a(ssssssouso))", g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
    return reply;
}

static GVariant *sample_unit_files(void)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a(ss)"));
    g_variant_builder_add(builder, "(ss)", "/usr/lib/systemd/system/alpha.service", "enabled");
    g_variant_builder_add(builder, "(ss)", "/usr/lib/systemd/system/beta.service", "disabled");
    GVariant *reply = g_variant_new("(@a(ss))", g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
    return reply;
}

static GVariant *sample_sessions(void)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a(susso)"));
    g_variant_builder_add(builder, "(susso)", "2", 1000u, "shannon", "seat0",
                          "/org/freedesktop/login1/session/_32");
    GVariant *reply = g_variant_new("(@a(susso))", g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
    return reply;
}

static GVariant *sample_properties(void)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(builder, "{sv}", "State", g_variant_new_string("active"));
    g_variant_builder_add(builder, "{sv}", "Type", g_variant_new_string("x11"));
    g_variant_builder_add(builder, "{sv}", "Remote", g_variant_new_boolean(FALSE));
    g_variant_builder_add(builder, "{sv}", "Leader", g_variant_new_uint32(4242));
    g_variant_builder_add(builder, "{sv}", "Timestamp", g_variant_new_uint64(1700000000000000ULL));
    GVariant *dictionary = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return dictionary;
}

int main(void)
{
    LsmServiceEntry *services = NULL;
    size_t count = 0, capacity = 0;
    GVariant *units = sample_units();
    merge_loaded_units(units, &services, &count, &capacity);
    g_variant_unref(units);
    GVariant *files = sample_unit_files();
    merge_unit_files(files, &services, &count, &capacity);
    g_variant_unref(files);
    assert(count == 2);
    ssize_t alpha = service_find(services, count, "alpha.service");
    ssize_t beta = service_find(services, count, "beta.service");
    assert(alpha >= 0 && beta >= 0);
    assert(strcmp(services[alpha].description, "Alpha Service") == 0);
    assert(strcmp(services[alpha].active, "active") == 0);
    assert(strcmp(services[alpha].startup, "enabled") == 0);
    assert(strcmp(services[beta].startup, "disabled") == 0);
    free(services);

    GVariant *sessions_reply = sample_sessions();
    size_t session_count = 0;
    LsmLinuxSessionRecord *sessions = parse_session_list(sessions_reply, &session_count);
    g_variant_unref(sessions_reply);
    assert(session_count == 1);
    assert(strcmp(sessions[0].session.id, "2") == 0);
    assert(sessions[0].uid == 1000);
    assert(strcmp(sessions[0].session.username, "shannon") == 0);
    assert(strcmp(sessions[0].session.seat, "seat0") == 0);
    free(sessions);

    GVariant *properties = sample_properties();
    char state[32] = "", type[32] = "";
    property_string(properties, "State", state, sizeof(state));
    property_string(properties, "Type", type, sizeof(type));
    assert(strcmp(state, "active") == 0);
    assert(strcmp(type, "x11") == 0);
    assert(!property_boolean(properties, "Remote"));
    assert(property_uint32(properties, "Leader") == 4242);
    assert(property_uint64(properties, "Timestamp") == 1700000000000000ULL);
    g_variant_unref(properties);

    puts("systemd/logind D-Bus model signatures passed");
    return 0;
}

#undef main
#undef collect_sessions
#undef parse_session_list
#undef fill_account_identity
#undef property_uint64
#undef property_uint32
#undef property_boolean
#undef property_string
#undef session_properties_on_bus
#undef login_manager_call_on_bus
#undef collect_services
#undef merge_unit_files
#undef merge_loaded_units
#undef service_compare
#undef service_get
#undef service_find
#undef manager_call_on_bus
#undef sample_properties
#undef sample_sessions
#undef sample_unit_files
#undef sample_units
#undef _POSIX_C_SOURCE

/* ---- preferences ---- */
#define main smoke_case_preferences
/**
 * @file preferences_smoke.c
 * @brief Preference round-trip and invalid-value fallback regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "app_internal.h"
#include "preferences.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    char directory[] = "/tmp/lsm-preferences-XXXXXX";
    assert(mkdtemp(directory));
    LsmApp *saved = calloc(1U, sizeof(*saved));
    LsmApp *loaded = calloc(1U, sizeof(*loaded));
    assert(saved && loaded);
    snprintf(saved->paths.config_dir, sizeof(saved->paths.config_dir), "%s", directory);
    snprintf(saved->paths.preferences_path, sizeof(saved->paths.preferences_path),
             "%s/preferences.conf", directory);
    saved->runtime.update_interval_ms = 2000U;
    saved->runtime.theme_mode = INFILTRATR_THEME_NIGHT;
    saved->runtime.newer_on_right = false;
    saved->runtime.network_use_bits = true;
    saved->runtime.always_on_top = true;
    saved->runtime.compact_summary = true;
    saved->runtime.window_width = 1440;
    saved->runtime.window_height = 900;
    saved->runtime.window_maximized = true;
    saved->runtime.last_tab = LSM_TAB_DETAILS;
    saved->runtime.page_scroll[LSM_TAB_PROCESSES] = 123.5;
    saved->runtime.page_scroll[LSM_TAB_DETAILS] = 456.25;
    strcpy(saved->runtime.selected_performance_page, "disk-nvme0n1");
    lsm_preferences_save(saved);
    struct stat status;
    assert(stat(saved->paths.preferences_path, &status) == 0);
    assert((status.st_mode & 0777) == 0600);

    loaded->runtime.update_interval_ms = 1000U;
    loaded->runtime.theme_mode = INFILTRATR_THEME_SYSTEM;
    loaded->runtime.newer_on_right = true;
    loaded->runtime.window_width = 1280;
    loaded->runtime.window_height = 800;
    snprintf(loaded->paths.config_dir, sizeof(loaded->paths.config_dir),
             "%s", saved->paths.config_dir);
    snprintf(loaded->paths.preferences_path, sizeof(loaded->paths.preferences_path),
             "%s", saved->paths.preferences_path);
    lsm_preferences_load(loaded);
    assert(loaded->runtime.update_interval_ms == 2000U);
    assert(loaded->runtime.theme_mode == INFILTRATR_THEME_NIGHT);
    assert(!loaded->runtime.newer_on_right);
    assert(loaded->runtime.network_use_bits);
    assert(loaded->runtime.always_on_top);
    assert(loaded->runtime.compact_summary);
    assert(loaded->runtime.window_width == 1440 && loaded->runtime.window_height == 900);
    assert(loaded->runtime.window_maximized);
    assert(loaded->runtime.last_tab == LSM_TAB_DETAILS);
    assert(fabs(loaded->runtime.page_scroll[LSM_TAB_PROCESSES] - 123.5) < 0.001);
    assert(fabs(loaded->runtime.page_scroll[LSM_TAB_DETAILS] - 456.25) < 0.001);
    assert(strcmp(loaded->runtime.selected_performance_page, "disk-nvme0n1") == 0);

    FILE *file = fopen(saved->paths.preferences_path, "w");
    assert(file);
    fputs("page_scroll_1=321,750\n", file);
    assert(fclose(file) == 0);
    loaded->runtime.page_scroll[LSM_TAB_PROCESSES] = 0.0;
    lsm_preferences_load(loaded);
    assert(fabs(loaded->runtime.page_scroll[LSM_TAB_PROCESSES] - 321.75) < 0.001);
    lsm_preferences_save(loaded);
    file = fopen(saved->paths.preferences_path, "r");
    assert(file);
    char canonical[4096];
    const size_t canonical_length =
        fread(canonical, 1U, sizeof(canonical) - 1U, file);
    canonical[canonical_length] = '\0';
    assert(fclose(file) == 0);
    assert(strstr(canonical, "page_scroll_1=321.750"));
    assert(!strstr(canonical, "page_scroll_1=321,750"));

    file = fopen(saved->paths.preferences_path, "w");
    assert(file);
    fputs("window_width=640\nwindow_height=420\n", file);
    assert(fclose(file) == 0);
    loaded->runtime.window_width = 1280;
    loaded->runtime.window_height = 800;
    lsm_preferences_load(loaded);
    assert(loaded->runtime.window_width == 640 && loaded->runtime.window_height == 420);

    file = fopen(saved->paths.preferences_path, "w");
    assert(file);
    fputs("update_interval_ms=7\ntheme_mode=ultraviolet\n"
          "window_width=-1\npage_scroll_1=nan\nlast_tab=999\n", file);
    assert(fclose(file) == 0);
    loaded->runtime.update_interval_ms = 1000U;
    loaded->runtime.theme_mode = INFILTRATR_THEME_DAY;
    loaded->runtime.window_width = 1280;
    loaded->runtime.page_scroll[LSM_TAB_PROCESSES] = 12.0;
    loaded->runtime.last_tab = LSM_TAB_PERFORMANCE;
    lsm_preferences_load(loaded);
    assert(loaded->runtime.update_interval_ms == 1000U);
    assert(loaded->runtime.theme_mode == INFILTRATR_THEME_DAY);
    assert(loaded->runtime.window_width == 1280);
    assert(loaded->runtime.page_scroll[LSM_TAB_PROCESSES] == 12.0);
    assert(loaded->runtime.last_tab == LSM_TAB_PERFORMANCE);

    unlink(saved->paths.preferences_path);
    rmdir(directory);
    free(loaded);
    free(saved);
    puts("Preference round-trip, theme, page scroll and invalid-value fallback passed.");
    return 0;
}

#undef main
#undef _POSIX_C_SOURCE

/* ---- startup ---- */
#define main smoke_case_startup
#define desktop_boolean lsm_test_startup_desktop_boolean
#define desktop_string lsm_test_startup_desktop_string
#define load_startup_entry lsm_test_startup_load_startup_entry
#define find_entry lsm_test_startup_find_entry
#define append_entry lsm_test_startup_append_entry
#define scan_directory lsm_test_startup_scan_directory
#define write_startup_override lsm_test_startup_write_startup_override
/**
 * @file startup_smoke.c
 * @brief XDG startup parsing and reversible override test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "../src/startup_backend_linux.c"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    char temporary[] = "/tmp/lsm-startup-test-XXXXXX";
    char *root = mkdtemp(temporary);
    assert(root != NULL);
    assert(setenv("XDG_CONFIG_HOME", root, 1) == 0);

    char source[PATH_MAX];
    snprintf(source, sizeof(source), "%s/source.desktop", root);
    const char desktop[] =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Backup Agent\n"
        "Comment=Test startup entry\n"
        "Exec=/usr/bin/backup-agent --quiet\n"
        "Hidden=false\n"
        "X-GNOME-Autostart-enabled=true\n";
    assert(g_file_set_contents(source, desktop, -1, NULL));

    LsmStartupEntry entry;
    assert(load_startup_entry(source, "backup.desktop", FALSE, &entry));
    assert(entry.enabled);
    assert(strcmp(entry.name, "Backup Agent") == 0);

    GError *error = NULL;
    assert(write_startup_override(source, "backup.desktop", FALSE, &error));
    assert(error == NULL);
    char target[PATH_MAX];
    snprintf(target, sizeof(target), "%s/autostart/backup.desktop", root);
    struct stat status;
    assert(stat(target, &status) == 0);
    assert((status.st_mode & 0777) == 0600);
    char *contents = NULL;
    assert(g_file_get_contents(target, &contents, NULL, NULL));
    assert(strstr(contents, "Hidden=true") != NULL);
    g_free(contents);

    assert(write_startup_override(target, "backup.desktop", TRUE, &error));
    assert(error == NULL);
    assert(g_file_get_contents(target, &contents, NULL, NULL));
    assert(strstr(contents, "Hidden=false") != NULL);
    g_free(contents);

    unlink(target);
    unlink(source);
    char autostart[PATH_MAX];
    snprintf(autostart, sizeof(autostart), "%s/autostart", root);
    rmdir(autostart);
    rmdir(root);
    puts("XDG startup parsing and override writes passed");
    return 0;
}

#undef main
#undef write_startup_override
#undef scan_directory
#undef append_entry
#undef find_entry
#undef load_startup_entry
#undef desktop_string
#undef desktop_boolean

/* ---- ui_update ---- */
#define main smoke_case_ui_update
/**
 * @file ui_update_smoke.c
 * @brief Verify unchanged labels are suppressed before entering GTK.
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "ui_helpers.h"

#include <stdio.h>

int main(void)
{
    if (lsm_ui_text_needs_update("42%", "42%")) return 1;
    if (!lsm_ui_text_needs_update("42%", "43%")) return 2;
    if (lsm_ui_text_needs_update(NULL, "")) return 3;
    if (!lsm_ui_text_needs_update(NULL, "N/A")) return 4;
    puts("Unchanged GTK label suppression policy passed.");
    return 0;
}

#undef main

/* ---- task_manager_layout ---- */
#define main smoke_case_task_manager_layout
/**
 * @file task_manager_layout_smoke.c
 * @brief Lock the canonical Performance-first tab order.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_config.h"

#include <stdio.h>

int main(void)
{
    if (LSM_TAB_PERFORMANCE != 0) return 1;
    if (LSM_TAB_PROCESSES != 1) return 2;
    if (LSM_TAB_APP_HISTORY != 2) return 3;
    if (LSM_TAB_STARTUP != 3) return 4;
    if (LSM_TAB_USERS != 4) return 5;
    if (LSM_TAB_DETAILS != 5) return 6;
    if (LSM_TAB_SERVICES != 6) return 7;
    if (LSM_TAB_FILESYSTEMS != 7) return 8;
    if (LSM_TAB_COUNT != 8) return 9;
    puts("Canonical Performance-first tab order passed.");
    return 0;
}

#undef main

typedef int (*LsmMergedSmokeCaseFunction)(void);
typedef struct { const char *name; LsmMergedSmokeCaseFunction function; } LsmMergedSmokeCase;

int main(void)
{
    static const LsmMergedSmokeCase cases[] = {
        {"dbus_models", smoke_case_dbus_models},
        {"preferences", smoke_case_preferences},
        {"startup", smoke_case_startup},
        {"ui_update", smoke_case_ui_update},
        {"task_manager_layout", smoke_case_task_manager_layout},
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0U; i < count; ++i) {
        const int status = cases[i].function();
        if (status != 0) {
            fprintf(stderr, "UI/preferences smoke suite: %s failed with status %d\n", cases[i].name, status);
            return status;
        }
    }
    printf("UI/preferences smoke suite passed (%zu cases).\n", count);
    return 0;
}
