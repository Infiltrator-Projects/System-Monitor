#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-shot mechanical refactor used only on the structural-coupling branch."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one occurrence, found {count}")
    return text.replace(old, new, 1)


def regex_once(text: str, pattern: str, replacement: str, label: str,
               flags: int = 0) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f"{label}: expected one regex match, found {count}")
    return updated


def find_function(text: str, name: str) -> tuple[str, int, int]:
    marker = name + "("
    pos = text.find(marker)
    if pos < 0:
        raise RuntimeError(f"unable to find function {name}")
    start = text.rfind("\nstatic ", 0, pos)
    if start < 0:
        raise RuntimeError(f"unable to find static declaration for {name}")
    start += 1
    brace = text.find("{", pos)
    if brace < 0:
        raise RuntimeError(f"unable to find body for {name}")

    depth = 0
    i = brace
    state = "code"
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if ch == '"':
                state = "string"
            elif ch == "'":
                state = "char"
            elif ch == "/" and nxt == "/":
                state = "line_comment"
                i += 1
            elif ch == "/" and nxt == "*":
                state = "block_comment"
                i += 1
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    end = i + 1
                    while end < len(text) and text[end] in " \t":
                        end += 1
                    if end < len(text) and text[end] == "\n":
                        end += 1
                    if end < len(text) and text[end] == "\n":
                        end += 1
                    return text[start:end], start, end
        elif state == "string":
            if ch == "\\":
                i += 1
            elif ch == '"':
                state = "code"
        elif state == "char":
            if ch == "\\":
                i += 1
            elif ch == "'":
                state = "code"
        elif state == "line_comment":
            if ch == "\n":
                state = "code"
        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 1
        i += 1
    raise RuntimeError(f"unterminated body for {name}")


def remove_functions(text: str, names: list[str]) -> tuple[str, dict[str, str]]:
    extracted: dict[str, str] = {}
    for name in names:
        segment, start, end = find_function(text, name)
        extracted[name] = segment
        text = text[:start] + text[end:]
    return text, extracted


def write_windows_theme() -> None:
    write("src/windows_theme.h", r'''// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_theme.h
 * @brief Native Windows projection of the shared Infiltratr design contract.
 *
 * Win32 theme persistence and Common palette projection are deliberately owned
 * here so the native shell does not also own design-contract interpretation.
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
    InfiltratrThemeMode mode;
    const InfiltratrDesignMetrics *design;
    LsmWindowsPalette palette;
} LsmWindowsTheme;

/** Validate the Common design ABI before native presentation consumes it. */
bool lsm_windows_theme_design_supported(const InfiltratrDesignMetrics *design);
/** Resolve the current theme mode into a native Win32 palette. */
bool lsm_windows_theme_resolve(LsmWindowsTheme *theme);
/** Load the persisted native theme preference and resolve its palette. */
bool lsm_windows_theme_load(LsmWindowsTheme *theme);
/** Persist the selected native theme mode. */
void lsm_windows_theme_save(const LsmWindowsTheme *theme);
/** Return whether the effective theme should use dark native chrome. */
bool lsm_windows_theme_is_dark(const LsmWindowsTheme *theme);

#endif
''')

    write("src/windows_theme.c", r'''// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_theme.c
 * @brief Native Windows theme persistence and Common palette projection.
 */
#include "windows_theme.h"

#include <limits.h>
#include <stddef.h>
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

static bool palette_supported(const InfiltratrThemePalette *palette)
{
    return palette &&
           palette->abi_version == INFILTRATR_THEME_PALETTE_ABI &&
           palette->struct_size >=
               offsetof(InfiltratrThemePalette, success_border_rgb) +
               sizeof(palette->success_border_rgb);
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
    if (!palette_supported(source)) return false;
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
''')


