// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_theme.h
 * @brief Native Windows projection of the shared Infiltratr design contract.
 *
 * Win32 theme persistence and Common palette projection are deliberately owned
 * here so the native shell does not also own design-contract interpretation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_WINDOWS_THEME_H
#define INFILTRATOR_SYSTEM_MONITOR_WINDOWS_THEME_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <infiltratr/design.h>

#include <stdbool.h>

/** Native Win32 colours projected from the shared Infiltratr theme palette. */
typedef struct {
    COLORREF background;
    COLORREF panel;
    COLORREF card;
    COLORREF surface;
    COLORREF input;
    COLORREF border;
    COLORREF text;
    COLORREF title;
    COLORREF subtle;
    COLORREF selection;
    COLORREF accent;
    COLORREF selected_summary;
    COLORREF heading;
    COLORREF summary;
    COLORREF detail;
    COLORREF connection;
    COLORREF connection_border;
    COLORREF card_hover;
} LsmWindowsPalette;

/** Win32 theme mode, validated Common design metrics and resolved palette. */
typedef struct {
    InfiltratrThemeMode mode;
    const InfiltratrDesignMetrics *design;
    LsmWindowsPalette palette;
} LsmWindowsTheme;

/**
 * Validate the Common design ABI before native presentation consumes it.
 * @param [in] design Common design metrics to validate.
 * @return true when the metrics satisfy the native presentation contract.
 */
bool lsm_windows_theme_design_supported(const InfiltratrDesignMetrics *design);
/**
 * Resolve the current theme mode into a native Win32 palette.
 * @param [in,out] theme Theme state receiving the resolved palette.
 * @return true when the Common palette contract is supported.
 */
bool lsm_windows_theme_resolve(LsmWindowsTheme *theme);
/**
 * Load the persisted native theme preference and resolve its palette.
 * @param [in,out] theme Theme state receiving the persisted mode and palette.
 * @return true when the resulting Common palette contract is supported.
 */
bool lsm_windows_theme_load(LsmWindowsTheme *theme);
/**
 * Persist the selected native theme mode.
 * @param [in] theme Theme state whose selected mode is saved.
 */
void lsm_windows_theme_save(const LsmWindowsTheme *theme);
/**
 * Return whether the effective theme should use dark native chrome.
 * @param [in] theme Current native theme state.
 * @return true when dark native chrome should be used.
 */
bool lsm_windows_theme_is_dark(const LsmWindowsTheme *theme);

#endif
