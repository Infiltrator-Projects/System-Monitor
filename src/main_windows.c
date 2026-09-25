// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main_windows.c
 * @brief Native Win32 System Monitor presentation shell.
 *
 * Windows uses the same product hierarchy and Infiltratr design language as
 * the Linux application while keeping presentation native to Win32. Top-level
 * product pages live in the tab strip; Performance owns its own resource rail.
 * CPU, memory, storage, network, graphics identity and process data are live
 * through native adapters. Unimplemented Windows backends remain explicit
 * placeholders rather than redefining the shared product model.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_platform.h"
#include "process_backend.h"
#include "presentation_contract.h"
#include "performance_view.h"

#include <infiltratr/core.h>
#include <infiltratr/design.h>
#include <infiltratr/format.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef LSM_VERSION
#error "LSM_VERSION must be supplied from support/VERSION by the Windows build"
#endif

#define LSM_WINDOWS_TIMER_ID 1U
#define LSM_WINDOWS_REFRESH_MS 1000U
#define LSM_WINDOWS_HISTORY_CAPACITY 120U
#define LSM_WINDOWS_MENU_HEIGHT 32
#define LSM_WINDOWS_SUMMARY_HEIGHT 56
#define LSM_WINDOWS_TAB_HEIGHT 38
#define LSM_WINDOWS_STATUS_HEIGHT 30
#define LSM_WINDOWS_MESSAGE_START_BACKEND (WM_APP + 1)
#define LSM_WINDOWS_MAX_PERFORMANCE_ITEMS \
    (2U + LSM_MAX_DISKS + LSM_MAX_NETS + LSM_MAX_GPUS)
#include "windows_resources.h"

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


typedef struct {
    LsmPageType type;
    size_t index;
    RECT rect;
} LsmWindowsPerformanceItem;

typedef struct {
    HINSTANCE instance;
    HWND window;
    HWND process_list;
    HFONT body_font;
    HFONT body_bold_font;
    HFONT rail_value_font;
    HFONT title_font;
    HFONT heading_font;
    HFONT metric_font;
    HANDLE font_resources[LSM_WINDOWS_FONT_RESOURCE_COUNT];
    size_t font_resource_count;
    LsmMonitor monitor;
    bool startup_smoke;
    bool monitor_initialised;
    bool monitor_ready;
    bool process_backend_attempted;
    LsmProcessBackend *process_backend;
    LsmProcessInfo *processes;
    size_t process_count;
    LsmTabIndex active_page;
    LsmPageType active_performance_item;
    size_t active_performance_index;
    InfiltratrThemeMode theme_mode;
    const InfiltratrDesignMetrics *design;
    LsmWindowsPalette palette;
    int hovered_tab;
    int hovered_performance_item;
    int hovered_menu;
    bool tracking_mouse_leave;
    RECT page_tabs[LSM_TAB_COUNT];
    LsmWindowsPerformanceItem
        performance_items[LSM_WINDOWS_MAX_PERFORMANCE_ITEMS];
    size_t performance_item_count;
    int performance_scroll_y;
    RECT file_menu_rect;
    RECT view_menu_rect;
    RECT help_menu_rect;
    wchar_t status_text[256];
    double cpu_history[LSM_WINDOWS_HISTORY_CAPACITY];
    double memory_history[LSM_WINDOWS_HISTORY_CAPACITY];
    size_t history_count;
    size_t history_position;
} LsmWindowsUiState;

enum {
    LSM_WINDOWS_ID_EXIT = 2000,
    LSM_WINDOWS_ID_ABOUT = 2001,
    LSM_WINDOWS_ID_THEME_SYSTEM = 2100,
    LSM_WINDOWS_ID_THEME_DAY = 2101,
    LSM_WINDOWS_ID_THEME_NIGHT = 2102
};

static LRESULT CALLBACK lsm_windows_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK lsm_windows_about_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam);
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance,
                   LPSTR command_line, int show_command);

static LsmWindowsUiState *window_state(HWND window)
{
    return (LsmWindowsUiState *)GetWindowLongPtrW(window, GWLP_USERDATA);
}

static COLORREF colourref_from_common(uint32_t rgb)
{
    return RGB(
        (BYTE)((rgb >> 16U) & 0xffU),
        (BYTE)((rgb >> 8U) & 0xffU),
        (BYTE)(rgb & 0xffU));
}

static bool common_design_supported(const InfiltratrDesignMetrics *design)
{
    return design &&
           design->abi_version == INFILTRATR_DESIGN_METRICS_ABI &&
           design->struct_size >=
               offsetof(InfiltratrDesignMetrics, screen_padding) +
               sizeof(design->screen_padding) &&
           design->small_radius <= (uint32_t)INT_MAX &&
           design->control_radius <= (uint32_t)INT_MAX &&
           design->card_radius <= (uint32_t)INT_MAX &&
           design->compact_spacing <= (uint32_t)INT_MAX &&
           design->control_spacing <= (uint32_t)INT_MAX &&
           design->section_spacing <= (uint32_t)INT_MAX &&
           design->screen_padding <= (uint32_t)INT_MAX;
}

static bool common_palette_supported(const InfiltratrThemePalette *palette)
{
    return palette &&
           palette->abi_version == INFILTRATR_THEME_PALETTE_ABI &&
           palette->struct_size >=
               offsetof(InfiltratrThemePalette, success_border_rgb) +
               sizeof(palette->success_border_rgb);
}

static void project_common_palette(const InfiltratrThemePalette *source,
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
    destination->accent =
        colourref_from_common(source->neutral_accent_rgb);
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

static bool system_prefers_dark(void)
{
    DWORD light_theme = 1U;
    DWORD size = sizeof(light_theme);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD, NULL, &light_theme, &size);
    return status == ERROR_SUCCESS && light_theme == 0U;
}

static bool theme_is_dark(const LsmWindowsUiState *state)
{
    if (!state) return true;
    if (state->theme_mode == INFILTRATR_THEME_NIGHT) return true;
    if (state->theme_mode == INFILTRATR_THEME_DAY) return false;
    return system_prefers_dark();
}

static bool resolve_theme(LsmWindowsUiState *state)
{
    if (!state) return false;
    const InfiltratrThemePalette *source =
        infiltratr_theme_resolve(state->theme_mode, system_prefers_dark());
    if (!common_palette_supported(source)) return false;
    project_common_palette(source, &state->palette);
    return true;
}

static bool load_theme_preference(LsmWindowsUiState *state)
{
    if (!state) return false;
    /* Preserve the established black/graphite preview unless the user
     * explicitly selects Follow system or Day. */
    state->theme_mode = INFILTRATR_THEME_NIGHT;

    DWORD mode = 0U;
    DWORD size = sizeof(mode);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Infiltrator\\System Monitor",
        L"ThemeMode",
        RRF_RT_REG_DWORD, NULL, &mode, &size);
    if (status == ERROR_SUCCESS && mode <= (DWORD)INFILTRATR_THEME_NIGHT)
        state->theme_mode = (InfiltratrThemeMode)mode;
    return resolve_theme(state);
}

static int performance_item_stride(const LsmWindowsUiState *state)
{
    return LSM_SIDE_BUTTON_HEIGHT + (int)state->design->compact_spacing;
}

static void save_theme_preference(const LsmWindowsUiState *state)
{
    if (!state) return;
    HKEY key = NULL;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Infiltrator\\System Monitor",
            0U, NULL, 0U, KEY_SET_VALUE, NULL, &key, NULL) !=
        ERROR_SUCCESS)
        return;

    const DWORD mode = (DWORD)state->theme_mode;
    (void)RegSetValueExW(
        key, L"ThemeMode", 0U, REG_DWORD,
        (const BYTE *)&mode, sizeof(mode));
    RegCloseKey(key);
}

static void write_startup_smoke_status(const char *status)
{
    if (!status) return;

    HANDLE file = CreateFileW(
        L"windows-startup-smoke.txt", GENERIC_WRITE, FILE_SHARE_READ,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;

    DWORD written = 0U;
    (void)WriteFile(
        file, status, (DWORD)lstrlenA(status), &written, NULL);
    CloseHandle(file);
}

static void set_status(LsmWindowsUiState *state, const wchar_t *text)
{
    if (!state) return;
    lstrcpynW(
        state->status_text,
        text ? text : L"",
        (int)(sizeof(state->status_text) / sizeof(state->status_text[0])));
    if (state->window) InvalidateRect(state->window, NULL, FALSE);
}

static void text_to_wide(const char *source, wchar_t *destination,
                         size_t capacity)
{
    if (!destination || capacity == 0U) return;
    destination[0] = L'\0';
    if (!source || !source[0] || capacity > (size_t)INT_MAX) return;

    const int converted = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
        destination, (int)capacity);
    if (converted <= 0) destination[0] = L'\0';
}

static COLORREF performance_colour_ref(LsmPageType type)
{
    const LsmPresentationColour colour =
        lsm_performance_colour_rgb(type);
    return RGB(colour.red, colour.green, colour.blue);
}

static void select_font(HDC dc, HFONT font)
{
    if (dc && font) (void)SelectObject(dc, font);
}

