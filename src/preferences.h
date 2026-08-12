// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file preferences.h
 * @brief Persistent graphical preferences and Preferences dialog.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PREFERENCES_H
#define LINUX_SYSTEM_MONITOR_PREFERENCES_H

#include "app.h"

/**
 * Load saved preferences while preserving compiled defaults for missing keys.
 *
 * Unknown keys and malformed values are ignored so a newer configuration file
 * remains safe when read by an older binary.
 *
 * @param [in,out] app Application receiving persisted values.
 */
void lsm_preferences_load(LsmApp *app);

/**
 * Atomically persist the current user-facing preferences.
 *
 * @param [in] app Application whose current preferences are written.
 */
void lsm_preferences_save(const LsmApp *app);

/**
 * Present the modal, entirely graphical Preferences dialog.
 *
 * @param [in,out] app Application receiving accepted changes.
 */
void lsm_preferences_show(LsmApp *app);

#endif
