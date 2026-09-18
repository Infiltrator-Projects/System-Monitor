// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file refresh_policy.c
 * @brief System Monitor-specific deferred-presentation policy.
 *
 * Generic interval cadence is supplied directly by Infiltratr Common through
 * refresh_policy.h; this module retains only application presentation policy.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "refresh_policy.h"

bool lsm_refresh_page_should_present(unsigned current_page,
                                     unsigned target_page, bool dirty)
{
    return dirty && current_page == target_page;
}
