#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply the one-shot 1.0.179 coupling cleanup with exact-source guards."""

from pathlib import Path
import re


def replace_exact(text: str, old: str, new: str, *, count: int = 1, label: str) -> str:
    found = text.count(old)
    if found != count:
        raise SystemExit(f"{label}: expected {count} matches, found {found}")
    return text.replace(old, new)


# Win32: remove worker synchronization/backend ownership from presentation state.
main_path = Path("src/main_windows.c")
main = main_path.read_text()
old_state = """    bool process_backend_attempted;\n    LsmProcessBackend *process_backend; /* Owned exclusively by process_thread. */\n    HANDLE process_thread;\n    HANDLE process_event;\n    CRITICAL_SECTION process_lock;\n    bool process_lock_initialised;\n    bool process_worker_abandoned;\n    bool process_stop_requested;\n    bool process_request_pending;\n    unsigned process_requested_flags;\n    LsmProcessInfo *process_completed;\n    size_t process_completed_count;\n    bool process_completed_ready;\n"""
new_state = """    bool process_backend_attempted;\n    LsmWindowsProcessWorker *process_worker;\n"""
main = replace_exact(main, old_state, new_state, label="Win32 process state")

main, removed = re.subn(
    r"static DWORD WINAPI process_worker_main\(LPVOID user_data\)\n\{.*?\n\}\n\nstatic void refresh_processes",
    "static void refresh_processes",
    main,
    count=1,
    flags=re.S,
)
if removed != 1:
    raise SystemExit(f"embedded Win32 worker: expected one block, removed {removed}")

main = replace_exact(
    main,
    """    if (!state->process_thread) {\n        set_status(state, L\"Processes backend unavailable\");\n        return;\n    }\n\n    request_process_snapshot(state, LSM_PROCESS_SCAN_NONE);\n    LsmProcessInfo *processes = NULL;\n    size_t count = 0U;\n    if (!take_process_snapshot(state, &processes, &count)) {\n""",
    """    if (!state->process_worker) {\n        set_status(state, L\"Processes backend unavailable\");\n        return;\n    }\n\n    (void)lsm_windows_process_worker_request(\n        state->process_worker, LSM_PROCESS_SCAN_NONE);\n    LsmProcessInfo *processes = NULL;\n    size_t count = 0U;\n    if (!lsm_windows_process_worker_take(\n            state->process_worker, &processes, &count)) {\n""",
    label="Processes request/take",
)
main = replace_exact(
    main,
    """static void refresh_overview_processes(LsmWindowsUiState *state)\n{\n    if (!state || !state->process_thread) return;\n    request_process_snapshot(state, LSM_PROCESS_SCAN_NONE);\n\n    LsmProcessInfo *processes = NULL;\n    size_t count = 0U;\n    if (!take_process_snapshot(state, &processes, &count)) return;\n""",
    """static void refresh_overview_processes(LsmWindowsUiState *state)\n{\n    if (!state || !state->process_worker) return;\n    (void)lsm_windows_process_worker_request(\n        state->process_worker, LSM_PROCESS_SCAN_NONE);\n\n    LsmProcessInfo *processes = NULL;\n    size_t count = 0U;\n    if (!lsm_windows_process_worker_take(\n            state->process_worker, &processes, &count))\n        return;\n""",
    label="Overview process worker",
)
main = replace_exact(
    main,
    """    state->process_backend_attempted = true;\n    if (!start_process_worker(state))\n        set_status(state, L\"Processes backend unavailable\");\n""",
    """    state->process_backend_attempted = true;\n    state->process_worker = lsm_windows_process_worker_create();\n    if (!state->process_worker)\n        set_status(state, L\"Processes backend unavailable\");\n""",
    label="Win32 process worker initialise",
)
main = replace_exact(
    main,
    """    if (state->window) KillTimer(state->window, LSM_WINDOWS_TIMER_ID);\n    const bool process_worker_stopped = stop_process_worker(state);\n    lsm_process_list_free(state->processes);\n    state->processes = NULL;\n    state->process_count = 0U;\n\n    if (process_worker_stopped)\n        state->process_backend = NULL;\n""",
    """    if (state->window) KillTimer(state->window, LSM_WINDOWS_TIMER_ID);\n    lsm_windows_process_worker_destroy(state->process_worker);\n    state->process_worker = NULL;\n    lsm_process_list_free(state->processes);\n    state->processes = NULL;\n    state->process_count = 0U;\n""",
    label="Win32 process worker destroy",
)
main = replace_exact(
    main,
    """    const int status = (int)message.wParam;\n    /*\n     * A process worker that ignored cancellation still owns process-related\n     * members of this state until process termination. Intentionally leave\n     * that one state allocation to the OS in this exceptional shutdown path;\n     * freeing it here would create a use-after-free race.\n     */\n    if (!state->process_worker_abandoned)\n        free(state);\n    return status;\n""",
    """    const int status = (int)message.wParam;\n    free(state);\n    return status;\n""",
    label="Win32 final state release",
)
forbidden = (
    "process_thread",
    "process_event",
    "process_lock",
    "process_worker_abandoned",
    "process_stop_requested",
    "process_request_pending",
    "process_requested_flags",
    "process_completed_ready",
    "process_completed_count",
    "state->process_completed",
    "LsmProcessBackend *process_backend",
    "start_process_worker(",
    "stop_process_worker(",
    "request_process_snapshot(",
    "take_process_snapshot(",
)
remaining = [token for token in forbidden if token in main]
if remaining:
    raise SystemExit(f"Win32 presentation still owns worker internals: {remaining}")
