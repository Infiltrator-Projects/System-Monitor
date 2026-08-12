// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file nvml.h
 * @brief Optional native NVIDIA Management Library adapter.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_NVML_H
#define LINUX_SYSTEM_MONITOR_NVML_H

#include "monitor_types.h"

/**
 * Refresh NVIDIA-only optional metrics through a dynamically loaded NVML ABI.
 *
 * Failure to load NVML or match a PCI identity leaves NVIDIA extension fields
 * unavailable without affecting generic DRM discovery.
 *
 * @param [in,out] monitor Snapshot containing enumerated graphics adapters.
 */
void lsm_nvml_refresh(LsmMonitor *monitor);
/** Release the optional NVML runtime and its driver context. */
void lsm_nvml_shutdown(void);

#endif
