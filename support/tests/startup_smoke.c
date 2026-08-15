// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file startup_smoke.c
 * @brief XDG startup parsing and reversible override test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "../src/startup.c"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>

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

    StartupEntry entry;
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