static void fill_solid(HDC dc, const RECT *rect, COLORREF colour)
{
    HBRUSH brush = CreateSolidBrush(colour);
    if (!brush) return;
    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void draw_round_panel(HDC dc, const RECT *rect, COLORREF fill,
                             COLORREF border, int radius)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }

    HGDIOBJ previous_brush = SelectObject(dc, brush);
    HGDIOBJ previous_pen = SelectObject(dc, pen);
    RoundRect(dc, rect->left, rect->top, rect->right, rect->bottom,
              radius * 2, radius * 2);
    SelectObject(dc, previous_brush);
    SelectObject(dc, previous_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void draw_text(HDC dc, const wchar_t *text, RECT rect, HFONT font,
                      COLORREF colour, UINT flags)
{
    if (!dc || !text) return;
    select_font(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colour);
    DrawTextW(dc, text, -1, &rect, flags | DT_NOPREFIX);
}

static const wchar_t about_body_text[] =
    L"Native Windows presentation using the Infiltratr "
    L"Day/Night design contract and Linux product layout.\r\n\r\n"
    L"CPU, memory, disks, network adapters, graphics identity and processes "
    L"are live and read-only. Additional Windows telemetry remains under "
    L"active development.";

static int measure_wrapped_text_height(
    HWND reference_window, HFONT font,
    const wchar_t *text, int width)
{
    if (!reference_window || !font || !text || width <= 0)
        return 0;

    HDC dc = GetDC(reference_window);
    if (!dc) return 0;

    HGDIOBJ previous = SelectObject(dc, font);
    RECT measure = {0, 0, width, 0};
    const int result = DrawTextW(
        dc, text, -1, &measure,
        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
    SelectObject(dc, previous);
    ReleaseDC(reference_window, dc);

    if (result <= 0) return 0;
    return measure.bottom - measure.top;
}

static RECT about_ok_rect(HWND window)
{
    RECT client = {0, 0, 0, 0};
    (void)GetClientRect(window, &client);
    RECT button = {
        client.right - 112,
        client.bottom - 54,
        client.right - 24,
        client.bottom - 20
    };
    return button;
}

static void paint_about_window(
    LsmWindowsUiState *state, HWND window, HDC dc)
{
    if (!state || !window || !dc) return;

    RECT client = {0, 0, 0, 0};
    if (!GetClientRect(window, &client)) return;
    fill_solid(dc, &client, state->palette.background);

    RECT card = {
        18, 18,
        client.right - 18,
        client.bottom - 72
    };
    draw_round_panel(
        dc, &card, state->palette.card,
        state->palette.border, (int)state->design->card_radius);

    wchar_t version[64] = L"development";
    text_to_wide(
        LSM_VERSION, version,
        sizeof(version) / sizeof(version[0]));
    wchar_t heading[128];
    (void)swprintf(
        heading, sizeof(heading) / sizeof(heading[0]),
        L"System Monitor %ls", version);

    RECT title = {
        card.left + 18, card.top + 14,
        card.right - 18, card.top + 48
    };
    draw_text(
        dc, heading, title, state->heading_font,
        state->palette.heading,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT divider = {
        card.left + 18, card.top + 52,
        card.right - 18, card.top + 53
    };
    fill_solid(dc, &divider, state->palette.border);

    RECT body = {
        card.left + 18, card.top + 66,
        card.right - 18, card.bottom - 18
    };
    draw_text(
        dc, about_body_text,
        body, state->body_font, state->palette.summary,
        DT_LEFT | DT_TOP | DT_WORDBREAK);

    RECT button = about_ok_rect(window);
    draw_round_panel(
        dc, &button, state->palette.card_hover,
        state->palette.accent, (int)state->design->control_radius);
    draw_text(
        dc, L"OK", button, state->body_bold_font,
        state->palette.heading,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK lsm_windows_about_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LsmWindowsUiState *state =
        (LsmWindowsUiState *)GetWindowLongPtrW(window, GWLP_USERDATA);

    if (message == WM_NCCREATE) {
        const CREATESTRUCTW *create = (const CREATESTRUCTW *)lparam;
        state = create
            ? (LsmWindowsUiState *)create->lpCreateParams
            : NULL;
        SetWindowLongPtrW(
            window, GWLP_USERDATA, (LONG_PTR)state);
    }

    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            paint_about_window(state, window, dc);
            EndPaint(window, &paint);
            return 0;
        }

        case WM_LBUTTONUP:
            if (state) {
                POINT point = {
                    GET_X_LPARAM(lparam),
                    GET_Y_LPARAM(lparam)
                };
                RECT button = about_ok_rect(window);
                if (PtInRect(&button, point)) {
                    DestroyWindow(window);
                    return 0;
                }
            }
            break;

        case WM_KEYDOWN:
            if (wparam == VK_RETURN || wparam == VK_ESCAPE) {
                DestroyWindow(window);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        default:
            break;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

static void show_about_window(LsmWindowsUiState *state)
{
    if (!state || !state->window || !state->instance) return;

    static const wchar_t about_class_name[] =
        L"InfiltratorSystemMonitorAbout";

    WNDCLASSEXW window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = lsm_windows_about_proc;
    window_class.hInstance = state->instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = NULL;
    window_class.lpszClassName = about_class_name;

    if (!RegisterClassExW(&window_class) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return;

    RECT owner = {0, 0, 0, 0};
    (void)GetWindowRect(state->window, &owner);

    const DWORD style =
        WS_POPUP | WS_CAPTION | WS_SYSMENU;
    const DWORD ex_style = WS_EX_DLGMODALFRAME;
    const int client_width = 560;
    const int body_width = client_width - 72;
    int body_height = measure_wrapped_text_height(
        state->window, state->body_font,
        about_body_text, body_width);
    if (body_height < 78) body_height = 78;

    const int client_height = 174 + body_height;
    RECT window_rect = {
        0, 0, client_width, client_height
    };
    if (!AdjustWindowRectEx(
            &window_rect, style, FALSE, ex_style))
        return;

    const int width =
        window_rect.right - window_rect.left;
    const int height =
        window_rect.bottom - window_rect.top;
    const int x =
        owner.left + ((owner.right - owner.left) - width) / 2;
    const int y =
        owner.top + ((owner.bottom - owner.top) - height) / 2;

    HWND about = CreateWindowExW(
        ex_style,
        about_class_name,
        L"About System Monitor",
        style,
        x, y, width, height,
        state->window, NULL, state->instance, state);
    if (!about) return;

    const BOOL dark_titlebar =
        theme_is_dark(state) ? TRUE : FALSE;
    (void)DwmSetWindowAttribute(
        about, 20U, &dark_titlebar, sizeof(dark_titlebar));

    EnableWindow(state->window, FALSE);
    ShowWindow(about, SW_SHOW);
    UpdateWindow(about);
    SetForegroundWindow(about);

    MSG message;
    while (IsWindow(about)) {
        const BOOL result = GetMessageW(
            &message, NULL, 0U, 0U);
        if (result <= 0) {
            if (result == 0)
                PostQuitMessage((int)message.wParam);
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    EnableWindow(state->window, TRUE);
    SetForegroundWindow(state->window);
}

static void append_history(LsmWindowsUiState *state)
{
    if (!state || !state->monitor_ready) return;

    state->cpu_history[state->history_position] =
        state->monitor.cpu.usage_percent;
    state->memory_history[state->history_position] =
        state->monitor.memory.usage_percent;
    state->history_position =
        (state->history_position + 1U) % LSM_WINDOWS_HISTORY_CAPACITY;
    if (state->history_count < LSM_WINDOWS_HISTORY_CAPACITY)
        state->history_count++;
}

static double history_value(const double *history, size_t count,
                            size_t position, size_t logical_index)
{
    if (!history || count == 0U || logical_index >= count) return 0.0;
    size_t first = position >= count
        ? position - count
        : LSM_WINDOWS_HISTORY_CAPACITY - (count - position);
    const size_t index =
        (first + logical_index) % LSM_WINDOWS_HISTORY_CAPACITY;
    return history[index];
}

static void draw_history_graph(LsmWindowsUiState *state, HDC dc, RECT rect,
                               const double *history, size_t count,
                               size_t position, COLORREF line_colour)
{
    draw_round_panel(
        dc, &rect, state->palette.surface,
        state->palette.connection_border, (int)state->design->card_radius);

    RECT inner = {
        rect.left + 14, rect.top + 14,
        rect.right - 14, rect.bottom - 14
    };
    HPEN grid_pen = CreatePen(PS_SOLID, 1, state->palette.border);
    if (grid_pen) {
        HGDIOBJ previous = SelectObject(dc, grid_pen);
        for (int row = 1; row < 4; row++) {
            const int y = inner.top +
                ((inner.bottom - inner.top) * row) / 4;
            MoveToEx(dc, inner.left, y, NULL);
            LineTo(dc, inner.right, y);
        }
        SelectObject(dc, previous);
        DeleteObject(grid_pen);
    }

    if (count < 2U) return;

    HPEN graph_pen = CreatePen(PS_SOLID, 2, line_colour);
    if (!graph_pen) return;
    HGDIOBJ previous = SelectObject(dc, graph_pen);
    const int width = inner.right - inner.left;
    const int height = inner.bottom - inner.top;

    for (size_t index = 0U; index < count; index++) {
        const double value =
            history_value(history, count, position, index);
        const double bounded =
            value < 0.0 ? 0.0 : (value > 100.0 ? 100.0 : value);
        const int x = inner.left +
            (int)((index * (size_t)width) / (count - 1U));
        const int y = inner.bottom -
            (int)((bounded / 100.0) * (double)height);
        if (index == 0U)
            MoveToEx(dc, x, y, NULL);
        else
            LineTo(dc, x, y);
    }

    SelectObject(dc, previous);
    DeleteObject(graph_pen);
}

static void draw_menu_strip(LsmWindowsUiState *state, HDC dc, int width)
{
    RECT menu = {0, 0, width, LSM_WINDOWS_MENU_HEIGHT};
    fill_solid(dc, &menu, state->palette.panel);

    RECT divider = {
        0, LSM_WINDOWS_MENU_HEIGHT - 1,
        width, LSM_WINDOWS_MENU_HEIGHT
    };
    fill_solid(dc, &divider, state->palette.border);

    SetRect(&state->file_menu_rect, 16, 0, 62, LSM_WINDOWS_MENU_HEIGHT);
    SetRect(&state->view_menu_rect, 68, 0, 122, LSM_WINDOWS_MENU_HEIGHT);
    SetRect(&state->help_menu_rect, 128, 0, 182, LSM_WINDOWS_MENU_HEIGHT);

    const RECT menu_rects[] = {
        state->file_menu_rect,
        state->view_menu_rect,
        state->help_menu_rect
    };
    const wchar_t *const names[] = {L"File", L"View", L"Help"};
    for (int index = 0; index < 3; index++) {
        if (state->hovered_menu == index) {
            RECT hover = menu_rects[index];
            hover.left -= 6;
            hover.right += 6;
            fill_solid(dc, &hover, state->palette.card_hover);
        }
        draw_text(
            dc, names[index], menu_rects[index], state->body_font,
            state->hovered_menu == index
                ? state->palette.title : state->palette.summary,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_summary_bar(LsmWindowsUiState *state, HDC dc, int width)
{
    const int screen_padding = (int)state->design->screen_padding;
    const int control_spacing = (int)state->design->control_spacing;
    RECT bar = {
        screen_padding,
        LSM_WINDOWS_MENU_HEIGHT + control_spacing,
        width - screen_padding,
        LSM_WINDOWS_MENU_HEIGHT + control_spacing +
            LSM_WINDOWS_SUMMARY_HEIGHT
    };
    draw_round_panel(
        dc, &bar, state->palette.connection,
        state->palette.connection_border, (int)state->design->card_radius);

    LsmSummaryPerformanceView summary;
    lsm_summary_performance_view(
        state->monitor_ready ? &state->monitor : NULL, false, &summary);

    wchar_t captions[LSM_SUMMARY_COUNT][32];
    wchar_t values[LSM_SUMMARY_COUNT][LSM_SUMMARY_VIEW_VALUE_LEN];
    for (size_t index = 0U; index < LSM_SUMMARY_COUNT; index++) {
        text_to_wide(
            lsm_summary_label((LsmSummaryField)index),
            captions[index],
            sizeof(captions[index]) / sizeof(captions[index][0]));
        text_to_wide(
            summary.values[index],
            values[index],
            sizeof(values[index]) / sizeof(values[index][0]));
    }

    const int item_width =
        (bar.right - bar.left) / (int)LSM_SUMMARY_COUNT;
    for (size_t index = 0U; index < LSM_SUMMARY_COUNT; index++) {
        const int item = (int)index;
        RECT caption = {
            bar.left + item * item_width,
            bar.top + 10,
            bar.left + (item + 1) * item_width,
            bar.top + 30
        };
        RECT value = {
            caption.left, bar.top + 29,
            caption.right, bar.bottom - 8
        };
        draw_text(
            dc, captions[index], caption, state->body_font,
            state->palette.summary, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        draw_text(
            dc, values[index], value, state->body_bold_font,
            state->palette.heading, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_page_tabs(LsmWindowsUiState *state, HDC dc, int width)
{
    const int top =
        LSM_WINDOWS_MENU_HEIGHT + (int)state->design->control_spacing +
        LSM_WINDOWS_SUMMARY_HEIGHT + (int)state->design->control_spacing;
    RECT strip = {0, top, width, top + LSM_WINDOWS_TAB_HEIGHT};
    fill_solid(dc, &strip, state->palette.panel);

    int x = (int)state->design->screen_padding;
    select_font(dc, state->body_bold_font);
    for (int index = 0; index < LSM_TAB_COUNT; index++) {
        wchar_t page_label[64];
        text_to_wide(
            lsm_tab_label((LsmTabIndex)index),
            page_label,
            sizeof(page_label) / sizeof(page_label[0]));
        SIZE text_size = {0, 0};
        (void)GetTextExtentPoint32W(
            dc, page_label,
            lstrlenW(page_label), &text_size);
        int tab_width = text_size.cx + 28;
        if (tab_width < 92) tab_width = 92;

        RECT tab = {
            x, top,
            x + tab_width,
            top + LSM_WINDOWS_TAB_HEIGHT
        };
        if (tab.right > width - (int)state->design->screen_padding)
            tab.right = width - (int)state->design->screen_padding;
        state->page_tabs[index] = tab;

        const bool active = index == (int)state->active_page;
        const bool hovered = index == state->hovered_tab;
        if (hovered && !active)
            fill_solid(dc, &tab, state->palette.card_hover);
        if (active) {
            RECT underline = {
                tab.left + 8, tab.bottom - 3,
                tab.right - 8, tab.bottom
            };
            fill_solid(dc, &underline, state->palette.accent);
        }
        draw_text(
            dc, page_label, tab,
            active ? state->body_bold_font : state->body_font,
            active ? state->palette.accent : state->palette.summary,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        x = tab.right;
        if (x >= width - (int)state->design->screen_padding) {
            for (int hidden = index + 1;
                 hidden < LSM_TAB_COUNT; hidden++)
                SetRectEmpty(&state->page_tabs[hidden]);
            break;
        }
    }

    RECT divider = {
        0, strip.bottom - 1, width, strip.bottom
    };
    fill_solid(dc, &divider, state->palette.border);
}

static RECT content_rect_for_client(const LsmWindowsUiState *state,
                                    int width, int height)
{
    const int top =
        LSM_WINDOWS_MENU_HEIGHT + (int)state->design->control_spacing +
        LSM_WINDOWS_SUMMARY_HEIGHT + (int)state->design->control_spacing +
        LSM_WINDOWS_TAB_HEIGHT + (int)state->design->section_spacing;
    RECT rect = {
        (int)state->design->screen_padding,
        top,
        width - (int)state->design->screen_padding,
        height - LSM_WINDOWS_STATUS_HEIGHT -
            (int)state->design->screen_padding
    };
    return rect;
}

static void draw_mini_history(
    LsmWindowsUiState *state, HDC dc, RECT rect,
    const double *history, COLORREF line_colour)
{
    draw_round_panel(
        dc, &rect, state->palette.surface,
        state->palette.connection_border, (int)state->design->small_radius);

    if (!history || state->history_count < 2U) return;

    HPEN pen = CreatePen(PS_SOLID, 2, line_colour);
    if (!pen) return;
    HGDIOBJ previous = SelectObject(dc, pen);
    const int width = rect.right - rect.left - 8;
    const int height = rect.bottom - rect.top - 8;
    for (size_t index = 0U; index < state->history_count; index++) {
        const double value = history_value(
            history, state->history_count,
            state->history_position, index);
        const double bounded =
            value < 0.0 ? 0.0 : (value > 100.0 ? 100.0 : value);
        const int x = rect.left + 4 +
            (int)((index * (size_t)width) /
                  (state->history_count - 1U));
        const int y = rect.bottom - 4 -
            (int)((bounded / 100.0) * (double)height);
        if (index == 0U)
            MoveToEx(dc, x, y, NULL);
        else
            LineTo(dc, x, y);
    }
    SelectObject(dc, previous);
    DeleteObject(pen);
}

static void draw_performance_rail_item(
    LsmWindowsUiState *state, HDC dc, size_t slot,
    LsmPageType type, size_t device_index, RECT rect,
    const wchar_t *title, const wchar_t *value,
    const double *history, COLORREF line_colour)
{
    if (!state || slot >= LSM_WINDOWS_MAX_PERFORMANCE_ITEMS) return;
    state->performance_items[slot].type = type;
    state->performance_items[slot].index = device_index;
    state->performance_items[slot].rect = rect;
    const bool active =
        type == state->active_performance_item &&
        device_index == state->active_performance_index;
    const bool hovered = (int)slot == state->hovered_performance_item;
    draw_round_panel(
        dc, &rect,
        active ? state->palette.selection :
            (hovered ? state->palette.card_hover : state->palette.panel),
        active ? line_colour :
            (hovered ? state->palette.border : state->palette.panel),
        (int)state->design->control_radius);

    RECT sparkline = {
        rect.left + 8, rect.top + 12,
        rect.left + 72, rect.top + 56
    };
    draw_mini_history(
        state, dc, sparkline, history, line_colour);

    RECT title_rect = {
        sparkline.right + 8, rect.top + 8,
        rect.right - 8, rect.top + 31
    };
    RECT value_rect = {
        sparkline.right + 8, rect.top + 32,
        rect.right - 8, rect.bottom - 7
    };
    draw_text(
        dc, title, title_rect, state->body_bold_font,
        active ? line_colour : state->palette.text,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    draw_text(
        dc, value, value_rect, state->rail_value_font,
        active ? state->palette.selected_summary : state->palette.summary,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void draw_metric_block(
    LsmWindowsUiState *state, HDC dc, RECT rect,
    const wchar_t *caption, const wchar_t *value)
{
    RECT caption_rect = {
        rect.left, rect.top,
        rect.right, rect.top + 18
    };
    RECT value_rect = {
        rect.left, rect.top + 18,
        rect.right, rect.bottom
    };
    draw_text(
        dc, caption, caption_rect, state->body_font,
        state->palette.summary,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    draw_text(
        dc, value, value_rect, state->metric_font,
        state->palette.heading,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static int measure_text_width(HDC dc, HFONT font, const wchar_t *text)
{
    if (!dc || !font || !text) return 0;
    HGDIOBJ previous = SelectObject(dc, font);
    SIZE size = {0, 0};
    const int length = lstrlenW(text);
    if (length > 0)
        (void)GetTextExtentPoint32W(dc, text, length, &size);
    SelectObject(dc, previous);
    return size.cx;
}

static void draw_detail_pair(
    LsmWindowsUiState *state, HDC dc, RECT rect,
    const wchar_t *label, const wchar_t *value, int label_width)
{
    const int available = rect.right - rect.left;
    if (label_width < 40) label_width = 40;
    if (label_width > available - 48) label_width = available - 48;
    const int split = rect.left + label_width + 10;
    RECT label_rect = {
        rect.left, rect.top,
        split - 8, rect.bottom
    };
    RECT value_rect = {
        split, rect.top,
        rect.right, rect.bottom
    };
    draw_text(
        dc, label, label_rect, state->body_font,
        state->palette.detail,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    draw_text(
        dc, value, value_rect, state->body_font,
        state->palette.text,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void draw_memory_composition(
    LsmWindowsUiState *state, HDC dc, RECT rect)
{
    draw_round_panel(
        dc, &rect, state->palette.surface,
        state->palette.connection_border, 6);

    if (!state->monitor_ready || state->monitor.memory.total_bytes == 0U)
        return;

    const double total = (double)state->monitor.memory.total_bytes;
    double used_fraction =
        (double)state->monitor.memory.used_bytes / total;
    if (used_fraction < 0.0) used_fraction = 0.0;
    if (used_fraction > 1.0) used_fraction = 1.0;

    RECT used = rect;
    used.left += 2;
    used.top += 2;
    used.bottom -= 2;
    used.right = used.left +
        (int)((double)(rect.right - rect.left - 4) * used_fraction);
    if (used.right > used.left)
        fill_solid(dc, &used, RGB(0x1C, 0x32, 0x48));

    HPEN marker = CreatePen(PS_SOLID, 2, performance_colour_ref(LSM_PAGE_MEMORY));
    if (marker) {
        HGDIOBJ previous = SelectObject(dc, marker);
        const int x = used.right;
        MoveToEx(dc, x, rect.top + 2, NULL);
        LineTo(dc, x, rect.bottom - 2);
        SelectObject(dc, previous);
        DeleteObject(marker);
    }
}

static void draw_cpu_page(LsmWindowsUiState *state, HDC dc, RECT content)
{
    LsmCpuPerformanceView view;
    lsm_cpu_performance_view(
        state->monitor_ready ? &state->monitor : NULL, &view);

    wchar_t model[LSM_NAME_LEN];
    wchar_t metric_values[LSM_CPU_METRIC_COUNT]
                         [LSM_PERFORMANCE_VIEW_VALUE_LEN];
    wchar_t detail_values[LSM_CPU_DETAIL_COUNT]
                         [LSM_PERFORMANCE_VIEW_VALUE_LEN];
    text_to_wide(
        view.subtitle, model,
        sizeof(model) / sizeof(model[0]));
    for (int index = 0; index < LSM_CPU_METRIC_COUNT; index++) {
        text_to_wide(
            view.metrics[index], metric_values[index],
            sizeof(metric_values[index]) /
                sizeof(metric_values[index][0]));
    }
    for (int index = 0; index < LSM_CPU_DETAIL_COUNT; index++) {
        text_to_wide(
            view.details[index], detail_values[index],
            sizeof(detail_values[index]) /
                sizeof(detail_values[index][0]));
    }

    const int header_height = 54;
    const int scale_height = 26;
    const int details_height = 214;
    const int gap = 7;

    RECT header = {
        content.left, content.top,
        content.right, content.top + header_height
    };
    draw_round_panel(
        dc, &header, state->palette.card,
        state->palette.border, (int)state->design->card_radius);

    RECT title_rect = {
        header.left + 14, header.top + 6,
        header.left + 180, header.bottom - 6
    };
    RECT subtitle_rect = {
        title_rect.right + 12, header.top + 6,
        header.right - 14, header.bottom - 6
    };
    wchar_t cpu_title[32];
    text_to_wide(
        lsm_performance_page_title(LSM_PAGE_CPU),
        cpu_title, sizeof(cpu_title) / sizeof(cpu_title[0]));
    draw_text(
        dc, cpu_title, title_rect, state->title_font,
        state->palette.heading,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(
        dc, model, subtitle_rect, state->body_font,
        state->palette.summary,
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT scale = {
        content.left, header.bottom + gap,
        content.right, header.bottom + gap + scale_height
    };
    RECT scale_name = scale;
    scale_name.right = scale.left + 180;
    RECT scale_max = scale;
    scale_max.left = scale.right - 80;
    wchar_t cpu_graph_caption[64];
    wchar_t percent_scale_max[16];
    text_to_wide(
        lsm_cpu_graph_caption(),
        cpu_graph_caption,
        sizeof(cpu_graph_caption) / sizeof(cpu_graph_caption[0]));
    text_to_wide(
        lsm_percent_scale_max_label(),
        percent_scale_max,
        sizeof(percent_scale_max) / sizeof(percent_scale_max[0]));
    draw_text(
        dc, cpu_graph_caption, scale_name, state->body_font,
        state->palette.summary,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(
        dc, percent_scale_max, scale_max, state->body_font,
        state->palette.summary,
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    RECT graph = {
        content.left,
        scale.bottom + gap,
        content.right,
        content.bottom - details_height - gap
    };
    if (graph.bottom < graph.top + LSM_PRIMARY_GRAPH_MIN_HEIGHT)
        graph.bottom = graph.top + LSM_PRIMARY_GRAPH_MIN_HEIGHT;
    draw_history_graph(
        state, dc, graph, state->cpu_history,
        state->history_count, state->history_position,
        performance_colour_ref(LSM_PAGE_CPU));

    RECT details = {
        content.left,
        graph.bottom + gap,
        content.right,
        content.bottom
    };
    draw_round_panel(
        dc, &details, state->palette.card,
        state->palette.border, (int)state->design->card_radius);

    const int pad = 14;
    const int metrics_width = 420;
    const int separator_x = details.left + pad + metrics_width;
    RECT separator = {
        separator_x, details.top + 12,
        separator_x + 1, details.bottom - 12
    };
    fill_solid(dc, &separator, state->palette.border);

    wchar_t metric_names[LSM_CPU_METRIC_COUNT][64];
    for (int index = 0; index < LSM_CPU_METRIC_COUNT; index++) {
        text_to_wide(
            lsm_cpu_metric_label((LsmCpuMetricField)index),
            metric_names[index],
            sizeof(metric_names[index]) / sizeof(metric_names[index][0]));
    }
    const int metric_col_width = (metrics_width - 28) / 2;
    const int metric_row_height = 36;
    for (int index = 0; index < LSM_CPU_METRIC_COUNT; index++) {
        const LsmPresentationGridPosition position =
            lsm_cpu_metric_position((LsmCpuMetricField)index);
        const int col = (int)position.column;
        const int row = (int)position.row;
        RECT block = {
            details.left + pad + col * (metric_col_width + 28),
            details.top + 12 + row * metric_row_height,
            details.left + pad + col * (metric_col_width + 28) +
                metric_col_width,
            details.top + 12 + (row + 1) * metric_row_height
        };
        draw_metric_block(
            state, dc, block,
            metric_names[index], metric_values[index]);
    }

    wchar_t detail_names[LSM_CPU_DETAIL_COUNT][64];
    for (int index = 0; index < LSM_CPU_DETAIL_COUNT; index++) {
        text_to_wide(
            lsm_cpu_detail_label((LsmCpuDetailField)index),
            detail_names[index],
            sizeof(detail_names[index]) / sizeof(detail_names[index][0]));
    }
    const int info_left = separator_x + 18;
    const int info_width = details.right - pad - info_left;
    const int info_col_width = (info_width - 18) / 2;
    const int info_row_height = 25;
    int detail_label_width[2] = {0, 0};
    for (int index = 0; index < LSM_CPU_DETAIL_COUNT; index++) {
        const LsmPresentationGridPosition position =
            lsm_cpu_detail_position((LsmCpuDetailField)index);
        const int group = (int)position.column;
        const int measured = measure_text_width(
            dc, state->body_font, detail_names[index]);
        if (measured > detail_label_width[group])
            detail_label_width[group] = measured;
    }
    for (int group = 0; group < 2; group++) {
        if (detail_label_width[group] > info_col_width - 58)
            detail_label_width[group] = info_col_width - 58;
    }

    for (int index = 0; index < LSM_CPU_DETAIL_COUNT; index++) {
        const LsmPresentationGridPosition position =
            lsm_cpu_detail_position((LsmCpuDetailField)index);
        const int group = (int)position.column;
        const int row = (int)position.row;
        RECT pair = {
            info_left + group * (info_col_width + 18),
            details.top + 14 + row * info_row_height,
            info_left + group * (info_col_width + 18) +
                info_col_width,
            details.top + 14 + (row + 1) * info_row_height
        };
        draw_detail_pair(
            state, dc, pair,
            detail_names[index], detail_values[index],
            detail_label_width[group]);
    }
}

static void draw_memory_page(LsmWindowsUiState *state, HDC dc, RECT content)
{
    LsmMemoryPerformanceView view;
    lsm_memory_performance_view(
        state->monitor_ready ? &state->monitor : NULL, &view);

    wchar_t total_text[LSM_PERFORMANCE_VIEW_VALUE_LEN];
    wchar_t usage_values[LSM_MEMORY_METRIC_COUNT]
                        [LSM_PERFORMANCE_VIEW_VALUE_LEN];
    wchar_t hardware_values[LSM_MEMORY_DETAIL_COUNT]
                           [LSM_MEMORY_MODULE_VIEW_LEN];
    text_to_wide(
        view.subtitle, total_text,
        sizeof(total_text) / sizeof(total_text[0]));
    for (int index = 0; index < LSM_MEMORY_METRIC_COUNT; index++) {
        text_to_wide(
            view.metrics[index], usage_values[index],
            sizeof(usage_values[index]) /
                sizeof(usage_values[index][0]));
    }
    for (int index = 0; index < LSM_MEMORY_DETAIL_COUNT; index++) {
        text_to_wide(
            view.details[index], hardware_values[index],
            sizeof(hardware_values[index]) /
                sizeof(hardware_values[index][0]));
    }

    const int header_height = 72;
    const int composition_label_height = 24;
    const int composition_height = LSM_MEMORY_COMPOSITION_HEIGHT;
    const int details_height = 196;
    const int gap = 7;

    RECT header = {
        content.left, content.top,
        content.right, content.top + header_height
    };
    draw_round_panel(
        dc, &header, state->palette.card,
        state->palette.border, (int)state->design->card_radius);

    RECT title_rect = {
        header.left + 14, header.top + 5,
        header.left + 210, header.top + 37
    };
    RECT total_rect = {
        title_rect.right + 12, header.top + 5,
        header.right - 14, header.top + 37
    };
    RECT usage_name = {
        header.left + 14, header.top + 39,
        header.left + 220, header.bottom - 5
    };
    RECT usage_max = {
        header.right - 90, header.top + 39,
        header.right - 14, header.bottom - 5
    };
    wchar_t memory_title[32];
    text_to_wide(
        lsm_performance_page_title(LSM_PAGE_MEMORY),
        memory_title, sizeof(memory_title) / sizeof(memory_title[0]));
    draw_text(
        dc, memory_title, title_rect, state->title_font,
        state->palette.heading,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(
        dc, total_text, total_rect, state->body_font,
        state->palette.summary,
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    wchar_t memory_graph_caption[64];
    wchar_t percent_scale_max[16];
    text_to_wide(
        lsm_memory_graph_caption(),
        memory_graph_caption,
        sizeof(memory_graph_caption) / sizeof(memory_graph_caption[0]));
    text_to_wide(
        lsm_percent_scale_max_label(),
        percent_scale_max,
        sizeof(percent_scale_max) / sizeof(percent_scale_max[0]));
    draw_text(
        dc, memory_graph_caption, usage_name, state->body_font,
        state->palette.summary,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(
        dc, percent_scale_max, usage_max, state->body_font,
        state->palette.summary,
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    const int reserved_below_graph =
        composition_label_height + composition_height +
        details_height + (gap * 4);
    RECT graph = {
        content.left,
        header.bottom + gap,
        content.right,
        content.bottom - reserved_below_graph
    };
    if (graph.bottom < graph.top + LSM_PRIMARY_GRAPH_MIN_HEIGHT)
        graph.bottom = graph.top + LSM_PRIMARY_GRAPH_MIN_HEIGHT;
    draw_history_graph(
        state, dc, graph, state->memory_history,
        state->history_count, state->history_position,
        performance_colour_ref(LSM_PAGE_MEMORY));

    RECT composition_label = {
        content.left,
        graph.bottom + gap,
        content.right,
        graph.bottom + gap + composition_label_height
    };
    wchar_t composition_caption[64];
    text_to_wide(
        lsm_memory_composition_caption(),
        composition_caption,
        sizeof(composition_caption) / sizeof(composition_caption[0]));
    draw_text(
        dc, composition_caption, composition_label,
        state->body_font, state->palette.summary,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT composition = {
        content.left,
        composition_label.bottom,
        content.right,
        composition_label.bottom + composition_height
    };
    draw_memory_composition(state, dc, composition);

    RECT details = {
        content.left,
        composition.bottom + gap,
        content.right,
        content.bottom
    };
    draw_round_panel(
        dc, &details, state->palette.card,
        state->palette.border, (int)state->design->card_radius);

    wchar_t usage_names[LSM_MEMORY_METRIC_COUNT][64];
    for (int index = 0; index < LSM_MEMORY_METRIC_COUNT; index++) {
        text_to_wide(
            lsm_memory_metric_label((LsmMemoryMetricField)index),
            usage_names[index],
            sizeof(usage_names[index]) / sizeof(usage_names[index][0]));
    }
    const int pad = 14;
    const int usage_width = 460;
    const int separator_x = details.left + pad + usage_width;
    RECT separator = {
        separator_x, details.top + 12,
        separator_x + 1, details.bottom - 12
    };
    fill_solid(dc, &separator, state->palette.border);

    const int metric_col_width = (usage_width - 30) / 2;
    const int metric_row_height = 34;
    for (int index = 0; index < LSM_MEMORY_METRIC_COUNT; index++) {
        const LsmPresentationGridPosition position =
            lsm_memory_metric_position((LsmMemoryMetricField)index);
        const int col = (int)position.column;
        const int row = (int)position.row;
        RECT block = {
            details.left + pad + col * (metric_col_width + 30),
            details.top + 10 + row * metric_row_height,
            details.left + pad + col * (metric_col_width + 30) +
                metric_col_width,
            details.top + 10 + (row + 1) * metric_row_height
        };
        draw_metric_block(
            state, dc, block,
            usage_names[index], usage_values[index]);
    }

    wchar_t hardware_names[LSM_MEMORY_DETAIL_COUNT][64];
    for (int index = 0; index < LSM_MEMORY_DETAIL_COUNT; index++) {
        text_to_wide(
            lsm_memory_detail_label((LsmMemoryDetailField)index),
            hardware_names[index],
            sizeof(hardware_names[index]) /
                sizeof(hardware_names[index][0]));
    }
    const int info_left = separator_x + 18;
    const int info_width = details.right - pad - info_left;
    int hardware_label_width = 0;
    for (int index = 0; index < LSM_MEMORY_DETAIL_COUNT; index++) {
        const int measured = measure_text_width(
            dc, state->body_font, hardware_names[index]);
        if (measured > hardware_label_width)
            hardware_label_width = measured;
    }
    if (hardware_label_width > info_width - 72)
        hardware_label_width = info_width - 72;

    for (int index = 0; index < LSM_MEMORY_DETAIL_COUNT; index++) {
        const LsmPresentationGridPosition position =
            lsm_memory_detail_position((LsmMemoryDetailField)index);
        const int row = (int)position.row;
        RECT pair = {
            info_left,
            details.top + 16 + row * 29,
            info_left + info_width,
            details.top + 16 + (row + 1) * 29
        };
        draw_detail_pair(
            state, dc, pair,
            hardware_names[index], hardware_values[index],
            hardware_label_width);
    }
}

static bool build_device_performance_view(
    const LsmWindowsUiState *state, LsmPageType type, size_t device_index,
    LsmDevicePerformanceView *view)
{
    if (!state || !view) return false;

    switch (type) {
        case LSM_PAGE_DISK:
            lsm_disk_performance_view(
                state->monitor_ready &&
                        device_index < state->monitor.disk_count
                    ? &state->monitor.disks[device_index] : NULL,
                device_index, view);
            return true;
        case LSM_PAGE_NETWORK:
            lsm_network_performance_view(
                state->monitor_ready &&
                        device_index < state->monitor.net_count
                    ? &state->monitor.nets[device_index] : NULL,
                device_index, false, view);
            return true;
        case LSM_PAGE_GPU:
            lsm_gpu_performance_view(
                state->monitor_ready &&
                        device_index < state->monitor.gpu_count
                    ? &state->monitor.gpus[device_index] : NULL,
                device_index, view);
            return true;
        default:
            return false;
    }
}

static void draw_device_performance_page(
    LsmWindowsUiState *state, HDC dc, RECT content,
    const LsmDevicePerformanceView *view, LsmPageType type)
{
    if (!state || !view) return;

    wchar_t title[LSM_PERFORMANCE_VIEW_VALUE_LEN];
    wchar_t subtitle[LSM_PERFORMANCE_VIEW_RAIL_LEN];
    text_to_wide(
        view->title, title, sizeof(title) / sizeof(title[0]));
    text_to_wide(
        view->subtitle, subtitle, sizeof(subtitle) / sizeof(subtitle[0]));

    RECT header = {
        content.left, content.top,
        content.right, content.top + 64
    };
    draw_round_panel(
        dc, &header, state->palette.card,
        state->palette.border, (int)state->design->card_radius);

    RECT title_rect = {
        header.left + 14, header.top + 6,
        header.right - 14, header.top + 34
    };
    RECT subtitle_rect = {
        header.left + 14, header.top + 34,
        header.right - 14, header.bottom - 6
    };
    draw_text(
        dc, title, title_rect, state->title_font,
        state->palette.heading,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    draw_text(
        dc, subtitle, subtitle_rect, state->body_font,
        state->palette.summary,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT metrics = {
        content.left, header.bottom + 10,
        content.right, content.bottom
    };
    draw_round_panel(
        dc, &metrics, state->palette.card,
        state->palette.border, (int)state->design->card_radius);

    const int pad = 16;
    const int gap = 18;
    const int column_width =
        (metrics.right - metrics.left - (pad * 2) - gap) / 2;
    const int row_height = 54;

    for (size_t index = 0U; index < view->metric_count; index++) {
        wchar_t label[LSM_DEVICE_PERFORMANCE_LABEL_LEN];
        wchar_t value[LSM_PERFORMANCE_VIEW_VALUE_LEN];
        text_to_wide(
            view->metric_labels[index], label,
            sizeof(label) / sizeof(label[0]));
        text_to_wide(
            view->metric_values[index], value,
            sizeof(value) / sizeof(value[0]));

        const int column = (int)(index % 2U);
        const int row = (int)(index / 2U);
        RECT block = {
            metrics.left + pad + column * (column_width + gap),
            metrics.top + 14 + row * row_height,
            metrics.left + pad + column * (column_width + gap) +
                column_width,
            metrics.top + 14 + (row + 1) * row_height
        };
        if (block.bottom > metrics.bottom - 8)
            break;
        draw_metric_block(state, dc, block, label, value);
    }

    if (view->metric_count == 0U) {
        wchar_t resource[64];
        wchar_t message_text[192];
        text_to_wide(
            lsm_performance_page_title(type), resource,
            sizeof(resource) / sizeof(resource[0]));
        (void)swprintf(
            message_text,
            sizeof(message_text) / sizeof(message_text[0]),
            L"No native %ls telemetry is currently available.",
            resource);
        RECT message = {
            metrics.left + 18, metrics.top + 18,
            metrics.right - 18, metrics.top + 72
        };
        draw_text(
            dc, message_text,
            message, state->body_font, state->palette.summary,
            DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
}

static void draw_performance_page(
    LsmWindowsUiState *state, HDC dc, RECT content)
{
    RECT rail = {
        content.left, content.top,
        content.left + LSM_SIDEBAR_WIDTH,
        content.bottom
    };
    fill_solid(dc, &rail, state->palette.panel);
    memset(
        state->performance_items, 0,
        sizeof(state->performance_items));
    state->performance_item_count = 0U;

    LsmCpuPerformanceView cpu_view;
    LsmMemoryPerformanceView memory_view;
    lsm_cpu_performance_view(
        state->monitor_ready ? &state->monitor : NULL, &cpu_view);
    lsm_memory_performance_view(
        state->monitor_ready ? &state->monitor : NULL, &memory_view);

    const size_t disk_count =
        state->monitor_ready ? state->monitor.disk_count : 0U;
    const size_t net_count =
        state->monitor_ready ? state->monitor.net_count : 0U;
    const size_t gpu_count =
        state->monitor_ready ? state->monitor.gpu_count : 0U;
    const size_t item_count =
        2U + disk_count + net_count + gpu_count;
    const int available_height =
        (rail.bottom - rail.top) - 16;
    const int total_height = item_count > 0U
        ? (int)item_count * performance_item_stride(state) -
            (int)state->design->compact_spacing
        : 0;
    const int maximum_scroll =
        total_height > available_height
            ? total_height - available_height : 0;
    if (state->performance_scroll_y < 0)
        state->performance_scroll_y = 0;
    if (state->performance_scroll_y > maximum_scroll)
        state->performance_scroll_y = maximum_scroll;

    const int saved_dc = SaveDC(dc);
    IntersectClipRect(
        dc, rail.left, rail.top, rail.right, rail.bottom);

    size_t slot = 0U;
    int top = rail.top + 8 - state->performance_scroll_y;

#define DRAW_WINDOWS_PERFORMANCE_ITEM(resource_type, device_index, title_text, value_text, history_ptr) \
    do { \
        if (slot < LSM_WINDOWS_MAX_PERFORMANCE_ITEMS) { \
            wchar_t item_title[LSM_PERFORMANCE_VIEW_VALUE_LEN]; \
            wchar_t item_value[LSM_PERFORMANCE_VIEW_RAIL_LEN]; \
            text_to_wide( \
                (title_text), item_title, \
                sizeof(item_title) / sizeof(item_title[0])); \
            text_to_wide( \
                (value_text), item_value, \
                sizeof(item_value) / sizeof(item_value[0])); \
            RECT item = { \
                rail.left + 4, top, \
                rail.left + 4 + LSM_SIDE_BUTTON_WIDTH, \
                top + LSM_SIDE_BUTTON_HEIGHT \
            }; \
            draw_performance_rail_item( \
                state, dc, slot, (resource_type), (device_index), \
                item, item_title, item_value, (history_ptr), \
                performance_colour_ref((resource_type))); \
            (void)IntersectRect( \
                &state->performance_items[slot].rect, &item, &rail); \
            slot++; \
            top += performance_item_stride(state); \
        } \
    } while (0)

    DRAW_WINDOWS_PERFORMANCE_ITEM(
        LSM_PAGE_CPU, 0U,
        lsm_performance_page_title(LSM_PAGE_CPU),
        state->monitor_ready ? cpu_view.rail_value : "Initialising...",
        state->cpu_history);
    DRAW_WINDOWS_PERFORMANCE_ITEM(
        LSM_PAGE_MEMORY, 0U,
        lsm_performance_page_title(LSM_PAGE_MEMORY),
        state->monitor_ready ? memory_view.rail_value : "Initialising...",
        state->memory_history);

    for (size_t index = 0U; index < disk_count; index++) {
        LsmDevicePerformanceView view;
        (void)build_device_performance_view(
            state, LSM_PAGE_DISK, index, &view);
        DRAW_WINDOWS_PERFORMANCE_ITEM(
            LSM_PAGE_DISK, index,
            view.title, view.rail_value, NULL);
    }
    for (size_t index = 0U; index < net_count; index++) {
        LsmDevicePerformanceView view;
        (void)build_device_performance_view(
            state, LSM_PAGE_NETWORK, index, &view);
        DRAW_WINDOWS_PERFORMANCE_ITEM(
            LSM_PAGE_NETWORK, index,
            view.title, view.rail_value, NULL);
    }
    for (size_t index = 0U; index < gpu_count; index++) {
        LsmDevicePerformanceView view;
        (void)build_device_performance_view(
            state, LSM_PAGE_GPU, index, &view);
        DRAW_WINDOWS_PERFORMANCE_ITEM(
            LSM_PAGE_GPU, index,
            view.title, view.rail_value, NULL);
    }

#undef DRAW_WINDOWS_PERFORMANCE_ITEM

    state->performance_item_count = slot;
    if (saved_dc != 0)
        RestoreDC(dc, saved_dc);

    RECT separator = {
        rail.right, rail.top,
        rail.right + 1, rail.bottom
    };
    fill_solid(dc, &separator, state->palette.border);

    RECT page = {
        rail.right + (int)state->design->section_spacing,
        content.top,
        content.right,
        content.bottom
    };

    switch (state->active_performance_item) {
        case LSM_PAGE_CPU:
            draw_cpu_page(state, dc, page);
            break;
        case LSM_PAGE_MEMORY:
            draw_memory_page(state, dc, page);
            break;
        case LSM_PAGE_DISK:
        case LSM_PAGE_NETWORK:
        case LSM_PAGE_GPU: {
            LsmDevicePerformanceView view;
            (void)build_device_performance_view(
                state, state->active_performance_item,
                state->active_performance_index, &view);
            draw_device_performance_page(
                state, dc, page, &view,
                state->active_performance_item);
            break;
        }
        default: {
            LsmDevicePerformanceView unavailable;
            memset(&unavailable, 0, sizeof(unavailable));
            infiltratr_copy_string(
                unavailable.title, sizeof(unavailable.title),
                lsm_performance_page_title(
                    state->active_performance_item));
            infiltratr_copy_string(
                unavailable.subtitle, sizeof(unavailable.subtitle),
                "Native Windows collector not implemented yet");
            infiltratr_copy_string(
                unavailable.rail_value, sizeof(unavailable.rail_value),
                "N/A");
            draw_device_performance_page(
                state, dc, page, &unavailable,
                state->active_performance_item);
            break;
        }
    }
}

static void draw_placeholder_page(
    LsmWindowsUiState *state, HDC dc, RECT content)
{
    RECT title = {
        content.left, content.top,
        content.right, content.top + 54
    };
    wchar_t page_title[64];
    text_to_wide(
        lsm_tab_label(state->active_page),
        page_title, sizeof(page_title) / sizeof(page_title[0]));
    draw_text(
        dc, page_title, title,
        state->title_font, state->palette.heading,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT card = {
        content.left, content.top + 66,
        content.right, content.top + 190
    };
    draw_round_panel(
        dc, &card, state->palette.card,
        state->palette.border, (int)state->design->card_radius);

    RECT message = {
        card.left + 22, card.top + 22,
        card.right - 22, card.bottom - 22
    };
    draw_text(
        dc,
        L"This page is part of the Windows System Monitor shell. "
        L"Its native Windows backend is not implemented yet.",
        message, state->body_font, state->palette.summary,
        DT_LEFT | DT_TOP | DT_WORDBREAK);
}

static void draw_process_page_header(
    LsmWindowsUiState *state, HDC dc, RECT content)
{
    RECT title = {
        content.left, content.top,
        content.right, content.top + 40
    };
    draw_text(
        dc, L"Processes", title, state->title_font,
        state->palette.heading, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle = {
        content.left, content.top + 40,
        content.right, content.top + 68
    };
    draw_text(
        dc, L"Read-only native Windows process inventory",
        subtitle, state->body_font,
        state->palette.summary, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void draw_status_bar(
    LsmWindowsUiState *state, HDC dc, int width, int height)
{
    RECT line = {
        0, height - LSM_WINDOWS_STATUS_HEIGHT - 1,
        width, height - LSM_WINDOWS_STATUS_HEIGHT
    };
    fill_solid(dc, &line, state->palette.border);

    RECT status = {
        (int)state->design->screen_padding,
        height - LSM_WINDOWS_STATUS_HEIGHT,
        width - (int)state->design->screen_padding,
        height
    };
    draw_text(
        dc, state->status_text, status,
        state->body_font, state->palette.subtle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void paint_window(LsmWindowsUiState *state, HDC target)
{
    if (!state || !target || !state->window) return;

    RECT client;
    if (!GetClientRect(state->window, &client)) return;
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    if (!dc || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        return;
    }

    HGDIOBJ previous_bitmap = SelectObject(dc, bitmap);
    fill_solid(dc, &client, state->palette.background);
    draw_menu_strip(state, dc, width);
    draw_summary_bar(state, dc, width);
    draw_page_tabs(state, dc, width);

    const RECT content = content_rect_for_client(state, width, height);
    if (state->active_page == LSM_TAB_PERFORMANCE)
        draw_performance_page(state, dc, content);
    else if (state->active_page == LSM_TAB_PROCESSES)
        draw_process_page_header(state, dc, content);
    else
        draw_placeholder_page(state, dc, content);

    draw_status_bar(state, dc, width, height);
    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);

    SelectObject(dc, previous_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

static void apply_process_list_theme(LsmWindowsUiState *state)
{
    if (!state || !state->process_list) return;
    ListView_SetBkColor(state->process_list, state->palette.input);
    ListView_SetTextBkColor(state->process_list, state->palette.input);
    ListView_SetTextColor(state->process_list, state->palette.text);
    HWND header = ListView_GetHeader(state->process_list);
    if (header) InvalidateRect(header, NULL, TRUE);
    InvalidateRect(state->process_list, NULL, TRUE);
}

static void initialise_process_list(LsmWindowsUiState *state)
{
    if (!state || !state->process_list) return;

    SendMessageW(
        state->process_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    apply_process_list_theme(state);

    static const wchar_t *const headings[] = {
        L"Name", L"PID", L"CPU", L"Memory", L"User", L"Threads"
    };
    static const int widths[] = {300, 90, 90, 100, 190, 90};

    for (int index = 0; index < 6; index++) {
        LVCOLUMNW column;
        ZeroMemory(&column, sizeof(column));
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = (wchar_t *)headings[index];
        column.cx = widths[index];
        column.iSubItem = index;
        SendMessageW(
            state->process_list, LVM_INSERTCOLUMNW,
            (WPARAM)index, (LPARAM)&column);
    }
}

static bool install_font_resource(
    LsmWindowsUiState *state, int resource_id)
{
    if (!state || !state->instance ||
        state->font_resource_count >= LSM_WINDOWS_FONT_RESOURCE_COUNT)
        return false;

    HRSRC resource = FindResourceW(
        state->instance, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource) return false;

    const DWORD size = SizeofResource(state->instance, resource);
    HGLOBAL loaded = LoadResource(state->instance, resource);
    if (!loaded || size == 0U) return false;

    void *data = LockResource(loaded);
    if (!data) return false;

    DWORD fonts_added = 0U;
    HANDLE handle = AddFontMemResourceEx(
        data, size, NULL, &fonts_added);
    if (!handle || fonts_added == 0U) {
        if (handle) (void)RemoveFontMemResourceEx(handle);
        return false;
    }

    state->font_resources[state->font_resource_count++] = handle;
    return true;
}

static void remove_font_resources(LsmWindowsUiState *state)
{
    if (!state) return;
    while (state->font_resource_count > 0U) {
        const size_t index = --state->font_resource_count;
        HANDLE handle = state->font_resources[index];
        if (handle) (void)RemoveFontMemResourceEx(handle);
        state->font_resources[index] = NULL;
    }
}

static bool install_embedded_typography(LsmWindowsUiState *state)
{
    if (!state) return false;

    const int resources[] = {
        LSM_WINDOWS_FONT_UI_REGULAR,
        LSM_WINDOWS_FONT_UI_BOLD,
        LSM_WINDOWS_FONT_BRAND_REGULAR
    };
    for (size_t index = 0U;
         index < sizeof(resources) / sizeof(resources[0]); index++) {
        if (!install_font_resource(state, resources[index])) {
            remove_font_resources(state);
            return false;
        }
    }
    return true;
}

static HFONT create_font_exact(
    int height, int weight, const wchar_t *family)
{
    if (!family || !family[0]) return NULL;

    HFONT font = CreateFontW(
        height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
    if (!font) return NULL;

    HDC dc = GetDC(NULL);
    if (!dc) {
        DeleteObject(font);
        return NULL;
    }

    HGDIOBJ previous = SelectObject(dc, font);
    wchar_t resolved[LF_FACESIZE] = L"";
    (void)GetTextFaceW(
        dc, (int)(sizeof(resolved) / sizeof(resolved[0])), resolved);
    SelectObject(dc, previous);
    ReleaseDC(NULL, dc);

    if (!resolved[0] || lstrcmpiW(resolved, family) != 0) {
        DeleteObject(font);
        return NULL;
    }
    return font;
}

static bool create_fonts(LsmWindowsUiState *state)
{
    if (!state) return false;

    const InfiltratrTypography *typography = infiltratr_typography();
    if (!typography ||
        typography->abi_version != INFILTRATR_TYPOGRAPHY_ABI)
        return false;

    wchar_t ui_family[LF_FACESIZE];
    wchar_t brand_family[LF_FACESIZE];
    text_to_wide(
        typography->ui_family, ui_family,
        sizeof(ui_family) / sizeof(ui_family[0]));
    text_to_wide(
        typography->brand_family, brand_family,
        sizeof(brand_family) / sizeof(brand_family[0]));
    if (!ui_family[0] || !brand_family[0])
        return false;

    const int regular_weight =
        typography->ui_regular_weight <= (uint32_t)INT_MAX
            ? (int)typography->ui_regular_weight : FW_NORMAL;
    const int bold_weight =
        typography->ui_bold_weight <= (uint32_t)INT_MAX
            ? (int)typography->ui_bold_weight : FW_BOLD;
    const int brand_weight =
        typography->brand_weight <= (uint32_t)INT_MAX
            ? (int)typography->brand_weight : FW_NORMAL;

    state->body_font =
        create_font_exact(-17, regular_weight, ui_family);
    state->body_bold_font =
        create_font_exact(-17, bold_weight, ui_family);
    state->rail_value_font =
        create_font_exact(-15, regular_weight, ui_family);
    state->title_font =
        create_font_exact(-24, brand_weight, brand_family);
    state->heading_font =
        create_font_exact(-19, bold_weight, ui_family);
    state->metric_font =
        create_font_exact(-21, regular_weight, ui_family);

    return state->body_font && state->body_bold_font &&
        state->rail_value_font && state->title_font &&
        state->heading_font && state->metric_font;
}

static bool create_children(LsmWindowsUiState *state)
{
    if (!state || !state->window) return false;

    state->process_list = CreateWindowExW(
        0U, WC_LISTVIEWW, L"",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, state->window, NULL, state->instance, NULL);
    if (!state->process_list) return false;

    SendMessageW(
        state->process_list, WM_SETFONT,
        (WPARAM)state->body_font, TRUE);
    initialise_process_list(state);
    return true;
}

static void layout_process_list(LsmWindowsUiState *state)
{
    if (!state || !state->window || !state->process_list) return;

    RECT client;
    if (!GetClientRect(state->window, &client)) return;
    const RECT content =
        content_rect_for_client(client.right, client.bottom);

    const int top = content.top + 78;
    MoveWindow(
        state->process_list,
        content.left, top,
        content.right - content.left,
        content.bottom - top,
        TRUE);
}

static void update_process_visibility(LsmWindowsUiState *state)
{
    if (!state || !state->process_list) return;
    ShowWindow(
        state->process_list,
        state->active_page == LSM_TAB_PROCESSES
            ? SW_SHOW : SW_HIDE);
}

static void list_view_set_text(HWND list, int row, int column,
                               wchar_t *text)
{
    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.iSubItem = column;
    item.pszText = text;
    SendMessageW(list, LVM_SETITEMTEXTW, (WPARAM)row, (LPARAM)&item);
}

static void refresh_processes(LsmWindowsUiState *state)
{
    if (!state || !state->process_list) return;
    if (!state->process_backend) {
        set_status(state, L"Processes backend unavailable");
        return;
    }

    LsmProcessInfo *processes = NULL;
    const size_t count = lsm_process_scan(
        state->process_backend, &processes,
        LSM_PROCESS_SCAN_EXECUTABLE | LSM_PROCESS_SCAN_HANDLE_COUNT);
    if (count == 0U || !processes) {
        lsm_process_list_free(processes);
        set_status(state, L"No process snapshot available");
        return;
    }

    lsm_process_list_free(state->processes);
    state->processes = processes;
    state->process_count = count;

    SendMessageW(state->process_list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(state->process_list, LVM_DELETEALLITEMS, 0, 0);

    const size_t shown = count > (size_t)INT_MAX ? (size_t)INT_MAX : count;
    for (size_t index = 0U; index < shown; index++) {
        const LsmProcessInfo *process = &processes[index];
        wchar_t name[256];
        wchar_t user[128];
        wchar_t number[64];

        text_to_wide(
            process->name, name,
            sizeof(name) / sizeof(name[0]));
        text_to_wide(
            process->user, user,
            sizeof(user) / sizeof(user[0]));
        if (!name[0]) lstrcpyW(name, L"-");
        if (!user[0]) lstrcpyW(user, L"-");

        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = (int)index;
        item.iSubItem = 0;
        item.pszText = name;
        const LRESULT inserted = SendMessageW(
            state->process_list, LVM_INSERTITEMW, 0, (LPARAM)&item);
        if (inserted < 0) continue;

        (void)swprintf(
            number, sizeof(number) / sizeof(number[0]),
            L"%llu", (unsigned long long)process->pid);
        list_view_set_text(state->process_list, (int)index, 1, number);

        (void)swprintf(
            number, sizeof(number) / sizeof(number[0]),
            L"%.1f%%", process->cpu_percent);
        list_view_set_text(state->process_list, (int)index, 2, number);

        (void)swprintf(
            number, sizeof(number) / sizeof(number[0]),
            L"%.1f%%", process->memory_percent);
        list_view_set_text(state->process_list, (int)index, 3, number);

        list_view_set_text(state->process_list, (int)index, 4, user);

        (void)swprintf(
            number, sizeof(number) / sizeof(number[0]),
            L"%u", process->threads);
        list_view_set_text(state->process_list, (int)index, 5, number);
    }

    SendMessageW(state->process_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(state->process_list, NULL, TRUE);

    wchar_t status[160];
    (void)swprintf(
        status, sizeof(status) / sizeof(status[0]),
        L"%zu Windows processes - read-only", count);
    set_status(state, status);
}

static void set_performance_status(LsmWindowsUiState *state)
{
    if (!state) return;
    wchar_t resource[64];
    wchar_t status[128];
    text_to_wide(
        lsm_performance_page_title(state->active_performance_item),
        resource, sizeof(resource) / sizeof(resource[0]));
    if (state->active_performance_item == LSM_PAGE_DISK ||
        state->active_performance_item == LSM_PAGE_NETWORK ||
        state->active_performance_item == LSM_PAGE_GPU) {
        (void)swprintf(
            status, sizeof(status) / sizeof(status[0]),
            L"Performance - %ls %zu",
            resource, state->active_performance_index);
    } else {
        (void)swprintf(
            status, sizeof(status) / sizeof(status[0]),
            L"Performance - %ls", resource);
    }
    set_status(state, status);
}

static void validate_performance_selection(LsmWindowsUiState *state)
{
    if (!state || !state->monitor_ready) return;

    bool available = true;
    switch (state->active_performance_item) {
        case LSM_PAGE_DISK:
            available =
                state->active_performance_index < state->monitor.disk_count;
            break;
        case LSM_PAGE_NETWORK:
            available =
                state->active_performance_index < state->monitor.net_count;
            break;
        case LSM_PAGE_GPU:
            available =
                state->active_performance_index < state->monitor.gpu_count;
            break;
        case LSM_PAGE_CPU:
        case LSM_PAGE_MEMORY:
            state->active_performance_index = 0U;
            break;
        default:
            available = false;
            break;
    }

    if (!available) {
        state->active_performance_item = LSM_PAGE_CPU;
        state->active_performance_index = 0U;
    }
}

static void initialise_monitor_backend(LsmWindowsUiState *state)
{
    if (!state || state->monitor_initialised) return;

    state->monitor_initialised = true;
    state->monitor_ready = lsm_monitor_platform_init(&state->monitor);
    if (!state->monitor_ready)
        set_status(state, L"Performance backend unavailable");
}

static void initialise_process_backend(LsmWindowsUiState *state)
{
    if (!state || state->process_backend_attempted) return;

    state->process_backend_attempted = true;
    state->process_backend = lsm_process_backend_create();
    if (!state->process_backend)
        set_status(state, L"Processes backend unavailable");
}

static void refresh_active_page(LsmWindowsUiState *state)
{
    if (!state) return;

    if (state->active_page == LSM_TAB_PERFORMANCE) {
        initialise_monitor_backend(state);
        if (state->monitor_ready) {
            (void)lsm_monitor_platform_update(&state->monitor);
            append_history(state);
            validate_performance_selection(state);
        }
        set_performance_status(state);
        InvalidateRect(state->window, NULL, FALSE);
    } else if (state->active_page == LSM_TAB_PROCESSES) {
        initialise_process_backend(state);
        refresh_processes(state);
    }
}

static void show_page(LsmWindowsUiState *state, LsmTabIndex page)
{
    if (!state || page < LSM_TAB_PERFORMANCE ||
        page > LSM_TAB_FILESYSTEMS)
        return;

    state->active_page = page;
    update_process_visibility(state);
    layout_process_list(state);

    if (page == LSM_TAB_PERFORMANCE) {
        set_status(state, L"Performance - CPU");
        refresh_active_page(state);
    } else if (page == LSM_TAB_PROCESSES) {
        set_status(state, L"Processes - starting native backend");
        refresh_active_page(state);
    } else {
        wchar_t page_title[64];
        wchar_t message[160];
        text_to_wide(
            lsm_tab_label(page),
            page_title, sizeof(page_title) / sizeof(page_title[0]));
        (void)swprintf(
            message, sizeof(message) / sizeof(message[0]),
            L"%ls - Windows backend not implemented yet",
            page_title);
        set_status(state, message);
    }

    InvalidateRect(state->window, NULL, FALSE);
}

static void show_file_menu(LsmWindowsUiState *state)
{
    if (!state || !state->window) return;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, LSM_WINDOWS_ID_EXIT, L"E&xit");
    POINT point = {
        state->file_menu_rect.left,
        state->file_menu_rect.bottom
    };
    ClientToScreen(state->window, &point);
    TrackPopupMenu(
        menu, TPM_LEFTALIGN | TPM_TOPALIGN,
        point.x, point.y, 0, state->window, NULL);
    DestroyMenu(menu);
}

static void show_view_menu(LsmWindowsUiState *state)
{
    if (!state || !state->window) return;

    HMENU menu = CreatePopupMenu();
    HMENU theme = CreatePopupMenu();
    if (!menu || !theme) {
        if (theme) DestroyMenu(theme);
        if (menu) DestroyMenu(menu);
        return;
    }

    AppendMenuW(
        theme,
        MF_STRING |
            (state->theme_mode == INFILTRATR_THEME_SYSTEM
                ? MF_CHECKED : MF_UNCHECKED),
        LSM_WINDOWS_ID_THEME_SYSTEM, L"Follow system");
    AppendMenuW(
        theme,
        MF_STRING |
            (state->theme_mode == INFILTRATR_THEME_DAY
                ? MF_CHECKED : MF_UNCHECKED),
        LSM_WINDOWS_ID_THEME_DAY, L"Day");
    AppendMenuW(
        theme,
        MF_STRING |
            (state->theme_mode == INFILTRATR_THEME_NIGHT
                ? MF_CHECKED : MF_UNCHECKED),
        LSM_WINDOWS_ID_THEME_NIGHT, L"Night");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)theme, L"Theme");

    POINT point = {
        state->view_menu_rect.left,
        state->view_menu_rect.bottom
    };
    ClientToScreen(state->window, &point);
    TrackPopupMenu(
        menu, TPM_LEFTALIGN | TPM_TOPALIGN,
        point.x, point.y, 0, state->window, NULL);
    DestroyMenu(menu);
}

static void show_help_menu(LsmWindowsUiState *state)
{
    if (!state || !state->window) return;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, LSM_WINDOWS_ID_ABOUT, L"&About");
    POINT point = {
        state->help_menu_rect.left,
        state->help_menu_rect.bottom
    };
    ClientToScreen(state->window, &point);
    TrackPopupMenu(
        menu, TPM_LEFTALIGN | TPM_TOPALIGN,
        point.x, point.y, 0, state->window, NULL);
    DestroyMenu(menu);
}

static void destroy_state(LsmWindowsUiState *state)
{
    if (!state) return;

    if (state->window) KillTimer(state->window, LSM_WINDOWS_TIMER_ID);
    lsm_process_list_free(state->processes);
    state->processes = NULL;
    state->process_count = 0U;

    lsm_process_backend_destroy(state->process_backend);
    state->process_backend = NULL;

    if (state->monitor_ready)
        lsm_monitor_platform_destroy(&state->monitor);
    state->monitor_ready = false;
    state->monitor_initialised = false;

    if (state->body_font) DeleteObject(state->body_font);
    if (state->body_bold_font) DeleteObject(state->body_bold_font);
    if (state->rail_value_font) DeleteObject(state->rail_value_font);
    if (state->title_font) DeleteObject(state->title_font);
    if (state->heading_font) DeleteObject(state->heading_font);
    if (state->metric_font) DeleteObject(state->metric_font);
    state->body_font = NULL;
    state->body_bold_font = NULL;
    state->rail_value_font = NULL;
    state->title_font = NULL;
    state->heading_font = NULL;
    state->metric_font = NULL;
    remove_font_resources(state);
}

static void apply_titlebar_theme(LsmWindowsUiState *state)
{
    if (!state || !state->window) return;
    const BOOL enabled = theme_is_dark(state) ? TRUE : FALSE;
    (void)DwmSetWindowAttribute(
        state->window, 20U,
        &enabled, sizeof(enabled));
}

static LRESULT CALLBACK lsm_windows_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LsmWindowsUiState *state = window_state(window);

    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTW *creation = (CREATESTRUCTW *)lparam;
            state = (LsmWindowsUiState *)creation->lpCreateParams;
            if (!state) return FALSE;
            state->window = window;
            SetWindowLongPtrW(
                window, GWLP_USERDATA, (LONG_PTR)state);
            return TRUE;
        }

        case WM_CREATE:
            if (!state) return -1;
            if (!load_theme_preference(state)) {
                if (state->startup_smoke)
                    write_startup_smoke_status("theme_contract_failed\n");
                return -1;
            }
            if (!install_embedded_typography(state)) {
                if (state->startup_smoke)
                    write_startup_smoke_status("install_fonts_failed\n");
                return -1;
            }
            if (!create_fonts(state)) {
                if (state->startup_smoke)
                    write_startup_smoke_status("create_fonts_failed\n");
                return -1;
            }
            if (!create_children(state)) {
                if (state->startup_smoke)
                    write_startup_smoke_status("create_children_failed\n");
                return -1;
            }
            apply_process_list_theme(state);
            apply_titlebar_theme(state);
            state->active_page = LSM_TAB_PERFORMANCE;
            state->active_performance_item =
                LSM_PAGE_CPU;
            state->active_performance_index = 0U;
            state->performance_scroll_y = 0;
            state->hovered_tab = -1;
            state->hovered_performance_item = -1;
            state->hovered_menu = -1;
            set_status(state, L"Performance - starting native backend");
            update_process_visibility(state);
            SetTimer(
                window, LSM_WINDOWS_TIMER_ID,
                LSM_WINDOWS_REFRESH_MS, NULL);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            paint_window(state, dc);
            EndPaint(window, &paint);
            return 0;
        }

        case WM_SIZE:
            layout_process_list(state);
            InvalidateRect(window, NULL, FALSE);
            return 0;

        case WM_NOTIFY: {
            if (!state || !state->process_list) break;
            NMHDR *header = (NMHDR *)lparam;
            if (!header || header->code != NM_CUSTOMDRAW) break;

            if (header->hwndFrom == state->process_list) {
                NMLVCUSTOMDRAW *custom = (NMLVCUSTOMDRAW *)lparam;
                if (custom->nmcd.dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                if (custom->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    const int row = (int)custom->nmcd.dwItemSpec;
                    const UINT selected = ListView_GetItemState(
                        state->process_list, row, LVIS_SELECTED);
                    custom->clrText = selected
                        ? state->palette.title : state->palette.text;
                    custom->clrTextBk = selected
                        ? state->palette.selection : state->palette.input;
                    return CDRF_NEWFONT;
                }
            }

            HWND list_header = ListView_GetHeader(state->process_list);
            if (header->hwndFrom == list_header) {
                NMCUSTOMDRAW *custom = (NMCUSTOMDRAW *)lparam;
                if (custom->dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                if (custom->dwDrawStage == CDDS_ITEMPREPAINT) {
                    wchar_t text_buffer[128] = L"";
                    HDITEMW item;
                    ZeroMemory(&item, sizeof(item));
                    item.mask = HDI_TEXT;
                    item.pszText = text_buffer;
                    item.cchTextMax =
                        (int)(sizeof(text_buffer) / sizeof(text_buffer[0]));
                    (void)SendMessageW(
                        list_header, HDM_GETITEMW,
                        custom->dwItemSpec, (LPARAM)&item);

                    fill_solid(
                        custom->hdc, &custom->rc,
                        state->palette.panel);
                    RECT text_rect = custom->rc;
                    text_rect.left += 9;
                    text_rect.right -= 7;
                    draw_text(
                        custom->hdc, text_buffer, text_rect,
                        state->body_bold_font, state->palette.summary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                        DT_END_ELLIPSIS);
                    RECT bottom = {
                        custom->rc.left, custom->rc.bottom - 1,
                        custom->rc.right, custom->rc.bottom
                    };
                    fill_solid(
                        custom->hdc, &bottom,
                        state->palette.border);
                    return CDRF_SKIPDEFAULT;
                }
            }
            break;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *limits = (MINMAXINFO *)lparam;
            limits->ptMinTrackSize.x = 980;
            limits->ptMinTrackSize.y = 680;
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!state) break;
            if (!state->tracking_mouse_leave) {
                TRACKMOUSEEVENT tracking;
                ZeroMemory(&tracking, sizeof(tracking));
                tracking.cbSize = sizeof(tracking);
                tracking.dwFlags = TME_LEAVE;
                tracking.hwndTrack = window;
                if (TrackMouseEvent(&tracking))
                    state->tracking_mouse_leave = true;
            }

            POINT point = {
                GET_X_LPARAM(lparam),
                GET_Y_LPARAM(lparam)
            };
            int hovered_menu = -1;
            if (PtInRect(&state->file_menu_rect, point)) hovered_menu = 0;
            else if (PtInRect(&state->view_menu_rect, point)) hovered_menu = 1;
            else if (PtInRect(&state->help_menu_rect, point)) hovered_menu = 2;

            int hovered_tab = -1;
            for (int index = 0; index < LSM_TAB_COUNT; index++) {
                if (PtInRect(&state->page_tabs[index], point)) {
                    hovered_tab = index;
                    break;
                }
            }

            int hovered_performance = -1;
            if (state->active_page == LSM_TAB_PERFORMANCE) {
                for (size_t index = 0U;
                     index < state->performance_item_count; index++) {
                    if (PtInRect(
                            &state->performance_items[index].rect, point)) {
                        hovered_performance = (int)index;
                        break;
                    }
                }
            }

            if (hovered_menu != state->hovered_menu ||
                hovered_tab != state->hovered_tab ||
                hovered_performance != state->hovered_performance_item) {
                state->hovered_menu = hovered_menu;
                state->hovered_tab = hovered_tab;
                state->hovered_performance_item = hovered_performance;
                InvalidateRect(window, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (state) {
                state->tracking_mouse_leave = false;
                state->hovered_menu = -1;
                state->hovered_tab = -1;
                state->hovered_performance_item = -1;
                InvalidateRect(window, NULL, FALSE);
            }
            return 0;

        case WM_MOUSEWHEEL:
            if (state &&
                state->active_page == LSM_TAB_PERFORMANCE) {
                const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
                if (delta != 0) {
                    state->performance_scroll_y -=
                        (delta / WHEEL_DELTA) *
                        performance_item_stride(state);
                    if (state->performance_scroll_y < 0)
                        state->performance_scroll_y = 0;
                    InvalidateRect(window, NULL, FALSE);
                }
                return 0;
            }
            break;

        case WM_LBUTTONUP: {
            if (!state) break;
            POINT point = {
                GET_X_LPARAM(lparam),
                GET_Y_LPARAM(lparam)
            };
            if (PtInRect(&state->file_menu_rect, point)) {
                show_file_menu(state);
                return 0;
            }
            if (PtInRect(&state->view_menu_rect, point)) {
                show_view_menu(state);
                return 0;
            }
            if (PtInRect(&state->help_menu_rect, point)) {
                show_help_menu(state);
                return 0;
            }

            for (int index = 0;
                 index < LSM_TAB_COUNT; index++) {
                if (PtInRect(&state->page_tabs[index], point)) {
                    show_page(state, (LsmTabIndex)index);
                    return 0;
                }
            }

            if (state->active_page ==
                LSM_TAB_PERFORMANCE) {
                for (size_t index = 0U;
                     index < state->performance_item_count;
                     index++) {
                    const LsmWindowsPerformanceItem *item =
                        &state->performance_items[index];
                    if (PtInRect(&item->rect, point)) {
                        state->active_performance_item = item->type;
                        state->active_performance_index = item->index;
                        set_performance_status(state);
                        InvalidateRect(window, NULL, FALSE);
                        return 0;
                    }
                }
            }
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wparam) >= LSM_WINDOWS_ID_THEME_SYSTEM &&
                LOWORD(wparam) <= LSM_WINDOWS_ID_THEME_NIGHT) {
                state->theme_mode = (InfiltratrThemeMode)(
                    LOWORD(wparam) - LSM_WINDOWS_ID_THEME_SYSTEM);
                if (!resolve_theme(state)) return 0;
                save_theme_preference(state);
                apply_process_list_theme(state);
                apply_titlebar_theme(state);
                RedrawWindow(
                    window, NULL, NULL,
                    RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                return 0;
            }
            if (LOWORD(wparam) == LSM_WINDOWS_ID_EXIT) {
                DestroyWindow(window);
                return 0;
            }
            if (LOWORD(wparam) == LSM_WINDOWS_ID_ABOUT) {
                show_about_window(state);
                return 0;
            }
            break;

        case LSM_WINDOWS_MESSAGE_START_BACKEND:
            refresh_active_page(state);
            return 0;

        case WM_SETTINGCHANGE:
            if (state &&
                state->theme_mode == INFILTRATR_THEME_SYSTEM) {
                if (!resolve_theme(state)) break;
                apply_process_list_theme(state);
                apply_titlebar_theme(state);
                RedrawWindow(
                    window, NULL, NULL,
                    RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            }
            break;

        case WM_TIMER:
            if (wparam == LSM_WINDOWS_TIMER_ID) {
                refresh_active_page(state);
                return 0;
            }
            break;

        case WM_DESTROY:
            destroy_state(state);
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance,
                   LPSTR command_line, int show_command)
{
    (void)previous_instance;
    const bool startup_smoke =
        command_line && lstrcmpA(command_line, "--startup-smoke") == 0;
    if (startup_smoke)
        write_startup_smoke_status("entry\n");

    INITCOMMONCONTROLSEX common_controls;
    ZeroMemory(&common_controls, sizeof(common_controls));
    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&common_controls)) {
        if (startup_smoke)
            write_startup_smoke_status("common_controls_failed\n");
        return EXIT_FAILURE;
    }

    (void)SetProcessDPIAware();

    static const wchar_t class_name[] =
        L"InfiltratorSystemMonitorWindows";

    WNDCLASSEXW window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = lsm_windows_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    window_class.hIconSm = LoadIconW(NULL, IDI_APPLICATION);
    window_class.hbrBackground =
        (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszClassName = class_name;

    if (!RegisterClassExW(&window_class)) {
        if (startup_smoke) {
            write_startup_smoke_status("register_class_failed\n");
        } else {
            MessageBoxW(
                NULL, L"Unable to register the System Monitor window.",
                L"System Monitor", MB_OK | MB_ICONERROR);
        }
        return EXIT_FAILURE;
    }

    LsmWindowsUiState *state = calloc(1U, sizeof(*state));
    if (!state) {
        if (startup_smoke) {
            write_startup_smoke_status("state_allocation_failed\n");
        } else {
            MessageBoxW(
                NULL, L"Unable to allocate System Monitor application state.",
                L"System Monitor", MB_OK | MB_ICONERROR);
        }
        return EXIT_FAILURE;
    }
    state->instance = instance;
    state->startup_smoke = startup_smoke;
    state->design = infiltratr_design_metrics();
    if (!common_design_supported(state->design)) {
        if (startup_smoke) {
            write_startup_smoke_status("design_contract_failed\n");
        } else {
            MessageBoxW(
                NULL, L"Infiltratr Common design contract is unavailable.",
                L"System Monitor", MB_OK | MB_ICONERROR);
        }
        free(state);
        return EXIT_FAILURE;
    }
    state->theme_mode = INFILTRATR_THEME_NIGHT;
    if (!resolve_theme(state)) {
        if (startup_smoke) {
            write_startup_smoke_status("theme_contract_failed\n");
        } else {
            MessageBoxW(
                NULL, L"Infiltratr Common theme contract is unavailable.",
                L"System Monitor", MB_OK | MB_ICONERROR);
        }
        free(state);
        return EXIT_FAILURE;
    }
    state->hovered_tab = -1;
    state->hovered_performance_item = -1;
    state->hovered_menu = -1;

    HWND window = CreateWindowExW(
        0U, class_name, L"System Monitor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1180, 760,
        NULL, NULL, instance, state);
    if (!window) {
        if (startup_smoke) {
            write_startup_smoke_status("create_window_failed\n");
        } else {
            MessageBoxW(
                NULL, L"Unable to create the System Monitor window.",
                L"System Monitor", MB_OK | MB_ICONERROR);
        }
        destroy_state(state);
        free(state);
        return EXIT_FAILURE;
    }

    ShowWindow(
        window,
        show_command == SW_HIDE ? SW_SHOWNORMAL : show_command);
    UpdateWindow(window);

    if (startup_smoke) {
        const bool visible = IsWindowVisible(window) != FALSE;
        RECT client;
        const bool sized =
            GetClientRect(window, &client) != FALSE &&
            client.right > client.left &&
            client.bottom > client.top;
        if (!visible)
            write_startup_smoke_status("window_not_visible\n");
        else if (!sized)
            write_startup_smoke_status("client_rect_invalid\n");
        else
            write_startup_smoke_status("success\n");
        DestroyWindow(window);
        free(state);
        return visible && sized ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    PostMessageW(
        window, LSM_WINDOWS_MESSAGE_START_BACKEND, 0U, 0);

    MSG message;
    while (GetMessageW(&message, NULL, 0U, 0U) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    const int status = (int)message.wParam;
    free(state);
    return status;
}
