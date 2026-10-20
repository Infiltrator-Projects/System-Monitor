// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file startup_backend_smoke.c
 * @brief Isolated Linux XDG autostart precedence and override safety regression.
 *
 * The higher-priority directory wins by desktop-file basename, including when
 * the winning file is invalid. Mutations must remain contained in the user's
 * autostart directory and cannot change another desktop identity.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "startup_backend.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool write_desktop(const char *folder, const char *name,
                          const char *contents)
{
    char *path = g_build_filename(folder, name, NULL);
    const gboolean ok = g_file_set_contents(path, contents, -1, NULL);
    g_free(path);
    return ok != FALSE;
}

static const LsmStartupEntry *find_entry(const LsmStartupEntry *entries,
                                         size_t count, const char *name)
{
    for (size_t index = 0U; index < count; index++)
        if (strcmp(entries[index].id, name) == 0) return &entries[index];
    return NULL;
}

static bool expect_entries(size_t count_expected, bool alpha_visible,
                           bool alpha_user, bool alpha_enabled,
                           bool beta_enabled)
{
    LsmStartupEntry *entries = NULL;
    size_t count = 0U;
    if (!lsm_startup_backend_collect(&entries, &count)) return false;
    const LsmStartupEntry *alpha = find_entry(entries, count, "alpha.desktop");
    const LsmStartupEntry *beta = find_entry(entries, count, "beta.desktop");
    const bool valid = count == count_expected &&
        (alpha != NULL) == alpha_visible &&
        (!alpha || (alpha->user_entry == alpha_user &&
                    alpha->enabled == alpha_enabled)) &&
        beta != NULL && beta->enabled == beta_enabled;
    lsm_startup_backend_free(entries);
    return valid;
}

static bool expect_tryexec(const char *id, bool enabled)
{
    LsmStartupEntry *entries = NULL;
    size_t count = 0U;
    if (!lsm_startup_backend_collect(&entries, &count)) return false;
    const LsmStartupEntry *entry = find_entry(entries, count, id);
    const bool valid = entry && entry->enabled == enabled;
    lsm_startup_backend_free(entries);
    return valid;
}

