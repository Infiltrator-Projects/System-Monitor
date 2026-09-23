// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main_windows.c
 * @brief Initial native Win32 System Monitor presentation shell.
 *
 * This is the first interactive Windows application front end. It deliberately
 * establishes the visible product shell before feature parity: Performance and
 * Processes consume the existing native Windows backends, while the remaining
 * product pages stay visible as explicit placeholders until their Windows
 * backends are implemented.
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
#include <commctrl.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#define LSM_WINDOWS_TIMER_ID 1U
#define LSM_WINDOWS_REFRESH_MS 1000U
#define LSM_WINDOWS_NAVIGATION_WIDTH 180
#define LSM_WINDOWS_MARGIN 18
#define LSM_WINDOWS_HEADER_HEIGHT 52
#define LSM_WINDOWS_STATUS_HEIGHT 28
#define LSM_WINDOWS_NAV_BUTTON_HEIGHT 36
#define LSM_WINDOWS_NAV_BUTTON_GAP 6
#define LSM_WINDOWS_MAX_PERFORMANCE_WIDGETS 16U

typedef enum {
    LSM_WINDOWS_PAGE_PERFORMANCE,
    LSM_WINDOWS_PAGE_PROCESSES,
    LSM_WINDOWS_PAGE_APP_HISTORY,
    LSM_WINDOWS_PAGE_STARTUP,
    LSM_WINDOWS_PAGE_USERS,
    LSM_WINDOWS_PAGE_DETAILS,
    LSM_WINDOWS_PAGE_SERVICES,
    LSM_WINDOWS_PAGE_FILESYSTEMS,
    LSM_WINDOWS_PAGE_COUNT
} LsmWindowsPage;

typedef struct {
    HINSTANCE instance;
    HWND window;
    HWND navigation[LSM_WINDOWS_PAGE_COUNT];
    HWND page_title;
    HWND status;
    HWND placeholder;
    HWND process_list;
    HWND performance_widgets[LSM_WINDOWS_MAX_PERFORMANCE_WIDGETS];
    size_t performance_widget_count;
    HWND cpu_model;
    HWND cpu_usage;
    HWND cpu_topology;
    HWND cpu_uptime;
    HWND cpu_totals;
    HWND memory_usage;
    HWND memory_available;
    HWND memory_commit;
    HWND memory_cached;
    HFONT body_font;
    HFONT title_font;
    HFONT section_font;
    LsmMonitor monitor;
    bool monitor_initialised;
    bool monitor_ready;
    bool process_backend_attempted;
    LsmProcessBackend *process_backend;
    LsmProcessInfo *processes;
    size_t process_count;
    LsmWindowsPage active_page;
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
    LSM_WINDOWS_ID_NAV_BASE = 1000,
    LSM_WINDOWS_ID_EXIT = 2000,
    LSM_WINDOWS_ID_ABOUT = 2001,
    LSM_WINDOWS_MESSAGE_START_BACKEND = WM_APP + 1
};

static LRESULT CALLBACK lsm_windows_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam);
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance,
                    PWSTR command_line, int show_command);

static LsmWindowsUiState *window_state(HWND window)
{
    return (LsmWindowsUiState *)GetWindowLongPtrW(window, GWLP_USERDATA);
}

static HWND create_control(LsmWindowsUiState *state, DWORD extended_style,
                           const wchar_t *class_name, const wchar_t *text,
                           DWORD style, int identifier)
{
    if (!state || !state->window) return NULL;
    return CreateWindowExW(
        extended_style, class_name, text, style | WS_CHILD,
        0, 0, 0, 0, state->window, (HMENU)(INT_PTR)identifier,
        state->instance, NULL);
}

