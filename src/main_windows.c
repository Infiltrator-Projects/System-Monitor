// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main_windows.c
 * @brief Native Win32 System Monitor presentation shell.
 *
 * Windows uses the same product hierarchy and Infiltratr design language as
 * the Linux application while keeping presentation native to Win32. Top-level
 * product pages live in the tab strip; Performance owns its own resource rail.
 * CPU, memory and process data are live, while unimplemented Windows backends
 * remain explicit placeholders.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_platform.h"
#include "process_backend.h"

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
#include <wchar.h>

#define LSM_WINDOWS_TIMER_ID 1U
#define LSM_WINDOWS_REFRESH_MS 1000U
#define LSM_WINDOWS_HISTORY_CAPACITY 120U
#define LSM_WINDOWS_PAGE_COUNT 8
#define LSM_WINDOWS_PERFORMANCE_ITEM_COUNT 2
#define LSM_WINDOWS_MENU_HEIGHT 32
#define LSM_WINDOWS_SUMMARY_HEIGHT 72
#define LSM_WINDOWS_TAB_HEIGHT 42
#define LSM_WINDOWS_STATUS_HEIGHT 30
#define LSM_WINDOWS_SCREEN_PADDING 20
#define LSM_WINDOWS_CONTENT_PADDING 16
#define LSM_WINDOWS_SECTION_SPACING 18
#define LSM_WINDOWS_CONTROL_SPACING 10
#define LSM_WINDOWS_CARD_RADIUS 12
#define LSM_WINDOWS_CONTROL_RADIUS 10
#define LSM_WINDOWS_RAIL_WIDTH 250
#define LSM_WINDOWS_RAIL_ITEM_HEIGHT 82
#define LSM_WINDOWS_MESSAGE_START_BACKEND (WM_APP + 1)

#define LSM_RGB_BACKGROUND RGB(0x05, 0x06, 0x08)
#define LSM_RGB_PANEL RGB(0x10, 0x13, 0x18)
#define LSM_RGB_CARD RGB(0x17, 0x1B, 0x20)
#define LSM_RGB_SURFACE RGB(0x0D, 0x10, 0x14)
#define LSM_RGB_INPUT RGB(0x0E, 0x11, 0x15)
#define LSM_RGB_BORDER RGB(0x35, 0x3A, 0x40)
#define LSM_RGB_TEXT RGB(0xE8, 0xEC, 0xEF)
#define LSM_RGB_TITLE RGB(0xEE, 0xF1, 0xF3)
#define LSM_RGB_MUTED RGB(0xAE, 0xB6, 0xBD)
#define LSM_RGB_SUBTLE RGB(0x89, 0x91, 0x98)
#define LSM_RGB_SELECTION RGB(0x2B, 0x31, 0x37)
#define LSM_RGB_ACCENT RGB(0x00, 0xAD, 0xEF)
#define LSM_RGB_SELECTED_SUMMARY RGB(0x79, 0xCA, 0xE8)
#define LSM_RGB_HEADING RGB(0xE7, 0xEB, 0xEE)
#define LSM_RGB_SUMMARY RGB(0x98, 0xA1, 0xA9)
#define LSM_RGB_DETAIL RGB(0x7E, 0x85, 0x8C)
#define LSM_RGB_CONNECTION RGB(0x0E, 0x11, 0x15)
#define LSM_RGB_CONNECTION_BORDER RGB(0x31, 0x36, 0x3B)
#define LSM_RGB_CARD_HOVER RGB(0x22, 0x27, 0x2D)

typedef enum {
    LSM_WINDOWS_PAGE_PERFORMANCE,
    LSM_WINDOWS_PAGE_PROCESSES,
    LSM_WINDOWS_PAGE_APP_HISTORY,
    LSM_WINDOWS_PAGE_STARTUP,
    LSM_WINDOWS_PAGE_USERS,
    LSM_WINDOWS_PAGE_DETAILS,
    LSM_WINDOWS_PAGE_SERVICES,
    LSM_WINDOWS_PAGE_FILESYSTEMS
} LsmWindowsPage;

typedef enum {
    LSM_WINDOWS_PERFORMANCE_CPU,
    LSM_WINDOWS_PERFORMANCE_MEMORY
} LsmWindowsPerformanceItem;