def write_windows_process_runtime(extracted: dict[str, str]) -> None:
    public = {
        "start_process_worker": "lsm_windows_process_runtime_start",
        "request_process_snapshot": "lsm_windows_process_runtime_request",
        "take_process_snapshot": "lsm_windows_process_runtime_take",
        "stop_process_worker": "lsm_windows_process_runtime_stop",
    }
    pieces: list[str] = []
    for name in ["process_worker_main", "start_process_worker",
                 "request_process_snapshot", "take_process_snapshot",
                 "stop_process_worker"]:
        segment = extracted[name].replace(
            "LsmWindowsUiState *state", "LsmWindowsProcessRuntime *state")
        if name in public:
            segment = segment.replace(name, public[name], 1)
            segment = re.sub(r"^static (bool|void) ", r"\1 ", segment, count=1)
        pieces.append(segment.rstrip() + "\n\n")

    write("src/windows_process_runtime.h", r'''// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_process_runtime.h
 * @brief Native process-worker lifetime contract for the Win32 shell.
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_WINDOWS_PROCESS_RUNTIME_H
#define INFILTRATOR_SYSTEM_MONITOR_WINDOWS_PROCESS_RUNTIME_H

#include "process_backend.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    LsmProcessBackend *process_backend;
    HANDLE process_thread;
    HANDLE process_event;
    CRITICAL_SECTION process_lock;
    bool process_lock_initialised;
    bool process_worker_abandoned;
    bool process_stop_requested;
    bool process_request_pending;
    unsigned process_requested_flags;
    LsmProcessInfo *process_completed;
    size_t process_completed_count;
    bool process_completed_ready;
} LsmWindowsProcessRuntime;

/** Start the native process worker if it is not already running. */
bool lsm_windows_process_runtime_start(LsmWindowsProcessRuntime *runtime);
/** Queue a process snapshot request without blocking the UI thread. */
void lsm_windows_process_runtime_request(LsmWindowsProcessRuntime *runtime,
                                         unsigned scan_flags);
/** Transfer ownership of a completed process snapshot to the caller. */
bool lsm_windows_process_runtime_take(LsmWindowsProcessRuntime *runtime,
                                      LsmProcessInfo **processes,
                                      size_t *count);
/** Stop the process worker; false means a native query could not be cancelled. */
bool lsm_windows_process_runtime_stop(LsmWindowsProcessRuntime *runtime);
/** Return whether the runtime currently owns a live worker thread. */
bool lsm_windows_process_runtime_available(
    const LsmWindowsProcessRuntime *runtime);
/** Return whether shutdown had to leave the worker storage alive until exit. */
bool lsm_windows_process_runtime_abandoned(
    const LsmWindowsProcessRuntime *runtime);

#endif
''')

    body = '''// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_process_runtime.c
 * @brief Native process-worker lifetime and snapshot handoff for Win32.
 */
#include "windows_process_runtime.h"

'''
    body += "".join(pieces)
    body += r'''bool lsm_windows_process_runtime_available(
    const LsmWindowsProcessRuntime *runtime)
{
    return runtime && runtime->process_thread &&
           runtime->process_lock_initialised;
}

bool lsm_windows_process_runtime_abandoned(
    const LsmWindowsProcessRuntime *runtime)
{
    return runtime && runtime->process_worker_abandoned;
}
'''
    write("src/windows_process_runtime.c", body)


