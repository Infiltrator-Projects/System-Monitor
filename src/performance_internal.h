// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_internal.h
 * @brief Private interfaces shared by Performance presentation modules.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PERFORMANCE_INTERNAL_H
#define LINUX_SYSTEM_MONITOR_PERFORMANCE_INTERNAL_H

#include "app_internal.h"

/** Rebuild one GPU graph selector from current backend capabilities. */
void lsm_performance_populate_gpu_metric_selector(
    LsmGpuGraphSlot *slot, const LsmGpuInfo *gpu, LsmGpuMetric preferred);

#endif