typedef struct {
    HINSTANCE instance;
    HWND window;
    HWND process_list;
    HFONT body_font;
    HFONT body_bold_font;
    HFONT title_font;
    HFONT heading_font;
    HFONT metric_font;
    LsmMonitor monitor;
    bool startup_smoke;
    bool monitor_initialised;
    bool monitor_ready;
    bool process_backend_attempted;
    LsmProcessBackend *process_backend;
    LsmProcessInfo *processes;
    size_t process_count;
    LsmWindowsPage active_page;
    LsmWindowsPerformanceItem active_performance_item;
    RECT page_tabs[LSM_WINDOWS_PAGE_COUNT];
    RECT performance_items[LSM_WINDOWS_PERFORMANCE_ITEM_COUNT];
    RECT file_menu_rect;
    RECT help_menu_rect;
    wchar_t status_text[256];
    double cpu_history[LSM_WINDOWS_HISTORY_CAPACITY];
    double memory_history[LSM_WINDOWS_HISTORY_CAPACITY];
    size_t history_count;
    size_t history_position;
} LsmWindowsUiState;

static const wchar_t *const page_names[LSM_WINDOWS_PAGE_COUNT] = {
    L"Performance",
    L"Processes",
    L"App History",
    L"Startup Apps",
    L"Users",
    L"Details",
    L"Services",
    L"File Systems"
};

enum {
    LSM_WINDOWS_ID_EXIT = 2000,
    LSM_WINDOWS_ID_ABOUT = 2001
};

static LRESULT CALLBACK lsm_windows_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam);
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance,
                   LPSTR command_line, int show_command);

static LsmWindowsUiState *window_state(HWND window)
{
    return (LsmWindowsUiState *)GetWindowLongPtrW(window, GWLP_USERDATA);
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

    int converted = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
        destination, (int)capacity);
    if (converted <= 0) {
        converted = MultiByteToWideChar(
            CP_ACP, 0U, source, -1, destination, (int)capacity);
    }
    if (converted <= 0) destination[0] = L'\0';
}

