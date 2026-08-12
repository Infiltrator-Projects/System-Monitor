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
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
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
    if (!lsm_process_set_efficiency(child, true)) {
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