main_path.write_text(main)

# History persistence must be linked by the isolated retention test targets.
cmake_path = Path("CMakeLists.txt")
cmake = cmake_path.read_text()
cmake = replace_exact(
    cmake,
    """    add_executable(lsm-history-retention-smoke\n        support/tests/history_retention_smoke.c src/history.c\n        src/temporal_presentation.c src/ui_helpers.c)\n""",
    """    add_executable(lsm-history-retention-smoke\n        support/tests/history_retention_smoke.c src/history.c\n        src/history_persistence.c\n        src/temporal_presentation.c src/ui_helpers.c)\n""",
    label="CMake history persistence",
)
cmake_path.write_text(cmake)

make_path = Path("Makefile")
make = make_path.read_text()
make = replace_exact(
    make,
    "support/tests/history_retention_smoke.c src/history.c src/temporal_presentation.c \\\n",
    "support/tests/history_retention_smoke.c src/history.c src/history_persistence.c src/temporal_presentation.c \\\n",
    count=2,
    label="Make history persistence",
)
make_path.write_text(make)

# Return Verify to read-only, remove this one-shot hook, and keep the new worker
# in both permanent Windows compilation paths.
ci_path = Path(".github/workflows/ci.yml")
ci = ci_path.read_text()
ci = replace_exact(
    ci,
    "permissions:\n  contents: write\n",
    "permissions:\n  contents: read\n",
    label="Verify permissions restore",
)
ci, hook_count = re.subn(
    r"\n      # COUPLING_REFACTOR_ONCE_BEGIN\n.*?\n      # COUPLING_REFACTOR_ONCE_END\n",
    "\n",
    ci,
    count=1,
    flags=re.S,
)
if hook_count != 1:
    raise SystemExit(f"one-shot Verify hook: expected one block, removed {hook_count}")
ci = replace_exact(
    ci,
    """            src/overview_history.c src/monitor_backend_windows.c src/process_backend_windows.c \\\n            build-cmake/windows-fonts.o \"${common_portable[@]}\" \\\n""",
    """            src/overview_history.c src/monitor_backend_windows.c src/process_backend_windows.c \\\n            src/process_backend_windows_worker.c \\\n            build-cmake/windows-fonts.o \"${common_portable[@]}\" \\\n""",
    label="Linux-hosted Windows worker source",
)
ci = replace_exact(
    ci,
    """            'src/overview_history.c', 'src/monitor_backend_windows.c', 'src/process_backend_windows.c',\n            'build/windows-fonts.o',\n""",
    """            'src/overview_history.c', 'src/monitor_backend_windows.c', 'src/process_backend_windows.c',\n            'src/process_backend_windows_worker.c',\n            'build/windows-fonts.o',\n""",
    label="native Windows worker source",
)
ci_path.write_text(ci)

# Advance source identity only after every guarded transformation succeeded.
version_path = Path("support/VERSION")
if version_path.read_text().strip() != "1.0.178":
    raise SystemExit("source version moved while bounded refactor was staged")
version_path.write_text("1.0.179\n")

readme_path = Path("README.md")
readme = readme_path.read_text()
readme = replace_exact(
    readme,
    "**Current source version:** 1.0.178",
    "**Current source version:** 1.0.179",
    label="README source version",
)
readme_path.write_text(readme)

changelog_path = Path("CHANGELOG.md")
changelog = changelog_path.read_text()
entry = """## 1.0.179 - 2026-10-11\n\n- Move native Windows process-worker ownership out of the Win32 presentation shell into an opaque, reference-counted backend owner so an uncancellable scan no longer retains the complete UI state.\n- Move App History save snapshots, serialization, generation ordering and durable publication into a dedicated persistence owner.\n- Move technical process-cell formatting and heatmap policy out of Details into the shared process-table presentation layer behind a narrow display context.\n\n"""
changelog = replace_exact(
    changelog,
    "# Changelog\n\n",
    "# Changelog\n\n" + entry,
    label="1.0.179 changelog",
)
changelog_path.write_text(changelog)

# This helper and its workflow hook are deliberately absent from the resulting
# source tree; only the architectural changes remain.
Path(__file__).unlink()