static double bytes_to_gb(uint64_t bytes)
{
    return (double)bytes / (1024.0 * 1024.0 * 1024.0);
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

static void format_percentage(wchar_t *buffer, size_t capacity, double value)
{
    if (!buffer || capacity == 0U) return;
    (void)swprintf(buffer, capacity, L"%.0f%%", value);
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

static void draw_history_graph(HDC dc, RECT rect, const double *history,
                               size_t count, size_t position)
{
    draw_round_panel(
        dc, &rect, LSM_RGB_SURFACE,
        LSM_RGB_CONNECTION_BORDER, LSM_WINDOWS_CARD_RADIUS);

    RECT inner = {
        rect.left + 14, rect.top + 14,
        rect.right - 14, rect.bottom - 14
    };
    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(0x24, 0x2A, 0x30));
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

    HPEN graph_pen = CreatePen(PS_SOLID, 2, LSM_RGB_ACCENT);
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
    fill_solid(dc, &menu, LSM_RGB_PANEL);

    RECT divider = {
        0, LSM_WINDOWS_MENU_HEIGHT - 1,
        width, LSM_WINDOWS_MENU_HEIGHT
    };
    fill_solid(dc, &divider, LSM_RGB_BORDER);

    SetRect(&state->file_menu_rect, 16, 0, 62, LSM_WINDOWS_MENU_HEIGHT);
    SetRect(&state->help_menu_rect, 68, 0, 120, LSM_WINDOWS_MENU_HEIGHT);
    draw_text(
        dc, L"File", state->file_menu_rect, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(
        dc, L"Help", state->help_menu_rect, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void draw_summary_bar(LsmWindowsUiState *state, HDC dc, int width)
{
    RECT bar = {
        LSM_WINDOWS_SCREEN_PADDING,
        LSM_WINDOWS_MENU_HEIGHT + LSM_WINDOWS_CONTROL_SPACING,
        width - LSM_WINDOWS_SCREEN_PADDING,
        LSM_WINDOWS_MENU_HEIGHT + LSM_WINDOWS_CONTROL_SPACING +
            LSM_WINDOWS_SUMMARY_HEIGHT
    };
    draw_round_panel(
        dc, &bar, LSM_RGB_CONNECTION,
        LSM_RGB_CONNECTION_BORDER, LSM_WINDOWS_CARD_RADIUS);

    static const wchar_t *const captions[] = {
        L"CPU", L"Memory", L"Disk", L"Network", L"GPU"
    };
    wchar_t values[5][64] = {
        L"N/A", L"N/A", L"N/A", L"N/A", L"N/A"
    };
    if (state->monitor_ready) {
        format_percentage(values[0], 64U, state->monitor.cpu.usage_percent);
        format_percentage(values[1], 64U, state->monitor.memory.usage_percent);
    }

    const int item_width = (bar.right - bar.left) / 5;
    for (int index = 0; index < 5; index++) {
        RECT caption = {
            bar.left + index * item_width,
            bar.top + 10,
            bar.left + (index + 1) * item_width,
            bar.top + 30
        };
        RECT value = {
            caption.left, bar.top + 29,
            caption.right, bar.bottom - 8
        };
        draw_text(
            dc, captions[index], caption, state->body_font,
            LSM_RGB_SUMMARY, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        draw_text(
            dc, values[index], value, state->body_bold_font,
            LSM_RGB_HEADING, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_page_tabs(LsmWindowsUiState *state, HDC dc, int width)
{
    const int top =
        LSM_WINDOWS_MENU_HEIGHT + LSM_WINDOWS_CONTROL_SPACING +
        LSM_WINDOWS_SUMMARY_HEIGHT + LSM_WINDOWS_CONTROL_SPACING;
    RECT strip = {0, top, width, top + LSM_WINDOWS_TAB_HEIGHT};
    fill_solid(dc, &strip, LSM_RGB_PANEL);

    const int left = LSM_WINDOWS_SCREEN_PADDING;
    const int available = width - (2 * LSM_WINDOWS_SCREEN_PADDING);
    const int tab_width = available / LSM_WINDOWS_PAGE_COUNT;
    for (int index = 0; index < LSM_WINDOWS_PAGE_COUNT; index++) {
        RECT tab = {
            left + index * tab_width,
            top,
            index == LSM_WINDOWS_PAGE_COUNT - 1
                ? width - LSM_WINDOWS_SCREEN_PADDING
                : left + (index + 1) * tab_width,
            top + LSM_WINDOWS_TAB_HEIGHT
        };
        state->page_tabs[index] = tab;

        const bool active = index == (int)state->active_page;
        if (active) {
            RECT underline = {
                tab.left + 8, tab.bottom - 3,
                tab.right - 8, tab.bottom
            };
            fill_solid(dc, &underline, LSM_RGB_ACCENT);
        }
        draw_text(
            dc, page_names[index], tab,
            active ? state->body_bold_font : state->body_font,
            active ? LSM_RGB_ACCENT : LSM_RGB_SUMMARY,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    RECT divider = {
        0, strip.bottom - 1, width, strip.bottom
    };
    fill_solid(dc, &divider, LSM_RGB_BORDER);
}

static RECT content_rect_for_client(int width, int height)
{
    const int top =
        LSM_WINDOWS_MENU_HEIGHT + LSM_WINDOWS_CONTROL_SPACING +
        LSM_WINDOWS_SUMMARY_HEIGHT + LSM_WINDOWS_CONTROL_SPACING +
        LSM_WINDOWS_TAB_HEIGHT + LSM_WINDOWS_SECTION_SPACING;
    RECT rect = {
        LSM_WINDOWS_SCREEN_PADDING,
        top,
        width - LSM_WINDOWS_SCREEN_PADDING,
        height - LSM_WINDOWS_STATUS_HEIGHT -
            LSM_WINDOWS_SCREEN_PADDING
    };
    return rect;
}

static void draw_performance_rail_item(
    LsmWindowsUiState *state, HDC dc, int index, RECT rect,
    const wchar_t *title, const wchar_t *value)
{
    state->performance_items[index] = rect;
    const bool active = index == (int)state->active_performance_item;
    draw_round_panel(
        dc, &rect,
        active ? LSM_RGB_SELECTION : LSM_RGB_PANEL,
        active ? LSM_RGB_ACCENT : LSM_RGB_PANEL,
        LSM_WINDOWS_CONTROL_RADIUS);

    RECT title_rect = {
        rect.left + 14, rect.top + 12,
        rect.right - 12, rect.top + 37
    };
    RECT value_rect = {
        rect.left + 14, rect.top + 39,
        rect.right - 12, rect.bottom - 9
    };
    draw_text(
        dc, title, title_rect, state->body_bold_font,
        active ? LSM_RGB_ACCENT : LSM_RGB_TEXT,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(
        dc, value, value_rect, state->body_font,
        active ? LSM_RGB_SELECTED_SUMMARY : LSM_RGB_SUMMARY,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void draw_cpu_page(LsmWindowsUiState *state, HDC dc, RECT content)
{
    wchar_t model[256] = L"Unavailable";
    wchar_t utilisation[64] = L"N/A";
    wchar_t details[256] = L"Telemetry unavailable";
    wchar_t uptime[128] = L"";
    if (state->monitor_ready) {
        text_to_wide(
            state->monitor.cpu.model, model,
            sizeof(model) / sizeof(model[0]));
        if (!model[0]) lstrcpyW(model, L"Unavailable");
        format_percentage(
            utilisation,
            sizeof(utilisation) / sizeof(utilisation[0]),
            state->monitor.cpu.usage_percent);
        (void)swprintf(
            details, sizeof(details) / sizeof(details[0]),
            L"User %.1f%%   Kernel %.1f%%   Logical processors %u",
            state->monitor.cpu.user_percent,
            state->monitor.cpu.kernel_percent,
            state->monitor.cpu.logical_cores);
        const uint64_t seconds = state->monitor.cpu.uptime_seconds;
        (void)swprintf(
            uptime, sizeof(uptime) / sizeof(uptime[0]),
            L"Processes %u   Threads %u   Handles %llu   Uptime %llu d %02llu:%02llu",
            state->monitor.cpu.process_count,
            state->monitor.cpu.thread_count,
            (unsigned long long)state->monitor.cpu.file_handle_count,
            (unsigned long long)(seconds / 86400ULL),
            (unsigned long long)((seconds % 86400ULL) / 3600ULL),
            (unsigned long long)((seconds % 3600ULL) / 60ULL));
    }

    RECT header = {
        content.left, content.top,
        content.right, content.top + 96
    };
    draw_round_panel(
        dc, &header, LSM_RGB_CARD,
        LSM_RGB_BORDER, LSM_WINDOWS_CARD_RADIUS);

    RECT title = {
        header.left + 18, header.top + 13,
        header.right - 18, header.top + 48
    };
    draw_text(
        dc, L"CPU", title, state->title_font,
        LSM_RGB_HEADING, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle = {
        header.left + 18, header.top + 50,
        header.right - 18, header.bottom - 10
    };
    draw_text(
        dc, model, subtitle, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE |
        DT_END_ELLIPSIS);

    RECT graph = {
        content.left, header.bottom + LSM_WINDOWS_CONTROL_SPACING,
        content.right, content.bottom - 122
    };
    draw_history_graph(
        dc, graph, state->cpu_history,
        state->history_count, state->history_position);

    RECT graph_caption = {
        graph.left + 18, graph.top + 12,
        graph.right - 18, graph.top + 38
    };
    draw_text(
        dc, L"Utilisation", graph_caption, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT graph_value = {
        graph.right - 180, graph.top + 8,
        graph.right - 18, graph.top + 52
    };
    draw_text(
        dc, utilisation, graph_value, state->metric_font,
        LSM_RGB_ACCENT, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    RECT details_card = {
        content.left, graph.bottom + LSM_WINDOWS_CONTROL_SPACING,
        content.right, content.bottom
    };
    draw_round_panel(
        dc, &details_card, LSM_RGB_CARD,
        LSM_RGB_BORDER, LSM_WINDOWS_CARD_RADIUS);
    RECT first = {
        details_card.left + 18, details_card.top + 16,
        details_card.right - 18, details_card.top + 46
    };
    RECT second = {
        details_card.left + 18, details_card.top + 51,
        details_card.right - 18, details_card.bottom - 12
    };
    draw_text(
        dc, details, first, state->body_bold_font,
        LSM_RGB_HEADING, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(
        dc, uptime, second, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void draw_memory_page(LsmWindowsUiState *state, HDC dc, RECT content)
{
    wchar_t usage[64] = L"N/A";
    wchar_t subtitle[256] = L"Physical memory telemetry unavailable";
    wchar_t row_one[256] = L"";
    wchar_t row_two[256] = L"";
    if (state->monitor_ready) {
        format_percentage(
            usage,
            sizeof(usage) / sizeof(usage[0]),
            state->monitor.memory.usage_percent);
        (void)swprintf(
            subtitle, sizeof(subtitle) / sizeof(subtitle[0]),
            L"%.2f GB in use of %.2f GB",
            bytes_to_gb(state->monitor.memory.used_bytes),
            bytes_to_gb(state->monitor.memory.total_bytes));
        (void)swprintf(
            row_one, sizeof(row_one) / sizeof(row_one[0]),
            L"Available %.2f GB   Cached %.2f GB",
            bytes_to_gb(state->monitor.memory.available_bytes),
            bytes_to_gb(state->monitor.memory.cached_bytes));
        (void)swprintf(
            row_two, sizeof(row_two) / sizeof(row_two[0]),
            L"Committed %.2f GB / %.2f GB",
            bytes_to_gb(state->monitor.memory.committed_bytes),
            bytes_to_gb(state->monitor.memory.commit_limit_bytes));
    }

    RECT header = {
        content.left, content.top,
        content.right, content.top + 96
    };
    draw_round_panel(
        dc, &header, LSM_RGB_CARD,
        LSM_RGB_BORDER, LSM_WINDOWS_CARD_RADIUS);
    RECT title = {
        header.left + 18, header.top + 13,
        header.right - 18, header.top + 48
    };
    draw_text(
        dc, L"Memory", title, state->title_font,
        LSM_RGB_HEADING, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT sub = {
        header.left + 18, header.top + 50,
        header.right - 18, header.bottom - 10
    };
    draw_text(
        dc, subtitle, sub, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT graph = {
        content.left, header.bottom + LSM_WINDOWS_CONTROL_SPACING,
        content.right, content.bottom - 122
    };
    draw_history_graph(
        dc, graph, state->memory_history,
        state->history_count, state->history_position);
    RECT graph_caption = {
        graph.left + 18, graph.top + 12,
        graph.right - 18, graph.top + 38
    };
    draw_text(
        dc, L"Memory usage", graph_caption, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT graph_value = {
        graph.right - 180, graph.top + 8,
        graph.right - 18, graph.top + 52
    };
    draw_text(
        dc, usage, graph_value, state->metric_font,
        LSM_RGB_ACCENT, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    RECT details_card = {
        content.left, graph.bottom + LSM_WINDOWS_CONTROL_SPACING,
        content.right, content.bottom
    };
    draw_round_panel(
        dc, &details_card, LSM_RGB_CARD,
        LSM_RGB_BORDER, LSM_WINDOWS_CARD_RADIUS);
    RECT first = {
        details_card.left + 18, details_card.top + 16,
        details_card.right - 18, details_card.top + 46
    };
    RECT second = {
        details_card.left + 18, details_card.top + 51,
        details_card.right - 18, details_card.bottom - 12
    };
    draw_text(
        dc, row_one, first, state->body_bold_font,
        LSM_RGB_HEADING, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(
        dc, row_two, second, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void draw_performance_page(
    LsmWindowsUiState *state, HDC dc, RECT content)
{
    RECT rail = {
        content.left, content.top,
        content.left + LSM_WINDOWS_RAIL_WIDTH,
        content.bottom
    };
    fill_solid(dc, &rail, LSM_RGB_PANEL);

    wchar_t cpu_value[96] = L"Initialising...";
    wchar_t memory_value[96] = L"Initialising...";
    if (state->monitor_ready) {
        (void)swprintf(
            cpu_value, sizeof(cpu_value) / sizeof(cpu_value[0]),
            L"%.0f%%  %u logical processors",
            state->monitor.cpu.usage_percent,
            state->monitor.cpu.logical_cores);
        (void)swprintf(
            memory_value, sizeof(memory_value) / sizeof(memory_value[0]),
            L"%.0f%%  %.1f / %.1f GB",
            state->monitor.memory.usage_percent,
            bytes_to_gb(state->monitor.memory.used_bytes),
            bytes_to_gb(state->monitor.memory.total_bytes));
    }

    RECT cpu = {
        rail.left + 7, rail.top + 8,
        rail.right - 7, rail.top + 8 + LSM_WINDOWS_RAIL_ITEM_HEIGHT
    };
    RECT memory = {
        cpu.left,
        cpu.bottom + 6,
        cpu.right,
        cpu.bottom + 6 + LSM_WINDOWS_RAIL_ITEM_HEIGHT
    };
    draw_performance_rail_item(
        state, dc, LSM_WINDOWS_PERFORMANCE_CPU,
        cpu, L"CPU", cpu_value);
    draw_performance_rail_item(
        state, dc, LSM_WINDOWS_PERFORMANCE_MEMORY,
        memory, L"Memory", memory_value);

    RECT separator = {
        rail.right, rail.top,
        rail.right + 1, rail.bottom
    };
    fill_solid(dc, &separator, LSM_RGB_BORDER);

    RECT page = {
        rail.right + LSM_WINDOWS_SECTION_SPACING,
        content.top,
        content.right,
        content.bottom
    };
    if (state->active_performance_item == LSM_WINDOWS_PERFORMANCE_CPU)
        draw_cpu_page(state, dc, page);
    else
        draw_memory_page(state, dc, page);
}

static void draw_placeholder_page(
    LsmWindowsUiState *state, HDC dc, RECT content)
{
    RECT title = {
        content.left, content.top,
        content.right, content.top + 54
    };
    draw_text(
        dc, page_names[state->active_page], title,
        state->title_font, LSM_RGB_HEADING,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT card = {
        content.left, content.top + 66,
        content.right, content.top + 190
    };
    draw_round_panel(
        dc, &card, LSM_RGB_CARD,
        LSM_RGB_BORDER, LSM_WINDOWS_CARD_RADIUS);

    RECT message = {
        card.left + 22, card.top + 22,
        card.right - 22, card.bottom - 22
    };
    draw_text(
        dc,
        L"This page is part of the Windows System Monitor shell. "
        L"Its native Windows backend is not implemented yet.",
        message, state->body_font, LSM_RGB_SUMMARY,
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
        LSM_RGB_HEADING, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle = {
        content.left, content.top + 40,
        content.right, content.top + 68
    };
    draw_text(
        dc, L"Read-only native Windows process inventory",
        subtitle, state->body_font,
        LSM_RGB_SUMMARY, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void draw_status_bar(
    LsmWindowsUiState *state, HDC dc, int width, int height)
{
    RECT line = {
        0, height - LSM_WINDOWS_STATUS_HEIGHT - 1,
        width, height - LSM_WINDOWS_STATUS_HEIGHT
    };
    fill_solid(dc, &line, LSM_RGB_BORDER);

    RECT status = {
        LSM_WINDOWS_SCREEN_PADDING,
        height - LSM_WINDOWS_STATUS_HEIGHT,
        width - LSM_WINDOWS_SCREEN_PADDING,
        height
    };
    draw_text(
        dc, state->status_text, status,
        state->body_font, LSM_RGB_SUBTLE,
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
    fill_solid(dc, &client, LSM_RGB_BACKGROUND);
    draw_menu_strip(state, dc, width);
    draw_summary_bar(state, dc, width);
    draw_page_tabs(state, dc, width);

    const RECT content = content_rect_for_client(width, height);
    if (state->active_page == LSM_WINDOWS_PAGE_PERFORMANCE)
        draw_performance_page(state, dc, content);
    else if (state->active_page == LSM_WINDOWS_PAGE_PROCESSES)
        draw_process_page_header(state, dc, content);
    else
        draw_placeholder_page(state, dc, content);

    draw_status_bar(state, dc, width, height);
    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);

    SelectObject(dc, previous_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

static void initialise_process_list(LsmWindowsUiState *state)
{
    if (!state || !state->process_list) return;

    SendMessageW(
        state->process_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    ListView_SetBkColor(state->process_list, LSM_RGB_INPUT);
    ListView_SetTextBkColor(state->process_list, LSM_RGB_INPUT);
    ListView_SetTextColor(state->process_list, LSM_RGB_TEXT);

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

static bool create_fonts(LsmWindowsUiState *state)
{
    if (!state) return false;

    state->body_font = CreateFontW(
        -17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"MB Corpo S Title WEB");
    state->body_bold_font = CreateFontW(
        -17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"MB Corpo S Title WEB");
    state->title_font = CreateFontW(
        -30, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"MB Corpo A Title Cond WEB");
    state->heading_font = CreateFontW(
        -21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"MB Corpo S Title WEB");
    state->metric_font = CreateFontW(
        -34, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"MB Corpo A Title Cond WEB");

    return state->body_font && state->body_bold_font &&
        state->title_font && state->heading_font &&
        state->metric_font;
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
        state->active_page == LSM_WINDOWS_PAGE_PROCESSES
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

    if (state->active_page == LSM_WINDOWS_PAGE_PERFORMANCE) {
        initialise_monitor_backend(state);
        if (state->monitor_ready) {
            (void)lsm_monitor_platform_update(&state->monitor);
            append_history(state);
        }
        set_status(
            state,
            state->active_performance_item == LSM_WINDOWS_PERFORMANCE_CPU
                ? L"Performance - CPU"
                : L"Performance - Memory");
        InvalidateRect(state->window, NULL, FALSE);
    } else if (state->active_page == LSM_WINDOWS_PAGE_PROCESSES) {
        initialise_process_backend(state);
        refresh_processes(state);
    }
}

static void show_page(LsmWindowsUiState *state, LsmWindowsPage page)
{
    if (!state || page < LSM_WINDOWS_PAGE_PERFORMANCE ||
        page > LSM_WINDOWS_PAGE_FILESYSTEMS)
        return;

    state->active_page = page;
    update_process_visibility(state);
    layout_process_list(state);

    if (page == LSM_WINDOWS_PAGE_PERFORMANCE) {
        set_status(state, L"Performance - CPU");
        refresh_active_page(state);
    } else if (page == LSM_WINDOWS_PAGE_PROCESSES) {
        set_status(state, L"Processes - starting native backend");
        refresh_active_page(state);
    } else {
        wchar_t message[160];
        (void)swprintf(
            message, sizeof(message) / sizeof(message[0]),
            L"%ls - Windows backend not implemented yet",
            page_names[page]);
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
    if (state->title_font) DeleteObject(state->title_font);
    if (state->heading_font) DeleteObject(state->heading_font);
    if (state->metric_font) DeleteObject(state->metric_font);
    state->body_font = NULL;
    state->body_bold_font = NULL;
    state->title_font = NULL;
    state->heading_font = NULL;
    state->metric_font = NULL;
}

static void apply_dark_titlebar(HWND window)
{
    if (!window) return;
    const BOOL enabled = TRUE;
    (void)DwmSetWindowAttribute(
        window, (DWMWINDOWATTRIBUTE)20,
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
            apply_dark_titlebar(window);
            state->active_page = LSM_WINDOWS_PAGE_PERFORMANCE;
            state->active_performance_item =
                LSM_WINDOWS_PERFORMANCE_CPU;
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

        case WM_GETMINMAXINFO: {
            MINMAXINFO *limits = (MINMAXINFO *)lparam;
            limits->ptMinTrackSize.x = 980;
            limits->ptMinTrackSize.y = 680;
            return 0;
        }

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
            if (PtInRect(&state->help_menu_rect, point)) {
                show_help_menu(state);
                return 0;
            }

            for (int index = 0;
                 index < LSM_WINDOWS_PAGE_COUNT; index++) {
                if (PtInRect(&state->page_tabs[index], point)) {
                    show_page(state, (LsmWindowsPage)index);
                    return 0;
                }
            }

            if (state->active_page ==
                LSM_WINDOWS_PAGE_PERFORMANCE) {
                for (int index = 0;
                     index < LSM_WINDOWS_PERFORMANCE_ITEM_COUNT;
                     index++) {
                    if (PtInRect(
                            &state->performance_items[index], point)) {
                        state->active_performance_item =
                            (LsmWindowsPerformanceItem)index;
                        set_status(
                            state,
                            index == LSM_WINDOWS_PERFORMANCE_CPU
                                ? L"Performance - CPU"
                                : L"Performance - Memory");
                        InvalidateRect(window, NULL, FALSE);
                        return 0;
                    }
                }
            }
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wparam) == LSM_WINDOWS_ID_EXIT) {
                DestroyWindow(window);
                return 0;
            }
            if (LOWORD(wparam) == LSM_WINDOWS_ID_ABOUT) {
                MessageBoxW(
                    window,
                    L"System Monitor 1.0.73\r\n\r\n"
                    L"Native Windows GUI preview using the Infiltratr "
                    L"Night palette and Linux product layout.\r\n"
                    L"CPU, memory and processes are live and read-only.",
                    L"About System Monitor",
                    MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            break;

        case LSM_WINDOWS_MESSAGE_START_BACKEND:
            refresh_active_page(state);
            return 0;

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