def refactor_windows_main() -> None:
    path = "src/main_windows.c"
    text = read(path)
    text = replace_once(
        text, '#include "performance_view.h"\n',
        '#include "performance_view.h"\n#include "windows_process_runtime.h"\n#include "windows_theme.h"\n',
        "windows owner includes")

    text = regex_once(
        text,
        r"typedef struct \{\n    COLORREF background;.*?\n\} LsmWindowsPalette;\n\n",
        "", "remove local Windows palette", re.S)

    text = regex_once(
        text,
        r"    LsmProcessBackend \*process_backend; /\* Owned exclusively by process_thread\. \*/\n"
        r"    HANDLE process_thread;\n"
        r"    HANDLE process_event;\n"
        r"    CRITICAL_SECTION process_lock;\n"
        r"    bool process_lock_initialised;\n"
        r"    bool process_worker_abandoned;\n"
        r"    bool process_stop_requested;\n"
        r"    bool process_request_pending;\n"
        r"    unsigned process_requested_flags;\n"
        r"    LsmProcessInfo \*process_completed;\n"
        r"    size_t process_completed_count;\n"
        r"    bool process_completed_ready;\n",
        "    LsmWindowsProcessRuntime process_runtime;\n",
        "replace process worker storage")

    text = replace_once(
        text,
        "    InfiltratrThemeMode theme_mode;\n"
        "    const InfiltratrDesignMetrics *design;\n"
        "    LsmWindowsPalette palette;\n",
        "    LsmWindowsTheme theme;\n",
        "replace theme storage")

    process_names = [
        "process_worker_main", "start_process_worker",
        "request_process_snapshot", "take_process_snapshot",
        "stop_process_worker",
    ]
    text, process_functions = remove_functions(text, process_names)
    write_windows_process_runtime(process_functions)

    theme_names = [
        "colourref_from_common", "common_design_supported",
        "common_palette_supported", "project_common_palette",
        "system_prefers_dark", "theme_is_dark", "resolve_theme",
        "load_theme_preference", "save_theme_preference",
    ]
    text, _ = remove_functions(text, theme_names)
    write_windows_theme()

    text = text.replace("start_process_worker(state)",
                        "lsm_windows_process_runtime_start(&state->process_runtime)")
    text = text.replace("request_process_snapshot(state,",
                        "lsm_windows_process_runtime_request(&state->process_runtime,")
    text = text.replace("take_process_snapshot(state,",
                        "lsm_windows_process_runtime_take(&state->process_runtime,")
    text = text.replace("stop_process_worker(state)",
                        "lsm_windows_process_runtime_stop(&state->process_runtime)")
    text = text.replace(
        "if (!state || !state->process_thread)",
        "if (!state || !lsm_windows_process_runtime_available(&state->process_runtime))")
    text = text.replace(
        "if (!state->process_thread)",
        "if (!lsm_windows_process_runtime_available(&state->process_runtime))")
    text = text.replace(
        "if (!state->process_worker_abandoned)",
        "if (!lsm_windows_process_runtime_abandoned(&state->process_runtime))")
    text = text.replace(
        "    if (process_worker_stopped)\n        state->process_backend = NULL;\n",
        "    (void)process_worker_stopped;\n")

    text = text.replace("theme_is_dark(state)",
                        "lsm_windows_theme_is_dark(&state->theme)")
    text = text.replace("resolve_theme(state)",
                        "lsm_windows_theme_resolve(&state->theme)")
    text = text.replace("load_theme_preference(state)",
                        "lsm_windows_theme_load(&state->theme)")
    text = text.replace("save_theme_preference(state)",
                        "lsm_windows_theme_save(&state->theme)")
    text = text.replace("common_design_supported(state->design)",
                        "lsm_windows_theme_design_supported(state->theme.design)")
    text = text.replace("state->theme_mode", "state->theme.mode")
    text = text.replace("state->design", "state->theme.design")
    text = text.replace("state->palette", "state->theme.palette")

    forbidden = [
        "state->process_backend", "state->process_thread", "state->process_event",
        "state->process_lock", "state->process_worker_abandoned",
        "state->process_stop_requested", "state->process_request_pending",
        "state->process_requested_flags", "state->process_completed",
        "state->theme_mode", "state->design", "state->palette",
        "static bool resolve_theme", "static DWORD WINAPI process_worker_main",
    ]
    for marker in forbidden:
        if marker in text:
            raise RuntimeError(f"main_windows.c still reaches through extracted owner: {marker}")

    write(path, text)


