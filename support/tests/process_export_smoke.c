// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_export_smoke.c
 * @brief Selected-row spreadsheet escaping and group-selection regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "app_internal.h"
#include "process_export.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    LsmApp app;
    memset(&app, 0, sizeof(app));
    LsmProcessInfo processes[3];
    memset(processes, 0, sizeof(processes));
    processes[0].pid = 10;
    processes[0].instance_id = 100U;
    strcpy(processes[0].name, "one, process");
    strcpy(processes[0].user, "tester");
    strcpy(processes[0].gpu_engine, "render");
    strcpy(processes[0].command, "one \"quoted\" command");
    processes[1].pid = 11;
    processes[1].instance_id = 101U;
    strcpy(processes[1].name, "=two");
    strcpy(processes[1].command, "@SUM(1,1)\nline");
    processes[2].pid = 12;
    processes[2].instance_id = 102U;
    strcpy(processes[2].name, "not selected");
    app.process.process_snapshot = processes;
    app.process.process_snapshot_count = 3U;
    app.process.selected_pid = 10;
    app.process.selected_instance_id = 100U;
    LsmProcessId group[] = {10, 11};
    LsmProcessInstanceId group_instances[] = {100U, 101U};
    app.process.selected_group_pids = group;
    app.process.selected_group_instance_ids = group_instances;
    app.process.selected_group_count = 2U;

    char path[] = "/tmp/lsm-process-export-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    close(descriptor);
    unlink(path);
    char error[256];
    assert(lsm_process_export_selected_csv(&app, path, error, sizeof(error)));
    struct stat status;
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0600);
    FILE *file = fopen(path, "r");
    assert(file);
    char contents[8192];
    const size_t length = fread(contents, 1U, sizeof(contents) - 1U, file);
    contents[length] = '\0';
    fclose(file);
    assert(strstr(contents, "\"one, process\""));
    assert(strstr(contents, "one \"\"quoted\"\" command"));
    assert(strstr(contents, "\"'=two\",11"));
    assert(strstr(contents, "\"'@SUM(1,1)line\""));
    assert(!strstr(contents, "not selected"));
    assert(!strstr(contents, "K" "iB"));

    unlink(path);
    group_instances[1] = 999U;
    assert(lsm_process_export_selected_csv(&app, path, error, sizeof(error)));
    file = fopen(path, "r");
    assert(file);
    const size_t stale_length = fread(
        contents, 1U, sizeof(contents) - 1U, file);
    contents[stale_length] = '\0';
    fclose(file);
    assert(strstr(contents, "\"one, process\""));
    assert(!strstr(contents, "\"'=two\",11"));
    unlink(path);
    puts("Selected process CSV identity, group export and spreadsheet escaping passed.");
    return 0;
}
