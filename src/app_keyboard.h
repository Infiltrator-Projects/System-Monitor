// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_keyboard.h
 * @brief Top-level keyboard policy for shell and page actions.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_KEYBOARD_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_KEYBOARD_H

#include "app.h"

/** Connect the single page-aware key handler to the application window. */
void lsm_app_keyboard_connect(LsmApp *app);

#endif
