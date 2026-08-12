// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ui_update_smoke.c
 * @brief Verify unchanged labels are suppressed before entering GTK.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "ui_helpers.h"

#include <stdio.h>

int main(void)
{
    if (lsm_ui_text_needs_update("42%", "42%")) return 1;
    if (!lsm_ui_text_needs_update("42%", "43%")) return 2;
    if (lsm_ui_text_needs_update(NULL, "")) return 3;
    if (!lsm_ui_text_needs_update(NULL, "N/A")) return 4;
    puts("Unchanged GTK label suppression policy passed.");
    return 0;
}
