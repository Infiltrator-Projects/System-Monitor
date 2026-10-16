// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file preferences.h
 * @brief Persistent graphical preferences and Preferences dialog.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_PREFERENCES_H
#define SYSTEM_MONITOR_PREFERENCES_H

#include "app.h"

#include <stdbool.h>

/** Editable preference values exposed to the graphical editor. */
typedef struct {
    guint update_interval_ms;
    guint filesystem_update_interval_ms;
    gboolean network_use_bits;
    gboolean network_total_separate;
    gboolean network_total_use_bits;
    gboolean process_cpu_per_core;
    gboolean newer_on_right;
    guint graph_data_points;
    gboolean graph_smooth;
    gboolean cpu_stacked;
    gboolean memory_logarithmic;
    gboolean confirm_process_actions;
    gboolean show_all_filesystems;
    gboolean process_heatmap;
    gboolean always_on_top;
    gboolean compact_summary;
} LsmPreferencesDialogValues;

/**
 * Initialise every persisted preference to its compiled product default.
 *
 * This is the single authority for preference defaults. The application
 * composition root owns lifecycle ordering only and then overlays persisted
 * values by calling lsm_preferences_load().
 *
 * @param [in,out] app Application receiving compiled preference defaults.
 */
void lsm_preferences_initialise_defaults(LsmApp *app);

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
 * Copy the values editable by the Preferences dialog without exposing LsmApp.
 *
 * @param [in] app Application owning the current preference state.
 * @param [out] values Destination receiving editable values.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_preferences_dialog_values(
    const LsmApp *app, LsmPreferencesDialogValues *values);

/**
 * Apply accepted dialog values as one preferences-domain transaction.
 *
 * This updates application state, synchronises dependent controls, persists the
 * accepted values and refreshes affected presentation models. Keeping these
 * side effects here prevents the GTK editor from depending on private LsmApp
 * storage owned by unrelated subsystems.
 *
 * @param [in,out] app Application receiving accepted preference values.
 * @param [in] values Accepted values to apply.
 */
void lsm_preferences_commit_dialog_values(
    LsmApp *app, const LsmPreferencesDialogValues *values);

/**
 * Present the modal, entirely graphical Preferences dialog.
 *
 * @param [in,out] app Application receiving accepted changes.
 */
void lsm_preferences_show(LsmApp *app);

#endif
