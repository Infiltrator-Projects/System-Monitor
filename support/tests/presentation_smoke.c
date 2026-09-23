// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file presentation_smoke.c
 * @brief Deterministic checks for the shared cross-platform presentation model.
 *
 * The native GTK and Win32 renderers must consume the same product labels,
 * field placement and formatted CPU/memory values. These checks intentionally
 * avoid either toolkit so architectural drift is caught at the shared boundary.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_view.h"
#include "presentation_contract.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void check_contract_identity(void)
{
    assert(strcmp(lsm_tab_label(LSM_TAB_PERFORMANCE), "Performance") == 0);
    assert(strcmp(lsm_tab_label(LSM_TAB_FILESYSTEMS), "File Systems") == 0);
    assert(strcmp(lsm_performance_page_title(LSM_PAGE_CPU), "CPU") == 0);
    assert(strcmp(lsm_performance_page_title(LSM_PAGE_MEMORY), "Memory") == 0);
    assert(strcmp(lsm_performance_colour_hex(LSM_PAGE_CPU), "#00adef") == 0);

    const LsmPresentationGridPosition user =
        lsm_cpu_metric_position(LSM_CPU_METRIC_USER);
    assert(user.column == 0U);
    assert(user.row == 4U);

    const LsmPresentationGridPosition switches =
        lsm_cpu_detail_position(LSM_CPU_DETAIL_CONTEXT_SWITCHES);
    assert(switches.column == 1U);
    assert(switches.row == 5U);

    const LsmPresentationGridPosition pressure =
        lsm_memory_metric_position(LSM_MEMORY_METRIC_PRESSURE);
    assert(pressure.column == 1U);
    assert(pressure.row == 4U);

    const LsmPresentationGridPosition modules =
        lsm_memory_detail_position(LSM_MEMORY_DETAIL_INSTALLED_MODULES);
    assert(modules.column == 0U);
    assert(modules.row == 4U);
}

static void check_cpu_availability_semantics(void)
{
    LsmMonitor monitor;
    memset(&monitor, 0, sizeof(monitor));
    (void)snprintf(
        monitor.cpu.model, sizeof(monitor.cpu.model), "Test CPU");
    monitor.cpu.usage_percent = 25.0;
    monitor.cpu.frequency_ghz = 2.5;
    monitor.cpu.physical_cores = 4U;
    monitor.cpu.logical_cores = 8U;
    monitor.cpu.virtualization_available = true;
    monitor.cpu.virtualization = false;
    monitor.cpu.load_average_available = true;
    monitor.cpu.interrupts_per_sec_available = true;
    monitor.cpu.interrupts_per_sec = 0.0;
    monitor.cpu.context_switches_per_sec_available = false;
    monitor.cpu.uptime_seconds = UINT64_C(3661);

    LsmCpuPerformanceView view;
    lsm_cpu_performance_view(&monitor, &view);

    assert(strcmp(view.subtitle, "Test CPU") == 0);
    assert(strcmp(view.rail_value, "25% 2.50 GHz") == 0);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_VIRTUALISATION], "Disabled") == 0);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_LOAD_AVERAGE],
        "0.00  0.00  0.00") == 0);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_INTERRUPTS], "0") == 0);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_CONTEXT_SWITCHES], "N/A") == 0);
    assert(strcmp(
        view.metrics[LSM_CPU_METRIC_TEMPERATURE], "N/A") == 0);

    monitor.cpu.virtualization_available = false;
    lsm_cpu_performance_view(&monitor, &view);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_VIRTUALISATION], "N/A") == 0);
}

static void check_memory_projection(void)
{
    LsmMonitor monitor;
    memset(&monitor, 0, sizeof(monitor));
    monitor.memory.total_bytes = UINT64_C(16) * 1024U * 1024U * 1024U;
    monitor.memory.used_bytes = UINT64_C(8) * 1024U * 1024U * 1024U;
    monitor.memory.available_bytes =
        UINT64_C(8) * 1024U * 1024U * 1024U;
    monitor.memory.usage_percent = 50.0;

    LsmMemoryPerformanceView view;
    lsm_memory_performance_view(&monitor, &view);

    assert(strcmp(view.subtitle, "16.0 GB") == 0);
    assert(strstr(view.rail_value, "50%") != NULL);
    assert(strcmp(
        view.metrics[LSM_MEMORY_METRIC_IN_USE], "8.0 GB") == 0);
    assert(strcmp(
        view.metrics[LSM_MEMORY_METRIC_AVAILABLE], "8.0 GB") == 0);
    assert(strcmp(
        view.details[LSM_MEMORY_DETAIL_SPEED], "N/A") == 0);
    assert(strcmp(
        view.details[LSM_MEMORY_DETAIL_SLOTS_USED], "N/A") == 0);
    assert(strcmp(
        view.details[LSM_MEMORY_DETAIL_INSTALLED_MODULES], "N/A") == 0);
}

int main(void)
{
    check_contract_identity();
    check_cpu_availability_semantics();
    check_memory_projection();
    puts("Shared presentation contract smoke passed.");
    return 0;
}