def write_source_groups() -> None:
    write("support/source-groups/hardware-monitor-linux.txt", '''# SPDX-License-Identifier: GPL-3.0-or-later
monitor_hardware.c
monitor_bluetooth.c
hardware_topology.c
intel_gpu.c
npu_telemetry.c
monitor_battery.c
bluetooth_battery.c
bluetooth_traffic.c
linux_capability.c
logitech_hidpp.c
logitech_hidpp_protocol.c
memory_hardware.c
smbios_memory.c
nvml.c
mountinfo.c
storage_metadata.c
system_sources.c
pci_names.c
pci_names_data.c
''')
    write("support/source-groups/monitor-linux.txt", '''# SPDX-License-Identifier: GPL-3.0-or-later
monitor.c
monitor_backend_linux.c
refresh_policy.c
monitor_cpu_memory.c
cpu_accounting.c
memory_accounting.c
pressure.c
cpu_direct.c
monitor_storage_network.c
disk_accounting.c
wifi_metadata.c
monitor_hardware.c
monitor_bluetooth.c
hardware_topology.c
intel_gpu.c
npu_telemetry.c
monitor_battery.c
bluetooth_battery.c
bluetooth_traffic.c
linux_capability.c
logitech_hidpp.c
logitech_hidpp_protocol.c
memory_hardware.c
smbios_memory.c
nvml.c
mountinfo.c
storage_metadata.c
system_sources.c
pci_names.c
pci_names_data.c
''')
    write("support/source-groups/process-linux.txt", '''# SPDX-License-Identifier: GPL-3.0-or-later
process_model.c
process_backend_linux.c
process_gpu.c
''')
    write("support/source-groups/windows-product.txt", '''# SPDX-License-Identifier: GPL-3.0-or-later
main_windows.c
windows_theme.c
windows_process_runtime.c
presentation_contract.c
performance_view.c
overview_history.c
monitor_backend_windows.c
process_backend_windows.c
''')


def refactor_source_manifest() -> None:
    path = "support/sources.txt"
    text = read(path)
    text = replace_once(
        text, "main_windows.c\n", "main_windows.c\nwindows_theme.c\nwindows_process_runtime.c\n",
        "register Windows owners")
    write(path, text)


def refactor_makefile() -> None:
    path = "Makefile"
    text = read(path)
    text = text.replace(
        "SOURCE_NAMES := $(filter-out main_windows.c monitor_backend_%.c process_backend_%.c service_backend_%.c user_backend_%.c startup_backend_%.c,$(ALL_SOURCE_NAMES)) \\\n",
        "SOURCE_NAMES := $(filter-out main_windows.c windows_%.c monitor_backend_%.c process_backend_%.c service_backend_%.c user_backend_%.c startup_backend_%.c,$(ALL_SOURCE_NAMES)) \\\n")
    text = regex_once(
        text,
        r"HARDWARE_MONITOR_SOURCES := \\\n.*?PROCESS_SOURCES := \$\(PROCESS_CORE_SOURCES\) \$\(PROCESS_PLATFORM_SOURCES\)\n\n",
        "SOURCE_GROUP_DIR := support/source-groups\n"
        "SOURCE_GROUP_READ = $(shell sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$$/d' $(1))\n"
        "HARDWARE_MONITOR_SOURCES := $(addprefix src/,$(call SOURCE_GROUP_READ,$(SOURCE_GROUP_DIR)/hardware-monitor-linux.txt))\n"
        "MONITOR_SOURCES := $(addprefix src/,$(call SOURCE_GROUP_READ,$(SOURCE_GROUP_DIR)/monitor-linux.txt))\n"
        "PROCESS_SOURCES := $(addprefix src/,$(call SOURCE_GROUP_READ,$(SOURCE_GROUP_DIR)/process-linux.txt))\n\n",
        "centralise Make source groups", re.S)
    write(path, text)


