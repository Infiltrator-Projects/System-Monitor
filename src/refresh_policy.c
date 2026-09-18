// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file refresh_policy.c
 * @brief Pure refresh-policy implementation shared by backend and UI code.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "refresh_policy.h"

bool lsm_refresh_page_should_present(unsigned current_page,
                                     unsigned target_page, bool dirty)
{
    return dirty && current_page == target_page;
}
