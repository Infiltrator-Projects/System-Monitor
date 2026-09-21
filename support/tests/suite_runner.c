// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file suite_runner.c
 * @brief Shared runner for coherent System Monitor regression suites.
 *
 * Individual *_smoke.c files remain focused regression cases. Their main
 * entry points are renamed at compile time and collected here by subsystem,
 * avoiding dozens of tiny executables while preserving the original checks.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stddef.h>
#include <stdio.h>

typedef int (*LsmSmokeCaseFunction)(void);

typedef struct {
    const char *name;
    LsmSmokeCaseFunction function;
} LsmSmokeCase;

#define LSM_CASE(symbol) { #symbol, smoke_case_##symbol }

#if defined(LSM_SUITE_CORE)
int smoke_case_atomic_file(void);
int smoke_case_duration_format(void);
int smoke_case_common(void);
int smoke_case_project_info(void);
static const char suite_name[] = "core";
static const LsmSmokeCase suite_cases[] = {
    LSM_CASE(atomic_file),
    LSM_CASE(duration_format),
    LSM_CASE(common),
    LSM_CASE(project_info),
};
#elif defined(LSM_SUITE_PERIPHERAL)
int smoke_case_bluetooth_battery(void);
int smoke_case_bluetooth_traffic(void);
int smoke_case_linux_capability(void);
int smoke_case_logitech_hidpp(void);
int smoke_case_wifi_metadata(void);
static const char suite_name[] = "peripheral";
static const LsmSmokeCase suite_cases[] = {
    LSM_CASE(bluetooth_battery),
    LSM_CASE(bluetooth_traffic),
    LSM_CASE(linux_capability),
    LSM_CASE(logitech_hidpp),
    LSM_CASE(wifi_metadata),
};
#elif defined(LSM_SUITE_METRICS)
int smoke_case_cpu_accounting(void);
int smoke_case_disk_accounting(void);
int smoke_case_memory_accounting(void);
int smoke_case_pressure(void);
int smoke_case_cpu_direct(void);
int smoke_case_quality_policy(void);
int smoke_case_sample_history(void);
int smoke_case_gpu_metrics(void);
int smoke_case_performance_navigation(void);
static const char suite_name[] = "metrics";
static const LsmSmokeCase suite_cases[] = {
    LSM_CASE(cpu_accounting),
    LSM_CASE(disk_accounting),
    LSM_CASE(memory_accounting),
    LSM_CASE(pressure),
    LSM_CASE(cpu_direct),
    LSM_CASE(quality_policy),
    LSM_CASE(sample_history),
    LSM_CASE(gpu_metrics),
    LSM_CASE(performance_navigation),
};
#elif defined(LSM_SUITE_STORAGE)
int smoke_case_mountinfo(void);
int smoke_case_storage_metadata(void);
int smoke_case_filesystem_inventory(void);
int smoke_case_bundled_pci(void);
int smoke_case_smbios_memory(void);
int smoke_case_system_sources(void);
static const char suite_name[] = "storage";
static const LsmSmokeCase suite_cases[] = {
    LSM_CASE(mountinfo),
    LSM_CASE(storage_metadata),
    LSM_CASE(filesystem_inventory),
    LSM_CASE(bundled_pci),
    LSM_CASE(smbios_memory),
    LSM_CASE(system_sources),
};
#elif defined(LSM_SUITE_PROCESS)
int smoke_case_process_model(void);
int smoke_case_process_grouping(void);
int smoke_case_process_gpu(void);
int smoke_case_process_inspection(void);
int smoke_case_process_management(void);
int smoke_case_efficiency(void);
static const char suite_name[] = "process";
static const LsmSmokeCase suite_cases[] = {
    LSM_CASE(process_model),
    LSM_CASE(process_grouping),
    LSM_CASE(process_gpu),
    LSM_CASE(process_inspection),
    LSM_CASE(process_management),
    LSM_CASE(efficiency),
};
#elif defined(LSM_SUITE_UI)
int smoke_case_dbus_models(void);
int smoke_case_preferences(void);
int smoke_case_startup(void);
int smoke_case_ui_update(void);
int smoke_case_task_manager_layout(void);
static const char suite_name[] = "UI/preferences";
static const LsmSmokeCase suite_cases[] = {
    LSM_CASE(dbus_models),
    LSM_CASE(preferences),
    LSM_CASE(startup),
    LSM_CASE(ui_update),
    LSM_CASE(task_manager_layout),
};
#elif defined(LSM_SUITE_ACCELERATOR)
int smoke_case_hardware_topology(void);
int smoke_case_intel_gpu(void);
int smoke_case_npu_telemetry(void);
static const char suite_name[] = "accelerator";
static const LsmSmokeCase suite_cases[] = {
    LSM_CASE(hardware_topology),
    LSM_CASE(intel_gpu),
    LSM_CASE(npu_telemetry),
};
#else
#error "A smoke suite selection must be defined"
#endif

int main(void)
{
    const size_t count = sizeof(suite_cases) / sizeof(suite_cases[0]);
    for (size_t index = 0U; index < count; index++) {
        const int status = suite_cases[index].function();
        if (status != 0) {
            fprintf(stderr, "%s smoke suite: %s failed with status %d\n",
                    suite_name, suite_cases[index].name, status);
            return status;
        }
    }
    printf("%s smoke suite passed (%zu cases).\n", suite_name, count);
    return 0;
}
