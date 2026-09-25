// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file backend_smoke.c
 * @brief Live native monitoring-backend smoke test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor.h"
#include "monitor_linux_internal.h"
#include "process_backend.h"
#include "system_sources.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>

#define LSM_BACKEND_SMOKE_STACK_BYTES (4U * 1024U * 1024U)

int __wrap_pthread_timedjoin_np(pthread_t thread, void **result,
                               const struct timespec *deadline);
int __wrap_pthread_detach(pthread_t thread);
int __real_pthread_detach(pthread_t thread);
void __wrap_lsm_sources_destroy(LsmSystemSources *sources);
void __real_lsm_sources_destroy(LsmSystemSources *sources);
size_t __wrap_lsm_sources_read_network_counters(
    LsmSystemSources *sources, LsmNetworkCounterRecord *records, size_t capacity);
size_t __real_lsm_sources_read_network_counters(
    LsmSystemSources *sources, LsmNetworkCounterRecord *records, size_t capacity);

static atomic_uint sources_destroyed;
static bool joined_timeout_thread;
static bool network_fixture;
static bool network_sample_available;
static uint64_t network_sample_bytes;

size_t __wrap_lsm_sources_read_network_counters(
    LsmSystemSources *sources, LsmNetworkCounterRecord *records, size_t capacity)
{
    if (!network_fixture)
        return __real_lsm_sources_read_network_counters(sources, records, capacity);
    if (!network_sample_available || capacity == 0U) return 0U;
    records[0] = (LsmNetworkCounterRecord){0};
    (void)snprintf(records[0].name, sizeof(records[0].name), "fixture0");
    records[0].rx_bytes = network_sample_bytes;
    records[0].tx_bytes = network_sample_bytes;
    return 1U;
}

static bool collector_recovery_valid(void)
{
    LsmMonitor *monitor = calloc(1U, sizeof(*monitor));
    LsmLinuxMonitorBackendState *state = calloc(1U, sizeof(*state));
    if (!monitor || !state) {
        free(monitor);
        free(state);
        return false;
    }
    monitor->backend_state = state;
    monitor->cpu.base_frequency_ghz = 3.0;
    lsm_cpu_memory_update(monitor, 1.0);
    const bool frequency_unavailable = monitor->cpu.frequency_ghz == 0.0 &&
        monitor->cpu.max_frequency_ghz == 0.0;
    monitor->net_count = state->network_count = 1U;
    (void)snprintf(monitor->nets[0].name, sizeof(monitor->nets[0].name), "fixture0");
    (void)snprintf(state->networks[0].name, sizeof(state->networks[0].name), "fixture0");
    (void)snprintf(monitor->nets[0].connection_state,
                   sizeof(monitor->nets[0].connection_state), "Up");
    monitor->nets[0].link_speed_mbps = 1000.0;
    network_fixture = network_sample_available = true;
    network_sample_bytes = 1000U;
    lsm_storage_update(monitor, 1.0, false);
    network_sample_bytes = 2000U;
    lsm_storage_update(monitor, 1.0, false);
    bool okay = frequency_unavailable &&
        monitor->nets[0].rx_bytes_per_sec == 1000.0;
    network_sample_available = false;
    lsm_storage_update(monitor, 1.0, false);
    okay = okay && !state->networks[0].initialized &&
        !monitor->nets[0].utilisation_available &&
        monitor->nets[0].rx_bytes_per_sec == 0.0;
    network_sample_available = true;
    network_sample_bytes = 5000U;
    lsm_storage_update(monitor, 1.0, false);
    okay = okay && monitor->nets[0].rx_bytes_per_sec == 0.0;
    network_sample_bytes = 6000U;
    lsm_storage_update(monitor, 1.0, false);
    okay = okay && monitor->nets[0].rx_bytes_per_sec == 1000.0 &&
        monitor->nets[0].tx_bytes_per_sec == 1000.0;
    network_fixture = false;
    free(state);
    free(monitor);
    return okay;
}