static void apply_font(HWND control, HFONT font)
{
    if (!control || !font) return;
    SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static void remember_performance_widget(LsmWindowsUiState *state, HWND widget)
{
    if (!state || !widget ||
        state->performance_widget_count >= LSM_WINDOWS_MAX_PERFORMANCE_WIDGETS)
        return;
    state->performance_widgets[state->performance_widget_count++] = widget;
}

static HWND create_performance_label(LsmWindowsUiState *state)
{
    HWND label = create_control(
        state, 0U, L"STATIC", L"",
        WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0);
    apply_font(label, state ? state->body_font : NULL);
    remember_performance_widget(state, label);
    return label;
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
    if (converted <= 0)
        destination[0] = L'\0';
}

static double bytes_to_gb(uint64_t bytes)
{
    return (double)bytes / (1024.0 * 1024.0 * 1024.0);
}

static void set_status(LsmWindowsUiState *state, const wchar_t *text)
{
    if (!state || !state->status) return;
    SetWindowTextW(state->status, text ? text : L"");
}

static void update_performance(LsmWindowsUiState *state)
{
    if (!state) return;
    if (!state->monitor_ready) {
        SetWindowTextW(state->cpu_model, L"CPU: unavailable");
        SetWindowTextW(state->memory_usage, L"Memory: unavailable");
        set_status(state, L"Windows preview - monitoring backend unavailable");
        return;
    }

    wchar_t buffer[512];
    wchar_t model[256];
    text_to_wide(state->monitor.cpu.model, model,
                 sizeof(model) / sizeof(model[0]));
    if (!model[0])
        wcscpy(model, L"Unavailable");

    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"Model: %ls", model);
    SetWindowTextW(state->cpu_model, buffer);

    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"Utilisation: %.1f%%    User: %.1f%%    Kernel: %.1f%%",
        state->monitor.cpu.usage_percent,
        state->monitor.cpu.user_percent,
        state->monitor.cpu.kernel_percent);
    SetWindowTextW(state->cpu_usage, buffer);

    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"Logical processors: %u",
        state->monitor.cpu.logical_cores);
    SetWindowTextW(state->cpu_topology, buffer);

    const uint64_t uptime = state->monitor.cpu.uptime_seconds;
    const uint64_t days = uptime / 86400ULL;
    const uint64_t hours = (uptime % 86400ULL) / 3600ULL;
    const uint64_t minutes = (uptime % 3600ULL) / 60ULL;
    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"Uptime: %llu d %llu h %llu min",
        (unsigned long long)days,
        (unsigned long long)hours,
        (unsigned long long)minutes);
    SetWindowTextW(state->cpu_uptime, buffer);

    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"Processes: %u    Threads: %u    Handles: %llu",
        state->monitor.cpu.process_count,
        state->monitor.cpu.thread_count,
        (unsigned long long)state->monitor.cpu.file_handle_count);
    SetWindowTextW(state->cpu_totals, buffer);

    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"In use: %.2f GB / %.2f GB    %.1f%%",
        bytes_to_gb(state->monitor.memory.used_bytes),
        bytes_to_gb(state->monitor.memory.total_bytes),
        state->monitor.memory.usage_percent);
    SetWindowTextW(state->memory_usage, buffer);

    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"Available: %.2f GB",
        bytes_to_gb(state->monitor.memory.available_bytes));
    SetWindowTextW(state->memory_available, buffer);

    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"Committed: %.2f GB / %.2f GB",
        bytes_to_gb(state->monitor.memory.committed_bytes),
        bytes_to_gb(state->monitor.memory.commit_limit_bytes));
    SetWindowTextW(state->memory_commit, buffer);

    (void)swprintf(
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"Cached: %.2f GB",
        bytes_to_gb(state->monitor.memory.cached_bytes));
    SetWindowTextW(state->memory_cached, buffer);

    set_status(
        state,
        L"Windows preview - live CPU and memory telemetry - read-only");
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
        set_status(state, L"Windows preview - process backend unavailable");
        return;
    }

    LsmProcessInfo *processes = NULL;
    const size_t count = lsm_process_scan(
        state->process_backend, &processes,
        LSM_PROCESS_SCAN_EXECUTABLE | LSM_PROCESS_SCAN_HANDLE_COUNT);
    if (count == 0U || !processes) {
        lsm_process_list_free(processes);
        set_status(state, L"Windows preview - no process snapshot available");
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

        text_to_wide(process->name, name,
                     sizeof(name) / sizeof(name[0]));
        text_to_wide(process->user, user,
                     sizeof(user) / sizeof(user[0]));
        if (!name[0]) wcscpy(name, L"-");
        if (!user[0]) wcscpy(user, L"-");

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
        L"Windows preview - %zu processes - read-only", count);
    set_status(state, status);
}

