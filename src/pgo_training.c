// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pgo_training.c
 * @brief Headless target-machine workload for profile-guided optimisation.
 *
 * The aggressive native installer first builds an instrumented System Monitor,
 * invokes this workload, then rebuilds from the resulting compiler profile.
 * The workload deliberately exercises production monitor/process contracts
 * instead of a synthetic arithmetic benchmark, while remaining display-free
 * and non-destructive.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "pgo_training.h"

#include "monitor.h"
#include "process_backend.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void pgo_training_delay(void)
{
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 75000000L
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

int lsm_pgo_train(void)
{
    bool trained = false;

    LsmMonitor *monitor = calloc(1U, sizeof(*monitor));
    if (monitor && lsm_monitor_init(monitor)) {
        trained = true;
        for (unsigned iteration = 0U; iteration < 12U; iteration++) {
            if (iteration == 6U)
                lsm_monitor_request_topology_refresh(monitor);
            if (lsm_monitor_update(monitor))
                trained = true;
            pgo_training_delay();
        }
        lsm_monitor_destroy(monitor);
    }
    free(monitor);

    LsmProcessBackend *backend = lsm_process_backend_create();
    if (backend) {
        const unsigned scan_flags =
            LSM_PROCESS_SCAN_EXECUTABLE |
            LSM_PROCESS_SCAN_HANDLE_COUNT |
            LSM_PROCESS_SCAN_GPU |
            LSM_PROCESS_SCAN_CGROUP;
        for (unsigned iteration = 0U; iteration < 6U; iteration++) {
            LsmProcessInfo *processes = NULL;
            const unsigned flags = iteration < 2U ? LSM_PROCESS_SCAN_NONE
                                                  : scan_flags;
            const size_t count =
                lsm_process_scan(backend, &processes, flags);
            if (count > 0U)
                trained = true;
            lsm_process_list_free(processes);
            pgo_training_delay();
        }
        lsm_process_backend_destroy(backend);
    }

    if (!trained) {
        fputs("PGO training could not exercise native monitoring paths.\n",
              stderr);
        return EXIT_FAILURE;
    }

    puts("PGO training workload completed.");
    return EXIT_SUCCESS;
}
