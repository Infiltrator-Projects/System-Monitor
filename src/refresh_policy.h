// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file refresh_policy.h
 * @brief Pure cadence and deferred-presentation policy.
 *
 * These functions contain no clock access and no GTK state. Callers supply
 * their current state, making edge cases deterministic and independently
 * testable. A missing or invalid time baseline intentionally means "due now" so
 * recovery cannot leave a category permanently frozen.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_REFRESH_POLICY_H
#define LINUX_SYSTEM_MONITOR_REFRESH_POLICY_H

#include <stdbool.h>

/**
 * Decide whether an interval has elapsed.
 *
 * @param now Current monotonic time in seconds.
 * @param last Monotonic time of the previous successful refresh.
 * @param interval Required interval in seconds.
 * @return true when due or when any input cannot form a safe baseline.
 */
bool lsm_refresh_interval_due(double now, double last, double interval);

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