def refactor_cmake() -> None:
    path = "CMakeLists.txt"
    text = read(path)
    text = replace_once(
        text,
        '    "${CMAKE_CURRENT_SOURCE_DIR}/support/sources.txt"\n',
        '    "${CMAKE_CURRENT_SOURCE_DIR}/support/sources.txt"\n'
        '    "${CMAKE_CURRENT_SOURCE_DIR}/support/source-groups/hardware-monitor-linux.txt"\n'
        '    "${CMAKE_CURRENT_SOURCE_DIR}/support/source-groups/monitor-linux.txt"\n'
        '    "${CMAKE_CURRENT_SOURCE_DIR}/support/source-groups/process-linux.txt"\n'
        '    "${CMAKE_CURRENT_SOURCE_DIR}/support/source-groups/windows-product.txt"\n',
        "CMake source group dependencies")
    text = replace_once(
        text,
        '    list(FILTER LSM_SOURCE_NAMES EXCLUDE REGEX "src/main_windows\\\\.c$")\n',
        '    list(FILTER LSM_SOURCE_NAMES EXCLUDE REGEX "src/(main_windows|windows_.*)\\\\.c$")\n',
        "exclude Windows owners from Linux CMake")
    text = regex_once(
        text,
        r"set\(LSM_MONITOR_CORE_SOURCES\n.*?set\(LSM_MONITOR_BACKEND_SOURCES\n    \$\{LSM_MONITOR_CORE_SOURCES\} \$\{LSM_MONITOR_PLATFORM_SOURCES\}\)\n",
        "function(lsm_read_source_group variable manifest)\n"
        "    file(STRINGS \"${CMAKE_CURRENT_SOURCE_DIR}/${manifest}\" group_entries)\n"
        "    list(FILTER group_entries EXCLUDE REGEX \"^[ \\t]*(#|$)\")\n"
        "    list(TRANSFORM group_entries PREPEND \"src/\")\n"
        "    set(${variable} \"${group_entries}\" PARENT_SCOPE)\n"
        "endfunction()\n\n"
        "lsm_read_source_group(LSM_HARDWARE_MONITOR_SOURCES\n"
        "    support/source-groups/hardware-monitor-linux.txt)\n"
        "lsm_read_source_group(LSM_MONITOR_BACKEND_SOURCES\n"
        "    support/source-groups/monitor-linux.txt)\n"
        "lsm_read_source_group(LSM_PROCESS_SOURCES\n"
        "    support/source-groups/process-linux.txt)\n",
        "centralise CMake source groups", re.S)
    text = text.replace(
        "${LSM_PROCESS_CORE_SOURCES} ${LSM_PROCESS_PLATFORM_SOURCES}",
        "${LSM_PROCESS_SOURCES}")
    text = text.replace(
        "${LSM_PROCESS_CORE_SOURCES}\n        ${LSM_PROCESS_PLATFORM_SOURCES}",
        "${LSM_PROCESS_SOURCES}")
    if "LSM_PROCESS_CORE_SOURCES" in text or "LSM_PROCESS_PLATFORM_SOURCES" in text:
        raise RuntimeError("CMake still contains duplicated process source groups")
    write(path, text)


def refactor_ci() -> None:
    path = ".github/workflows/ci.yml"
    text = read(path)
    old = '''          x86_64-w64-mingw32-gcc \\
            -std=c17 -O2 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE \\
            -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2 -Wundef \\
            -Wstrict-prototypes -Wmissing-prototypes \\
            -Isrc -Isrc/infiltratr-common/include -include "$version_header" \\
            src/main_windows.c src/presentation_contract.c src/performance_view.c \\
            src/overview_history.c src/monitor_backend_windows.c src/process_backend_windows.c \\
            build-cmake/windows-fonts.o "${common_portable[@]}" \\
'''
    new = '''          mapfile -t windows_sources < <(sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$/d' \\
            support/source-groups/windows-product.txt | sed 's#^#src/#')
          x86_64-w64-mingw32-gcc \\
            -std=c17 -O2 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE \\
            -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2 -Wundef \\
            -Wstrict-prototypes -Wmissing-prototypes \\
            -Isrc -Isrc/infiltratr-common/include -include "$version_header" \\
            "${windows_sources[@]}" \\
            build-cmake/windows-fonts.o "${common_portable[@]}" \\
'''
    text = replace_once(text, old, new, "centralise Windows CI source group")
    write(path, text)


def write_app_internal_allowlist() -> None:
    entries: list[str] = []
    for path in sorted((ROOT / "src").glob("*.[ch]")):
        if path.name == "app_internal.h":
            continue
        text = path.read_text(encoding="utf-8")
        if '#include "app_internal.h"' in text:
            entries.append(path.name)
    content = (
        "# SPDX-License-Identifier: GPL-3.0-or-later\n"
        "# Deliberate direct consumers of the complete private LsmApp layout.\n"
        "# New consumers must use a narrow context/contract instead.\n" +
        "\n".join(entries) + "\n"
    )
    write("support/app-internal-allowlist.txt", content)


