// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef INFILTRATOR_SYSTEM_MONITOR_SUITE_CASES_H
#define INFILTRATOR_SYSTEM_MONITOR_SUITE_CASES_H

int lsm_case_atomic_file(void);
int lsm_case_duration_format(void);
int lsm_case_common(void);
int lsm_case_project_info(void);
int lsm_case_backend(void);
int lsm_case_monitor_platform(void);
int lsm_case_bluetooth_battery(void);
int lsm_case_bluetooth_traffic(void);
int lsm_case_linux_capability(void);
int lsm_case_logitech_hidpp(void);
int lsm_case_wifi_metadata(void);
int lsm_case_cpu_accounting(void);
int lsm_case_disk_accounting(void);
int lsm_case_memory_accounting(void);
int lsm_case_pressure(void);
int lsm_case_cpu_direct(void);
int lsm_case_quality_policy(void);
int lsm_case_sample_history(void);
int lsm_case_gpu_metrics(void);
int lsm_case_performance_navigation(void);
int lsm_case_mountinfo(void);
int lsm_case_storage_metadata(void);
int lsm_case_filesystem_inventory(void);
int lsm_case_bundled_pci(void);
int lsm_case_smbios_memory(void);
int lsm_case_system_sources(void);
int lsm_case_process_model(void);
int lsm_case_process_grouping(void);
int lsm_case_process_gpu(void);
int lsm_case_process_inspection(void);
int lsm_case_process_management(void);
int lsm_case_efficiency(void);
int lsm_case_dbus_models(void);
int lsm_case_preferences(void);
int lsm_case_startup(void);
int lsm_case_ui_update(void);
int lsm_case_task_manager_layout(void);
int lsm_case_hardware_topology(void);
int lsm_case_intel_gpu(void);
int lsm_case_npu_telemetry(void);

#endif