int main(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("lsm-startup-XXXXXX", &error);
    if (!root) {
        g_printerr("Unable to create startup fixture: %s\n",
                   error ? error->message : "unknown error");
        g_clear_error(&error);
        return 1;
    }
    char *user = g_build_filename(root, "user", NULL);
    char *system = g_build_filename(root, "system", NULL);
    char *user_autostart = g_build_filename(user, "autostart", NULL);
    char *system_autostart = g_build_filename(system, "autostart", NULL);
    char *user_alpha = g_build_filename(user_autostart, "alpha.desktop", NULL);
    char *user_beta = g_build_filename(user_autostart, "beta.desktop", NULL);
    char *system_alpha = g_build_filename(system_autostart, "alpha.desktop", NULL);
    char *system_beta = g_build_filename(system_autostart, "beta.desktop", NULL);
    char *outside = g_build_filename(root, "escape.desktop", NULL);
    static const char alpha[] =
        "[Desktop Entry]\nType=Application\nName=System Alpha\nExec=/bin/true\n";
    static const char beta[] =
        "[Desktop Entry]\nType=Application\nName=System Beta\nExec=/bin/true\n";
    static const char disabled[] =
        "[Desktop Entry]\nType=Application\nName=User Alpha\n"
        "Exec=/bin/true\nHidden=true\n";
    static const char malformed[] =
        "[Desktop Entry]\nType=Link\nURL=https://example.invalid/\n";
    int status = 0;
    if (g_mkdir_with_parents(user_autostart, 0700) != 0 ||
        g_mkdir_with_parents(system_autostart, 0700) != 0 ||
        !g_setenv("XDG_CONFIG_HOME", user, TRUE) ||
        !g_setenv("XDG_CONFIG_DIRS", system, TRUE) ||
        !g_setenv("XDG_CURRENT_DESKTOP", "Cinnamon", TRUE) ||
        !write_desktop(system_autostart, "alpha.desktop", alpha) ||
        !write_desktop(system_autostart, "beta.desktop", beta) ||
        !write_desktop(user_autostart, "alpha.desktop", malformed)) {
        status = 2;
        goto cleanup;
    }

    /* A malformed *higher-priority* physical file still masks the system
     * entry. Parsing and directory precedence are different concerns. */
    if (!expect_entries(1U, false, false, false, true)) {
        status = 3;
        goto cleanup;
    }
    if (g_remove(user_alpha) != 0 ||
        !expect_entries(2U, true, false, true, true)) {
        status = 4;
        goto cleanup;
    }
    if (!write_desktop(user_autostart, "alpha.desktop", disabled) ||
        !expect_entries(2U, true, true, false, true)) {
        status = 5;
        goto cleanup;
    }

    if (lsm_startup_backend_set_enabled("../escape.desktop",
            system_beta, system_beta, false, &error) ||
        error == NULL || g_file_test(outside, G_FILE_TEST_EXISTS)) {
        status = 6;
        goto cleanup;
    }
    g_clear_error(&error);
    if (lsm_startup_backend_set_enabled("gamma.desktop",
            system_beta, system_beta, false, &error) || error == NULL) {
        status = 7;
        goto cleanup;
    }
    g_clear_error(&error);
    if (!lsm_startup_backend_set_enabled(
            "beta.desktop", system_beta, system_beta, false, &error) ||
        !g_file_test(user_beta, G_FILE_TEST_EXISTS) ||
        !expect_entries(2U, true, true, false, false)) {
        status = 8;
        goto cleanup;
    }

    /* TryExec follows executable-file lookup, including empty PATH segments
     * for the working directory. Directory search permission is insufficient
     * for an executable target. Never execute the fixtures. */
    char *saved_directory = g_get_current_dir();
    char *saved_path = g_strdup(g_getenv("PATH"));
    char *tool = g_build_filename(root, "lsm-tryexec-probe", NULL);
    char *folder = g_build_filename(root, "lsm-tryexec-folder", NULL);
    char *gamma = g_build_filename(system_autostart, "gamma.desktop", NULL);
    static const char gamma_file[] =
        "[Desktop Entry]\nType=Application\nName=TryExec Probe\n"
        "Exec=/bin/true\nTryExec=lsm-tryexec-probe\n";
    static const char gamma_directory[] =
        "[Desktop Entry]\nType=Application\nName=TryExec Directory\n"
        "Exec=/bin/true\nTryExec=lsm-tryexec-folder\n";

    bool tryexec_ok = saved_directory != NULL &&
        g_file_set_contents(tool, "#!/bin/sh\nexit 0\n", -1, NULL) &&
        g_chmod(tool, 0700) == 0 &&
        g_mkdir(folder, 0700) == 0 &&
        write_desktop(system_autostart, "gamma.desktop", gamma_file) &&
        g_chdir(root) == 0 &&
        g_setenv("PATH", "/nonexistent:", TRUE) &&
        expect_tryexec("gamma.desktop", true) &&
        g_setenv("PATH", "", TRUE) &&
        expect_tryexec("gamma.desktop", true) &&
        write_desktop(system_autostart, "gamma.desktop", gamma_directory) &&
        g_setenv("PATH", root, TRUE) &&
        expect_tryexec("gamma.desktop", false);

    if (saved_directory && g_chdir(saved_directory) != 0)
        tryexec_ok = false;
    if (saved_path) {
        if (!g_setenv("PATH", saved_path, TRUE)) tryexec_ok = false;
    } else {
        g_unsetenv("PATH");
    }
    (void)g_remove(gamma);
    (void)g_remove(tool);
    (void)g_rmdir(folder);
    g_free(gamma);
    g_free(folder);
    g_free(tool);
    g_free(saved_path);
    g_free(saved_directory);
    if (!tryexec_ok) {
        status = 9;
        goto cleanup;
    }

cleanup:
    if (status != 0)
        g_printerr("Startup XDG/override regression failed at phase %d: %s\n",
                   status, error ? error->message : "no error detail");
    g_clear_error(&error);
    (void)g_remove(user_alpha);
    (void)g_remove(user_beta);
    (void)g_remove(system_alpha);
    (void)g_remove(system_beta);
    (void)g_remove(outside);
    (void)g_rmdir(user_autostart);
    (void)g_rmdir(system_autostart);
    (void)g_rmdir(user);
    (void)g_rmdir(system);
    (void)g_rmdir(root);
    g_free(outside);
    g_free(user_alpha);
    g_free(user_beta);
    g_free(system_alpha);
    g_free(system_beta);
    g_free(user_autostart);
    g_free(system_autostart);
    g_free(user);
    g_free(system);
    g_free(root);
    if (status == 0) puts("XDG startup precedence and override safety passed.");
    return status;
}
