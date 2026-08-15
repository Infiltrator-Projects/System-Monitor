// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file efficiency_smoke.c
 * @brief Process Efficiency mode scheduler-control smoke test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

int main(void)
{
    if (!procfs_uses_current_pid_namespace()) {
        puts("SKIP: procfs PID namespace does not match getpid().");
        return 0;
    }

    const double total_percent =
        lsm_process_cpu_total_percent(100U, 800U);
    if (fabs(total_percent - 12.5) > 0.000001 ||
        fabs(total_percent * 8.0 - 100.0) > 0.000001) {
        fputs("process CPU normalisation is incorrect\n", stderr);
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        for (;;) pause();
    }

    usleep(50000);
    LsmProcessInstanceId child_instance_id = 0U;
    if (!capture_identity(child, &child_instance_id)) {
        fputs("child process identity was unavailable\n", stderr);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }
    errno = 0;
    if (lsm_process_set_efficiency(
            child, child_instance_id + 1U, true) || errno != ESRCH) {
        fputs("stale process identity was accepted\n", stderr);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }
    if (!lsm_process_set_efficiency(child, child_instance_id, true)) {
        perror("lsm_process_set_efficiency");
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }

    errno = 0;
    int priority = getpriority(PRIO_PROCESS, child);
    if (errno != 0 || priority < 10) {
        fprintf(stderr, "unexpected nice value: %d (errno=%d)\n", priority, errno);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }

    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    puts("Efficiency mode smoke test passed.");
    return 0;
}
