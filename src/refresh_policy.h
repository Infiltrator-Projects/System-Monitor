// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file refresh_policy.h
 * @brief Shared cadence plus System Monitor deferred-presentation policy.
 *
 * Generic monotonic interval policy is owned by Infiltratr Common. System
 * Monitor retains only the page-presentation decision that depends on its UI.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_REFRESH_POLICY_H
#define LINUX_SYSTEM_MONITOR_REFRESH_POLICY_H

#include <stdbool.h>
#include <infiltratr/timing.h>

/** Use Common's tested interval policy without a local trampoline function. */
#define lsm_refresh_interval_due infiltratr_interval_due

/**
 * Decide whether a dirty page model should be presented immediately.
 *
 * @param current_page Currently visible page index.
 * @param target_page Page owning the dirty model.
 * @param dirty true when a newer model has not yet been presented.
 * @return true only when the dirty model belongs to the visible page.
 */
bool lsm_refresh_page_should_present(unsigned current_page,
                                     unsigned target_page, bool dirty);

#endif
