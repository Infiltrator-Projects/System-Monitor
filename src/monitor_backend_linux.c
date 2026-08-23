// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_backend_linux.c
 * @brief Linux implementation of the operating-system monitor backend contract.
 *
 * This file is the platform seam for the Performance monitoring lifecycle.
 * Linux sampling policy, procfs/sysfs source ownership and collector ordering
 * remain below this boundary. The application-facing monitor.c contains no
 * Linux collector knowledge and can be reused by another native backend.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_platform.h"

#include "common.h"
#include "monitor_linux_internal.h"
#include "refresh_policy.h"
#include "sampling_policy.h"
#include "system_sources.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

bool lsm_monitor_platform_init(LsmMonitor *monitor)
{
    if (!monitor) return false;
    memset(monitor, 0, sizeof(*monitor));

    LsmLinuxMonitorBackendState *state = calloc(1U, sizeof(*state));
    if (!state) return false;
    monitor->backend_state = state;

    if (!lsm_sources_init(&state->system_sources)) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    state->wifi_metadata = lsm_wifi_metadata_create();
    if (!state->wifi_metadata) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }

    if (!lsm_cpu_memory_initialise(monitor)) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    if (!lsm_storage_initialise(monitor)) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    lsm_hardware_initialise(monitor);

    const double now = lsm_monotonic_seconds();
    if (!isfinite(now) || now <= 0.0) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    state->last_update_monotonic = now;
    state->last_topology_scan_monotonic = now;
    state->last_battery_update_monotonic = now;
    return true;
}

bool lsm_monitor_platform_update(LsmMonitor *monitor)
{
    if (!monitor) return false;
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return false;

    const double now = lsm_monotonic_seconds();
    if (!isfinite(now) || now <= state->last_update_monotonic)
        return false;
    const double elapsed = now - state->last_update_monotonic;
    state->last_update_monotonic = now;

    const bool refresh_topology = state->topology_refresh_requested ||
        lsm_refresh_interval_due(
            now, state->last_topology_scan_monotonic,
            LSM_TOPOLOGY_SCAN_INTERVAL_SECONDS);
    const bool refresh_batteries = lsm_refresh_interval_due(
        now, state->last_battery_update_monotonic,
        LSM_BATTERY_UPDATE_INTERVAL_SECONDS);

    lsm_cpu_memory_update(monitor, elapsed);
    lsm_storage_update(monitor, elapsed, refresh_topology);
    lsm_hardware_update(monitor, elapsed, refresh_topology, refresh_batteries);

    if (refresh_topology) {
        state->last_topology_scan_monotonic = now;
        state->topology_refresh_requested = false;
    }
    if (refresh_batteries) state->last_battery_update_monotonic = now;
    return true;
}

void lsm_monitor_platform_request_topology_refresh(LsmMonitor *monitor)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (state) state->topology_refresh_requested = true;
}

void lsm_monitor_platform_destroy(LsmMonitor *monitor)
{
    if (!monitor) return;

    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    lsm_hardware_shutdown(monitor);
    lsm_cpu_memory_shutdown(monitor);
    if (state) {
        lsm_wifi_metadata_destroy(state->wifi_metadata);
        state->wifi_metadata = NULL;
        lsm_sources_destroy(state->system_sources);
        state->system_sources = NULL;
        free(state);
    }
    monitor->backend_state = NULL;
}