static void layout_window(LsmWindowsUiState *state)
{
    if (!state || !state->window) return;

    RECT client;
    if (!GetClientRect(state->window, &client)) return;

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int nav_x = LSM_WINDOWS_MARGIN;
    const int nav_y = LSM_WINDOWS_MARGIN + LSM_WINDOWS_HEADER_HEIGHT;
    const int nav_width = LSM_WINDOWS_NAVIGATION_WIDTH - (2 * LSM_WINDOWS_MARGIN);
    const int content_x = LSM_WINDOWS_NAVIGATION_WIDTH + LSM_WINDOWS_MARGIN;
    const int content_width = width - content_x - LSM_WINDOWS_MARGIN;
    const int content_top = LSM_WINDOWS_MARGIN + LSM_WINDOWS_HEADER_HEIGHT;
    const int content_bottom =
        height - LSM_WINDOWS_STATUS_HEIGHT - LSM_WINDOWS_MARGIN;
    const int content_height = content_bottom - content_top;

    MoveWindow(
        state->page_title,
        content_x, LSM_WINDOWS_MARGIN,
        content_width, LSM_WINDOWS_HEADER_HEIGHT,
        TRUE);

    for (int index = 0; index < LSM_WINDOWS_PAGE_COUNT; index++) {
        const int y = nav_y +
            index * (LSM_WINDOWS_NAV_BUTTON_HEIGHT +
                     LSM_WINDOWS_NAV_BUTTON_GAP);
        MoveWindow(
            state->navigation[index],
            nav_x, y, nav_width, LSM_WINDOWS_NAV_BUTTON_HEIGHT,
            TRUE);
    }

    MoveWindow(
        state->status,
        LSM_WINDOWS_NAVIGATION_WIDTH + LSM_WINDOWS_MARGIN,
        height - LSM_WINDOWS_STATUS_HEIGHT - 4,
        content_width, LSM_WINDOWS_STATUS_HEIGHT,
        TRUE);

    MoveWindow(
        state->process_list,
        content_x, content_top,
        content_width, content_height,
        TRUE);

    MoveWindow(
        state->placeholder,
        content_x + 20, content_top + 40,
        content_width - 40, 120,
        TRUE);

    const int group_gap = 14;
    const int group_height = (content_height - group_gap) / 2;
    HWND cpu_group = state->performance_widgets[0];
    HWND memory_group = state->performance_widgets[6];

    MoveWindow(
        cpu_group,
        content_x, content_top,
        content_width, group_height,
        TRUE);
    MoveWindow(
        memory_group,
        content_x, content_top + group_height + group_gap,
        content_width, group_height,
        TRUE);

    const int label_x = content_x + 18;
    const int label_width = content_width - 36;
    const int label_height = 25;
    const int cpu_label_y = content_top + 34;
    const int memory_label_y =
        content_top + group_height + group_gap + 34;

    MoveWindow(state->cpu_model, label_x, cpu_label_y,
               label_width, label_height, TRUE);
    MoveWindow(state->cpu_usage, label_x, cpu_label_y + 30,
               label_width, label_height, TRUE);
    MoveWindow(state->cpu_topology, label_x, cpu_label_y + 60,
               label_width, label_height, TRUE);
    MoveWindow(state->cpu_uptime, label_x, cpu_label_y + 90,
               label_width, label_height, TRUE);
    MoveWindow(state->cpu_totals, label_x, cpu_label_y + 120,
               label_width, label_height, TRUE);

    MoveWindow(state->memory_usage, label_x, memory_label_y,
               label_width, label_height, TRUE);
    MoveWindow(state->memory_available, label_x, memory_label_y + 30,
               label_width, label_height, TRUE);
    MoveWindow(state->memory_commit, label_x, memory_label_y + 60,
               label_width, label_height, TRUE);
    MoveWindow(state->memory_cached, label_x, memory_label_y + 90,
               label_width, label_height, TRUE);
}

