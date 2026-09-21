// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file suite_cases.h
 * @brief Internal entry points for consolidated regression-suite cases.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_SUITE_CASES_H
#define INFILTRATOR_SYSTEM_MONITOR_SUITE_CASES_H

int smoke_case_atomic_file(void);
int smoke_case_duration_format(void);
int smoke_case_common(void);
int smoke_case_project_info(void);
int smoke_case_backend(void);
int smoke_case_monitor_platform(void);
int smoke_case_bluetooth_battery(void);
int smoke_case_bluetooth_traffic(void);
int smoke_case_linux_capability(void);
int smoke_case_logitech_hidpp(void);
int smoke_case_wifi_metadata(void);
int smoke_case_cpu_accounting(void);
int smoke_case_disk_accounting(void);
int smoke_case_memory_accounting(void);
int smoke_case_pressure(void);
int smoke_case_cpu_direct(void);
int smoke_case_quality_policy(void);
int smoke_case_sample_history(void);
int smoke_case_gpu_metrics(void);
int smoke_case_performance_navigation(void);
int smoke_case_mountinfo(void);
int smoke_case_storage_metadata(void);
int smoke_case_filesystem_inventory(void);
int smoke_case_bundled_pci(void);
int smoke_case_smbios_memory(void);
int smoke_case_system_sources(void);
int smoke_case_process_model(void);
int smoke_case_process_grouping(void);
int smoke_case_process_gpu(void);
int smoke_case_process_inspection(void);
int smoke_case_process_management(void);
int smoke_case_efficiency(void);
int smoke_case_dbus_models(void);
int smoke_case_preferences(void);
int smoke_case_startup(void);
int smoke_case_ui_update(void);
int smoke_case_task_manager_layout(void);
int smoke_case_hardware_topology(void);
int smoke_case_intel_gpu(void);
int smoke_case_npu_telemetry(void);

#endif
