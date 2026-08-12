// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file refresh_policy.c
 * @brief Pure refresh-policy implementation shared by backend and UI code.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "refresh_policy.h"

#include <math.h>

bool lsm_refresh_interval_due(double now, double last, double interval)
{
    return !isfinite(now) || !isfinite(last) || !isfinite(interval) ||
           last <= 0.0 || interval <= 0.0 || now < last ||
           now - last >= interval;
}

bool lsm_refresh_page_should_present(unsigned current_page,
                                     unsigned target_page, bool dirty)
{
    return dirty && current_page == target_page;
}