/* Force the timeout/worker-exit ordering deterministically. Joining here
 * establishes that the worker has already released its reference before the
 * caller sees ETIMEDOUT. The paired detach wrapper avoids detaching a joined
 * pthread handle; production still uses the real timed join and detach. */
int __wrap_pthread_timedjoin_np(pthread_t thread, void **result,
                               const struct timespec *deadline)
{
    (void)deadline;
    const int error = pthread_join(thread, result);
    if (error != 0) return error;
    joined_timeout_thread = true;
    return ETIMEDOUT;
}

int __wrap_pthread_detach(pthread_t thread)
{
    if (joined_timeout_thread) {
        joined_timeout_thread = false;
        return 0;
    }
    return __real_pthread_detach(thread);
}

void __wrap_lsm_sources_destroy(LsmSystemSources *sources)
{
    if (sources) (void)atomic_fetch_add(&sources_destroyed, 1U);
    __real_lsm_sources_destroy(sources);
}

static bool constrain_main_stack(void)
{
    struct rlimit limit;
    if (getrlimit(RLIMIT_STACK, &limit) != 0) return false;
    const rlim_t requested = (rlim_t)LSM_BACKEND_SMOKE_STACK_BYTES;
    if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > requested) {
        limit.rlim_cur = requested;
        if (setrlimit(RLIMIT_STACK, &limit) != 0) return false;
    }
    return true;
}

int main(void)
{
    if (!collector_recovery_valid()) {
        fputs("collector recovery or frequency availability contract failed\n", stderr);
        return 1;
    }
    if (!constrain_main_stack()) {
        fputs("unable to constrain backend-smoke stack\n", stderr);
        return 2;
    }

    /* LsmMonitor intentionally contains bounded in-place device arrays and is
     * therefore a large value type. Keep the test model on the heap so this
     * smoke test also detects accidental multi-megabyte stack temporaries in
     * monitor update paths under a realistic constrained stack. */
    LsmMonitor *monitor = calloc(1U, sizeof(*monitor));
    if (!monitor) return 2;
    if (!lsm_monitor_init(monitor)) {
        fputs("monitor initialisation failed\n", stderr);
        free(monitor);
        return 1;
    }
    usleep(250000);
    if (!lsm_monitor_update(monitor)) {
        fputs("monitor update failed\n", stderr);
        lsm_monitor_destroy(monitor);
        free(monitor);
        return 1;
    }

    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) {
        lsm_monitor_destroy(monitor);
        free(monitor);
        return 1;
    }
    LsmProcessInfo *processes = NULL;
    size_t process_count = lsm_process_scan(
        backend, &processes, LSM_PROCESS_SCAN_NONE);
    uint64_t expected_threads = 0U;
    for (size_t index = 0U; index < process_count; index++)
        expected_threads += processes[index].threads;
    lsm_monitor_set_process_totals(monitor, processes, process_count);

    bool process_totals_valid = monitor->cpu.process_count == process_count
        && monitor->cpu.thread_count == expected_threads;
    printf("CPU: %.1f%%, logical processors: %u\n",
           monitor->cpu.usage_percent, monitor->cpu.logical_cores);
    printf("Memory: %.1f%%, disks: %zu, networks: %zu, GPUs: %zu\n",
           monitor->memory.usage_percent, monitor->disk_count,
           monitor->net_count, monitor->gpu_count);
    printf("Processes visible: %zu, threads: %u\n",
           process_count, monitor->cpu.thread_count);

    bool monitor_valid = monitor->cpu.logical_cores > 0
        && monitor->memory.total_bytes > 0 && process_totals_valid;

    lsm_process_list_free(processes);
    lsm_process_backend_destroy(backend);
    lsm_monitor_destroy(monitor);
    if (atomic_load(&sources_destroyed) != 1U || monitor->backend_state) {
        fputs("sampler timeout/exit race leaked native source ownership\n", stderr);
        monitor_valid = false;
    }
    free(monitor);
    return monitor_valid ? 0 : 1;
}
