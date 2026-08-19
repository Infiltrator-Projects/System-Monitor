// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file preferences_smoke.c
 * @brief Preference round-trip and invalid-value fallback regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
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
    loaded->runtime.newer_on_right = true;
    loaded->runtime.window_width = 1280;
    loaded->runtime.window_height = 800;
    snprintf(loaded->paths.preferences_path, sizeof(loaded->paths.preferences_path),
             "%s", saved->paths.preferences_path);
    lsm_preferences_load(loaded);
    assert(loaded->runtime.update_interval_ms == 2000U);
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
    fputs("window_width=640\nwindow_height=420\n", file);
    assert(fclose(file) == 0);
    loaded->runtime.window_width = 1280;
    loaded->runtime.window_height = 800;
    lsm_preferences_load(loaded);
    assert(loaded->runtime.window_width == 640 && loaded->runtime.window_height == 420);

    file = fopen(saved->paths.preferences_path, "w");
    assert(file);
    fputs("update_interval_ms=7\nwindow_width=-1\npage_scroll_1=nan\n"
          "last_tab=999\n", file);
    assert(fclose(file) == 0);
    loaded->runtime.update_interval_ms = 1000U;
    loaded->runtime.window_width = 1280;
    loaded->runtime.page_scroll[LSM_TAB_PROCESSES] = 12.0;
    loaded->runtime.last_tab = LSM_TAB_PERFORMANCE;
    lsm_preferences_load(loaded);
    assert(loaded->runtime.update_interval_ms == 1000U);
    assert(loaded->runtime.window_width == 1280);
    assert(loaded->runtime.page_scroll[LSM_TAB_PROCESSES] == 12.0);
    assert(loaded->runtime.last_tab == LSM_TAB_PERFORMANCE);

    unlink(saved->paths.preferences_path);
    rmdir(directory);
    free(loaded);
    free(saved);
    puts("Preference round-trip, page scroll and invalid-value fallback passed.");
    return 0;
}
