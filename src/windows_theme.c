// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_theme.c
 * @brief Native Windows theme persistence and Common palette projection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "windows_theme.h"

#include <limits.h>
#include <stdint.h>

static COLORREF colourref_from_common(uint32_t rgb)
{
    return RGB(
        (BYTE)((rgb >> 16U) & 0xffU),
        (BYTE)((rgb >> 8U) & 0xffU),
        (BYTE)(rgb & 0xffU));
}

bool lsm_windows_theme_design_supported(const InfiltratrDesignMetrics *design)
{
    return infiltratr_design_metrics_is_valid(design) &&
           design->small_radius <= (uint32_t)INT_MAX &&
           design->control_radius <= (uint32_t)INT_MAX &&
           design->card_radius <= (uint32_t)INT_MAX &&
           design->compact_spacing <= (uint32_t)INT_MAX &&
           design->control_spacing <= (uint32_t)INT_MAX &&
           design->section_spacing <= (uint32_t)INT_MAX &&
           design->screen_padding <= (uint32_t)INT_MAX;
}

static bool system_prefers_dark(void)
{
    DWORD light_theme = 1U;
    DWORD size = sizeof(light_theme);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light_theme, &size);
    return status == ERROR_SUCCESS && light_theme == 0U;
}

static void project_palette(const InfiltratrThemePalette *source,
                            LsmWindowsPalette *destination)
{
    if (!source || !destination) return;
    destination->background = colourref_from_common(source->background_rgb);
    destination->panel = colourref_from_common(source->panel_rgb);
    destination->card = colourref_from_common(source->card_rgb);
    destination->surface = colourref_from_common(source->surface_rgb);
    destination->input = colourref_from_common(source->input_rgb);
    destination->border = colourref_from_common(source->border_rgb);
    destination->text = colourref_from_common(source->text_rgb);
    destination->title = colourref_from_common(source->title_rgb);
    destination->subtle = colourref_from_common(source->subtle_rgb);
    destination->selection =
        colourref_from_common(source->selection_background_rgb);
    destination->accent = colourref_from_common(source->neutral_accent_rgb);
    destination->selected_summary =
        colourref_from_common(source->selected_summary_rgb);
    destination->heading = colourref_from_common(source->heading_rgb);
    destination->summary = colourref_from_common(source->summary_rgb);
    destination->detail = colourref_from_common(source->detail_label_rgb);
    destination->connection = colourref_from_common(source->connection_rgb);
    destination->connection_border =
        colourref_from_common(source->connection_border_rgb);
    destination->card_hover = colourref_from_common(source->card_hover_rgb);
}

bool lsm_windows_theme_is_dark(const LsmWindowsTheme *theme)
{
    if (!theme) return true;
    if (theme->mode == INFILTRATR_THEME_NIGHT) return true;
    if (theme->mode == INFILTRATR_THEME_DAY) return false;
    return system_prefers_dark();
}

bool lsm_windows_theme_resolve(LsmWindowsTheme *theme)
{
    if (!theme) return false;
    const InfiltratrThemePalette *source =
        infiltratr_theme_resolve(theme->mode, system_prefers_dark());
    if (!infiltratr_theme_palette_is_valid(source)) return false;
    project_palette(source, &theme->palette);
    return true;
}

bool lsm_windows_theme_load(LsmWindowsTheme *theme)
{
    if (!theme) return false;
    theme->mode = INFILTRATR_THEME_NIGHT;

    DWORD mode = 0U;
    DWORD size = sizeof(mode);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Infiltrator\\System Monitor",
        L"ThemeMode", RRF_RT_REG_DWORD, NULL, &mode, &size);
    if (status == ERROR_SUCCESS && mode <= (DWORD)INFILTRATR_THEME_NIGHT)
        theme->mode = (InfiltratrThemeMode)mode;
    return lsm_windows_theme_resolve(theme);
}

void lsm_windows_theme_save(const LsmWindowsTheme *theme)
{
    if (!theme) return;
    HKEY key = NULL;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Infiltrator\\System Monitor",
            0U, NULL, 0U, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
        return;

    const DWORD mode = (DWORD)theme->mode;
    (void)RegSetValueExW(
        key, L"ThemeMode", 0U, REG_DWORD,
        (const BYTE *)&mode, sizeof(mode));
    RegCloseKey(key);
}
