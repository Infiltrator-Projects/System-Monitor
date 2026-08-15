// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_management_smoke.c
 * @brief Process detail, accounting and affinity smoke test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static bool process_is_stopped(pid_t pid)
{
    char path[64];
    if (snprintf(path, sizeof(path), "/proc/%d/stat", pid) < 0) return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char text[1024];
    const bool read_ok = fgets(text, sizeof(text), file) != NULL;
    fclose(file);
    if (!read_ok) return false;
    const char *right = strrchr(text, ')');
    return right && right[1] == ' ' && (right[2] == 'T' || right[2] == 't');
}

static bool procfs_uses_current_pid_namespace(void)
{
    char target[64];
    const ssize_t length = readlink("/proc/self", target, sizeof(target) - 1U);
    if (length <= 0 || (size_t)length >= sizeof(target)) return true;
    target[(size_t)length] = '\0';

    char *end = NULL;
    errno = 0;
    const long procfs_pid = strtol(target, &end, 10);
    if (errno != 0 || !end || *end != '\0' || procfs_pid <= 0)
        return true;
    return procfs_pid == (long)getpid();
}

static bool wait_for_stopped(pid_t pid)
{
    for (unsigned attempt = 0U; attempt < 100U; attempt++) {
        if (process_is_stopped(pid)) return true;
        usleep(10000U);
    }
    return false;
}

static bool capture_identity(pid_t pid, LsmProcessInstanceId *instance_id)
{
    if (!instance_id) return false;
    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) return false;
    LsmProcessInfo *processes = NULL;
    const size_t count = lsm_process_scan(
        backend, &processes, LSM_PROCESS_SCAN_NONE);
    bool found = false;
    for (size_t index = 0U; index < count; index++) {
        if (processes[index].pid == (LsmProcessId)pid &&
            processes[index].instance_id != 0U) {
            *instance_id = processes[index].instance_id;
            found = true;
            break;
        }
    }
    lsm_process_list_free(processes);
    lsm_process_backend_destroy(backend);
    return found;
}

static bool exercise_process_tree_control(void)
{
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) return false;

    pid_t child = fork();
    if (child < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return false;
    }
    if (child == 0) {
        close(pipe_fds[0]);
        pid_t grandchild = fork();
        if (grandchild < 0) _exit(2);
        if (grandchild == 0) {
            close(pipe_fds[1]);
            for (;;) pause();
        }
        const ssize_t written = write(pipe_fds[1], &grandchild,
                                      sizeof(grandchild));
        close(pipe_fds[1]);
        if (written != (ssize_t)sizeof(grandchild)) _exit(3);
        for (;;) pause();
    }

    close(pipe_fds[1]);
    pid_t grandchild = -1;
    const ssize_t received = read(pipe_fds[0], &grandchild,
                                  sizeof(grandchild));
    close(pipe_fds[0]);
    if (received != (ssize_t)sizeof(grandchild) || grandchild <= 1) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return false;
    }

    usleep(50000U);
    LsmProcessInstanceId child_instance_id = 0U;
    if (!capture_identity(child, &child_instance_id)) {
        (void)kill(child, SIGKILL);
        (void)kill(grandchild, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return false;
    }
    errno = 0;
    if (lsm_process_control_tree(
            (LsmProcessId)child, child_instance_id + 1U,
            LSM_PROCESS_CONTROL_SUSPEND) || errno != ESRCH) {
        (void)kill(child, SIGKILL);
        (void)kill(grandchild, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return false;
    }
    const bool signalled = lsm_process_control_tree(
        (LsmProcessId)child, child_instance_id,
        LSM_PROCESS_CONTROL_SUSPEND);
    const bool child_stopped = signalled && wait_for_stopped(child);
    const bool grandchild_stopped = signalled && wait_for_stopped(grandchild);

    (void)lsm_process_control_tree(
        (LsmProcessId)child, child_instance_id,
        LSM_PROCESS_CONTROL_FORCE_TERMINATE);
    (void)kill(grandchild, SIGKILL);
    (void)waitpid(child, NULL, 0);
    return child_stopped && grandchild_stopped;
}

int main(void)
{
    /*
     * Some build sandboxes virtualise getpid() without mounting the matching
     * procfs namespace. The application is not run in that arrangement, and
     * process-control assertions cannot identify or signal their own children
     * there. Report an explicit skip instead of misdiagnosing the backend.
     */
    if (!procfs_uses_current_pid_namespace()) {
        puts("SKIP: procfs PID namespace does not match getpid().");
        return 0;
    }

    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) return 1;
    LsmProcessInfo *processes = NULL;
    size_t count = lsm_process_scan(backend, &processes,
        LSM_PROCESS_SCAN_EXECUTABLE | LSM_PROCESS_SCAN_HANDLE_COUNT);
    if (!count || !processes) {
        fputs("process scan returned no rows\n", stderr);
        return 1;
    }

    pid_t self = getpid();
    const LsmProcessInfo *found = NULL;
    for (size_t i = 0; i < count; i++)
        if (processes[i].pid == (LsmProcessId)self) { found = &processes[i]; break; }
    if (!found) {
        fputs("current process was not found\n", stderr);
        return 1;
    }
    if (found->ppid <= 0 || found->threads == 0 || found->instance_id == 0 ||
        found->elapsed_seconds > 86400ULL || !found->command[0]) {
        fputs("current process details were incomplete\n", stderr);
        return 1;
    }
    if (!lsm_process_identity_matches(found->pid, found->instance_id) ||
        lsm_process_identity_matches(found->pid, found->instance_id + 1U)) {
        fputs("process instance validation failed\n", stderr);
        return 1;
    }

    bool cpus[LSM_PROCESS_MAX_CPUS] = {0};
    size_t cpu_count = lsm_process_affinity_get(
        (LsmProcessId)self, found->instance_id,
        cpus, LSM_PROCESS_MAX_CPUS);
    if (!cpu_count) {
        fputs("affinity query failed\n", stderr);
        return 1;
    }

    printf("PID %llu, parent %llu, threads %u, handles %u, CPUs %zu\n",
           (unsigned long long)found->pid,
           (unsigned long long)found->ppid,
           found->threads, found->handle_count, cpu_count);
    lsm_process_list_free(processes);
    lsm_process_backend_destroy(backend);

    if (!exercise_process_tree_control()) {
        fputs("process-tree control did not reach every descendant\n", stderr);
        return 1;
    }
    puts("Process-tree control passed.");
    return 0;
}