static void show_page(LsmWindowsUiState *state, LsmWindowsPage page)
{
    if (!state || page < 0 || page >= LSM_WINDOWS_PAGE_COUNT) return;

    state->active_page = page;
    SetWindowTextW(state->page_title, page_names[page]);

    for (int index = 0; index < LSM_WINDOWS_PAGE_COUNT; index++) {
        SendMessageW(
            state->navigation[index], BM_SETCHECK,
            index == (int)page ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    const bool performance = page == LSM_WINDOWS_PAGE_PERFORMANCE;
    const bool processes = page == LSM_WINDOWS_PAGE_PROCESSES;
    for (size_t index = 0U;
         index < state->performance_widget_count; index++) {
        ShowWindow(
            state->performance_widgets[index],
            performance ? SW_SHOW : SW_HIDE);
    }

    ShowWindow(
        state->process_list,
        processes ? SW_SHOW : SW_HIDE);

    const bool placeholder = !performance && !processes;
    ShowWindow(
        state->placeholder,
        placeholder ? SW_SHOW : SW_HIDE);

    if (placeholder) {
        wchar_t message[512];
        (void)swprintf(
            message, sizeof(message) / sizeof(message[0]),
            L"%ls\r\n\r\n"
            L"This page is part of the Windows GUI shell but its native "
            L"Windows backend is not implemented yet.",
            page_names[page]);
        SetWindowTextW(state->placeholder, message);
        set_status(
            state,
            L"Windows preview - page shell present - backend not implemented");
    } else if (performance) {
        update_performance(state);
    } else {
        refresh_processes(state);
    }

    layout_window(state);
}

static void initialise_process_list(HWND list)
{
    if (!list) return;

    SendMessageW(
        list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

    static const wchar_t *const headings[] = {
        L"Name", L"PID", L"CPU", L"Memory", L"User", L"Threads"
    };
    static const int widths[] = {260, 90, 90, 100, 180, 90};

    for (int index = 0; index < 6; index++) {
        LVCOLUMNW column;
        ZeroMemory(&column, sizeof(column));
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = (wchar_t *)headings[index];
        column.cx = widths[index];
        column.iSubItem = index;
        SendMessageW(
            list, LVM_INSERTCOLUMNW,
            (WPARAM)index, (LPARAM)&column);
    }
}

static HMENU create_application_menu(void)
{
    HMENU menu = CreateMenu();
    HMENU file_menu = CreatePopupMenu();
    HMENU help_menu = CreatePopupMenu();
    if (!menu || !file_menu || !help_menu) return menu;

    AppendMenuW(
        file_menu, MF_STRING, LSM_WINDOWS_ID_EXIT, L"E&xit");
    AppendMenuW(
        help_menu, MF_STRING, LSM_WINDOWS_ID_ABOUT, L"&About");
    AppendMenuW(
        menu, MF_POPUP, (UINT_PTR)file_menu, L"&File");
    AppendMenuW(
        menu, MF_POPUP, (UINT_PTR)help_menu, L"&Help");
    return menu;
}

static bool create_fonts(LsmWindowsUiState *state)
{
    if (!state) return false;

    state->body_font = CreateFontW(
        -17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    state->title_font = CreateFontW(
        -30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    state->section_font = CreateFontW(
        -18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return state->body_font && state->title_font && state->section_font;
}

static bool create_children(LsmWindowsUiState *state)
{
    if (!state) return false;

    state->page_title = create_control(
        state, 0U, L"STATIC", L"Performance",
        WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0);
    state->status = create_control(
        state, 0U, L"STATIC",
        L"Windows preview - starting",
        WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0);
    state->placeholder = create_control(
        state, 0U, L"STATIC", L"",
        SS_CENTER | SS_CENTERIMAGE | SS_NOPREFIX, 0);
    state->process_list = create_control(
        state, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0);

    if (!state->page_title || !state->status ||
        !state->placeholder || !state->process_list)
        return false;

    apply_font(state->page_title, state->title_font);
    apply_font(state->status, state->body_font);
    apply_font(state->placeholder, state->section_font);
    apply_font(state->process_list, state->body_font);
    initialise_process_list(state->process_list);

    for (int index = 0; index < LSM_WINDOWS_PAGE_COUNT; index++) {
        DWORD style = WS_VISIBLE | WS_TABSTOP |
            BS_AUTORADIOBUTTON | BS_PUSHLIKE;
        if (index == 0) style |= WS_GROUP;
        state->navigation[index] = create_control(
            state, 0U, L"BUTTON", page_names[index],
            style, LSM_WINDOWS_ID_NAV_BASE + index);
        if (!state->navigation[index]) return false;
        apply_font(state->navigation[index], state->body_font);
    }

    HWND cpu_group = create_control(
        state, 0U, L"BUTTON", L"CPU",
        WS_VISIBLE | BS_GROUPBOX, 0);
    apply_font(cpu_group, state->section_font);
    remember_performance_widget(state, cpu_group);
    state->cpu_model = create_performance_label(state);
    state->cpu_usage = create_performance_label(state);
    state->cpu_topology = create_performance_label(state);
    state->cpu_uptime = create_performance_label(state);
    state->cpu_totals = create_performance_label(state);

    HWND memory_group = create_control(
        state, 0U, L"BUTTON", L"Memory",
        WS_VISIBLE | BS_GROUPBOX, 0);
    apply_font(memory_group, state->section_font);
    remember_performance_widget(state, memory_group);
    state->memory_usage = create_performance_label(state);
    state->memory_available = create_performance_label(state);
    state->memory_commit = create_performance_label(state);
    state->memory_cached = create_performance_label(state);

    return state->performance_widget_count == 11U;
}

static void initialise_monitor_backend(LsmWindowsUiState *state)
{
    if (!state || state->monitor_initialised) return;

    state->monitor_initialised = true;
    state->monitor_ready = lsm_monitor_platform_init(&state->monitor);
    if (!state->monitor_ready)
        set_status(
            state,
            L"Windows preview - GUI ready - Performance backend unavailable");
}

static void initialise_process_backend(LsmWindowsUiState *state)
{
    if (!state || state->process_backend_attempted) return;

    state->process_backend_attempted = true;
    state->process_backend = lsm_process_backend_create();
    if (!state->process_backend)
        set_status(
            state,
            L"Windows preview - GUI ready - Processes backend unavailable");
}

static void destroy_state(LsmWindowsUiState *state)
{
    if (!state) return;

    KillTimer(state->window, LSM_WINDOWS_TIMER_ID);
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
    if (state->title_font) DeleteObject(state->title_font);
    if (state->section_font) DeleteObject(state->section_font);
    state->body_font = NULL;
    state->title_font = NULL;
    state->section_font = NULL;
}

static void refresh_active_page(LsmWindowsUiState *state)
{
    if (!state) return;

    if (state->active_page == LSM_WINDOWS_PAGE_PERFORMANCE) {
        initialise_monitor_backend(state);
        if (state->monitor_ready)
            (void)lsm_monitor_platform_update(&state->monitor);
        update_performance(state);
    } else if (state->active_page == LSM_WINDOWS_PAGE_PROCESSES) {
        initialise_process_backend(state);
        refresh_processes(state);
    }
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
            if (!state || !create_fonts(state) || !create_children(state))
                return -1;
            SetMenu(window, create_application_menu());
            state->active_page = LSM_WINDOWS_PAGE_PERFORMANCE;
            show_page(state, state->active_page);
            set_status(
                state,
                L"Windows preview - GUI ready - starting Performance backend");
            SetTimer(
                window, LSM_WINDOWS_TIMER_ID,
                LSM_WINDOWS_REFRESH_MS, NULL);
            return 0;

        case WM_SIZE:
            layout_window(state);
            return 0;

        case WM_GETMINMAXINFO: {
            MINMAXINFO *limits = (MINMAXINFO *)lparam;
            limits->ptMinTrackSize.x = 860;
            limits->ptMinTrackSize.y = 620;
            return 0;
        }

        case WM_COMMAND: {
            const int identifier = LOWORD(wparam);
            if (identifier >= LSM_WINDOWS_ID_NAV_BASE &&
                identifier < LSM_WINDOWS_ID_NAV_BASE +
                    LSM_WINDOWS_PAGE_COUNT) {
                show_page(
                    state,
                    (LsmWindowsPage)(
                        identifier - LSM_WINDOWS_ID_NAV_BASE));
                return 0;
            }
            if (identifier == LSM_WINDOWS_ID_EXIT) {
                DestroyWindow(window);
                return 0;
            }
            if (identifier == LSM_WINDOWS_ID_ABOUT) {
                MessageBoxW(
                    window,
                    L"System Monitor 1.0.72\r\n\r\n"
                    L"Initial native Windows GUI preview.\r\n"
                    L"Performance and Processes are live and read-only.\r\n"
                    L"Remaining pages are visible placeholders.",
                    L"About System Monitor",
                    MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            break;
        }

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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance,
                    PWSTR command_line, int show_command)
{
    (void)previous_instance;
    (void)command_line;

    INITCOMMONCONTROLSEX common_controls;
    ZeroMemory(&common_controls, sizeof(common_controls));
    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&common_controls))
        return EXIT_FAILURE;

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
    window_class.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    window_class.lpszClassName = class_name;

    if (!RegisterClassExW(&window_class)) {
        MessageBoxW(
            NULL, L"Unable to register the System Monitor window.",
            L"System Monitor", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    LsmWindowsUiState state;
    ZeroMemory(&state, sizeof(state));
    state.instance = instance;

    HWND window = CreateWindowExW(
        0U, class_name, L"System Monitor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1120, 720,
        NULL, NULL, instance, &state);
    if (!window) {
        MessageBoxW(
            NULL, L"Unable to create the System Monitor window.",
            L"System Monitor", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    ShowWindow(window, show_command == SW_HIDE ? SW_SHOWNORMAL : show_command);
    UpdateWindow(window);

    if (command_line && wcscmp(command_line, L"--startup-smoke") == 0) {
        const bool visible = IsWindowVisible(window) != FALSE;
        RECT client;
        const bool sized = GetClientRect(window, &client) != FALSE &&
            client.right > client.left && client.bottom > client.top;
        DestroyWindow(window);
        return visible && sized ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    PostMessageW(window, LSM_WINDOWS_MESSAGE_START_BACKEND, 0U, 0);

    MSG message;
    while (GetMessageW(&message, NULL, 0U, 0U) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return (int)message.wParam;
}