def add_coupling_guardrails() -> None:
    path = "support/tools/check_source_style.cpp"
    text = read(path)
    insertion = r'''
static void check_private_app_coupling(void)
{
    StringList allowed;
    read_manifest("support/app-internal-allowlist.txt", "src", &allowed);

    std::error_code error;
    for (std::filesystem::directory_iterator iterator("src", error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error)) continue;
        const std::string name = iterator->path().filename().string();
        if (!ends_with(name, ".c") && !ends_with(name, ".h")) continue;
        if (name == "app_internal.h") continue;
        size_t size = 0U;
        char *contents = read_file(iterator->path().string().c_str(), &size);
        (void)size;
        if (!contents) continue;
        if (strstr(contents, "#include \"app_internal.h\"") &&
            !list_contains(&allowed, name.c_str()))
            report_error("src/%s: direct app_internal.h access is not approved; use a narrow context bridge",
                         name.c_str());
        free(contents);
    }
    if (error)
        report_error("src: unable to audit private application coupling: %s",
                     error.message().c_str());

    size_t size = 0U;
    char *windows = read_file("src/main_windows.c", &size);
    (void)size;
    if (windows) {
        static const char *const forbidden[] = {
            "typedef struct {\n    COLORREF background",
            "static bool resolve_theme(",
            "static DWORD WINAPI process_worker_main(",
            "static bool start_process_worker(",
            "state->process_lock",
            "state->process_event",
            "state->process_thread"
        };
        for (size_t index = 0U;
             index < sizeof(forbidden) / sizeof(forbidden[0]); index++)
            if (strstr(windows, forbidden[index]))
                report_error("src/main_windows.c: extracted Windows owner leaked back into shell: %s",
                             forbidden[index]);
        free(windows);
    }
}

static void check_source_group_manifests(const StringList *sources)
{
    static const char *const manifests[] = {
        "support/source-groups/hardware-monitor-linux.txt",
        "support/source-groups/monitor-linux.txt",
        "support/source-groups/process-linux.txt",
        "support/source-groups/windows-product.txt"
    };
    for (size_t manifest = 0U;
         manifest < sizeof(manifests) / sizeof(manifests[0]); manifest++) {
        StringList group;
        read_manifest(manifests[manifest], "src", &group);
        for (const std::string &entry : group)
            if (!list_contains(sources, entry.c_str()))
                report_error("%s: %s is not registered in support/sources.txt",
                             manifests[manifest], entry.c_str());
    }
}

'''
    text = replace_once(text, "int main(void)\n{\n", insertion + "int main(void)\n{\n",
                        "insert coupling guardrails")
    text = replace_once(
        text,
        '    check_source_manifest(&sources);\n',
        '    check_source_manifest(&sources);\n'
        '    check_source_group_manifests(&sources);\n'
        '    check_private_app_coupling();\n',
        "activate coupling guardrails")
    write(path, text)


def update_architecture_doc() -> None:
    path = "docs/ARCHITECTURE.md"
    text = read(path)
    marker = "Private application storage remains owned by the composition root. Coordinators that do not own that storage consume narrow views through dedicated runtime, process and presentation context bridges rather than including the complete `LsmApp` layout. Top-level page activation, keyboard commands, window-manager mechanics and theme projection have separate presentation owners so a change in one policy does not require the others to know its private state.\n"
    addition = marker + (
        "\nCoupling boundaries are mechanically enforced. `support/app-internal-allowlist.txt` "
        "is the closed set of implementation files permitted to include `app_internal.h`; "
        "new coordinators must use a narrow context bridge. Native Windows theme projection "
        "and process-worker lifetime are separate owners from the Win32 shell. Shared source "
        "groups under `support/source-groups/` are consumed by Make, CMake and CI so test and "
        "cross-build membership has one authority instead of parallel hand-maintained lists.\n")
    text = replace_once(text, marker, addition, "document coupling guardrails")
    write(path, text)


def cleanup_one_shot_files() -> None:
    (ROOT / ".github/workflows/structural-refactor.yml").unlink(missing_ok=True)
    Path(__file__).unlink(missing_ok=True)


def main() -> None:
    refactor_windows_main()
    write_source_groups()
    refactor_source_manifest()
    refactor_makefile()
    refactor_cmake()
    refactor_ci()
    write_app_internal_allowlist()
    add_coupling_guardrails()
    update_architecture_doc()
    cleanup_one_shot_files()


if __name__ == "__main__":
    main()
